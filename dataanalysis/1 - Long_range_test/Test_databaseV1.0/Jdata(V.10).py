import sqlite3
from datetime import datetime
import sys
from PyQt5.QtWidgets import *
from PyQt5.QtGui import QPalette, QColor
from PyQt5.QtCore import Qt, QDate
import os

DB_PATH = "./Ground station/database/V1.0/SatelliteDB.db"

# Database initialization and packet definition
def database_initialization(path=DB_PATH):
    """Initialize the SQLite database and create the packets table if it doesn't exist."""
    # If the database exists it will be opened, otherwise it will be created
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

    # Setting window style
    # win.setStyleSheet("""
    #     QWidget {
    #         background-color: #1e1e1e;
    #     }
    #     QLabel {
    #         color: #ff4500;
    #         font-size: 20pt;
    #         font-weight: bold;
    #         padding: 40px;
    #     }
    #     QPushButton {
    #         background-color: #ff4500;
    #         color: white;
    #         border: none;
    #         padding: 8px 24px;
    #         font-size: 12pt;
    #         font-weight: bold;
    #         border-radius: 4px;
    #         margin: 0 10px;
    #     }
    #     QPushButton:hover {
    #         background-color: #e03d00;
    #     }
    # """) # Dark mode
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

    # Setting window style
    # dialog.setStyleSheet("""
    #     QDialog {
    #         background-color: #1e1e1e;
    #     }
    #     QLabel {
    #         color: #ff4500;
    #         font-size: 14pt;
    #         font-weight: bold;
    #         padding: 20px;
    #     }
    #     QPushButton {
    #         background-color: #ff4500;
    #         color: white;
    #         border: none;
    #         padding: 8px 24px;
    #         font-size: 11pt;
    #         font-weight: bold;
    #         border-radius: 4px;
    #         margin: 0 10px;
    #     }
    #     QPushButton:hover {
    #         background-color: #e03d00;
    #     }
    # """) # Dark mode
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
    """Function to manually access the database by inserting the path"""

    # Open a dialog to input the database path
    dialog = QDialog(parent)
    dialog.setWindowTitle("Manual Access")

    # Setting the dialog style
    # dialog.setStyleSheet("""
    #     QDialog {
    #         background-color: #1e1e1e;
    #     }
    #     QLabel {
    #         color: #ff4500;
    #         font-size: 13pt;
    #         font-weight: bold;
    #         padding: 10px;
    #     }
    #     QLineEdit {
    #         background-color: #2e2e2e;
    #         color: white;
    #         border: 1px solid #555;
    #         padding: 6px;
    #         font-size: 11pt;
    #     }
    #     QPushButton {
    #         background-color: #ff4500;
    #         color: white;
    #         border: none;
    #         padding: 8px 24px;
    #         font-size: 11pt;
    #         font-weight: bold;
    #         border-radius: 4px;
    #         margin: 0 10px;
    #     }
    #     QPushButton:hover {
    #         background-color: #e03d00;
    #     }
    # """) # Dark mode
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
def save_packet(conn, timestamp, ground_station_id, status, mac, payload, tec, direction = "Transmitter", rssi = "0", snr = "0", freq_offset = "0", metadata=""):
    """conn, timestamp, ground_station_id, status, mac, payload, tec, direction(F), rssi(F), snr(F), freq_offset(F), metadata [conn is the database definition --> use init_db()]"""
    
    cursor = conn.cursor()
    length = len(payload)

    # # Get data of packet saving(UTC)
    # timestamp = datetime.utcnow().isoformat()

    # Convert timestamp to UTC datatime from UNIX
    dt = datetime.utcfromtimestamp(timestamp)

    # Creation of the packet from raw data
    cursor.execute('''
        INSERT INTO packets (
            timestamp, direction, ground_station_id, tec, rssi, snr, freq_offset,
            status, length, mac, payload, metadata
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    ''', (dt, direction, ground_station_id, tec, rssi, snr, freq_offset, status, length, mac, payload, metadata))
    
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
    print("ID | Timestamp           | Direction | Ground Station ID | TEC   | RSSI  | SNR   | Freq Offset | Status     | Length | MAC               | Metadata")
    print("-" * 120)
    for row in rows:
        print(f"{row[0]:<3} | {row[1]:<20} | {row[2]:<9} | {row[3]:<17} | {row[4]:<5} | {row[5]:<5} | {row[6]:<5} | {row[7]:<12} | {row[8]:<10} | {row[9]:<6} | {row[10]:<17} | {row[11]}")

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

            # Set the dark theme for GUI
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
                    background-color: #e74c3c;
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

            # Getting only the first 10 paylod's digit to visualization
            payload_full = str(pkt[11])
            payload_preview = payload_full[:10] + ("..." if len(payload_full) > 10 else "")

            rows = [
                f"ID: {pkt[0]}    |   Direction: {pkt[2]}    |   GS id: {pkt[3]}",
                f"Timestamp: {pkt[1].replace('T', ' ')}",
                f"TEC: {pkt[4]}    |   RSSI: {pkt[5]}    |   SNR: {pkt[6]}",
                f"Length: {pkt[9]}    |   Status: {pkt[8]}    |   Freq Offset: {pkt[7]}",
                f"MAC: {pkt[10]}",
                f"Payload: {payload_preview}",
                f"Metadata: {pkt[12]}"
            ]

            for i, ((frame, layout), text) in enumerate(zip(self.detail_frames, rows)):
                
                # Clear all before the next interaction
                for j in reversed(range(layout.count())):

                    item = layout.itemAt(j)
                    widget = item.widget()

                    # If the item is a widget delete it
                    if widget is not None:
                        layout.removeWidget(widget)
                        widget.deleteLater()

                    # If the item is a layout delete it and all his sub-items
                    else:
                        sub_layout = item.layout()
                        if sub_layout is not None:

                            # Remove all widgets from the sub-layout
                            for k in reversed(range(sub_layout.count())):
                                sub_item = sub_layout.itemAt(k)
                                sub_widget = sub_item.widget()
                                if sub_widget is not None:
                                    sub_layout.removeWidget(sub_widget)
                                    sub_widget.deleteLater()
                            layout.removeItem(sub_layout)

                # Adding a "Show All" button in the payload line
                if i == 5:
                    hbox = QHBoxLayout()
                    label = QLabel(text)
                    hbox.addWidget(label)
                    if len(payload_full) > 10:
                        btn = QPushButton("Show All")

                        # Setting the button style
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
                        btn.clicked.connect(lambda _, p=payload_full: self.show_full_payload(p))
                        hbox.addWidget(btn)
                    layout.addLayout(hbox)
                else:
                    label = QLabel(text)
                    layout.addWidget(label)

        # Function to show all the paylod
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

    light_palette = QPalette()

    # DARK MODE
    # dark_palette = QPalette()

    # # Color for dark theme of layout
    # dark_palette.setColor(QPalette.Window, QColor(30, 30, 30))              
    # dark_palette.setColor(QPalette.WindowText, Qt.white)                   
    # dark_palette.setColor(QPalette.Base, QColor(45, 45, 45))               
    # dark_palette.setColor(QPalette.AlternateBase, QColor(60, 60, 60))   
    # dark_palette.setColor(QPalette.ToolTipBase, Qt.white)
    # dark_palette.setColor(QPalette.ToolTipText, Qt.white)
    # dark_palette.setColor(QPalette.Text, Qt.white)
    # dark_palette.setColor(QPalette.Button, QColor(60, 60, 60))            
    # dark_palette.setColor(QPalette.ButtonText, QColor(255, 100, 0))       
    # dark_palette.setColor(QPalette.BrightText, Qt.red)                    
    # dark_palette.setColor(QPalette.Highlight, QColor(255, 69, 0))         
    # dark_palette.setColor(QPalette.HighlightedText, Qt.black)

    # # Apply dark theme
    # QApplication.setPalette(dark_palette)

    # White theme
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

    app = QApplication(sys.argv)
    viewer = PacketViewer()

    # Show the windows
    viewer.resize(1300, 700) # Setting window dimensions
    viewer.show()
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
    
    #show_all_packets(conn)

    open_database()