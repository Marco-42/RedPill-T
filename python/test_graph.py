import Jdata as jd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib as mpl
from scipy.optimize import curve_fit
import mplhep as hep
from cycler import cycler
import statistics
from scipy.odr import *
import struct

# settaggio globale grafici
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

# Function definition - Linear function
def fitf(par, x):
    """par[0] = m  -  par[1] = q"""
    return x*(par[0]) + par[1]

def extract_rssi(payload):
    """Extract RSSI from payload"""
    rssi_bytes = payload[12:16]
    rssi = struct.unpack(">f", rssi_bytes)[0]
    return rssi

def extract_snr(payload):
    """Extract SNR from payload"""
    snr_bytes = payload[16:20]
    snr = struct.unpack(">f", snr_bytes)[0]
    return snr
    
# Database initialization
db = jd.init_db()

# Axis vector and variable definition
angle = [0, 0, 0, 0, 0, 30, 30, 30, 30, 30, 45, 45, 45, 45, 45]
angle_mean = [0, 30, 45]
angle_mean_ag = [0, 45, 60, 70, 30]
angle_ag = [0, 0, 0, 0, 0, 45, 45, 45, 45, 45, 60, 60, 60, 60, 60, 70, 70, 70, 70, 70, 30, 30, 30, 30, 30]
RSSI = []
SNR = []
ID = []

PICO_RSSI = []
PICO_SNR = []
PICO_RSSI_ag = []
PICO_SNR_ag = []

conta = 0
RSSI_ag = []
SNR_ag = []

# jd.open_database()

# Searching for data inside database
cursor = db.cursor()
cursor.execute("SELECT id, rssi, snr, payload FROM packets ORDER BY timestamp")

rows = cursor.fetchall()

# Fill vectors with data
for pkt_id, rssi, snr, payload in rows:
    ID.append(pkt_id)

    # Selliting only data for pico plotting
    if(pkt_id >= 37):
        RSSI.append(rssi)
        SNR.append(snr)
        PICO_RSSI.append(extract_rssi(payload))
        PICO_SNR.append(extract_snr(payload))

    elif(pkt_id < 37 and pkt_id >= 12):
        RSSI_ag.append(rssi)
        SNR_ag.append(snr)
        PICO_RSSI_ag.append(extract_rssi(payload))
        PICO_SNR_ag.append(extract_snr(payload))

# Debug
# for i in range(len(PICO_RSSI)):
#     print(PICO_RSSI_ag[i], " ", RSSI_ag[i])

# Debug
# for i in range(len(PICO_RSSI)):
#     print(PICO_SNR_ag[i], " ", SNR_ag[i])

# Computing the mean of RSSI and SNR every five packets
RSSI_mean = []
RSSI_std = []
SNR_mean = []
SNR_std = []

PICO_RSSI_mean = []
PICO_RSSI_std = []
PICO_SNR_mean = []
PICO_SNR_std = []

RSSI_mean_ag = []
RSSI_std_ag = []
SNR_mean_ag = []
SNR_std_ag = []

PICO_RSSI_mean_ag = []
PICO_RSSI_std_ag = []
PICO_SNR_mean_ag = []
PICO_SNR_std_ag = []

for i in range(0, len(RSSI)):
    if i % 5 == 0:
        # print(RSSI[i:i+5])
        # print("-----------")
        RSSI_mean.append(np.mean(RSSI[i:i+5]))
        RSSI_std.append(statistics.stdev(RSSI[i:i+5]))
        SNR_mean.append(np.mean(SNR[i:i+5]))
        SNR_std.append(statistics.stdev(SNR[i:i+5]))
        PICO_RSSI_mean.append(np.mean(PICO_RSSI[i:i+5]))
        PICO_RSSI_std.append(statistics.stdev(PICO_RSSI[i:i+5]))
        PICO_SNR_mean.append(np.mean(PICO_SNR[i:i+5]))
        PICO_SNR_std.append(statistics.stdev(PICO_SNR[i:i+5]))

for i in range(0, len(RSSI_ag)):
    if i % 5 == 0:
        # print(RSSI_ag[i:i+5])
        # print("-----------")
        RSSI_mean_ag.append(np.mean(RSSI_ag[i:i+5]))
        RSSI_std_ag.append(statistics.stdev(RSSI_ag[i:i+5]))
        SNR_mean_ag.append(np.mean(SNR_ag[i:i+5]))
        SNR_std_ag.append(statistics.stdev(SNR_ag[i:i+5]))
        PICO_RSSI_mean_ag.append(np.mean(PICO_RSSI_ag[i:i+5]))
        PICO_RSSI_std_ag.append(statistics.stdev(PICO_RSSI_ag[i:i+5]))
        PICO_SNR_mean_ag.append(np.mean(PICO_SNR_ag[i:i+5]))
        PICO_SNR_std_ag.append(statistics.stdev(PICO_SNR_ag[i:i+5]))

print(PICO_RSSI_mean_ag)

# Converting array in vectors
SNR_std = np.array(SNR_std)
SNR_mean = np.array(SNR_mean)
angle_mean = np.array(angle_mean)
RSSI_std = np.array(RSSI_std)
RSSI_mean = np.array(RSSI_mean)
SNR_std_ag = np.array(SNR_std_ag)
SNR_mean_ag = np.array(SNR_mean_ag)
angle_mean_ag = np.array(angle_mean_ag)
RSSI_std_ag = np.array(RSSI_std_ag)
RSSI_mean_ag = np.array(RSSI_mean_ag)
PICO_RSSI_std = np.array(PICO_RSSI_std)
PICO_RSSI_mean = np.array(PICO_RSSI_mean)
PICO_SNR_std = np.array(PICO_SNR_std)
PICO_SNR_mean = np.array(PICO_SNR_mean)
PICO_RSSI_std_ag = np.array(PICO_RSSI_std_ag)
PICO_RSSI_mean_ag = np.array(PICO_RSSI_mean_ag)
PICO_SNR_std_ag = np.array(PICO_SNR_std_ag)
PICO_SNR_mean_ag = np.array(PICO_SNR_mean_ag)


#region - Single element graph
# Plotting SNR single points
fig, ax = plt.subplots(1, 1, figsize=(6, 6),sharex=True)
ax.errorbar(angle,SNR,xerr = 5, fmt='o', label=r'SNR data [Pico - Pico]',ms=3,color='black', zorder = 2, lw = 1.5)

ax.set_ylabel(r'$SNR \, [db]$', size = 13)
ax.set_xlabel(r'Inclination', size = 13)
ax.set_xlim(-10, 55)
ax.set_ylim(-13, 0)
ax.legend(prop={'size': 14}, loc='upper right', frameon=False).set_zorder(2)

plt.savefig('./python/graph/SNR'+'.png',
            pad_inches = 1,
            transparent = True,
            facecolor ="w",
            edgecolor ='w',
            orientation ='Portrait',
            dpi = 100)

# Fitting SNR means with a linear function
# Create a model for fitting - Function the fitting method is able to read
function_model = Model(fitf)

# Create a RealData object using our initiated data from above
graph_data = RealData(angle_mean, SNR_mean, sx=5, sy=SNR_std)

# Set up ODR with the model and data - ODR is the fitting method used to consider the errors both in x and y
# beta0 are the input parameters
odr = ODR(graph_data, function_model, beta0=[1, -2])

# Start the fitting method
out = odr.run()

# Output of result
out.pprint()

# get parameters and parameters errors from fit 
params = out.beta
params_err = out.sd_beta

# get chi square from fit
chi = out.sum_square

# get the x_distance and y_distance point - fit_function
x_residual = out.delta
y_residual = out.delta

# calculate the residuals error by quadratic sum using the variance theorem
y_residual_err = np.sqrt(pow(SNR_std, 2) + pow(params_err[1], 2) + pow(angle_mean*params_err[0], 2))
x_residual_err = np.sqrt(pow(5, 2) + pow((SNR_mean-params[1])*params_err[1]/(params[0]**2), 2) + pow(params_err[1]/params[0], 2))

# Computing the weighted mean of the residuals
weighted_mean_y_residual = np.average(y_residual, weights=1/y_residual_err**2)
weighted_mean_y_residual_std = np.sqrt(1 / np.sum(1/y_residual_err**2))

# computing compatibility between weighted mean of residuals and 0
r_residual = np.abs(weighted_mean_y_residual)/weighted_mean_y_residual_std

# print the chi square value 
print("CHI SUQARE: " + str(chi))

# Print the weighted mean of residuals
print("Weighted mean of residuals: " + str(weighted_mean_y_residual))

# Array of element for plotting the wheigted mean of residuals
y_residual_array = np.zeros(len(angle_mean), dtype=np.float64)
for i in range(len(angle_mean)):
    y_residual_array[i] = weighted_mean_y_residual

# Getting fit points to plot
x_fit = np.linspace(angle_mean[0]-10, angle_mean[-1]+10, 1000)
y_fit = fitf(params, x_fit)

# Plotting SNR mean points
fig, ax = plt.subplots(1, 1, figsize=(6, 6),sharex=True)
ax.errorbar(angle_mean,SNR_mean,xerr = 5, yerr = SNR_std, fmt='o', label=r'SNR data [Pico - Pico]',ms=3,color='black', zorder = 2, lw = 1.5)
ax.plot(x_fit, y_fit, label=r'SNR linear fit', color='red', lw=1.5, zorder=1)
ax.set_ylabel(r'$SNR \, [db]$', size = 13)
ax.set_xlabel(r'Inclination', size = 13)
ax.set_xlim(-10, 55)
ax.set_ylim(-13, 0)
ax.legend(prop={'size': 14}, loc='upper right', frameon=False).set_zorder(2)

plt.savefig('./python/graph/SNR_mean'+'.png',
            pad_inches = 1,
            transparent = True,
            facecolor ="w",
            edgecolor ='w',
            orientation ='Portrait',
            dpi = 100)

# Plotting RSSI single points
fig, ax = plt.subplots(1, 1, figsize=(6, 6),sharex=True)
ax.errorbar(angle,RSSI,xerr = 5, fmt='o', label=r'RSSI data [Pico - Pico]',ms=3,color='black', zorder = 2, lw = 1.5)

ax.set_ylabel(r'$RSSI \,\, [dbm]$', size = 13)
ax.set_xlabel(r'Inclination', size = 13)
# ax.set_xlim(-10, 55)
# ax.set_ylim(-13, 0)
ax.legend(prop={'size': 14}, loc='upper right', frameon=False).set_zorder(2)

plt.savefig('./python/graph/RSSI'+'.png',
            pad_inches = 1,
            transparent = True,
            facecolor ="w",
            edgecolor ='w',
            orientation ='Portrait',
            dpi = 100)

# Fitting RSSI means with a linear function
# Create a model for fitting - Function the fitting method is able to read
function_model = Model(fitf)

# Create a RealData object using our initiated data from above
graph_data = RealData(angle_mean, RSSI_mean, sx=5, sy=RSSI_std)

# Set up ODR with the model and data - ODR is the fitting method used to consider the errors both in x and y
# beta0 are the input parameters
odr = ODR(graph_data, function_model, beta0=[-10, -104])

# Start the fitting method
out = odr.run()

# Output of result
out.pprint()

# get parameters and parameters errors from fit 
params = out.beta
params_err = out.sd_beta

# get chi square from fit
chi = out.sum_square

# get the x_distance and y_distance point - fit_function
x_residual = out.delta
y_residual = out.delta

# calculate the residuals error by quadratic sum using the variance theorem
y_residual_err = np.sqrt(pow(RSSI_std, 2) + pow(params_err[1], 2) + pow(angle_mean*params_err[0], 2))
x_residual_err = np.sqrt(pow(5, 2) + pow((RSSI_mean-params[1])*params_err[1]/(params[0]**2), 2) + pow(params_err[1]/params[0], 2))

# Computing the weighted mean of the residuals
weighted_mean_y_residual = np.average(y_residual, weights=1/y_residual_err**2)
weighted_mean_y_residual_std = np.sqrt(1 / np.sum(1/y_residual_err**2))

# computing compatibility between weighted mean of residuals and 0
r_residual = np.abs(weighted_mean_y_residual)/weighted_mean_y_residual_std

# print the chi square value 
print("CHI SUQARE: " + str(chi))

# Print the weighted mean of residuals
print("Weighted mean of residuals: " + str(weighted_mean_y_residual))

# Array of element for plotting the wheigted mean of residuals
y_residual_array = np.zeros(len(angle_mean), dtype=np.float64)
for i in range(len(angle_mean)):
    y_residual_array[i] = weighted_mean_y_residual

# Getting fit points to plot
x_fit = np.linspace(angle_mean[0]-10, angle_mean[-1]+10, 1000)
y_fit = fitf(params, x_fit)

# Plotting RSSI mean points
fig, ax = plt.subplots(1, 1, figsize=(6, 6),sharex=True)
ax.errorbar(angle_mean,RSSI_mean,xerr = 5, yerr = RSSI_std, fmt='o', label=r'RSSI data [Pico - Pico]',ms=3,color='black', zorder = 2, lw = 1.5)
ax.plot(x_fit, y_fit, label=r'RSSI linear fit', color='darkorange', lw=1.5, zorder=1)

ax.set_ylabel(r'$RSSI \,\, [db]$', size = 13)
ax.set_xlabel(r'Inclination', size = 13)
# ax.set_xlim(-10, 55)
# ax.set_ylim(-13, 0)
ax.legend(prop={'size': 14}, loc='upper right', frameon=False).set_zorder(2)

plt.savefig('./python/graph/RSSI_mean'+'.png',
            pad_inches = 1,
            transparent = True,
            facecolor ="w",
            edgecolor ='w',
            orientation ='Portrait',
            dpi = 100)

# endregion

#region - Mean graphs(data from gs - Monte Ceva)
diff = np.zeros(len(SNR_mean), dtype=np.float64)
diff_std = np.zeros(len(SNR_mean), dtype=np.float64)
conta = 0

for i in range(len(SNR_mean)):
    for k in range(len(SNR_mean_ag)):
        if angle_mean_ag[k] == angle_mean[i]:
            diff[conta] = (SNR_mean_ag[k] - SNR_mean[i])
            diff_std[conta] = np.sqrt(pow(SNR_std_ag[k], 2) + pow(SNR_std[i], 2))
            conta = conta + 1
            break

# Plotting SNR pico-pico e pico-high gain(with another graph for delta)
fig, axx = plt.subplots(2, 1, figsize=(6, 6),sharex=True, height_ratios=[2, 1])
fig.suptitle("DATA FROM GS - M. Ceva", fontsize=13, fontweight = 2)
axx[0].errorbar(angle_mean,SNR_mean,xerr = 5, yerr = SNR_std, fmt='o', label=r'SNR data [Pico - Pico]',ms=3,color='black', zorder = 2, lw = 1.5)
axx[0].errorbar(angle_mean_ag,SNR_mean_ag,xerr = 5, yerr = SNR_std_ag, fmt='o', label=r'SNR data [Pico - High gain]',ms=3,color='darkorange', zorder = 2, lw = 1.5)

axx[0].set_ylabel(r'$SNR \, [db]$', size = 13)
axx[0].set_xlabel(r'Inclination', size = 13)
axx[1].set_ylabel(r'$\Delta  \, [db]$', size = 13)
axx[1].set_xlabel(r'Inclination', size = 13)

axx[1].errorbar(angle_mean,diff,xerr = 5, yerr =diff_std, fmt='o', label=r'$\Delta \,$SNR [P/H - P/P]',ms=3,color='darkred', zorder = 2, lw = 1.5)

axx[0].set_xlim(-10, 82)
axx[0].set_ylim(-20, 2)
axx[1].set_xlim(-10, 82)
axx[1].set_ylim(-9, 5)

axx[0].text(70, -13.5, r'15 db', fontsize=12, color='black', ha='center', va='center')

axx[0].legend(prop={'size': 13}, loc='upper right', frameon=False).set_zorder(2)
axx[1].legend(prop={'size': 11}, loc='lower right', frameon=False).set_zorder(2)

plt.savefig('./python/graph/SNR_conf'+'.png',
            pad_inches = 1,
            transparent = True,
            facecolor ="w",
            edgecolor ='w',
            orientation ='Portrait',
            dpi = 100)


diff = np.zeros(len(RSSI_mean), dtype=np.float64)
diff_std = np.zeros(len(RSSI_mean), dtype=np.float64)
conta = 0

for i in range(len(RSSI_mean)):
    for k in range(len(RSSI_mean_ag)):
        if angle_mean_ag[k] == angle_mean[i]:
            diff[conta] = (RSSI_mean_ag[k] - RSSI_mean[i])
            diff_std[conta] = np.sqrt(pow(RSSI_std_ag[k], 2) + pow(RSSI_std[i], 2))
            conta = conta + 1
            break

# Plotting RSSI pico-pico e pico-high gain(with another graph for delta)
fig, axx = plt.subplots(2, 1, figsize=(6, 6),sharex=True, height_ratios=[2, 1])
fig.suptitle("DATA FROM GS - M. Ceva", fontsize=13, fontweight = 2)
axx[0].errorbar(angle_mean,RSSI_mean,xerr = 5, yerr = RSSI_std, fmt='o', label=r'RSSI data [Pico - Pico]',ms=3,color='black', zorder = 2, lw = 1.5)
axx[0].errorbar(angle_mean_ag,RSSI_mean_ag,xerr = 5, yerr = RSSI_std_ag, fmt='o', label=r'RSSI data [Pico - High gain]',ms=3,color='darkorange', zorder = 2, lw = 1.5)

axx[0].set_ylabel(r'$RSSI \, [dbm]$', size = 13)
axx[0].set_xlabel(r'Inclination', size = 13)
axx[1].set_ylabel(r'$\Delta  \, [dbm]$', size = 13)
axx[1].set_xlabel(r'Inclination', size = 13)

axx[1].errorbar(angle_mean,diff,xerr = 5, yerr =diff_std, fmt='o', label=r'$\Delta \,$RSSI [P/H - P/P]',ms=3,color='darkred', zorder = 2, lw = 1.5)

axx[0].set_xlim(-10, 82)
axx[0].set_ylim(-115, -100)
axx[1].set_xlim(-10, 82)
axx[1].set_ylim(-4, 10)

axx[0].text(70, -111, r'15 db', fontsize=12, color='black', ha='center', va='center')

axx[0].legend(prop={'size': 13}, loc='upper right', frameon=False).set_zorder(2)
axx[1].legend(prop={'size': 11}, loc='lower right', frameon=False).set_zorder(2)

plt.savefig('./python/graph/RSSI_conf'+'.png',
            pad_inches = 1,
            transparent = True,
            facecolor ="w",
            edgecolor ='w',
            orientation ='Portrait',
            dpi = 100)

# Plotting SNR_ag single points
fig, ax = plt.subplots(1, 1, figsize=(6, 6), sharex=True)
ax.errorbar(angle_ag, SNR_ag, xerr=5, fmt='o', label=r'SNR_ag data [Pico - High gain]', ms=3, color='darkorange', zorder=2, lw=1.5)

ax.set_ylabel(r'$SNR \, [db]$', size=13)
ax.set_xlabel(r'Inclination', size=13)
ax.set_xlim(-10, 82)
ax.set_ylim(-20, -2)
ax.legend(prop={'size': 14}, loc='upper right', frameon=False).set_zorder(2)

plt.savefig('./python/graph/SNR_ag'+'.png',
            pad_inches=1,
            transparent=True,
            facecolor="w",
            edgecolor='w',
            orientation='Portrait',
            dpi=100)

# Plotting RSSI_ag single points
fig, ax = plt.subplots(1, 1, figsize=(6, 6), sharex=True)
ax.errorbar(angle_ag, RSSI_ag, xerr=5, fmt='o', label=r'RSSI_ag data [Pico - High gain]', ms=3, color='darkorange', zorder=2, lw=1.5)

ax.set_ylabel(r'$RSSI \, [dbm]$', size=13)
ax.set_xlabel(r'Inclination', size=13)
ax.set_xlim(-10, 82)
ax.set_ylim(-115, -100)
ax.legend(prop={'size': 14}, loc='upper right', frameon=False).set_zorder(2)

plt.savefig('./python/graph/RSSI_ag'+'.png',
            pad_inches=1,
            transparent=True,
            facecolor="w",
            edgecolor='w',
            orientation='Portrait',
            dpi=100)

# Computing gain
gain_SNR = np.zeros(len(SNR_mean), dtype=np.float64)
gain_SNR_ag = np.zeros(len(SNR_mean_ag), dtype=np.float64)
gain_SNR_std = np.zeros(len(SNR_mean), dtype=np.float64)
gain_SNR_ag_std = np.zeros(len(SNR_mean_ag), dtype=np.float64)

for i in range(len(gain_SNR)):
    gain_SNR[i] = SNR_mean[i] - SNR_mean[0]
    gain_SNR_std[i] = np.sqrt(pow(SNR_std[0], 2) + pow(SNR_std[i], 2))

for i in range(len(gain_SNR_ag)):
    gain_SNR_ag[i] = SNR_mean_ag[i] - SNR_mean_ag[0]
    gain_SNR_ag_std[i] = np.sqrt(pow(SNR_std_ag[0], 2) + pow(SNR_std_ag[i], 2))

# Computing the difference between the gain of pico and antenna
gain_diff = np.zeros(len(SNR_mean), dtype=np.float64)
gain_diff_std = np.zeros(len(SNR_mean), dtype=np.float64)

# Resetting counter
conta = 0

for i in range(len(SNR_mean)):
    for k in range(len(SNR_mean_ag)):
        if angle_mean_ag[k] == angle_mean[i]:
            gain_diff[conta] = (gain_SNR_ag[k] - gain_SNR[i])
            gain_diff_std[conta] = np.sqrt(pow(gain_SNR_ag_std[k], 2) + pow(gain_SNR_std[i], 2))
            conta = conta + 1
            break

# Plotting gain and gain difference
fig, axx = plt.subplots(2, 1, figsize=(6, 6),sharex=True, height_ratios=[2, 1])
fig.suptitle("DATA FROM GS - M. Ceva", fontsize=13, fontweight = 2)
axx[0].errorbar(angle_mean,gain_SNR,xerr = 5, yerr = gain_SNR_std, fmt='o', label=r'SNR gain [Pico - Pico]',ms=3,color='black', zorder = 2, lw = 1.5)
axx[0].errorbar(angle_mean_ag,gain_SNR_ag,xerr = 5, yerr = gain_SNR_ag_std, fmt='o', label=r'SNR gain [Pico - High gain]',ms=3,color='deepskyblue', zorder = 1, lw = 1.5)

axx[0].set_ylabel(r'$Gain \, [db]$', size = 13)
axx[0].set_xlabel(r'Inclination', size = 13)
axx[1].set_ylabel(r'$\Delta  \, [db]$', size = 13)
axx[1].set_xlabel(r'Inclination', size = 13)

axx[1].errorbar(angle_mean,gain_diff,xerr = 5, yerr = gain_diff_std, fmt='o', label=r'$\Delta \, $ Gain [P/H - P/P]',ms=3,color='darkblue', zorder = 2, lw = 1.5)

axx[0].set_xlim(-18, 82)
axx[0].set_ylim(-15, 3)
axx[1].set_xlim(-10, 82)
axx[1].set_ylim(-4, 12)

axx[0].text(70, -11.9, r'15 db', fontsize=12, color='black', ha='center', va='center')
axx[0].text(0, -2.5, r'Same', fontsize=12, color='black', ha='center', va='center')

axx[0].legend(prop={'size': 13}, loc='lower left', frameon=False).set_zorder(2)
axx[1].legend(prop={'size': 13}, loc='lower right', frameon=False).set_zorder(2)

plt.savefig('./python/graph/SNR_gain'+'.png',
            pad_inches = 1,
            transparent = True,
            facecolor ="w",
            edgecolor ='w',
            orientation ='Portrait',
            dpi = 100)

#endregion

#region - Mean graphs(data from pico - Noventa)

# Computing the difference between the pico-pico and pico-high gain
diff = np.zeros(len(PICO_SNR_mean), dtype=np.float64)
diff_std = np.zeros(len(PICO_SNR_mean), dtype=np.float64)
conta = 0

for i in range(len(PICO_SNR_mean)):
    for k in range(len(PICO_SNR_mean_ag)):
        if angle_mean_ag[k] == angle_mean[i]:
            diff[conta] = (PICO_SNR_mean_ag[k] - PICO_SNR_mean[i])
            diff_std[conta] = np.sqrt(pow(PICO_SNR_std_ag[k], 2) + pow(PICO_SNR_std[i], 2))
            conta = conta + 1
            break

# Plotting PICO_SNR pico-pico e pico-high gain(with another graph for delta)
fig, axx = plt.subplots(2, 1, figsize=(6, 6), sharex=True, height_ratios=[2, 1])
fig.suptitle("DATA FROM PICO - Noventa", fontsize=13, fontweight = 2)
axx[0].errorbar(angle_mean, PICO_SNR_mean, xerr=5, yerr=PICO_SNR_std, fmt='o', label=r'PICO_SNR data [Pico - Pico]', ms=3, color='black', zorder=2, lw=1.5)
axx[0].errorbar(angle_mean_ag, PICO_SNR_mean_ag, xerr=5, yerr=PICO_SNR_std_ag, fmt='o', label=r'PICO_SNR data [Pico - High gain]', ms=3, color='darkorange', zorder=2, lw=1.5)

axx[0].set_ylabel(r'$PICO\_SNR \, [db]$', size=13)
axx[0].set_xlabel(r'Inclination', size=13)
axx[1].set_ylabel(r'$\Delta  \, [db]$', size=13)
axx[1].set_xlabel(r'Inclination', size=13)

axx[1].errorbar(angle_mean, diff, xerr=5, yerr=diff_std, fmt='o', label=r'$\Delta \,$PICO\_SNR [P/H - P/P]', ms=3, color='darkred', zorder=2, lw=1.5)

axx[0].set_xlim(-10, 82)
axx[0].set_ylim(-12.5, 8.5)
axx[1].set_xlim(-10, 82)
axx[1].set_ylim(2.5, 15)

axx[0].text(70, -9, r'15 db', fontsize=12, color='black', ha='center', va='center')

axx[0].legend(prop={'size': 13}, loc='upper right', frameon=False).set_zorder(2)
axx[1].legend(prop={'size': 11}, loc='upper right', frameon=False).set_zorder(2)

plt.savefig('./python/graph/PICO_SNR_conf'+'.png',
            pad_inches=1,
            transparent=True,
            facecolor="w",
            edgecolor='w',
            orientation='Portrait',
            dpi=100)

diff = np.zeros(len(PICO_RSSI_mean), dtype=np.float64)
diff_std = np.zeros(len(PICO_RSSI_mean), dtype=np.float64)
conta = 0

for i in range(len(PICO_RSSI_mean)):
    for k in range(len(PICO_RSSI_mean_ag)):
        if angle_mean_ag[k] == angle_mean[i]:
            diff[conta] = (PICO_RSSI_mean_ag[k] - PICO_RSSI_mean[i])
            diff_std[conta] = np.sqrt(pow(PICO_RSSI_std_ag[k], 2) + pow(PICO_RSSI_std[i], 2))
            conta = conta + 1
            break

# Plotting PICO_RSSI pico-pico e pico-high gain(with another graph for delta)
fig, axx = plt.subplots(2, 1, figsize=(6, 6), sharex=True, height_ratios=[2, 1])
fig.suptitle("DATA FROM PICO - Noventa", fontsize=13, fontweight = 2)
axx[0].errorbar(angle_mean, PICO_RSSI_mean, xerr=5, yerr=PICO_RSSI_std, fmt='o', label=r'PICO\_RSSI data [Pico - Pico]', ms=3, color='black', zorder=2, lw=1.5)
axx[0].errorbar(angle_mean_ag, PICO_RSSI_mean_ag, xerr=5, yerr=PICO_RSSI_std_ag, fmt='o', label=r'PICO\_RSSI data [Pico - High gain]', ms=3, color='darkorange', zorder=2, lw=1.5)

axx[0].set_ylabel(r'$RSSI \, [dbm]$', size=13)
axx[0].set_xlabel(r'Inclination', size=13)
axx[1].set_ylabel(r'$\Delta  \, [dbm]$', size=13)
axx[1].set_xlabel(r'Inclination', size=13)

axx[1].errorbar(angle_mean, diff, xerr=5, yerr=diff_std, fmt='o', label=r'$\Delta \,$PICO\_RSSI [P/H - P/P]', ms=3, color='darkred', zorder=2, lw=1.5)

axx[0].set_xlim(-10, 82)
axx[0].set_ylim(-117.5, -92.5)
axx[1].set_xlim(-10, 82)
axx[1].set_ylim(6, 18)

axx[0].text(70, -107, r'15 db', fontsize=12, color='black', ha='center', va='center')

axx[0].legend(prop={'size': 13}, loc='upper right', frameon=False).set_zorder(2)
axx[1].legend(prop={'size': 11}, loc='upper left', frameon=False).set_zorder(2)

plt.savefig('./python/graph/PICO_RSSI_conf'+'.png',
            pad_inches=1,
            transparent=True,
            facecolor="w",
            edgecolor='w',
            orientation='Portrait',
            dpi=100)

# Computing gain
gain_PICO_SNR = np.zeros(len(PICO_SNR_mean), dtype=np.float64)
gain_PICO_SNR_ag = np.zeros(len(PICO_SNR_mean_ag), dtype=np.float64)
gain_PICO_SNR_std = np.zeros(len(PICO_SNR_mean), dtype=np.float64)
gain_PICO_SNR_ag_std = np.zeros(len(PICO_SNR_mean_ag), dtype=np.float64)

for i in range(len(gain_PICO_SNR)):
    gain_PICO_SNR[i] = PICO_SNR_mean[i] - PICO_SNR_mean[0]
    gain_PICO_SNR_std[i] = np.sqrt(pow(PICO_SNR_std[0], 2) + pow(PICO_SNR_std[i], 2))

for i in range(len(gain_PICO_SNR_ag)):
    gain_PICO_SNR_ag[i] = PICO_SNR_mean_ag[i] - PICO_SNR_mean_ag[0]
    gain_PICO_SNR_ag_std[i] = np.sqrt(pow(PICO_SNR_std_ag[0], 2) + pow(PICO_SNR_std_ag[i], 2))

# Computing the difference between the gain of pico and antenna
gain_diff = np.zeros(len(PICO_SNR_mean), dtype=np.float64)
gain_diff_std = np.zeros(len(PICO_SNR_mean), dtype=np.float64)

# Resetting counter
conta = 0

for i in range(len(PICO_SNR_mean)):
    for k in range(len(PICO_SNR_mean_ag)):
        if angle_mean_ag[k] == angle_mean[i]:
            gain_diff[conta] = (gain_PICO_SNR_ag[k] - gain_PICO_SNR[i])
            gain_diff_std[conta] = np.sqrt(pow(gain_PICO_SNR_ag_std[k], 2) + pow(gain_PICO_SNR_std[i], 2))
            conta = conta + 1
            break

# Plotting gain and gain difference
fig, axx = plt.subplots(2, 1, figsize=(6, 6), sharex=True, height_ratios=[2, 1])
fig.suptitle("DATA FROM PICO - Noventa", fontsize=13, fontweight = 2)
axx[0].errorbar(angle_mean, gain_PICO_SNR, xerr=5, yerr=gain_PICO_SNR_std, fmt='o', label=r'SNR gain [Pico - Pico]', ms=3, color='black', zorder=2, lw=1.5)
axx[0].errorbar(angle_mean_ag, gain_PICO_SNR_ag, xerr=5, yerr=gain_PICO_SNR_ag_std, fmt='o', label=r'SNR gain [Pico - High gain]', ms=3, color='deepskyblue', zorder=1, lw=1.5)

axx[0].set_ylabel(r'$Gain \, [db]$', size=13)
axx[0].set_xlabel(r'Inclination', size=13)
axx[1].set_ylabel(r'$\Delta  \, [db]$', size=13)
axx[1].set_xlabel(r'Inclination', size=13)

axx[1].errorbar(angle_mean, gain_diff, xerr=5, yerr=gain_diff_std, fmt='o', label=r'$\Delta \, $ Gain [P/H - P/P]', ms=3, color='darkblue', zorder=2, lw=1.5)

axx[0].set_xlim(-18, 82)
axx[0].set_ylim(-15, 3)
axx[1].set_xlim(-10, 82)
axx[1].set_ylim(-7, 7)

axx[0].text(70, -5.8, r'15 db', fontsize=12, color='black', ha='center', va='center')
axx[0].text(0, -2.5, r'Same', fontsize=12, color='black', ha='center', va='center')

axx[0].legend(prop={'size': 13}, loc='lower left', frameon=False).set_zorder(2)
axx[1].legend(prop={'size': 13}, loc='upper right', frameon=False).set_zorder(2)

plt.savefig('./python/graph/PICO_SNR_gain'+'.png',
            pad_inches=1,
            transparent=True,
            facecolor="w",
            edgecolor='w',
            orientation='Portrait',
            dpi=100)

#endregion

#region - Data comparason 

# Plotting SNR pico-pico from GS and PICO and SNR pico-high gain from GS and PICO
fig, axx = plt.subplots(1, 1, figsize=(6, 6), sharex=True)
fig.suptitle("SNR DATA", fontsize=13, fontweight = 2)
axx.errorbar(angle_mean, PICO_SNR_mean, xerr=5, yerr=PICO_SNR_std, fmt='o', label=r'PICO - Noventa [Pico - Pico]', ms=3, color='orange', zorder=2, lw=1.5)
axx.errorbar(angle_mean_ag, PICO_SNR_mean_ag, xerr=5, yerr=PICO_SNR_std_ag, fmt='o', label=r'PICO - Noventa [Pico - High gain]', ms=3, color='black', zorder=2, lw=1.5)
axx.errorbar(angle_mean, SNR_mean, xerr=5, yerr=SNR_std, fmt='o', label=r'GS - M. Ceva [Pico - Pico]', ms=3, color='firebrick', zorder=2, lw=1.5)
axx.errorbar(angle_mean_ag, SNR_mean_ag, xerr=5, yerr=SNR_std_ag, fmt='o', label=r'GS - M. Ceva [Pico - High gain]', ms=3, color='forestgreen', zorder=1, lw=1.5)

axx.set_ylabel(r'$SNR \, [db]$', size=13)
axx.set_xlabel(r'Inclination', size=13)

axx.set_xlim(-10, 82)
axx.set_ylim(-18.5, 11)

axx.text(70, -9, r'15 db', fontsize=12, color='black', ha='center', va='center')

axx.legend(prop={'size': 13}, loc='upper right', frameon=False).set_zorder(2)

plt.savefig('./python/graph/SNR_comp'+'.png',
            pad_inches=1,
            transparent=True,
            facecolor="w",
            edgecolor='w',
            orientation='Portrait',
            dpi=100)

# Plotting RSSI pico-pico from GS and PICO and SNR pico-high gain from GS and PICO
fig, axx = plt.subplots(1, 1, figsize=(6, 6), sharex=True)
fig.suptitle("RSSI DATA", fontsize=13, fontweight = 2)
axx.errorbar(angle_mean, PICO_RSSI_mean, xerr=5, yerr=PICO_RSSI_std, fmt='o', label=r'PICO - Noventa [Pico - Pico]', ms=3, color='darkorange', zorder=2, lw=1.5)
axx.errorbar(angle_mean_ag, PICO_RSSI_mean_ag, xerr=5, yerr=PICO_RSSI_std_ag, fmt='o', label=r'PICO - Noventa [Pico - High gain]', ms=3, color='black', zorder=2, lw=1.5)
axx.errorbar(angle_mean, RSSI_mean, xerr=5, yerr=RSSI_std, fmt='o', label=r'GS - M. Ceva [Pico - Pico]', ms=3, color='deepskyblue', zorder=1, lw=1.5)
axx.errorbar(angle_mean_ag, RSSI_mean_ag, xerr=5, yerr=RSSI_std_ag, fmt='o', label=r'GS - M. Ceva [Pico - High gain]', ms=3, color='mediumblue', zorder=2, lw=1.5)

axx.set_ylabel(r'$RSSI \, [dbm]$', size=13)
axx.set_xlabel(r'Inclination', size=13)

axx.set_xlim(-10, 82)
axx.set_ylim(-117.5, -93)

axx.text(70, -116, r'15 db', fontsize=12, color='black', ha='center', va='center')

axx.legend(prop={'size': 13}, loc='upper right', frameon=False).set_zorder(2)

plt.savefig('./python/graph/RSSI_comp'+'.png',
            pad_inches=1,
            transparent=True,
            facecolor="w",
            edgecolor='w',
            orientation='Portrait',
            dpi=100)

plt.show()