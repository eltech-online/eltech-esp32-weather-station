// I2C scanner for the ESP32 weather station — run this FIRST to find the real
// SDA/SCL pins and confirm the AHT20+BMP280 and the OLED all respond.
//
// How to use:
//   1. Wire up the sensor and the OLED (one device at a time is easiest to
//      read, but both at once is fine). With everything connected, a working
//      scan reports THREE addresses: 0x38 (AHT20), 0x76 or 0x77 (BMP280), and
//      0x3C or 0x3D (OLED).
//   2. Flash this sketch and open Serial Monitor at 115200 baud. It tries every
//      pin pair in PIN_SETS automatically, one after another — no need to edit
//      or re-flash between attempts. The scan runs once at boot, so if you
//      opened Serial Monitor too late, press the board's RESET button.
//   3. Whichever pair reports device addresses is your real SDA/SCL — put those
//      numbers into I2C_SDA / I2C_SCL at the top of weather_station.ino (and
//      weather_station_ap.ino, if you use the WiFi version), along with any
//      addresses that differ from the defaults there.

#include <Wire.h>

// Common candidate pin pairs to try. {SDA, SCL}
// Only pins that exist on the ESP32-C3 are listed. Don't add GPIO 18/19 —
// they are the C3's USB pins, and scanning them cuts the Serial Monitor connection.
const int PIN_SETS[][2] = {
  {8, 9},    // the ESP32-C3 SuperMini's own labeled I2C pins
  {6, 7},
  {4, 5},
  {2, 3},
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
