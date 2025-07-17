import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# Einfacher 1D-Kalman-Filter
class Kalman1D:
    def __init__(self, process_variance=1e-3, measurement_variance=1.0):
        self.process_variance = process_variance
        self.measurement_variance = measurement_variance
        self.estimated_value = 0.0
        self.error_estimate = 1.0

    def update(self, measurement):
        kalman_gain = self.error_estimate / (self.error_estimate + self.measurement_variance)
        self.estimated_value += kalman_gain * (measurement - self.estimated_value)
        self.error_estimate = (1 - kalman_gain) * self.error_estimate + self.process_variance
        return self.estimated_value

# CSV einlesen
df = pd.read_csv("log_0034.csv")
df["time_s"] = df["time_us"] / 1_000_000

# Kalman-Filter für alle Spalten
filters = {
    "temp_C": Kalman1D(),
    "hum_pct": Kalman1D(),
    "press_hPa": Kalman1D(),
    "altitude_m": Kalman1D(),
    "accelX": Kalman1D(),
    "accelY": Kalman1D(),
    "accelZ": Kalman1D()
}

# Glätten mit Kalman-Filter
for col in filters:
    df[f"{col}_filtered"] = df[col].apply(filters[col].update)

# Gesamtbeschleunigung aus gefilterten Werten
df["accel_total"] = np.sqrt(
    df["accelX_filtered"]**2 +
    df["accelY_filtered"]**2 +
    df["accelZ_filtered"]**2
)

df = df[(df["time_s"] >= 40) & (df["time_s"] <= 80)]

# Plot
fig, ax1 = plt.subplots(figsize=(14, 8))


# color1 = 'tab:red'
# ax1.set_xlabel("Zeit (s)")
# ax1.set_ylabel("Temperatur (°C)", color=color1)
# ln1 = ax1.plot(df["time_s"], df["temp_C_filtered"], color=color1, label="Temperatur (°C)")
# ax1.tick_params(axis='y', labelcolor=color1)

# ax3 = ax1.twinx()
# color3 = 'tab:blue'
# ax3.spines["right"].set_position(("outward", 60))
# ax3.set_ylabel("Luftfeuchte (%)", color=color3)
# ln3 = ax3.plot(df["time_s"], df["hum_pct_filtered"], color=color3, label="Luftfeuchte (%)")
# ax3.tick_params(axis='y', labelcolor=color3)

# ax4 = ax1.twinx()
# color4 = 'tab:green'
# ax4.spines["right"].set_position(("outward", 120))
# ax4.set_ylabel("Druck (hPa)", color=color4)
# ln4 = ax4.plot(df["time_s"], df["press_hPa_filtered"], color=color4, label="Druck (hPa)")
# ax4.tick_params(axis='y', labelcolor=color4)

ax5 = ax1.twinx()
color5 = 'tab:purple'
ax5.spines["right"].set_position(("outward", 180))
ax5.set_ylabel("Höhe (m)", color=color5)
ln5 = ax5.plot(df["time_s"], df["altitude_m_filtered"], color=color5, label="Höhe (m)")
ax5.tick_params(axis='y', labelcolor=color5)

ax6 = ax1.twinx()
color6 = 'tab:gray'
ax6.spines["right"].set_position(("outward", 240))
ax6.set_ylabel("Gesamtbeschleunigung (mg)", color=color6)
ln6 = ax6.plot(df["time_s"], df["accel_total"], color=color6, label="Beschl. gesamt (mg)")
ax6.tick_params(axis='y', labelcolor=color6)

plt.title("Sensorverlauf mit Kalman-gefilterten Werten")
plt.tight_layout()
plt.show()
