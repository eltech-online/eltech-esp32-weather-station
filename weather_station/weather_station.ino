// ESP32-C3 Weather Station Kit — basic version (OLED + Serial, no WiFi)
// Reads temperature/humidity/pressure from AHT20+BMP280 and shows them on an
// OLED SH1106 128x64 display. Prints a self-test on boot so you can confirm
// everything is wired correctly before writing any more code.
//
// Libraries needed (Arduino IDE Library Manager):
//   Adafruit AHTX0
//   Adafruit BMP280 Library
//   Adafruit SH110X
//   Adafruit GFX Library
// (Library Manager also offers Adafruit BusIO and Adafruit Unified Sensor as
// dependencies — click "Install all". Tested versions are listed in the README.)
// Board package: esp32 by Espressif Systems — select "ESP32C3 Dev Module"

#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "logo_bitmap.h"  // ElTech-Online shop logo, converted from the real eBay shop icon

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR    0x3C   // common default; try 0x3D if blank
#define BMP280_ADDR  0x77   // common default; some modules ship at 0x76

// These pins were confirmed on an earlier ESP32-C6 build, not yet re-tested on
// C3. If your sensors/display don't respond, run i2c_scanner.ino first to find
// the right pins and addresses for your board, then update these.
#define I2C_SDA 6
#define I2C_SCL 7

Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

bool ahtOK = false, bmpOK = false, oledOK = false;

// Self-test ranges. A reading outside these almost always means a faulty or
// badly wired sensor rather than the real weather, so the self-test fails it.
const float TEMP_MIN_C   = -20.0, TEMP_MAX_C   = 60.0;
const float HUM_MIN_PCT  =   0.0, HUM_MAX_PCT  = 100.0;
const float PRES_MIN_HPA = 870.0, PRES_MAX_HPA = 1085.0;

// Set by runSelfTest(): true only if the sensor was found AND its first
// reading was inside the ranges above.
bool ahtPassed = false, bmpPassed = false;

void centerText(const String& text, int y, int textSize);
bool runSelfTest();
void showSelfTestFailure();

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  oledOK = display.begin(OLED_ADDR, true);
  ahtOK  = aht.begin();
  bmpOK  = bmp.begin(BMP280_ADDR);

  if (oledOK) {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);
  }

  // Serial splash banner.
  Serial.println("========================================");
  Serial.println("           ElTech-Online");
  Serial.println("       ESP32 Weather Station Kit");
  Serial.println("========================================");

  // Self-test: check this in Serial Monitor to confirm everything is wired right.
  bool selfTestPassed = runSelfTest();

  // OLED splash screen — shown for 2s before the sensor-reading loop starts.
  // Real shop logo (converted from the eBay shop icon) centered on top, with the
  // shop name and kit name centered underneath.
  if (oledOK) {
    display.clearDisplay();
    display.drawBitmap((SCREEN_WIDTH - LOGO_WIDTH) / 2, 0, logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, SH110X_WHITE);
    centerText("ElTech-Online", 36, 1);
    centerText("Weather Station", 48, 1);
    display.display();
    delay(2000);

    if (!selfTestPassed) showSelfTestFailure();
  }
}

void loop() {
  float temperature = NAN, humidityPct = NAN, pressureHpa = NAN;

  if (ahtOK) {
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    temperature = temp.temperature;
    humidityPct = humidity.relative_humidity;
  }
  if (bmpOK) {
    pressureHpa = bmp.readPressure() / 100.0F;
  }

  // Serial output — aligned columns, always printed regardless of OLED status.
  Serial.println("----------------------------------------");
  Serial.print("  Temperature : ");
  if (ahtOK) Serial.printf("%5.1f C\n", temperature); else Serial.println("  n/a");
  Serial.print("  Humidity    : ");
  if (ahtOK) Serial.printf("%5.1f %%\n", humidityPct); else Serial.println("  n/a");
  Serial.print("  Pressure    : ");
  if (bmpOK) Serial.printf("%6.1f hPa\n", pressureHpa); else Serial.println("  n/a");

  // OLED output — temperature is the headline reading (large, centered), with
  // humidity and pressure as smaller supporting stats underneath. This gives a
  // clear visual hierarchy instead of three equally-weighted rows, which reads
  // more like a real weather display and less like a debug printout.
  if (oledOK) {
    display.clearDisplay();

    // Thin header, centered.
    centerText("Weather Station", 0, 1);
    display.drawLine(0, 9, SCREEN_WIDTH, 9, SH110X_WHITE);

    // Big centered temperature — the headline number.
    String tempStr = ahtOK ? String(temperature, 1) + "C" : "n/a";
    centerText(tempStr, 16, 3);

    // Divider between the headline and the supporting stats.
    display.drawLine(0, 45, SCREEN_WIDTH, 45, SH110X_WHITE);

    // Humidity (left) and pressure (right), smaller, side by side.
    display.setTextSize(1);
    display.setCursor(4, 52);
    display.print("Hum ");
    display.print(ahtOK ? String(humidityPct, 0) : "--");
    display.print("%");

    display.setCursor(70, 52);
    display.print("P ");
    display.print(bmpOK ? String(pressureHpa, 0) : "--");
    display.print("hPa");

    display.display();
  }

  delay(2000);
}

// Draws `text` horizontally centered on the display at the given y, for the given text size.
// Uses a fixed 6px-per-character advance (the default GFX font's width at size 1) rather than
// getTextBounds() — that call returned inconsistent widths across Adafruit_GFX library versions
// and pushed text off-screen.
void centerText(const String& text, int y, int textSize) {
  display.setTextSize(textSize);
  int textWidthPx = text.length() * 6 * textSize;
  int x = (SCREEN_WIDTH - textWidthPx) / 2;
  if (x < 0) x = 0;
  display.setCursor(x, y);
  display.print(text);
}

bool inRange(float value, float minValue, float maxValue) {
  return !isnan(value) && value >= minValue && value <= maxValue;
}

// Checks each part is connected AND gives a believable first reading, prints
// the result to Serial, and returns true only if everything passed. A sensor
// that answers but reads e.g. 0 hPa or NaN is reported as BAD READING.
bool runSelfTest() {
  Serial.println("--- Self-test ---");
  Serial.print("OLED (SH1106): "); Serial.println(oledOK ? "OK" : "NOT FOUND");

  Serial.print("AHT20:         ");
  if (!ahtOK) {
    Serial.println("NOT FOUND");
  } else {
    sensors_event_t humidity, temp;
    bool readOK = aht.getEvent(&humidity, &temp);
    ahtPassed = readOK
             && inRange(temp.temperature, TEMP_MIN_C, TEMP_MAX_C)
             && inRange(humidity.relative_humidity, HUM_MIN_PCT, HUM_MAX_PCT);
    if (readOK) {
      Serial.printf("%s (%.1f C, %.1f %%)\n", ahtPassed ? "OK" : "BAD READING",
                    temp.temperature, humidity.relative_humidity);
    } else {
      Serial.println("BAD READING (no data)");
    }
  }

  Serial.print("BMP280:        ");
  if (!bmpOK) {
    Serial.println("NOT FOUND");
  } else {
    delay(100);  // give the BMP280 time to finish its first measurement after begin()
    float pressureHpa = bmp.readPressure() / 100.0F;
    bmpPassed = inRange(pressureHpa, PRES_MIN_HPA, PRES_MAX_HPA);
    Serial.printf("%s (%.1f hPa)\n", bmpPassed ? "OK" : "BAD READING", pressureHpa);
  }

  bool passed = oledOK && ahtPassed && bmpPassed;
  Serial.print("RESULT:        "); Serial.println(passed ? "PASS" : "FAIL");
  return passed;
}

// Shown on the OLED only when the self-test fails, so a problem is visible even
// without a computer attached. (If the OLED itself failed, only Serial shows it.)
void showSelfTestFailure() {
  display.clearDisplay();
  centerText("SELF-TEST FAILED", 0, 1);
  display.drawLine(0, 9, SCREEN_WIDTH, 9, SH110X_WHITE);
  int y = 14;
  if (!ahtPassed) {
    display.setCursor(0, y); y += 10;
    display.print(ahtOK ? "AHT20: bad reading" : "AHT20: not found");
  }
  if (!bmpPassed) {
    display.setCursor(0, y); y += 10;
    display.print(bmpOK ? "BMP280: bad reading" : "BMP280: not found");
  }
  display.setCursor(0, 54);
  display.print("See Serial Monitor");
  display.display();
  delay(5000);
}
