# ElTech-Online ESP32-C3 Weather Station

[![Compile sketches](https://github.com/eltech-online/eltech-esp32-weather-station/actions/workflows/compile.yml/badge.svg)](https://github.com/eltech-online/eltech-esp32-weather-station/actions/workflows/compile.yml)

A beginner-friendly **learning kit**: build a real weather station from an **ESP32-C3 SuperMini**, an **AHT20+BMP280** combo sensor (temperature/humidity/pressure), and a **1.3" OLED SH1106** display, while picking up the core skills behind almost any IoT project — wiring I2C sensors, flashing Arduino firmware, and hosting a live WiFi dashboard from the board itself. No prior electronics or coding experience needed. This firmware ships pre-flashed and pre-tested on every [ElTech-Online](https://www.ebay.co.uk/usr/eltech-online) weather station kit (not yet listed) — but it's public so you can read it, learn from it, and build your own from scratch too.

![ElTech-Online logo](logo.png)

## What you'll learn

This kit is designed to teach, not just work out of the box:

- **Wiring an I2C bus** — how two different sensors and a display can share the same two data wires (SDA/SCL), and how to find a device's address with a scanner sketch when you're not sure
- **Flashing Arduino firmware** — installing a board package, picking the right board/Tools settings, and uploading code to real hardware
- **Reading sensor data** — pulling temperature, humidity, and pressure off I2C sensors and showing it on a small OLED display
- **Hosting your own WiFi dashboard** — turning the board into its own Access Point and serving a live-updating webpage, with no router or internet needed
- **Verifying your own work** — the firmware prints a pass/fail self-test on boot, so you get immediate proof each part is wired correctly before moving on

Every step is documented below, and the full source is here to read, copy, or modify.

## What it does

- Reads temperature, humidity (AHT20) and barometric pressure (BMP280) over I2C
- Shows a boot splash (shop logo + name) for 2 seconds, then a live readout: a large centered temperature as the headline reading, with humidity and pressure as smaller stats underneath
- Mirrors every reading to Serial (115200 baud) each cycle
- Runs a self-test on boot and prints the result to Serial — this is the actual bench-test procedure used before dispatch, and it doubles as your first confirmation that everything is wired correctly. Each part must not only be **found**, its first reading must also make sense (temperature −20 to 60 °C, humidity 0–100 %, pressure 870–1085 hPa), so a sensor that answers but gives garbage still fails:

  ```
  --- Self-test ---
  OLED (SH1106): OK
  AHT20:         OK (21.4 C, 48.2 %)
  BMP280:        OK (1012.6 hPa)
  WiFi AP:       OK          <- weather_station_ap only
  RESULT:        PASS
  ```

  Each line reads `OK`, `NOT FOUND` (check the wiring/address) or `BAD READING` (the part answers but its data is wrong). If anything fails, the OLED also shows a **SELF-TEST FAILED** screen listing what failed, so you can spot a problem without a computer attached.

There are **two sketches** in this repo:

| Sketch | What it adds |
|---|---|
| `weather_station/` | The basic version above — OLED + Serial only. |
| `weather_station_ap/` | Same sensor/OLED behaviour, **plus** the board starts its own WiFi network (an Access Point — no router needed) and serves a live-updating webpage with the readings. Connect any phone or laptop to the network it creates and browse to the address it shows on the OLED. |

## Hardware

| Component | Notes |
|---|---|
| ESP32-C3 SuperMini (or any ESP32-C3 dev board) | Needs WiFi support for the `weather_station_ap` sketch's Access Point + web dashboard |
| AHT20+BMP280 combo sensor | I2C, address `0x38` (AHT20) / `0x77` (BMP280) — confirm yours with the included scanner, some modules ship at `0x76` |
| 1.3" OLED, SH1106 driver, 128×64, I2C | Address `0x3C` (try `0x3D` if blank) |
| Breadboard + jumper wires | Both sensor and display share one I2C bus — 4 wires to each (VDD, GND, SDA, SCL) |

> **Why the temperature reads a little warm:** the ESP32 gets warm while it runs — more so with WiFi on in the `weather_station_ap` sketch — and on a breadboard the sensor sits only a few centimetres away, so it picks up some of that heat. Expect the reading to be roughly 1–3 °C above the real room temperature (humidity reads slightly low for the same reason). That's normal, not a faulty sensor. For more accurate readings, move the sensor away from the board on longer jumper wires, ideally with it below or beside the board rather than above it (heat rises).

## Wiring

All three devices share one I2C bus — wire SDA together and SCL together (this is normal for I2C, each device has its own address):

| Signal | ESP32-C3 | AHT20+BMP280 | OLED |
|---|---|---|---|
| 3.3V | 3V3 | VDD | VDD |
| GND | GND | GND | GND |
| SDA | GPIO 8 | SDA | SDA |
| SCL | GPIO 9 | SCL | SCK |

**Pin numbers vary between SuperMini clone boards.** If your board doesn't respond on GPIO 8/9, use `i2c_scanner/i2c_scanner.ino` first — it sweeps several candidate pin pairs and reports which one finds your devices.

> **Note:** GPIO 8/9 above are the ESP32-C3 SuperMini's own labeled I2C pins (see the wiring diagram). The firmware in this repo (`I2C_SDA`/`I2C_SCL` in both sketches) is **not updated yet** — it still has the values confirmed on the earlier ESP32-C6 build. It'll be updated to match once the C3 boards arrive and are bench-tested; until then, adjust `I2C_SDA`/`I2C_SCL` in the sketch to 8/9 yourself if you're wiring against this README.

![Wiring diagram: ESP32-C3 SuperMini to AHT20+BMP280 sensor and OLED SH1106 display](wiring_diagram.png)

## Setup (Arduino IDE)

1. **Add the ESP32 board index**: `File > Preferences` → Additional Boards Manager URLs:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
2. **Install the board package**: `Tools > Board > Boards Manager`, search "esp32", install **esp32 by Espressif Systems**.
3. **Select the board**: `Tools > Board > esp32 > ESP32C3 Dev Module`.
4. **Tools menu settings** below are carried over from this project's earlier ESP32-**C6** build and not yet re-confirmed on real C3 hardware — treat as a starting point, not a guarantee, until re-tested:

   | Setting | Value |
   |---|---|
   | Board | ESP32C3 Dev Module |
   | USB CDC On Boot | Enabled |
   | CPU Frequency | 160MHz |
   | Core Debug Level | None |
   | Erase All Flash Before Sketch Upload | Disabled |
   | Flash Frequency | 80MHz |
   | Flash Mode | QIO |
   | Flash Size | 4MB (32Mb) |
   | JTAG Adapter | Disabled |
   | Partition Scheme | Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS) |
   | Upload Speed | 921600 |

   (Port will be whatever your OS assigns the board — e.g. `/dev/cu.usbmodem*` on macOS, `COM*` on Windows.)
5. **Install libraries** via `Sketch > Include Library > Manage Libraries`:
   - Adafruit AHTX0
   - Adafruit BMP280 Library
   - Adafruit SH110X
   - Adafruit GFX Library

   If Library Manager asks to install dependencies (Adafruit BusIO, Adafruit Unified Sensor), click **Install all**.

   **Tested with** these versions — if something doesn't compile or display correctly, installing these exact versions is the first thing to try (both Boards Manager and Library Manager let you pick a version from a dropdown):

   | Package | Version |
   |---|---|
   | esp32 by Espressif Systems (board package) | 3.3.11 |
   | Adafruit AHTX0 | 2.0.6 |
   | Adafruit BMP280 Library | 3.0.0 |
   | Adafruit SH110X | 2.1.15 |
   | Adafruit GFX Library | 1.12.6 |
   | Adafruit BusIO | 1.17.4 |
   | Adafruit Unified Sensor | 1.1.15 |

   (Confirmed on real hardware with an ESP32-C6 so far; re-confirmation on the ESP32-C3 is pending, same as the Tools settings above.)
6. If you're unsure of your I2C pins/addresses, flash `i2c_scanner/i2c_scanner.ino` first and check Serial Monitor (115200 baud).
7. Open `weather_station/weather_station.ino` (or `weather_station_ap/weather_station_ap.ino` for the WiFi version), adjust `I2C_SDA`/`I2C_SCL`/`BMP280_ADDR`/`OLED_ADDR` at the top if your scan found different values, and upload.

## WiFi dashboard (`weather_station_ap`)

This version needs no extra libraries — `WiFi.h` and `WebServer.h` are built into the ESP32 board package.

1. Open `weather_station_ap/weather_station_ap.ino` and upload it. You don't need to change anything: by default every board creates its **own** network name (from its unique hardware address, e.g. `ElTech-WS-A3F2`) and its **own** random 8-character password, so two kits in the same room never clash and nobody can guess yours from this code. The password is made on first boot and saved in the board's flash memory, so it stays the same after reboots and re-uploads.
2. The OLED and Serial Monitor show the network name, password, and a URL like `http://192.168.4.1`. On the OLED they appear full-screen for 6 seconds after the splash, then stay visible around the live readings (name and password at the top, URL at the bottom), so you never have to catch the boot screen.
3. On your phone or laptop, connect to that WiFi network, then open that URL in a browser.
4. The page shows temperature/humidity/pressure and updates itself every 2 seconds — no need to refresh. Underneath, a **Self-test (at power-on)** panel shows the overall PASS/FAIL result and each part's status (OLED, AHT20, BMP280, WiFi AP) with the first reading it took, so you can check the self-test from your phone without opening Serial Monitor.

**Want your own name/password instead?** Type them into `AP_SSID` / `AP_PASSWORD` near the top of the sketch (name up to 32 characters, password 8–63 characters), or set `AP_OPEN_NETWORK` to `true` for no password at all. **Want a fresh random password?** Set `Tools > Erase All Flash Before Sketch Upload` to **Enabled**, upload once, then set it back to Disabled. If the network can't start (e.g. your password is too short), the self-test reports `WiFi AP: FAILED` with the reason instead of showing a URL that won't work.

This is a standalone Access Point, not connected to your home WiFi/the internet — it's meant for a local demo (e.g. showing the kit working at a table, or on a shared network with no internet needed). Range is the same as any small WiFi device, roughly a typical room.

## Using your own logo instead

Both `weather_station/logo_bitmap.h` and `weather_station_ap/logo_bitmap.h` contain the ElTech-Online shop logo as a 48×32 monochrome bitmap — they're separate copies (one per sketch folder, since Arduino sketches can't share files across folders), so **update both** if you swap the logo, or they'll drift out of sync with each other. To swap in your own:

1. Crop your logo to just the icon (no fine text — anything under ~10px tall won't render legibly at this resolution)
2. Convert it to a 1-bit bitmap sized to fit within 48×32 (any tool that exports an [Adafruit GFX-style `drawBitmap` byte array](https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts) works — e.g. [image2cpp](https://javl.github.io/image2cpp/))
3. Replace the contents of both `logo_bitmap.h` files with your generated array, keeping the `LOGO_WIDTH`/`LOGO_HEIGHT` defines in sync

Or just delete the `drawBitmap(...)` line in `setup()` (in both sketches) and keep the text-only splash.

## License

The code, documentation and wiring diagram are MIT-licensed — see [LICENSE](LICENSE). Use them, modify them, build your own kit with them.

**The ElTech-Online name and logo are not covered by the MIT license.** The logo files (`logo.png` and the bitmap in both `logo_bitmap.h` files) are © ElTech-Online, all rights reserved. If you build or sell your own version, swap in your own logo (see [Using your own logo instead](#using-your-own-logo-instead)) and don't present it as an ElTech-Online product.
