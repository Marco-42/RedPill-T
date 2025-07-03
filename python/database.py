import sqlite3
from datetime import datetime
from pydrive.auth import GoogleAuth
from pydrive.drive import GoogleDrive

DB_PATH = "./python/GS_database.db"

# ========== Database Initialization =====
def init_db(path=DB_PATH):
    conn = sqlite3.connect(path)
    c = conn.cursor()
    
    # Packet structure
    c.execute('''CREATE TABLE IF NOT EXISTS packets (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    timestamp TEXT,
                    packet_type TEXT,
                    payload BLOB
                )''')
    conn.commit()
    return conn

# ========== Packet saving ===============
def save_packet(conn, packet_type, payload):
    c = conn.cursor()
    c.execute("INSERT INTO packets (timestamp, packet_type, payload) VALUES (?, ?, ?)",
              (datetime.utcnow().isoformat(), packet_type, payload))
    conn.commit()

# ========== Show all packets ============
def show_all_packets(conn):
    c = conn.cursor()
    c.execute("SELECT id, timestamp, packet_type, payload FROM packets ORDER BY timestamp")
    rows = c.fetchall()

    print("\n Packets in the database:")
    for row in rows:
        print(f"ID: {row[0]}, Time: {row[1]}, Type: {row[2]}, Payload: {row[3]}")

if __name__ == "__main__":
    conn = init_db()

    # Debug
    uplink_payload = b"Hello Satellite!"
    downlink_payload = b"Reply from Satellite"

    save_packet(conn, "uplink", uplink_payload)
    save_packet(conn, "downlink", downlink_payload)

    # show packets
    show_all_packets(conn)