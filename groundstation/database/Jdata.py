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
from groundstation import GS_task as gt

DB_PATH = "./groundstation/database/SatelliteDB.db"
PACKET_HEADER_LENGTH = 12 # 4 bytes for header + 4 bytes for MAC + 4 bytes for timestamp
BYTE_RS_ON = 0xAA
import pandas as pd
BYTE_RS_OFF = 0x55

# TER VALUES
TER_BEACON = 0x30 # telemetry beacon reply
TER_ACK = 0x31 # ACK reply
TER_NACK = 0x32 # NACK reply
TER_LORA_PONG = 0x33 # LoRa pong state reply

# ============ DATABASE INITIALIZATION ============

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
            GS_time TEXT,
            HEX BLOB,
            source BLOB,
            ecc BLOB,
            tec_ter BLOB,
            pl_length INTEGER,
            TX_time TEXT,
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

# ============ DATABASE OPERATIONS ============

# Saving function (save packet in database)
def save_packet(conn, GS_time, HEX_str, rssi_str, snr_str, deltaf_str, comment = ""):
    """conn, GS_time, HEX, rssi, snr, deltaf, comment [conn is the database definition --> use init_db()]"""

    cursor = conn.cursor()

    # Convert HEX string to bytes
    if isinstance(HEX_str, str):
        HEX = bytes.fromhex(HEX_str)

    # Convert str data in float
    rssi = float(rssi_str) if rssi_str else None
    snr = float(snr_str) if snr_str else None
    deltaf = float(deltaf_str) if deltaf_str else None

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
    TX_time = datetime.utcfromtimestamp(HEX_decoded['timestamp']) # Convert UNIX to UTC datetime

    # Creation of the packet from raw data
    cursor.execute('''
        INSERT INTO packets (
            GS_time, HEX, source, ecc, tec_ter, pl_length, TX_time, mac, rssi, snr, deltaf, comment
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    ''', (GS_time, HEX, source, ecc, ter_tec, pl_length, TX_time, mac, rssi, snr, deltaf, comment))

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
        SELECT id, GS_time, HEX, source, ecc, tec_ter, pl_length, TX_time, mac, rssi, snr, deltaf, comment
        FROM packets
        ORDER BY GS_time
    ''')
    rows = cursor.fetchall()
    print("ID | GS_time            | HEX         | Source      | ECC        | TEC_TER    | PL_LEN | TX_time        | MAC           | RSSI   | SNR    | DELTAF  | Comment")
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

# ============ DATABASE GUI ============

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

            self.show_ter_decoded = False
            self.setWindowTitle("Jdata Database - Viewer")
            self.resize(1300, 800)

            # Shared style
            self.setStyleSheet("""
            QPushButton { background-color: #f39c12; color: #333; border: none; padding: 6px 10px; }
            QPushButton:hover { background-color: #e67e22; }
            QLineEdit, QComboBox, QDateEdit { background-color: #fff; color: #333; border: 1px solid #bbb; }
            QLabel { color: #333; }
            QTableWidget { background-color: #fff; color: #333; }
            """)

            # Top: filters/search bar
            self.top_layout = QHBoxLayout()

            # Searching for comment only
            self.search_input = QLineEdit()
            self.search_input.setPlaceholderText("Search packets comment...")
            self.search_input.returnPressed.connect(self.load_data)

            # Searching for TEC/TER types
            self.type_combo = QComboBox()
            self.type_combo.addItem("All Types")
            self.type_combo.currentIndexChanged.connect(self.load_data)
            
            # populate types from DB
            self.populate_type_combo()
            
            # Searching for GS id
            self.gs_id_input = QComboBox()
            self.gs_id_input.addItem("All GS IDs")
            self.gs_id_input.currentIndexChanged.connect(self.load_data)

            # populate GS IDs from DB
            self.populate_gs_id_combo()

            # Search by date range
            self.date_from = QDateEdit(calendarPopup=True)
            self.date_from.setDisplayFormat("yyyy-MM-dd")
            self.date_from.setDate(QDate.currentDate().addDays(-30))
            self.date_to = QDateEdit(calendarPopup=True)
            self.date_to.setDisplayFormat("yyyy-MM-dd")
            self.date_to.setDate(QDate.currentDate())

            # Refresh button
            self.refresh_btn = QPushButton("Refresh")
            self.refresh_btn.clicked.connect(self.load_data)

            # Showing each button in the research bar
            self.top_layout.addWidget(QLabel("Type:"))
            self.top_layout.addWidget(self.type_combo, stretch=1)
            self.top_layout.addWidget(QLabel("GS ID:"))
            self.top_layout.addWidget(self.gs_id_input, stretch=1)
            self.top_layout.addWidget(QLabel("From:"))
            self.top_layout.addWidget(self.date_from)
            self.top_layout.addWidget(QLabel("To:"))
            self.top_layout.addWidget(self.date_to)
            self.top_layout.addWidget(QLabel("Search:"))
            self.top_layout.addWidget(self.search_input, stretch=2)
            self.top_layout.addWidget(self.refresh_btn)

            # Main grid: 2x2 below the search bar
            grid = QGridLayout()

            # Top-left: full filtered packet list (ID, type, date, comment)
            self.left_top_table = QTableWidget()
            self.left_top_table.setColumnCount(5)
            self.left_top_table.setHorizontalHeaderLabels(["ID", "Type", "Gs ID", "Date", "Comment"])
            self.left_top_table.verticalHeader().setVisible(False)
            self.left_top_table.setSelectionBehavior(QAbstractItemView.SelectRows)
            self.left_top_table.setSelectionMode(QAbstractItemView.SingleSelection)
            self.left_top_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
            self.left_top_table.cellClicked.connect(self.on_left_table_select)

            # Bottom-left: last saved packet
            self.left_bottom_group = QGroupBox("Last saved packet")
            lb_layout = QVBoxLayout()
            self.last_packet_text = QTextEdit()
            self.last_packet_text.setReadOnly(True)
            lb_layout.addWidget(self.last_packet_text)
            self.left_bottom_group.setLayout(lb_layout)

            # Top-right: detailed packet characteristics
            self.right_top_group = QGroupBox("Packet Details")
            rt_layout = QVBoxLayout()
            self.packet_details = QTextEdit()
            self.packet_details.setReadOnly(True)
            rt_layout.addWidget(self.packet_details)
            
            # Action buttons (delete / toggle)
            btn_row = QHBoxLayout()
            self.delete_btn = QPushButton("DELETE PACKET")
            self.delete_btn.clicked.connect(self.delete_selected_packet)
            
            # Button to export to excel
            self.export_btn = QPushButton("EXPORT TO EXCEL")
            
            # Function to export to excel with GUI notification
            def export_to_excel_db_gui():
                conn = sqlite3.connect(DB_PATH)
                export_tables_to_excel(conn)
                QMessageBox.information(self, "Export to Excel", "Data exported successfully.")

            # Connect the export button to the export function
            self.export_btn.clicked.connect(export_to_excel_db_gui)

            btn_row.addStretch()
            btn_row.addWidget(self.delete_btn)
            btn_row.addWidget(self.export_btn)
            rt_layout.addLayout(btn_row)
            self.right_top_group.setLayout(rt_layout)

            # Bottom-right: related table values (LORA_PONG, ACK, NACK)
            self.right_bottom_group = QGroupBox("Related Tables")
            rb_layout = QVBoxLayout()
            self.related_table = QTableWidget()
            self.related_table.setColumnCount(2)
            self.related_table.setHorizontalHeaderLabels(["Field", "Value"])
            self.related_table.verticalHeader().setVisible(False)
            self.related_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
            rb_layout.addWidget(self.related_table)
            self.right_bottom_group.setLayout(rb_layout)

            # Arrange grid
            grid.addWidget(self.left_top_table, 0, 0)
            grid.addWidget(self.right_top_group, 0, 1)
            grid.addWidget(self.left_bottom_group, 1, 0)
            grid.addWidget(self.right_bottom_group, 1, 1)

            # Main vertical layout: search on top, grid below
            main_v = QVBoxLayout()
            main_v.addLayout(self.top_layout)
            main_v.addLayout(grid)
            self.setLayout(main_v)

            # Internal buffers
            self.packets = []
            # Improve spacing and indentation for the top-left section
            grid.setHorizontalSpacing(12)
            grid.setVerticalSpacing(12)
            grid.setContentsMargins(12, 12, 12, 12)

            # Make the timestamp column wider so the full datetime is visible
            # Columns: 0 ID, 1 Type, 2 Gs ID, 3 Date, 4 Comment
            header = self.left_top_table.horizontalHeader()
            try:
                # Set reasonable default widths
                header.setSectionResizeMode(0, header.ResizeToContents)
                header.setSectionResizeMode(1, header.ResizeToContents)
                header.setSectionResizeMode(2, header.ResizeToContents)
                # Make Date column fixed wider so timestamps are shown
                header.setSectionResizeMode(3, header.Fixed)
                self.left_top_table.setColumnWidth(3, 260)
                # Let comment column stretch to fill remaining space
                header.setSectionResizeMode(4, header.Stretch)
            except Exception:
                # Fallback: ignore if header API differs
                pass

            self.load_data()

        # ============ DATABASE GUI FUNCTIONS ============

        # Function to load packets from DB applying search/date filters
        def load_data(self):
            """Load packets from DB applying search/date filters and populate left_top_table and last packet."""
            conn = sqlite3.connect(DB_PATH)
            cursor = conn.cursor()

            query = "SELECT id, GS_time, HEX, source, ecc, tec_ter, pl_length, TX_time, mac, rssi, snr, deltaf, comment FROM packets WHERE 1=1"
            params = []

            # Date range filter
            from_date = self.date_from.date().toString("yyyy-MM-dd")
            to_date = self.date_to.date().toString("yyyy-MM-dd")
            query += " AND date(GS_time) >= ? AND date(GS_time) <= ?"
            params.extend([from_date, to_date])

            # Search input: comment
            s = self.search_input.text().strip()
            if s:
                query += " AND comment LIKE ?"
                params.append(f"%{s}%")

            # Searching for GS id
            gs_id = self.gs_id_input.currentText()
            if gs_id and gs_id != "All GS IDs":
                query += " AND source = ?"
                params.append(gt.get_gs_id(gs_id))

            # Filter by TEC or TER types
            s_type = self.type_combo.currentText()
            if s_type and s_type == "All Types":
                pass
            if s_type and s_type != "All Types":
                query += " AND tec_ter = ?"
                params.append(gt.get_ter_tec_id(s_type))

            query += " ORDER BY GS_time DESC"
            cursor.execute(query, params)
            rows = cursor.fetchall()
            self.packets = rows

            # Populate left_top_table
            self.left_top_table.setRowCount(len(rows))
            for i, pkt in enumerate(rows):
                pkt_id = str(pkt[0])
                tec_ter = str(pkt[5])
                gs_time = str(pkt[1])
                gs_id = str(pkt[3])
                comment = str(pkt[12]) if pkt[12] is not None else ""
                self.left_top_table.setItem(i, 0, QTableWidgetItem(pkt_id))
                self.left_top_table.setItem(i, 1, QTableWidgetItem(tec_ter))
                self.left_top_table.setItem(i, 2, QTableWidgetItem(gs_id))
                self.left_top_table.setItem(i, 3, QTableWidgetItem(gs_time))
                self.left_top_table.setItem(i, 4, QTableWidgetItem(comment))

            # Load last saved packet into left_bottom
            if rows:
                last = rows[0]
                self.show_last_packet(last)
            else:
                self.last_packet_text.setPlainText("No packets found")

            conn.close()

        # Function to show TEC and TER types in the research bar
        def populate_type_combo(self):
            try:
                conn = sqlite3.connect(DB_PATH)
                cursor = conn.cursor()
                cursor.execute("SELECT DISTINCT tec_ter FROM packets ORDER BY tec_ter")
                rows = cursor.fetchall()
                for r in rows:
                    val = str(r[0])
                    if val not in [self.type_combo.itemText(i) for i in range(self.type_combo.count())]:
                        self.type_combo.addItem(gt.get_ter_tec_label(r[0]))
                conn.close()
            except Exception:
                # ignore if DB not accessible at populate time
                pass
        
        # Function to show GS IDs in the research bar
        def populate_gs_id_combo(self):
            try:
                conn = sqlite3.connect(DB_PATH)
                cursor = conn.cursor()
                cursor.execute("SELECT DISTINCT source FROM packets ORDER BY source")
                rows = cursor.fetchall()
                for r in rows:
                    val = str(r[0])
                    if val not in [self.gs_id_input.itemText(i) for i in range(self.gs_id_input.count())]:
                        self.gs_id_input.addItem(gt.get_gs_label(r[0]))
                conn.close()
            except Exception:
                # ignore if DB not accessible at populate time
                pass
        
        # Function to show information about the last saved packet
        def show_last_packet(self, pkt):
            # pkt is a row from packets SELECT
            hex_blob = pkt[2]
            if isinstance(hex_blob, bytes):
                hex_preview = ' '.join(f"{b:02X}" for b in hex_blob[:32])
            else:
                hex_preview = str(hex_blob)[:256]

            # Extract informatin about ECC
            if pkt[4] == 1:
                ecc_info = "Enabled"
            else:
                ecc_info = "Disabled"

            html = (
                "<style>"
                "table { border-collapse: collapse; width: 100%; }"
                "td, th { border: 1px solid #bbb; padding: 3px 6px; font-size: 9pt; }"
                "th { background: #f9f9f9; color: #333; font-weight: bold; }"
                "tr:nth-child(even) { background: #f5f5f5; }"
                ".label { width: 100px; text-align: right; color: #555; font-weight: bold; font-size: 9pt; }"
                ".value { text-align: left; color: #222; font-size: 9pt; }"
                "</style>"
                "<table width='100%'>"
            )

            html += f'<tr><td class="label">ID</td><td class="value">{pkt[0]}</td><td class="label">Source</td><td class="value">{gt.get_gs_label(pkt[3])}</td></tr>'
            html += f'<tr><td class="label">ECC</td><td class="value">{ecc_info}</td><td class="label">TEC_TER</td><td class="value">{gt.get_ter_tec_label(pkt[5])}</td></tr>'
            html += f'<tr><td class="label">PL_LEN</td><td class="value" colspan="3">{pkt[6]}</td></tr>'
            html += f'<tr><td class="label">GS_time</td><td class="value" colspan="3">{pkt[1]}</td></tr>'
            html += f'<tr><td class="label">TX_time</td><td class="value" colspan="3">{pkt[7]}</td></tr>'
            html += f'<tr><td class="label">MAC</td><td class="value" colspan="3">{pkt[8]}</td></tr>'
            html += f'<tr><td class="label">HEX</td><td class="value" colspan="3">{hex_preview}</td></tr>'
            html += f'<tr><td class="label">RSSI</td><td class="value" colspan="3">{pkt[9]}</td></tr>'
            html += f'<tr><td class="label">SNR</td><td class="value" colspan="3">{pkt[10]}</td></tr>'
            html += f'<tr><td class="label">DELTAF</td><td class="value" colspan="3">{pkt[11]}</td></tr>'
            html += f'<tr><td class="label">Comment</td><td class="value" colspan="3">{pkt[12] if pkt[12] is not None else ""}</td></tr>'
            html += "</table>"
            self.last_packet_text.setHtml(html)

        # Function to handle selection of a packet in the left table
        def on_left_table_select(self, row, col):

            # Called when a packet row is selected in left_top_table
            if row < 0 or row >= len(self.packets):
                return
            pkt = self.packets[row]
            self.show_packet_details(pkt)

        # Function to show detailed packet information in the right top section
        def show_packet_details(self, pkt):

            # pkt is a tuple from SELECT
            # Show details in right_top_group as a two-column HTML table
            hex_blob = pkt[2]
            if isinstance(hex_blob, bytes):
                hex_preview = ' '.join(f"{b:02X}" for b in hex_blob[:32])
            else:
                hex_preview = str(hex_blob)[:256]

            # Extract informatin about ECC
            if pkt[4] == 1:
                ecc_info = "Enabled"
            else:
                ecc_info = "Disabled"

            # Compact layout: font 8pt, minimal padding, RSSI/SNR/DELTAF on one row
            html = (
                "<style>"
                "table { border-collapse: collapse; width: 100%; }"
                "td, th { border: 1px solid #bbb; padding: 3px 6px; font-size: 9pt; }"
                "th { background: #f9f9f9; color: #333; font-weight: bold; }"
                "tr:nth-child(even) { background: #f5f5f5; }"
                ".label { width: 100px; text-align: right; color: #555; font-weight: bold; font-size: 9pt; }"
                ".value { text-align: left; color: #222; font-size: 9pt; }"
                "</style>"
                "<table width='100%'>"
            )

            # ID, Source, ECC, TEC_TER, PL_LEN in two columns
            html += f'<tr><td class="label">ID</td><td class="value">{pkt[0]}</td><td class="label">Source</td><td class="value">{gt.get_gs_label(pkt[3])}</td></tr>'
            html += f'<tr><td class="label">ECC</td><td class="value">{ecc_info}</td><td class="label">TEC_TER</td><td class="value">{gt.get_ter_tec_label(pkt[5])}</td></tr>'
            html += f'<tr><td class="label">PL_LEN</td><td class="value" colspan="3">{pkt[6]}</td></tr>'

            # GS_time, TX_time, MAC each on their own row (full width)
            html += f'<tr><td class="label">GS_time</td><td class="value" colspan="3">{pkt[1]}</td></tr>'
            html += f'<tr><td class="label">TX_time</td><td class="value" colspan="3">{pkt[7]}</td></tr>'
            html += f'<tr><td class="label">MAC</td><td class="value" colspan="3">{pkt[8]}</td></tr>'

            # HEX preview (full width)
            html += f'<tr><td class="label">HEX</td><td class="value" colspan="3">{hex_preview}</td></tr>'

            # RSSI, SNR, DELTAF each on their own row
            html += f'<tr><td class="label">RSSI</td><td class="value">{pkt[9]}</td><td class="label">SNR</td><td class="value">{pkt[10]}</td></tr>'
            html += f'<tr><td class="label">DELTAF</td><td class="value" colspan="3">{pkt[11]}</td></tr>'

            # Comment (full width)
            html += f'<tr><td class="label">Comment</td><td class="value" colspan="3">{pkt[12] if pkt[12] is not None else ""}</td></tr>'

            html += "</table>"

            self.packet_details.setHtml(html)

            # Load related tables
            self.load_related_tables(pkt[0], pkt[5])

        # Function to load related tables (LORA_PONG, ACK, NACK) for the selected packet
        def load_related_tables(self, pkt_id, tec_ter):
            # Clear related_table
            self.related_table.setRowCount(0)

            # Show packet type (decoded label) as first row
            try:
                code = int(tec_ter)
            except Exception:
                code = tec_ter
            # Try both TER and TEC
            label = gt.get_ter_tec_label(code)
            self.related_table.insertRow(0)
            self.related_table.setItem(0, 0, QTableWidgetItem("Packet Type"))
            self.related_table.setItem(0, 1, QTableWidgetItem(label))
            self.related_table.setItem(0, 2, QTableWidgetItem(""))

            conn = sqlite3.connect(DB_PATH)
            cursor = conn.cursor()

            # Check LORA_PONG
            cursor.execute("SELECT rssi, snr, deltaf FROM LORA_PONG WHERE id = ?", (pkt_id,))
            row = cursor.fetchone()
            if row:
                r = self.related_table.rowCount()
                self.related_table.insertRow(r)
                self.related_table.setItem(r, 0, QTableWidgetItem("RSSI"))
                self.related_table.setItem(r, 1, QTableWidgetItem(str(row[0])))
                self.related_table.setItem(r, 2, QTableWidgetItem(""))
                r = self.related_table.rowCount()
                self.related_table.insertRow(r)
                self.related_table.setItem(r, 0, QTableWidgetItem("SNR"))
                self.related_table.setItem(r, 1, QTableWidgetItem(str(row[1])))
                self.related_table.setItem(r, 2, QTableWidgetItem(""))
                r = self.related_table.rowCount()
                self.related_table.insertRow(r)
                self.related_table.setItem(r, 0, QTableWidgetItem("DELTAF"))
                self.related_table.setItem(r, 1, QTableWidgetItem(f"{row[2]:.2f}"))
                self.related_table.setItem(r, 2, QTableWidgetItem(""))

            # Check ACK
            cursor.execute("SELECT task_ID FROM ACK WHERE id = ?", (pkt_id,))
            row_ack = cursor.fetchone()
            if row_ack:
                r = self.related_table.rowCount()
                self.related_table.insertRow(r)
                self.related_table.setItem(r, 0, QTableWidgetItem("TASK ID"))
                self.related_table.setItem(r, 1, QTableWidgetItem(str(row_ack[0])))
                self.related_table.setItem(r, 2, QTableWidgetItem(""))

            # Check NACK
            cursor.execute("SELECT task_ID, error_code FROM NACK WHERE id = ?", (pkt_id,))
            row_nack = cursor.fetchone()
            if row_nack:
                r = self.related_table.rowCount()
                self.related_table.insertRow(r)
                self.related_table.setItem(r, 0, QTableWidgetItem("TASK ID"))
                self.related_table.setItem(r, 1, QTableWidgetItem(str(row_nack[0])))
                self.related_table.setItem(r, 2, QTableWidgetItem(""))
                r = self.related_table.rowCount()
                self.related_table.insertRow(r)
                self.related_table.setItem(r, 0, QTableWidgetItem("ERROR CODE"))
                self.related_table.setItem(r, 1, QTableWidgetItem(str(row_nack[1])))
                self.related_table.setItem(r, 2, QTableWidgetItem(""))

            conn.close()

        # Function to delete selected packets from the database
        def delete_selected_packet(self):
            # Temporarily enable multi-selection for deletion
            self.left_top_table.setSelectionMode(QAbstractItemView.MultiSelection)

            # Hide EXPORT TO EXCEL during multi-selection
            self.export_btn.setVisible(False)

            # Remove if already present
            btn_row = self.right_top_group.layout().itemAt(1)
            if btn_row and isinstance(btn_row, QHBoxLayout):
                btn_row = btn_row
            else:
                # fallback: search for QHBoxLayout
                for i in range(self.right_top_group.layout().count()):
                    item = self.right_top_group.layout().itemAt(i)
                    if isinstance(item, QHBoxLayout):
                        btn_row = item
                        break
            # Remove buttons/label if present
            if hasattr(self, 'cancel_delete_btn'):
                btn_row.removeWidget(self.cancel_delete_btn)
                self.cancel_delete_btn.deleteLater()
                del self.cancel_delete_btn
            if hasattr(self, 'select_packets_label'):
                btn_row.removeWidget(self.select_packets_label)
                self.select_packets_label.deleteLater()
                del self.select_packets_label
            btn_row.removeWidget(self.delete_btn)

            # Add label to select packets
            self.select_packets_label = QLabel("Selection mode: ")
            self.select_packets_label.setStyleSheet("color: #d35400; font-weight: bold; margin-left: 12px;")
            btn_row.addWidget(self.select_packets_label)

            # Change button to confirm/cancel
            self.delete_btn.setText("CONFIRM DELETE")
            self.delete_btn.clicked.disconnect()
            self.delete_btn.clicked.connect(self.confirm_delete_packets)
            btn_row.addWidget(self.delete_btn)

            # Add CANCEL button
            self.cancel_delete_btn = QPushButton("CANCEL")
            self.cancel_delete_btn.clicked.connect(self.cancel_delete_packets)
            btn_row.addWidget(self.cancel_delete_btn)

        # Function to confirm deletion of selected packets
        def confirm_delete_packets(self):
            selected_rows = self.left_top_table.selectionModel().selectedRows()
            if not selected_rows:
                # Just do nothing if none selected
                return
            pkt_ids = [self.packets[row.row()][0] for row in selected_rows]
            pkt_ids_str = ', '.join(str(pid) for pid in pkt_ids)
            reply = QMessageBox.question(
                self,
                "Confirm",
                f"Sure to delete IDs {pkt_ids_str} from database?",
                QMessageBox.Yes | QMessageBox.No
            )
            if reply == QMessageBox.Yes:
                conn = sqlite3.connect(DB_PATH)
                cursor = conn.cursor()
                # Delete from all related tables for each packet id
                for pid in pkt_ids:
                    cursor.execute("DELETE FROM LORA_PONG WHERE id = ?", (pid,))
                    cursor.execute("DELETE FROM ACK WHERE id = ?", (pid,))
                    cursor.execute("DELETE FROM NACK WHERE id = ?", (pid,))
                cursor.executemany("DELETE FROM packets WHERE id = ?", [(pid,) for pid in pkt_ids])
                conn.commit()
                conn.close()
                self.load_data()

            # Restore EXPORT TO EXCEL after the selection
            self.export_btn.setVisible(True)

            self.left_top_table.setSelectionMode(QAbstractItemView.SingleSelection)

            # Restore DELETE PACKET to btn_row and remove CANCEL/label
            btn_row = self.right_top_group.layout().itemAt(1)
            if btn_row and isinstance(btn_row, QHBoxLayout):
                btn_row = btn_row
            else:
                for i in range(self.right_top_group.layout().count()):
                    item = self.right_top_group.layout().itemAt(i)
                    if isinstance(item, QHBoxLayout):
                        btn_row = item
                        break
            btn_row.removeWidget(self.delete_btn)
            self.delete_btn.setText("DELETE PACKET")
            self.delete_btn.clicked.disconnect()
            self.delete_btn.clicked.connect(self.delete_selected_packet)
            btn_row.addWidget(self.delete_btn)
            if hasattr(self, 'cancel_delete_btn'):
                btn_row.removeWidget(self.cancel_delete_btn)
                self.cancel_delete_btn.deleteLater()
                del self.cancel_delete_btn
            if hasattr(self, 'select_packets_label'):
                btn_row.removeWidget(self.select_packets_label)
                self.select_packets_label.deleteLater()
                del self.select_packets_label

        # Function to cancel deletion of packets
        def cancel_delete_packets(self):
            # Restore EXPORT TO EXCEL after the multi-selection
            self.export_btn.setVisible(True)
            self.left_top_table.setSelectionMode(QAbstractItemView.SingleSelection)

            # Restore DELETE PACKET to btn_row and remove CANCEL/label
            btn_row = self.right_top_group.layout().itemAt(1)
            if btn_row and isinstance(btn_row, QHBoxLayout):
                btn_row = btn_row
            else:
                for i in range(self.right_top_group.layout().count()):
                    item = self.right_top_group.layout().itemAt(i)
                    if isinstance(item, QHBoxLayout):
                        btn_row = item
                        break

            # Button definition for packet elimination
            btn_row.removeWidget(self.delete_btn)
            self.delete_btn.setText("DELETE PACKET")
            self.delete_btn.clicked.disconnect()
            self.delete_btn.clicked.connect(self.delete_selected_packet)
            btn_row.addWidget(self.delete_btn)
            if hasattr(self, 'cancel_delete_btn'):
                btn_row.removeWidget(self.cancel_delete_btn)
                self.cancel_delete_btn.deleteLater()
                del self.cancel_delete_btn
            if hasattr(self, 'select_packets_label'):
                btn_row.removeWidget(self.select_packets_label)
                self.select_packets_label.deleteLater()
                del self.select_packets_label

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
    #     GS_time=datetime.utcnow().timestamp(),  # GS_time
    #     HEX_str=HEX_data,
    #     rssi_str="-70.0",
    #     snr_str="10.1",
    #     deltaf_str="0.0",
    #     comment="Dati telemetria"
    # )
