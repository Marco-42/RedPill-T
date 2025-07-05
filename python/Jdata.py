import sqlite3
from datetime import datetime
import sys
import sqlite3
import pandas as pd
from PyQt5.QtWidgets import *

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
            status TEXT,
            length INTEGER,
            mac TEXT,
            payload BLOB,
            metadata TEXT
        )
    ''')
    conn.commit()
    return conn

# Saving function(save packet in database)
def save_packet(conn, direction, ground_station_id, tec, status, mac, payload, metadata=""):
    """Save on database: conn, direction, ground_station_id, tec, status, mac, payload, metadata"""
    cursor = conn.cursor()
    length = len(payload)

    # Get current timestamp in ISO format
    timestamp = datetime.utcnow().isoformat()

    # Save the packet data
    cursor.execute('''
        INSERT INTO packets (
            timestamp, direction, ground_station_id, tec, status,
            length, mac, payload, metadata
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    ''', (timestamp, direction, ground_station_id, tec, status, length, mac, payload, metadata))
    
    conn.commit()

# Show all packet inside database
def show_all_packets(conn):
    cursor = conn.cursor()
    cursor.execute('''
        SELECT id, timestamp, direction, ground_station_id, tec,
               status, length, mac, metadata
        FROM packets
        ORDER BY timestamp
    ''')
    rows = cursor.fetchall()

    print("\n📡 Saved packets:")
    for row in rows:
        print(f"ID:{row[0]} | TIME: [{row[1]}] | {row[2].upper()} | GS:{row[3]} | TEC:{row[4]} | "
              f"Status:{row[5]} | Length:{row[6]} | MAC:{row[7]} | Notes: {row[8]}")
        
def open_database():
    class PacketViewer(QWidget):
        def __init__(self):
            super().__init__()
            self.setWindowTitle("Jdata database")
            self.resize(1000, 600)

            # Layout
            layout = QVBoxLayout()

            # Filter controls
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

            layout.addLayout(filter_layout)

            # Table
            self.table = QTableWidget()
            layout.addWidget(self.table)

            self.setLayout(layout)

            # Initial load
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
            rows = cursor.fetchall()


            
            # Extract column names
            headers = [desc[0] for desc in cursor.description]

            # Setting Tabel 
            self.table.setRowCount(len(rows))
            self.table.setColumnCount(len(headers))
            self.table.setHorizontalHeaderLabels(headers)

            # Insert data into tabel
            for i, row in enumerate(rows):
                for j, value in enumerate(row):
                    self.table.setItem(i, j, QTableWidgetItem(str(value)))
        
            conn.close()
    app = QApplication(sys.argv)
    viewer = PacketViewer()
    viewer.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    open_database()


# if __name__ == "__main__":
#     conn = init_db()

#     for i in range(10000): 
#         # Simulated packet: uplink
#         save_packet(
#             conn,
#             direction="uplink",
#             ground_station_id="GS001",
#             tec=12.5,
#             status="ok",
#             mac="00:1A:C2:7B:00:47",
#             payload=b"Command: RESET",
#             metadata="initial command"
#         )

#         # Simulated packet: downlink
#         save_packet(
#             conn,
#             direction="downlink",
#             ground_station_id="GS002",
#             tec=13.1,
#             status="ack",
#             mac="00:1A:C2:7B:00:47",
#             payload=b"ACK:RESET",
#             metadata="reply to reset command"
#         )
#         print("ciao")
# #     show_all_packets(conn)