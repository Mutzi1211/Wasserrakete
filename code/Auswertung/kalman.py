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

# Filter für jede Achse
kf_x = Kalman1D()
kf_y = Kalman1D()
kf_z = Kalman1D()

filtered_accelX = df["accelX"].apply(kf_x.update)
filtered_accelY = df["accelY"].apply(kf_y.update)
filtered_accelZ = df["accelZ"].apply(kf_z.update)

# Gesamtbeschleunigung aus geglätteten Werten
df["accel_total"] = np.sqrt(filtered_accelX**2 + filtered_accelY**2 + filtered_accelZ**2)

# Plot
fig, ax1 = plt.subplots(figsize=(14, 8))

color1 = 'tab:red'
ax1.set_xlabel("Zeit (s)")
ax1.set_ylabel("Temperatur (°C)", color=color1)
ln1 = ax1.plot(df["time_s"], df["temp_C"], color=color1, label="Temperatur (°C)")
ax1.tick_params(axis='y', labelcolor=color1)

# ax2 = ax1.twinx()
# color2 = 'tab:blue'
# ax2.set_ylabel("Luftfeuchte (%)", color=color2)
# ln2 = ax2.plot(df["time_s"], df["hum_pct"], color=color2, label="Luftfeuchte (%)")
# ax2.tick_params(axis='y', labelcolor=color2)

ax3 = ax1.twinx()
color3 = 'tab:green'
ax3.spines["right"].set_position(("outward", 60))
ax3.set_ylabel("Luftdruck (hPa)", color=color3)
ln3 = ax3.plot(df["time_s"], df["press_hPa"], color=color3, label="Druck (hPa)")
ax3.tick_params(axis='y', labelcolor=color3)

ax4 = ax1.twinx()
color4 = 'tab:purple'
ax4.spines["right"].set_position(("outward", 120))
ax4.set_ylabel("Höhe (m)", color=color4)
ln4 = ax4.plot(df["time_s"], df["altitude_m"], color=color4, label="Höhe (m)")
ax4.tick_params(axis='y', labelcolor=color4)

ax5 = ax1.twinx()
color5 = 'tab:gray'
ax5.spines["right"].set_position(("outward", 180))
ax5.set_ylabel("Gesamtbeschleunigung (mg)", color=color5)
ln5 = ax5.plot(df["time_s"], df["accel_total"], color=color5, label="Beschl. gesamt (mg)")
ax5.tick_params(axis='y', labelcolor=color5)


plt.title("Sensorverlauf mit Kalman-gefilterter Beschleunigung")
plt.tight_layout()
plt.show()
