import struct
import sys
import serial
import serial.tools.list_ports
from datetime import datetime
from PyQt5.QtWidgets import *
from PyQt5.QtGui import QColor, QPalette, QTextCursor
from PyQt5.QtCore import QTimer, Qt
import hashlib
import hmac

import traceback

# ========== CONSTANTS AND CONFIGURATION ==========
# Packet configuration
PACKET_HEADER_LENGTH = 12 # 4 bytes for header + 4 bytes for MAC + 4 bytes for timestamp
PACKET_PAYLOAD_MAX = 98  # maximum payload length in bytes

# MAC configuration
SECRET_KEY = 0xA1B2C3D4

# RS encoding configuration
BYTE_RS_OFF = 0x55
BYTE_RS_ON = 0xAA

# Error codes
PACKET_ERR_NONE = 0
PACKET_ERR_RS = -1
PACKET_ERR_DECODE = -2
PACKET_ERR_LENGTH = -3
PACKET_ERR_MAC = -4
PACKET_ERR_CMD_FULL = -5

PACKET_ERR_DESCRIPTION = {
    PACKET_ERR_NONE: "Unexpected behavior: no error code provided.",
    PACKET_ERR_RS: "Reed-Solomon decoding failed.",
    PACKET_ERR_DECODE: "Packet decode error.",
    PACKET_ERR_LENGTH: "Invalid packet length.",
    PACKET_ERR_MAC: "MAC verification failed.",
    PACKET_ERR_CMD_FULL: "Command buffer full."
}

# ========== HELPER FUNCTIONS ==========

# Build the payload based on type index and task name
def build_payload(TEC_name, input_widgets):
    if TEC_name == "OBC reboot":
        payload = b''

    elif TEC_name == "Exit state":
        from_state = input_widgets["From state:"].value() & 0x0F
        to_state = input_widgets["To state:"].value() & 0x0F
        byte_val = (from_state << 4) | to_state
        payload = bytes([byte_val])

    elif TEC_name == "Variable change":
        address = input_widgets["Address:"].value() & 0xFF
        value = input_widgets["Value:"].value() & 0xFF
        payload = bytes([address, value])

    elif TEC_name == "EPS reboot":
        payload = b''

    elif TEC_name == "ADCS reboot":
        payload = b''

    elif TEC_name == "TLE update":
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

    else:
        payload = b''

    return payload

# Simple MAC function
def simple_mac(timestamp, key):
    x = (timestamp ^ key) & 0xFFFFFFFF
    x = ((x >> 16) ^ x) * 0x45d9f3b
    x = x & 0xFFFFFFFF
    x = ((x >> 16) ^ x) * 0x45d9f3b
    x = x & 0xFFFFFFFF
    x = (x >> 16) ^ x
    return x & 0xFFFFFFFF

# Build the full packet with header, payload, and ECC(for transmission)
def build_packet(gs_text, TEC_type, TEC_name, payload_bytes, ecc_enabled):
    # === Byte 0: Station ID ===
    if gs_text == "UniPD":
        byte_station = 0x01
    elif gs_text == "Mobile":
        byte_station = 0x02
    else:
        raise ValueError(f"Invalid Ground Station: {gs_text}")

    # === Byte 1: ECC Flag ===
    byte_ecc = BYTE_RS_ON if ecc_enabled else BYTE_RS_OFF

    # === Byte 2: TEC Type (bits 1-2), Task Code (bits 3-8) ===
    byte_command = MainWindow.TECs.get(TEC_type, {}).get(TEC_name, 0)  # directly return TEC code

    # === Byte 3: Payload length ===
    payload_length = len(payload_bytes)
    if payload_length > PACKET_PAYLOAD_MAX:
        raise ValueError(f"Payload too long (max {PACKET_PAYLOAD_MAX} bytes)")
    byte_payload_length = payload_length

    # === Byte 4-7: MAC ===
    unix_time = int(datetime.now().timestamp())

    mac = simple_mac(unix_time, SECRET_KEY)
    bytes_mac = mac.to_bytes(4, byteorder='big')

    # === Byte 8-11: UNIX timestamp ===
    bytes_time = unix_time.to_bytes(4, byteorder='big')

    # === Final packet ===
    header = bytes([
        byte_station,
        byte_ecc,
        byte_command,
        byte_payload_length
    ])

    return header + bytes_mac + bytes_time + payload_bytes

# Decode a packet from bytes(for reception) TODO: update
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

    # Byte 2: TRC (raw int)
    TRC = packet_bytes[2]

    # Byte 3: Payload length
    payload_length = packet_bytes[3]

    # Ensure entire packet is present
    expected_len = PACKET_HEADER_LENGTH + payload_length
    if len(packet_bytes) < expected_len:
        raise ValueError(f"Packet too short for payload: length {len(packet_bytes)} < expected {expected_len}")

    # Bytes 4–7: MAC (int)
    bytes_mac = packet_bytes[4:7]
    mac = int.from_bytes(bytes_mac, byteorder='big')

    # Bytes 8–11: Timestamp (int)
    bytes_time = packet_bytes[8:11]
    timestamp = int.from_bytes(bytes_time, byteorder='big')

    # Payload: list of ints
    if payload_length > 0:
        payload_bytes = list(packet_bytes[12:12 + payload_length])
    else:
        payload_bytes = []

    return {
        "station_id": station_id,
        "ecc_enabled": ecc_enabled,
        "TRC": TRC,
        "payload_length": payload_length,
        "mac": mac,
        "timestamp": timestamp,
        "payload_bytes": payload_bytes  # always a list of ints
    }


# ========== MAIN WINDOW CLASS ==========

class MainWindow(QWidget):
    # Centralized task definitions + codes
    TECs = {
        0: {  # HK
            "OBC reboot": 1,
            "Exit state": 2,
            "Variable change": 3,
            "Set clock": 4,
            "EPS reboot": 8,
            "ADCS reboot": 16,
            "TLE update": 17
        },
        1: {  # DAQ
            "Start DAQ": 68,
            "Stop DAQ": 69
        },
        2: {  # PE
            "Execute A": 132,
            "Execute B": 133
        },
        3: {  # DT
            "Upload": 193,
            "Download": 194
        }
    }

    # Define your form input configs here:
    # Each key is a (TEC_type, TEC_name) tuple
    # Each value is a list of input field definitions:
    # (label, widget_type, optional widget args)
    INPUT_FIELDS_CONFIG = {
        "OBC reboot": [
            ("No configuration available, execution is immediate", QLabel, None, None, None, None)
        ],
        "Exit state": [
            # ("Delay:", QSpinBox, 0, 0, 100, "[s]"),
            ("From state:", QSpinBox, 0, 0, 14, "0-14"),
            ("To state:", QSpinBox, 1, 0, 14, "0-14"),
        ],
        "Variable change": [
            ("Address 1:", QSpinBox, 0, 0, 254, "0-254"),
            ("Value 1:", QSpinBox, 0, 0, 254, "0-254"),
            ("Address 2:", QSpinBox, 0, 0, 254, "0-254"),
            ("Value 2:", QSpinBox, 0, 0, 254, "0-254"),
            ("Address 3:", QSpinBox, 0, 0, 254, "0-254"),
            ("Value 3:", QSpinBox, 0, 0, 254, "0-254"),
            ("Address 4:", QSpinBox, 0, 0, 254, "0-254"),
            ("Value 4:", QSpinBox, 0, 0, 254, "0-254"),
            ("Address 5:", QSpinBox, 0, 0, 254, "0-254"),
            ("Value 5:", QSpinBox, 0, 0, 254, "0-254"),
            ("Address 6:", QSpinBox, 0, 0, 254, "0-254"),
            ("Value 6:", QSpinBox, 0, 0, 254, "0-254"),
            ("Address 7:", QSpinBox, 0, 0, 254, "0-254"),
            ("Value 7:", QSpinBox, 0, 0, 254, "0-254"),
            ("Address 8:", QSpinBox, 0, 0, 254, "0-254"),
            ("Value 8:", QSpinBox, 0, 0, 254, "0-254")
        ],
        "Set clock": [
            ("No configuration available, execution is immediate", QLabel, None, None, None, None)
        ],
        "EPS reboot": [
            ("No configuration available, execution is immediate", QLabel, None, None, None, None)
        ],
        "ADCS reboot": [
            ("No configuration available, execution is immediate", QLabel, None, None, None, None)
        ],
        "TLE update": [
            ("TLE Data:", QPlainTextEdit, "Line 1\nLine 2", None, None, None)
        ]

        
        # TLE form(TO CHECK NOT SURE): 
        # line 1: 25544U 98067A   24174.42871528  .00005597  00000+0  10490-3 0  9990
        # line2: 25544  51.6425 121.0145 0005864  64.1487  54.4007 15.49923611452417
    }

    # Windows graphical user interface class
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ESP32 Serial GUI")
        self.serial_conn = None
        self.command_queue = []
        self.created_widgets = {}

        # === Left Panel Components ===
        self.gs_selector = QComboBox()
        self.gs_selector.addItems(["None", "UniPD", "Mobile"])
        self.last_gs_index = 0  # default is first item (UniPD)
        self.gs_selector.currentIndexChanged.connect(self.check_gs_password)

        # ECC Radio Buttons
        self.ecc_enable_radio = QRadioButton("Enabled")
        self.ecc_disable_radio = QRadioButton("Disabled")
        self.ecc_disable_radio.setChecked(True)  # Default selection

        # Group the radio buttons
        self.ecc_radio_group = QButtonGroup()
        self.ecc_radio_group.addButton(self.ecc_enable_radio)
        self.ecc_radio_group.addButton(self.ecc_disable_radio)

        # Layout for ECC radio buttons
        self.ecc_radio_layout = QHBoxLayout()
        self.ecc_radio_layout.addWidget(self.ecc_enable_radio)
        self.ecc_radio_layout.addWidget(self.ecc_disable_radio)

        self.type_selector = QComboBox()
        self.type_selector.addItems([
            "[HK] Housekeeping",
            "[DAQ] Data Acquisition",
            "[PE] Payload Execution",
            "[DT] Data Transfer"
        ])
        self.type_selector.currentIndexChanged.connect(self.update_task_selector)

        self.task_selector = QComboBox()
        self.task_selector.currentIndexChanged.connect(self.update_packet_content_form)

        self.add_to_queue_button = QPushButton("Add to Queue")
        self.add_to_queue_button.clicked.connect(self.add_to_queue)

        self.queue_table = QTableWidget()
        self.queue_table.setColumnCount(3)
        self.queue_table.setHorizontalHeaderLabels(["ID", "Command", "HEX"])
        self.queue_table.horizontalHeader().setStretchLastSection(True)
        self.queue_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.queue_table.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        self.queue_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)  # ID

        # Abort command buttons
        self.abort_last_button = QPushButton("Abort Last")
        self.abort_last_button.clicked.connect(self.abort_last_command)
        # self.abort_last_button.setStyleSheet("background-color: rgba(255, 0, 0, 128);")

        self.abort_next_button = QPushButton("Abort Next")
        self.abort_next_button.clicked.connect(self.abort_next_command)
        # self.abort_next_button.setStyleSheet("background-color: rgba(255, 0, 0, 128);")

        # Execute next command button
        self.execute_next_button = QPushButton("Execute Next")
        self.execute_next_button.clicked.connect(self.execute_next_command)
        self.execute_next_button.setEnabled(False)
        
        # Last command status
        self.last_command_status = QLabel("NO COMMS")
        self.last_command_status.setAlignment(Qt.AlignCenter)
        self.last_command_status.setStyleSheet("background-color: lightgray; color: black; font-weight: bold; padding: 5px;")

        # Last command description
        self.last_command_error = QLabel("")

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

        # Status console for events
        self.status_console = QTextEdit()
        self.status_console.setReadOnly(True)
        self.clear_status_button = QPushButton("Clear")
        self.clear_status_button.clicked.connect(self.status_console.clear)

        # Serial console for traffic
        self.serial_console = QTextEdit()
        self.serial_console.setReadOnly(True)
        self.clear_serial_button = QPushButton("Clear")
        self.clear_serial_button.clicked.connect(self.serial_console.clear)


        # === Layouts ===
        main_layout = QHBoxLayout()

        # === LEFT PANEL ===
        left_col = QVBoxLayout()

        # TEC Setup group
        packet_group = QGroupBox("TEC Setup")
        packet_layout = QFormLayout()
        packet_layout.addRow("Ground Station:", self.gs_selector)
        packet_layout.addRow("ECC:", self.ecc_radio_layout)
        packet_layout.addRow("Type:", self.type_selector)
        packet_layout.addRow("Task:", self.task_selector)
        packet_group.setLayout(packet_layout)
        packet_group.adjustSize()
        packet_group.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Minimum)

        # TEC Content group
        packet_setup_group = QGroupBox("TEC Content")
        self.packet_setup_layout = QFormLayout()
        packet_setup_group.setLayout(self.packet_setup_layout)
        self.update_task_selector(0)  # Initialize with options for the first type
        self.update_packet_content_form()
        packet_setup_group.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Minimum)


        # Queued TECs group
        queued_commands_group = QGroupBox("Queued TECs")
        queued_commands_layout = QVBoxLayout()
        queued_commands_layout.addWidget(self.queue_table)

        # Add buttons inside this group box
        button_row = QHBoxLayout()
        button_row.addWidget(self.abort_next_button)
        button_row.addWidget(self.abort_last_button)
        queued_commands_layout.addLayout(button_row)
        queued_commands_layout.addWidget(self.execute_next_button)
        queued_commands_group.setLayout(queued_commands_layout)
        # queued_commands_group.setMaximumHeight(500)
        self.queue_table.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        
        # Group box for Last Command Status
        last_command_group = QGroupBox("Last TEC Status")
        last_command_layout = QVBoxLayout()

        # Create table for last command (same columns as queue)
        self.last_command_table = QTableWidget()
        self.last_command_table.setColumnCount(3)
        self.last_command_table.setHorizontalHeaderLabels(["ID", "Command", "HEX"])
        self.last_command_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.last_command_table.horizontalHeader().setStretchLastSection(True)
        self.last_command_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)

        last_command_layout.addWidget(self.last_command_status)
        last_command_layout.addWidget(self.last_command_error)
        last_command_layout.addWidget(self.last_command_table)
        last_command_group.setLayout(last_command_layout)

        # Add groups and buttons
        left_col.addWidget(packet_group, 0)
        left_col.addWidget(packet_setup_group, 0)
        left_col.addWidget(self.add_to_queue_button, 0)
        left_col.addWidget(queued_commands_group, 1)
        left_col.addWidget(last_command_group)

        # === RIGHT PANEL ===
        serial_group = QGroupBox("Serial Communication")
        serial_layout = QVBoxLayout()

        serial_layout.addWidget(self.serial_status_label)

        form_layout = QFormLayout()
        form_layout.addRow("Select COM Port:", self.port_selector)
        serial_layout.addLayout(form_layout)

        button_row = QHBoxLayout()
        button_row.addWidget(self.refresh_button)
        button_row.addWidget(self.connect_button)
        serial_layout.addLayout(button_row)

        serial_group.setLayout(serial_layout)

        status_group = QGroupBox("Status Messages")
        status_layout = QVBoxLayout()
        status_layout.addWidget(self.status_console)
        # status_layout.addWidget(self.clear_status_button, alignment=Qt.AlignRight)
        status_layout.addWidget(self.clear_status_button)
        status_group.setLayout(status_layout)

        traffic_group = QGroupBox("Serial Port Traffic")
        traffic_layout = QVBoxLayout()
        traffic_layout.addWidget(self.serial_console)
        # traffic_layout.addWidget(self.clear_serial_button, alignment=Qt.AlignRight)
        traffic_layout.addWidget(self.clear_serial_button)
        traffic_group.setLayout(traffic_layout)
        self.new_line_pending = True  # Flag to insert timestamp only at line start

        right_col = QVBoxLayout()
        right_col.addWidget(serial_group)
        right_col.addWidget(status_group)
        right_col.addWidget(traffic_group)

        main_layout.addLayout(left_col, 1)
        main_layout.addLayout(right_col, 1)

        self.setLayout(main_layout)

        # Timer for serial read
        self.serial_timer = QTimer()
        self.serial_timer.timeout.connect(self.read_serial)

        # Timer for command timeout
        self.timeout_timer = QTimer()
        self.timeout_timer.timeout.connect(self.check_command_timeout)

        # Sent commands history
        self.sent_commands = []  # To store tuples: (cmd_id, tec_str, hex_str, status)



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
                            self.handle_reception(decoded_packet)

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

    # Set the last command status label with appropriate styles and updates sent command queue
    def set_last_command_status(self, status):
        display_status = status.upper()
        self.last_command_status.setText(display_status)

        # Update the status of the last command in self.sent_commands
        if self.sent_commands:
            # Update status of the most recent command (the last appended)
            last = self.sent_commands[-1]
            self.sent_commands[-1] = (last[0], last[1], last[2], display_status)

        # Update the label style
        bg_color, text_color = self.get_color_for_status(display_status)
        self.last_command_status.setStyleSheet(
            f"background-color: {bg_color.name()}; color: {text_color.name()}; font-weight: bold; padding: 5px;"
        )

        # Now repaint the whole last_command_table, row by row, preserving each row's color based on stored status
        for row_idx, (_, _, _, row_status) in enumerate(reversed(self.sent_commands)):
            bg, fg = self.get_color_for_status(row_status)
            bg_transparent = QColor(bg)
            bg_transparent.setAlpha(64)
            for col in range(self.last_command_table.columnCount()):
                item = self.last_command_table.item(row_idx, col)
                if item:
                    item.setBackground(bg_transparent)
                    # item.setForeground(fg)

    # Get the color for the status of the last command
    def get_color_for_status(self, status):
        status = status.upper()
        if status == "NO COMMS":
            bg_color = QColor("lightgray")
        elif status.startswith("WAITING"):
            bg_color = QColor("orange")
        elif status.startswith("ACK"):
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

        # Get the task names for this type index or empty dict if none
        tasks_for_type = self.TECs.get(index, {})
        self.task_selector.addItems(tasks_for_type.keys())

        self.task_selector.blockSignals(False)
        self.update_packet_content_form()

    # Update the packet content form based on selected type and task
    def update_packet_content_form(self):
    # Clear and remove all widgets from layout and memory
        while self.packet_setup_layout.count():
            item = self.packet_setup_layout.takeAt(0)
            widget = item.widget()
            if widget:
                widget.deleteLater()

        self.created_widgets.clear()  # Fully reset cache

        # TEC_type = self.type_selector.currentIndex()
        TEC_name = self.task_selector.currentText()
        # key = (TEC_type, TEC_name)

        fields = self.INPUT_FIELDS_CONFIG.get(TEC_name, [])

        for field in fields:
            label_text = field[0]
            widget_class = field[1]
            default_value = field[2] if len(field) > 2 else None
            min_val = field[3] if len(field) > 3 else None
            max_val = field[4] if len(field) > 4 else None
            right_text = field[5] if len(field) > 5 else None

            if widget_class in (QSpinBox, QDoubleSpinBox):
                spinbox = widget_class()
                if min_val is not None and max_val is not None:
                    spinbox.setRange(min_val, max_val)
                if default_value is not None:
                    spinbox.setValue(default_value)

                if right_text:
                    container = QWidget()
                    h_layout = QHBoxLayout(container)
                    h_layout.setContentsMargins(0, 0, 0, 0)
                    h_layout.setSpacing(5)

                    h_layout.addWidget(spinbox)
                    h_layout.addWidget(QLabel(right_text))

                    self.created_widgets[(TEC_name, label_text)] = (container, spinbox)
                    self.packet_setup_layout.addRow(label_text, container)
                else:
                    self.created_widgets[(TEC_name, label_text)] = spinbox
                    self.packet_setup_layout.addRow(label_text, spinbox)

            else:
                widget = widget_class()
                if default_value is not None:
                    if isinstance(widget, QPlainTextEdit):
                        widget.setPlainText(str(default_value))
                        widget.setMaximumHeight(200)
                    else:
                        widget.setText(str(default_value))

                self.created_widgets[(TEC_name, label_text)] = widget
                self.packet_setup_layout.addRow(label_text, widget)


    # ========== PACKET GENERATION ==========

    # Add the current packet to the command queue
    def add_to_queue(self):
        TEC_type = self.type_selector.currentIndex()
        # Extract only the text between brackets [] for type_label
        type_text = self.type_selector.currentText()
        if "[" in type_text and "]" in type_text:
            type_label = type_text[type_text.find("["):type_text.find("]")+1]
        else:
            type_label = type_text
        TEC_label = self.task_selector.currentText()

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

            # Check if the field widget is a container (like QWidget containing spinbox)
            if isinstance(field_widget, QWidget):
                # Try to find spinbox inside container
                spinbox = field_widget.findChild(QSpinBox)
                if spinbox:
                    input_widgets[label_text] = spinbox
                    continue

                # Try to find plain text edit inside container
                plain_text = field_widget.findChild(QPlainTextEdit)
                if plain_text:
                    input_widgets[label_text] = plain_text
                    continue

            # If not a container, check if field_widget itself is input widget
            if isinstance(field_widget, (QSpinBox, QLineEdit, QPlainTextEdit)):
                input_widgets[label_text] = field_widget
                    
        try:
            payload = build_payload(TEC_label, input_widgets)
        except Exception as e:
            self.log_status(f"[ERROR] Failed to build payload: {e}")
            return

        cmd_id = 1 if not self.command_queue else self.command_queue[-1][0] + 1

        # Only one packet will be sent; no splitting needed
        TEC_str = f"{type_label} {TEC_label}"

        try:
            ecc_enabled = self.ecc_enable_radio.isChecked()
            packet_bytes = build_packet(
            self.gs_selector.currentText(),
            TEC_type,
            TEC_label,
            payload,
            ecc_enabled
            )
            hex_repr = ' '.join(f"{b:02X}" for b in packet_bytes)
        except Exception as e:
            self.log_status(f"[ERROR] Packet build failed: {e}")
            return

        self.command_queue.append((cmd_id, TEC_str, hex_repr))

        self.update_queue_display()
        self.log_status(f"[INFO] Added CMD {cmd_id} to queue.")


    # ========== COMMAND QUEUE MANAGEMENT ==========

    # Update the queue display in the table widget
    def update_queue_display(self):
        self.queue_table.setRowCount(len(self.command_queue))
        for row, (cmd_id, TEC_str, hex_str) in enumerate(self.command_queue):
            self.queue_table.setItem(row, 0, QTableWidgetItem(str(cmd_id)))
            self.queue_table.setItem(row, 1, QTableWidgetItem(TEC_str))
            self.queue_table.setItem(row, 2, QTableWidgetItem(hex_str))

    # Abort the last command in the queue
    def abort_last_command(self):
        if not self.command_queue:
            self.log_status("[INFO] No commands to abort.")
            return

        last_cmd_id = self.command_queue[-1][0]
        last_command_name = self.command_queue[-1][2]
        self.command_queue = [entry for entry in self.command_queue if entry[0] != last_cmd_id]
        self.update_queue_display()
        self.log_status(f"[INFO] Aborted CMD {last_cmd_id} {last_command_name} packet.")

    # Abort the next command in the queue
    def abort_next_command(self):
        if not self.command_queue:
            self.log_status("[INFO] No commands to abort.")
            return

        next_cmd_id = self.command_queue[0][0]
        next_cmd_name = self.command_queue[0][2]
        self.command_queue = [entry for entry in self.command_queue if entry[0] != next_cmd_id]
        self.update_queue_display()
        self.log_status(f"[INFO] Aborted CMD {next_cmd_id} {next_cmd_name} packet.")

    # Execute the next command in the queue
    def execute_next_command(self):
        if not self.command_queue:
            self.log_status("[INFO] No commands in queue.")
            return

        if not self.serial_conn or not self.serial_conn.is_open:
            self.log_status("[ERROR] Not connected.")
            return

        # Get the CMD details of the next group
        cmd_id = self.command_queue[0][0]
        cmd_name = self.command_queue[0][1]
        cmd_hex = self.command_queue[0][2]

        try:
            # Convert HEX back to bytes and send it
            hex_bytes = bytes.fromhex(cmd_hex)
            self.serial_conn.write(hex_bytes + b'\n')

            # Add "go" string to execute TX
            self.serial_conn.write(b"go\n")
            
            # Log command execution and display message on serial console
            self.log_serial(f"[TX]: CMD {cmd_id} -> {cmd_name}")

            self.log_status(f"[INFO] Executed CMD {cmd_id} {cmd_name}.")

            # Save task code from 3rd byte (index 2) of hex_bytes for ACK comparison
            self.last_sent_command = hex_bytes[2]

        except Exception as e:
            self.log_status(f"[ERROR] Failed to send CMD {cmd_id} {cmd_name}: {e}")

        # Log sent command to history with "WAITING" status
        self.sent_commands.append((cmd_id, cmd_name, cmd_hex, "WAITING"))
        
        # Insert a new row at the top
        self.last_command_table.insertRow(0)

        # Fill cells
        self.last_command_table.setItem(0, 0, QTableWidgetItem(str(cmd_id)))
        self.last_command_table.setItem(0, 1, QTableWidgetItem(cmd_name))
        self.last_command_table.setItem(0, 2, QTableWidgetItem(cmd_hex))

        # Remove sent packet from queue
        self.command_queue.pop(0)
        self.update_queue_display()

        # Update last command status
        self.last_command_error.setText("")
        self.set_last_command_status(f"WAITING: 0 s")
        self.last_command_sent_time = datetime.now()
        self.timeout_timer.start(1000)  # check every second


    # ========== PACKET RECEPTION HANDLING ==========
    
    # Handle reception of a decoded packet
    def handle_reception(self, decoded_packet):

        self.timeout_timer.stop()  # Stop timeout check

        TRC = decoded_packet["TRC"]
        payload = decoded_packet["payload_bytes"]
        elapsed = (datetime.now() - self.last_command_sent_time).total_seconds()

        # self.log_status(f"[DEBUG] TRC: 0x{TRC:02X}, payload: {payload}, last_sent_command: {self.last_sent_command}")
        
        # Determine status string
        if TRC == 0x31 and payload == [self.last_sent_command]:
            status = f"ACK in {elapsed:.2f} s"
            self.set_last_command_status(status)
            self.log_status(f"[INFO] ACK received after {elapsed:.2f} s")

        elif TRC == 0x31 and payload != [self.last_sent_command]:
            status = "INVALID ACK"
            self.set_last_command_status(status)

        elif TRC == 0x32:
            status = "NACK"
            self.set_last_command_status(status)

            if len(payload) == 2:
                cmd_code = payload[0]
                error_code = int.from_bytes([payload[1]], byteorder='big', signed=True)

                error_msg = PACKET_ERR_DESCRIPTION.get(error_code, f"Unknown error code: {error_code}")
                
                label_item = self.last_command_table.item(0, 1)  # Column 1: Command label
                command_label = label_item.text() if label_item else f"0x{cmd_code:02X}"

                self.last_command_error.setText(f"Command {command_label} was not executed. ERROR: {error_msg}")
                self.log_status(f"[WARN] NACK received. Error: {error_msg}")
            else:
                self.last_command_error.setText("Malformed NACK payload.")
                self.log_status("[ERROR] Malformed NACK payload.")

        else:
            status = "UNKNOWN ERROR"
            self.set_last_command_status(status)

    # Check if the last command sent has timed out
    def check_command_timeout(self):
        elapsed = (datetime.now() - self.last_command_sent_time).total_seconds()
        if elapsed > 10:
            self.timeout_timer.stop()
            self.set_last_command_status("NO REPLY")
            self.log_status(f"[WARN] Timeout: No reply received after 10 seconds")
        else:
            self.set_last_command_status(f"WAITING: {round(elapsed)} s")


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.resize(1000, 800)
    window.show()
    sys.exit(app.exec_())
