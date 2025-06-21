import sys
import serial
import serial.tools.list_ports
from datetime import datetime
from PyQt5.QtWidgets import *
from PyQt5.QtGui import QColor, QPalette, QIntValidator, QDoubleValidator
from PyQt5.QtCore import QTimer


# ========== HELPER FUNCTIONS ==========

def build_payload(type_index, task_name, input_widgets):
    if (type_index, task_name) == (0, "OBC reboot"):
        return b''

    elif (type_index, task_name) == (0, "Exit state"):
        from_state = input_widgets["From state:"].value() & 0x0F
        to_state = input_widgets["To state:"].value() & 0x0F
        byte_val = (from_state << 4) | to_state
        return bytes([byte_val])

    elif (type_index, task_name) == (0, "Variable change"):
        address = input_widgets["Address:"].value() & 0xFF
        value = input_widgets["Value:"].value() & 0xFF
        return bytes([address, value])

    elif (type_index, task_name) == (0, "EPS reboot"):
        return b''

    elif (type_index, task_name) == (0, "ADCS reboot"):
        return b''

    elif (type_index, task_name) == (0, "TLE update"):
        tle_line1 = input_widgets["TLE Line 1:"].text().encode('ascii')
        tle_line2 = input_widgets["TLE Line 2:"].text().encode('ascii')
        return tle_line1 + b'\n' + tle_line2
    # Add additional tasks here for DAQ, PE, DT as needed

    else:
        return b''

def build_packet(gs_text, type_index, task_name, packet_id, total_packets, payload_bytes):
    # First byte: Ground Station
    if gs_text == "UniPD":
        gs_byte = b'\x01'
    elif gs_text == "Mobile":
        gs_byte = b'\x02'
    else:
        raise ValueError(f"Invalid Ground Station: {gs_text}")

    # Second byte: Type (2 bit) + Task (6 bit)
    task_code = MainWindow.TASKS.get(type_index, {}).get(task_name, 0)
    second_byte = (type_index << 6) | (task_code & 0x3F)

    # Third byte: Total Packets (4 bit) + Packet ID (4 bit)
    third_byte_val = ((total_packets & 0x0F) << 4) | (packet_id & 0x0F)

    header = bytes([gs_byte[0], second_byte, third_byte_val])

    # Time bytes: 4 bytes of Unix timestamp
    unix_time = int(datetime.now().timestamp())
    time_bytes = unix_time.to_bytes(4, byteorder='big')

    # End byte: 0xFF
    end_byte = b'\xFF'

    # Assemble the full packet
    return header + time_bytes + payload_bytes + end_byte

def split_payload_if_needed(type_index, task_label, payload):
    # Define commands that require payload splitting and their max chunk size
    MULTI_PACKET_TASKS = {
        (0, "TLE update"): 10
    }

    max_len = MULTI_PACKET_TASKS.get((type_index, task_label))
    if max_len:
        return [payload[i:i + max_len] for i in range(0, len(payload), max_len)]
    else:
        return [payload]

class MainWindow(QWidget):
    # Centralized task definitions + codes
    TASKS = {
        0: {  # HK
            "OBC reboot": 0,
            "Exit state": 1,
            "Variable change": 2,
            "EPS reboot": 8,
            "ADCS reboot": 16,
            "TLE update": 17
        },
        1: {  # DAQ
            "Start DAQ": 5,
            "Stop DAQ": 6
        },
        2: {  # PE
            "Execute A": 22,
            "Execute B": 23
        },
        3: {  # DT
            "Upload": 30,
            "Download": 31
        }
    }

    # Define your form input configs here:
    # Each key is a (type_index, task_name) tuple
    # Each value is a list of input field definitions:
    # (label, widget_type, optional widget args)
    INPUT_FIELDS_CONFIG = {
        (0, "OBC reboot"): [
            ("No configuration available, execution is immediate", QLabel, None, None, None, None)
        ],
        (0, "Exit state"): [
            # ("Delay:", QSpinBox, 0, 0, 100, "[s]"),
            ("From state:", QSpinBox, 0, 0, 14, "0-14"),
            ("To state:", QSpinBox, 1, 0, 14, "0-14"),
        ],
        (0, "Variable change"): [
            ("Address:", QSpinBox, 0, 0, 254, "0-254"),
            ("Value:", QSpinBox, 0, 0, 254, "0-254")
        ],
        (0, "EPS reboot"): [
            ("No configuration available, execution is immediate", QLabel, None, None, None, None)
        ],
        (0, "ADCS reboot"): [
            ("No configuration available, execution is immediate", QLabel, None, None, None, None)
        ],
        (0, "TLE update"): [
            ("to be done...", QLabel, None, None, None, None)
        ]
    }

    def __init__(self):
        super().__init__()
        self.setWindowTitle("ESP32 Serial GUI")
        self.serial_conn = None
        self.command_queue = []
        self.created_widgets = {}

        # === Left Panel Components ===
        self.gs_selector = QComboBox()
        self.gs_selector.addItems(["UniPD", "Mobile"])

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
        self.queue_table.setColumnCount(4)
        self.queue_table.setHorizontalHeaderLabels(["CMD", "Command", "Packet", "HEX"])
        self.queue_table.horizontalHeader().setStretchLastSection(True)
        self.queue_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.queue_table.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        self.queue_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)  # CMD
        self.queue_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeToContents)  # Packet
        # self.queue_table.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeToContents)  # Delay

        self.execute_next_button = QPushButton("Execute Next")
        self.execute_next_button.clicked.connect(self.execute_next_command)
        self.execute_next_button.setEnabled(False)

        self.abort_next_button = QPushButton("Abort Next")
        self.abort_next_button.clicked.connect(self.abort_next_command)
        self.abort_next_button.setStyleSheet("background-color: #ffcccc;")

        # === Right Panel Components ===
        self.port_selector = QComboBox()
        self.refresh_ports()

        self.refresh_button = QPushButton("Refresh Ports")
        self.refresh_button.clicked.connect(self.refresh_ports)

        self.connect_button = QPushButton("Connect")
        self.connect_button.clicked.connect(self.toggle_connection)

        self.status_label = QLabel("Disconnected")
        self.status_label.setAutoFillBackground(True)
        self.set_status_light(False)

        self.status_console = QTextEdit()
        self.status_console.setReadOnly(True)

        self.serial_console = QTextEdit()
        self.serial_console.setReadOnly(True)

        # === Layouts ===
        main_layout = QHBoxLayout()

        # === LEFT PANEL ===
        left_col = QVBoxLayout()

        # TEC Setup group (form layout)
        packet_group = QGroupBox("TEC Setup")
        packet_layout = QFormLayout()
        packet_layout.addRow("Ground Station:", self.gs_selector)
        packet_layout.addRow("Type:", self.type_selector)
        packet_layout.addRow("Task:", self.task_selector)
        packet_group.setLayout(packet_layout)
        packet_group.adjustSize()
        packet_group.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)

        # TEC Content group (single form layout for all inputs)
        packet_setup_group = QGroupBox("TEC Content")
        self.packet_setup_layout = QFormLayout()
        packet_setup_group.setLayout(self.packet_setup_layout)
        self.update_task_selector(0)  # Initialize with options for the first typeù
        self.update_packet_content_form()

        # Add groups and buttons
        left_col.addWidget(packet_group)
        left_col.addWidget(packet_setup_group)
        left_col.addWidget(self.add_to_queue_button)

        # Queued commands table inside group box
        queued_commands_group = QGroupBox("Queued Commands")
        queued_commands_layout = QVBoxLayout()
        queued_commands_layout.addWidget(self.queue_table)

        # Add buttons inside this group box
        button_row = QHBoxLayout()
        button_row.addWidget(self.abort_next_button)
        button_row.addWidget(self.execute_next_button)
        queued_commands_layout.addLayout(button_row)

        queued_commands_group.setLayout(queued_commands_layout)
        # queued_commands_group.setMaximumHeight(500)

        self.queue_table.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        left_col.addWidget(queued_commands_group)

        # === RIGHT PANEL ===
        serial_group = QGroupBox("Serial Communication")
        serial_layout = QVBoxLayout()

        form_layout = QFormLayout()
        form_layout.addRow("Select COM Port:", self.port_selector)
        serial_layout.addLayout(form_layout)

        button_row = QHBoxLayout()
        button_row.addWidget(self.refresh_button)
        button_row.addWidget(self.connect_button)
        serial_layout.addLayout(button_row)

        serial_layout.addWidget(self.status_label)

        serial_group.setLayout(serial_layout)

        status_group = QGroupBox("Status Messages")
        status_layout = QVBoxLayout()
        status_layout.addWidget(self.status_console)
        status_group.setLayout(status_layout)

        traffic_group = QGroupBox("Serial Port Traffic")
        traffic_layout = QVBoxLayout()
        traffic_layout.addWidget(self.serial_console)
        traffic_group.setLayout(traffic_layout)

        right_col = QVBoxLayout()
        right_col.addWidget(serial_group)
        right_col.addWidget(status_group)
        right_col.addWidget(traffic_group)

        main_layout.addLayout(left_col, 1)
        main_layout.addLayout(right_col, 1)

        self.setLayout(main_layout)

        # Timer for serial read
        self.timer = QTimer()
        self.timer.timeout.connect(self.read_serial)
    

    # ========== SERIAL COMMUNICATION ==========

    def connect_serial(self):
        port = self.port_selector.currentText()
        try:
            self.serial_conn = serial.Serial(port, 9600, timeout=0.1)
            self.set_status_light(True)
            self.connect_button.setText("Disconnect")
            self.execute_next_button.setEnabled(True)
            self.execute_next_button.setStyleSheet("background-color: #ccffcc;")  # green
            self.log_status(f"[INFO] Connected to {port}")
            self.timer.start(100)
        except Exception as e:
            self.log_status(f"[ERROR] Could not connect: {e}")
            self.set_status_light(False)

    def disconnect_serial(self):
        if self.serial_conn:
            self.timer.stop()
            self.serial_conn.close()
            self.set_status_light(False)
            self.connect_button.setText("Connect")
            self.execute_next_button.setEnabled(False)
            self.execute_next_button.setStyleSheet("")
            self.log_status("[INFO] Disconnected")

    def set_status_light(self, connected):
        palette = self.status_label.palette()
        color = QColor('green') if connected else QColor('red')
        palette.setColor(QPalette.Window, color)
        palette.setColor(QPalette.WindowText, QColor('white'))
        self.status_label.setPalette(palette)
        self.status_label.setText("Connected" if connected else "Disconnected")

    def refresh_ports(self):
        self.port_selector.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_selector.addItem(port.device)

    def toggle_connection(self):
        if self.serial_conn and self.serial_conn.is_open:
            self.disconnect_serial()
        else:
            self.connect_serial()

    def read_serial(self):
        if self.serial_conn and self.serial_conn.in_waiting:
            try:
                data = self.serial_conn.read(self.serial_conn.in_waiting).decode(errors='ignore')
                # print(repr(data))  # See actual control characters like '\r'
                lines = data.replace('\r', '\n').split('\n')
                for line in lines:
                    line = line.strip()
                    if line:
                        timestamp = datetime.now().strftime("[%H:%M:%S]")
                        self.serial_console.append(f"{timestamp} [RX]: {line}")
            except Exception as e:
                self.log_status(f"[ERROR] Reading failed: {e}")

    def log_status(self, message):
        timestamp = datetime.now().strftime("[%H:%M:%S]")
        self.status_console.append(f"{timestamp} {message}")
        
    
    # ========== DYNAMIC PACKET SETUP ==========

    def update_task_selector(self, index):
        self.task_selector.blockSignals(True)
        self.task_selector.clear()

        # Get the task names for this type index or empty dict if none
        tasks_for_type = self.TASKS.get(index, {})
        self.task_selector.addItems(tasks_for_type.keys())

        self.task_selector.blockSignals(False)
        self.update_packet_content_form()

    def update_packet_content_form(self):
        while self.packet_setup_layout.count():
            item = self.packet_setup_layout.takeAt(0)
            widget = item.widget()
            if widget:
                widget.hide()

        type_index = self.type_selector.currentIndex()
        task_name = self.task_selector.currentText()

        key = (type_index, task_name)
        fields = self.INPUT_FIELDS_CONFIG.get(key, [])

        for field in fields:
            label_text = field[0]
            widget_class = field[1]

            default_value = field[2] if len(field) > 2 else None
            min_val = field[3] if len(field) > 3 else None
            max_val = field[4] if len(field) > 4 else None
            right_text = field[5] if len(field) > 5 else None  # Optional label text

            cache_key = (type_index, task_name, label_text)
            if cache_key not in self.created_widgets:

                if widget_class in (QSpinBox, QDoubleSpinBox):
                    spinbox = widget_class()
                    if min_val is not None and max_val is not None:
                        spinbox.setRange(min_val, max_val)
                    if default_value is not None:
                        spinbox.setValue(default_value)

                    if right_text:
                        container = QWidget()
                        h_layout = QHBoxLayout(container)
                        h_layout.setContentsMargins(0, 0, 0, 0)  # No margins
                        h_layout.setSpacing(5)  # Some space between spinbox and label

                        spinbox.setSizePolicy(QSizePolicy.Minimum, QSizePolicy.Fixed)
                        h_layout.addWidget(spinbox)

                        label = QLabel(right_text)
                        label.setSizePolicy(QSizePolicy.Minimum, QSizePolicy.Fixed)
                        h_layout.addWidget(label)

                        container.setSizePolicy(QSizePolicy.Minimum, QSizePolicy.Fixed)
                        self.created_widgets[cache_key] = (container, spinbox)  # store both for access
                    else:
                        self.created_widgets[cache_key] = spinbox

                else:
                    widget = widget_class()
                    if default_value is not None:
                        widget.setText(str(default_value))
                    self.created_widgets[cache_key] = widget

            stored = self.created_widgets[cache_key]
            if isinstance(stored, tuple):  # container + spinbox
                container, spinbox = stored
                self.packet_setup_layout.addRow(label_text, container)
                container.show()
            else:
                self.packet_setup_layout.addRow(label_text, stored)
                stored.show()

    
    # ========== PACKET GENERATION ==========

    
    def generate_hex(self, payload, packet_id=1, total_packets=1):
        # === HEADER BYTE 1: Ground station ===
        gs_text = self.gs_selector.currentText()
        gs_byte = {
            "UniPD": 0x01,
            "Mobile": 0x02
        }.get(gs_text)

        if gs_byte is None:
            self.log_status(f"[ERROR] Invalid GS selected: {gs_text}")
            return None

        # === HEADER BYTE 2: Type + Task ===
        type_code = self.type_selector.currentIndex()
        task_text = self.task_selector.currentText()
        task_code = self.TASKS.get(type_code, {}).get(task_text, 0)
        second_byte = ((type_code & 0x03) << 6) | (task_code & 0x3F)

        # === HEADER BYTE 3: Packet ID Info ===
        third_byte = ((total_packets & 0x0F) << 4) | (packet_id & 0x0F)

        # === TIME BYTES ===
        unix_time = int(datetime.now().timestamp())
        time_bytes = unix_time.to_bytes(4, byteorder='big')

        # === PAYLOAD + END ===
        payload_bytes = payload.encode()
        end_byte = b'\xFF'

        # === Assemble All ===
        packet = bytes([gs_byte, second_byte, third_byte]) + time_bytes + payload_bytes + end_byte
        return ' '.join(f"{b:02X}" for b in packet)

    def add_to_queue(self):
        type_index = self.type_selector.currentIndex()
        type_label = self.type_selector.currentText()
        task_label = self.task_selector.currentText()

        input_widgets = {}
        for i in range(self.packet_setup_layout.rowCount()):
            label_item = self.packet_setup_layout.itemAt(i, QFormLayout.LabelRole)
            field_item = self.packet_setup_layout.itemAt(i, QFormLayout.FieldRole)
            if label_item and field_item:
                label_text = label_item.widget().text()
                widget = field_item.widget()

                if isinstance(widget, QWidget):
                    spinbox = widget.findChild(QSpinBox)
                    if spinbox:
                        input_widgets[label_text] = spinbox
                elif isinstance(widget, QSpinBox):
                    input_widgets[label_text] = widget
                elif isinstance(widget, QLineEdit):
                    input_widgets[label_text] = widget

        try:
            payload = build_payload(type_index, task_label, input_widgets)
        except Exception as e:
            self.log_status(f"[ERROR] Failed to build payload: {e}")
            return

        cmd_id = 1 if not self.command_queue else self.command_queue[-1][0] + 1

        try:
            packets = split_payload_if_needed(type_index, task_label, payload)
        except Exception as e:
            self.log_status(f"[ERROR] Payload splitting failed: {e}")
            return

        for i, packet_payload in enumerate(packets):
            packet_id = i + 1
            command_str = f"{type_label}:{task_label}"

            try:
                packet_bytes = build_packet(
                    self.gs_selector.currentText(),
                    type_index,
                    task_label,
                    packet_id,
                    len(packets),
                    packet_payload
                )
                hex_repr = ' '.join(f"{b:02X}" for b in packet_bytes)
            except Exception as e:
                self.log_status(f"[ERROR] Packet build failed: {e}")
                return

            self.command_queue.append((cmd_id, packet_id, command_str, hex_repr))

        self.update_queue_display()
        self.log_status(f"[INFO] Added CMD {cmd_id} with {len(packets)} packet(s) to queue.")


    # ========== COMMAND QUEUE MANAGEMENT ==========

    def update_queue_display(self):
        self.queue_table.setRowCount(len(self.command_queue))
        for row, (cmd_id, packet_id, command_str, hex_str) in enumerate(self.command_queue):
            self.queue_table.setItem(row, 0, QTableWidgetItem(str(cmd_id)))
            self.queue_table.setItem(row, 1, QTableWidgetItem(command_str))
            self.queue_table.setItem(row, 2, QTableWidgetItem(str(packet_id)))
            self.queue_table.setItem(row, 3, QTableWidgetItem(hex_str))

    def abort_next_command(self):
        if not self.command_queue:
            self.log_status("[INFO] No commands to abort.")
            return

        next_cmd_id = self.command_queue[0][0]
        num_removed = sum(1 for entry in self.command_queue if entry[0] == next_cmd_id)
        self.command_queue = [entry for entry in self.command_queue if entry[0] != next_cmd_id]
        self.update_queue_display()
        self.log_status(f"[INFO] Aborted CMD {next_cmd_id} with {num_removed} packet.")

    def execute_next_command(self):
        if not self.command_queue:
            self.log_status("[INFO] No commands in queue.")
            return

        if not self.serial_conn or not self.serial_conn.is_open:
            self.log_status("[ERROR] Not connected.")
            return

        # Get the CMD ID of the next group
        next_cmd_id = self.command_queue[0][0]

        # Extract all packets belonging to this CMD
        packets_to_send = [entry for entry in self.command_queue if entry[0] == next_cmd_id]

        for entry in packets_to_send:
            _, packet_id, command_str, _, hex_str = entry
            try:
                # Convert HEX back to bytes
                hex_bytes = bytes.fromhex(hex_str)
                self.serial_conn.write(hex_bytes + b'\n')

                timestamp = datetime.now().strftime("[%H:%M:%S]")
                self.serial_console.append(f"{timestamp} [TX]: CMD {next_cmd_id} P{packet_id} -> {command_str}")

                self.log_status(f"[INFO] Executed CMD {next_cmd_id} with {len(packets_to_send)} packet.")


            except Exception as e:
                self.log_status(f"[ERROR] Failed to send CMD {next_cmd_id} P{packet_id}: {e}")

        # Remove sent packets from queue
        self.command_queue = [entry for entry in self.command_queue if entry[0] != next_cmd_id]
        self.update_queue_display()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.resize(1000, 800)
    window.show()
    sys.exit(app.exec_())
