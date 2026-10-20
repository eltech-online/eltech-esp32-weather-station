// ElTech-Online ESP32 Weather Station — WiFi Access Point + web dashboard
//
// Same sensor/OLED behaviour as weather_station.ino, plus: the board starts its
// own WiFi network (an Access Point, no router needed) and serves a live-updating
// webpage showing temperature/humidity/pressure to anything that connects to it —
// a phone, laptop, whatever. Good for a demo where you don't want to depend on
// the venue's WiFi, and it's how a beginner sees "IoT" actually work end to end.
//
// Libraries needed (Arduino IDE Library Manager):
//   Adafruit AHTX0
//   Adafruit BMP280 Library
//   Adafruit SH110X
//   Adafruit GFX Library
// (Library Manager also offers Adafruit BusIO and Adafruit Unified Sensor as
// dependencies — click "Install all". Tested versions are listed in the README.)
// (WiFi and WebServer are built into the ESP32 board package — no separate install)
// Board package: esp32 by Espressif Systems
//
// How to use:
//   1. Flash this sketch.
//   2. The OLED and Serial Monitor show this board's WiFi network name and
//      password. Connect your phone/laptop to that network.
//   3. Open a browser to http://192.168.4.1 (also shown on the OLED and Serial).
//   4. The page updates every 2 seconds on its own — no need to refresh.
// Full source, wiring diagram and setup guide: github.com/eltech-online/eltech-esp32-weather-station

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>  // saves this board's generated WiFi password in flash
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "logo_bitmap.h"    // shop logo bitmap for the OLED splash screen
#include "page_template.h"  // the web dashboard's HTML page (used by handleRoot() below)

// ---- WiFi Access Point settings ----
// Leave both empty ("") and every board gets its OWN network name, made from its
// unique hardware (MAC) address, e.g. "ElTech-WS-A3F2", and its OWN random
// 8-character password. The password is created on first boot and saved in
// flash, so it stays the same after every reboot and re-flash. Both are shown on
// the OLED and in Serial Monitor.
// Or type your own in: the name can be up to 32 characters (only the first 21
// fit on the OLED) and the password must be 8-63 characters.
const char* AP_SSID     = "";
const char* AP_PASSWORD = "";
const bool  AP_OPEN_NETWORK = false;  // true = no password at all (anyone nearby can join)

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR    0x3C   // common default; try 0x3D if blank
#define BMP280_ADDR  0x77   // common default; some modules ship at 0x76

// GPIO 8/9 are the ESP32-C3 SuperMini's labeled I2C pins (confirmed on real
// hardware). If your sensors/display don't respond, run i2c_scanner.ino first
// to find the right pins and addresses for your board, then update these.
#define I2C_SDA 8
#define I2C_SCL 9

Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WebServer server(80);

bool ahtOK = false, bmpOK = false, oledOK = false;

// Self-test ranges. A reading outside these almost always means a faulty or
// badly wired sensor rather than the real weather, so the self-test fails it.
const float TEMP_MIN_C   = -20.0, TEMP_MAX_C   = 60.0;
const float HUM_MIN_PCT  =   0.0, HUM_MAX_PCT  = 100.0;
const float PRES_MIN_HPA = 870.0, PRES_MAX_HPA = 1085.0;

// Set by runSelfTest(): true only if the sensor was found AND its first
// reading was inside the ranges above.
bool ahtPassed = false, bmpPassed = false;
bool selfTestPassed = false;

// Each part's self-test result as text ("OK", "NOT FOUND", "BAD READING"),
// plus the first reading it took — kept so the web dashboard can show them too.
String ahtStatus = "NOT FOUND", bmpStatus = "NOT FOUND";
String ahtDetail, bmpDetail;

// The network name/password actually in use (from the settings above, or
// generated), and whether the Access Point started.
String apSsid, apPassword, apUrl;
bool wifiOK = false;
String wifiError;

void centerText(const String& text, int y, int textSize);
bool runSelfTest();
void showSelfTestFailure();
void drawDataScreen();
float g_temperature = NAN, g_humidity = NAN, g_pressure = NAN;

void handleRoot() {
  // No-cache headers: stops your browser reusing an old cached page after you
  // reflash the sketch, since the ESP32 always serves from the same IP.
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send(200, "text/html", PAGE_TEMPLATE);
}

// One self-test row as a JSON array: ["name","status","detail"].
String selfTestRow(const String& name, const String& status, const String& detail) {
  return "[\"" + name + "\",\"" + status + "\",\"" + detail + "\"]";
}

void handleData() {
  String json = "{";
  json += "\"temp\":\"" + (ahtOK ? String(g_temperature, 1) + " °C" : String("n/a")) + "\",";
  json += "\"hum\":\""  + (ahtOK ? String(g_humidity, 1) + " %" : String("n/a")) + "\",";
  json += "\"pres\":\"" + (bmpOK ? String(g_pressure, 0) + " hPa" : String("n/a")) + "\",";

  // The power-on self-test results, so you can see them without Serial Monitor.
  json += "\"selftest\":{\"result\":\"" + String(selfTestPassed ? "PASS" : "FAIL") + "\",\"parts\":[";
  json += selfTestRow("OLED (SH1106)", oledOK ? "OK" : "NOT FOUND", "") + ",";
  json += selfTestRow("AHT20", ahtStatus, ahtDetail) + ",";
  json += selfTestRow("BMP280", bmpStatus, bmpDetail) + ",";
  json += selfTestRow("WiFi AP", wifiOK ? "OK" : "FAILED", wifiOK ? "" : wifiError);
  json += "]}}";
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send(200, "application/json", json);
}

// "ElTech-WS-" + the last 4 hex digits of this board's MAC address. Every ESP32
// has a different MAC, so two kits in the same room never clash.
String makeUniqueSsid() {
  uint8_t mac[6];
  WiFi.softAPmacAddress(mac);
  char name[20];
  snprintf(name, sizeof(name), "ElTech-WS-%02X%02X", mac[4], mac[5]);
  return String(name);
}

// Loads this board's saved password, or creates and saves a random one on first
// boot. Uses lowercase letters and digits only, minus look-alikes (i, l, o, 0, 1),
// so it's easy to read off the OLED and type on a phone. To get a new one, set
// Tools > "Erase All Flash Before Sketch Upload" to Enabled and upload once.
String loadOrCreatePassword() {
  Preferences prefs;
  prefs.begin("weather", false);
  String password = prefs.getString("ap_password", "");
  if (password.length() < 8) {
    const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
    password = "";
    for (int i = 0; i < 8; i++) {
      password += alphabet[esp_random() % (sizeof(alphabet) - 1)];
    }
    prefs.putString("ap_password", password);
  }
  prefs.end();
  return password;
}

// Starts the Access Point and returns true if it worked. On failure, wifiError
// says why — instead of silently printing a URL for a network that doesn't exist.
bool startAccessPoint() {
  // Turning WiFi on first also powers up the radio, which esp_random() uses as a
  // source of true randomness for the generated password.
  WiFi.mode(WIFI_AP);

  apSsid = strlen(AP_SSID) ? String(AP_SSID) : makeUniqueSsid();
  if (AP_OPEN_NETWORK) {
    apPassword = "";
  } else {
    apPassword = strlen(AP_PASSWORD) ? String(AP_PASSWORD) : loadOrCreatePassword();
  }

  if (apSsid.length() > 32) {
    wifiError = "name over 32 chars";   // error texts fit one OLED line (21 chars)
    return false;
  }
  if (!AP_OPEN_NETWORK && (apPassword.length() < 8 || apPassword.length() > 63)) {
    wifiError = "pass not 8-63 chars";
    return false;
  }
  if (!WiFi.softAP(apSsid.c_str(), AP_OPEN_NETWORK ? NULL : apPassword.c_str())) {
    wifiError = "softAP() failed";
    return false;
  }
  // The ESP32-C3 SuperMini's tiny antenna can't handle full transmit power:
  // the signal gets so distorted that phones can't see or join the network.
  // Lowering it to 8.5 dBm fixes this and is still plenty for a room.
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  return true;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  oledOK = display.begin(OLED_ADDR, true);
  ahtOK  = aht.begin();
  bmpOK  = bmp.begin(BMP280_ADDR);

  // Required — without this the driver's default text color doesn't exactly
  // match any of its defined color constants, so every text pixel write
  // silently no-ops while drawLine/drawBitmap (which pass SH110X_WHITE
  // explicitly themselves) still render fine. This was the actual bug behind
  // "logo/lines show, but no text ever appears".
  if (oledOK) {
    display.setTextColor(SH110X_WHITE);
    // Clip text that's too wide instead of wrapping it onto the next line, so a
    // long custom WiFi name/password can't spill over the readings below it.
    display.setTextWrap(false);
  }

  Serial.println("========================================");
  Serial.println("           ElTech-Online");
  Serial.println("   ESP32 Weather Station (WiFi AP mode)");
  Serial.println("========================================");

  // Start the Access Point first, so the self-test can report whether it worked.
  wifiOK = startAccessPoint();
  IPAddress ip = WiFi.softAPIP();
  apUrl = "http://" + ip.toString();

  // Self-test: check this in Serial Monitor to confirm everything is wired right.
  selfTestPassed = runSelfTest();

  Serial.println("--- WiFi Access Point ---");
  if (wifiOK) {
    Serial.print("SSID:     "); Serial.println(apSsid);
    Serial.print("Password: "); Serial.println(AP_OPEN_NETWORK ? "(open network)" : apPassword.c_str());
    Serial.print("URL:      http://"); Serial.println(ip);
  } else {
    Serial.print("FAILED to start: "); Serial.println(wifiError);
  }

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();

  if (oledOK) {
    display.clearDisplay();
    display.drawBitmap((SCREEN_WIDTH - LOGO_WIDTH) / 2, 0, logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, SH110X_WHITE);
    centerText("ElTech-Online", 36, 1);
    centerText("Weather Station", 48, 1);
    display.display();
    delay(2000);

    if (!selfTestPassed) showSelfTestFailure();

    // Show the WiFi connection details full-screen once at boot. They also stay
    // visible on the live data screen afterwards (see drawDataScreen()), since a
    // beginner has no other way to learn the password once the unit is sealed.
    if (wifiOK) {
      display.clearDisplay();
      centerText("Connect to WiFi:", 0, 1);
      centerText(apSsid, 12, 1);
      centerText(AP_OPEN_NETWORK ? String("(open network)") : "Pass: " + apPassword, 24, 1);
      centerText("then open:", 36, 1);
      centerText(apUrl, 48, 1);
      display.display();
      delay(6000);
    }
  }
}

void loop() {
  server.handleClient();

  static unsigned long lastRead = 0;
  if (millis() - lastRead >= 2000) {
    lastRead = millis();

    if (ahtOK) {
      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);
      g_temperature = temp.temperature;
      g_humidity = humidity.relative_humidity;
    }
    if (bmpOK) {
      g_pressure = bmp.readPressure() / 100.0F;
    }

    Serial.println("----------------------------------------");
    Serial.print("  Temperature : ");
    if (ahtOK) Serial.printf("%5.1f C\n", g_temperature); else Serial.println("  n/a");
    Serial.print("  Humidity    : ");
    if (ahtOK) Serial.printf("%5.1f %%\n", g_humidity); else Serial.println("  n/a");
    Serial.print("  Pressure    : ");
    if (bmpOK) Serial.printf("%6.1f hPa\n", g_pressure); else Serial.println("  n/a");

    if (oledOK) drawDataScreen();
  }
}

// The live data screen. Same readings as the basic sketch, with the WiFi details
// added so you can always see how to connect — no need to catch the boot screen:
//
//   y=0   WiFi: ElTech-WS-A3F2      <- network name
//   y=9   Pass: abcd2345            <- password
//   y=19     21.4C                  <- big headline temperature (text size 3)
//   y=46  Hum 48%      P 1012hPa    <- humidity + pressure
//   y=56  http://192.168.4.1        <- the address to open in your browser
void drawDataScreen() {
  display.clearDisplay();

  if (wifiOK) {
    String nameLine = "WiFi: " + apSsid;
    centerText(nameLine.length() <= 21 ? nameLine : apSsid, 0, 1);
    String passLine = AP_OPEN_NETWORK ? String("Open network") : "Pass: " + apPassword;
    centerText(passLine.length() <= 21 ? passLine : apPassword, 9, 1);
  } else {
    centerText("WiFi FAILED", 0, 1);
    centerText(wifiError, 9, 1);
  }
  display.drawLine(0, 17, SCREEN_WIDTH, 17, SH110X_WHITE);

  String tempStr = ahtOK ? String(g_temperature, 1) + "C" : "n/a";
  centerText(tempStr, 20, 3);

  display.drawLine(0, 43, SCREEN_WIDTH, 43, SH110X_WHITE);

  display.setTextSize(1);
  display.setCursor(4, 46);
  display.print("Hum ");
  display.print(ahtOK ? String(g_humidity, 0) : "--");
  display.print("%");

  display.setCursor(70, 46);
  display.print("P ");
  display.print(bmpOK ? String(g_pressure, 0) : "--");
  display.print("hPa");

  if (wifiOK) centerText(apUrl, 56, 1);

  display.display();
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
    ahtStatus = ahtPassed ? "OK" : "BAD READING";
    ahtDetail = readOK ? String(temp.temperature, 1) + " °C, " + String(humidity.relative_humidity, 1) + " %"
                       : String("no data");
    if (readOK) {
      Serial.printf("%s (%.1f C, %.1f %%)\n", ahtStatus.c_str(),
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
    bmpStatus = bmpPassed ? "OK" : "BAD READING";
    bmpDetail = String(pressureHpa, 1) + " hPa";
    Serial.printf("%s (%.1f hPa)\n", bmpStatus.c_str(), pressureHpa);
  }

  Serial.print("WiFi AP:       ");
  if (wifiOK) { Serial.println("OK"); } else { Serial.print("FAILED ("); Serial.print(wifiError); Serial.println(")"); }

  bool passed = oledOK && ahtPassed && bmpPassed && wifiOK;
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
  if (!wifiOK) {
    display.setCursor(0, y); y += 10;
    display.print("WiFi: failed");
  }
  display.setCursor(0, 54);
  display.print("See Serial Monitor");
  display.display();
  delay(5000);
}
