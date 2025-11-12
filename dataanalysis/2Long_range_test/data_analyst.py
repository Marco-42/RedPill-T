# DATA_ANALYST.PY - J2050 TT&C - LONG RANGE TEST DATA ANALYSIS PROGRAM
# Comment format: LRT_<number>

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

from groundstation.database import Jdata as jd

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
# values come "LRT_<numero>". Escludi "LRTdata".
pattern = 'LRT_%'

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
        if comment.startswith('LRT_'):
            ID.append(pkt_id)
            RSSI.append(rssi)
            SNR.append(snr)
            COMMENT.append(comment)
            DELTAF.append(deltaf)

    # Extracting angle from comment
    for com in COMMENT:
        # Comment format: LRT_<number>
        if(com.split('_')[0] == 'LRT'):
            angle_str = com.split('_')[1]  # Get the part after the underscore
            angle_value = float(angle_str)  # Convert to float
            ANGLE.append(angle_value)

    # Extract from lora pong
    # Extract only packets from ID list
    #try:
    #     if ID:
    #         id_tuple = tuple(ID)
    #         query = f"SELECT id, rssi, snr, deltaf FROM LORA_PONG WHERE id IN {id_tuple} ORDER BY GS_time"
    #         cursor.execute(query)
    #     else:
    #         cursor.execute("SELECT id, rssi, snr, deltaf FROM LORA_PONG WHERE comment LIKE ? ORDER BY GS_time", (pattern,))
    # except Exception as e:
    #     print(f"Error occurred while fetching data: {e}")
    #     return [], [], [], []
    
    return ID, RSSI, SNR, DELTAF, COMMENT, ANGLE

def extract_from_db_lora_pong(ids_to_extract):
    """Function to extract data from LORA_PONG database.
    Extract ID, RSSI, SNR, DELTAF from LORA_PONG database."""

    # Database initialization
    db = jd.init_db()

    # Searching for data inside database
    cursor = db.cursor()


    # extract only data with specific ids
    if not ids_to_extract:
        return [], [], [], []
    
    # Build the query with placeholders
    placeholders = ','.join(['?'] * len(ids_to_extract))
    query = f"SELECT id, rssi, snr, deltaf FROM LORA_PONG WHERE id IN ({placeholders}) ORDER BY id"
    try:
        cursor.execute(query, ids_to_extract)
    except Exception as e:
        print(f"Error occurred while fetching data: {e}")
        return [], [], [], []

    rows = cursor.fetchall()

    # Data vectors definition (initialize lists used later in the script)
    ID_out = []
    RSSI = []
    SNR = []
    DELTAF = []

    # Fill vectors with data
    for pkt_id, rssi, snr, deltaf in rows:
        ID_out.append(pkt_id)
        RSSI.append(rssi)
        SNR.append(snr)
        DELTAF.append(deltaf)

    return ID_out, RSSI, SNR, DELTAF

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
    if not sorted_pairs:
        return np.array([]), np.array([]), np.array([])
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
    if not sorted_pairs:
        return np.array([]), np.array([]), np.array([])
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
    if not sorted_pairs:
        return np.array([]), np.array([]), np.array([])
    unique_angles_sorted, DELTAF_mean_sorted, DELTAF_std_sorted = zip(*sorted_pairs)
    unique_angles_sorted_list = list(unique_angles_sorted)
    DELTAF_mean_sorted_list = list(DELTAF_mean_sorted)
    DELTAF_std_sorted_list = list(DELTAF_std_sorted)
    return np.array(unique_angles_sorted_list), np.array(DELTAF_mean_sorted_list), np.array(DELTAF_std_sorted_list)

#=============e MAINl CODE =============#

# Extracting data from database
ID, RSSI, SNR, DELTAF, COMMENT, ANGLE = extract_from_db()

# Extracting data from lora pong
ID_lora, RSSI_lora, SNR_lora, DELTAF_lora = extract_from_db_lora_pong(ID)

# Not extract data with specific IDs
ids_to_remove = {193, 194, 203}
filtered = [(i, r, s, d, c, a) for i, r, s, d, c, a in zip(ID, RSSI, SNR, DELTAF, COMMENT, ANGLE) if i not in ids_to_remove]
filtered_lora = [(i, r, s, d) for i, r, s, d in zip(ID_lora, RSSI_lora, SNR_lora, DELTAF_lora) if i not in ids_to_remove]
if filtered and filtered_lora:
    ID, RSSI, SNR, DELTAF, COMMENT, ANGLE = map(list, zip(*filtered))
    ID_lora, RSSI_lora, SNR_lora, DELTAF_lora = map(list, zip(*filtered_lora))
else:
    ID, RSSI, SNR, DELTAF, COMMENT, ANGLE = [], [], [], [], [], []
    ID_lora, RSSI_lora, SNR_lora, DELTAF_lora = [], [], [], []

# Printing data
print("DATA EXTRACTED FROM DB:")
print("ID \t ANGLE \t RSSI \t SNR")
for i in range (len(ID)):
    print(ID[i], ANGLE[i], RSSI[i], SNR[i])

# Computing the values mean and std for each unique angle from GS
angle, rssi_mean, rssi_std = get_RSSI_mean(set(ANGLE), RSSI, ANGLE)
angle, snr_mean, snr_std = get_SNR_mean(set(ANGLE), SNR, ANGLE)
angle, deltaf_mean, deltaf_std = get_DELTAF_mean(set(ANGLE), DELTAF, ANGLE)

# Computing the values mean and std for each unique angle from LORA PONG
angle_lora, rssi_lora_mean, rssi_lora_std = get_RSSI_mean(set(ANGLE), RSSI_lora, ANGLE)
angle_lora, snr_lora_mean, snr_lora_std = get_SNR_mean(set(ANGLE), SNR_lora, ANGLE)
angle_lora, deltaf_lora_mean, deltaf_lora_std = get_DELTAF_mean(set(ANGLE), DELTAF_lora, ANGLE)

# Computing differences
RSSI_diff = rssi_mean - rssi_lora_mean
RSSI_diff_std = np.sqrt(rssi_std**2 + rssi_lora_std**2)
SNR_diff = snr_mean - snr_lora_mean
SNR_diff_std = np.sqrt(snr_std**2 + snr_lora_std**2)
DELTAF_diff = deltaf_mean - deltaf_lora_mean
DELTAF_diff_std = np.sqrt(deltaf_std**2 + deltaf_lora_std**2)

# Printing max RSSI ans SNR values
print("MAX RSSI GS: ", max(rssi_mean), " at angle ", angle[np.argmax(rssi_mean)])
print("MAX RSSI PICO: ", max(rssi_lora_mean), " at angle ", angle_lora[np.argmax(rssi_lora_mean)])
print("MAX SNR GS: ", max(snr_mean), " at angle ", angle[np.argmax(snr_mean)])
print("MAX SNR PICO: ", max(snr_lora_mean), " at angle ", angle_lora[np.argmax(snr_lora_mean)])

# Computing RSSI LOSS
# find max RSSI value and its std for GS
if len(rssi_mean) > 0:
    idx_max = int(np.argmax(rssi_mean))
    max_rssi = rssi_mean[idx_max]
    max_rssi_std = rssi_std[idx_max]
else:
    max_rssi = 0
    max_rssi_std = 0

# find max RSSI value and its std for PICO (LORA)
if len(rssi_lora_mean) > 0:
    idx_max_lora = int(np.argmax(rssi_lora_mean))
    max_rssi_lora = rssi_lora_mean[idx_max_lora]
    max_rssi_lora_std = rssi_lora_std[idx_max_lora]
else:
    max_rssi_lora = 0
    max_rssi_lora_std = 0

# compute losses and propagate uncertainties
RSSI_loss = rssi_mean - max_rssi
RSSI_loss_std = np.sqrt(rssi_std**2 + max_rssi_std**2)
RSSI_lora_loss = rssi_lora_mean - max_rssi_lora
RSSI_lora_loss_std = np.sqrt(rssi_lora_std**2 + max_rssi_lora_std**2)
RSSI_loss_diff = RSSI_loss - RSSI_lora_loss
RSSI_loss_diff_std = np.sqrt(RSSI_loss_std**2 + RSSI_lora_loss_std**2)

# Plotting RSSI mean points
fig, ax = plt.subplots(2, 1, figsize=(6, 6),sharex=True, height_ratios= (2, 1))
fig.suptitle("RSSI DATA", fontsize=11, fontweight = 2)
ax[0].errorbar(angle,rssi_mean,xerr = Sangle, yerr = rssi_std, fmt='o', label=r'RSSI data [GS]',ms=3,color='firebrick', zorder = 2, lw = 1.5)
ax[0].errorbar(angle, rssi_lora_mean, xerr = Sangle, yerr = rssi_lora_std, fmt='o', label=r'RSSI data [PICO]',ms=3,color='dodgerblue', zorder = 1, lw = 1.5)
ax[0].axvline(x=-15, color='gray', linestyle='--', linewidth=1.5, label = 'Reference Angle')
ax[0].set_ylabel(r'$RSSI \, [dbm]$', size = 11)
ax[0].set_xlabel(r'Inclination', size = 11)
ax[0].legend(prop={'size': 11}, loc='upper right', frameon=False).set_zorder(2)
ax[1].errorbar(angle, RSSI_diff, yerr=RSSI_diff_std, fmt='o', ms=3, color='darkorange', zorder=3, lw=1.5)
ax[1].legend([r'RSSI Difference [GS - PICO]'], prop={'size': 11}, loc='upper right', frameon=False).set_zorder(2)
ax[1].set_ylabel(r'$RSSI \, [dbm]$', size = 11)
ax[1].set_xlabel(r'Inclination', size = 11)
ax[1].axvline(x=-15, color='gray', linestyle='--', linewidth=1.5)
ax[1].set_ylim([-5, 10])
ax[0].set_ylim([-122, -93])
plt.savefig('./dataanalysis/2Long_range_test/RSSI_mean'+'.png',
            pad_inches = 1,
            transparent = True,
            facecolor ="w",
            edgecolor ='w',
            orientation ='Portrait',
            dpi = 100)

# Plotting SNR mean points
fig, ax = plt.subplots(2, 1, figsize=(6, 6),sharex=True, height_ratios= (2, 1))
fig.suptitle("SNR DATA", fontsize=11, fontweight = 2)
ax[0].errorbar(angle,snr_mean,xerr = Sangle, yerr = snr_std, fmt='o', label=r'SNR data [GS - Pico]',ms=3,color='firebrick', zorder = 2, lw = 1.5)
ax[0].errorbar(angle_lora, snr_lora_mean, xerr = Sangle, yerr = snr_lora_std, fmt='o', label=r'SNR data [PICO]',ms=3,color='dodgerblue', zorder = 1, lw = 1.5)
ax[0].axvline(x=-15, color='gray', linestyle='--', linewidth=1.5, label = 'Reference Angle')
ax[0].set_ylabel(r'$SNR \, (db)$', size = 11)
ax[0].set_xlabel(r'Inclination', size = 11)
ax[0].legend(prop={'size': 11}, loc='upper right', frameon=False).set_zorder(2)
ax[1].errorbar(angle, SNR_diff, yerr=SNR_diff_std, fmt='o', ms=3, color='darkorange', zorder=3, lw=1.5)
ax[1].legend([r'SNR Difference [GS - PICO]'], prop={'size': 11}, loc='upper right', frameon=False).set_zorder(2)
ax[1].set_ylabel(r'$SNR \, (db)$', size = 11)
ax[1].set_xlabel(r'Inclination', size = 11)
ax[1].axvline(x=-15, color='gray', linestyle='--', linewidth=1.5)
ax[0].set_ylim([-8, 26])
ax[1].set_ylim([-15, 13])
plt.savefig('./dataanalysis/2Long_range_test/SNR_mean'+'.png',
            pad_inches = 1,
            transparent = True,
            facecolor ="w",
            edgecolor ='w',
            orientation ='Portrait',
            dpi = 100)

# Plotting DELTAF mean points
fig, ax = plt.subplots(1, 1 , figsize=(6, 6),sharex=True)
fig.suptitle("DELTAF DATA", fontsize=11, fontweight = 2)
ax.errorbar(angle,deltaf_mean,xerr = Sangle, yerr = deltaf_std, fmt='o', label=r'DELTAF data [GS - Pico]',ms=3,color='firebrick', zorder = 2, lw = 1.5)
ax.errorbar(angle_lora, deltaf_lora_mean, xerr = Sangle, yerr = deltaf_lora_std, fmt='o', label=r'DELTAF data [PICO]',ms=3,color='dodgerblue', zorder = 1, lw = 1.5)
ax.axvline(x=-15, color='gray', linestyle='--', linewidth=1.5, label = 'Reference Angle')
ax.set_ylabel(r'$\Delta F \, (Hz)$', size = 11)
ax.set_xlabel(r'Inclination', size = 11)
ax.legend(prop={'size': 11}, loc='upper right', frameon=False).set_zorder(2)
ax.set_ylim([-2000, 2500])

plt.savefig('./dataanalysis/2Long_range_test/DELTAF_mean'+'.png',
            pad_inches = 1,
            transparent = True,
            facecolor ="w",
            edgecolor ='w',
            orientation ='Portrait',
            dpi = 100)

# Plotting RSSI LOSS
fig, ax = plt.subplots(2, 1, figsize=(6, 6),sharex=True, height_ratios= (2, 1))
fig.suptitle("RSSI LOSS DATA", fontsize=11, fontweight = 2)
ax[0].errorbar(angle,RSSI_loss,xerr = Sangle, yerr = RSSI_loss_std, fmt='o', label=r'RSSI loss [GS]',ms=3,color='firebrick', zorder = 2, lw = 1.5)
ax[0].errorbar(angle, RSSI_lora_loss, xerr = Sangle, yerr = RSSI_lora_loss_std, fmt='o', label=r'RSSI loss [PICO]',ms=3,color='dodgerblue', zorder = 1, lw = 1.5)
ax[0].axvline(x=-15, color='gray', linestyle='--', linewidth=1.5, label = 'Reference Angle')
ax[0].set_ylabel(r'$RSSI \, [dbm]$', size = 11)
ax[0].set_xlabel(r'Inclination', size = 11)
ax[0].legend(prop={'size': 11}, loc='upper right', frameon=False).set_zorder(2)
ax[1].errorbar(angle, RSSI_loss_diff, yerr=RSSI_loss_diff_std, fmt='o', ms=3, color='darkorange', zorder=3, lw=1.5)
ax[1].legend([r'RSSI loss Difference [GS - PICO]'], prop={'size': 11}, loc='upper right', frameon=False).set_zorder(2)
ax[1].set_ylabel(r'$RSSI \, [dbm]$', size = 11)
ax[1].set_xlabel(r'Inclination', size = 11)
ax[1].axvline(x=-15, color='gray', linestyle='--', linewidth=1.5)
ax[1].set_ylim([-6, 9.2])
ax[0].set_ylim([-24, 8])
plt.savefig('./dataanalysis/2Long_range_test/RSSI_loss'+'.png',
            pad_inches = 1,
            transparent = True,
            facecolor ="w",
            edgecolor ='w',
            orientation ='Portrait',
            dpi = 100)


plt.show()
