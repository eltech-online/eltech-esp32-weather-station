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
// (WiFi and WebServer are built into the ESP32 board package — no separate install)
// Board package: esp32 by Espressif Systems
//
// How to use:
//   1. Flash this sketch.
//   2. On your phone/laptop, connect to the WiFi network AP_SSID below.
//   3. Open a browser to http://192.168.4.1 (also shown on the OLED and Serial).
//   4. The page updates every 2 seconds on its own — no need to refresh.
// Full source, wiring diagram and setup guide: github.com/eltech-online/eltech-esp32-weather-station

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

// Explicit forward declaration — Arduino's auto-prototype scanner can fail to find
// this on its own when a large raw-string literal (the logo's base64 data) appears
// earlier in the file, so don't rely on it being auto-generated.
void centerText(const String& text, int y, int textSize);
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
    .brand { display:flex; align-items:center; justify-content:center; gap:10px; margin-bottom:2px; }
    .brand img { width:48px; height:auto; }
    h1 { font-size:20px; font-weight:600; margin:0; }
    .sub { color:#94a3b8; font-size:13px; margin-bottom:24px; }
    .cards { display:flex; gap:12px; justify-content:center; flex-wrap:wrap; }
    .card { background:#1e293b; border-radius:12px; padding:20px 24px; min-width:110px; }
    .card .label { color:#94a3b8; font-size:12px; text-transform:uppercase; letter-spacing:0.05em; }
    .card .value { font-size:32px; font-weight:700; margin-top:6px; }
    .getstarted { margin-top:32px; background:#1e293b; border-radius:12px; padding:18px 22px;
                  text-align:left; max-width:420px; margin-left:auto; margin-right:auto; }
    .getstarted h2 { font-size:14px; margin:0 0 10px; color:#e2e8f0; }
    .getstarted ol { margin:0; padding-left:20px; color:#cbd5e1; font-size:13px; line-height:1.6; }
    .getstarted a { color:#7dd3fc; }
    .footer { margin-top:24px; color:#64748b; font-size:11px; }
    .footer a { color:#94a3b8; }
  </style>
</head>
<body>
  <div class="brand">
    <img src="data:image/png;base64,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" alt="ElTech-Online logo">
    <h1>ElTech-Online</h1>
  </div>
  <div class="sub">ESP32 Weather Station &mdash; live readings, updated every 2 seconds</div>
  <div class="cards">
    <div class="card"><div class="label">Temperature</div><div class="value" id="t">--</div></div>
    <div class="card"><div class="label">Humidity</div><div class="value" id="h">--</div></div>
    <div class="card"><div class="label">Pressure</div><div class="value" id="p">--</div></div>
  </div>
  <div class="getstarted">
    <h2>Build your own</h2>
    <ol>
      <li>Get the full source, wiring diagram and setup guide on <a href="https://github.com/eltech-online/eltech-esp32-weather-station" target="_blank">GitHub</a></li>
      <li>Flash it onto an ESP32 with the Arduino IDE (board package + 4 libraries, all listed in the README)</li>
      <li>Power it on, connect to the WiFi network it creates, and you'll see this exact page</li>
    </ol>
  </div>
  <div class="footer">ESP32 + AHT20/BMP280 &middot; <a href="https://github.com/eltech-online/eltech-esp32-weather-station" target="_blank">github.com/eltech-online/eltech-esp32-weather-station</a></div>
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
  // No-cache headers: the ESP32's own IP (192.168.4.1) is reused across every
  // flash of this sketch, so a browser that visited an earlier/buggy version of
  // this page can otherwise keep showing it from cache after a normal reload.
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send(200, "text/html", PAGE_TEMPLATE);
}

void handleData() {
  String json = "{";
  json += "\"temp\":\"" + (ahtOK ? String(g_temperature, 1) + " °C" : String("n/a")) + "\",";
  json += "\"hum\":\""  + (ahtOK ? String(g_humidity, 1) + " %" : String("n/a")) + "\",";
  json += "\"pres\":\"" + (bmpOK ? String(g_pressure, 0) + " hPa" : String("n/a")) + "\"";
  json += "}";
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
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
