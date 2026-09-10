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
//
// IMPORTANT implementation note: this sketch deliberately avoids the Arduino
// String class for anything drawn to the OLED. WiFi + WebServer together use a
// large chunk of heap, and Arduino's String class can silently fail (producing
// an empty string, no error) when a heap allocation doesn't succeed under that
// pressure. That was the actual cause of a real bug here: drawBitmap/drawLine
// write directly into a pre-allocated framebuffer (no allocation, never fails),
// while every text string was being built with String concatenation — so text
// silently vanished while graphics kept working. Fixed-size char buffers with
// snprintf() never allocate, so they can't fail that way.

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
char g_ipStr[24] = "0.0.0.0";  // filled in setup() once the AP is up

// The dashboard page. Auto-refreshes its numbers every 2s via fetch() to /data — no full page reload.
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
  char temp[16], hum[16], pres[16];
  if (ahtOK) snprintf(temp, sizeof(temp), "%.1f &deg;C", g_temperature); else snprintf(temp, sizeof(temp), "n/a");
  if (ahtOK) snprintf(hum,  sizeof(hum),  "%.1f %%",     g_humidity);    else snprintf(hum,  sizeof(hum),  "n/a");
  if (bmpOK) snprintf(pres, sizeof(pres), "%.0f hPa",    g_pressure);    else snprintf(pres, sizeof(pres), "n/a");

  char json[128];
  snprintf(json, sizeof(json), "{\"temp\":\"%s\",\"hum\":\"%s\",\"pres\":\"%s\"}", temp, hum, pres);
  server.send(200, "application/json", json);
}

// Draws `text` horizontally centered on the display at the given y, for the given text size.
// Takes a plain C-string (not Arduino String) — see the note at the top of this file for why.
void centerText(const char* text, int y, int textSize) {
  display.setTextSize(textSize);
  int len = strlen(text);
  int textWidthPx = len * 6 * textSize;
  int x = (SCREEN_WIDTH - textWidthPx) / 2;
  if (x < 0) x = 0;
  display.setCursor(x, y);
  display.print(text);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  oledOK = display.begin(OLED_ADDR, true);
  ahtOK  = aht.begin();
  bmpOK  = bmp.begin(BMP280_ADDR);

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
  snprintf(g_ipStr, sizeof(g_ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  Serial.println("--- WiFi Access Point ---");
  Serial.print("SSID: "); Serial.println(AP_SSID);
  Serial.print("URL:  http://"); Serial.println(g_ipStr);

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
    char urlLine[32];
    snprintf(urlLine, sizeof(urlLine), "http://%s", g_ipStr);

    display.clearDisplay();
    centerText("Connect to WiFi:", 4, 1);
    centerText(AP_SSID, 16, 1);
    centerText("then open:", 32, 1);
    centerText(urlLine, 44, 1);
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
      char tempStr[12], humStr[12], presStr[12];
      if (ahtOK) snprintf(tempStr, sizeof(tempStr), "%.1fC", g_temperature); else snprintf(tempStr, sizeof(tempStr), "n/a");
      if (ahtOK) snprintf(humStr,  sizeof(humStr),  "%.0f",  g_humidity);    else snprintf(humStr,  sizeof(humStr),  "--");
      if (bmpOK) snprintf(presStr, sizeof(presStr), "%.0f",  g_pressure);   else snprintf(presStr, sizeof(presStr), "--");

      display.clearDisplay();
      centerText("ElTech-Online", 0, 1);
      display.drawLine(0, 9, SCREEN_WIDTH, 9, SH110X_WHITE);

      centerText(tempStr, 16, 3);

      display.drawLine(0, 45, SCREEN_WIDTH, 45, SH110X_WHITE);

      display.setTextSize(1);
      display.setCursor(4, 52);
      display.print("Hum ");
      display.print(humStr);
      display.print("%");

      display.setCursor(70, 52);
      display.print("P ");
      display.print(presStr);
      display.print("hPa");

      display.display();
    }
  }
}
