import struct
import sys
from pyparsing import line
import serial
import serial.tools.list_ports

from datetime import datetime

from PyQt5.QtWidgets import *
from PyQt5.QtGui import QColor
from PyQt5.QtCore import *

import hmac
import hashlib
import traceback


# ========== CONSTANTS AND CONFIGURATION ==========

RX_TIMEOUT = 5  # seconds to wait for a reply after sending a TEC

# Packet configuration
PACKET_HEADER_LENGTH = 12 # 4 bytes for header + 4 bytes for MAC + 4 bytes for timestamp
PACKET_PAYLOAD_MAX = 98  # maximum payload length in bytes

# MAC configuration
SECRET_KEY = 0xA1B2C3D4

# Fixed bytes configuration
BYTE_RS_OFF = 0x55
BYTE_RS_ON = 0xAA
BYTE_TX_OFF = 0x00
BYTE_TX_ON = 0x01
BYTE_TX_NOBEACON = 0x02

# Error codes
PACKET_ERR_NONE = 0
PACKET_ERR_RS = -1
PACKET_ERR_DECODE = -2
PACKET_ERR_LENGTH = -3
PACKET_ERR_MAC = -4
PACKET_ERR_CMD_FULL = -5
PACKET_ERR_CMD_POINTER = -6
PACKET_ERR_CMD_UNKNOWN = -7
PACKET_ERR_CMD_PAYLOAD = -8
PACKET_ERR_CMD_MEMORY = -9

PACKET_ERR_DESCRIPTION = {
	PACKET_ERR_NONE: "Unexpected behavior: no error code provided",
	PACKET_ERR_RS: "Reed-Solomon decoding failed",
	PACKET_ERR_DECODE: "Packet decode error",
	PACKET_ERR_LENGTH: "Invalid packet length",
	PACKET_ERR_MAC: "MAC verification failed",
	PACKET_ERR_CMD_FULL: "TEC buffer full",
	PACKET_ERR_CMD_POINTER: "Command pointer error",
	PACKET_ERR_CMD_UNKNOWN: "Requested command not found",
	PACKET_ERR_CMD_PAYLOAD: "Payload of command is invalid",
	PACKET_ERR_CMD_MEMORY: "Memory allocation error during command execution"
}

# Source codes for transmission
TX_SOURCES = {
	"RedPill": 0x01,
	"None": 0x10,
	"UniPD": 0x11,
	"Mobile": 0x12
}

# TEC_TER_TYPES definition
TEC_TER_TYPES = {
	"[HK] Housekeeping": 0,
	"[DAQ] Data Acquisition": 1,
	"[PE] Payload Execution": 2,
	"[DT] Data Transfer": 3
}

# TEC task codes definitions
TEC_OBC_REBOOT = 0x01 # reboot OBC command
TEC_EXIT_STATE = 0x02 # exit state command
TEC_VAR_CHANGE = 0x03 # variable change command
TEC_SET_TIME = 0x04 # set time command
TEC_EPS_REBOOT = 0x08 # reboot EPS command
TEC_ADCS_REBOOT = 0x10 # reboot ADCS command
TEC_ADCS_TLE = 0x11 # send TLE to ADCS command
TEC_LORA_STATE = 0x18 # send LoRa state command
TEC_LORA_CONFIG = 0x19 # send LoRa configuration command
TEC_LORA_PING = 0x1A # send LoRa ping status command

TEC_CRY_EXP = 0x80 # execute crystals experiment command

# TEC_TASKS labels
TEC_TASKS = {
	# HK
	"TLE update": TEC_ADCS_TLE,
	"Set clock": TEC_SET_TIME,
	"LoRa ping": TEC_LORA_PING,
	"LoRa state": TEC_LORA_STATE,
	"LoRa config": TEC_LORA_CONFIG,
	"Exit state": TEC_EXIT_STATE,
	"Variable change": TEC_VAR_CHANGE,
	"OBC reboot": TEC_OBC_REBOOT,
	"EPS reboot": TEC_EPS_REBOOT,
	"ADCS reboot": TEC_ADCS_REBOOT,
	# DAQ
	"Start DAQ": 68,
	"Stop DAQ": 69,
	# PE
	"Crystals experiment": TEC_CRY_EXP,
	"Execute B": 133,
	# DT
	"Picture download": 193,
	"Download": 194
}

# TER task codes definitions
TER_BEACON = 0x30 # telemetry beacon reply
TER_ACK = 0x31 # ACK reply
TER_NACK = 0x32 # NACK reply
TER_LORA_PING = 0x33 # LoRa ping state reply

# TER_TASKS labels
TER_TASKS = {
	# HK
	"Beacon": TER_BEACON,
	"ACK": TER_ACK,
	"NACK": TER_NACK,
	"LoRa ping": TER_LORA_PING
}

# Input form configs (label, widget_type, optional widget args)
INPUT_FIELDS_CONFIG = {
	"OBC reboot": [
		("TEXT", QLabel, "No configuration available, execution is immediate", None, None, None)
	],
	"Exit state": [
		# ("Delay:", QSpinBox, 0, 0, 100, "[s]"),
		("From state:", QSpinBox, 0, 0, 14, "0-14"),
		("To state:", QSpinBox, 1, 0, 14, "0-14"),
	],
	"Variable change": [
		("Address:", QSpinBox, 0, 0, 254, "0-254"),
		("Value:", QSpinBox, 0, 0, 254, "0-254"),
	],
	"Set clock": [
		("Date and Time:", QDateTimeEdit, QDateTime.currentDateTime(), None, None, None)
	],
	"EPS reboot": [
		("TEXT", QLabel, "No configuration available, execution is immediate", None, None, None)
	],
	"ADCS reboot": [
		("TEXT", QLabel, "No configuration available, execution is immediate", None, None, None)
	],
	"TLE update": [
		("TLE Data:", QPlainTextEdit, "Line 1\nLine 2", None, None, None)
	],
	"LoRa state": [
		("TX State:", QComboBox, "On", ["On", "Beacon off", "Off"], None, None),
		("TEXT", QLabel, "Enter time to keep state for, 0 is forever:", None, None, None),
		("Days:", QSpinBox, 0, 0, 193, ""),
		("Hours:", QSpinBox, 0, 0, 23, ""),
		("Minutes:", QSpinBox, 0, 0, 59, ""),
		("Seconds:", QSpinBox, 0, 0, 59, ""),
		("TEXT", QLabel, "Reverts to TX State: On after time is elapsed", None, None, None),
	],
	"LoRa config": [
		("Frequency:", QDoubleSpinBox, 436.0, 400.0, 500.0, "MHz"),
		("Bandwidth:", QComboBox, "125 kHz", ["62.5 kHz", "125 kHz", "250 kHz", "500 kHz"], None, None),
		("SF:", QSpinBox, 10, 6, 12, None),
		("CR:", QSpinBox, 5, 5, 8, None),
		("Power:", QSpinBox, 10, -9, 22, "dBm"), #TODO: check power range mapping
		("TEXT", QLabel, "Enter time to keep state for:", None, None, None),
		("Seconds:", QSpinBox, 0, 1, 255, ""),
	],
	"LoRa ping": [
		("TEXT", QLabel, "No configuration available, execution is immediate", None, None, None)
	],
	"Crystals experiment": [
		("Glass:", QComboBox, "Off", ["Off", "Dark", "Light"], None, None),
		("TEXT", QLabel, "Enter time to delay Crystals activation since packet RX", None, None, None),
		("Activation delay:", QTimeEdit, QTime(0, 0, 0), 0, 0, "hh:mm:ss"),
		("Photodiode:", QComboBox, "No", ["Yes", "No"], None, None),
		("Picture:", QComboBox, "No", ["Yes", "No"], None, None),
		("TEXT", QLabel, "Enter time to delay observations since Crystals activation", None, None, None),
		("Observation delay:", QTimeEdit, QTime(0, 0, 0), 0, 0, "hh:mm:ss"),
	],
	"Picture settings": [
		("TEXT", QLabel, "TODO", None, None, None)
	],
}

# ============= GUI HELPER FUNCTIONS =============

# Compute HMAC-SHA256 and return first 4 bytes as uint32
def hmac_mac(key_int, message: bytes) -> int:
	"""Compute HMAC-SHA256 and return first 4 bytes as uint32: key_int - message"""
	# Convert 32-bit integer key to bytes
	key_bytes = key_int.to_bytes(4, byteorder='big')

	# Compute HMAC-SHA256 and return first 4 bytes as uint32
	mac = hmac.new(key_bytes, message, hashlib.sha256).digest()
	return int.from_bytes(mac[:4], byteorder='big')

# ============= PACKET FUNCTIONS =================

# Build the payload based on type index and task name
def build_payload(tec_code, input_widgets):
	"""Build the payload based on TEC code and input widgets dictionary: tec_code - input_widgets"""

	if tec_code == TEC_OBC_REBOOT:
		payload = b''

	elif tec_code == TEC_EXIT_STATE:
		from_state = input_widgets["From state:"].value() & 0x0F
		to_state = input_widgets["To state:"].value() & 0x0F
		byte_val = (from_state << 4) | to_state
		payload = bytes([byte_val])

	elif tec_code == TEC_VAR_CHANGE:
		address = input_widgets["Address:"].value() & 0xFF
		value = input_widgets["Value:"].value() & 0xFF
		payload = bytes([address, value])

	elif tec_code == TEC_SET_TIME:
		dt_widget = input_widgets["Date and Time:"]
		qdt = dt_widget.dateTime()  # QDateTime object
		unix_time = int(qdt.toSecsSinceEpoch())  # convert to UNIX timestamp (int)
		payload = unix_time.to_bytes(4, byteorder='big')
	
	elif tec_code == TEC_EPS_REBOOT:
		payload = b''

	elif tec_code == TEC_ADCS_REBOOT:
		payload = b''

	elif tec_code == TEC_ADCS_TLE:
		tle_data = input_widgets["TLE Data:"].toPlainText().strip()
		tle_lines = tle_data.splitlines()
		if len(tle_lines) < 2:
			raise ValueError("TLE Data must contain two lines")

		tle_line1 = tle_lines[0]
		tle_line2 = tle_lines[1]

		# Validate TLE lengths
		if len(tle_line1) != 69 or len(tle_line2) != 69:
			raise ValueError("TLE lines must be 69 characters long")

		try:
			epoch_year = int(tle_line1[18:20])
			epoch_year += 2000 if epoch_year < 57 else 1900
			epoch_day = float(tle_line1[20:32])
			mm_dot = float(tle_line1[33:43])
			mm_ddot = float(f"{tle_line1[44:50].strip()}e{tle_line1[50:52]}")
			bstar = float(f"{tle_line1[53:59].strip()}e{tle_line1[59:61]}")
			inclination = float(tle_line2[8:16])
			raan = float(tle_line2[17:25])
			eccentricity = float(f"0.{tle_line2[26:33].strip()}")
			arg_perigee = float(tle_line2[34:42])
			mean_anomaly = float(tle_line2[43:51])
			mean_motion = float(tle_line2[52:63])
			rev_number = int(tle_line2[63:68])
		except Exception as e:
			raise ValueError(f"Invalid TLE format: {e}")

		# Pack into 43 bytes, big-endian
		payload = struct.pack(
			">HffffffffffI",  # > = big endian
			epoch_year,
			epoch_day,
			mm_dot,
			mm_ddot,
			bstar,
			inclination,
			raan,
			eccentricity,
			arg_perigee,
			mean_anomaly,
			mean_motion,
			rev_number
		)

	elif tec_code == TEC_LORA_STATE:
		# TX State (1 byte)
		tx_state = input_widgets["TX State:"].currentText()
		if tx_state == "On":
			tx_state_val = BYTE_TX_ON & 0b1111
		elif tx_state == "Beacon off":
			tx_state_val = BYTE_TX_NOBEACON & 0b1111
		elif tx_state == "Off":
			tx_state_val = BYTE_TX_OFF & 0b11
		else:
			raise ValueError(f"Invalid TX State: {tx_state}")
		tx_state_byte = (tx_state_val << 4) |  tx_state_val

		# Duration (3 bytes)
		days = input_widgets["Days:"].value()
		hours = input_widgets["Hours:"].value()
		minutes = input_widgets["Minutes:"].value()
		seconds = input_widgets["Seconds:"].value()
		duration_total = (days * 86400) + (hours * 3600) + (minutes * 60) + seconds
		duration_bytes = duration_total.to_bytes(3, 'big')  # 3 bytes

		# Combine all into one payload
		payload = bytes([tx_state_byte]) + duration_bytes

	elif tec_code == TEC_LORA_CONFIG:
		frequency_khz = int(input_widgets["Frequency:"].value() * 1000)
		frequency_bytes = frequency_khz.to_bytes(3, 'big')

		bandwidth = input_widgets["Bandwidth:"].currentText()
		if bandwidth.endswith(" kHz"):
			bandwidth = bandwidth[:-4]
		if bandwidth == "62.5":
			bandwidth_bits = 0b00
		elif bandwidth == "125":
			bandwidth_bits = 0b01
		elif bandwidth == "250":
			bandwidth_bits = 0b10
		elif bandwidth == "500":
			bandwidth_bits = 0b11
		else:
			raise ValueError(f"Invalid bandwidth value: {bandwidth}")
		
		sf = input_widgets["SF:"].value()
		sf_bits = (sf - 6) & 0b111  # SF 6-12 maps to bits 0-6, 3 bits total

		cr = input_widgets["CR:"].value()
		cr_bits = (cr - 5) & 0b111  # CR 5-8 maps to bits 0-3, 3 bits total

		sf_cr_byte = bytes([(bandwidth_bits << 6) | (sf_bits << 3) | cr_bits])

		power = input_widgets["Power:"].value()
		power_bits = (power + 9) & 0b11111 # Power -9 to +22 maps to bits 0-31, 5 bits total

		reserved_bits = 0b000
		power_byte = bytes([(power_bits << 3) | reserved_bits])

		duration = input_widgets["Seconds:"].value()
		duration_byte = bytes([duration & 0xFF])  # 1 byte for duration

		payload = frequency_bytes + sf_cr_byte + power_byte + duration_byte

	elif tec_code == TEC_LORA_PING:
		payload = b''

	elif tec_code == TEC_CRY_EXP:
		# Glass state: Off, Dark, Light (3 bits)
		glass_state = input_widgets["Glass:"].currentText()
		if glass_state == "Off":
			glass_val = 0b000
		elif glass_state == "Dark":
			glass_val = 0b001
		elif glass_state == "Light":
			glass_val = 0b010
		else:
			raise ValueError(f"Invalid Glass state: {glass_state}")

		# Repeat glass_val twice (6 bits total)
		glass_bits = (glass_val << 3) | glass_val

		activation_time = input_widgets["Activation delay:"].time()
		activation_secs = (activation_time.hour() * 3600 + activation_time.minute() * 60 + activation_time.second())

		# Combine: 6 bits glass_bits + 18 bits activation_secs
		glass_and_delay = (glass_bits << 18) | activation_secs
		glass_and_delay_bytes = glass_and_delay.to_bytes(3, 'big')

		# Photodiode: Yes/No (3 bits for future expansion)
		photodiode = input_widgets["Photodiode:"].currentText()
		if photodiode == "No":
			diode_val = 0b000
		elif photodiode == "Yes":
			diode_val = 0b001
		else:
			raise ValueError(f"Invalid Photodiode state: {photodiode}")

		# Picture: Yes/No (3 bits for future expansion)
		picture = input_widgets["Picture:"].currentText()
		if picture == "No":
			picture_val = 0b000
		elif picture == "Yes":
			picture_val = 0b001
		else:
			raise ValueError(f"Invalid Picture state: {picture}")

		# Combine: 3 bits diode + 3 bits picture
		state_bits = (diode_val << 3) | picture_val

		observation_time = input_widgets["Observation delay:"].time()
		observation_secs = (observation_time.hour() * 3600 + observation_time.minute() * 60 + observation_time.second())

		# Combine: 6 bits state_bits + 18 bits observation_secs
		state_and_delay = (state_bits << 18) | observation_secs
		state_and_delay_bytes = state_and_delay.to_bytes(3, 'big')

		# Final payload: 6 bytes
		payload = glass_and_delay_bytes + state_and_delay_bytes

	else:
		payload = b''

	return payload

# Build the full packet with header, payload, and ECC(for transmission)
def build_packet(gs_text, tec_code, payload_bytes, ecc_enabled):
	"""Build the full packet with header, payload, and ECC: gs_text - tec_code - payload_bytes - ecc_enabled"""
	
	# === Byte 0: Station ID ===
	if gs_text in TX_SOURCES:
		station = TX_SOURCES[gs_text]
	else:
		raise ValueError(f"Invalid Ground Station: {gs_text}")

	# === Byte 1: ECC Flag ===
	ecc = BYTE_RS_ON if ecc_enabled else BYTE_RS_OFF

	# === Byte 2: TEC
	tec = tec_code

	# === Byte 3: Payload length ===
	payload_length = len(payload_bytes)
	if payload_length > PACKET_PAYLOAD_MAX:
		raise ValueError(f"Payload too long (max {PACKET_PAYLOAD_MAX} bytes)")

	# === Byte 4-7: UNIX timestamp ===
	unix_time = int(datetime.now().timestamp())
	bytes_time = unix_time.to_bytes(4, byteorder='big')

	# === Byte 8-11: MAC ===
	mac_placeholder = b'\x00\x00\x00\x00'
	header_partial = bytes([
		station,
		ecc,
		tec,
		payload_length
	])
	packet_placeholder = header_partial + bytes_time + mac_placeholder + payload_bytes

	# Compute MAC over the whole packet
	mac = hmac_mac(SECRET_KEY, packet_placeholder)
	bytes_mac = mac.to_bytes(4, byteorder='big')

	# Build final packet
	return header_partial + bytes_time + bytes_mac + payload_bytes

# Function to decode the packet
def decode_packet(packet_bytes):
	"""Decode the packet and return a dictionary with fields: packet_bytes"""

	if len(packet_bytes) < PACKET_HEADER_LENGTH:
		raise ValueError(f"Packet too short to decode: length {len(packet_bytes)} < {PACKET_HEADER_LENGTH}")

	# Byte 0: Station ID (raw int)
	station_id = packet_bytes[0]

	# Byte 1: ECC Flag (bool)
	byte_ecc = packet_bytes[1]
	if byte_ecc == BYTE_RS_ON:
		ecc_enabled = True
	elif byte_ecc == BYTE_RS_OFF:
		ecc_enabled = False
	else:
		raise ValueError(f"Invalid ECC flag: 0x{byte_ecc:02X}")

	# Byte 2: TER (raw int)
	ter = packet_bytes[2]

	# Byte 3: Payload length
	payload_length = packet_bytes[3]

	# Ensure entire packet is present
	expected_len = PACKET_HEADER_LENGTH + payload_length
	if len(packet_bytes) < expected_len:
		raise ValueError(f"Packet too short for payload: length {len(packet_bytes)} < expected {expected_len}")

	# Bytes 4-7: Timestamp (int)
	bytes_time = packet_bytes[4:8]
	timestamp = int.from_bytes(bytes_time, byteorder='big')

	# Bytes 8–11: MAC (int)
	bytes_mac = packet_bytes[8:12]
	mac = int.from_bytes(bytes_mac, byteorder='big')

	# Payload: list of ints
	if payload_length > 0:
		payload_bytes = list(packet_bytes[12:12 + payload_length])
	else:
		payload_bytes = []

	return {
		"station_id": station_id,
		"ecc_enabled": ecc_enabled,
		"ter": ter,
		"payload_length": payload_length,
		"mac": mac,
		"timestamp": timestamp,
		"payload_bytes": payload_bytes  # always a list of ints
	}

# ============= CONVERSION FUNCTIONS ============= 

# Functions to get labels of TER and TEC codes
def get_ter_tec_label(code):
	"""Get the label for a given TER or TEC code"""

	# search in TER tasks first
	for label, val in TER_TASKS.items():
		if int(val) == code:
			return label
		
	# then search in TEC tasks
	for label, val in TEC_TASKS.items():
		if int(val) == code:
			return label
	return f"Unknown ({code})"

# Functions to get IDs of TER and TEC labels
def get_ter_tec_id(tec_label):
	"""Get the ID for a given TEC or TER label"""

	# search in TER tasks first
	for label, val in TER_TASKS.items():
		if label == tec_label:
			return int(val)
	
	# then search in TEC tasks
	for label, val in TEC_TASKS.items():
		if label == tec_label:
			return int(val)

	return f"Unknown ({tec_label})"

# Functions to get Ground Station labels and IDs
def get_gs_label(station_id):
	"""Get the Ground Station label for a given station ID: station_id"""

	for label, val in TX_SOURCES.items():
		if int(val) == station_id:
			return label
	return f"Unknown ({station_id})"

# Functions to get Ground Station IDs
def get_gs_id(gs_label):

	"""Get the Ground Station ID for a given station label: gs_label"""
	for label, val in TX_SOURCES.items():
		if label == gs_label:
			return int(val)
	return f"Unknown ({gs_label})"

# ============= DATA EXTRACTION FROM HEX =============
# Function for data extraction from HEX to save specific fields in the database

# HOUSEKEEPING TER
# LORA PONG
def extract_lora_pong(payload_bytes):
	"""Extract RSSI, SNR, and frequency shift from LORA_PONG payload: payload_bytes"""

	rssi_bytes = bytes(payload_bytes[0:4])
	rssi = struct.unpack(">f", rssi_bytes)[0]
	snr_bytes = bytes(payload_bytes[4:8])
	snr = struct.unpack(">f", snr_bytes)[0]
	freq_shift_bytes = bytes(payload_bytes[8:12])
	freq_shift = struct.unpack(">f", freq_shift_bytes)[0]

	return {
		"rssi": rssi,
		"snr": snr,
		"deltaf": freq_shift
	}

# NACK
def extract_nack(payload_bytes):
	"""Extract task_ID and error_code from NACK payload: payload_bytes"""

	task_ID = payload_bytes[0]
	error_code = int.from_bytes([payload_bytes[1]], byteorder='big', signed=True)

	return {
		"task_ID": task_ID,
		"error_code": error_code
	}

# ACK
def extract_ack(payload_bytes):
	"""Extract task_ID from ACK payload: payload_bytes"""

	task_ID = payload_bytes[0]

	return {
		"task_ID": task_ID
	}

# HOUSEKEEPING TEC
# LORA STATE
def extract_lora_state(payload_bytes):
	"""Extract TX state and duration from LORA_STATE payload: payload_bytes"""

	tx_state = bytes(payload_bytes[0])[0]
	duration_bytes = bytes(payload_bytes[1:4])
	duration = int.from_bytes(duration_bytes, byteorder='big')

	return {
		"tx_state": tx_state,
		"duration": duration
	}

# LORA CONFIG
def extract_lora_config(payload_bytes):
	"""Extract frequency, bandwidth, SF, CR, power, and duration from LORA_CONFIG payload: payload_bytes"""

	frequency_bytes = bytes(payload_bytes[0:3])
	frequency_mhz = int.from_bytes(frequency_bytes, byteorder='big')/1000.0

	sf_cr_byte = bytes(payload_bytes[3])[0]
	bandwidth_bits = (sf_cr_byte >> 6) & 0b11
	if bandwidth_bits == 0b00:
		bandwidth = 62.5
	elif bandwidth_bits == 0b01:
		bandwidth = 125
	elif bandwidth_bits == 0b10:
		bandwidth = 250
	elif bandwidth_bits == 0b11:
		bandwidth = 500
	else:
		bandwidth = None

	sf_bits = (sf_cr_byte >> 3) & 0b111
	sf = sf_bits + 5

	cr_bits = sf_cr_byte & 0b111
	# Mappatura CR: 0b000 -> CR4/5, 0b001 -> CR4/6, 0b010 -> CR4/7, 0b011 -> CR4/8
	if cr_bits == 0b000:
		cr = "CR4/5"
	elif cr_bits == 0b001:
		cr = "CR4/6"
	elif cr_bits == 0b010:
		cr = "CR4/7"
	elif cr_bits == 0b011:
		cr = "CR4/8"
	else:
		cr = None  # Invalid CR bits

	power_byte = bytes(payload_bytes[4])[0]
	power_bits = (power_byte >> 3) & 0b11111
	TX_power = power_bits - 9

	duration_byte = bytes([payload_bytes[5]])
	duration = int.from_bytes(duration_byte, byteorder='big')

	return {
		"frequency_mhz": frequency_mhz,
		"bandwidth_khz": bandwidth,
		"sf": sf,
		"cr": cr,
		"TX_power_dbm": TX_power,
		"duration": duration
	}