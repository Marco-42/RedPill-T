import sys
from datetime import datetime
from PyQt5.QtWidgets import *
from PyQt5.QtCore import QTimer
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
from matplotlib.animation import FuncAnimation

# line1 = "1 25544U 98067A   20300.83097691  .00001534  00000-0  35580-4 0  9996"
# line2 = "2 25544  51.6453  57.0843 0001671  64.9808  73.0513 15.49338189252428"

line1 = "1 25544U 98067A   25232.46847203  .00013114  00000-0  23692-3 0  9993"
line2 = "2 25544  51.6355 348.4629 0003387 244.7503 115.3135 15.50052228525134"

R_EARTH = 6371.0  # Earth radius in kilometers
MINUTES = 1440     # Number of minutes to simulate

# Ground station parameters
gs_lons = 11.893123
gs_lats = 45.410935
gs_range = 1000 # Ground station range in Km
gs_altitude = 12 # Ground station altitude in m

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
        
        # Creating display layout
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
        self.setGeometry(100, 100, 1000, 600) 

        self.main_widget = QWidget()
        self.setCentralWidget(self.main_widget)

        # Main layout
        layout = QHBoxLayout(self.main_widget)

        # Controll pannel
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

        # Altitude input
        control_layout.addWidget(QLabel("Ground Station Altitude (m):"))
        self.altitude_input = QLineEdit("")
        control_layout.addWidget(self.altitude_input)

        # Minimum elevation angle input
        control_layout.addWidget(QLabel("Minimum Elevation Angle (deg):"))
        self.elev_input = QLineEdit("10")
        control_layout.addWidget(self.elev_input)
        
        # Projection selector 
        control_layout.addWidget(QLabel("Map Projection:"))

        self.btn_platecarree = QPushButton("Global MAP")
        self.btn_orthographic = QPushButton("Local MAP")
        self.current_projection = "PlateCarree"

        simulation_type = "orbit"  # Default simulation type 
        self.btn_platecarree.clicked.connect(lambda: self.set_projection("Global MAP", simulation_type))
        self.btn_orthographic.clicked.connect(lambda: self.set_projection("Local MAP", simulation_type))

        control_layout.addWidget(self.btn_platecarree)
        control_layout.addWidget(self.btn_orthographic)

        self.loading_label = QLabel("Loading...")
        self.loading_label.setStyleSheet("color: orange; font-size: 16px;")
        self.loading_label.setAlignment(Qt.AlignCenter)
        self.loading_label.setVisible(False)  # initially hide
        control_layout.addWidget(self.loading_label)
        
        # Plotting bar
        control_layout.addWidget(QLabel("Plot option:"))
        self.btn_only_sat = QPushButton("Satellite position zero")
        self.btn_only_gs = QPushButton("GS position")
        self.btn_orbit = QPushButton("Orbit(24 hours)")

        self.btn_only_sat.clicked.connect(lambda: self.run_simulation("only_sat"))
        self.btn_only_gs.clicked.connect(lambda: self.run_simulation("only_gs"))
        self.btn_orbit.clicked.connect(lambda: self.run_simulation("orbit"))

        control_layout.addWidget(self.btn_only_sat)
        control_layout.addWidget(self.btn_only_gs)
        control_layout.addWidget(self.btn_orbit)
        
        # Animation sector
        # Button for full animation
        control_layout.addWidget(QLabel("Animation:"))
        self.btn_full_animation = QPushButton("Complete Animation")
        self.btn_full_animation.clicked.connect(lambda: self.run_simulation("full_animation"))
        control_layout.addWidget(self.btn_full_animation)
        
        control_layout.addStretch()
        layout.addLayout(control_layout, 1) 

        # Button for live tracking
        self.btn_live_animation = QPushButton("Live Tracking")
        self.btn_live_animation.clicked.connect(self.start_live_tracking)
        control_layout.addWidget(self.btn_live_animation)
        self.btn_live_animation.setStyleSheet("""
            QPushButton {
                background-color: #1e1e1e;
                color: #FFCD00;
                font-weight: bold;
                font-size: 16px;
                border: 3px solid #FFCD00;
                border-radius: 8px;
                padding: 10px 20px;
            }
            QPushButton:hover {
                background-color: #FFCD00;
                color: #1e1e1e;
            }
        """)

        # Graphic pannel
        self.figure = Figure()
        self.canvas = FigureCanvas(self.figure)
        self.figure.patch.set_facecolor("#1e1e1e")  # Figure Background 
        self.canvas.setStyleSheet("background-color: #1e1e1e;")
        layout.addWidget(self.canvas, 4)

        self.coord_combo.currentIndexChanged.connect(self.on_preset_changed)

        # Graphic pannel + timer
        plot_layout = QVBoxLayout()
        plot_layout.addWidget(self.canvas)

        # "Current time"
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

        # Contact time label
        self.contact_time = QLabel("""
            <div style='text-align: center;'>
                <div style='font-size: 16px; font-family: Segoe UI; color: #00BFFF;'>CONTACT TIME</div>
                <div style='font-size: 24px; font-family: Courier New; color: #00BFFF;'>--:--:--</div>
            </div>
        """)
        self.contact_time.setAlignment(Qt.AlignCenter)
        self.contact_time.setStyleSheet("""
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
        
        # Setting the default index for the next contact
        self.index = 1

        # Button to set the next contact index
        self.button1 = QPushButton("1")
        self.button2 = QPushButton("2")
        self.button3 = QPushButton("3")
        self.button1.clicked.connect(lambda: self.set_next_contact_index(1))
        self.button2.clicked.connect(lambda: self.set_next_contact_index(2))
        self.button3.clicked.connect(lambda: self.set_next_contact_index(3))
        for btn in (self.button1, self.button2, self.button3):
            btn.setFixedWidth(40)
            btn.setStyleSheet("font-size: 16px; background-color: #222; color: #DAA520; border: 2px solid #DAA520; border-radius: 6px;")

        # Vertical layout per i pulsanti
        btns_layout = QVBoxLayout()
        btns_layout.addWidget(self.button1)
        btns_layout.addWidget(self.button2)
        btns_layout.addWidget(self.button3)
        # Non aggiungere stretch!

        # Layout orizzontale principale
        time_and_btns_layout = QHBoxLayout()
        time_and_btns_layout.setSpacing(10)  # Puoi ridurre questo valore per avvicinare i widget
        time_and_btns_layout.setContentsMargins(0, 0, 0, 0)
        time_and_btns_layout.addWidget(self.clock_label)
        time_and_btns_layout.addLayout(btns_layout)
        time_and_btns_layout.addWidget(self.contact_time)
        time_and_btns_layout.addWidget(self.next_contact_label)

        plot_layout.addLayout(time_and_btns_layout)    
        
        # Add to main layout
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

        # Appling the function to stop the animation or the live tracking if any button is pressed
        for btn in self.findChildren(QPushButton):
            btn.clicked.connect(self.stop_animation)
            if btn != self.btn_live_animation and btn != self.button1 and btn != self.button2 and btn != self.button3:
                btn.clicked.connect(self.stop_live_tracking)
    


    # Function to set the index of the next contact
    def set_next_contact_index(self, number):
        self.loading_label.setVisible(True)  # Loading message
        self.index = number

    # Function to show the error message
    def show_error(self, message):
        self.figure.clear()
        self.canvas.draw()
        self.error_label.setText(message)
        self.error_label.setVisible(True)

        # Function to check if there is an error in the GS setup
    
    # Function to check if there is an error in the GS setup
    def error_in_gs_setup(self):
        """return the error or lat - lon - elevation angle - altitude""" 
        # Extract data relative to GS
        lat_text = self.lat_input.text().strip()
        lon_text = self.lon_input.text().strip()
        alt_text = self.altitude_input.text().strip()
        elev_text = self.elev_input.text().strip()

        if not lat_text or not lon_text or not elev_text or not alt_text:
            self.show_error("Insert value for all fields[GS]")
            return

        try:
            lat = float(lat_text)
            lon = float(lon_text)
            min_elev = float(elev_text)
            alt = float(alt_text) / 1000.0  # Convert altitude from meters to kilometers
            return lat, lon, min_elev, alt
        
        except ValueError:
            self.show_error("Insert values must be numeric")
            return
        
    # Function to update the timer, the countdown for the next contact and the contact time
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
        
        # Computing time of the next contact
        if simulation_flag == False and contact_time and end_contact_time and self.index <= len(contact_time):

            # Extract next contact time
            next_contact = contact_time[self.index-1]

            # Extract the next contact final time
            next_contact_end = end_contact_time[self.index-1]
            
            # Computing the contact time
            CT = next_contact_end - next_contact
            h, remainder = divmod(int(CT.total_seconds()), 3600)
            m, s = divmod(remainder, 60)
            CT_str = f"{h:02}:{m:02}:{s:02}"

            # Computing the countdown
            delta = next_contact - now
            h, remainder = divmod(int(delta.total_seconds()), 3600)
            m, s = divmod(remainder, 60)
            countdown_str = f"{h:02}:{m:02}:{s:02}"

            #print(delta.total_seconds())

            # Setting the style for the contact time
            if(int(delta.total_seconds()) > 0):
                self.contact_time.setText(f"""
                <div style='text-align: center;'>
                    <div style='font-size: 14px; font-family: Segoe UI; color: #00BFFF;'>CONTACT TIME</div>
                    <div style='font-size: 24px; font-family: Courier New; color: #00BFFF;'>{CT_str}</div>
                </div>
            """)
            elif(int(delta.total_seconds()) < 0 and now < next_contact_end):
                # Computing the countdown
                h, remainder = divmod(int(delta.total_seconds()), 3600)
                m, s = divmod(remainder, 60)
                contanct_countdown_str = f"{h:02}:{m:02}:{s:02}"

                self.contact_time.setText(f"""
                <div style='text-align: center;'>
                    <div style='font-size: 14px; font-family: Segoe UI; color: #00BFFF;'>CONTACT COUNTDOWN</div>
                    <div style='font-size: 24px; font-family: Courier New; color: #00BFFF;'>{contanct_countdown_str}</div>
                </div>
            """)
            elif(delta.total_seconds() < 0 and now > next_contact_end):
                self.contact_time.setText(f"""
                <div style='text-align: center;'>
                    <div style='font-size: 14px; font-family: Segoe UI; color: #00BFFF;'>CONTACT TIME</div>
                    <div style='font-size: 24px; font-family: Courier New; color: #00BFFF;'>{"--:--:--"}</div>
                </div>
            """)           

        elif simulation_flag == False and not contact_time and not end_contact_time or (self.index > len(contact_time) and self.lat_input.text().strip() != ""): 
            countdown_str = "NO CONTACTS"
            CT_str = "NO CONTACTS"
        else:
            countdown_str = "--:--:--"
            CT_str = "--:--:--"

        # Setting the style for the next contact label(in relation of witch contact is selected)
        if self.index == 1:
            self.next_contact_label.setText(f"""
                <div style='text-align: center;'>
                    <div style='font-size: 14px; font-family: Segoe UI; color: #00BFFF;'>NEXT CONTACT</div>
                    <div style='font-size: 24px; font-family: Courier New; color: #00BFFF;'>{countdown_str}</div>
                </div>
            """)
        elif self.index == 2:
            self.next_contact_label.setText(f"""
            <div style='text-align: center;'>
                <div style='font-size: 14px; font-family: Segoe UI; color: #00BFFF;'>SECOND CONTACT</div>
                <div style='font-size: 24px; font-family: Courier New; color: #00BFFF;'>{countdown_str}</div>
            </div>
        """)
        elif self.index == 3: 
            self.next_contact_label.setText(f"""
            <div style='text-align: center;'>
                <div style='font-size: 14px; font-family: Segoe UI; color: #00BFFF;'>THIRD CONTACT</div>
                <div style='font-size: 24px; font-family: Courier New; color: #00BFFF;'>{countdown_str}</div>
            </div>
        """)
                        
        # Unset loading message
        self.loading_label.setVisible(False)

    # Function to display the simulation
    def run_simulation(self, simulation_type):

        self.loading_label.setVisible(True)  # Loading message

        # Check if the simulation is static or animated
        if(simulation_type == "full_animation"):
            QTimer.singleShot(100, lambda: self.animate_satellite())  # Start afeter 100 ms
        else: 
            QTimer.singleShot(100, lambda: self._execute_simulation(simulation_type))  # Start afeter 100 ms

    # Function for the projection's setting
    def set_projection(self, projection_name, simulation_type):
        self.current_projection = projection_name
        self.run_simulation(simulation_type)  # Redone the simulation with new projection

    # Setting GS coordinates
    def on_preset_changed(self, index):
        preset = self.coord_combo.currentText()
        presets = {
            "Padua GS": (gs_lats, gs_lons, gs_altitude),
        }

        if preset == "Personalizzato":
            self.lat_input.setText("")
            self.lon_input.setText("")
            self.elev_input.setText("")
        else:
            lat, lon, alt = presets.get(preset, ("", "", ""))
            self.lat_input.setText(str(lat))
            self.lon_input.setText(str(lon))
            self.altitude_input.setText(str(alt))

    # ORBIT VISUALIZATION FUNCTION
    def _execute_simulation(self, simulation_type):
        lat_text = self.lat_input.text().strip()
        lon_text = self.lon_input.text().strip()
        alt_text = self.altitude_input.text().strip()
        elev_text = self.elev_input.text().strip()

        if not lat_text or not lon_text or not elev_text or not alt_text:
            self.show_error("Insert value for all fields[GS]")
            return

        try:
            lat = float(lat_text)
            lon = float(lon_text)
            min_elev = float(elev_text)
            alt = float(alt_text) / 1000.0  # Convert altitude from meters to kilometers
        except ValueError:
            self.show_error("Insert values must be numeric")
            return

        lons, lats = self.simulate_satellite(lat, lon, alt, min_elev)

        self.figure.clear()
        
        if self.current_projection != "Global MAP" and self.current_projection != "Local MAP":
            self.current_projection = "Global MAP"  # Default projection if not set

        if self.current_projection == "Global MAP":
            ax = self.figure.add_subplot(111, projection=ccrs.PlateCarree())
            self.figure.subplots_adjust(left=0.125, right=0.9, top=0.88, bottom=0.11)

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
            ax.plot(lon, lat, marker='o', color='blue', markersize=10, transform=ccrs.Geodetic(), label='Ground Station')
        elif(simulation_type == "orbit"):
            ax.plot(lons, lats, '-', color='orange', linewidth=1.2, transform=ccrs.PlateCarree())
            ax.plot(lons[0], lats[0], marker='o', color='darkred', markersize=10, transform=ccrs.PlateCarree(), label='Start')

        self.canvas.draw()

        # Hide loading and error labels
        self.loading_label.setVisible(False)
        self.error_label.setVisible(False)
    
    # ORBIT SIMULATION FUNCTION
    def simulate_satellite(self, gs_lat, gs_lon, gs_alt, min_elev, return_time = False):

        # Clearing the contact time vectors
        contact_time.clear()
        end_contact_time.clear()

        global simulation_flag
        satellite = Satrec.twoline2rv(line1, line2)

        in_contact = False  # No concat at the beginning

        # Get the epoch time from the TLE
        jd0, fr0 = satellite.jdsatepoch, satellite.jdsatepochF

        # Calculate the epoch datetime
        epoch_datetime = datetime(2000, 1, 1, 12) + timedelta(days=(jd0 + fr0 - 2451545.0))
        start_time = epoch_datetime

        time_steps = [start_time + timedelta(minutes=i) for i in range(0, MINUTES)]

        latitudes = []
        longitudes = []
        altitudes = []
        #conta = 0

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
                elev = elevation_angle(sat_ecef, gs_lat, gs_lon, gs_alt)

                # Verify the elevation angle with the minimum elevation angle setted by user
                if elev >= min_elev and not in_contact:
                    contact_time.append(t)
                    in_contact = True
                elif elev < min_elev and in_contact:
                    end_contact_time.append(t)
                    in_contact = False

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


        # If the simulation is only for the orbit, return the latitudes and longitudes
        if return_time == False: 
            return longitudes, latitudes
        
        # If the simulation is for the live animation, return the latitudes, longitudes and contact times
        elif( return_time == True):
            return longitudes, latitudes, time_steps
        
    # ORBIT ANIMATION FUNCTION
    def animate_satellite(self):
        
        # Check possibile errors in gs setting
        result = self.error_in_gs_setup()
        if result is None:
            return
        lat, lon, min_elev, alt = result 

        # Running the simulation
        lons, lats = self.simulate_satellite(lat, lon, alt, min_elev)

        self.figure.clear()

        # Selection the type of projection
        if self.current_projection == "Global MAP":
            ax = self.figure.add_subplot(111, projection=ccrs.PlateCarree())
        elif self.current_projection == "Local MAP":
            ax = self.figure.add_subplot(111, projection=ccrs.Orthographic(central_longitude=lon, central_latitude=lat))
            self.figure.subplots_adjust(left=0.05, right=0.95, top=1.5, bottom=0.05)
        else:
            self.current_projection = "Global MAP"
            ax = self.figure.add_subplot(111, projection=ccrs.PlateCarree())

        # Setting the map style
        ax.set_facecolor("#1e1e1e")
        self.figure.patch.set_facecolor("#1e1e1e")
        ax.add_feature(cfeature.LAND, facecolor="dimgray")
        ax.add_feature(cfeature.OCEAN, facecolor="lightgray")
        ax.add_feature(cfeature.COASTLINE, edgecolor="gray")
        ax.add_feature(cfeature.BORDERS, edgecolor="darkred", linestyle=':')

        gl = ax.gridlines(draw_labels=True, dms=True, x_inline=False, y_inline=False)
        gl.xlabel_style = {'color': 'white'}
        gl.ylabel_style = {'color': 'white'}

        ax.set_global()

        # Some correction to possibile discontinuities in the longitude values(like -pi or + pi)
        lons = np.unwrap(np.radians(lons))
        lons = np.degrees(lons)

        # To obtain a 1D array
        lons = np.asarray(lons).flatten()
        lats = np.asarray(lats).flatten()

        # Plotting the GS position
        ax.plot(lon, lat, marker='o', color='blue', markersize=8, transform=ccrs.Geodetic(), label='Ground Station')

        # Setting the line and marker
        satellite_path, = ax.plot([], [], '-', color='orange', linewidth=1.2, transform=ccrs.PlateCarree())
        satellite_dot, = ax.plot([], [], 'o', color='darkred', markersize=10, transform=ccrs.PlateCarree())

        # Update each frame
        def update(frame):
            if len(lons) == 0 or len(lats) == 0:
                satellite_path.set_data([], [])
                satellite_dot.set_data([], [])
            elif frame == 0:
                satellite_path.set_data([], [])
                satellite_dot.set_data([lons[0]], [lats[0]])
            else:
                satellite_path.set_data(lons[:frame], lats[:frame])
                satellite_dot.set_data([lons[frame-1]], [lats[frame-1]])
            return satellite_path, satellite_dot

        # # Animation time
        interval = 50  # ms between each frame
        self.ani = FuncAnimation(self.figure, update, frames=len(lons), interval=interval, blit=True)
        self.canvas.draw()
    
        # Hide loading and error labels
        self.loading_label.setVisible(False)
        self.error_label.setVisible(False)

    # START LIVE TRACKING
    def start_live_tracking(self):
        
        self.loading_label.setVisible(True)  # Loading message

        # Closed the previous timer if active
        if hasattr(self, 'live_timer') and self.live_timer.isActive():
            self.live_timer.stop()

        # Check possibile errors in gs setting
        result = self.error_in_gs_setup()
        if result is None:
            return
        lat, lon, min_elev, alt = result 


        if self.current_projection == "Global MAP":
            ax = self.figure.add_subplot(111, projection=ccrs.PlateCarree())
        elif self.current_projection == "Local MAP":
            ax = self.figure.add_subplot(111, projection=ccrs.Orthographic(central_longitude=lon, central_latitude=lat))
            self.figure.subplots_adjust(left=0.05, right=0.95, top=1.5, bottom=0.05)
        else:
            self.current_projection = "Global MAP"
            ax = self.figure.add_subplot(111, projection=ccrs.PlateCarree())

        # Setting the map style
        ax.set_facecolor("#1e1e1e")
        self.figure.patch.set_facecolor("#1e1e1e")
        ax.add_feature(cfeature.LAND, facecolor="dimgray")
        ax.add_feature(cfeature.OCEAN, facecolor="lightgray")
        ax.add_feature(cfeature.COASTLINE, edgecolor="gray")
        ax.add_feature(cfeature.BORDERS, edgecolor="darkred", linestyle=':')
        gl = ax.gridlines(draw_labels=True, dms=True, x_inline=False, y_inline=False)
        gl.xlabel_style = {'color': 'white'}
        gl.ylabel_style = {'color': 'white'}
        ax.set_global()

        # Plotting ground station position
        ax.plot(lon, lat, marker='o', color='blue', markersize=8, transform=ccrs.Geodetic(), label='Ground Station')

        # Create the marker for the live satellite position
        self.live_sat_dot, = ax.plot([], [], 'o', color='darkred', markersize=10, transform=ccrs.PlateCarree(), zorder = 2)

        # Create the line for the satellite path
        self.satellite_path, = ax.plot([], [], '-', color='orange', linewidth=1.2, transform=ccrs.PlateCarree(), zorder = 1)
        self.canvas.draw()

        # Timer to update the satellite position every second
        self.live_timer = QTimer()
        self.live_timer.timeout.connect(self.update_live_position)
        self.live_timer.start(1000)  # Update every second

    # UPDATE THE LIVE POSITION
    def update_live_position(self):

        seconds_to_simulate = 3600

        # Check possibile errors in gs setting
        result = self.error_in_gs_setup()
        if result is None:
            return
        lat, lon, min_elev, alt = result 

        # Get the current time
        now = datetime.utcnow()

        # Run the simulation to get the current satellite position
        lons_temp, lats_temp, time_temp = self.simulate_satellite(lat, lon, alt, min_elev, True)

        # Hide loading and error labels
        self.loading_label.setVisible(False)
        self.error_label.setVisible(False)

        if now > max(time_temp):
            self.show_error("Please update TLE lines(to old)")
            return
        
        # Find the index of the closest time to now
        idx = np.argmin([abs((t - now).total_seconds()) for t in time_temp])
        delta_t = (time_temp[idx+1] - time_temp[idx]).total_seconds()
        sat_lat = lats_temp[idx]
        sat_lon = lons_temp[idx]

        # Extract the satellite path for the last +-30 minutes
        sat_path_lat = lats_temp[idx-int(900/delta_t):idx+int(seconds_to_simulate/delta_t)]
        sat_path_lon = lons_temp[idx-int(900/delta_t):idx+int(seconds_to_simulate/delta_t)]

        # Some correction to possibile discontinuities in the longitude values(like -pi or + pi)
        sat_path_lon = np.unwrap(np.radians(sat_path_lon))
        sat_path_lon = np.degrees(sat_path_lon)

        # Update the live satellite position on the map
        self.live_sat_dot.set_data([sat_lon], [sat_lat])
        self.satellite_path.set_data([sat_path_lon], [sat_path_lat])
        self.canvas.draw()   
           
    # Function to stop the animation(important to not crash the program)
    def stop_animation(self):
        if hasattr(self, 'ani') and self.ani is not None:
            self.ani.event_source.stop()

    # Function to stop the live tracking
    def stop_live_tracking(self):
        if hasattr(self, 'live_timer') and self.live_timer.isActive():
            self.live_timer.stop()

if __name__ == "__main__":
    dark_palette = QPalette()

    # Setting the color palette for the graphic layout
    dark_palette.setColor(QPalette.Window, QColor(30, 30, 30))              # window background
    dark_palette.setColor(QPalette.WindowText, Qt.white)                   # text
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

    # Applaing the palette
    QApplication.setPalette(dark_palette)

    app = QApplication(sys.argv)
    window = SatelliteSimApp()

    # Show the windows
    window.show()
    sys.exit(app.exec_())
