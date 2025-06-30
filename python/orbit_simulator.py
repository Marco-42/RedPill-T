import struct
import sys
import serial
import serial.tools.list_ports
from datetime import datetime
from PyQt5.QtWidgets import *
from PyQt5.QtGui import QColor, QPalette, QTextCursor
from PyQt5.QtCore import QTimer
import hashlib
import numpy as np
from sgp4.api import Satrec, jday
from datetime import datetime, timedelta
import matplotlib.pyplot as plt
import cartopy.crs as ccrs
import cartopy.feature as cfeature
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
from PyQt5.QtGui import QPalette, QColor
from PyQt5.QtCore import Qt

line1 = "1 25544U 98067A   20300.83097691  .00001534  00000-0  35580-4 0  9996"
line2 = "2 25544  51.6453  57.0843 0001671  64.9808  73.0513 15.49338189252428"

R_EARTH = 6371.0  # Earth radius in kilometers
MINUTES = 720     # Number of minutes to simulate

# Ground station parameters
gs_lons = 11.893123
gs_lats = 45.410935
gs_range = 1000 # Ground station range in Km
gs_altitude = 0.012 # Ground station altitude in km 

# Defining vector for time contact
contact_time = []
end_contact_time = []

# Check flag for first simulation
simulation_flag = True

# Function to calculate the elevation angle of a satellite from a ground station
def elevation_angle(sat_ecef, gs_lat, gs_lon, gs_alt):
    """Calculate the elevation angle of a satellite from a ground station."""

    # Convert ground station to ECEF
    gs_ecef = latlonalt_to_ecef(gs_lat, gs_lon, gs_alt)

    # Distance vector from ground station to satellite in ECEF coordinates
    rho = sat_ecef - gs_ecef

    # Coordinates in radians
    lat_rad = np.radians(gs_lat)
    lon_rad = np.radians(gs_lon)

    # From ECEF to ENU
    R = np.array([
        [-np.sin(lon_rad),               np.cos(lon_rad),              0],
        [-np.sin(lat_rad)*np.cos(lon_rad), -np.sin(lat_rad)*np.sin(lon_rad), np.cos(lat_rad)],
        [ np.cos(lat_rad)*np.cos(lon_rad),  np.cos(lat_rad)*np.sin(lon_rad), np.sin(lat_rad)]
    ])

    enu = R @ rho  # ENU coordinates vector
    east, north, up = enu

    # Elevation angle in radians
    elev_rad = np.arcsin(up / np.linalg.norm(enu))
    return np.degrees(elev_rad)

# Function to convert Julian date to Greenwich Mean Sidereal Time (GMST)
def gmst_from_jd(jd):
    """Convert Julian date to Greenwich Mean Sidereal Time (GMST) in radians - jd"""

    T = (jd - 2451545.0) / 36525.0
    GMST = 280.46061837 + 360.98564736629 * (jd - 2451545) + \
        0.000387933 * T**2 - (T**3) / 38710000.0
    GMST = GMST % 360.0
    return np.radians(GMST)

# Function to convert ECI coordinates to latitude and longitude
def eci_to_latlon(r_eci, gmst):
    """Convert ECI coordinates to latitude and longitude - ECI - gmst"""

    x_eci, y_eci, z_eci = r_eci
    x_ecef = x_eci * np.cos(gmst) + y_eci * np.sin(gmst)
    y_ecef = -x_eci * np.sin(gmst) + y_eci * np.cos(gmst)
    z_ecef = z_eci

    r = np.sqrt(x_ecef**2 + y_ecef**2 + z_ecef**2)
    lon = np.arctan2(y_ecef, x_ecef)
    lat = np.arcsin(z_ecef / r)

    lat_deg = np.degrees(lat)
    lon_deg = np.degrees(lon)
    return lat_deg, lon_deg

# Function to convert latitude, longitude (in degrees), and altitude (in km) to ECEF coordinates (km)
def latlonalt_to_ecef(lat, lon, alt):
    """Convert latitude, longitude (in degrees), and altitude (in km) to ECEF coordinates (km) - lat, lon, alt"""

    R_EARTH = 6378.137  # raggio equatoriale in km
    e2 = 6.69437999014e-3  # earth squared eccentricity WGS84

    lat_rad = np.radians(lat)
    lon_rad = np.radians(lon)

    N = R_EARTH / np.sqrt(1 - e2 * np.sin(lat_rad)**2) 

    x = (N + alt) * np.cos(lat_rad) * np.cos(lon_rad)
    y = (N + alt) * np.cos(lat_rad) * np.sin(lon_rad)
    z = (N * (1 - e2) + alt) * np.sin(lat_rad)

    return np.array([x, y, z])

# Function to calculate the 3D distance between two points given their latitude, longitude, and altitude
def distance_3d(lat1, lon1, alt1, lat2, lon2, alt2):
    """Calculate the 3D distance between two points given their latitude, longitude, and altitude - lat1, lon1, alt1, lat2, lon2, alt2"""

    p1 = latlonalt_to_ecef(lat1, lon1, alt1)
    p2 = latlonalt_to_ecef(lat2, lon2, alt2)
    distanza = np.linalg.norm(p1 - p2)
    return distanza


class SatelliteSimApp(QMainWindow):
    def __init__(self):
        super().__init__()

        # Foglio di stile per rifinire i pulsanti e input
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

        self.setWindowTitle("Satellite Simulation GUI")
        self.setGeometry(100, 100, 1000, 600)  # Dimensioni iniziali

        self.main_widget = QWidget()
        self.setCentralWidget(self.main_widget)

        # Layout principale orizzontale
        layout = QHBoxLayout(self.main_widget)

        # Pannello controlli (a sinistra)
        control_layout = QVBoxLayout()

        # ComboBox preset coordinate
        control_layout.addWidget(QLabel("Preset Coordinate:"))
        self.coord_combo = QComboBox()
        self.coord_combo.addItem("Saved")
        self.coord_combo.addItem("Padua GS")
        control_layout.addWidget(self.coord_combo)

        # Latitude input
        control_layout.addWidget(QLabel("Ground Station Latitude (deg):"))
        self.lat_input = QLineEdit("")
        control_layout.addWidget(self.lat_input)

        # Longitude input
        control_layout.addWidget(QLabel("Ground Station Longitude (deg):"))
        self.lon_input = QLineEdit("")
        control_layout.addWidget(self.lon_input)

        # Minimum elevation angle input
        control_layout.addWidget(QLabel("Minimum Elevation Angle (deg):"))
        self.elev_input = QLineEdit("10")
        control_layout.addWidget(self.elev_input)
        
        # Selettore di proiezione
        control_layout.addWidget(QLabel("Map Projection:"))

        self.btn_platecarree = QPushButton("PlateCarree")
        self.btn_orthographic = QPushButton("Orthographic")
        self.current_projection = "PlateCarree"

        simulation_type = "orbit"  # Default simulation type 
        self.btn_platecarree.clicked.connect(lambda: self.set_projection("PlateCarree", simulation_type))
        self.btn_orthographic.clicked.connect(lambda: self.set_projection("Orthographic", simulation_type))

        control_layout.addWidget(self.btn_platecarree)
        control_layout.addWidget(self.btn_orthographic)

        self.loading_label = QLabel("Loading...")
        self.loading_label.setStyleSheet("color: orange; font-size: 16px;")
        self.loading_label.setAlignment(Qt.AlignCenter)
        self.loading_label.setVisible(False)  # nascosta all'inizio
        control_layout.addWidget(self.loading_label)
        
        # Plotting bar
        control_layout.addWidget(QLabel("Plot option:"))
        self.btn_only_sat = QPushButton("Satellite position zero")
        self.btn_only_gs = QPushButton("GS position")
        self.btn_orbit = QPushButton("Orbit(12 hours)")

        self.btn_only_sat.clicked.connect(lambda: self.run_simulation("only_sat"))
        self.btn_only_gs.clicked.connect(lambda: self.run_simulation("only_gs"))
        self.btn_orbit.clicked.connect(lambda: self.run_simulation("orbit"))

        control_layout.addWidget(self.btn_only_sat)
        control_layout.addWidget(self.btn_only_gs)
        control_layout.addWidget(self.btn_orbit)

        control_layout.addStretch()
        layout.addLayout(control_layout, 1)  # proporzione 1

        # Pannello grafico (a destra)
        self.figure = Figure()
        self.canvas = FigureCanvas(self.figure)
        self.figure.patch.set_facecolor("#1e1e1e")  # Sfondo della figura
        self.canvas.setStyleSheet("background-color: #1e1e1e;")  # Sfondo del canvas Qt
        layout.addWidget(self.canvas, 4)  # proporzione 4 (più grande)

        # Collegamenti segnali
        self.coord_combo.currentIndexChanged.connect(self.on_preset_changed)

        # Pannello grafico + timer
        plot_layout = QVBoxLayout()
        plot_layout.addWidget(self.canvas)

        # Etichetta "Current time"
        self.clock_title = QLabel("Current time")
        self.clock_title.setAlignment(Qt.AlignCenter)
        self.clock_title.setStyleSheet("""
            QLabel {
                color: #FF6A00;
                font-family: 'Segoe UI', sans-serif;
                font-size: 14px;
                margin-bottom: 4px;
            }
        """)

        # Time label
        self.clock_label = QLabel("""
            <div style='text-align: center;'>
                <div style='font-size: 16px; font-family: Segoe UI; color: #FF6A00;'>CURRENT TIME</div>
                <div style='font-size: 24px; font-family: Courier New; color: #FF6A00;'>00:00:00 UTC</div>
            </div>
        """)
        self.clock_label.setAlignment(Qt.AlignCenter)
        self.clock_label.setStyleSheet("""
            QLabel {
                color: #FF6A00;
                font-family: 'Courier New', monospace;
                font-size: 20px;
                border: 2px solid #FF6A00;
                background-color: #1e1e1e;
                padding: 10px 16px;
                border-radius: 6px;
                max-width: 200px;
            }
        """)

        # Countdown label (Next Contact)
        self.next_contact_label = QLabel("""
            <div style='text-align: center;'>
                <div style='font-size: 16px; font-family: Segoe UI; color: #00BFFF;'>NEXT CONTACT</div>
                <div style='font-size: 24px; font-family: Courier New; color: #00BFFF;'>--:--:--</div>
            </div>
        """)
        self.next_contact_label.setAlignment(Qt.AlignCenter)
        self.next_contact_label.setStyleSheet("""
            QLabel {
                color: #00BFFF;
                font-family: 'Courier New', monospace;
                font-size: 20px;
                border: 2px solid #00BFFF;
                background-color: #1e1e1e;
                padding: 10px 16px;
                border-radius: 6px;
                max-width: 200px;
            }
        """)

        time_layout = QHBoxLayout()
        time_layout.addWidget(self.clock_label, alignment=Qt.AlignLeft)
        time_layout.addWidget(self.next_contact_label)
        plot_layout.addLayout(time_layout)

        # Aggiungi al layout principale
        layout.addLayout(plot_layout, 4)

        self.clock_timer = QTimer()
        self.clock_timer.timeout.connect(self.update_clock)
        self.clock_timer.start(1000)
        self.update_clock()

        # Error text
        self.error_label = QLabel("")
        self.error_label.setAlignment(Qt.AlignCenter)
        self.error_label.setStyleSheet("""
            color: red;
            font-size: 16px;
            background-color: #1e1e1e;
            padding: 10px;
        """)
        plot_layout.addWidget(self.error_label)
        
        # Hide the error label initially
        self.error_label.setVisible(False)

    # Function to show the error message
    def show_error(self, message):
        self.figure.clear()
        self.canvas.draw()
        self.error_label.setText(message)
        self.error_label.setVisible(True)
        
    def update_clock(self):
        global simulation_flag
        now = datetime.utcnow()
        current_time_str = now.strftime("%H:%M:%S UTC")

        self.clock_label.setText(f"""
            <div style='text-align: center;'>
                <div style='font-size: 14px; font-family: Segoe UI; color: #FF6A00;'>CURRENT TIME</div>
                <div style='font-size: 24px; font-family: Courier New; color: #FF6A00;'>{current_time_str}</div>
            </div>
        """)

        # Calcolo tempo al prossimo contatto
        if simulation_flag == False:
            next_contact = contact_time[0]
        else: 
            next_contact = 0

        if next_contact != 0:
            delta = next_contact - now
            h, remainder = divmod(int(delta.total_seconds()), 3600)
            m, s = divmod(remainder, 60)
            countdown_str = f"{h:02}:{m:02}:{s:02}"
        else:
            countdown_str = "--:--:--"

        self.next_contact_label.setText(f"""
            <div style='text-align: center;'>
                <div style='font-size: 14px; font-family: Segoe UI; color: #00BFFF;'>NEXT CONTACT</div>
                <div style='font-size: 24px; font-family: Courier New; color: #00BFFF;'>{countdown_str}</div>
            </div>
        """)

    def run_simulation(self, simulation_type):
        self.loading_label.setVisible(True)  # Mostra "Loading..."
        QTimer.singleShot(100, lambda: self._execute_simulation(simulation_type))  # Avvia dopo 100 ms

    def set_projection(self, projection_name, simulation_type):
        self.current_projection = projection_name
        self.run_simulation(simulation_type)  # Ricalcola la simulazione con la nuova proiezione

    def on_preset_changed(self, index):
        preset = self.coord_combo.currentText()
        presets = {
            "Padua GS": (gs_lats, gs_lons),
        }

        if preset == "Personalizzato":
            self.lat_input.setText("")
            self.lon_input.setText("")
        else:
            lat, lon = presets.get(preset, ("", ""))
            self.lat_input.setText(str(lat))
            self.lon_input.setText(str(lon))
        
    def _execute_simulation(self, simulation_type):
        lat_text = self.lat_input.text().strip()
        lon_text = self.lon_input.text().strip()
        elev_text = self.elev_input.text().strip()

        if not lat_text or not lon_text or not elev_text:
            self.show_error("Insert value for all fields[GS]")
            return

        try:
            lat = float(lat_text)
            lon = float(lon_text)
            min_elev = float(elev_text)
        except ValueError:
            self.show_error("Insert values must be numeric")
            return

        lons, lats = self.simulate_satellite(lat, lon, min_elev)

        self.figure.clear()
        
        if self.current_projection != "Global MAP" and self.current_projection != "Local MAP":
            self.current_projection = "Global MAP"  # Default projection if not set

        if self.current_projection == "Global MAP":
            ax = self.figure.add_subplot(111, projection=ccrs.PlateCarree())
        elif self.current_projection == "Local MAP":
            ax = self.figure.add_subplot(111, projection=ccrs.Orthographic(central_longitude=lon, central_latitude=lat))
            self.figure.subplots_adjust(left=0.05, right=0.95, top=1.5, bottom=0.05)

        ax.set_facecolor("#1e1e1e")
        self.figure.patch.set_facecolor("#1e1e1e")

        ax.add_feature(cfeature.LAND, facecolor="dimgray")
        ax.add_feature(cfeature.OCEAN, facecolor="lightgray")
        ax.add_feature(cfeature.COASTLINE, edgecolor="gray")
        ax.add_feature(cfeature.BORDERS, edgecolor="darkred", linestyle=':')

        gl = ax.gridlines(draw_labels=True, dms=True, x_inline=False, y_inline=False)
        gl.xlabel_style = {'color': 'white'}
        gl.ylabel_style = {'color': 'white'}

        lons = np.unwrap(np.radians(lons))
        lons = np.degrees(lons)

        ax.set_global()

        if(simulation_type == "only_sat"):
            ax.plot(lons[0], lats[0], marker='o', color='darkred', markersize=10, transform=ccrs.PlateCarree(), label='Start')
        elif(simulation_type == "only_gs"):
            ax.plot(gs_lons, gs_lats, marker='o', color='blue', markersize=10, transform=ccrs.Geodetic(), label='Ground Station')
        elif(simulation_type == "orbit"):
            ax.plot(lons, lats, '-', color='orange', linewidth=1.2, transform=ccrs.PlateCarree())
            ax.plot(lons[0], lats[0], marker='o', color='darkred', markersize=10, transform=ccrs.PlateCarree(), label='Start')

        self.canvas.draw()

        # Hide loading and error labels
        self.loading_label.setVisible(False)
        self.error_label.setVisible(False)
    

    def simulate_satellite(self, gs_lat, gs_lon, min_elev):
        global simulation_flag
        satellite = Satrec.twoline2rv(line1, line2)

        # Get the epoch time from the TLE
        jd0, fr0 = satellite.jdsatepoch, satellite.jdsatepochF

        # Calculate the epoch datetime
        epoch_datetime = datetime(2000, 1, 1, 12) + timedelta(days=(jd0 + fr0 - 2451545.0))
        start_time = epoch_datetime

        time_steps = [start_time + timedelta(minutes=i) for i in range(0, MINUTES)]

        print("INITIAL TIME: " + str(start_time) + " UTC")
        latitudes = []
        longitudes = []
        altitudes = []
        conta = 0
        ver = True

        for t in time_steps:
            jd, fr = jday(t.year, t.month, t.day, t.hour, t.minute, t.second + t.microsecond*1e-6)
            # e is the error code, r is the position vector in ECI coordinates, v is the velocity vector in ECI coordinates
            e, r, v = satellite.sgp4(jd, fr) # Computing trajectory using SGP4 model
            if e == 0:

                # Convert ECI coordinates to latitude and longitude
                gmst = gmst_from_jd(jd + fr)
                lat, lon = eci_to_latlon(r, gmst)
                latitudes.append(lat)
                longitudes.append(lon)
                altitudes.append(np.linalg.norm(r) - R_EARTH)

                # Convert the satellite position in ECEF coordinates 
                x_eci, y_eci, z_eci = r
                x_ecef = x_eci * np.cos(gmst) + y_eci * np.sin(gmst)
                y_ecef = -x_eci * np.sin(gmst) + y_eci * np.cos(gmst)
                z_ecef = z_eci
                sat_ecef = np.array([x_ecef, y_ecef, z_ecef])

                # Computing the elevation angle 
                elev = elevation_angle(sat_ecef, gs_lats, gs_lons, gs_altitude)

                # Verify the elevation angle with the minimum elevation angle setted by user
                min_elev_deg = 10
                if elev >= min_elev_deg and ver and simulation_flag:
                    contact_time.append(t)
                    ver = False
                    conta += 1
                elif elev < min_elev_deg and not ver and not simulation_flag: # --> RICORDARE DI TOGLIERE IL FLAG SE SI INSERISCONO NUOVE TLE
                    end_contact_time.append(t)
                    ver = True
            #     # Calculate the distance from the ground station and ck if it's within range
            #     distance = distance_3d(lat, lon, np.linalg.norm(r) - R_EARTH, gs_lats, gs_lons, 0)
            #     if distance <= gs_range and ver == True:
            #         print("CONNECTION: " + str(conta))
            #         print(f"GS in range at {t} UTC")
            #         ver = False
            #         conta += 1
            #     elif distance > gs_range and ver == False: 
            #         print(f"GS out of range at {t} UTC")
            #         ver = True
            # else:
            #     latitudes.append(np.nan)
            #     longitudes.append(np.nan)
            #     altitudes.append(np.nan)
        
        simulation_flag = False # Set the flag to false after the first simulation

        return longitudes, latitudes

if __name__ == "__main__":
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
    window = SatelliteSimApp()
    window.show()
    sys.exit(app.exec_())


