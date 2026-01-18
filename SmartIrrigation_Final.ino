/*
 * ═══════════════════════════════════════════════════════════════════
 *  SMART IRRIGATION SYSTEM - IOT PRECISION FARMING
 *  LED Demo Version (No Relay Required)
 *  USB Powered - Laptop Compatible
 * ═══════════════════════════════════════════════════════════════════
 *  
 *  Author: Bro
 *  Project: Final Year Biomedical Engineering
 *  College: PSG College of Technology
 *  Version: 2.0
 *  Date: January 2026
 *  
 * ═══════════════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ═══════════════════════════════════════════════════════════════════
//  CONFIGURATION - MODIFY THESE VALUES
// ═══════════════════════════════════════════════════════════════════

// WiFi Credentials
const char* WIFI_SSID = "YOUR_WIFI_NAME";        // Change this
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"; // Change this

// Backend API Endpoint (if you have one)
const char* API_ENDPOINT = "http://your-backend-url.com/api/sensor-data";
const bool ENABLE_CLOUD_UPLOAD = false;  // Set to true when backend is ready

// Irrigation Thresholds (adjust based on your crop/soil)
const float MOISTURE_THRESHOLD_LOW = 30.0;   // Below this = start irrigation
const float MOISTURE_THRESHOLD_HIGH = 60.0;  // Above this = stop irrigation
const float PH_MIN = 5.5;                    // Minimum safe pH
const float PH_MAX = 7.5;                    // Maximum safe pH
const float TEMP_WARNING = 35.0;             // Temperature warning threshold

// Timing Intervals (milliseconds)
const unsigned long SENSOR_READ_INTERVAL = 2000;   // Read sensors every 2 seconds
const unsigned long CLOUD_UPLOAD_INTERVAL = 30000; // Upload every 30 seconds
const unsigned long LCD_UPDATE_INTERVAL = 1000;    // Update LCD every second

// ═══════════════════════════════════════════════════════════════════
//  PIN DEFINITIONS - DO NOT CHANGE UNLESS WIRING CHANGES
// ═══════════════════════════════════════════════════════════════════

// Sensor Input Pins
#define SOIL_MOISTURE_PIN  34   // Analog input (ADC1_CH6)
#define RAIN_SENSOR_PIN    35   // Digital input (ADC1_CH7)
#define PH_SENSOR_PIN      36   // Analog input (ADC1_CH0)
#define DHT_PIN            4    // Digital I/O
#define DHT_TYPE           DHT22

// LED Output Pins (Direct GPIO Control - No Relay)
#define LED_PUMP_ACTIVE    26   // 🔴 Red - Pump would be running
#define LED_SYSTEM_OK      27   // 🟢 Green - System healthy/WiFi connected
#define LED_UPLOADING      25   // 🔵 Blue - Uploading to cloud
#define LED_WARNING        33   // 🟡 Yellow - Low moisture warning

// I2C Pins (predefined for ESP32)
#define I2C_SDA            21
#define I2C_SCL            22

// ═══════════════════════════════════════════════════════════════════
//  GLOBAL OBJECTS
// ═══════════════════════════════════════════════════════════════════

LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address 0x27 (or 0x3F)
DHT dht(DHT_PIN, DHT_TYPE);

// ═══════════════════════════════════════════════════════════════════
//  DATA STRUCTURES
// ═══════════════════════════════════════════════════════════════════

struct SensorData {
  float soilMoisture;
  bool rainDetected;
  float phValue;
  float temperature;
  float humidity;
  bool pumpActive;
  String systemStatus;
  unsigned long timestamp;
};

SensorData currentData;

// Timing variables
unsigned long lastSensorRead = 0;
unsigned long lastCloudUpload = 0;
unsigned long lastLCDUpdate = 0;

// Sensor calibration values (adjust after testing with real soil)
const int SOIL_DRY_VALUE = 3500;   // ADC reading when completely dry
const int SOIL_WET_VALUE = 1000;   // ADC reading when completely wet

// ═══════════════════════════════════════════════════════════════════
//  SETUP FUNCTION - Runs Once at Startup
// ═══════════════════════════════════════════════════════════════════

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(1000);
  
  printHeader();
  
  // Initialize GPIO pins
  initializePins();
  
  // Initialize LCD Display
  initializeLCD();
  
  // Initialize DHT Sensor
  dht.begin();
  Serial.println("✓ DHT22 sensor initialized");
  
  // Connect to WiFi
  connectWiFi();
  
  // Initialize sensor data structure
  currentData.pumpActive = false;
  currentData.systemStatus = "INITIALIZING";
  
  // Startup sequence complete
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   SYSTEM READY - MONITORING STARTED   ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready!");
  lcd.setCursor(0, 1);
  lcd.print("Monitoring...");
  delay(2000);
}

// ═══════════════════════════════════════════════════════════════════
//  MAIN LOOP - Runs Continuously
// ═══════════════════════════════════════════════════════════════════

void loop() {
  unsigned long currentMillis = millis();
  
  // Read sensors at specified interval
  if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
    readAllSensors();
    controlIrrigation();
    checkWarnings();
    lastSensorRead = currentMillis;
  }
  
  // Update LCD display
  if (currentMillis - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
    updateLCD();
    lastLCDUpdate = currentMillis;
  }
  
  // Upload to cloud at specified interval
  if (ENABLE_CLOUD_UPLOAD && currentMillis - lastCloudUpload >= CLOUD_UPLOAD_INTERVAL) {
    uploadToCloud();
    lastCloudUpload = currentMillis;
  }
  
  // Update LED indicators continuously
  updateLEDs();
  
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️  WiFi disconnected! Reconnecting...");
    connectWiFi();
  }
}

// ═══════════════════════════════════════════════════════════════════
//  INITIALIZATION FUNCTIONS
// ═══════════════════════════════════════════════════════════════════

void printHeader() {
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════════════════════╗");
  Serial.println("║                                                        ║");
  Serial.println("║     SMART IRRIGATION SYSTEM FOR PRECISION FARMING     ║");
  Serial.println("║                   IoT Demo Version                    ║");
  Serial.println("║                                                        ║");
  Serial.println("╚════════════════════════════════════════════════════════╝");
  Serial.println();
  Serial.println("Project: Final Year Biomedical Engineering");
  Serial.println("College: PSG College of Technology");
  Serial.println("Version: 2.0 (LED Demo - No Relay)");
  Serial.println("Power: USB (Laptop/Power Bank)");
  Serial.println();
  Serial.println("════════════════════════════════════════════════════════");
  Serial.println("                 SYSTEM INITIALIZATION");
  Serial.println("════════════════════════════════════════════════════════\n");
}

void initializePins() {
  // Configure LED pins as outputs
  pinMode(LED_PUMP_ACTIVE, OUTPUT);
  pinMode(LED_SYSTEM_OK, OUTPUT);
  pinMode(LED_UPLOADING, OUTPUT);
  pinMode(LED_WARNING, OUTPUT);
  
  // Turn off all LEDs initially
  digitalWrite(LED_PUMP_ACTIVE, LOW);
  digitalWrite(LED_SYSTEM_OK, LOW);
  digitalWrite(LED_UPLOADING, LOW);
  digitalWrite(LED_WARNING, LOW);
  
  // Configure sensor pins
  pinMode(RAIN_SENSOR_PIN, INPUT_PULLUP);  // Pull-up for digital sensor
  
  Serial.println("✓ GPIO pins configured");
  
  // Test LEDs (blink each once)
  Serial.println("✓ Testing LEDs...");
  digitalWrite(LED_PUMP_ACTIVE, HIGH); delay(200); digitalWrite(LED_PUMP_ACTIVE, LOW);
  digitalWrite(LED_SYSTEM_OK, HIGH); delay(200); digitalWrite(LED_SYSTEM_OK, LOW);
  digitalWrite(LED_UPLOADING, HIGH); delay(200); digitalWrite(LED_UPLOADING, LOW);
  digitalWrite(LED_WARNING, HIGH); delay(200); digitalWrite(LED_WARNING, LOW);
  Serial.println("✓ LED test complete");
}

void initializeLCD() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Irrigation");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  Serial.println("✓ LCD display initialized");
  delay(1000);
}

// ═══════════════════════════════════════════════════════════════════
//  WIFI CONNECTION
// ═══════════════════════════════════════════════════════════════════

void connectWiFi() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connecting");
  
  Serial.print("\n⏳ Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    
    // Show progress on LCD
    lcd.setCursor(attempts % 16, 1);
    lcd.print(".");
    
    // Blink green LED while connecting
    digitalWrite(LED_SYSTEM_OK, !digitalRead(LED_SYSTEM_OK));
    attempts++;
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ WiFi Connected Successfully!");
    Serial.print("📶 IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("📡 Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    
    digitalWrite(LED_SYSTEM_OK, HIGH);  // Solid green
    delay(2000);
  } else {
    Serial.println("❌ WiFi Connection Failed!");
    Serial.println("⚠️  System will continue without cloud connectivity");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!");
    lcd.setCursor(0, 1);
    lcd.print("Offline Mode");
    
    digitalWrite(LED_SYSTEM_OK, LOW);
    delay(2000);
  }
}

// ═══════════════════════════════════════════════════════════════════
//  SENSOR READING FUNCTIONS
// ═══════════════════════════════════════════════════════════════════

void readAllSensors() {
  Serial.println("\n┌─────────────────────────────────────┐");
  Serial.println("│      SENSOR READINGS                │");
  Serial.println("└─────────────────────────────────────┘");
  
  // Read Soil Moisture
  readSoilMoisture();
  
  // Read Rain Sensor
  readRainSensor();
  
  // Read pH Sensor
  readpHSensor();
  
  // Read Temperature & Humidity
  readDHT22();
  
  currentData.timestamp = millis();
  
  Serial.println("└─────────────────────────────────────┘\n");
}

void readSoilMoisture() {
  int rawValue = analogRead(SOIL_MOISTURE_PIN);
  
  // Convert ADC reading to percentage (calibrate these values!)
  // Higher ADC = drier soil (for most capacitive sensors)
  currentData.soilMoisture = map(rawValue, SOIL_WET_VALUE, SOIL_DRY_VALUE, 100, 0);
  currentData.soilMoisture = constrain(currentData.soilMoisture, 0, 100);
  
  Serial.print("🌱 Soil Moisture: ");
  Serial.print(currentData.soilMoisture, 1);
  Serial.print("% (ADC: ");
  Serial.print(rawValue);
  Serial.println(")");
}

void readRainSensor() {
  // Rain sensor gives LOW when rain detected (pull-up resistor)
  currentData.rainDetected = (digitalRead(RAIN_SENSOR_PIN) == LOW);
  
  Serial.print("🌧️  Rain Detected: ");
  Serial.println(currentData.rainDetected ? "YES ☔" : "NO ☀️");
}

void readpHSensor() {
  int rawValue = analogRead(PH_SENSOR_PIN);
  
  // Convert ADC to pH (needs calibration with buffer solutions!)
  // This is a simplified linear mapping
  float voltage = rawValue * (3.3 / 4095.0);
  currentData.phValue = 7.0 + ((voltage - 1.65) * 3.5);  // Simplified formula
  currentData.phValue = constrain(currentData.phValue, 0, 14);
  
  Serial.print("🧪 pH Level: ");
  Serial.print(currentData.phValue, 1);
  Serial.print(" (ADC: ");
  Serial.print(rawValue);
  Serial.println(")");
}

void readDHT22() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  
  if (isnan(temp) || isnan(hum)) {
    Serial.println("⚠️  DHT22 read failed! Using last values.");
  } else {
    currentData.temperature = temp;
    currentData.humidity = hum;
  }
  
  Serial.print("🌡️  Temperature: ");
  Serial.print(currentData.temperature, 1);
  Serial.println("°C");
  
  Serial.print("💧 Humidity: ");
  Serial.print(currentData.humidity, 1);
  Serial.println("%");
}

// ═══════════════════════════════════════════════════════════════════
//  IRRIGATION CONTROL LOGIC
// ═══════════════════════════════════════════════════════════════════

void controlIrrigation() {
  bool shouldActivatePump = false;
  String reason = "";
  
  // Decision Logic with Hysteresis
  if (currentData.rainDetected) {
    shouldActivatePump = false;
    reason = "Rain detected - irrigation not needed";
    currentData.systemStatus = "RAIN STOP";
  } 
  else if (currentData.soilMoisture < MOISTURE_THRESHOLD_LOW) {
    shouldActivatePump = true;
    reason = "Soil moisture below threshold";
    currentData.systemStatus = "IRRIGATING";
  }
  else if (currentData.soilMoisture > MOISTURE_THRESHOLD_HIGH) {
    shouldActivatePump = false;
    reason = "Soil moisture adequate";
    currentData.systemStatus = "MONITORING";
  }
  else {
    // Hysteresis zone - maintain current state
    shouldActivatePump = currentData.pumpActive;
    reason = "Hysteresis zone - maintaining state";
    currentData.systemStatus = currentData.pumpActive ? "IRRIGATING" : "MONITORING";
  }
  
  // Update pump state if changed
  if (shouldActivatePump != currentData.pumpActive) {
    currentData.pumpActive = shouldActivatePump;
    
    Serial.println("\n╔════════════════════════════════════════╗");
    if (shouldActivatePump) {
      Serial.println("║  🔴 PUMP ACTIVATED (LED ON)           ║");
    } else {
      Serial.println("║  🟢 PUMP STOPPED (LED OFF)            ║");
    }
    Serial.println("╚════════════════════════════════════════╝");
    Serial.print("Reason: ");
    Serial.println(reason);
    Serial.println();
  }
}

// ═══════════════════════════════════════════════════════════════════
//  WARNING SYSTEM
// ═══════════════════════════════════════════════════════════════════

void checkWarnings() {
  // Check pH levels
  if (currentData.phValue < PH_MIN || currentData.phValue > PH_MAX) {
    Serial.println("⚠️  WARNING: pH out of optimal range!");
    Serial.print("   Current pH: ");
    Serial.print(currentData.phValue, 1);
    Serial.print(" (Optimal: ");
    Serial.print(PH_MIN, 1);
    Serial.print("-");
    Serial.print(PH_MAX, 1);
    Serial.println(")");
  }
  
  // Check temperature
  if (currentData.temperature > TEMP_WARNING) {
    Serial.println("⚠️  WARNING: High temperature detected!");
    Serial.print("   Current: ");
    Serial.print(currentData.temperature, 1);
    Serial.println("°C");
  }
}

// ═══════════════════════════════════════════════════════════════════
//  LED INDICATOR CONTROL
// ═══════════════════════════════════════════════════════════════════

void updateLEDs() {
  // 🔴 Red LED - Pump Active Indicator
  digitalWrite(LED_PUMP_ACTIVE, currentData.pumpActive ? HIGH : LOW);
  
  // 🟢 Green LED - System OK (WiFi connected)
  digitalWrite(LED_SYSTEM_OK, WiFi.status() == WL_CONNECTED ? HIGH : LOW);
  
  // 🟡 Yellow LED - Low Moisture Warning
  bool lowMoisture = currentData.soilMoisture < MOISTURE_THRESHOLD_LOW;
  digitalWrite(LED_WARNING, lowMoisture ? HIGH : LOW);
  
  // 🔵 Blue LED - handled in uploadToCloud() function
}

// ═══════════════════════════════════════════════════════════════════
//  LCD DISPLAY UPDATE
// ═══════════════════════════════════════════════════════════════════

void updateLCD() {
  lcd.clear();
  
  // Line 1: Moisture & Temperature
  lcd.setCursor(0, 0);
  lcd.print("M:");
  lcd.print((int)currentData.soilMoisture);
  lcd.print("% T:");
  lcd.print((int)currentData.temperature);
  lcd.print("C");
  
  // Line 2: Status & Humidity
  lcd.setCursor(0, 1);
  if (currentData.pumpActive) {
    lcd.print("PUMP:ON ");
  } else if (currentData.rainDetected) {
    lcd.print("RAIN    ");
  } else {
    lcd.print("PUMP:OFF");
  }
  
  lcd.print(" H:");
  lcd.print((int)currentData.humidity);
  lcd.print("%");
}

// ═══════════════════════════════════════════════════════════════════
//  CLOUD UPLOAD FUNCTION
// ═══════════════════════════════════════════════════════════════════

void uploadToCloud() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ No WiFi - skipping cloud upload");
    return;
  }
  
  // Blink blue LED while uploading
  digitalWrite(LED_UPLOADING, HIGH);
  
  Serial.println("\n📤 Uploading data to cloud...");
  
  HTTPClient http;
  http.begin(API_ENDPOINT);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);  // 5 second timeout
  
  // Create JSON payload
  StaticJsonDocument<512> doc;
  doc["deviceId"] = "ESP32_SMART_IRRIGATION_001";
  doc["timestamp"] = currentData.timestamp;
  doc["soilMoisture"] = currentData.soilMoisture;
  doc["rainDetected"] = currentData.rainDetected;
  doc["phValue"] = currentData.phValue;
  doc["temperature"] = currentData.temperature;
  doc["humidity"] = currentData.humidity;
  doc["pumpActive"] = currentData.pumpActive;
  doc["systemStatus"] = currentData.systemStatus;
  
  String jsonPayload;
  serializeJson(doc, jsonPayload);
  
  Serial.println("Payload: " + jsonPayload);
  
  int httpCode = http.POST(jsonPayload);
  
  if (httpCode > 0) {
    Serial.print("✅ Upload successful! HTTP Code: ");
    Serial.println(httpCode);
    
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
      String response = http.getString();
      Serial.println("Response: " + response);
    }
  } else {
    Serial.print("❌ Upload failed: ");
    Serial.println(http.errorToString(httpCode));
  }
  
  http.end();
  digitalWrite(LED_UPLOADING, LOW);
}

// ═══════════════════════════════════════════════════════════════════
//  END OF CODE
// ═══════════════════════════════════════════════════════════════════
