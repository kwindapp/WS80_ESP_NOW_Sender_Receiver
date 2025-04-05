#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_now.h>
#include <PubSubClient.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>

bool useMQTT = true;

// Display settings
#define DISPLAY_I2C_PIN_RST 16
#define ESP_SDA_PIN 4
#define ESP_SCL_PIN 15

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, DISPLAY_I2C_PIN_RST, ESP_SCL_PIN, ESP_SDA_PIN);

#define STATUS_LED 2

unsigned long lastDataTime = 0;
const unsigned long timeoutPeriod = 30000;

unsigned long lastMqttSendTime = 0;
const unsigned long mqttSendInterval = 20000;

// MQTT settings
const char* mqtt_server = "xxxxxxxxxxx";
WiFiClient espClient;
PubSubClient mqttClient(espClient);
const char* mqttTopic = "KWind/data/WS80_Lora";

// Structure
typedef struct struct_message {
  int windDir;
  float windSpeed;
  float windGust;
  float temperature;
  float humidity;
} struct_message;

struct_message receivedData;

// 🔄 Degrees to cardinal
String getCardinalDirection(int degrees) {
  const char* directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int index = ((degrees + 22) % 360) / 45;
  return String(directions[index]);
}

// 🖨 Print all data
void printData() {
  Serial.println("🖨️ Displaying Current Weather Data:");
  Serial.printf("🌪️ WindDir: %d° (%s)\n", receivedData.windDir, getCardinalDirection(receivedData.windDir).c_str());
  Serial.printf("💨 WindSpeed: %.1f knots\n", receivedData.windSpeed);
  Serial.printf("🌬️ WindGust: %.1f knots\n", receivedData.windGust);
  Serial.printf("🌡️ Temperature: %.1f°C\n", receivedData.temperature);
  Serial.printf("💧 Humidity: %.1f%%\n", receivedData.humidity);
}

// 📟 OLED Display
void displayData() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Latest Weather Data:");
  u8g2.drawStr(0, 20, ("WindDir: " + String(receivedData.windDir) + " " + getCardinalDirection(receivedData.windDir)).c_str());
  u8g2.drawStr(0, 30, ("Wind: " + String(receivedData.windSpeed) + " knots").c_str());
  u8g2.drawStr(0, 40, ("Gust: " + String(receivedData.windGust) + " knots").c_str());
  u8g2.drawStr(0, 50, ("Temp: " + String(receivedData.temperature) + " C").c_str());
  u8g2.drawStr(0, 60, ("Humi: " + String(receivedData.humidity) + " %").c_str());
  u8g2.sendBuffer();
}

// 📩 ESP-NOW Receive
void onDataReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  Serial.println("\n📩 ESP-NOW Data Received");

  digitalWrite(STATUS_LED, HIGH);
  delay(100);
  digitalWrite(STATUS_LED, LOW);

  memcpy(&receivedData, data, sizeof(receivedData));
  lastDataTime = millis();

  printData();
  displayData();
}

// ⚠️ Timeout check
void checkForNoData() {
  if (millis() - lastDataTime > timeoutPeriod) {
    Serial.println("⚠️ NO DATA RECEIVED FOR 30 SECONDS");
    // Don't clear display
  }
}

// 🌐 WiFi connect
void setupWiFi() {
  WiFiManager wifiManager;
  wifiManager.autoConnect("ESP32_AP");
  Serial.println("✅ Wi-Fi Connected");

  int currentChannel = WiFi.channel();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  Serial.printf("📡 Locked Channel: %d\n", currentChannel);
}

// 🔌 MQTT reconnect
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("MQTT connecting...");
    if (mqttClient.connect("LoRaReceiverClient")) {
      Serial.println("✅ MQTT Connected");
      mqttClient.subscribe(mqttTopic);  // 👈 Ensure we get data too
    } else {
      Serial.print("❌ failed, rc=");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }
}

// 📥 MQTT Receive + Parse
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("📥 MQTT Message Received");

  String jsonStr;
  for (unsigned int i = 0; i < length; i++) {
    jsonStr += (char)payload[i];
  }
  Serial.println(jsonStr);

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error) {
    Serial.print("❌ JSON error: ");
    Serial.println(error.f_str());
    return;
  }

  receivedData.windDir     = doc["wind_dir_deg"] | 0;
  receivedData.windSpeed   = doc["wind_avg_m_s"] | 0.0;
  receivedData.windGust    = doc["wind_max_m_s"] | 0.0;
  receivedData.temperature = doc["temperature_C"] | 0.0;
  receivedData.humidity    = doc["Humi"] | 0.0;

  lastDataTime = millis();

  printData();
  displayData();
}

// 📤 MQTT send
void sendDataToMQTT() {
  String payload = String("{\"model\":\"WS80_LoraReceiver\", ") +
                   "\"id\":\"KWind_Lora\", " +
                   "\"wind_dir_deg\":" + String(receivedData.windDir) + ", " +
                   "\"wind_avg_m_s\":" + String(receivedData.windSpeed) + ", " +
                   "\"wind_max_m_s\":" + String(receivedData.windGust) + ", " +
                   "\"temperature_C\":" + String(receivedData.temperature) + ", " +
                   "\"Humi\":" + String(receivedData.humidity) + "}";

  Serial.println("📤 Sending to MQTT:");
  Serial.println(payload);

  if (mqttClient.publish(mqttTopic, payload.c_str())) {
    Serial.println("✅ MQTT Send Success");
  } else {
    Serial.println("❌ MQTT Send Failed");
  }
}

// 🔧 Setup
void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  Wire.begin(ESP_SDA_PIN, ESP_SCL_PIN);
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(20, 20, "ESP32 Receiver");
  u8g2.drawStr(10, 40, "Waiting for Data...");
  u8g2.sendBuffer();

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW Init Failed");
    return;
  }
  esp_now_register_recv_cb(onDataReceive);
  Serial.println("✅ ESP-NOW Ready");

  setupWiFi();

  if (useMQTT) {
    mqttClient.setServer(mqtt_server, 1883);
    mqttClient.setCallback(mqttCallback);
  }

  lastDataTime = millis();
  lastMqttSendTime = millis();
}

// 🔁 Loop
void loop() {
  checkForNoData();

  if (useMQTT && (millis() - lastMqttSendTime > mqttSendInterval)) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    sendDataToMQTT();
    lastMqttSendTime = millis();
  }

  if (useMQTT) {
    mqttClient.loop();
  }

  delay(500);
}
