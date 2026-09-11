// I2C scanner for the ESP32-C3 SuperMini — run this FIRST to find the real
// SDA/SCL pins and confirm both the AHT20+BMP280 and the OLED respond.
//
// How to use:
//   1. Wire up ONE device at a time is easiest to interpret, but wiring both
//      at once is fine too — just note that a working scan should report
//      TWO addresses once both are connected (typically 0x38 for AHT20 and
//      0x76 or 0x77 for BMP280, plus 0x3C or 0x3D for the OLED).
//   2. Try each pin pair below in turn (edit PIN_SETS) — re-flash and re-open
//      Serial Monitor at 115200 baud between attempts.
//   3. Whichever pair reports device addresses is your real SDA/SCL — use
//      those numbers in weather_station.ino's I2C_SDA / I2C_SCL defines.

#include <Wire.h>

// Common candidate pin pairs to try on an ESP32-C3 SuperMini clone.
// {SDA, SCL}
const int PIN_SETS[][2] = {
  {8, 9},    // the C3 SuperMini's own labeled I2C pins
  {6, 7},    // used during this project's earlier ESP32-C6 build
  {4, 5},
  {2, 3},
  {18, 19},
  {22, 23},
};
const int NUM_SETS = sizeof(PIN_SETS) / sizeof(PIN_SETS[0]);

void scanOnce(int sda, int sck) {
  Wire.end();          // release any previous bus config
  delay(50);
  Wire.begin(sda, sck);

  Serial.print("--- Trying SDA=");
  Serial.print(sda);
  Serial.print(" SCL=");
  Serial.print(sck);
  Serial.println(" ---");

  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  Found device at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0) {
    Serial.println("  (nothing found on this pin pair)");
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("=== I2C Scanner: trying candidate pin pairs ===");

  for (int i = 0; i < NUM_SETS; i++) {
    scanOnce(PIN_SETS[i][0], PIN_SETS[i][1]);
    delay(300);
  }

  Serial.println("=== Done. Note which pin pair (if any) found devices. ===");
  Serial.println("If NONE found anything, double-check power (3V3/GND) and");
  Serial.println("that the Dupont wires are fully seated, before trying more pins.");
}

void loop() {
  delay(5000);
}
