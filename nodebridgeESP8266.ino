// ============================================================
//  ESP32 + DHT22 → MQTT
//  Bibliothèques requises (Library Manager) :
//    - PubSubClient   par Nick O'Leary
//    - DHT sensor library  par Adafruit
//    - ArduinoJson    par Benoit Blanchon
// ============================================================

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ── Configuration réseau ─────────────────────────────────────
#define WIFI_SSID     "Nana Tom"
#define WIFI_PASSWORD "!!Ils sont beaux nos choupis!!"

// ── Configuration MQTT ───────────────────────────────────────
#define MQTT_BROKER   "192.168.1.100"   // IP de votre broker (ex: Mosquitto, NodeBridge)
#define MQTT_PORT     1883
#define MQTT_USER     ""                // laisser vide si pas d'auth
#define MQTT_PASSWORD ""
#define MQTT_CLIENT_ID "esp32-salon"    // unique par appareil

// Topics
#define TOPIC_DATA    "maison/salon/dht22"       // publish : données capteur
#define TOPIC_STATUS  "maison/salon/status"      // publish : état de connexion
#define TOPIC_CMD     "maison/salon/cmd"         // subscribe : commandes reçues

// ── Configuration DHT22 ──────────────────────────────────────
#define DHT_PIN       4         // GPIO4 — adaptez selon votre câblage
#define DHT_TYPE      DHT22

// ── Intervalle de publication (ms) ──────────────────────────
#define PUBLISH_INTERVAL  10000   // 10 secondes

// ────────────────────────────────────────────────────────────

DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long lastPublish = 0;

// ── Connexion Wi-Fi ──────────────────────────────────────────
void connectWiFi() {
  Serial.print("Connexion Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("Wi-Fi connecté — IP : %s\n", WiFi.localIP().toString().c_str());
}

// ── Callback : messages MQTT reçus ──────────────────────────
void onMessage(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.printf("[MQTT IN] %s → %s\n", topic, msg.c_str());

  // Exemple : répondre à une commande "ping"
  if (String(topic) == TOPIC_CMD && msg == "ping") {
    mqtt.publish(TOPIC_STATUS, "pong");
  }
}

// ── Connexion / reconnexion MQTT ────────────────────────────
void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Connexion MQTT...");

    bool ok = (strlen(MQTT_USER) > 0)
      ? mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD,
                     TOPIC_STATUS, 1, true, "offline")
      : mqtt.connect(MQTT_CLIENT_ID,
                     TOPIC_STATUS, 1, true, "offline");

    if (ok) {
      Serial.println(" connecté");
      mqtt.publish(TOPIC_STATUS, "online", true);   // message persistant
      mqtt.subscribe(TOPIC_CMD);
    } else {
      Serial.printf(" échec (rc=%d), nouvel essai dans 5 s\n", mqtt.state());
      delay(5000);
    }
  }
}

// ── Publication des données capteur ─────────────────────────
void publishSensorData() {
  float temp = dht.readTemperature();
  float humi = dht.readHumidity();

  if (isnan(temp) || isnan(humi)) {
    Serial.println("[DHT22] Erreur de lecture — capteur déconnecté ?");
    return;
  }

  // Payload JSON  { "temperature": 22.4, "humidity": 58.0, "device": "esp32-salon" }
  StaticJsonDocument<128> doc;
  doc["temperature"] = temp;
  doc["humidity"]    = humi;
  doc["device"]      = MQTT_CLIENT_ID;

  char buffer[128];
  serializeJson(doc, buffer);

  if (mqtt.publish(TOPIC_DATA, buffer)) {
    Serial.printf("[MQTT OUT] %s → %s\n", TOPIC_DATA, buffer);
  } else {
    Serial.println("[MQTT OUT] Échec de publication");
  }
}

// ── Setup ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);

  dht.begin();
  connectWiFi();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setKeepAlive(60);
}

// ── Loop ─────────────────────────────────────────────────────
void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected()) connectMQTT();

  mqtt.loop();   // traite les messages entrants

  unsigned long now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;
    publishSensorData();
  }
}

