#!/usr/bin/env python3
# rocket_flight_plot.py

import argparse
import pandas as pd
import numpy as np
from scipy.integrate import cumulative_trapezoid as cumtrapz
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

def main():
    parser = argparse.ArgumentParser(description="Plot rocket flight from CSV und exportiere berechnete Daten")
    parser.add_argument('input_csv', help="Pfad zur CSV-Datei mit den Rohdaten")
    parser.add_argument('-o', '--output_csv', default='flight_data_processed.csv',
                        help="Pfad zur Ausgabedatei für die berechneten Daten (default: %(default)s)")
    args = parser.parse_args()

    # 1) CSV einlesen
    df = pd.read_csv(args.input_csv)

    # 2) Rohdaten extrahieren
    t         = df['time_s'].values
    acc_x     = df['acc_x'].values
    acc_y     = df['acc_y'].values
    acc_z     = df['acc_z'].values
    altitude  = df['altitude_m'].values

    # 3) Integration: Beschleunigung -> Geschwindigkeit
    v_x = cumtrapz(acc_x, t, initial=0)
    v_y = cumtrapz(acc_y, t, initial=0)
    v_z = cumtrapz(acc_z, t, initial=0)

    # 4) Integration: Geschwindigkeit -> Position (X, Y)
    x = cumtrapz(v_x, t, initial=0)
    y = cumtrapz(v_y, t, initial=0)
    # Z = gemessene Höhe
    z = altitude

    # 5) Daten in DataFrame für Export sammeln
    df_out = pd.DataFrame({
        'time_s':        t,
        'acc_x_m_s2':    acc_x,
        'acc_y_m_s2':    acc_y,
        'acc_z_m_s2':    acc_z,
        'v_x_m_s':       v_x,
        'v_y_m_s':       v_y,
        'v_z_m_s':       v_z,
        'x_pos_m':       x,
        'y_pos_m':       y,
        'z_pos_m':       z,
        'altitude_m':    altitude,
    })

    # 6) Export in CSV
    # df_out.to_csv(args.output_csv, index=False)
    # print(f"Berechnete Daten gespeichert in: {args.output_csv}")

    # --- 2D-Plot: Höhe vs. Zeit mit Markern ---
    plt.figure(figsize=(10, 6))
    plt.plot(t, altitude, linestyle='-', marker='o', markersize=3, label='Altitude (m)')    # type: ignore
    plt.xlabel('Zeit (s)')
    plt.ylabel('Höhe (m)')
    plt.title('Flugkurve: Höhe vs. Zeit')
    plt.grid(True)
    plt.legend()

    # --- 3D-Plot: Flugtrajektorie mit Markern ---
    fig = plt.figure(figsize=(10, 8))
    ax = fig.add_subplot(111, projection='3d')
    ax.plot(x, y, z, linestyle='-', marker='o', markersize=2, label='Flugbahn')     # type: ignore
    ax.set_xlabel('X-Position (m)')
    ax.set_ylabel('Y-Position (m)')
    ax.set_zlabel('Höhe (m)')       # type: ignore
    ax.set_title('3D-Flugtrajektorie')
    ax.legend()

    plt.tight_layout()
    plt.show()

if __name__ == '__main__':
    main()
