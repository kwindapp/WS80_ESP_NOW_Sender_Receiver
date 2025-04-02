#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_now.h>

// Display settings
#define DISPLAY_I2C_PIN_RST 16
#define ESP_SDA_PIN 4
#define ESP_SCL_PIN 15
#define DISPLAY_I2C_ADDR 0x3C

// Setup OLED display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, DISPLAY_I2C_PIN_RST, ESP_SCL_PIN, ESP_SDA_PIN);

// Serial communication settings
HardwareSerial mySerial(2);  // Use UART2 for serial communication
#define MY_SERIAL_RX_PIN 12   // Change this to match your wiring
#define MY_SERIAL_TX_PIN 13   // Change this to match your wiring

// Structure for ESP-NOW data transmission
typedef struct struct_message {
  int windDir;
  float windSpeed;
  float windGust;
  float temperature;
  float humidity;
} struct_message;

struct_message dataToSend;

// MAC address of the receiver ESP32

uint8_t receiverMAC[] = {0x3C, 0x71, 0xBF, 0xAB, 0x5B, 0x78};

// Serial buffer
String serialBuffer = "";

void setup() {
  Serial.begin(115200);
  mySerial.begin(115200, SERIAL_8N1, MY_SERIAL_RX_PIN, MY_SERIAL_TX_PIN); // Initialize mySerial
  
  WiFi.mode(WIFI_STA);
  Serial.println("WiFi initialized in station mode");

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialization failed");
    return;
  }

  // Add the receiver as a peer
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  Serial.println("ESP-NOW Peer Added Successfully");

  // Initialize OLED display
  Wire.begin(ESP_SDA_PIN, ESP_SCL_PIN);
  u8g2.begin();

  // Display startup message
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 20, "ESP32 Sender");
  u8g2.drawStr(10, 40, "Waiting for Data...");
  u8g2.sendBuffer();
}

void loop() {
  // Read incoming serial data
  while (mySerial.available()) {
    char incomingByte = mySerial.read();
    serialBuffer += incomingByte;
  }

  // Process data if we have a complete message
  if (serialBuffer.indexOf("SHT30") != -1) {
    Serial.println("\nValid data received:");
    Serial.println(serialBuffer);
    
    parseData(serialBuffer);  // Extract wind and environmental data
    serialBuffer = "";  // Clear buffer after processing

    // Send data via ESP-NOW
    sendESPNowData();

    // Display data on OLED
    displayData();
  }

  delay(500); // Small delay to avoid overloading
}

// Function to send parsed data via ESP-NOW
void sendESPNowData() {
  Serial.println("Sending data...");
  esp_err_t result = esp_now_send(receiverMAC, (uint8_t *)&dataToSend, sizeof(dataToSend));
  if (result == ESP_OK) {
    Serial.println("✅ Data sent successfully!");
  } else {
    Serial.println("❌ Failed to send data.");
  }
}

// Function to parse serial data and extract values
void parseData(String data) {
  data.trim();

  if (data.indexOf("WindDir") != -1) {
    dataToSend.windDir = getValue(data, "WindDir");
  }
  if (data.indexOf("WindSpeed") != -1) {
    dataToSend.windSpeed = getValue(data, "WindSpeed") * 1.94384;  // Convert m/s to knots
  }
  if (data.indexOf("WindGust") != -1) {
    dataToSend.windGust = getValue(data, "WindGust") * 1.94384;  // Convert m/s to knots
  }
  if (data.indexOf("Temperature") != -1) {
    dataToSend.temperature = getValue(data, "Temperature");
  }
  if (data.indexOf("Humi") != -1) {
    dataToSend.humidity = getValue(data, "Humi");
  }

  Serial.println("\n📊 Parsed Data:");
  Serial.printf("🌬 WindDir: %d°\n", dataToSend.windDir);
  Serial.printf("💨 WindSpeed: %.1f knots\n", dataToSend.windSpeed);
  Serial.printf("🌪 WindGust: %.1f knots\n", dataToSend.windGust);
  Serial.printf("🌡 Temp: %.1f C\n", dataToSend.temperature);
  Serial.printf("💧 Humi: %.1f %%\n", dataToSend.humidity);
}

// Helper function to extract values from the serial data
float getValue(String data, String key) {
  int keyPos = data.indexOf(key);
  if (keyPos != -1) {
    int startPos = data.indexOf("=", keyPos) + 1;
    int endPos = data.indexOf("\n", startPos);
    String valueString = data.substring(startPos, endPos);
    valueString.trim();
    return valueString.toFloat();
  }
  return 0.0;
}

// Function to display data on OLED
void displayData() {
  Serial.println("\n🖥 Displaying data on OLED...");

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, ("WindDir: " + String(dataToSend.windDir) + "°").c_str());
  u8g2.drawStr(0, 20, ("WindSpeed: " + String(dataToSend.windSpeed) + " knots").c_str());
  u8g2.drawStr(0, 30, ("WindGust: " + String(dataToSend.windGust) + " knots").c_str());
  u8g2.drawStr(0, 40, ("Temp: " + String(dataToSend.temperature) + " C").c_str());
  u8g2.drawStr(0, 50, ("Humi: " + String(dataToSend.humidity) + " %").c_str());
  u8g2.sendBuffer();
}
