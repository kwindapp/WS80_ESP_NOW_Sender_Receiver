#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>

// Wi-Fi Credentials
const char* ssid = "KWindMobile";
const char* password = "12345678";

// Display settings
#define DISPLAY_I2C_PIN_RST 16
#define ESP_SDA_PIN 4
#define ESP_SCL_PIN 15
#define DISPLAY_I2C_ADDR 0x3C

// MQTT settings
const char* mqtt_server = "152.53.16.228";
WiFiClient espClient;
PubSubClient mqttClient(espClient);
const char* mqttTopic = "KWind/data/WS80_Lora";

bool useMQTT = false;
bool useKnots = true;
unsigned long mqttInterval = 10000;  // Default 30 seconds
unsigned long lastMQTTSendTime = 0;

// OLED display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, DISPLAY_I2C_PIN_RST, ESP_SCL_PIN, ESP_SDA_PIN);

// ESP-NOW data structure
typedef struct struct_message {
  int windDir;
  float windSpeed;
  float windGust;
  float temperature;
  float humidity;
  float BatVoltage;
} struct_message;

struct_message receivedData;

// 🧭 Cardinal direction from degrees
String getCardinalDirection(int windDir) {
  if (windDir >= 0 && windDir < 22.5) return "N";
  if (windDir < 67.5) return "NE";
  if (windDir < 112.5) return "E";
  if (windDir < 157.5) return "SE";
  if (windDir < 202.5) return "S";
  if (windDir < 247.5) return "SW";
  if (windDir < 292.5) return "W";
  if (windDir < 337.5) return "NW";
  return "N";
}

// 💨 Convert wind speed if needed
float convertWindSpeed(float speed) {
  return useKnots ? speed * 1.94384 : speed;
}

// 🖥 Display on OLED + Serial
void displayData() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  String windDirStr = String(receivedData.windDir) + "   " + getCardinalDirection(receivedData.windDir);
  String windSpeedStr = String(convertWindSpeed(receivedData.windSpeed)) + (useKnots ? " knots" : " m/s");
  String windGustStr = String(convertWindSpeed(receivedData.windGust)) + (useKnots ? " knots" : " m/s");

  u8g2.drawStr(0, 10, ("Wind   : " + windSpeedStr).c_str());
  u8g2.drawStr(0, 20, ("Gust   : " + windGustStr).c_str());
  u8g2.drawStr(0, 30, ("Dir    : " + windDirStr).c_str());
  u8g2.drawStr(0, 40, ("Temp   : " + String(receivedData.temperature) + " C").c_str());
  u8g2.drawStr(0, 50, ("Humi   : " + String(receivedData.humidity) + " %").c_str());
  u8g2.drawStr(0, 60, ("Batt   : " + String(receivedData.BatVoltage) + "  V").c_str());
  u8g2.sendBuffer();

  Serial.println("\n🖥 OLED Display Data:");
  Serial.println("🧭 WindDir: " + windDirStr);
  Serial.println("💨 WindSpeed: " + windSpeedStr);
  Serial.println("🌬 WindGust: " + windGustStr);
  Serial.println("🌡 Temperature: " + String(receivedData.temperature) + " °C");
  Serial.println("💧 Humidity: " + String(receivedData.humidity) + " %");
  Serial.println("🔋 Battery Voltage: " + String(receivedData.BatVoltage) + "  V");
}

// 📤 Send to MQTT with interval check
void sendDataToMQTT() {
  String payload = String("{\"model\":\"WS80_LoraReceiver\", ") + 
                   "\"id\":\"KWind_Lora\", " + 
                   "\"wind_dir_deg\":" + String(receivedData.windDir) + ", " + 
                   "\"wind_avg_m_s\":" + String(receivedData.windSpeed) + ", " + 
                   "\"wind_max_m_s\":" + String(receivedData.windGust) + ", " + 
                   "\"temperature_C\":" + String(receivedData.temperature) + ", " + 
                   "\"Humi\":" + String(receivedData.humidity) + ", " + 
                   "\"BatVoltage\":" + String(receivedData.BatVoltage) + "}";

  Serial.println("\n📤 Sending to MQTT:");
  Serial.println(payload);

  if (mqttClient.publish(mqttTopic, payload.c_str())) {
    Serial.println("✅ MQTT Send Success");
  } else {
    Serial.println("❌ MQTT Send Failed");
  }
}

// 🔌 MQTT reconnect
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("MQTT connecting...");
    if (mqttClient.connect("LoRaReceiverClient")) {
      Serial.println("✅ MQTT Connected");
      mqttClient.subscribe(mqttTopic);
    } else {
      Serial.print("❌ failed, rc=");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }
}

// 🕹️ Optional: Serial command to set interval
void serialCommandHandler() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.startsWith("interval=")) {
      mqttInterval = input.substring(9).toInt();
      Serial.printf("🛠 MQTT interval updated to %lu ms\n", mqttInterval);
    }
  }
}

// 📩 ESP-NOW callback
void onDataRecv(const uint8_t* mac_addr, const uint8_t* data, int len) {
  Serial.println("\n📩 Data Received:");
  Serial.printf("Length: %d bytes\n", len);
  for (int i = 0; i < len; i++) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();

  if (len == sizeof(struct_message)) {
    memcpy(&receivedData, data, sizeof(receivedData));

    Serial.println("📊 Parsed Data:");
    Serial.printf("🧭 WindDir: %d° (%s)\n", receivedData.windDir, getCardinalDirection(receivedData.windDir).c_str());
    Serial.printf("💨 WindSpeed: %.2f %s\n", convertWindSpeed(receivedData.windSpeed), useKnots ? "knots" : "m/s");
    Serial.printf("🌬 WindGust: %.2f %s\n", convertWindSpeed(receivedData.windGust), useKnots ? "knots" : "m/s");
    Serial.printf("🌡 Temp: %.1f °C\n", receivedData.temperature);
    Serial.printf("💧 Humi: %.1f %%\n", receivedData.humidity);
    Serial.printf("🔋 BatVoltage: %.2f V\n", receivedData.BatVoltage);

    displayData();

    if (useMQTT) {
      unsigned long currentTime = millis();
      Serial.printf("⏱ MQTT interval: %lu ms, elapsed: %lu ms\n", mqttInterval, currentTime - lastMQTTSendTime);
      if (currentTime - lastMQTTSendTime >= mqttInterval) {
        sendDataToMQTT();
        lastMQTTSendTime = currentTime;
      } else {
        Serial.println("⏳ MQTT send skipped (interval not reached)");
      }
    }
  } else {
    Serial.println("❌ Received data length mismatch");
  }
}

void setup() {
  Serial.begin(115200);

  if (useMQTT) {
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\n✅ WiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  }

  Wire.begin(ESP_SDA_PIN, ESP_SCL_PIN);
  u8g2.begin();

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(onDataRecv);
  Serial.println("✅ ESP-NOW initialized");

  if (useMQTT) {
    mqttClient.setServer(mqtt_server, 1883);
    mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
      Serial.println("📥 MQTT message received");
    });
  }
}

void loop() {
  if (useMQTT && !mqttClient.connected()) {
    Serial.println("🔌 MQTT disconnected, reconnecting...");
    reconnectMQTT();
  }

  if (useMQTT) {
    mqttClient.loop();
  }

  serialCommandHandler();
  delay(100);
}
