/*
 * ESP32 + DHT11 + AsyncWebServer + WebSocket
 * Live temperature monitor
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHTesp.h>

// ───── WIFI ───────────────────────────────────────────
const char *ssid     = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

// ───── DHT11 ──────────────────────────────────────────
const int DHT_PIN = 15;
DHTesp dht;

// ───── WEB SERVER ─────────────────────────────────────
AsyncWebServer  server(80);
AsyncWebSocket  ws("/ws");

// ───── HTML (stored in flash) ─────────────────────────
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>ESP32 Temperature Monitor</title>
<style>
body{font-family:Arial,sans-serif;margin:0;padding:24px;text-align:center}
h1{margin-top:0}.card{display:inline-block;margin:12px;padding:24px 32px;border:1px solid #ccc;border-radius:12px;box-shadow:0 2px 6px rgba(0,0,0,.1)}
.value{font-size:3rem;font-weight:700}.label{color:#555}
</style></head><body>
<h1>ESP32 Temperature Monitor</h1>
<div class="card"><div class="label">Temperature (°C)</div><div id="temp" class="value">%TEMP%</div></div>
<script>
const ws=new WebSocket(`ws://${location.host}/ws`);
ws.onmessage=e=>{try{const d=JSON.parse(e.data);
 if('temp'in d)document.getElementById('temp').textContent=d.temp.toFixed(1);}catch{}};
ws.onclose=()=>setTimeout(()=>location.reload(),2000);
</script>
</body></html>
)rawliteral";

// ───── WebSocket callback ────────────────────────────
void onEvent(AsyncWebSocket * /*srv*/, AsyncWebSocketClient *client,
             AwsEventType type, void *, uint8_t *, size_t) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WS client #%u connected\n", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WS client #%u disconnected\n", client->id());
  }
}

// ───── Template processor ────────────────────────────
String processor(const String& var) {
  TempAndHumidity d = dht.getTempAndHumidity();
  if (var == F("TEMP")) return String(d.temperature, 1);
  return String();
}

// ───── SETUP ─────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  dht.setup(DHT_PIN, DHTesp::DHT11);

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) { Serial.print('.'); delay(500); }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  // WebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Serve index page from PROGMEM with template processor
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html", index_html, processor);
  });

  server.begin();
}

// ───── LOOP ──────────────────────────────────────────
void loop() {
  static unsigned long lastPush = 0;
  if (millis() - lastPush > 2000) {          // every 2 seconds
    lastPush = millis();
    TempAndHumidity d = dht.getTempAndHumidity();
    if (!isnan(d.temperature) && ws.count()) {
      char json[32];
      snprintf(json, sizeof(json), "{\"temp\":%.1f}", d.temperature);
      ws.textAll(json);
    }
  }
  ws.cleanupClients();
}
