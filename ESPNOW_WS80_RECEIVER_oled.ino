#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_now.h>

// Display settings
#define DISPLAY_I2C_PIN_RST 16
#define ESP_SDA_PIN 4
#define ESP_SCL_PIN 15
#define DISPLAY_I2C_ADDR 0x3C

// Setup OLED display using U8g2
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, DISPLAY_I2C_PIN_RST, ESP_SCL_PIN, ESP_SDA_PIN);

// Define LED Pin for status indication
#define STATUS_LED 2  // Built-in LED (ESP32)

// Timer to detect missing data
unsigned long lastDataTime = 0;
const unsigned long timeoutPeriod = 10000;  // 10 seconds

// Structure for receiving data
typedef struct struct_message {
  int windDir;
  float windSpeed;
  float windGust;
  float temperature;
  float humidity;
} struct_message;

struct_message receivedData;

// Callback function when ESP-NOW data is received
void onDataReceive(const esp_now_recv_info_t* recv_info, const uint8_t* incomingData, int len) {
  Serial.println("\n📩 NEW DATA RECEIVED!");

  // Blink LED to indicate data reception
  digitalWrite(STATUS_LED, HIGH);
  delay(200);
  digitalWrite(STATUS_LED, LOW);

  // Reset last data time
  lastDataTime = millis();

  // Print Sender's MAC Address
  Serial.print("📡 Sender MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", recv_info->src_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  // Copy received data into our structured format
  memcpy(&receivedData, incomingData, sizeof(receivedData));

  // Print received data to Serial Monitor
  Serial.printf("🌪️ WindDir: %d°\n", receivedData.windDir);
  Serial.printf("💨 WindSpeed: %.1f knots\n", receivedData.windSpeed);
  Serial.printf("🌬️ WindGust: %.1f knots\n", receivedData.windGust);
  Serial.printf("🌡️ Temperature: %.1f°C\n", receivedData.temperature);
  Serial.printf("💧 Humidity: %.1f%%\n", receivedData.humidity);

  // Display data on OLED
  displayData();
}

// Function to display data on OLED
void displayData() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "NEW DATA RECEIVED!");  // Alert Message
  u8g2.drawStr(0, 20, ("WindDir: " + String(receivedData.windDir) + "").c_str());
  u8g2.drawStr(0, 30, ("WindSpeed: " + String(receivedData.windSpeed) + " knots").c_str());
  u8g2.drawStr(0, 40, ("WindGust: " + String(receivedData.windGust) + " knots").c_str());
  u8g2.drawStr(0, 50, ("Temp: " + String(receivedData.temperature) + " C").c_str());
  u8g2.drawStr(0, 60, ("Humi: " + String(receivedData.humidity) + " %").c_str());
  u8g2.sendBuffer();
}

// Function to check if no data has been received
void checkForNoData() {
  if (millis() - lastDataTime > timeoutPeriod) {
    Serial.println("⚠️ NO DATA RECEIVED FOR 10 SECONDS!");
    
    // Display "NO DATA RECEIVED" message
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(20, 30, "NO DATA RECEIVED!");
    u8g2.sendBuffer();
  }
}

void setup() {
  Serial.begin(115200);
// Set the maximum Wi-Fi transmission power (range 0-84)
  // Setup LED
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  // Initialize I2C for OLED display
  Wire.begin(ESP_SDA_PIN, ESP_SCL_PIN);
  u8g2.begin();

  // Display startup message on OLED
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(20, 20, "ESP32 Receiver");
  u8g2.drawStr(10, 40, "Waiting for Data...");
  u8g2.sendBuffer();

  // Initialize ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW Initialization Failed!");
    return;
  }
  Serial.println("✅ ESP-NOW Initialized");

  // Register callback function to receive data
  esp_now_register_recv_cb(onDataReceive);

  Serial.println("🔹 ESP-NOW Receiver Setup Complete");

  // Set initial last data time to current time
  lastDataTime = millis();
}

void loop() {
  checkForNoData();
  delay(1000);  // Small delay to avoid overloading
}
