#!/usr/bin/env python3
import os
import requests
import argparse

# ---------------- Konfiguration ----------------
# Basisnamen von Dateien, die _nicht_ hochgeladen werden sollen:
SKIP_FILES = [
    "secret.txt",
    "config.json",
    "dont_upload.bin",
    "chart.js"
]
# ------------------------------------------------

def upload_file(file_path: str, upload_url: str):
    """Lädt eine einzelne Datei zum ESP32 hoch."""
    if not os.path.isfile(file_path):
        print(f"✘ Datei nicht gefunden: {file_path}")
        return
    name = os.path.basename(file_path)
    if name in SKIP_FILES:
        print(f"→ Skip {name}")
        return

    with open(file_path, 'rb') as f:
        files = {'upload': (name, f)}
        try:
            r = requests.post(upload_url, files=files, timeout=10)
            r.raise_for_status()
            print(f"✔ {file_path} → {r.status_code} {r.text.strip()}")
        except Exception as e:
            print(f"✘ Fehler bei {file_path}: {e}")

def upload_folder(folder: str, upload_url: str, recursive: bool = False):
    """
    Lädt alle Dateien im Ordner hoch, überspringt Dateien in SKIP_FILES
    und optional Python-Skripte; Unterordner nur, wenn recursive=True.
    """
    for root, dirs, files in os.walk(folder):
        if not recursive:
            # keine Unterordner durchlaufen
            dirs.clear()
        for fname in files:
            # Python-Skripte überspringen
            if fname.lower().endswith('.py'):
                continue
            # Skip-Liste prüfen
            if fname in SKIP_FILES:
                print(f"→ Skip {os.path.join(root, fname)}")
                continue

            full_path = os.path.join(root, fname)
            upload_file(full_path, upload_url)

def main():
    # Arbeitsverzeichnis = Skriptordner
    script_dir = os.path.dirname(os.path.realpath(__file__))

    parser = argparse.ArgumentParser(
        description="Alle Dateien im Skript-Verzeichnis (ohne .py) zum ESP32 hochladen"
    )
    parser.add_argument(
        "host",
        help="IP oder Hostname deines ESP32 (z.B. 192.168.0.71 oder http://192.168.0.71)"
    )
    parser.add_argument(
        "-r", "--recursive",
        action="store_true",
        help="Unterordner mit hochladen"
    )
    parser.add_argument(
        "--files", "-f",
        nargs="+",
        help="Liste von Dateien, die hochgeladen werden sollen (anstatt ganzes Verzeichnis)"
    )
    args = parser.parse_args()

    # Schema ergänzen, falls vergessen
    host = args.host
    if not host.startswith(("http://", "https://")):
        host = "http://" + host
    upload_url = host.rstrip("/") + "/upload"

    print(f"Upload-Ziel: {upload_url}")

    if args.files:
        print("Lade diese Dateien hoch (Skip-Liste gilt auch hier):")
        for fp in args.files:
            upload_file(fp, upload_url)
    else:
        print(f"Starte Ordner-Upload aller Dateien in '{script_dir}'")
        upload_folder(script_dir, upload_url, recursive=args.recursive)

    print("Fertig.")

if __name__ == "__main__":
    main()
