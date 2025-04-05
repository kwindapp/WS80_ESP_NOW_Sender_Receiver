#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>

// Wi-Fi Credentials
const char* ssid = "KWindMobile";   // Replace with your Wi-Fi SSID
const char* password = "12345678";  // Replace with your Wi-Fi password

// Display settings
#define DISPLAY_I2C_PIN_RST 16
#define ESP_SDA_PIN 4
#define ESP_SCL_PIN 15
#define DISPLAY_I2C_ADDR 0x3C

// MQTT settings (optional, controlled by useMQTT)
const char* mqtt_server = "152.xxxxxxxx";  // Replace with your MQTT server address
WiFiClient espClient;
PubSubClient mqttClient(espClient);
const char* mqttTopic = "KWind/data/WS80_Lora";

bool useMQTT = false;   // Set to true to enable MQTT, false to disable
bool useKnots = true;  // Set to true to show wind speed in knots, false to show in m/s

// Setup OLED display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, DISPLAY_I2C_PIN_RST, ESP_SCL_PIN, ESP_SDA_PIN);

// Structure for received data
typedef struct struct_message {
  int windDir;
  float windSpeed;
  float windGust;
  float temperature;
  float humidity;
  float BatVoltage;  // Battery voltage added
} struct_message;

struct_message receivedData;

// ESP-NOW receive callback function
void onDataRecv(const esp_now_recv_info* info, const uint8_t* data, int len) {
  Serial.println("\n📩 Data Received:");
  Serial.print("Length: ");
  Serial.println(len);

  // Print raw data
  for (int i = 0; i < len; i++) {
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // If the data length is correct, parse the data
  if (len == sizeof(struct_message)) {
    memcpy(&receivedData, data, sizeof(struct_message));

    Serial.println("📊 Parsed Data:");
    Serial.printf("🧭 WindDir: %d° (%s)\n", receivedData.windDir, getCardinalDirection(receivedData.windDir).c_str());
    Serial.printf("💨 WindSpeed: %.1f %s\n", convertWindSpeed(receivedData.windSpeed), useKnots ? "knots" : "m/s");
    Serial.printf("🌬️ WindGust: %.1f %s\n", convertWindSpeed(receivedData.windGust), useKnots ? "knots" : "m/s");
    Serial.printf("🌡️ Temp: %.1f°C\n", receivedData.temperature);
    Serial.printf("💧 Humidity: %.1f%%\n", receivedData.humidity);
    Serial.printf("🔋 BatVoltage: %.2f V\n", receivedData.BatVoltage); // Display battery voltage

    displayData();

    // If MQTT is enabled, send data to MQTT
    if (useMQTT) {
      sendDataToMQTT();
    }
  }
}

// Function to get the cardinal direction based on wind direction
String getCardinalDirection(int windDir) {
  if (windDir >= 0 && windDir < 22.5) return "N";
  if (windDir >= 22.5 && windDir < 67.5) return "NE";
  if (windDir >= 67.5 && windDir < 112.5) return "E";
  if (windDir >= 112.5 && windDir < 157.5) return "SE";
  if (windDir >= 157.5 && windDir < 202.5) return "S";
  if (windDir >= 202.5 && windDir < 247.5) return "SW";
  if (windDir >= 247.5 && windDir < 292.5) return "W";
  if (windDir >= 292.5 && windDir < 337.5) return "NW";
  return "N";  // Default to North if something goes wrong
}

// Function to convert wind speed to the desired units (m/s or knots)
float convertWindSpeed(float speed) {
  if (useKnots) {
    return speed * 1.94384;  // Convert m/s to knots
  }
  return speed;  // Return m/s if useKnots is false
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


// Function to send data to MQTT
void sendDataToMQTT() {
  String payload = String("{\"model\":\"WS80_LoraReceiver\", ") + "\"id\":\"KWind_Lora\", " + "\"wind_dir_deg\":" + String(receivedData.windDir) + ", " + "\"wind_avg_m_s\":" + String(receivedData.windSpeed) + ", " + "\"wind_max_m_s\":" + String(receivedData.windGust) + ", " + "\"temperature_C\":" + String(receivedData.temperature) + ", " + "\"Humi\":" + String(receivedData.humidity) + ", " + "\"BatVoltage\":" + String(receivedData.BatVoltage) + "}";

  Serial.println("📤 Sending to MQTT:");
  Serial.println(payload);

  if (mqttClient.publish(mqttTopic, payload.c_str())) {
    Serial.println("✅ MQTT Send Success");
  } else {
    Serial.println("❌ MQTT Send Failed");
  }
}

// Function to reconnect to MQTT if disconnected
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("MQTT connecting...");
    if (mqttClient.connect("LoRaReceiverClient")) {
      Serial.println("✅ MQTT Connected");
     // mqttClient.subscribe(mqttTopic);  // Ensure we receive data too
    } else {
      Serial.print("❌ failed, rc=");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize OLED display before any Wi-Fi or MQTT operations
  Wire.begin(ESP_SDA_PIN, ESP_SCL_PIN);
  u8g2.begin();

  // Initialize ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialization failed");
    return;
  }

  // Register the data receive callback
  esp_now_register_recv_cb(onDataRecv);

  Serial.println("ESP-NOW initialized and ready");

  // If MQTT is enabled, only then connect to Wi-Fi
  if (useMQTT) {
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\n✅ Connected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Initialize MQTT only after Wi-Fi connection is successful
    mqttClient.setServer(mqtt_server, 1883);
    mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
      Serial.println("📥 MQTT Message Received");
      // Handle incoming MQTT messages here if needed
    });
  } else {
    Serial.println("Wi-Fi not connected since MQTT is disabled.");
  }
}

void loop() {
  // If MQTT is enabled, ensure MQTT connection is maintained
  if (useMQTT) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    mqttClient.loop();  // Handle MQTT communication in background
  }

  // Main loop continues to run to keep displaying data
  delay(100);  // Small delay to avoid overwhelming the loop
}
