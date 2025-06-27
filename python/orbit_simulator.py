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


line1 = "1 25544U 98067A   20300.83097691  .00001534  00000-0  35580-4 0  9996"
line2 = "2 25544  51.6453  57.0843 0001671  64.9808  73.0513 15.49338189252428"

R_EARTH = 6371.0  # Earth radius in kilometers
MINUTES = 800     # Number of minutes to simulate

# Ground station parameters
gs_lons = 11.893123
gs_lats = 45.410935
gs_range = 1000 # Ground station range in Km

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

        # Calculate the distance from the ground station and ck if it's within range
        distance = distance_3d(lat, lon, np.linalg.norm(r) - R_EARTH, gs_lats, gs_lons, 0)
        if distance <= gs_range and ver == True:
            print("CONNECTION: " + str(conta))
            print(f"GS in range at {t} UTC")
            ver = False
            conta += 1
        elif distance > gs_range and ver == False: 
            print(f"GS out of range at {t} UTC")
            ver = True
    else:
        latitudes.append(np.nan)
        longitudes.append(np.nan)
        altitudes.append(np.nan)

# Plotting the trajectory on a map using Cartopy
plt.figure(figsize=(12,6))
ax = plt.axes(projection=ccrs.PlateCarree())

ax.add_feature(cfeature.LAND)
ax.add_feature(cfeature.OCEAN)
ax.add_feature(cfeature.COASTLINE)
ax.add_feature(cfeature.BORDERS, linestyle=':')

ax.gridlines(draw_labels=True, dms=True, x_inline=False, y_inline=False)

lons = np.array(longitudes)
lats = np.array(latitudes)

lons = np.unwrap(np.radians(lons))
lons = np.degrees(lons)

ax.plot(lons, lats, '-', color='orange', linewidth=1.5, transform=ccrs.PlateCarree())
ax.plot(lons[0], lats[0], marker='o', color='darkred', markersize=10, transform=ccrs.PlateCarree(), label='Inizio')

plt.title('ORBIT SIMULATION FROM TLE DATA (' + str(MINUTES) + ' MINUTES)')
plt.legend()

plt.show()
