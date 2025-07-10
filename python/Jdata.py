import sqlite3
from datetime import datetime
import sys
from PyQt5.QtWidgets import *
from PyQt5.QtGui import QPalette, QColor
from PyQt5.QtCore import Qt

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
def save_packet(conn, ground_station_id, status, mac, payload, tec, direction = "Transmitter", rssi = "None", snr = "None", metadata=""):
    """conn, ground_station_id, status, mac, payload, tec, direction(F), rssi(F), snr(F), metadata="""""
    
    cursor = conn.cursor()
    length = len(payload)
    timestamp = datetime.utcnow().isoformat()

    # Setting known parameters
    if ground_station_id == "RedPill":
        direction = "Source"
        rssi, snr = "None"

    cursor.execute('''
        INSERT INTO packets (
            timestamp, direction, ground_station_id, tec, rssi, snr,
            status, length, mac, payload, metadata
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    ''', (timestamp, direction, ground_station_id, tec, rssi, snr, status, length, mac, payload, metadata))
    
    conn.commit()

# Show all packets in terminal (optional)
def show_all_packets(conn):
    cursor = conn.cursor()
    cursor.execute('''
        SELECT id, timestamp, direction, ground_station_id, tec, rssi, snr,
               status, length, mac, metadata
        FROM packets
        ORDER BY timestamp
    ''')
    rows = cursor.fetchall()

    print("\n📡 Saved packets:")
    for row in rows:
        print(f"ID:{row[0]} | TIME:[{row[1]}] | {row[2].upper()} | GS:{row[3]} | "
              f"TEC:{row[4]} | RSSI:{row[5]} | SNR:{row[6]} | Status:{row[7]} | "
              f"Length:{row[8]} | MAC:{row[9]} | Notes:{row[10]}")

# GUI using PyQt5
def open_database():
    class PacketViewer(QWidget):
        def __init__(self):
            super().__init__()

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
            QLineEdit, QComboBox {
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
            self.direction_combo = QComboBox()
            self.direction_combo.addItems(["All", "uplink", "downlink"])
            filter_layout.addWidget(QLabel("Direction:"))
            filter_layout.addWidget(self.direction_combo)

            self.gs_input = QLineEdit()
            filter_layout.addWidget(QLabel("Ground Station ID:"))
            filter_layout.addWidget(self.gs_input)

            self.status_input = QLineEdit()
            filter_layout.addWidget(QLabel("Status:"))
            filter_layout.addWidget(self.status_input)

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
            right_panel.addWidget(QLabel("Packet Details:"))
            self.details_box = QTextEdit()
            self.details_box.setReadOnly(True)
            self.details_box.setStyleSheet("font-family: monospace;")
            right_panel.addWidget(self.details_box)

            # Add both panels to main layout
            main_layout.addLayout(left_panel, stretch=3)
            main_layout.addLayout(right_panel, stretch=4)

            self.setLayout(main_layout)
            self.packets = []  # buffer for fetched packets
            self.load_data()

        def load_data(self):
            conn = sqlite3.connect(DB_PATH)
            cursor = conn.cursor()

            query = "SELECT * FROM packets WHERE 1=1"
            params = []

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

            cursor.execute(query, params)
            self.packets = cursor.fetchall()

            self.table.setRowCount(len(self.packets))

            for i, pkt in enumerate(self.packets):
                self.table.setItem(i, 0, QTableWidgetItem(str(pkt[0])))  # ID
                self.table.setItem(i, 1, QTableWidgetItem(pkt[1]))       # Timestamp
                self.table.setItem(i, 2, QTableWidgetItem(pkt[2]))       # Direction

            conn.close()
            self.details_box.setStyleSheet("font-family: monospace; font-size: 12pt;")
            self.details_box.clear()

        def show_packet_details(self, row, col):
            pkt = self.packets[row]
            details = (
                f"ID: {pkt[0]}\n"
                f"Timestamp: {pkt[1]}\n"
                f"Direction: {pkt[2]}\n"
                f"Ground Station ID: {pkt[3]}\n"
                f"TEC: {pkt[4]}\n"
                f"RSSI: {pkt[5]}\n"
                f"SNR: {pkt[6]}\n"
                f"Status: {pkt[7]}\n"
                f"Length: {pkt[8]}\n"
                f"MAC: {pkt[9]}\n"
                f"Payload: {pkt[10]}\n"
                f"Metadata: {pkt[11]}"
            )
            self.details_box.setText(details)

    dark_palette = QPalette()

    # Colori di base
    dark_palette.setColor(QPalette.Window, QColor(30, 30, 30))              # sfondo finestra
    dark_palette.setColor(QPalette.WindowText, Qt.white)                   # testo
    dark_palette.setColor(QPalette.Base, QColor(45, 45, 45))               # sfondo input
    dark_palette.setColor(QPalette.AlternateBase, QColor(60, 60, 60))      # sfondo alternativo
    dark_palette.setColor(QPalette.ToolTipBase, Qt.white)
    dark_palette.setColor(QPalette.ToolTipText, Qt.white)
    dark_palette.setColor(QPalette.Text, Qt.white)
    dark_palette.setColor(QPalette.Button, QColor(60, 60, 60))             # sfondo pulsanti
    dark_palette.setColor(QPalette.ButtonText, QColor(255, 100, 0))        # testo pulsanti (arancione)
    dark_palette.setColor(QPalette.BrightText, Qt.red)                     # testo evidenziato
    dark_palette.setColor(QPalette.Highlight, QColor(255, 69, 0))          # evidenziazione (rosso/arancione)
    dark_palette.setColor(QPalette.HighlightedText, Qt.black)

    # Applica il tema
    QApplication.setPalette(dark_palette)

    app = QApplication(sys.argv)
    viewer = PacketViewer()
    viewer.show()
    sys.exit(app.exec_())

# Debug/test/demo
if __name__ == "__main__":
    conn = init_db()

    # Inserisci pacchetti di esempio
    save_packet(
        conn,
        direction="uplink",
        ground_station_id="GS001",
        tec=12.5,
        rssi=-75.2,
        snr=10.1,
        status="ok",
        mac="00:1A:C2:7B:00:47",
        payload=b"Command: RESET",
        metadata="initial command"
    )

    save_packet(
        conn,
        direction="downlink",
        ground_station_id="GS002",
        tec=13.1,
        rssi=-70.8,
        snr=9.3,
        status="ack",
        mac="00:1A:C2:7B:00:47",
        payload=b"ACK:RESET",
        metadata="reply to reset command"
    )

    show_all_packets(conn)
    open_database()