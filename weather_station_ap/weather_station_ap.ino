// ElTech-Online ESP32-C6 Weather Station — WiFi Access Point + web dashboard
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
// (WiFi and WebServer are built into the ESP32 board package — no separate install)
// Board package: esp32 by Espressif Systems (select an ESP32-C6 board/DevKit)
//
// How to use:
//   1. Flash this sketch.
//   2. On your phone/laptop, connect to the WiFi network AP_SSID below.
//   3. Open a browser to http://192.168.4.1 (also shown on the OLED and Serial).
//   4. The page updates every 2 seconds on its own — no need to refresh.

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "logo_bitmap.h"

// ---- WiFi Access Point settings — change these if you like ----
const char* AP_SSID     = "ElTech-WeatherStation";
const char* AP_PASSWORD = "weather123";   // must be 8+ characters, or "" for an open network

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR    0x3C
#define BMP280_ADDR  0x77   // confirmed via i2c_scanner.ino on real hardware
#define I2C_SDA 6
#define I2C_SCL 7

Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WebServer server(80);

bool ahtOK = false, bmpOK = false, oledOK = false;
float g_temperature = NAN, g_humidity = NAN, g_pressure = NAN;

// The dashboard page. %PLACEHOLDER% values are filled in by handleRoot() below.
// Auto-refreshes its numbers every 2s via fetch() to /data — no full page reload.
const char PAGE_TEMPLATE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ElTech-Online Weather Station</title>
  <style>
    body { font-family: -apple-system, Arial, sans-serif; background:#0f172a; color:#e2e8f0;
           margin:0; padding:24px; text-align:center; }
    h1 { font-size:20px; font-weight:600; margin:0 0 4px; }
    .sub { color:#94a3b8; font-size:13px; margin-bottom:24px; }
    .cards { display:flex; gap:12px; justify-content:center; flex-wrap:wrap; }
    .card { background:#1e293b; border-radius:12px; padding:20px 24px; min-width:110px; }
    .card .label { color:#94a3b8; font-size:12px; text-transform:uppercase; letter-spacing:0.05em; }
    .card .value { font-size:32px; font-weight:700; margin-top:6px; }
    .footer { margin-top:28px; color:#64748b; font-size:11px; }
  </style>
</head>
<body>
  <h1>ElTech-Online Weather Station</h1>
  <div class="sub">Live sensor readings, updated every 2 seconds</div>
  <div class="cards">
    <div class="card"><div class="label">Temperature</div><div class="value" id="t">--</div></div>
    <div class="card"><div class="label">Humidity</div><div class="value" id="h">--</div></div>
    <div class="card"><div class="label">Pressure</div><div class="value" id="p">--</div></div>
  </div>
  <div class="footer">ESP32-C6 + AHT20/BMP280 &middot; github.com/eltech-online/eltech-esp32-weather-station</div>
  <script>
    async function refresh() {
      try {
        const r = await fetch('/data');
        const d = await r.json();
        document.getElementById('t').textContent = d.temp;
        document.getElementById('h').textContent = d.hum;
        document.getElementById('p').textContent = d.pres;
      } catch (e) { /* device rebooting or momentarily busy — try again next tick */ }
    }
    refresh();
    setInterval(refresh, 2000);
  </script>
</body>
</html>
)HTML";

void handleRoot() {
  server.send(200, "text/html", PAGE_TEMPLATE);
}

void handleData() {
  String json = "{";
  json += "\"temp\":\"" + (ahtOK ? String(g_temperature, 1) + " °C" : String("n/a")) + "\",";
  json += "\"hum\":\""  + (ahtOK ? String(g_humidity, 1) + " %" : String("n/a")) + "\",";
  json += "\"pres\":\"" + (bmpOK ? String(g_pressure, 0) + " hPa" : String("n/a")) + "\"";
  json += "}";
  server.send(200, "application/json", json);
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
  }

  Serial.println("========================================");
  Serial.println("           ElTech-Online");
  Serial.println("   ESP32 Weather Station (WiFi AP mode)");
  Serial.println("========================================");
  Serial.println("--- Self-test ---");
  Serial.print("OLED (SH1106): "); Serial.println(oledOK ? "OK" : "NOT FOUND");
  Serial.print("AHT20:         "); Serial.println(ahtOK  ? "OK" : "NOT FOUND");
  Serial.print("BMP280:        "); Serial.println(bmpOK  ? "OK" : "NOT FOUND");

  // Start the Access Point.
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress ip = WiFi.softAPIP();
  Serial.println("--- WiFi Access Point ---");
  Serial.print("SSID: "); Serial.println(AP_SSID);
  Serial.print("URL:  http://"); Serial.println(ip);

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

    // Show the WiFi connection details so a beginner knows how to reach the dashboard.
    display.clearDisplay();
    centerText("Connect to WiFi:", 4, 1);
    centerText(AP_SSID, 16, 1);
    centerText("then open:", 32, 1);
    centerText("http://" + ip.toString(), 44, 1);
    display.display();
    delay(4000);
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

    if (oledOK) {
      display.clearDisplay();
      centerText("ElTech-Online", 0, 1);
      display.drawLine(0, 9, SCREEN_WIDTH, 9, SH110X_WHITE);

      display.setTextSize(3);
      String tempStr = ahtOK ? String(g_temperature, 1) + "C" : "n/a";
      centerText(tempStr, 16, 3);

      display.drawLine(0, 45, SCREEN_WIDTH, 45, SH110X_WHITE);

      display.setTextSize(1);
      display.setCursor(4, 52);
      display.print("Hum ");
      display.print(ahtOK ? String(g_humidity, 0) : "--");
      display.print("%");

      display.setCursor(70, 52);
      display.print("P ");
      display.print(bmpOK ? String(g_pressure, 0) : "--");
      display.print("hPa");

      display.display();
    }
  }
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
