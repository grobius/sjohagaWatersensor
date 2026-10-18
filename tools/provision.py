"""Write a device's OTAA identity into the STM32L0 data EEPROM.

Credentials come from devices.csv in the project root (ignored by Git; see
devices_example.csv). The record layout matches include/DeviceIdentity.h.

    python tools/provision.py --list
    python tools/provision.py ttn07              # build record and program it
    python tools/provision.py ttn07 --no-flash   # only build .provision/ttn07.bin
"""
import argparse
import csv
import shutil
import string
import struct
import subprocess
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEVICES_CSV = ROOT / "devices.csv"
OUT_DIR = ROOT / ".provision"
EEPROM_ADDRESS = "0x08080000"
VERSION = 1
CLI_NAME = "STM32_Programmer_CLI"
CLI_DEFAULT = Path(r"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe")


def load_devices(path):
    if not path.exists():
        sys.exit(f"{path} not found; copy devices_example.csv and fill in the credentials")
    with path.open(newline="", encoding="utf-8") as f:
        return {row["name"].strip(): row for row in csv.DictReader(f) if row.get("name", "").strip()}


def hex_field(row, field, chars):
    value = row[field].strip()
    if len(value) != chars or any(c not in string.hexdigits for c in value):
        sys.exit(f"{row['name']}: {field} must be {chars} hex characters, got {value!r}")
    return value.upper().encode("ascii")


def build_record(row):
    body = (b"WSID" + bytes([VERSION, 0, 0, 0])
            + hex_field(row, "devEui", 16)
            + hex_field(row, "appEui", 16)
            + hex_field(row, "appKey", 32))
    return body + struct.pack("<I", zlib.crc32(body))


def find_programmer(explicit):
    if explicit:
        return explicit
    found = shutil.which(CLI_NAME)
    if found:
        return found
    if CLI_DEFAULT.exists():
        return str(CLI_DEFAULT)
    sys.exit(f"{CLI_NAME} not found; install STM32CubeProgrammer or pass --programmer")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("name", nargs="?", help="device name from devices.csv")
    parser.add_argument("--list", action="store_true", help="list devices in devices.csv")
    parser.add_argument("--csv", type=Path, default=DEVICES_CSV, help="credentials file")
    parser.add_argument("--no-flash", action="store_true", help="only write the .bin file")
    parser.add_argument("--mode", default="UR",
                        help="ST-Link connect mode: UR (under reset, default; reaches a sleeping MCU), Normal or HotPlug")
    parser.add_argument("--programmer", help="path to STM32_Programmer_CLI")
    args = parser.parse_args()

    devices = load_devices(args.csv)
    if args.list or not args.name:
        for name, row in devices.items():
            print(f"{name:24} DevEUI {row['devEui'].strip().upper()}")
        return
    if args.name not in devices:
        sys.exit(f"{args.name!r} not in {args.csv}; use --list")

    row = devices[args.name]
    OUT_DIR.mkdir(exist_ok=True)
    image = OUT_DIR / f"{args.name}.bin"
    image.write_bytes(build_record(row))
    print(f"Wrote {image} (DevEUI {row['devEui'].strip().upper()})")
    if args.no_flash:
        return

    command = [find_programmer(args.programmer), "-c", "port=SWD", f"mode={args.mode}",
               "-w", str(image), EEPROM_ADDRESS, "-v", "-rst"]
    print(" ".join(command))
    sys.exit(subprocess.call(command))


if __name__ == "__main__":
    main()
