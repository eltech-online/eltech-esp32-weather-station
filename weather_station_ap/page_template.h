#pragma once

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
    h1 { font-size:20px; font-weight:600; margin:0 0 2px; }
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
  <h1>ElTech-Online</h1>
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
