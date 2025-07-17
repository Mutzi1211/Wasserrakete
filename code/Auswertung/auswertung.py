



import csv

def csv_to_dict(filename):
    with open(filename) as f:
        reader = csv.reader(f)
        data = list(reader)
    header = data[0]
    content = data[1:]
    result = {h: [c[i] for c in content] for i, h in enumerate(header)}
    return result



datas = []

for i in range(1,46):
    file_name = "log_{:04d}.csv".format(i)
    try:
        datas.append(csv_to_dict(file_name))
    except:
        print(f"Skipped: {file_name}")





for i,data in enumerate(datas,1):
    height = data["altitude_m"]
    print(f"Data Set {i}:")
    print(f"Max height: {max(height)}")
    print(f"Min height: {min(height)}")
    diff = float(max(height))- float(min(height))
    print(f"diff: {diff:.2f}")
    print("")




import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# CSV-Datei einlesen – passe ggf. den Dateinamen an
df = pd.read_csv("log_0034.csv")  # <-- CSV-Dateiname hier anpassen

# Zeit in Sekunden umrechnen
df["time_s"] = df["time_us"] / 1_000_000

# Gesamtbeschleunigung berechnen (in mg)
df["accel_total"] = np.sqrt(df["accelX"]**2 + df["accelY"]**2 + df["accelZ"]**2)

# Hauptplot
fig, ax1 = plt.subplots(figsize=(14, 8))

# # Temperatur
# color1 = 'tab:red'
# ax1.set_xlabel("Zeit (s)")
# ax1.set_ylabel("Temperatur (°C)", color=color1)
# ln1 = ax1.plot(df["time_s"], df["temp_C"], color=color1, label="Temperatur (°C)")
# ax1.tick_params(axis='y', labelcolor=color1)

# # 2. Achse: Luftfeuchte
# ax2 = ax1.twinx()
# color2 = 'tab:blue'
# ax2.set_ylabel("Luftfeuchte (%)", color=color2)
# ln2 = ax2.plot(df["time_s"], df["hum_pct"], color=color2, label="Luftfeuchte (%)")
# ax2.tick_params(axis='y', labelcolor=color2)

# # 3. Achse: Luftdruck
# ax3 = ax1.twinx()
# color3 = 'tab:green'
# ax3.spines["right"].set_position(("outward", 60))
# ax3.set_ylabel("Luftdruck (hPa)", color=color3)
# ln3 = ax3.plot(df["time_s"], df["press_hPa"], color=color3, label="Druck (hPa)")
# ax3.tick_params(axis='y', labelcolor=color3)

# 4. Achse: Höhe
ax4 = ax1.twinx()
color4 = 'tab:purple'
ax4.spines["right"].set_position(("outward", 120))
ax4.set_ylabel("Höhe (m)", color=color4)
ln4 = ax4.plot(df["time_s"], df["altitude_m"], color=color4, label="Höhe (m)")
ax4.tick_params(axis='y', labelcolor=color4)

# 5. Achse: Gesamtbeschleunigung
ax5 = ax1.twinx()
color5 = 'tab:gray'
ax5.spines["right"].set_position(("outward", 180))
ax5.set_ylabel("Gesamtbeschleunigung (mg)", color=color5)
ln5 = ax5.plot(df["time_s"], df["accel_total"], color=color5, label="Beschl. gesamt (mg)")
ax5.tick_params(axis='y', labelcolor=color5)

# # Legende
# all_lines = ln1 + ln2 + ln3 + ln4 + ln5
# labels = [l.get_label() for l in all_lines]
# # ax1.legend(all_lines, labels, loc="upper left", fontsize="small")

plt.title("Sensorverlauf – Mehrfachachsen mit Gesamtbeschleunigung")
plt.tight_layout()
plt.show()
