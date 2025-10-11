import sqlite3
from datetime import datetime
import sys
from PyQt5.QtWidgets import *
from PyQt5.QtGui import QPalette, QColor
from PyQt5.QtCore import Qt, QDate
import os
import struct

# Tasks for packet management
# Uncomment this line if the database is run from GUI
from database import GS_task as gt 
# Uncomment this line if the database is run itself
#import GS_task as gt

DB_PATH = "./Ground station/database/SatelliteDB.db"
PACKET_HEADER_LENGTH = 12 # 4 bytes for header + 4 bytes for MAC + 4 bytes for timestamp
BYTE_RS_ON = 0xAA
import pandas as pd
BYTE_RS_OFF = 0x55

# TER VALUES
TER_BEACON = 0x30 # telemetry beacon reply
TER_ACK = 0x31 # ACK reply
TER_NACK = 0x32 # NACK reply
TER_LORA_PONG = 0x33 # LoRa pong state reply

#DATABASE FUNCTIONS 
# Database initialization and packet definition
def database_initialization(path=DB_PATH):
    """Initialize the SQLite database and create the packets table if it doesn't exist."""
    
    # A common table is defined for all the packets, specific tables are connected with the ID
    
    # If the database exists it will be opened, otherwise it will be created
    conn = sqlite3.connect(path)
    cursor = conn.cursor()
    
    # Packet table definition
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS packets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            HEX BLOB,
            source BLOB,
            ecc BLOB,
            tec_ter BLOB,
            pl_length INTEGER,
            unix REAL,
            mac TEXT,
            rssi REAL,
            snr REAL,
            deltaf REAL,
            comment TEXT
        )
    ''')
    
    # LORA_PONG table definition
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS LORA_PONG (
            id INTEGER PRIMARY KEY,
            rssi REAL,
            snr REAL,
            deltaf REAL
        )
    ''')

    # NACK table definition
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS NACK (
            id INTEGER PRIMARY KEY,
            task_ID INTEGER,
            error_code INTEGER
        )
    ''')

    # ACK table definition
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS ACK (
            id INTEGER PRIMARY KEY,
            task_ID INTEGER
        )
    ''')
    
    # OTHER TABLE DEFINITION HERE
    # NOTE: everytime a table is imported inside the database, his structure change,
    # so it has to be re-created(attention to not lose data)
    
    conn.commit()
    return conn

# Database check
def init_db(path=DB_PATH):
    """Initialize the database connection and check if the database file exists."""

    global connection # Global variable

    # Check if the database file exists
    db_check = os.path.exists(path)

    if not db_check: 
        # If the database does not exist, show an error message and return None
        connection = None
        conn = show_db_not_found(path)
    else: 
        # If the database exists, initialize it
        conn = database_initialization(path)

    return conn

# Window for database error visualization
def show_db_not_found(path = DB_PATH):
    """Function to show a window when the database is not found"""
    global connection # Global variable

    # Create the window for show the error message
    app = QApplication.instance() or QApplication(sys.argv)
    win = QWidget()
    win.setWindowTitle("Database Error")

    win.setStyleSheet("""
        QWidget {
            background-color: #f5f5f5;
        }
        QLabel {
            color: #d35400;
            font-size: 20pt;
            font-weight: bold;
            padding: 40px;
        }
        QPushButton {
            background-color: #f39c12;
            color: #333;
            border: none;
            padding: 8px 24px;
            font-size: 12pt;
            font-weight: bold;
            border-radius: 4px;
            margin: 0 10px;
        }
        QPushButton:hover {
            background-color: #e67e22;
        }
    """)

    # Showing main error message on the window
    layout = QVBoxLayout()
    label = QLabel("Database not founded")
    label.setAlignment(Qt.AlignCenter)
    layout.addWidget(label)

    # Button layout
    button_layout = QHBoxLayout()

    btn_CD = QPushButton("Create Database")
    btn_TA = QPushButton("Try Again")
    btn_MA = QPushButton("Manual access")

    btn_CD.clicked.connect(lambda: create_new_database(win, path))
    btn_TA.clicked.connect(lambda: try_again_function(win))
    btn_MA.clicked.connect(lambda: manual_access_function(win))

    button_layout.addWidget(btn_CD)
    button_layout.addWidget(btn_TA)
    button_layout.addWidget(btn_MA)

    layout.addLayout(button_layout)
    win.setLayout(layout)
    win.resize(400, 250)
    win.show()
    app.exec_()

    return connection

# Function to create a new database
def create_new_database(parent, path):
    """Function to create a new database, deleting the existing one if it exists."""

    dialog = QDialog(parent)
    dialog.setWindowTitle("Database creation")

    dialog.setStyleSheet("""
        QDialog {
            background-color: #f5f5f5;
        }
        QLabel {
            color: #d35400;
            font-size: 14pt;
            font-weight: bold;
            padding: 20px;
        }
        QPushButton {
            background-color: #f39c12;
            color: #333;
            border: none;
            padding: 8px 24px;
            font-size: 11pt;
            font-weight: bold;
            border-radius: 4px;
            margin: 0 10px;
        }
        QPushButton:hover {
            background-color: #e67e22;
        }
    """)

    # Checking window
    layout = QVBoxLayout()
    label = QLabel("Are you sure to create a new database?\nThis will delete the existing one if it exists.")
    label.setAlignment(Qt.AlignCenter)
    layout.addWidget(label)

    button_layout = QHBoxLayout()
    btn_cancel = QPushButton("EXIT")
    btn_create = QPushButton("CREATE DATABASE")

    # Setting the button as default
    btn_create.setDefault(True) 

    # Exit button is pressed EXIT
    btn_cancel.clicked.connect(dialog.reject)

    # Create the database if checked
    def do_create():
        global connection # Global variable
        connection = database_initialization(path)
        dialog.accept()
        parent.close()

    btn_create.clicked.connect(do_create)

    button_layout.addWidget(btn_cancel)
    button_layout.addWidget(btn_create)
    layout.addLayout(button_layout)
    dialog.setLayout(layout)
    dialog.exec_()

# Function to try to access again
def try_again_function(parent, path=DB_PATH):
    """Function to try again to connect to the database."""

    global connection # Global variable

    if connection == None: 
        connection = init_db(path)
    else: 
        connection == None

    parent.close()

# Function to manually access the database
def manual_access_function(parent):
    """Function to manually access the database by insering the path"""

    # Open a dialog to input the database path
    dialog = QDialog(parent)
    dialog.setWindowTitle("Manual Access")

    dialog.setStyleSheet("""
        QDialog {
            background-color: #f5f5f5;
        }
        QLabel {
            color: #d35400;
            font-size: 13pt;
            font-weight: bold;
            padding: 10px;
        }
        QLineEdit {
            background-color: #fff;
            color: #333;
            border: 1px solid #bbb;
            padding: 6px;
            font-size: 11pt;
        }
        QPushButton {
            background-color: #f39c12;
            color: #333;
            border: none;
            padding: 8px 24px;
            font-size: 11pt;
            font-weight: bold;
            border-radius: 4px;
            margin: 0 10px;
        }
        QPushButton:hover {
            background-color: #e67e22;
        }
    """)

    # Asking to insert the database path
    layout = QVBoxLayout()
    label = QLabel("Insert the database path:")
    layout.addWidget(label)

    text_input = QLineEdit()
    layout.addWidget(text_input)

    # Two buttons to confirm or exit
    button_layout = QHBoxLayout()
    btn_cancel = QPushButton("EXIT")
    btn_ok = QPushButton("OK")

    # Setting the button as default
    btn_ok.setDefault(True) 

    button_layout.addWidget(btn_cancel)
    button_layout.addWidget(btn_ok)
    layout.addLayout(button_layout)

    dialog.setLayout(layout)

    # If the text is inserted
    def on_ok():

        # Global variable
        global connection
        global DB_PATH 

        user_text = text_input.text()
        if connection == None:
            connection = init_db(user_text)
            DB_PATH = user_text  # Update the global DB_PATH variable
            parent.close()
        else:
            connection == None
        dialog.accept()

    btn_ok.clicked.connect(on_ok)
    btn_cancel.clicked.connect(dialog.reject)

    dialog.exec_()

# Saving function (save packet in database)
def save_packet(conn, timestamp, HEX_str, rssi_str, snr_str, deltaf_str, comment = ""):
    """conn, timestamp, HEX, rssi, snr, deltaf, comment [conn is the database definition --> use init_db()]"""

    cursor = conn.cursor()

    # Convert HEX string to bytes
    if isinstance(HEX_str, str):
        HEX = bytes.fromhex(HEX_str)

    # Convert str data in float
    rssi = float(rssi_str) if rssi_str else None
    snr = float(snr_str) if snr_str else None
    deltaf = float(deltaf_str) if deltaf_str else None

    # Convert timestamp to UTC datatime from UNIX
    dt = datetime.utcfromtimestamp(int(timestamp))

    # Packet decoding
    # Getting TER_TEC value from HEX
    HEX_decoded = gt.decode_packet(HEX)

    # Getting the decoded values from HEX_decoded
    ter_tec = HEX_decoded['ter']
    source = HEX_decoded['station_id']
    payload_bytes = HEX_decoded['payload_bytes']
    pl_length = HEX_decoded['payload_length']
    ecc = HEX_decoded['ecc_enabled']
    mac = HEX_decoded['mac']
    unix = HEX_decoded['timestamp']

    # Creation of the packet from raw data
    cursor.execute('''
        INSERT INTO packets (
            timestamp, HEX, source, ecc, tec_ter, pl_length, unix, mac, rssi, snr, deltaf, comment
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    ''', (dt, HEX, source, ecc, ter_tec, pl_length, unix, mac, rssi, snr, deltaf, comment))

    # Getting the packet ID
    packet_id = cursor.lastrowid

    # Saving the specific packet data
    # Saving data from TER LORA PONG
    if ter_tec == TER_LORA_PONG:
        data = gt.extract_lora_pong(payload_bytes)

        # Insert data in the LORA_PONG table
        cursor.execute('''
            INSERT INTO LORA_PONG (id, rssi, snr, deltaf)
            VALUES (?, ?, ?, ?)
        ''', (packet_id, data['rssi'], data['snr'], data['deltaf']))

    # Saving data from NACK
    elif ter_tec == TER_NACK:
        data = gt.extract_nack(payload_bytes)

        # Insert data in the ACK table
        cursor.execute('''
            INSERT INTO NACK (id, task_ID, error_code)
            VALUES (?, ?, ?)
        ''', (packet_id, data['task_ID'], data['error_code']))
    
    # Saving data from ACK
    elif ter_tec == TER_ACK:
        data = gt.extract_ack(payload_bytes)

        # Insert data in the ACK table
        cursor.execute('''
            INSERT INTO ACK (id, task_ID)
            VALUES (?, ?)
        ''', (packet_id, data['task_ID']))

    # Commit packet to database
    conn.commit()

# Show all packets in terminal(usefull for debug operations)
def show_all_packets(conn):
    cursor = conn.cursor()
    cursor.execute('''
        SELECT id, timestamp, HEX, source, ecc, tec_ter, pl_length, unix, mac, rssi, snr, deltaf, comment
        FROM packets
        ORDER BY timestamp
    ''')
    rows = cursor.fetchall()
    print("ID | Timestamp           | HEX         | Source      | ECC        | TEC_TER    | PL_LEN | UNIX        | MAC           | RSSI   | SNR    | DELTAF  | Comment")
    print("-" * 160)
    for row in rows:
        print(f"{row[0]:<3} | {row[1]:<20} | {str(row[2])[:10]:<10} | {str(row[3])[:8]:<8} | {str(row[4])[:8]:<8} | {str(row[5])[:8]:<8} | {row[6]:<6} | {row[7]:<10} | {str(row[8]):<13} | {row[9]:<6} | {row[10]:<6} | {row[11]:<7} | {row[12]}")

# Function to export packets to Excel
def export_tables_to_excel(conn, excel_path="packets_export.xlsx"):
    """Export all tables to a single Excel file with multiple sheets."""

    # Packet and table definition
    tables = ["packets", "LORA_PONG", "NACK", "ACK"]

    # Export the tables in different sheets of the same Excel file
    with pd.ExcelWriter(excel_path, engine="openpyxl") as writer:
        for table in tables:
            try:
                df = pd.read_sql_query(f"SELECT * FROM {table}", conn)
                df.to_excel(writer, sheet_name=table, index=False)
            except Exception as e:
                print(f"[ERROR] Impossible to export {table}: {e}")

# Create the GUI for database visualization
def open_database():
    
    # Check if the database exists
    if not os.path.exists(DB_PATH):
        app = QApplication(sys.argv)
        app.setStyleSheet("""
        QDialog {
            background-color: #f5f5f5;
        }
        QLabel {
            color: #d35400;
            font-size: 14pt;
            font-weight: bold;
            padding: 20px;
        }
        QPushButton {
            background-color: #f39c12;
            color: #333;
            border: none;
            padding: 8px 24px;
            font-size: 11pt;
            font-weight: bold;
            border-radius: 4px;
            margin: 0 10px;
        }
        QPushButton:hover {
            background-color: #e67e22;
        }
    """)
        QMessageBox.critical(None, "ERROR", "Database not found")
        return

    # If the database exists, show the database viewer
    class PacketViewer(QWidget):
        def __init__(self):
            super().__init__()

            self.show_ter_decoded = False  # Flag to toggle TER_decoded display
            
            # Set the theme for GUI
            self.setStyleSheet("""
            QPushButton {
                background-color: #f39c12;
                color: #333;
                border: none;
                padding: 5px 10px;
            }
            QPushButton:hover {
                background-color: #e67e22;
            }
            QLineEdit, QComboBox, QDateEdit {
                background-color: #fff;
                color: #333;
                border: 1px solid #bbb;
            }
            QLabel {
                color: #333;
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


            # Filter for date range
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

            # Filter for ground station ID
            self.gs_input = QLineEdit()
            filter_layout.addWidget(QLabel("Ground Station ID:"))
            filter_layout.addWidget(self.gs_input)

            # Filter for TER/TEC name
            self.status_input = QLineEdit()
            filter_layout.addWidget(QLabel("TER/TEC Names:"))
            filter_layout.addWidget(self.status_input)

            # Bottone refresh
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
                    background-color: #fff;
                    color: #333;
                    gridline-color: #bbb;
                }
                QHeaderView::section {
                    background-color: #f5f5f5;
                    color: #333;
                    font-weight: bold;
                    border: 1px solid #bbb;
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
            details_label.setStyleSheet("font-size: 12pt; font-weight: bold; color: #333;")

            # Setting the delete button layout
            self.delete_btn = QPushButton("DELETE PACKET")
            self.delete_btn.setStyleSheet("""
                QPushButton {
                    background-color: #b30000;
                    color: white;
                    border: none;
                    padding: 4px 10px;
                    font-weight: bold;
                    font-size: 11pt;
                }
                QPushButton:hover {
                    background-color: #c0392b;
                }
            """)
            self.delete_btn.clicked.connect(self.delete_selected_packet)

            # Button to toggle TER_decoded display
            self.ter_toggle_btn = QPushButton("TER DATA")
            self.ter_toggle_btn.setStyleSheet("""
                QPushButton {
                    background-color: #27ae60;
                    color: white;
                    border: none;
                    padding: 4px 10px;
                    font-weight: bold;
                    font-size: 11pt;
                    border-radius: 4px;
                    margin-left: 10px;
                }
                QPushButton:hover {
                    background-color: #229954;
                }
            """)
            self.ter_toggle_btn.clicked.connect(self.toggle_ter_decoded)

            header_layout.addWidget(details_label)
            header_layout.addStretch()
            header_layout.addWidget(self.ter_toggle_btn) # TER toggle button
            header_layout.addWidget(self.delete_btn)

            right_panel.addLayout(header_layout)

            self.detail_frames = []

            # Modify the layout for the right panel
            for _ in range(7):
                frame = QFrame()
                frame.setStyleSheet("""
                    QFrame {
                        background-color: #f9f9f9;
                        border: 1px solid #ddd;
                        border-radius: 6px;
                        padding: 8px;
                        margin-bottom: 6px;
                    }
                    QLabel {
                        color: #333;
                        font-family: Consolas, monospace;
                        font-size: 10pt;
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

            query = "SELECT id, timestamp, HEX, source, ecc, tec_ter, pl_length, unix, mac, rssi, snr, deltaf, comment FROM packets WHERE 1=1"
            params = []

            # Filter for date range
            from_date = self.date_from.date().toString("yyyy-MM-dd")
            to_date = self.date_to.date().toString("yyyy-MM-dd")
            query += " AND date(timestamp) >= ? AND date(timestamp) <= ?"
            params.extend([from_date, to_date])

            # Filter for ground station ID (source field)
            gs_id = self.gs_input.text().strip()
            if gs_id:
                query += " AND source = ?"
                params.append(gs_id)

            # Filter for TER/TEC name (tec_ter field)
            status = self.status_input.text().strip()
            if status:
                query += " AND tec_ter LIKE ?"
                params.append(f"%{status}%")

            cursor.execute(query, params)
            self.packets = cursor.fetchall()

            self.table.setRowCount(len(self.packets))
            self.table.setColumnCount(5)
            self.table.setHorizontalHeaderLabels(["ID", "Timestamp", "HEX", "MAC", "Comment"])

            # Show only the principal info in the table
            for i, pkt in enumerate(self.packets):
                self.table.setItem(i, 0, QTableWidgetItem(str(pkt[0])))  # ID
                self.table.setItem(i, 1, QTableWidgetItem(str(pkt[1])))  # Timestamp
                self.table.setItem(i, 2, QTableWidgetItem(str(pkt[2])[:16]))  # HEX (first 16 characters)
                self.table.setItem(i, 3, QTableWidgetItem(str(pkt[8])))  # MAC
                self.table.setItem(i, 4, QTableWidgetItem(str(pkt[12]))) # Comment

            conn.close()

        # Function to show the packet details when a row is clicked
        def show_packet_details(self, row, col):
            pkt = self.packets[row]
            # HEX preview (BLOB): mostra primi 32 caratteri HEX
            hex_blob = pkt[2]
            if isinstance(hex_blob, bytes):
                # Show the first 16 bytes as preview
                hex_preview = str(hex_blob[:16]) + ("..." if len(hex_blob) > 16 else "")
                hex_full = str(hex_blob)
            else:
                # If not bytes, show the string
                hex_preview = str(hex_blob)[:32] + ("..." if len(str(hex_blob)) > 32 else "")
                hex_full = str(hex_blob)

            # TER decoded data (only if LORA_PONG)
            conn = sqlite3.connect(DB_PATH)
            cursor = conn.cursor()
            cursor.execute("SELECT rssi, snr, deltaf FROM LORA_PONG WHERE id = ?", (pkt[0],))
            ter_row = cursor.fetchone()
            conn.close()
            self.ter_decoded_data = ter_row

            # Prepare the rows with the new structure
            if self.show_ter_decoded and self.ter_decoded_data:
                rssi, snr, deltaf = self.ter_decoded_data
                snr_row = f"RSSI: {rssi}    |   SNR: {snr}    |   DELTAF: {deltaf}"
            else:
                snr_row = f"RSSI: {pkt[9]}    |   SNR: {pkt[10]}    |   DELTAF: {pkt[11]}"

            rows = [
                f"ID: {pkt[0]}",
                f"Timestamp: {pkt[1].replace('T', ' ')}",
                f"HEX: {hex_preview}",
                f"Source: {pkt[3]}    |   ECC: {pkt[4]}    |   TEC_TER: {pkt[5]}",
                f"PL_LENGTH: {pkt[6]}    |   UNIX: {pkt[7]}",
                f"MAC: {pkt[8]}",
                snr_row,
                f"Comment: {pkt[12]}"
            ]

            # Show the rows in the frames
            for i, ((frame, layout), text) in enumerate(zip(self.detail_frames, rows)):
                # Clear the layout
                for j in reversed(range(layout.count())):
                    item = layout.itemAt(j)
                    widget = item.widget()
                    if widget is not None:
                        layout.removeWidget(widget)
                        widget.deleteLater()
                    else:
                        sub_layout = item.layout()
                        if sub_layout is not None:
                            for k in reversed(range(sub_layout.count())):
                                sub_item = sub_layout.itemAt(k)
                                sub_widget = sub_item.widget()
                                if sub_widget is not None:
                                    sub_layout.removeWidget(sub_widget)
                                    sub_widget.deleteLater()
                            layout.removeItem(sub_layout)

                # HEX preview row (index 2): add Show All button
                if i == 2:
                    hbox = QHBoxLayout()
                    label = QLabel(text)
                    hbox.addWidget(label)
                    btn = QPushButton("Show All")
                    btn.setStyleSheet("""
                        QPushButton {
                            background-color: #f39c12;
                            color: #333;
                            border: none;
                            padding: 2px 8px;
                            font-size: 9pt;
                            font-weight: bold;
                            border-radius: 4px;
                            margin-left: 10px;
                        }
                        QPushButton:hover {
                            background-color: #e67e22;
                        }
                    """)
                    btn.clicked.connect(lambda _, p=hex_blob: self.show_full_packet(p))
                    hbox.addWidget(btn)
                    layout.addLayout(hbox)
                else:
                    label = QLabel(text)
                    layout.addWidget(label)
        
        # Function to toggle between normal and TER_decoded values
        def toggle_ter_decoded(self):
            self.show_ter_decoded = not self.show_ter_decoded
            # Update button text
            if self.show_ter_decoded:
                self.ter_toggle_btn.setText("GS DATA")
            else:
                self.ter_toggle_btn.setText("TER DATA")
            selected_row = self.table.currentRow()
            if selected_row >= 0:
                self.show_packet_details(selected_row, 0)

        # Function to show all the payload
        def show_full_payload(self, payload):

            # Creating the window
            dialog = QDialog(self)
            dialog.setWindowTitle("Full Payload")
            dialog.setStyleSheet(self.styleSheet())
            dialog.resize(700, 400)

            vbox = QVBoxLayout()
            label = QLabel("Full Payload:")
            label.setStyleSheet("font-size: 12pt; font-weight: bold; color: #333;")
            vbox.addWidget(label)

            text = QTextEdit()
            text.setReadOnly(True)
            text.setText(str(payload))

            # Setting the window style
            text.setStyleSheet("""
                QTextEdit {
                    background-color: #fff;
                    color: #333;
                    font-family: Consolas, monospace;
                    font-size: 10pt;
                    border: 1px solid #bbb;
                    border-radius: 6px;
                }
            """)
            vbox.addWidget(text)

            # Button to close the window
            btn = QPushButton("Close")
            btn.setStyleSheet("""
                QPushButton {
                    background-color: #f39c12;
                    color: #333;
                    border: none;
                    padding: 5px 15px;
                    font-size: 11pt;
                    font-weight: bold;
                    border-radius: 4px;
                }
                QPushButton:hover {
                    background-color: #e67e22;
                }
            """)
            btn.clicked.connect(dialog.accept)
            vbox.addWidget(btn, alignment=Qt.AlignRight)

            dialog.setLayout(vbox)
            dialog.exec_()
            
        # Function to show the full packet in a new window
        def show_full_packet(self, packet):

            # Creating the window
            dialog = QDialog(self)
            dialog.setWindowTitle("Full Packet")
            dialog.setStyleSheet(self.styleSheet())
            dialog.resize(700, 400)

            vbox = QVBoxLayout()
            label = QLabel("Full Packet:")
            label.setStyleSheet("font-size: 12pt; font-weight: bold; color: #333;")
            vbox.addWidget(label)

            text = QTextEdit()
            text.setReadOnly(True)
            text.setText(str(packet))

            # Setting the window style
            text.setStyleSheet("""
                QTextEdit {
                    background-color: #fff;
                    color: #333;
                    font-family: Consolas, monospace;
                    font-size: 10pt;
                    border: 1px solid #bbb;
                    border-radius: 6px;
                }
            """)
            vbox.addWidget(text)

            # Button to close the window
            btn = QPushButton("Close")
            btn.setStyleSheet("""
                QPushButton {
                    background-color: #f39c12;
                    color: #333;
                    border: none;
                    padding: 5px 15px;
                    font-size: 11pt;
                    font-weight: bold;
                    border-radius: 4px;
                }
                QPushButton:hover {
                    background-color: #e67e22;
                }
            """)
            btn.clicked.connect(dialog.accept)
            vbox.addWidget(btn, alignment=Qt.AlignRight)

            dialog.setLayout(vbox)
            dialog.exec_()

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
                        background-color: #f5f5f5;
                        color: #333;
                        font-size: 11pt;
                    }
                    QPushButton {
                        background-color: #f39c12;
                        color: #333;
                        border: none;
                        padding: 5px 10px;
                    }
                    QPushButton:hover {
                        background-color: #e67e22;
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
                    background-color: #f5f5f5;
                    color: #333;
                    font-size: 11pt;
                }
                QPushButton {
                    background-color: #f39c12;
                    color: #333;
                    border: none;
                    padding: 5px 10px;
                }
                QPushButton:hover {
                    background-color: #e67e22;
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

    # White theme
    light_palette = QPalette()
    light_palette.setColor(QPalette.Window, QColor(245, 245, 245))
    light_palette.setColor(QPalette.WindowText, QColor(51, 51, 51))
    light_palette.setColor(QPalette.Base, QColor(255, 255, 255))
    light_palette.setColor(QPalette.AlternateBase, QColor(240, 240, 240))
    light_palette.setColor(QPalette.ToolTipBase, QColor(255, 255, 220))
    light_palette.setColor(QPalette.ToolTipText, QColor(51, 51, 51))
    light_palette.setColor(QPalette.Text, QColor(51, 51, 51))
    light_palette.setColor(QPalette.Button, QColor(240, 240, 240))
    light_palette.setColor(QPalette.ButtonText, QColor(51, 51, 51))
    light_palette.setColor(QPalette.BrightText, QColor(255, 0, 0))
    light_palette.setColor(QPalette.Highlight, QColor(243, 156, 18))
    light_palette.setColor(QPalette.HighlightedText, QColor(255, 255, 255))

    # Apply light theme
    QApplication.setPalette(light_palette)

    global db_viewer
    app = QApplication.instance() or QApplication(sys.argv)
    db_viewer = PacketViewer()
    db_viewer.resize(1300, 700) # Setting window dimensions
    db_viewer.show()
    # Run the application event loop only if not already running
    if not QApplication.instance() or not app.thread().isRunning():
        sys.exit(app.exec_())

# Debug/test
if __name__ == "__main__":
    conn = init_db()

    # Example of telemetry packet saving
    # HEX_data deve essere una stringa esadecimale senza prefisso b''
    # HEX_data = "01aa330c677485dad2ea8b07c2df0000c1280000438d089200000000"
    # save_packet(
    #     conn,
    #     timestamp=datetime.utcnow().timestamp(),  # timestamp UNIX
    #     HEX_str=HEX_data,
    #     rssi_str="-70.0",
    #     snr_str="10.1",
    #     deltaf_str="0.0",
    #     comment="Dati telemetria"
    # )
