
/*
  COE67-351 Lab 1
  ESP32-S3 + DHT22 + Wi-Fi + MQTT
  Tasks 1, 2 and 4
  Wi-Fi Scan + Auto Recovery

  Libraries:
  - DHT sensor library (Adafruit)
  - Adafruit Unified Sensor
  - ArduinoJson v7
  - ArduinoMqttClient
*/

#include <WiFi.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <ArduinoMqttClient.h>

// ---------------- Settings ----------------

const char* NODE_ID = "p06";

const char* WIFI_SSID = "Achi";
const char* WIFI_PASSWORD = "123456789";

const char* MQTT_BROKER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

// ---------------- MQTT Topics ----------------

String ENV_TOPIC;
String STATUS_TOPIC;

// ---------------- Sensor ----------------

#define DHTPIN 4
#define DHTTYPE DHT22

// ---------------- Intervals ----------------

const unsigned long READ_INTERVAL = 10000;
const unsigned long WIFI_RETRY_INTERVAL = 10000;
const unsigned long MQTT_RETRY_INTERVAL = 5000;

const unsigned long MQTT_KEEPALIVE_MS = 15000;
const unsigned long MQTT_CONNECT_TIMEOUT_MS = 3000;

// ---------------- Objects ----------------

DHT dht(DHTPIN, DHTTYPE);

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

// ---------------- Variables ----------------

unsigned long lastRead = 0;
unsigned long lastWiFiAttempt = 0;
unsigned long lastMqttAttempt = 0;
unsigned long seqNumber = 0;

bool wifiWasUp = false;

// ==================================================
// Wi-Fi SCAN
// ==================================================

void scanWiFi() {

  Serial.println();
  Serial.println("# Scanning WiFi networks...");

  int n = WiFi.scanNetworks();

  if (n == 0) {

    Serial.println("# No WiFi networks found");

  } else {

    Serial.print("# Networks found: ");
    Serial.println(n);

    bool foundTarget = false;

    for (int i = 0; i < n; i++) {

      Serial.print("# WiFi found: ");
      Serial.print(WiFi.SSID(i));

      Serial.print(" | RSSI: ");
      Serial.println(WiFi.RSSI(i));

      if (WiFi.SSID(i) == WIFI_SSID) {
        foundTarget = true;
      }
    }

    if (foundTarget) {
      Serial.println("# Target WiFi FOUND");
    } else {
      Serial.println("# Target WiFi NOT FOUND");
    }
  }

  WiFi.scanDelete();

  Serial.println("# WiFi scan completed");
}

// ==================================================
// Wi-Fi CONNECTION
// ==================================================

void maintainWiFi() {

  bool up = (WiFi.status() == WL_CONNECTED);

  if (up && !wifiWasUp) {

    Serial.print("# WiFi connected, IP: ");
    Serial.println(WiFi.localIP());

  }

  if (!up && wifiWasUp) {

    Serial.println("# WiFi lost");

    mqttClient.stop();
  }

  wifiWasUp = up;

  if (up) return;

  unsigned long now = millis();

  if (now - lastWiFiAttempt < WIFI_RETRY_INTERVAL) {
    return;
  }

  lastWiFiAttempt = now;

  Serial.println("# Connecting to WiFi...");

  WiFi.disconnect();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ==================================================
// MQTT CONNECTION
// ==================================================

void maintainMqtt() {

  if (WiFi.status() != WL_CONNECTED) return;

  if (mqttClient.connected()) return;

  unsigned long now = millis();

  if (now - lastMqttAttempt < MQTT_RETRY_INTERVAL) {
    return;
  }

  lastMqttAttempt = now;

  Serial.println("# Connecting to MQTT broker...");

  mqttClient.setId(String("esp32s3_") + NODE_ID);

  mqttClient.setKeepAliveInterval(MQTT_KEEPALIVE_MS);

  mqttClient.setConnectionTimeout(
    MQTT_CONNECT_TIMEOUT_MS
  );

  // Last Will: offline

  mqttClient.beginWill(
    STATUS_TOPIC.c_str(),
    true,
    1
  );

  mqttClient.print("offline");

  mqttClient.endWill();

  if (!mqttClient.connect(MQTT_BROKER, MQTT_PORT)) {

    Serial.print("# MQTT connect failed, error: ");

    Serial.println(mqttClient.connectError());

    return;
  }

  Serial.println("# MQTT connected");

  // Publish online status

  mqttClient.beginMessage(
    STATUS_TOPIC.c_str(),
    true,
    1
  );

  mqttClient.print("online");

  mqttClient.endMessage();

  Serial.println("# status: online");
}

// ==================================================
// READ SENSOR AND PUBLISH MQTT
// ==================================================

void readAndPublish() {

  float humidity = dht.readHumidity();

  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {

    Serial.println("# DHT22 read failed");

    return;
  }

  JsonDocument doc;

  doc["node"] = NODE_ID;

  doc["seq"] = seqNumber++;

  doc["temp_c"] =
    round(temperature * 10.0) / 10.0;

  doc["rh"] =
    round(humidity * 10.0) / 10.0;

  doc["rssi"] =
    (WiFi.status() == WL_CONNECTED)
    ? WiFi.RSSI()
    : 0;

  // Serial JSON output

  serializeJson(doc, Serial);

  Serial.println();

  // Stop if MQTT disconnected

  if (!mqttClient.connected()) {

    Serial.println(
      "# not connected - reading NOT sent"
    );

    return;
  }

  // Publish JSON with QoS 1

  size_t len = measureJson(doc);

  mqttClient.beginMessage(
    ENV_TOPIC.c_str(),
    len,
    false,
    1
  );

  serializeJson(doc, mqttClient);

  if (mqttClient.endMessage() != 1) {

    Serial.println("# publish failed");

  } else {

    Serial.println("# MQTT publish successful");
  }
}

// ==================================================
// SETUP
// ==================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  ENV_TOPIC =
    String("coe67351/") + NODE_ID + "/env";

  STATUS_TOPIC =
    String("coe67351/") + NODE_ID + "/status";

  Serial.println();

  Serial.println(
    "# ESP32-S3 Lab 1 node " + String(NODE_ID)
  );

  Serial.println("# env topic: " + ENV_TOPIC);

  Serial.println("# status topic: " + STATUS_TOPIC);

  // Initialize DHT22

  dht.begin();

  // Initialize WiFi

  WiFi.mode(WIFI_STA);

  WiFi.setAutoReconnect(true);

  // Scan available WiFi networks

  scanWiFi();

  // Start WiFi connection

  Serial.println("# Starting WiFi connection...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  lastWiFiAttempt = millis();
}

// ==================================================
// LOOP
// ==================================================

void loop() {

  // Maintain WiFi

  maintainWiFi();

  // Maintain MQTT

  maintainMqtt();

  // MQTT keep alive

  if (mqttClient.connected()) {

    mqttClient.poll();
  }

  // Read sensor every 10 seconds

  unsigned long now = millis();

  if (now - lastRead >= READ_INTERVAL) {

    lastRead = now;

    readAndPublish();
  }
}
