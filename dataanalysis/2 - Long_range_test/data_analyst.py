# DATA_ANALYST.PY - J2050 TT&C - LONG RANGE TEST DATA ANALYSIS PROGRAM
# Comment format: LRTdata_angle<number>

import numpy as np
import matplotlib.pyplot as plt
import matplotlib as mpl
from scipy.optimize import curve_fit
import mplhep as hep
from cycler import cycler
import statistics
from scipy.odr import *
import struct
import pandas as pd
import argparse

from Ground_station.database import Jdata as jd

# Setting the plot style
plt.style.use(hep.style.ROOT)
params = {'legend.fontsize': '10',
         'legend.loc': 'upper right',
          'legend.frameon':       'True',
          'legend.framealpha':    '0.8',      # legend patch transparency
          'legend.facecolor':     'w', # inherit from axes.facecolor; or color spec
          'legend.edgecolor':     'w',      # background patch boundary color
          'figure.figsize': (6, 4),
         'axes.labelsize': '10',
         'figure.titlesize' : '14',
         'axes.titlesize':'12',
         'xtick.labelsize':'10',
         'ytick.labelsize':'10',
         'lines.linewidth': '1',
         'text.usetex': False,
#         'axes.formatter.limits': '-5, -3',
         'axes.formatter.min_exponent': '2',
#         'axes.prop_cycle': cycler('color', 'bgrcmyk')
         'figure.subplot.left':'0.125',
         'figure.subplot.bottom':'0.125',
         'figure.subplot.right':'0.925',
         'figure.subplot.top':'0.925',
         'figure.subplot.wspace':'0.1',
         'figure.subplot.hspace':'0.1',
#         'figure.constrained_layout.use' : True
          }
plt.rcParams.update(params)
plt.rcParams['axes.prop_cycle'] = cycler(color=['b','g','r','c','m','y','k'])

# Angle uncertainty
Sangle = 10/np.sqrt(24)

# We want to extract all packets related to LRT tests. The comment field contains
# values like "LRTdata<number>".
pattern = 'LRTdata%'

#============= FUNCTIONS =============#

# Function to extract data from database
def extract_from_db():
    """Function to extract data from database.
    Extract ID, RSSI, SNR, DELTAF, COMMENT and ANGLE from database."""

    # Database initialization
    db = jd.init_db()

    # Searching for data inside database
    cursor = db.cursor()

    # Filter comments starting with LRTsetting
    # Extract from packets
    try:
        cursor.execute("SELECT id, rssi, snr, HEX, deltaf, comment FROM packets WHERE comment LIKE ? ORDER BY GS_time", (pattern,))
    except Exception as e:
        print(f"Error occurred while fetching data: {e}")
        return [], [], [], [], [], []

    rows = cursor.fetchall()

    # Data vectors definition (initialize lists used later in the script)
    ID = []
    RSSI = []
    SNR = []
    COMMENT = []
    ANGLE = []
    DELTAF = []

    # Fill vectors with data
    for pkt_id, rssi, snr, payload, deltaf, comment in rows:

        ID.append(pkt_id)
        RSSI.append(rssi)
        SNR.append(snr)
        COMMENT.append(comment)
        DELTAF.append(deltaf)

    # Extracting angle from comment
    for com in COMMENT:
        # Comment format: LRTdata_angle<number>
        angle_str = com.split('_')[1]  # Get the part after the underscore
        angle_value = float(angle_str.replace('angle', ''))  # Remove 'angle' and convert to float
        ANGLE.append(angle_value)

    # Extract from lora pong
    # Extract only packets from ID list
    try:
        if ID:
            id_tuple = tuple(ID)
            query = f"SELECT id, rssi, snr, deltaf, comment FROM LORA_PONG WHERE id IN {id_tuple} ORDER BY GS_time"
            cursor.execute(query)
        else:
            cursor.execute("SELECT id, rssi, snr, deltaf, comment FROM LORA_PONG WHERE comment LIKE ? ORDER BY GS_time", (pattern,))
    except Exception as e:
        print(f"Error occurred while fetching data: {e}")
        return [], [], [], [], [], []
    
    return ID, RSSI, SNR, DELTAF, COMMENT, ANGLE

# Function to compute the mean RSSI for each unique angle
def get_RSSI_mean(unique_angles, RSSI, ANGLE):
    """Function to compute the mean RSSI for each unique angle.
    input: unique_angles - RSSI - ANGLE
    output: unique_angles_sorted - RSSI_mean_sorted - RSSI_std_sorted
    """

    RSSI_mean = []
    RSSI_std = []

    # Computing the mean of RSSI for each unique angle
    for ang in unique_angles:
        # Getting RSSI value corresponding to the specific angle
        rssi_values = [rssi for rssi, a in zip(RSSI, ANGLE) if a == ang]
        # Saving the mean RSSI value for the specific angle
        RSSI_mean.append(np.mean(rssi_values))
        RSSI_std.append(statistics.stdev(rssi_values))

    # Sorting the RSSI means and angles based on increasing angle
    sorted_pairs = sorted(zip(unique_angles, RSSI_mean, RSSI_std))
    unique_angles_sorted, RSSI_mean_sorted, RSSI_std_sorted = zip(*sorted_pairs)
    unique_angles_sorted_list = list(unique_angles_sorted)
    RSSI_mean_sorted_list = list(RSSI_mean_sorted)
    RSSI_std_sorted_list = list(RSSI_std_sorted)

    return np.array(unique_angles_sorted_list), np.array(RSSI_mean_sorted_list), np.array(RSSI_std_sorted_list)

# Function to compute the mean SNR for each unique angle
def get_SNR_mean(unique_angles, SNR, ANGLE):
    """Function to compute the mean SNR for each unique angle.
    input: unique_angles - SNR - ANGLE
    output: unique_angles_sorted - SNR_mean_sorted - SNR_std_sorted
    """

    SNR_mean = []
    SNR_std = []
    
    # Computing the mean of SNR for each unique angle
    for ang in unique_angles:
        # Getting SNR value corresponding to the specific angle
        snr_values = [snr for snr, a in zip(SNR, ANGLE) if a == ang]
        # Saving the mean SNR value for the specific angle
        SNR_mean.append(np.mean(snr_values))
        SNR_std.append(statistics.stdev(snr_values))

    # Sorting the SNR means and angles based on increasing angle
    sorted_pairs = sorted(zip(unique_angles, SNR_mean, SNR_std))
    unique_angles_sorted, SNR_mean_sorted, SNR_std_sorted = zip(*sorted_pairs)
    unique_angles_sorted_list = list(unique_angles_sorted)
    SNR_mean_sorted_list = list(SNR_mean_sorted)
    SNR_std_sorted_list = list(SNR_std_sorted)

    return np.array(unique_angles_sorted_list), np.array(SNR_mean_sorted_list), np.array(SNR_std_sorted_list)

# Function to compute the mean DELTAF for each unique angle
def get_DELTAF_mean(unique_angles, DELTAF, ANGLE):
    """Function to compute the mean DELTAF for each unique angle.
    input: unique_angles - DELTAF - ANGLE
    output: unique_angles_sorted - DELTAF_mean_sorted - DELTAF_std_sorted
    """

    DELTAF_mean = []
    DELTAF_std = []

    # Computing the mean of DELTAF for each unique angle
    for ang in unique_angles:
        # Getting DELTAF value corresponding to the specific angle
        deltaf_values = [deltaf for deltaf, a in zip(DELTAF, ANGLE) if a == ang]
        # Saving the mean DELTAF value for the specific angle
        DELTAF_mean.append(np.mean(deltaf_values))
        DELTAF_std.append(statistics.stdev(deltaf_values))

    # Sorting the DELTAF means and angles based on increasing angle
    sorted_pairs = sorted(zip(unique_angles, DELTAF_mean, DELTAF_std))
    unique_angles_sorted, DELTAF_mean_sorted, DELTAF_std_sorted = zip(*sorted_pairs)
    unique_angles_sorted_list = list(unique_angles_sorted)
    DELTAF_mean_sorted_list = list(DELTAF_mean_sorted)
    DELTAF_std_sorted_list = list(DELTAF_std_sorted)

    return np.array(unique_angles_sorted_list), np.array(DELTAF_mean_sorted_list), np.array(DELTAF_std_sorted_list)

#============= MAIN CODE =============#

# Extracting data from database
ID, RSSI, SNR, DELTAF, COMMENT, ANGLE = extract_from_db()

angle, rssi_mean, rssi_std = get_RSSI_mean(set(ANGLE), RSSI, ANGLE)
angle, snr_mean, snr_std = get_SNR_mean(set(ANGLE), SNR, ANGLE)
angle, deltaf_mean, deltaf_std = get_DELTAF_mean(set(ANGLE), DELTAF, ANGLE)

# Plotting RSSI mean points
fig, ax = plt.subplots(1, 1, figsize=(6, 6),sharex=True)
fig.suptitle("DATA FROM GS", fontsize=13, fontweight = 2)
ax.errorbar(angle,rssi_mean,xerr = Sangle, yerr = rssi_std, fmt='o', label=r'RSSI data [GS - Pico]',ms=3,color='red', zorder = 2, lw = 1.5)
ax.set_ylabel(r'$RSSI \, [dbm]$', size = 13)
ax.set_xlabel(r'Inclination', size = 13)
ax.legend(prop={'size': 14}, loc='upper right', frameon=False).set_zorder(2)

plt.savefig('./python/test/graph/RSSI_mean'+'.png',
            pad_inches = 1,
            transparent = True,
            facecolor ="w",
            edgecolor ='w',
            orientation ='Portrait',
            dpi = 100)

# Plotting SNR mean points
fig, ax = plt.subplots(1, 1, figsize=(6, 6),sharex=True)
fig.suptitle("DATA FROM GS", fontsize=13, fontweight = 2)
ax.errorbar(angle,snr_mean,xerr = Sangle, yerr = snr_std, fmt='o', label=r'SNR data [GS - Pico]',ms=3,color='blue', zorder = 2, lw = 1.5)
ax.set_ylabel(r'$SNR$', size = 13)
ax.set_xlabel(r'Inclination', size = 13)
ax.legend(prop={'size': 14}, loc='upper right', frameon=False).set_zorder(2)

plt.savefig('./python/test/graph/SNR_mean'+'.png',
            pad_inches = 1,
            transparent = True,
            facecolor ="w",
            edgecolor ='w',
            orientation ='Portrait',
            dpi = 100)

# Plotting DELTAF mean points
fig, ax = plt.subplots(1, 1, figsize=(6, 6),sharex=True)
fig.suptitle("DATA FROM GS", fontsize=13, fontweight = 2)
ax.errorbar(angle,deltaf_mean,xerr = Sangle, yerr = deltaf_std, fmt='o', label=r'DELTAF data [GS - Pico]',ms=3,color='green', zorder = 2, lw = 1.5)
ax.set_ylabel(r'$DELTAF$', size = 13)
ax.set_xlabel(r'Inclination', size = 13)
ax.legend(prop={'size': 14}, loc='upper right', frameon=False).set_zorder(2)

plt.savefig('./python/test/graph/DELTAF_mean'+'.png',
            pad_inches = 1,
            transparent = True,
            facecolor ="w",
            edgecolor ='w',
            orientation ='Portrait',
            dpi = 100)

# # Un-comment for SNR FITTING AND PLOT

# # Fitting SNR means with a parabolic function
# function_model_snr = Model(fitf)
# graph_data_snr = RealData(angle, snr_mean, sx=Sangle, sy=snr_std)
# odr_snr = ODR(graph_data_snr, function_model_snr, beta0=[-1, 0, max(snr_mean)])
# out_snr = odr_snr.run()
# out_snr.pprint()
# params_snr = out_snr.beta
# params_err_snr = out_snr.sd_beta
# chi_snr = out_snr.sum_square
# print("Parabolic fitting SNR: ")
# print("Max value: ", params_snr[2], " +- ", params_err_snr[2])
# print("Zero angle founded: ", -params_snr[1]/(2*params_snr[0]), "+-", np.sqrt(pow(params_err_snr[1]/(2*params_snr[0]), 2) + pow(params_snr[1]*params_err_snr[0]/(2*params_snr[0]**2), 2)))
# x_fit_snr = np.linspace(angle[0]-10, snr_mean[-1]+10, 1000)
# y_fit_snr = fitf(params_snr, x_fit_snr)
# fig_snr, ax_snr = plt.subplots(1, 1, figsize=(6, 6),sharex=True)
# ax_snr.errorbar(angle, snr_mean, Sangle, yerr = snr_std, fmt='o', label=r'SNR data [GS - Pico]',ms=3,color='blue', zorder = 2, lw = 1.5)
# ax_snr.plot(x_fit_snr, y_fit_snr, label=r'SNR parabolic fit', color='orange', lw=1.5, zorder=1)
# ax_snr.set_ylabel(r'$SNR$', size = 13)
# ax_snr.set_xlabel(r'Inclination', size = 13)
# ax_snr.legend(prop={'size': 14}, loc='upper right', frameon=False).set_zorder(2)
# graph_data_snr = RealData(angle, snr_mean, sx=Sangle, sy=snr_std)
# odr_snr = ODR(graph_data_snr, function_model_snr, beta0=[-1, 0, max(snr_mean)])
# out_snr = odr_snr.run()
# out_snr.pprint()
# params_snr = out_snr.beta
# params_err_snr = out_snr.sd_beta
# chi_snr = out_snr.sum_square
# print("Parabolic fitting SNR: ")
# print("Max value: ", params_snr[2], " +- ", params_err_snr[2])
# print("Zero angle founded: ", -params_snr[1]/(2*params_snr[0]), "+-", np.sqrt(pow(params_err_snr[1]/(2*params_snr[0]), 2) + pow(params_snr[1]*params_err_snr[0]/(2*params_snr[0]**2), 2)))
# x_fit_snr = np.linspace(angle[0]-10, snr_mean[-1]+10, 1000)
# y_fit_snr = fitf(params_snr, x_fit_snr)
# fig_snr, ax_snr = plt.subplots(1, 1, figsize=(6, 6),sharex=True)
# ax_snr.errorbar(angle, snr_mean, Sangle, yerr = snr_std, fmt='o', label=r'SNR data [GS - Pico]',ms=3,color='blue', zorder = 2, lw = 1.5)
# ax_snr.plot(x_fit_snr, y_fit_snr, label=r'SNR parabolic fit', color='orange', lw=1.5, zorder=1)
# ax_snr.set_ylabel(r'$SNR$', size = 13)
# ax_snr.set_xlabel(r'Inclination', size = 13)
# ax_snr.legend(prop={'size': 14}, loc='upper right', frameon=False).set_zorder(2)
# plt.savefig('./python/test/graph/SNR_mean'+'.png',
#         pad_inches = 1,
#         transparent = True,
#         facecolor ="w",
#         edgecolor ='w',
#         orientation ='Portrait',
#         dpi = 100)

