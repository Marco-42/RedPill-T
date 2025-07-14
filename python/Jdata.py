import sqlite3
from datetime import datetime
import sys
from PyQt5.QtWidgets import *
from PyQt5.QtGui import QPalette, QColor
from PyQt5.QtCore import Qt, QDate

DB_PATH = "./python/SatelliteDB.db"

# Database initialization and packet definition
def init_db(path=DB_PATH):
    conn = sqlite3.connect(path)
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS packets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            direction TEXT,
            ground_station_id TEXT,
            tec REAL,
            rssi REAL,
            snr REAL, 
            freq_offset REAL,
            status TEXT,
            length INTEGER,
            mac TEXT,
            payload BLOB,
            metadata TEXT
        )
    ''')
    conn.commit()
    return conn

# Saving function (save packet in database)
def save_packet(conn, ground_station_id, status, mac, payload, tec, direction = "Transmitter", rssi = "0", snr = "0", freq_offset = "0", metadata=""):
    """conn, ground_station_id, status, mac, payload, tec, direction(F), rssi(F), snr(F), freq_offset(F), metadata [conn is the database definition --> use init_db()]"""
    
    cursor = conn.cursor()
    length = len(payload)

    # Get data of packet saving(UTC)
    timestamp = datetime.utcnow().isoformat()

    # Creation of the packet from raw data
    cursor.execute('''
        INSERT INTO packets (
            timestamp, direction, ground_station_id, tec, rssi, snr, freq_offset,
            status, length, mac, payload, metadata
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    ''', (timestamp, direction, ground_station_id, tec, rssi, snr, freq_offset, status, length, mac, payload, metadata))
    
    # Commit packet to database
    conn.commit()

# Show all packets in terminal(usefull for debug operations)
def show_all_packets(conn):
    cursor = conn.cursor()
    cursor.execute('''
        SELECT id, timestamp, direction, ground_station_id, tec, rssi, snr, freq_offset,
               status, length, mac, metadata
        FROM packets
        ORDER BY timestamp
    ''')
    rows = cursor.fetchall()

# Create the GUI for database visualization
def open_database():
    class PacketViewer(QWidget):
        def __init__(self):
            super().__init__()

            # Set the dark theme for GUI
            self.setStyleSheet("""
            QPushButton {
                background-color: #ff4500;
                color: white;
                border: none;
                padding: 5px 10px;
            }
            QPushButton:hover {
                background-color: #e03d00;
            }
            QLineEdit, QComboBox, QDateEdit {
                background-color: #2e2e2e;
                color: white;
                border: 1px solid #555;
            }
            QLabel {
                color: white;
            }
        """)
            
            self.setWindowTitle("Jdata Database")
            self.resize(1200, 600)

            # Main horizontal layout (left: table, right: details)
            main_layout = QHBoxLayout()

            # Left panel (filters + table)
            left_panel = QVBoxLayout()

            # Filters
            filter_layout = QHBoxLayout()

            # Different filter that the user can apply to find packets inside the database
            # Search for data
            self.date_from = QDateEdit(calendarPopup=True)
            self.date_from.setDisplayFormat("yyyy-MM-dd")
            self.date_from.setDate(QDate.currentDate().addDays(-7))  # default: 7 days ago
            filter_layout.addWidget(QLabel("From Date:"))
            filter_layout.addWidget(self.date_from)

            self.date_to = QDateEdit(calendarPopup=True)
            self.date_to.setDisplayFormat("yyyy-MM-dd")
            self.date_to.setDate(QDate.currentDate())  # default: today
            filter_layout.addWidget(QLabel("To Date:"))
            filter_layout.addWidget(self.date_to)

            # Search for direction(source or trasmitter)
            self.direction_combo = QComboBox()
            self.direction_combo.addItems(["All", "Source", "Transmitter"])
            filter_layout.addWidget(QLabel("Direction:"))
            filter_layout.addWidget(self.direction_combo)

            # Search for ground station ID
            self.gs_input = QLineEdit()
            filter_layout.addWidget(QLabel("Ground Station ID:"))
            filter_layout.addWidget(self.gs_input)

            # Search for the status of transmission/reception
            self.status_input = QLineEdit()
            filter_layout.addWidget(QLabel("Status:"))
            filter_layout.addWidget(self.status_input)

            # Refresh button
            self.refresh_btn = QPushButton("Refresh")
            self.refresh_btn.clicked.connect(self.load_data)
            filter_layout.addWidget(self.refresh_btn)

            left_panel.addLayout(filter_layout)

            # Table with compact info
            self.table = QTableWidget()
            self.table.setColumnCount(3)
            self.table.setHorizontalHeaderLabels(["ID", "Timestamp", "Direction"])
            self.table.setStyleSheet("""
                QTableWidget {
                    background-color: #1e1e1e;
                    color: white;
                    gridline-color: #555;
                }
                QHeaderView::section {
                    background-color: #2e2e2e;
                    color: white;
                    font-weight: bold;
                    border: 1px solid #555;
                }
            """)
            self.table.horizontalHeader().setStretchLastSection(True)
            self.table.verticalHeader().setVisible(False)
            self.table.cellClicked.connect(self.show_packet_details)
            left_panel.addWidget(self.table)

            # Right panel (packet detail)
            right_panel = QVBoxLayout()

            # First line in the right panel "PACKET DETAILS" + "DELETE PACKET" button
            header_layout = QHBoxLayout()

            # Setting the header for the right panel
            details_label = QLabel("PACKET DETAILS:")
            details_label.setStyleSheet("font-size: 12pt; font-weight: bold; color: white;")

            # Setting the delete button layout
            self.delete_btn = QPushButton("DELETE PACKET")
            self.delete_btn.setStyleSheet("""
                QPushButton {
                    background-color: #8b0000;
                    color: white;
                    border: none;
                    padding: 4px 10px;
                    font-weight: bold;
                    font-size: 11pt;
                }
                QPushButton:hover {
                    background-color: #a30000;
                }
            """)

            # Connect the delete button to the function
            self.delete_btn.clicked.connect(self.delete_selected_packet)

            header_layout.addWidget(details_label)
            header_layout.addStretch()
            header_layout.addWidget(self.delete_btn)

            right_panel.addLayout(header_layout)

            self.detail_frames = []

            # Modify the layout for the right panel
            for _ in range(7):
                frame = QFrame()
                frame.setStyleSheet("""
                    QFrame {
                        background-color: #2b2b2b;
                        border: 1px solid #444;
                        border-radius: 6px;
                        padding: 8px;
                        margin-bottom: 6px;
                    }
                    QLabel {
                        color: white;
                        font-family: Consolas, monospace;
                        font-size: 12pt;
                    }
                """)
                layout = QHBoxLayout()
                layout.setContentsMargins(10, 4, 10, 4)
                frame.setLayout(layout)
                right_panel.addWidget(frame)
                self.detail_frames.append((frame, layout))

            # Add both panels to main layout
            main_layout.addLayout(left_panel, stretch=1)
            main_layout.addLayout(right_panel, stretch=5)

            self.setLayout(main_layout)
            self.packets = []  # buffer for fetched packets
            self.load_data()

        # Function to display data applying filters
        def load_data(self):
            conn = sqlite3.connect(DB_PATH)
            cursor = conn.cursor()

            query = "SELECT * FROM packets WHERE 1=1"
            params = []

            # Date range
            from_date = self.date_from.date().toString("yyyy-MM-dd")
            to_date = self.date_to.date().toString("yyyy-MM-dd")
            query += " AND date(timestamp) >= ? AND date(timestamp) <= ?"
            params.extend([from_date, to_date])

            # Other filters
            direction = self.direction_combo.currentText()
            gs_id = self.gs_input.text().strip()
            status = self.status_input.text().strip()

            if direction != "All":
                query += " AND direction = ?"
                params.append(direction)
            if gs_id:
                query += " AND ground_station_id = ?"
                params.append(gs_id)
            if status:
                query += " AND status = ?"
                params.append(status)

            # Search data inside database
            cursor.execute(query, params)
            self.packets = cursor.fetchall()

            self.table.setRowCount(len(self.packets))

            # Plotting founded data
            for i, pkt in enumerate(self.packets):
                self.table.setItem(i, 0, QTableWidgetItem(str(pkt[0])))  # ID
                self.table.setItem(i, 1, QTableWidgetItem(pkt[1]))       # Timestamp
                self.table.setItem(i, 2, QTableWidgetItem(pkt[2]))       # Direction

            conn.close()

        # Function to show packet details when selected
        def show_packet_details(self, row, col):
            pkt = self.packets[row]

            rows = [
                f"ID: {pkt[0]}    |   Direction: {pkt[2]}    |   GS id: {pkt[3]}",
                f"Timestamp: {pkt[1].replace('T', ' ')}",
                f"TEC: {pkt[4]}    |   RSSI: {pkt[5]}    |   SNR: {pkt[6]}",
                f"Length: {pkt[9]}    |   Status: {pkt[8]}    |   Freq Offset: {pkt[7]}",
                f"MAC: {pkt[10]}",
                f"Payload: {pkt[11]}",
                f"Metadata: {pkt[12]}"
            ]

            for (frame, layout), text in zip(self.detail_frames, rows):

                # Clear the previous content of the frame
                for i in reversed(range(layout.count())):
                    widget_to_remove = layout.itemAt(i).widget()
                    layout.removeWidget(widget_to_remove)
                    widget_to_remove.deleteLater()

                # Ad the new label with packet details
                label = QLabel(text)
                layout.addWidget(label)

        # Deleting selected packet from database
        def delete_selected_packet(self):
            selected_row = self.table.currentRow()

            # Error message if no packet is selected
            if selected_row < 0:
                msg = QMessageBox(self)
                msg.setIcon(QMessageBox.Warning)
                msg.setWindowTitle("Warning")
                msg.setText("Select a packet to delete.")
                
                # Setting the style of message box
                msg.setStyleSheet("""
                    QMessageBox {
                        background-color: #2e2e2e;
                        color: white;
                        font-size: 11pt;
                    }
                    QPushButton {
                        background-color: #ff4500;
                        color: white;
                        border: none;
                        padding: 5px 10px;
                    }
                    QPushButton:hover {
                        background-color: #555;
                    }
                """)

                msg.exec_()
                return

            # Confirm elimination of the selected packet
            pkt_id = self.packets[selected_row][0]
            msgbox = QMessageBox(self)
            msgbox.setWindowTitle("Confirm")
            msgbox.setText(f"Sure to delete ID {pkt_id} from database?")
            msgbox.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
            
            # Setting the style of message box
            msgbox.setStyleSheet("""
                QMessageBox {
                    background-color: #2e2e2e;
                    color: white;
                    font-size: 11pt;
                }
                QPushButton {
                    background-color: #ff4500;
                    color: white;
                    border: none;
                    padding: 5px 10px;
                }
                QPushButton:hover {
                    background-color: #555;
                }
            """)

            reply = msgbox.exec_()

            # Deleting the packet if confirmed
            if reply == QMessageBox.Yes:
                conn = sqlite3.connect(DB_PATH)
                cursor = conn.cursor()
                cursor.execute("DELETE FROM packets WHERE id = ?", (pkt_id,))
                conn.commit()
                conn.close()

                # Refresh the left table after elimination
                self.load_data()

    dark_palette = QPalette()

    # Color for dark theme of layout
    dark_palette.setColor(QPalette.Window, QColor(30, 30, 30))              
    dark_palette.setColor(QPalette.WindowText, Qt.white)                   
    dark_palette.setColor(QPalette.Base, QColor(45, 45, 45))               
    dark_palette.setColor(QPalette.AlternateBase, QColor(60, 60, 60))   
    dark_palette.setColor(QPalette.ToolTipBase, Qt.white)
    dark_palette.setColor(QPalette.ToolTipText, Qt.white)
    dark_palette.setColor(QPalette.Text, Qt.white)
    dark_palette.setColor(QPalette.Button, QColor(60, 60, 60))            
    dark_palette.setColor(QPalette.ButtonText, QColor(255, 100, 0))       
    dark_palette.setColor(QPalette.BrightText, Qt.red)                    
    dark_palette.setColor(QPalette.Highlight, QColor(255, 69, 0))         
    dark_palette.setColor(QPalette.HighlightedText, Qt.black)

    # Apply dark theme
    QApplication.setPalette(dark_palette)

    app = QApplication(sys.argv)
    viewer = PacketViewer()

    # Show the windows
    viewer.showMaximized()
    sys.exit(app.exec_())

# Debug/test
if __name__ == "__main__":
    conn = init_db()

    # Example of packet saving
    # save_packet(
    #     conn,
    #     ground_station_id="GS-ITA-01",
    #     status="transmit",
    #     mac="00:1A:C2:7B:00:47",
    #     payload=b"CMD:RESET",
    #     tec=12.34,
    #     direction="uplink",
    #     rssi=-85.3,
    #     snr=9.2,
    #     freq_offset=0.5,
    #     metadata="Initial uplink reset command"
    # )

    # save_packet(
    #     conn,
    #     ground_station_id="GS-ITA-01",
    #     status="received",
    #     mac="00:1A:C2:7B:00:47",
    #     payload=b"ACK:RESET",
    #     tec=11.88,
    #     direction="downlink",
    #     rssi=-72.6,
    #     snr=12.4,
    #     freq_offset=0.2,
    #     metadata="Response from satellite"
    # )

    # save_packet(
    #     conn,
    #     ground_station_id="RedPill",
    #     status="telemetry",
    #     mac="RP:00:00:00",
    #     payload=b"Telemetry: Temp=33.4",
    #     tec=13.77,
    #     direction="source",
    #     rssi=-70.0,
    #     snr=10.1,
    #     freq_offset=0.0,
    #     metadata="Sent from onboard source"
    # )
    
    # show_all_packets(conn)
    open_database()