import sys
import serial
import serial.tools.list_ports
from datetime import datetime
from PyQt5.QtWidgets import *
# from PyQt5.QtWidgets import QTableWidget, QTableWidgetItem, QSpacerItem, QSizePolicy
from PyQt5.QtGui import QColor, QPalette
from PyQt5.QtCore import QTimer


class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ESP32 Serial GUI")
        self.serial_conn = None
        self.command_queue = []

        # === Left Panel Components ===
        self.packet_type = QComboBox()
        self.packet_type.addItems(["Custom", "A", "B"])
        self.payload_input = QLineEdit()
        self.add_to_queue_button = QPushButton("Add to Queue")
        self.add_to_queue_button.clicked.connect(self.add_to_queue)

        self.queue_table = QTableWidget()
        self.queue_table.setColumnCount(5)
        self.queue_table.setHorizontalHeaderLabels(["CMD", "Command", "Packet", "Delay", "HEX"])
        self.queue_table.horizontalHeader().setStretchLastSection(True)
        self.queue_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.queue_table.setMaximumHeight(400)

        # Resize columns to fit header for narrow columns
        self.queue_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)  # CMD
        self.queue_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeToContents)  # Packet
        self.queue_table.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeToContents)  # Delay

        self.execute_next_button = QPushButton("Execute Next Command")
        self.execute_next_button.clicked.connect(self.execute_next_command)
        self.execute_next_button.setEnabled(False)

        self.abort_next_button = QPushButton("Abort Next Command")
        self.abort_next_button.clicked.connect(self.abort_next_command)

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

        # LEFT COLUMN (Commands)
        left_col = QVBoxLayout()
        left_col.addWidget(QLabel("Packet Type:"))
        left_col.addWidget(self.packet_type)
        left_col.addWidget(QLabel("Payload:"))
        left_col.addWidget(self.payload_input)
        left_col.addWidget(self.add_to_queue_button)

        left_col.addWidget(QLabel("Queued Commands:"))
        left_col.addWidget(self.queue_table)

        # Abort and execute buttons
        button_row = QHBoxLayout()
        button_row.addWidget(self.abort_next_button)
        button_row.addWidget(self.execute_next_button)
        left_col.addLayout(button_row)
        # self.execute_next_button.setStyleSheet("background-color: #ccffcc;") # light green
        # self.abort_next_button.setStyleSheet("background-color: #ffcccc;") # light red

        # RIGHT COLUMN (Connection & Status)
        right_col = QVBoxLayout()
        right_col.addWidget(QLabel("Select COM Port:"))
        right_col.addWidget(self.port_selector)

        button_row = QHBoxLayout()
        button_row.addWidget(self.refresh_button)
        button_row.addWidget(self.connect_button)
        right_col.addLayout(button_row)

        right_col.addWidget(self.status_label)

        right_col.addWidget(QLabel("Status Messages:"))
        right_col.addWidget(self.status_console)

        right_col.addWidget(QLabel("Serial Port Traffic:"))
        right_col.addWidget(self.serial_console)

        # Combine into main layout with 1:1 ratio
        main_layout.addLayout(left_col, 1)
        main_layout.addLayout(right_col, 1)

        self.setLayout(main_layout)

        # Timer for serial read
        self.timer = QTimer()
        self.timer.timeout.connect(self.read_serial)

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
        payload = self.payload_input.text().strip()

        if not payload:
            self.log_status("[WARNING] Payload is empty.")
            return

        # Simulate splitting payload into multiple packets (e.g., max 10 chars each)
        max_packet_len = 10
        packets = [payload[i:i + max_packet_len] for i in range(0, len(payload), max_packet_len)]

        cmd_id = 1 if not self.command_queue else self.command_queue[-1][0] + 1

        for i, packet_payload in enumerate(packets):
            packet_id = i + 1
            command_str = f"{packet_type}:{packet_payload}"

            # Dummy delay and HEX values for now
            delay = "0"
            hex_repr = self.generate_hex(packet_type, packet_payload)

            self.command_queue.append((cmd_id, packet_id, command_str, delay, hex_repr))

        self.update_queue_display()
        self.log_status(f"[INFO] Added CMD {cmd_id} with {len(packets)} packet to queue.")


    def generate_hex(self, packet_type, payload):
        # if packet_type == "Custom":
        #     prefix = b'\x00'
        # elif packet_type == "A":
        #     prefix = b'\xA0'
        # elif packet_type == "B":
        #     prefix = b'\xB0'
        # else:
        #     prefix = b'\x00'

        # packet_bytes = prefix + payload.encode()
        packet_bytes = payload.encode()
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
    window.resize(900, 600)
    window.show()
    sys.exit(app.exec_())
