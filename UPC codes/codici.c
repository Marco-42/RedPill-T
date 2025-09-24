void COMMS_StateMachine( void )
{
    /* Target board initialization*/
    BoardInitMcu();
    BoardInitPeriph();

    /* Radio initialization */
    RadioEvents.TxDone = OnTxDone; // standby
    RadioEvents.RxDone = OnRxDone; // standby
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.RxError = OnRxError;
    RadioEvents.CadDone = OnCadDone;

    /* Timer used to restart the CAD --> check if the channel is free for transmission*/
    TimerInit(&CADTimeoutTimer, CADTimeoutTimeoutIrq);
    TimerSetValue(&CADTimeoutTimer, CAD_TIMER_TIMEOUT);

    /* App timer used to check the RX’s end */
    TimerInit(&RxAppTimeoutTimer, RxTimeoutTimerIrq);
    TimerSetValue(&RxAppTimeoutTimer, RX_TIMER_TIMEOUT);

    Radio.Init(&RadioEvents); //Initializes the Radio
    configuration(); //Configures the transceiver

    for(;;) //The only option to end the state machine is killing COMMS thread (by the OBC)
    {
        Radio.IrqProcess(); //Checks the interruptions
        switch(State)
        {
            case RX_TIMEOUT: //--> Timeout in reception
                RxTimeoutCnt++;
                Radio.Standby();
                State = LOWPOWER;
                break;

            case RX_ERROR: //--> Error in reception
                RxErrorCnt++;
                PacketReceived = false;
                Radio.Standby();
                State = LOWPOWER;
                break;

            case RX: //The reception is valid
            {
                if(PacketReceived == true)
                {
                    if(Buffer[0] == 0xC8) //if first byte of received buffer is 200 decimal
                    {
                        // DEINTERLEAVE
                        unsigned char codeword_deinterleaved[127];
                        int index = deinterleave(Buffer, BufferSize, codeword_deinterleaved);
                        decoded_size = index - NPAR;

                        // DECODE REED SOLOMON
                        int erasures[16];
                        int nerasures = 0;
                        decode_data(codeword_deinterleaved, decoded_size);
                        int syndrome = check_syndrome();

                        if (syndrome == 0) {
                            // No errors detected
                        } else {
                            // Attempt to correct errors
                            int result = correct_errors_erasures(codeword_deinterleaved, index, nerasures, erasures);
                        }
                        
                        // Copy the decoded data
                        memcpy(decoded, codeword_deinterleaved, decoded_size);
                        
                        // The pin code in the first two bytes of the decoded data(if not correct give an error message)
                        if (pin_correct(decoded[0], decoded[1]))
                        {
                            State = LOWPOWER;
                            if (decoded[2] == TLE)
                            {
                                Stop_timer_16();
                                if (!tle_telecommand)
                                {
                                    State = RX;
                                    telecommand_rx = true;
                                    //QEST: tle_conter = 3 is used for? 
                                    if (tle_counter == 3){
                                        tle_telecommand = true;
                                    }
                                    tle_counter++;
                                }
                                else
                                {
                                    tle_telecommand = false;
                                    State = LOWPOWER;
                                    telecommand_rx = false;
                                    tle_counter = 0;
                                }
                                process_telecommand(decoded[2], decoded[3]);
                            }

                            else if (decoded[2] == ADCS_CALIBRATION)
                            {
                                Stop_timer_16();
                                if (!calibration_telecommand)
                                {
                                    State = RX;
                                    telecommand_rx = true;
                                    if (calibration_counter == 1){
                                        calibration_telecommand = true;
                                    }
                                    calibration_counter++;
                                }
                                else
                                {
                                    calibration_telecommand = false;
                                    State = LOWPOWER;
                                    telecommand_rx = false;
                                    calibration_counter = 0;
                                }
                                process_telecommand(decoded[2], decoded[3]);
                            }

                            else if (telecommand_rx)
                            {
                                if (decoded[2] == last_telecommand[2])
                                {
                                    Stop_timer_16();
                                    if ((decoded[2] == RESET2) || (decoded[2] == EXIT_STATE) || (decoded[2] == TLE) ||
                                        (decoded[2] == ADCS_CALIBRATION) || (decoded[2] == SEND_DATA) || 
                                        (decoded[2] == SEND_TELEMETRY) || (decoded[2] == STOP_SENDING_DATA) ||
                                        (decoded[2] == CHANGE_TIMEOUT) || (decoded[2] == ACK_DATA) ||
                                        (decoded[2] == NACK_TELEMETRY) || (decoded[2] == NACK_CONFIG) ||
                                        (decoded[2] == ACTIVATE_PAYLOAD) || (decoded[2] == SEND_CONFIG) ||
                                        (decoded[2] == UPLINK_CONFIG))
                                    {
                                        telecommand_rx = false;
                                        process_telecommand(decoded[2], decoded[3]);
                                    }
                                    else {
                                        request_execution = true;
                                        State = TX;
                                    }
                                }

                                else if (decoded[2] == ACK)
                                {
                                    Stop_timer_16();
                                    request_counter = 0;
                                    request_execution = false;
                                    reception_ack_mode = false;
                                    telecommand_rx = false;
                                    process_telecommand(last_telecommand[2], last_telecommand[3]);
                                    State = RX;
                                }

                                else
                                {
                                    State = TX;
                                    telecommand_rx = false;
                                    error_telecommand = true;
                                    Stop_timer_16();
                                }
                            }

                            else
                            {
                                memcpy(last_telecommand, decoded, BUFFER_SIZE);
                                last_telecommand[0] = MISSION_ID;
                                last_telecommand[1] = POCKETQUBE_ID;
                                tle_telecommand = false;
                                telecommand_rx = true;
                                State = RX;
                                Start_timer_16();
                            }
                        }
                        else
                        {
                            State = TX;
                            error_telecommand = true;
                            Stop_timer_16();
                            xEventGroupSetBits(xEventGroup, COMMS_WRONGPACKET_EVENT);
                        }

                        PacketReceived = false;
                    }
                }else // If packet not received, restart reception process
                {
                    if (CadRx == CAD_SUCCESS)
                    {
                        channelActivityDetectedCnt++; // Update counter
                        RxTimeoutTimerIrqFlag = false;
                        TimerReset(&RxAppTimeoutTimer); // Start the Rx’s Timer
                    }
                    else
                    {
                        TimerStart(&CADTimeoutTimer); // Start the CAD’s Timer
                    }
                
                    Radio.Rx(RX_TIMEOUT_VALUE); // Basic RX code
                    State = LOWPOWER;
                
                    if (reception_ack_mode){
                        reception_ack_mode = false;
                    }
                }
                break;

            }	
            case TX:
            {
                State = LOWPOWER;
            
                if (error_telecommand)
                {
                    // Send error message
                    uint8_t packet_to_send[] = {MISSION_ID, POCKETQUBE_ID, ERROR};
                    Radio.Send(packet_to_send, sizeof(packet_to_send));
                    Radio.Send(packet_to_send, sizeof(packet_to_send));
                    error_telecommand = false;
                }
                else if (request_execution)
                {
                    Radio.Send(last_telecommand, 3); // Normally packets are TX in pairs
                    Radio.Send(last_telecommand, 3);
                    request_counter++;
                    reception_ack_mode = true;
                    State = RX;
                    Stop_timer_16();
                    Start_timer_16();
                }
                else if (tx_flag)
                {
                    uint64_t read_photo[12];
                    uint8_t transformed[DATA_PACKET_SIZE];
            
                    if (window_packet < WINDOW_SIZE)
                    {
                        if (nack_flag)
                        {
                            if (nack_counter < nack_size)
                            {
                                Read_Flash(PHOTO_ADDR + nack[nack_counter] * DATA_PACKET_SIZE, &read_photo, sizeof(read_photo));
                                decoded[3] = nack[nack_counter]; // Number of the retransmitted packet
                                nack_counter++;
                            }
                            else
                            {
                                nack_flag = false;
                                nack_counter = 0;
                                packet_number = 0;
                                window_packet = WINDOW_SIZE;
                            }
                        }
                        else
                        {
                            Read_Flash(PHOTO_ADDR + packet_number * DATA_PACKET_SIZE, &read_photo, sizeof(read_photo));
                            decoded[3] = packet_number; // Number of the packet
                            packet_number++;
                        }
            
                        decoded[0] = MISSION_ID;      // Satellite ID
                        decoded[1] = POCKETQUBE_ID;   // PocketQube ID
                        decoded[2] = SEND_DATA;
                        memcpy(&transformed, read_photo, sizeof(transformed));
                        for (uint8_t i = 4; i < DATA_PACKET_SIZE + 4; i++)
                        {
                            decoded[i] = transformed[i - 4];
                        }
            
                        TimerTime_t currentUnixTime = RtcGetTimerValue();
                        uint32_t unixTime32 = (uint32_t)currentUnixTime;
                        decoded[DATA_PACKET_SIZE + 4] = (unixTime32 >> 24) & 0xFF;
                        decoded[DATA_PACKET_SIZE + 5] = (unixTime32 >> 16) & 0xFF;
                        decoded[DATA_PACKET_SIZE + 6] = (unixTime32 >> 8) & 0xFF;
                        decoded[DATA_PACKET_SIZE + 7] = unixTime32 & 0xFF;
                        decoded[DATA_PACKET_SIZE + 8] = 0xFF; // End of packet indicator
            
                        window_packet++;
                        State = TX;
                        Radio.Send(decoded, DATA_PACKET_SIZE + 9);
                        vTaskDelay(pdMS_TO_TICKS(3000));
                        Radio.Send(decoded, DATA_PACKET_SIZE + 9);
                    }
                    else
                    {
                        tx_flag = false;
                        packet_number = 0;
                        send_data = false;
                        window_packet = 0;
                        State = RX;
                    }
                }
                else if (beacon_flag)
                {
                    uint8_t packet_to_send[] = {MISSION_ID, POCKETQUBE_ID, BEACON};
                    Radio.Send(packet_to_send, sizeof(packet_to_send));
                    beacon_flag = false;
                }
                break;
            }

            case TX_TIMEOUT:
            {
                State = LOWPOWER;
                break;
            }
            case LOWPOWER:
            default:
            {
                if (error_telecommand || tx_flag){
                    State = TX;
                }
                else if (reception_ack_mode || tle_telecommand){
                    State = RX;
                }
                else if (telecommand_rx)
                {
                    if (request_execution)
                    {
                        if (request_counter >= 3)
                        {
                            request_execution = false;
                            error_telecommand = true;
                            telecommand_rx = false;
                            State = TX;
                            request_counter = 0;
                            Stop_timer_16();
                        }
                        else if (protocol_timeout)
                        {
                            protocol_timeout = false;
                            State = TX;
                        }
                        else
                        {
                            State = RX;
                        }
                    }
                    else
                    {
                        // Check timing for second telecommand reception
                        if (protocol_timeout)
                        {
                            PacketReceived = true;
                            protocol_timeout = false;
                            Stop_timer_16();
                        }
                        State = RX;
                    }
                }
                else if (beacon_flag)
                {
                    State = TX;
                }
                else
                {
                    State = RX;
                }
                break;
            }
        }
    }
}

void process_telecommand(uint8_t header, uint8_t info)
{
    uint8_t info_write;

    switch(header)
    {
        case RESET2:
        {
            HAL_NVIC_SystemReset();
            break;
        }

        case EXIT_STATE:
        {
            if(decoded[3] == 0xF0){
                xTaskNotify(OBC_Handle, EXIT_CONTINGENCY_NOTI, eSetBits);
            }
            else if(decoded[3] == 0x0F){
                xTaskNotify(OBC_Handle, EXIT_SUNSAFE_NOTI, eSetBits);
            }
            else if(decoded[4] == 0xF0){
                xTaskNotify(OBC_Handle, EXIT_SURVIVAL_NOTI, eSetBits);
            }
            break;
        }

        case TLE:
        {
            if (tle_counter == 1 || tle_counter == 2){
                Send_to_WFQueue(&decoded[3], TLE_PACKET_SIZE, TLE_ADDR1 + (tle_counter - 1) * TLE_PACKET_SIZE, COMMSsender);
            }
            else if (tle_counter == 3){
                Send_to_WFQueue(&decoded[3], 1, TLE_ADDR1 + 2 * TLE_PACKET_SIZE, COMMSsender);
                Send_to_WFQueue(&decoded[4], TLE_PACKET_SIZE - 1, TLE_ADDR2, COMMSsender);
            }
            else if(tle_counter == 4 || tle_counter == 5){
                Send_to_WFQueue(&decoded[3], TLE_PACKET_SIZE, TLE_ADDR2 + (tle_counter - 3) * TLE_PACKET_SIZE - 1, COMMSsender);
            }
            xTaskNotify(OBC_Handle, TLE_NOTI, eSetBits);
            break;
        }

        case SEND_DATA:
        {
            if (!contingency){
                tx_flag = true;
                State = TX;
                send_data = true;
            }
            break;
        }

        case SEND_TELEMETRY:
        case NACK_TELEMETRY:
        {
            uint64_t read_telemetry[5];
            uint8_t transformed[TELEMETRY_PACKET_SIZE];

            if (!contingency){
                Read_Flash(PHOTO_ADDR, &read_telemetry, sizeof(read_telemetry)); // Cambiare a TELEMETRY_ADDR se necessario
                Send_to_WFQueue(&read_telemetry, sizeof(read_telemetry), PHOTO_ADDR, COMMSsender);

                decoded[0] = MISSION_ID;
                decoded[1] = POCKETQUBE_ID;
                decoded[2] = SEND_TELEMETRY;
                memcpy(&transformed, read_telemetry, sizeof(transformed));

                for (uint8_t i = 3; i < TELEMETRY_PACKET_SIZE + 3; i++){
                    decoded[i] = transformed[i - 3];
                }

                TimerTime_t currentUnixTime = RtcGetTimerValue();
                uint32_t unixTime32 = (uint32_t)currentUnixTime;
                decoded[TELEMETRY_PACKET_SIZE + 3] = (unixTime32 >> 24) & 0xFF;
                decoded[TELEMETRY_PACKET_SIZE + 4] = (unixTime32 >> 16) & 0xFF;
                decoded[TELEMETRY_PACKET_SIZE + 5] = (unixTime32 >> 8) & 0xFF;
                decoded[TELEMETRY_PACKET_SIZE + 6] = unixTime32 & 0xFF;

                uint8_t encoded[256];
                int encoded_len_bytes = encode(decoded, encoded, TELEMETRY_PACKET_SIZE + 7);
                vTaskDelay(pdMS_TO_TICKS(3000));
                Radio.Send(encoded, encoded_len_bytes);
                vTaskDelay(pdMS_TO_TICKS(3000));
                Radio.Send(encoded, encoded_len_bytes);

                State = RX;
            }
            break;
        }
        case STOP_SENDING_DATA:
        {
            tx_flag = false;
            State = RX;
            send_data = false;
            break;
        }

        case ACK_DATA:
        {
            if (!contingency && info != 0)
            {
                nack_flag = true;
                int j = 0;
                for(int i = 3; i < decoded_size - 4; i++) // -4 per escludere timestamp
                {
                    if(decoded[i] == 0x0){
                        nack[j] = i - 3;
                        j++;
                    }
                }
                nack_size = j;

                if (nack_size != 0){
                    tx_flag = true;
                    State = TX;
                    send_data = true;
                }
                else{
                    tx_flag = false;
                    State = RX;
                    send_data = false;
                }
            }
            break;
        }

        case ADCS_CALIBRATION:
        {
            Send_to_WFQueue(&decoded[4], CALIBRATION_PACKET_SIZE,
                            CALIBRATION_ADDR + CALIBRATION_PACKET_SIZE * calibration_counter, COMMSsender);
            xTaskNotify(OBC_Handle, CALIBRATION_NOTI, eSetBits);
            break;
        }

        case CHANGE_TIMEOUT:
        {
            // Send_to_WFQueue(&decoded[3], 2, COMMS_TIME_ADDR, COMMSsender);
            break;
        }

        case ACTIVATE_PAYLOAD:
        {
            Send_to_WFQueue(&decoded[3], 4, PL_TIME_ADDR, COMMSsender);
            Send_to_WFQueue(&decoded[7], 1, PHOTO_RESOL_ADDR, COMMSsender);
            Send_to_WFQueue(&decoded[8], 1, PHOTO_COMPRESSION_ADDR, COMMSsender);
            xTaskNotify(OBC_Handle, TAKEPHOTO_NOTI, eSetBits);
            Send_to_WFQueue(&decoded[9], 8, PL_RF_TIME_ADDR, COMMSsender);
            Send_to_WFQueue(&decoded[17], 1, F_MIN_ADDR, COMMSsender);
            Send_to_WFQueue(&decoded[18], 1, F_MAX_ADDR, COMMSsender);
            Send_to_WFQueue(&decoded[19], 1, DELTA_F_ADDR, COMMSsender);
            Send_to_WFQueue(&decoded[20], 1, INTEGRATION_TIME_ADDR, COMMSsender);
            break;
        }

        case SEND_CONFIG:
        case NACK_CONFIG:
        {
            uint64_t read_config[4];
            uint8_t transformed[CONFIG_PACKET_SIZE];

            if (!contingency){
                Read_Flash(PHOTO_ADDR, &read_config, sizeof(read_config)); // Cambiare a CONFIG_ADDR
                Send_to_WFQueue(&read_config, sizeof(read_config), PHOTO_ADDR, COMMSsender);

                decoded[0] = MISSION_ID;
                decoded[1] = POCKETQUBE_ID;
                decoded[2] = SEND_CONFIG;

                TimerTime_t currentUnixTime = RtcGetTimerValue();
                memcpy(&transformed, read_config, sizeof(transformed));

                for (uint8_t i = 3; i < CONFIG_PACKET_SIZE + 3; i++){
                    decoded[i] = transformed[i - 3];
                }

                uint32_t unixTime32 = (uint32_t)currentUnixTime;
                decoded[CONFIG_PACKET_SIZE + 3] = (unixTime32 >> 24) & 0xFF;
                decoded[CONFIG_PACKET_SIZE + 4] = (unixTime32 >> 16) & 0xFF;
                decoded[CONFIG_PACKET_SIZE + 5] = (unixTime32 >> 8) & 0xFF;
                decoded[CONFIG_PACKET_SIZE + 6] = unixTime32 & 0xFF;

                uint8_t conv_encoded[256];
                int encoded_len_bytes = encode(decoded, conv_encoded, CONFIG_PACKET_SIZE + 7);
                vTaskDelay(pdMS_TO_TICKS(3000));
                Radio.Send(conv_encoded, encoded_len_bytes);
                vTaskDelay(pdMS_TO_TICKS(3000));
                Radio.Send(conv_encoded, encoded_len_bytes);

                State = RX;
            }
            break;
        }

        case UPLINK_CONFIG:
        {
            Send_to_WFQueue(&decoded[3], 4, SET_TIME_ADDR, COMMSsender);
            Send_to_WFQueue(&decoded[7], 2, SF_ADDR, COMMSsender);
            Send_to_WFQueue(&decoded[9], 2, CRC_ADDR, COMMSsender);
            Send_to_WFQueue(&decoded[11], 1, KP_ADDR, COMMSsender);
            Send_to_WFQueue(&decoded[11], 1, GYRO_RES_ADDR, COMMSsender);
            break;
        }

        default:
        {
            State = TX;
            error_telecommand = true;
            break;
        }
    }

    if (!error_telecommand)
    {
        xEventGroupSetBits(xEventGroup, COMMS_TELECOMMAND_EVENT);
    }
}
