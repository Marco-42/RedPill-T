import struct
import sys
import serial
import serial.tools.list_ports

from datetime import datetime

from PyQt5.QtWidgets import *
from PyQt5.QtGui import QColor
from PyQt5.QtCore import *

import hmac
import hashlib
import traceback

# ========== CONSTANTS AND CONFIGURATION ==========

RX_TIMEOUT = 5  # seconds to wait for a reply after sending a TEC

# Packet configuration
PACKET_HEADER_LENGTH = 12 # 4 bytes for header + 4 bytes for MAC + 4 bytes for timestamp
PACKET_PAYLOAD_MAX = 98  # maximum payload length in bytes

# MAC configuration
SECRET_KEY = 0xA1B2C3D4

# Fixed bytes configuration
BYTE_RS_OFF = 0x55
BYTE_RS_ON = 0xAA
BYTE_TX_OFF = 0x00
BYTE_TX_ON = 0x01
BYTE_TX_NOBEACON = 0x02

# Error codes
PACKET_ERR_NONE = 0
PACKET_ERR_RS = -1
PACKET_ERR_DECODE = -2
PACKET_ERR_LENGTH = -3
PACKET_ERR_MAC = -4
PACKET_ERR_CMD_FULL = -5
PACKET_ERR_CMD_POINTER = -6
PACKET_ERR_CMD_UNKNOWN = -7
PACKET_ERR_CMD_PAYLOAD = -8
PACKET_ERR_CMD_MEMORY = -9

PACKET_ERR_DESCRIPTION = {
    PACKET_ERR_NONE: "Unexpected behavior: no error code provided",
    PACKET_ERR_RS: "Reed-Solomon decoding failed",
    PACKET_ERR_DECODE: "Packet decode error",
    PACKET_ERR_LENGTH: "Invalid packet length",
    PACKET_ERR_MAC: "MAC verification failed",
    PACKET_ERR_CMD_FULL: "TEC buffer full",
    PACKET_ERR_CMD_POINTER: "Command pointer error",
    PACKET_ERR_CMD_UNKNOWN: "Requested command not found",
    PACKET_ERR_CMD_PAYLOAD: "Payload of command is invalid",
    PACKET_ERR_CMD_MEMORY: "Memory allocation error during command execution"
}

# Source codes for transmission
TX_SOURCES = {
    "RedPill": 0x01,
    "None": 0x10,
    "UniPD": 0x11,
    "Mobile": 0x12
}

# TEC_TER_TYPES definition
TEC_TER_TYPES = {
    "[HK] Housekeeping": 0,
    "[DAQ] Data Acquisition": 1,
    "[PE] Payload Execution": 2,
    "[DT] Data Transfer": 3
}

# TEC task codes definitions
TEC_OBC_REBOOT = 0x01 # reboot OBC command
TEC_EXIT_STATE = 0x02 # exit state command
TEC_VAR_CHANGE = 0x03 # variable change command
TEC_SET_TIME = 0x04 # set time command
TEC_EPS_REBOOT = 0x08 # reboot EPS command
TEC_ADCS_REBOOT = 0x10 # reboot ADCS command
TEC_ADCS_TLE = 0x11 # send TLE to ADCS command
TEC_LORA_STATE = 0x18 # send LoRa state command
TEC_LORA_CONFIG = 0x19 # send LoRa configuration command
TEC_LORA_PING = 0x1A # send LoRa ping status command

TEC_CRY_EXP = 0x80 # execute crystals experiment command

# TEC_TASKS labels
TEC_TASKS = {
    # HK
    "TLE update": TEC_ADCS_TLE,
    "Set clock": TEC_SET_TIME,
    "LoRa ping": TEC_LORA_PING,
    "LoRa state": TEC_LORA_STATE,
    "LoRa config": TEC_LORA_CONFIG,
    "Exit state": TEC_EXIT_STATE,
    "Variable change": TEC_VAR_CHANGE,
    "OBC reboot": TEC_OBC_REBOOT,
    "EPS reboot": TEC_EPS_REBOOT,
    "ADCS reboot": TEC_ADCS_REBOOT,
    # DAQ
    "Start DAQ": 68,
    "Stop DAQ": 69,
    # PE
    "Crystals experiment": TEC_CRY_EXP,
    "Execute B": 133,
    # DT
    "Picture download": 193,
    "Download": 194
}

# TER task codes definitions
TER_BEACON = 0x30 # telemetry beacon reply
TER_ACK = 0x31 # ACK reply
TER_NACK = 0x32 # NACK reply
TER_LORA_PING = 0x33 # LoRa ping state reply

# TER_TASKS labels
TER_TASKS = {
    # HK
    "Beacon": TER_BEACON,
    "ACK": TER_ACK,
    "NACK": TER_NACK,
    "LoRa ping": TER_LORA_PING
}

# Input form configs (label, widget_type, optional widget args)
INPUT_FIELDS_CONFIG = {
    "OBC reboot": [
        ("TEXT", QLabel, "No configuration available, execution is immediate", None, None, None)
    ],
    "Exit state": [
        # ("Delay:", QSpinBox, 0, 0, 100, "[s]"),
        ("From state:", QSpinBox, 0, 0, 14, "0-14"),
        ("To state:", QSpinBox, 1, 0, 14, "0-14"),
    ],
    "Variable change": [
        ("Address:", QSpinBox, 0, 0, 254, "0-254"),
        ("Value:", QSpinBox, 0, 0, 254, "0-254"),
    ],
    "Set clock": [
        ("Date and Time:", QDateTimeEdit, QDateTime.currentDateTime(), None, None, None)
    ],
    "EPS reboot": [
        ("TEXT", QLabel, "No configuration available, execution is immediate", None, None, None)
    ],
    "ADCS reboot": [
        ("TEXT", QLabel, "No configuration available, execution is immediate", None, None, None)
    ],
    "TLE update": [
        ("TLE Data:", QPlainTextEdit, "Line 1\nLine 2", None, None, None)
    ],
    "LoRa state": [
        ("TX State:", QComboBox, "On", ["On", "Beacon off", "Off"], None, None),
        ("TEXT", QLabel, "Enter time to keep state for, 0 is forever:", None, None, None),
        ("Days:", QSpinBox, 0, 0, 193, "d"),
        ("Hours:", QSpinBox, 0, 0, 23, "h"),
        ("Minutes:", QSpinBox, 0, 0, 59, "m"),
        ("Seconds:", QSpinBox, 0, 0, 59, "s"),
        ("TEXT", QLabel, "Reverts to TX State: On after time is elapsed", None, None, None),
    ],
    "LoRa config": [
        ("Frequency:", QDoubleSpinBox, 436.0, 400.0, 500.0, "MHz"),
        ("Bandwidth:", QComboBox, "125", ["62.5", "125", "250", "500"], None, "kHz"),
        ("SF:", QSpinBox, 10, 6, 12, None),
        ("CR:", QSpinBox, 5, 5, 8, None),
        ("Power:", QSpinBox, -9, 10, 22, "dBm"), #TODO: check power range mapping
    ],
    "LoRa ping": [
        ("TEXT", QLabel, "No configuration available, execution is immediate", None, None, None)
    ],
    "Crystals experiment": [
        ("Glass:", QComboBox, "Off", ["Off", "Dark", "Light"], None, None),
        ("TEXT", QLabel, "Enter time to delay Crystals activation since packet RX", None, None, None),
        ("Activation delay:", QTimeEdit, QTime(0, 0, 0), 0, 0, "hh:mm:ss"),
        ("Photodiode:", QComboBox, "No", ["Yes", "No"], None, None),
        ("Picture:", QComboBox, "No", ["Yes", "No"], None, None),
        ("TEXT", QLabel, "Enter time to delay observations since Crystals activation", None, None, None),
        ("Observation delay:", QTimeEdit, QTime(0, 0, 0), 0, 0, "hh:mm:ss"),
    ],
    "Picture settings": [
        ("TEXT", QLabel, "TODO", None, None, None)
    ],
}


# ========== HELPER FUNCTIONS ==========

# Build the payload based on type index and task name
def build_payload(tec_code, input_widgets):
    
    if tec_code == TEC_OBC_REBOOT:
        payload = b''

    elif tec_code == TEC_EXIT_STATE:
        from_state = input_widgets["From state:"].value() & 0x0F
        to_state = input_widgets["To state:"].value() & 0x0F
        byte_val = (from_state << 4) | to_state
        payload = bytes([byte_val])

    elif tec_code == TEC_VAR_CHANGE:
        address = input_widgets["Address:"].value() & 0xFF
        value = input_widgets["Value:"].value() & 0xFF
        payload = bytes([address, value])

    elif tec_code == TEC_SET_TIME:
        dt_widget = input_widgets["Date and Time:"]
        qdt = dt_widget.dateTime()  # QDateTime object
        unix_time = int(qdt.toSecsSinceEpoch())  # convert to UNIX timestamp (int)
        payload = unix_time.to_bytes(4, byteorder='big')
    
    elif tec_code == TEC_EPS_REBOOT:
        payload = b''

    elif tec_code == TEC_ADCS_REBOOT:
        payload = b''

    elif tec_code == TEC_ADCS_TLE:
        tle_data = input_widgets["TLE Data:"].toPlainText().strip()
        tle_lines = tle_data.splitlines()
        if len(tle_lines) < 2:
            raise ValueError("TLE Data must contain two lines.")

        tle_line1 = tle_lines[0]
        tle_line2 = tle_lines[1]

        # Validate TLE lengths
        if len(tle_line1) != 69 or len(tle_line2) != 69:
            raise ValueError("TLE lines must be 69 characters long.")

        try:
            epoch_year = int(tle_line1[18:20])
            epoch_year += 2000 if epoch_year < 57 else 1900
            epoch_day = float(tle_line1[20:32])
            mm_dot = float(tle_line1[33:43])
            mm_ddot = float(f"{tle_line1[44:50].strip()}e{tle_line1[50:52]}")
            bstar = float(f"{tle_line1[53:59].strip()}e{tle_line1[59:61]}")
            inclination = float(tle_line2[8:16])
            raan = float(tle_line2[17:25])
            eccentricity = float(f"0.{tle_line2[26:33].strip()}")
            arg_perigee = float(tle_line2[34:42])
            mean_anomaly = float(tle_line2[43:51])
            mean_motion = float(tle_line2[52:63])
            rev_number = int(tle_line2[63:68])
        except Exception as e:
            raise ValueError(f"Invalid TLE format: {e}")

        # Pack into 43 bytes, big-endian
        payload = struct.pack(
            ">HffffffffffI",  # > = big endian
            epoch_year,
            epoch_day,
            mm_dot,
            mm_ddot,
            bstar,
            inclination,
            raan,
            eccentricity,
            arg_perigee,
            mean_anomaly,
            mean_motion,
            rev_number
        )

    elif tec_code == TEC_LORA_STATE:
        # TX State (1 byte)
        tx_state = input_widgets["TX State:"].currentText()
        if tx_state == "On":
            tx_state_val = BYTE_TX_ON & 0b1111
        elif tx_state == "Beacon off":
            tx_state_val = BYTE_TX_NOBEACON & 0b1111
        elif tx_state == "Off":
            tx_state_val = BYTE_TX_OFF & 0b11
        else:
            raise ValueError(f"Invalid TX State: {tx_state}")
        tx_state_byte = (tx_state_val << 4) |  tx_state_val

        # Duration (3 bytes)
        days = input_widgets["Days:"].value()
        hours = input_widgets["Hours:"].value()
        minutes = input_widgets["Minutes:"].value()
        seconds = input_widgets["Seconds:"].value()
        duration_total = (days * 86400) + (hours * 3600) + (minutes * 60) + seconds
        duration_bytes = duration_total.to_bytes(3, 'big')  # 3 bytes

        # Combine all into one payload
        payload = bytes([tx_state_byte]) + duration_bytes

    elif tec_code == TEC_LORA_CONFIG:
        frequency_khz = int(input_widgets["Frequency:"].value() * 1000)
        frequency_bytes = frequency_khz.to_bytes(3, 'big')

        bandwidth = input_widgets["Bandwidth:"].currentText()
        if bandwidth == "62.5":
            bandwidth_bits = 0b00
        elif bandwidth == "125":
            bandwidth_bits = 0b01
        elif bandwidth == "250":
            bandwidth_bits = 0b10
        elif bandwidth == "500":
            bandwidth_bits = 0b11
        else:
            raise ValueError(f"Invalid bandwidth value: {bandwidth}")
        
        sf = input_widgets["SF:"].value()
        sf_bits = (sf - 6) & 0b111  # SF 6-12 maps to bits 0-6, 3 bits total

        cr = input_widgets["CR:"].value()
        cr_bits = (cr - 5) & 0b111  # CR 5-8 maps to bits 0-3, 3 bits total

        first_byte = bytes([(bandwidth_bits << 6) | (sf_bits << 3) | cr_bits])

        power = input_widgets["Power:"].value()
        power_bits = (power + 9) & 0b11111 # Power -9 to +22 maps to bits 0-31, 5 bits total

        reserved_bits = 0b000
        second_byte = bytes([(power_bits << 3) | reserved_bits])

        payload = frequency_bytes + first_byte + second_byte

    elif tec_code == TEC_LORA_PING:
        payload = b''

    elif tec_code == TEC_CRY_EXP:
        # Glass state: Off, Dark, Light (3 bits)
        glass_state = input_widgets["Glass:"].currentText()
        if glass_state == "Off":
            glass_val = 0b000
        elif glass_state == "Dark":
            glass_val = 0b001
        elif glass_state == "Light":
            glass_val = 0b010
        else:
            raise ValueError(f"Invalid Glass state: {glass_state}")

        # Repeat glass_val twice (6 bits total)
        glass_bits = (glass_val << 3) | glass_val

        activation_time = input_widgets["Activation delay:"].time()
        activation_secs = (activation_time.hour() * 3600 + activation_time.minute() * 60 + activation_time.second())

        # Combine: 6 bits glass_bits + 18 bits activation_secs
        glass_and_delay = (glass_bits << 18) | activation_secs
        glass_and_delay_bytes = glass_and_delay.to_bytes(3, 'big')

        # Photodiode: Yes/No (3 bits for future expansion)
        photodiode = input_widgets["Photodiode:"].currentText()
        if photodiode == "No":
            diode_val = 0b000
        elif photodiode == "Yes":
            diode_val = 0b001
        else:
            raise ValueError(f"Invalid Photodiode state: {photodiode}")

        # Picture: Yes/No (3 bits for future expansion)
        picture = input_widgets["Picture:"].currentText()
        if picture == "No":
            picture_val = 0b000
        elif picture == "Yes":
            picture_val = 0b001
        else:
            raise ValueError(f"Invalid Picture state: {picture}")

        # Combine: 3 bits diode + 3 bits picture
        state_bits = (diode_val << 3) | picture_val

        observation_time = input_widgets["Observation delay:"].time()
        observation_secs = (observation_time.hour() * 3600 + observation_time.minute() * 60 + observation_time.second())

        # Combine: 6 bits state_bits + 18 bits observation_secs
        state_and_delay = (state_bits << 18) | observation_secs
        state_and_delay_bytes = state_and_delay.to_bytes(3, 'big')

        # Final payload: 6 bytes
        payload = glass_and_delay_bytes + state_and_delay_bytes

    else:
        payload = b''

    return payload

def hmac_mac(key_int, message: bytes) -> int:
    # Convert 32-bit integer key to bytes
    key_bytes = key_int.to_bytes(4, byteorder='big')

    # Compute HMAC-SHA256 and return first 4 bytes as uint32
    mac = hmac.new(key_bytes, message, hashlib.sha256).digest()
    return int.from_bytes(mac[:4], byteorder='big')

# Build the full packet with header, payload, and ECC(for transmission)
def build_packet(gs_text, tec_code, payload_bytes, ecc_enabled):
    # === Byte 0: Station ID ===
    if gs_text in TX_SOURCES:
        station = TX_SOURCES[gs_text]
    else:
        raise ValueError(f"Invalid Ground Station: {gs_text}")

    # === Byte 1: ECC Flag ===
    ecc = BYTE_RS_ON if ecc_enabled else BYTE_RS_OFF

    # === Byte 2: TEC
    tec = tec_code

    # === Byte 3: Payload length ===
    payload_length = len(payload_bytes)
    if payload_length > PACKET_PAYLOAD_MAX:
        raise ValueError(f"Payload too long (max {PACKET_PAYLOAD_MAX} bytes)")

    # === Byte 4-7: UNIX timestamp ===
    unix_time = int(datetime.now().timestamp())
    bytes_time = unix_time.to_bytes(4, byteorder='big')

    # === Byte 8-11: MAC ===
    mac_placeholder = b'\x00\x00\x00\x00'
    header_partial = bytes([
        station,
        ecc,
        tec,
        payload_length
    ])
    packet_placeholder = header_partial + bytes_time + mac_placeholder + payload_bytes

    # Compute MAC over the whole packet
    mac = hmac_mac(SECRET_KEY, packet_placeholder)
    bytes_mac = mac.to_bytes(4, byteorder='big')

    # Build final packet
    return header_partial + bytes_time + bytes_mac + payload_bytes

# Decode a packet from bytes (for reception)
def decode_packet(packet_bytes):
    if len(packet_bytes) < PACKET_HEADER_LENGTH:
        raise ValueError(f"Packet too short to decode: length {len(packet_bytes)} < {PACKET_HEADER_LENGTH}")

    # Byte 0: Station ID (raw int)
    station_id = packet_bytes[0]

    # Byte 1: ECC Flag (bool)
    byte_ecc = packet_bytes[1]
    if byte_ecc == BYTE_RS_ON:
        ecc_enabled = True
    elif byte_ecc == BYTE_RS_OFF:
        ecc_enabled = False
    else:
        raise ValueError(f"Invalid ECC flag: 0x{byte_ecc:02X}")

    # Byte 2: TER (raw int)
    ter = packet_bytes[2]

    # Byte 3: Payload length
    payload_length = packet_bytes[3]

    # Ensure entire packet is present
    expected_len = PACKET_HEADER_LENGTH + payload_length
    if len(packet_bytes) < expected_len:
        raise ValueError(f"Packet too short for payload: length {len(packet_bytes)} < expected {expected_len}")

    # Bytes 4-7: Timestamp (int)
    bytes_time = packet_bytes[4:8]
    timestamp = int.from_bytes(bytes_time, byteorder='big')

    # Bytes 8–11: MAC (int)
    bytes_mac = packet_bytes[8:12]
    mac = int.from_bytes(bytes_mac, byteorder='big')

    # Payload: list of ints
    if payload_length > 0:
        payload_bytes = list(packet_bytes[12:12 + payload_length])
    else:
        payload_bytes = []

    return {
        "station_id": station_id,
        "ecc_enabled": ecc_enabled,
        "ter": ter,
        "payload_length": payload_length,
        "mac": mac,
        "timestamp": timestamp,
        "payload_bytes": payload_bytes  # always a list of ints
    }

# Get the type index from a task code
def get_type_from_task(code):
    # Determine the type key based on code ranges (0-63: HK, 64-127: DAQ, 128-191: PE, 192-255: DT)
    if 0 <= code <= 63:
        return 0  # HK
    elif 64 <= code <= 127:
        return 1  # DAQ
    elif 128 <= code <= 191:
        return 2  # PE
    elif 192 <= code <= 255:
        return 3  # DT
    else:
        return None  # Unknown type

# Get the label for a task code from the tasks dictionary
def get_task_label(tasks_dict, code):
    for label, task_code in tasks_dict.items():
        if task_code == code:
            type_index = get_type_from_task(code)
            full_prefix = next((k for k, v in TEC_TER_TYPES.items() if v == type_index), "[Unknown]")
            # Extract the text between the first '[' and ']'
            start = full_prefix.find('[')
            end = full_prefix.find(']')
            prefix = full_prefix[start+1:end] if start != -1 and end != -1 else full_prefix
            return f"[{prefix}] {label}"
    return f"Unknown: 0x{code:02X}"


# ========== MAIN WINDOW CLASS ==========

class MainWindow(QWidget):

    def __init__(self):
        super().__init__()
        self.setWindowTitle("ESP32 Serial GUI")
        self.serial_conn = None
        self.tec_queue = []
        self.created_widgets = {}

        # === Left Panel Components ===
        self.gs_selector = QComboBox()
        self.gs_selector.addItems(["None", "UniPD", "Mobile"])
        self.last_gs_index = 0
        self.gs_selector.currentIndexChanged.connect(self.check_gs_password)

        self.ecc_enable_radio = QRadioButton("Enabled")
        self.ecc_disable_radio = QRadioButton("Disabled")
        self.ecc_disable_radio.setChecked(True)

        self.ecc_radio_group = QButtonGroup()
        self.ecc_radio_group.addButton(self.ecc_enable_radio)
        self.ecc_radio_group.addButton(self.ecc_disable_radio)

        self.ecc_radio_layout = QHBoxLayout()
        self.ecc_radio_layout.addWidget(self.ecc_enable_radio)
        self.ecc_radio_layout.addWidget(self.ecc_disable_radio)

        self.type_selector = QComboBox()
        self.type_selector.addItems(TEC_TER_TYPES)
        self.type_selector.currentIndexChanged.connect(self.update_task_selector)

        self.task_selector = QComboBox()
        self.task_selector.currentIndexChanged.connect(self.update_packet_content_form)

        self.add_to_queue_button = QPushButton("Add to Queue")
        self.add_to_queue_button.clicked.connect(self.add_to_queue)

        self.queue_table = QTableWidget()
        self.queue_table.setColumnCount(2)
        self.queue_table.setHorizontalHeaderLabels(["TEC", "HEX"])
        self.queue_table.horizontalHeader().setStretchLastSection(True)
        self.queue_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.queue_table.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.queue_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)

        self.abort_last_button = QPushButton("Abort Last")
        self.abort_last_button.clicked.connect(self.abort_last_tec)

        self.abort_next_button = QPushButton("Abort Next")
        self.abort_next_button.clicked.connect(self.abort_next_tec)

        self.execute_next_button = QPushButton("Execute Next")
        self.execute_next_button.clicked.connect(self.execute_next_tec)
        self.execute_next_button.setEnabled(False)

        self.last_tec_status = QLabel("NO COMMS")
        self.last_tec_status.setAlignment(Qt.AlignCenter)
        self.last_tec_status.setStyleSheet("background-color: lightgray; color: black; font-weight: bold; padding: 5px;")

        self.last_tec_status_description = QLabel("")

        # === Right Panel Components ===
        self.serial_status_label = QLabel("DISCONNECTED")
        self.serial_status_label.setAlignment(Qt.AlignCenter)
        self.serial_status_label.setStyleSheet("font-weight: bold; padding: 5px;")
        self.set_serial_status(False)

        self.port_selector = QComboBox()
        self.refresh_ports()

        self.refresh_button = QPushButton("Refresh Ports")
        self.refresh_button.clicked.connect(self.refresh_ports)

        self.connect_button = QPushButton("Connect")
        self.connect_button.clicked.connect(self.toggle_connection)

        # Serial Communication group box (for port, connect, status)
        serial_comm_group = QGroupBox("Serial Communication")
        serial_comm_layout = QVBoxLayout()
        serial_comm_layout.addWidget(self.serial_status_label)

        serial_comm_form = QFormLayout()
        serial_comm_form.addRow("Select COM Port:", self.port_selector)
        serial_comm_layout.addLayout(serial_comm_form)

        serial_comm_buttons = QHBoxLayout()
        serial_comm_buttons.addWidget(self.refresh_button)
        serial_comm_buttons.addWidget(self.connect_button)
        serial_comm_layout.addLayout(serial_comm_buttons)

        serial_comm_group.setLayout(serial_comm_layout)

        # Serial Port Traffic group box (text area + clear button)
        serial_console_group = QGroupBox("Serial Port Traffic")
        serial_console_layout = QVBoxLayout()

        self.serial_console = QTextEdit()
        self.serial_console.setReadOnly(True)

        self.clear_serial_button = QPushButton("Clear")
        self.clear_serial_button.clicked.connect(self.serial_console.clear)

        serial_console_layout.addWidget(self.serial_console)
        serial_console_layout.addWidget(self.clear_serial_button)
        serial_console_group.setLayout(serial_console_layout)

        # Received Messages tab
        received_tab = QWidget()
        received_tab_layout = QVBoxLayout()

        # RX MANAGER title
        rx_title_label = QLabel("TER DECODER")
        rx_title_label.setAlignment(Qt.AlignCenter)
        rx_title_label.setStyleSheet("font-size: 24px; font-weight: bold;")
        received_tab_layout.addWidget(rx_title_label)

        # TER Content group
        ter_content_group = QGroupBox("Selected TER Content")
        ter_content_layout = QVBoxLayout()
        self.ter_content_display = QTextEdit()
        self.ter_content_display.setReadOnly(True)
        ter_content_layout.addWidget(self.ter_content_display)
        ter_content_group.setLayout(ter_content_layout)
        received_tab_layout.addWidget(ter_content_group)

        # Received TERs group
        received_ter_group = QGroupBox("Received TERs")
        received_ter_layout = QVBoxLayout()
        self.received_ter_table = QTableWidget()
        self.received_ter_table.setColumnCount(2)
        self.received_ter_table.setHorizontalHeaderLabels(["TER", "HEX"])
        self.received_ter_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.received_ter_table.horizontalHeader().setStretchLastSection(True)
        self.received_ter_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)

        # Connect selection change signal to handler
        self.received_ter_table.itemSelectionChanged.connect(self.display_selected_ter_content)

        received_ter_layout.addWidget(self.received_ter_table)
        received_ter_group.setLayout(received_ter_layout)
        received_tab_layout.addWidget(received_ter_group)

        received_tab.setLayout(received_tab_layout)

        # RX MANAGER title
        serial_title_label = QLabel("SERIAL MANAGER")
        serial_title_label.setAlignment(Qt.AlignCenter)
        serial_title_label.setStyleSheet("font-size: 24px; font-weight: bold;")

        # Tabs widget
        self.tabs = QTabWidget()
        # Add a container widget for the Serial Console tab that stacks both groups vertically
        serial_tab = QWidget()
        serial_tab_layout = QVBoxLayout()
       
        serial_tab_layout.addWidget(serial_title_label)
        serial_tab_layout.addWidget(serial_comm_group)
        serial_tab_layout.addWidget(serial_console_group)
        serial_tab.setLayout(serial_tab_layout)

        self.tabs.addTab(serial_tab, "Serial Console")
        self.tabs.addTab(received_tab, "Received Messages")

        # Status Messages group box (always visible at top)
        status_group = QGroupBox("Status Messages")
        status_layout = QVBoxLayout()
        self.status_console = QTextEdit()
        self.status_console.setReadOnly(True)
        self.clear_status_button = QPushButton("Clear")
        self.clear_status_button.clicked.connect(self.status_console.clear)
        status_layout.addWidget(self.status_console)
        status_layout.addWidget(self.clear_status_button)
        status_group.setLayout(status_layout)
        status_group.setMaximumHeight(300)

        # === Layouts ===
        main_layout = QHBoxLayout()

        left_col = QVBoxLayout()

        tx_label = QLabel("TEC ENCODER")
        tx_label.setAlignment(Qt.AlignCenter)
        tx_label.setStyleSheet("font-size: 24px; font-weight: bold;")

        packet_group = QGroupBox("TEC Setup")
        packet_layout = QFormLayout()
        packet_layout.addRow("Ground Station:", self.gs_selector)
        packet_layout.addRow("ECC:", self.ecc_radio_layout)
        packet_layout.addRow("Type:", self.type_selector)
        packet_layout.addRow("Task:", self.task_selector)
        packet_group.setLayout(packet_layout)

        packet_setup_group = QGroupBox("TEC Content")
        self.packet_setup_layout = QFormLayout()
        packet_setup_group.setLayout(self.packet_setup_layout)
        self.update_task_selector(0)
        self.update_packet_content_form()

        queued_group = QGroupBox("Queued TECs")
        queued_layout = QVBoxLayout()
        queued_layout.addWidget(self.queue_table)
        btn_row = QHBoxLayout()
        btn_row.addWidget(self.abort_next_button)
        btn_row.addWidget(self.abort_last_button)
        queued_layout.addLayout(btn_row)
        queued_layout.addWidget(self.execute_next_button)
        queued_group.setLayout(queued_layout)

        last_group = QGroupBox("Last TEC Status")
        last_layout = QVBoxLayout()
        self.last_tec_table = QTableWidget()
        self.last_tec_table.setColumnCount(2)
        self.last_tec_table.setHorizontalHeaderLabels(["TEC", "HEX"])
        self.last_tec_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.last_tec_table.horizontalHeader().setStretchLastSection(True)
        self.last_tec_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)
        last_layout.addWidget(self.last_tec_status)
        last_layout.addWidget(self.last_tec_status_description)
        last_layout.addWidget(self.last_tec_table)
        last_group.setLayout(last_layout)
        # last_group.setMaximumHeight(400)

        left_col.addWidget(tx_label)
        left_col.addWidget(packet_group)
        left_col.addWidget(packet_setup_group)
        left_col.addWidget(self.add_to_queue_button)
        left_col.addWidget(queued_group)
        left_col.addWidget(last_group)

        right_col = QVBoxLayout()
        right_col.addWidget(self.tabs)
        right_col.addWidget(status_group)

        main_layout.addLayout(left_col, 1)
        main_layout.addLayout(right_col, 1)

        self.setLayout(main_layout)

        # Timers
        self.serial_timer = QTimer()
        self.serial_timer.timeout.connect(self.read_serial)

        self.timeout_timer = QTimer()
        self.timeout_timer.timeout.connect(self.check_tec_timeout)

        self.sent_tecs = []
        self.new_line_pending = True


    # ========== AUTHENTICATION ==========
    
    # Check the password for the selected ground station
    def check_gs_password(self, index):
        # It dosn't make sense to check password if the GS is not selected
        if index == self.last_gs_index:
            return

        selected_gs = self.gs_selector.itemText(index)

        # Codification of passwords with hashing
        def hash_password(password: str) -> str:
            return hashlib.sha256(password.encode()).hexdigest()
    
        EXPECTED_HASHES = {
        "UniPD": "428a78083a063c44052773604b01378ed403d29fa913fa7ba48a9ef46a19d931",
        "Mobile": "e691608526fb575a015ebdd21c4d4676c6660ec9ed200a1fc301acf2cac7c18f"
        }

        password = "Oracolo42"
        print(hashlib.sha256(password.encode()).hexdigest())
        password, ok = QInputDialog.getText(
            self,
            "Insert Password",
            f"Insert Password for: '{selected_gs}':",
            QLineEdit.Password
        )

        user_hash = hash_password(password)
        expected_hash = EXPECTED_HASHES.get(selected_gs, "")

        if not ok or user_hash != expected_hash:
            QMessageBox.warning(self, "Access denied", "Wrong password!")
            self.gs_selector.blockSignals(True)
            self.gs_selector.setCurrentIndex(self.last_gs_index)
            self.gs_selector.blockSignals(False)
        else:
            self.last_gs_index = index


    # ========== SERIAL COMMUNICATION ==========

    # Connect to the selected serial port
    def connect_serial(self):
        port = self.port_selector.currentText()
        try:
            self.serial_conn = serial.Serial(port, 9600, timeout=0.1)
            self.set_serial_status(True)
            self.connect_button.setText("Disconnect")
            self.execute_next_button.setEnabled(True)
            # self.execute_next_button.setStyleSheet("background-color: rgba(0, 255, 0, 128);")
            self.log_status(f"[INFO] Connected to {port}")
            self.serial_timer.start(100)
        except Exception as e:
            self.log_status(f"[ERROR] Could not connect: {e}")
            self.set_serial_status(False)

    # Disconnect from the serial port
    def disconnect_serial(self):
        if self.serial_conn:
            self.serial_timer.stop()
            self.serial_conn.close()
            self.set_serial_status(False)
            self.connect_button.setText("Connect")
            self.execute_next_button.setEnabled(False)
            self.execute_next_button.setStyleSheet("")
            self.log_status("[INFO] Disconnected")

    # Refresh the list of available serial ports
    def refresh_ports(self):
        self.port_selector.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_selector.addItem(port.device)

    # Toggle serial connection state based on current status
    def toggle_connection(self):
        if self.serial_conn and self.serial_conn.is_open:
            self.disconnect_serial()
        else:
            self.connect_serial()

    # Read data from the serial port
    def read_serial(self):
        if self.serial_conn:
            try:
                while self.serial_conn.in_waiting:
                    # Read a full line from serial and decode it
                    line = self.serial_conn.readline().decode(errors='ignore').strip()

                    if not line:
                        continue  # skip empty lines

                    if line.startswith("Decoded:"):
                        # Put raw received line into serial_console
                        self.log_serial(f"[RX]: {line}")

                        hex_str = line[len("Decoded:"):].strip()
                        hex_parts = hex_str.split()

                        try:
                            packet_bytes = bytes(int(h, 16) for h in hex_parts)
                            decoded_packet = decode_packet(packet_bytes)

                            # Show decoded info for debug in status_console
                            self.log_status(f"[INFO] Packet decoded: {decoded_packet}")

                            # Pass to handler for ACK/NACK or other processing
                            self.handle_reception(decoded_packet, packet_bytes)

                        except Exception as e:
                            self.log_status(f"[ERROR] Packet not decoded: {e}")
                            error_type = type(e).__name__
                            error_msg = str(e)
                            tb = traceback.format_exc()
                            self.log_status(f"[ERROR] Packet not decoded: {error_type}: {error_msg}\n{tb}")

                    else:
                        # Normal lines go to serial_console (raw serial data)
                        self.log_serial(f"[RX]: {line}")

            except Exception as e:
                self.log_status(f"[ERROR] Reading failed: {e}")
                self.disconnect_serial()

    # Send message to status console with timestamp
    def log_status(self, message):
        timestamp = datetime.now().strftime("[%H:%M:%S]")
        self.status_console.append(f"{timestamp} {message}")
        
    # Send message to serial console with timestamp
    def log_serial(self, message):
        timestamp = datetime.now().strftime("[%H:%M:%S]")
        self.serial_console.append(f"{timestamp} {message}")


    # ========== STATUS VISUALIZATION ==========

    # Set the status light color and text based on connection status
    def set_serial_status(self, connected):
        bg_color = QColor('green') if connected else QColor('red')
        text_color = QColor('white')  # Always white for contrast in this case

        self.serial_status_label.setText("CONNECTED" if connected else "DISCONNECTED")
        self.serial_status_label.setStyleSheet(
            f"background-color: {bg_color.name()}; color: {text_color.name()}; font-weight: bold; padding: 5px;"
        )

    # Set the last TEC status label with appropriate styles and updates sent TEC queue
    def set_last_tec_status(self, status):
        display_status = status.upper()
        self.last_tec_status.setText(display_status)

        # Update the status of the last TEC in self.sent_tecs
        if self.sent_tecs:
            # Update status of the most recent TEC (the last appended)
            last = self.sent_tecs[-1]
            self.sent_tecs[-1] = (last[0], last[1], display_status)

        # Update the label style
        bg_color, text_color = self.get_color_for_status(display_status)
        self.last_tec_status.setStyleSheet(
            f"background-color: {bg_color.name()}; color: {text_color.name()}; font-weight: bold; padding: 5px;"
        )

        # Now repaint the whole last_tec_table, row by row, preserving each row's color based on stored status
        for row_idx, (_, _, row_status) in enumerate(reversed(self.sent_tecs)):
            bg, fg = self.get_color_for_status(row_status)
            bg_transparent = QColor(bg)
            bg_transparent.setAlpha(64)
            for col in range(self.last_tec_table.columnCount()):
                item = self.last_tec_table.item(row_idx, col)
                if item:
                    item.setBackground(bg_transparent)
                    # item.setForeground(fg)

    # Get the color for the status of the last TEC
    def get_color_for_status(self, status):
        status = status.upper()
        if status == "NO COMMS":
            bg_color = QColor("lightgray")
        elif status.startswith("WAITING") or status == "NO REPLY":
            bg_color = QColor("orange")
        elif status.startswith("ACK") or status.startswith("REPLY"):
            bg_color = QColor("green")
        else:
            bg_color = QColor("red")

        # Determine text color
        if bg_color in [QColor("green"), QColor("red")]:
            text_color = QColor("white")
        else:
            text_color = QColor("black")

        return bg_color, text_color
    

    # ========== DYNAMIC PACKET SETUP ==========

    # Update the task selector based on the selected type index
    def update_task_selector(self, index):
        self.task_selector.blockSignals(True)
        self.task_selector.clear()

        tasks_for_type = {label: code for label, code in TEC_TASKS.items()
                        if get_type_from_task(code) == index}

        self.task_selector.addItems(tasks_for_type.keys())

        self.task_selector.blockSignals(False)
        self.update_packet_content_form()

    # Update the packet content form based on selected type and task
    def update_packet_content_form(self):
        # Clear and remove all widgets and nested layouts from layout
        while self.packet_setup_layout.count():
            item = self.packet_setup_layout.takeAt(0)

            if item is None:
                continue

            widget = item.widget()
            layout = item.layout()

            if widget is not None:
                widget.deleteLater()
            elif layout is not None:
                # Recursively delete items in nested layouts
                while layout.count():
                    sub_item = layout.takeAt(0)
                    sub_widget = sub_item.widget()
                    if sub_widget is not None:
                        sub_widget.deleteLater()

        self.created_widgets.clear()  # Fully reset cache

        tec_name = self.task_selector.currentText()
        fields = INPUT_FIELDS_CONFIG.get(tec_name, [])

        for field in fields:
            label_text = field[0]
            widget_class = field[1]
            default_value = field[2] if len(field) > 2 else None
            options_or_min = field[3] if len(field) > 3 else None
            max_val = field[4] if len(field) > 4 else None
            right_text = field[5] if len(field) > 5 else None
            
            # Special case: full-width row label
            if label_text == "TEXT" and widget_class == QLabel:
                label = QLabel(default_value)
                # label.setStyleSheet("font-weight: bold")  # Optional: make it stand out
                self.packet_setup_layout.addRow(label)
                continue

            if widget_class in (QSpinBox, QDoubleSpinBox):
                spinbox = widget_class()
                if options_or_min is not None and max_val is not None:
                    spinbox.setRange(options_or_min, max_val)
                if default_value is not None:
                    spinbox.setValue(default_value)

                if right_text:
                    container = QWidget()
                    h_layout = QHBoxLayout(container)
                    h_layout.setContentsMargins(0, 0, 0, 0)
                    h_layout.setSpacing(5)

                    h_layout.addWidget(spinbox)
                    h_layout.addWidget(QLabel(right_text))

                    self.created_widgets[(tec_name, label_text)] = (container, spinbox)
                    self.packet_setup_layout.addRow(label_text, container)
                else:
                    self.created_widgets[(tec_name, label_text)] = spinbox
                    self.packet_setup_layout.addRow(label_text, spinbox)

            else:
                widget = widget_class()

                if default_value is not None:
                    if isinstance(widget, QPlainTextEdit):
                        widget.setPlainText(str(default_value))
                        widget.setMaximumHeight(200)
                    elif isinstance(widget, QComboBox):
                        if isinstance(options_or_min, list):
                            widget.addItems(options_or_min)
                            if default_value in options_or_min:
                                widget.setCurrentText(default_value)
                            # else:
                            #     widget.setCurrentIndex(0)
                        else:
                            widget.setCurrentText(str(default_value))
                    elif isinstance(widget, QTimeEdit):
                        widget.setDisplayFormat("HH:mm:ss")
                        if isinstance(default_value, QTime):
                            widget.setTime(default_value)

                    elif isinstance(widget, QDateTimeEdit):
                        if isinstance(default_value, QDateTime):
                            widget.setDateTime(default_value)
                    elif hasattr(widget, "setText"):
                        widget.setText(str(default_value))

                if right_text:
                    container = QWidget()
                    h_layout = QHBoxLayout(container)
                    h_layout.setContentsMargins(0, 0, 0, 0)
                    h_layout.setSpacing(5)

                    h_layout.addWidget(widget)
                    h_layout.addWidget(QLabel(right_text))

                    self.created_widgets[(tec_name, label_text)] = (container, widget)
                    self.packet_setup_layout.addRow(label_text, container)
                else:
                    self.created_widgets[(tec_name, label_text)] = widget
                    self.packet_setup_layout.addRow(label_text, widget)


    # ========== PACKET GENERATION ==========

    # Add the current packet to the TEC queue
    def add_to_queue(self):
        tec_label = self.task_selector.currentText()
        tec_code = TEC_TASKS.get(tec_label, 0)
        tec_name = get_task_label(TEC_TASKS, tec_code)

        input_widgets = {}
        rows = self.packet_setup_layout.rowCount()

        for row in range(rows):
            label_item = self.packet_setup_layout.itemAt(row, QFormLayout.LabelRole)
            field_item = self.packet_setup_layout.itemAt(row, QFormLayout.FieldRole)
            if not label_item or not field_item:
                continue

            label_widget = label_item.widget()
            field_widget = field_item.widget()

            if label_widget is None or field_widget is None:
                continue

            label_text = label_widget.text()

            # Check if field_widget itself is input widget
            if isinstance(field_widget, (QSpinBox, QDoubleSpinBox   , QPlainTextEdit, QComboBox, QDateTimeEdit, QTimeEdit, QLineEdit)):
                input_widgets[label_text] = field_widget
                continue

            # Check if the field widget is a container (like QWidget containing spinbox)
            if isinstance(field_widget, QWidget):
                # Try to find any supported input widget inside the container
                for widget_type in (QSpinBox, QDoubleSpinBox, QPlainTextEdit, QComboBox, QDateTimeEdit, QTimeEdit, QLineEdit):
                    found_widget = field_widget.findChild(widget_type)
                    if found_widget:
                        input_widgets[label_text] = found_widget
                        break

        print("Input widget keys:", list(input_widgets.keys()))
        for k, w in input_widgets.items():
            print(f"Label: '{k}', widget type: {type(w).__name__}")
        try:
            payload = build_payload(tec_code, input_widgets)
        except Exception as e:
            self.log_status(f"[ERROR] Failed to build payload: {e}")
            return

        try:
            ecc_enabled = self.ecc_enable_radio.isChecked()
            packet_bytes = build_packet(
            self.gs_selector.currentText(),
            tec_code,
            payload,
            ecc_enabled
            )
            tec_hex = ' '.join(f"{b:02X}" for b in packet_bytes)
        except Exception as e:
            self.log_status(f"[ERROR] Packet build failed: {e}")
            return

        self.tec_queue.append((tec_name, tec_hex))

        self.update_queue_display()
        self.log_status(f"[INFO] Added TEC to queue.")


    # ========== TEC QUEUE MANAGEMENT ==========

    # Update the queue display in the table widget
    def update_queue_display(self):
        self.queue_table.setRowCount(len(self.tec_queue))
        for row, (tec_name, hex_str) in enumerate(self.tec_queue):
            self.queue_table.setItem(row, 0, QTableWidgetItem(tec_name))
            self.queue_table.setItem(row, 1, QTableWidgetItem(hex_str))

    # Abort the last TEC in the queue
    def abort_last_tec(self):
        if not self.tec_queue:
            self.log_status("[INFO] No TECs to abort.")
            return

        removed_cmd = self.tec_queue.pop()  # Remove the last TEC in the queue
        self.update_queue_display()
        self.log_status(f"[INFO] Aborted TEC {removed_cmd[0]}.")

    # Abort the next TEC in the queue
    def abort_next_tec(self):
        if not self.tec_queue:
            self.log_status("[INFO] No TECs to abort.")
            return

        # Remove the first (next) TEC in the queue
        removed_cmd = self.tec_queue.pop(0)
        self.update_queue_display()
        self.log_status(f"[INFO] Aborted TEC {removed_cmd[0]}.")

    # Execute the next TEC in the queue
    def execute_next_tec(self):
        if not self.tec_queue:
            self.log_status("[INFO] No TECs in queue.")
            return

        if not self.serial_conn or not self.serial_conn.is_open:
            self.log_status("[ERROR] Not connected.")
            return

        # Get the TEC details
        tec_name = self.tec_queue[0][0]
        tec_hex = self.tec_queue[0][1]

        try:
            # Convert HEX back to bytes and send it
            hex_bytes = bytes.fromhex(tec_hex)
            self.serial_conn.write(hex_bytes + b'\n')

            # Add "go" string to execute TX
            self.serial_conn.write(b"go\n")

            # Log TEC execution and display message on serial console
            self.log_serial(f"[TX]: TEC {tec_name}: {tec_hex}")

            self.log_status(f"[INFO] Executed TEC {tec_name}.")

            # Save task code from 3rd byte (index 2) of hex_bytes for ACK comparison
            self.last_sent_tec = hex_bytes[2]

        except Exception as e:
            self.log_status(f"[ERROR] Failed to send TEC {tec_name}: {e}")

        # Log sent TEC to history with "WAITING" status
        self.sent_tecs.append((tec_name, tec_hex, "WAITING"))
        self.last_tec_table.insertRow(0)
        self.last_tec_table.setItem(0, 0, QTableWidgetItem(tec_name))
        self.last_tec_table.setItem(0, 1, QTableWidgetItem(tec_hex))

        # Remove sent packet from queue
        self.tec_queue.pop(0)
        self.update_queue_display()

        # Update last TEC status
        self.last_tec_status_description.setText("Waiting for ACK/NACK/REPLY...")
        self.set_last_tec_status(f"WAITING: 0 s")
        self.last_tec_sent_time = datetime.now()
        self.timeout_timer.start(1000)  # check every second
        self.execute_next_button.setEnabled(False) # disable until ACK/NACK/REPLY is received


    # ========== PACKET RECEPTION HANDLING ==========
    
    # Handle reception of a decoded packet
    def handle_reception(self, decoded_packet, packet_bytes):

        self.timeout_timer.stop()  # stop timeout check
        self.execute_next_button.setEnabled(True)  # re-enable next execution button

        # Extract fields from the decoded packet
        ter = decoded_packet["ter"]
        payload = decoded_packet["payload_bytes"]
        
        # Get the label of the last TEC sent
        label_item = self.last_tec_table.item(0, 0)  # Column 1: TEC label
        tec_label = label_item.text() if label_item else f"0x{tec_requested:02X}"
        
        # Update ACK/NACK status of the last TEC
        elapsed = (datetime.now() - self.last_tec_sent_time).total_seconds()

        if ter == TER_ACK:
            if payload == [self.last_sent_tec]:
                status = f"ACK in {elapsed:.2f} s"
                self.set_last_tec_status(status)
                self.last_tec_status_description.setText(f"TEC {tec_label} executed successfully.")
                self.log_status(f"[INFO] ACK received after {elapsed:.2f} s.")

            else:
                status = "INVALID ACK"
                self.set_last_tec_status(status)
                self.last_tec_status_description.setText(f"TEC {tec_label} seems to be executed, but ACK was invalid. PAYLOAD: {payload}")
                self.log_status(f"[WARN] Malformed ACK payload {payload} received after {elapsed:.2f} s.")

        elif ter == TER_NACK:
            status = "NACK"
            self.set_last_tec_status(status)

            if len(payload) == 2:
                tec_requested = payload[0]
                error_code = int.from_bytes([payload[1]], byteorder='big', signed=True)
                status_msg = PACKET_ERR_DESCRIPTION.get(error_code, f"Unknown error code: {error_code}")

                self.last_tec_status_description.setText(f"TEC {tec_label} was not executed. ERROR: {status_msg}")
                self.log_status(f"[WARN] NACK received. Error: {status_msg}")
            
            else:
                self.last_tec_status_description.setText("Malformed NACK payload.")
                self.log_status("[ERROR] Malformed NACK payload.")

        elif ter == TER_LORA_PING:
            status = f"REPLY in {elapsed:.2f} s"
            self.set_last_tec_status(status)
            self.last_tec_status_description.setText(f"TEC {tec_label} executed successfully, requested data received.")
            self.log_status(f"[INFO] REPLY received after {elapsed:.2f} s.")

        else:
            status = "UNEXPECTED TER"
            self.set_last_tec_status(status)
            self.last_tec_status_description.setText(f"Received unexpected TER: {ter:02X}")
            self.log_status(f"[WARN] Received unexpected TER: {ter:02X}.")

        # Add to received TERs table
        self.received_ter_table.insertRow(0)

        ter_label = get_task_label(TER_TASKS, ter)
        self.received_ter_table.setItem(0, 0, QTableWidgetItem(ter_label))

        ter_hex = ' '.join(f"{b:02X}" for b in packet_bytes)
        self.received_ter_table.setItem(0, 1, QTableWidgetItem(ter_hex))

    # Check if the last TEC sent has timed out
    def check_tec_timeout(self):
        elapsed = (datetime.now() - self.last_tec_sent_time).total_seconds()
        if elapsed > RX_TIMEOUT:
            self.timeout_timer.stop()
            self.execute_next_button.setEnabled(True)  # re-enable next execution button
            self.set_last_tec_status("NO REPLY")
            self.last_tec_status_description.setText(f"No reply received after {RX_TIMEOUT} s, check if command in blind is enabled.")
            self.log_status(f"[WARN] Timeout: No reply received after {RX_TIMEOUT} s.")
        else:
            self.set_last_tec_status(f"WAITING: {round(elapsed)} s")


    # ========== PACKET VISUALIZATION ==========

    def display_selected_ter_content(self):
        self.ter_content_display.clear()
        selected_items = self.received_ter_table.selectedItems()

        if not selected_items:
            return

        selected_rows = sorted(set(item.row() for item in selected_items))

        for row in selected_rows:
            item_label = self.received_ter_table.item(row, 0)
            item_hex = self.received_ter_table.item(row, 1)

            ter_hex = item_hex.text() if item_hex else "HEX error"
            ter_label = item_label.text() if item_label else "TER error"
            self.ter_content_display.append(f"<b>PACKET > {ter_hex}</b>")

            try:
                packet_bytes = [int(byte, 16) for byte in ter_hex.split()]
                ter_decoded = decode_packet(packet_bytes)

                # Display header information
                self.ter_content_display.append(f"<b>HEADER > {ter_hex[:PACKET_HEADER_LENGTH * 3]}</b>")

                source_label = next((name for name, code in TX_SOURCES.items() if code == ter_decoded['station_id']), f"Unknown {ter_decoded['station_id']}")
                self.ter_content_display.append(f"Source: {source_label}")

                ecc_label = "Enabled" if ter_decoded['ecc_enabled'] else "Disabled"
                self.ter_content_display.append(f"ECC: {ecc_label}")

                self.ter_content_display.append(f"TER: {ter_label}")

                timestamp = ter_decoded['timestamp']
                try:
                    timestamp_label = datetime.fromtimestamp(timestamp).strftime('%Y-%m-%d %H:%M:%S')
                except Exception:
                    timestamp_label = str(timestamp)
                self.ter_content_display.append(f"Timestamp: {timestamp_label}")

                self.ter_content_display.append(f"MAC: {ter_decoded['mac']}")

                self.ter_content_display.append(f"Payload Length: {ter_decoded['payload_length']}")

                # Display payload information
                self.ter_content_display.append(f"<b>PAYLOAD > {ter_hex[PACKET_HEADER_LENGTH * 3:]}</b>")
                ter = ter_decoded['ter']
                payload_bytes = ter_decoded['payload_bytes']

                if ter == TER_BEACON:
                    self.ter_content_display.append("Type: Beacon")
                    # Optionally decode payload if known

                elif ter == TER_ACK:
                    if payload_bytes:
                        tec_executed = payload_bytes[0]
                        tec_executed_label = get_task_label(TEC_TASKS, tec_executed)

                        self.ter_content_display.append(f"TEC executed: {tec_executed_label}")

                elif ter == TER_NACK:
                    if len(payload_bytes) == 2:
                        tec_requested = payload_bytes[0]
                        error_code = int.from_bytes([payload_bytes[1]], byteorder='big', signed=True)
                        error_msg = PACKET_ERR_DESCRIPTION.get(error_code, f"Unknown error code: {error_code}")

                        self.ter_content_display.append(f"TEC not executed: {tec_requested:02X}")
                        self.ter_content_display.append(f"Error: {error_msg}")

                    else:
                        self.ter_content_display.append("Malformed NACK payload.")

                elif ter == TER_LORA_PING:
                    rssi_bytes = bytes(payload_bytes[0:4])
                    rssi = struct.unpack(">f", rssi_bytes)[0]
                    snr_bytes = bytes(payload_bytes[4:8])
                    snr = struct.unpack(">f", snr_bytes)[0]
                    freq_shift_bytes = bytes(payload_bytes[8:12])
                    freq_shift = struct.unpack(">f", freq_shift_bytes)[0]


                    self.ter_content_display.append(f"RSSI: {rssi:.2f} dBm")
                    self.ter_content_display.append(f"SNR: {snr:.2f} dB")
                    self.ter_content_display.append(f"Frequency Shift: {freq_shift:.2f} Hz")

                else:
                    self.ter_content_display.append("Type: Unknown or not handled")
            except Exception as e:
                self.ter_content_display.append(f"[Error decoding TER: {e}]")

            self.ter_content_display.append("<br>")


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.resize(1000, 800)
    window.show()
    sys.exit(app.exec_())
