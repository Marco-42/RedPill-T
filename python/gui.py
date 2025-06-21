import sys
import serial
import serial.tools.list_ports
from datetime import datetime
from PyQt5.QtWidgets import *
from PyQt5.QtGui import QColor, QPalette
from PyQt5.QtCore import QTimer


class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ESP32 Serial GUI")
        self.serial_conn = None
        self.command_queue = []

        # === Left Panel Components ===
        self.gs_selector = QComboBox()
        self.gs_selector.addItems(["UniPD", "Mobile"])

        self.packet_type = QComboBox()
        self.packet_type.addItems(["Custom", "A", "B"])
        self.packet_type.currentIndexChanged.connect(self.switch_input_form)

        # Input widgets for different packet types
        # Custom input
        self.custom_input = QLineEdit()

        # A input
        self.a_opt1 = QComboBox()
        self.a_opt1.addItems(["1", "2", "3"])
        self.a_opt2 = QComboBox()
        self.a_opt2.addItems(["1", "2", "3"])

        # B input
        self.b_set1 = QComboBox()
        self.b_set1.addItems(["1", "2", "3"])
        self.b_set2 = QComboBox()
        self.b_set2.addItems(["1", "2", "3"])

        self.add_to_queue_button = QPushButton("Add to Queue")
        self.add_to_queue_button.clicked.connect(self.add_to_queue)

        self.queue_table = QTableWidget()
        self.queue_table.setColumnCount(5)
        self.queue_table.setHorizontalHeaderLabels(["CMD", "Command", "Packet", "Delay", "HEX"])
        self.queue_table.horizontalHeader().setStretchLastSection(True)
        self.queue_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.queue_table.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        self.queue_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)  # CMD
        self.queue_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeToContents)  # Packet
        self.queue_table.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeToContents)  # Delay

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
        packet_layout.addRow("Command:", self.packet_type)
        packet_group.setLayout(packet_layout)
        packet_group.adjustSize()
        packet_group.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)

        # TEC Content group (single form layout for all inputs)
        packet_setup_group = QGroupBox("TEC Content")
        self.packet_setup_layout = QFormLayout()
        packet_setup_group.setLayout(self.packet_setup_layout)

        # Initially populate input fields for the default packet type
        self.switch_input_form(self.packet_type.currentIndex())

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

    def switch_input_form(self, index):
        # Remove all widgets from the layout but do NOT delete them, so they can be reused
        while self.packet_setup_layout.count():
            item = self.packet_setup_layout.takeAt(0)
            widget = item.widget()
            if widget:
                # Just remove from layout, don't delete!
                widget.setParent(None)

        # Add widgets according to packet type
        if index == 0:  # Custom
            self.packet_setup_layout.addRow("Payload:", self.custom_input)
        elif index == 1:  # A
            self.packet_setup_layout.addRow("Opt 1:", self.a_opt1)
            self.packet_setup_layout.addRow("Opt 2:", self.a_opt2)
        elif index == 2:  # B
            self.packet_setup_layout.addRow("Set 1:", self.b_set1)
            self.packet_setup_layout.addRow("Set 2:", self.b_set2)


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

    def log_status(self, message):
        timestamp = datetime.now().strftime("[%H:%M:%S]")
        self.status_console.append(f"{timestamp} {message}")

    def add_to_queue(self):
        packet_type = self.packet_type.currentText()

        if packet_type == "Custom":
            payload = self.custom_input.text().strip()
            if not payload:
                self.log_status("[WARNING] Payload is empty.")
                return
            packets = [payload[i:i + 10] for i in range(0, len(payload), 10)]

        elif packet_type == "A":
            val1 = self.a_opt1.currentText()
            val2 = self.a_opt2.currentText()
            payload = f"A_{val1}_{val2}"
            packets = [payload]

        elif packet_type == "B":
            val1 = self.b_set1.currentText()
            val2 = self.b_set2.currentText()
            payload = f"B_{val1}_{val2}"
            packets = [payload]

        max_packet_len = 10
        packets = [payload[i:i + max_packet_len] for i in range(0, len(payload), max_packet_len)]

        cmd_id = 1 if not self.command_queue else self.command_queue[-1][0] + 1

        for i, packet_payload in enumerate(packets):
            packet_id = i + 1
            command_str = f"{packet_type}:{packet_payload}"

            delay = "0"
            hex_repr = self.generate_hex(packet_type, packet_payload)

            self.command_queue.append((cmd_id, packet_id, command_str, delay, hex_repr))

        self.update_queue_display()
        self.log_status(f"[INFO] Added CMD {cmd_id} with {len(packets)} packet(s) to queue.")

    def generate_hex(self, packet_type, payload):
        if self.gs_selector.currentText() == "UniPD":
            gs_byte = b'\x01'
        elif self.gs_selector.currentText() == "Mobile":
            gs_byte = b'\x02'
        else:
            gs_byte = b'\x00'

        if packet_type == "Custom":
            type_bytes = b'\x02\x03'
        elif packet_type == "A":
            type_bytes = b'\xA2\xA3'
        elif packet_type == "B":
            type_bytes = b'\xB2\xB3'
        else:
            type_bytes = b'\x00\x00'

        header = gs_byte + type_bytes
        unix_time = int(datetime.now().timestamp())
        time_bytes = unix_time.to_bytes(4, byteorder='big')
        payload_bytes = payload.encode()
        end_byte = b'\xFF'

        packet_bytes = header + time_bytes + payload_bytes + end_byte
        return ' '.join(f"{byte:02X}" for byte in packet_bytes)


    def update_queue_display(self):
        self.queue_table.setRowCount(len(self.command_queue))
        for row, (cmd_id, packet_id, command_str, delay, hex_str) in enumerate(self.command_queue):
            self.queue_table.setItem(row, 0, QTableWidgetItem(str(cmd_id)))
            self.queue_table.setItem(row, 1, QTableWidgetItem(command_str))
            self.queue_table.setItem(row, 2, QTableWidgetItem(str(packet_id)))
            self.queue_table.setItem(row, 3, QTableWidgetItem(str(delay)))
            self.queue_table.setItem(row, 4, QTableWidgetItem(hex_str))


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



if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.resize(1000, 800)
    window.show()
    sys.exit(app.exec_())
