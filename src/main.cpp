#include <Arduino.h>
#include <TinyGPS++.h>

// 1. Hardware Selection
#define TINY_GSM_MODEM_SIM800 
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>

// ========================================== 
// ⚙️ CONFIGURATION (EDIT THIS!)
// ========================================== 

// 1. Airtel APN (Classic 2G APN)
const char apn[]      = "airtelgprs.com"; 
const char gprsUser[] = "";
const char gprsPass[] = "";

// 2. YOUR SERVER URL
// ⚠️ IMPORTANT: Remove "https://" and "/" at the end. 
const char server[]   = "ev-fleet-telematics-firmware.onrender.com"; 

const int  port       = 80; 
const String endpoint = "/api/update"; 

// Hardware Pins
#define SIM_TX        16
#define SIM_RX        17
#define GPS_TX        26
#define GPS_RX        27
#define RELAY_PIN     18
#define LED_PIN       19

// Serial Objects
HardwareSerial SerialGSM(2);
HardwareSerial SerialGPS(1);

// Objects
TinyGPSPlus gps;
TinyGsm modem(SerialGSM);
TinyGsmClient client(modem);
HttpClient http(client, server, port);

// Variables
unsigned long lastUploadTime = 0;
const unsigned long uploadInterval = 5000; // Fast 5s updates for smooth tracking

// Prototypes
void sendDataToServer();
void parseResponse(String response);

// ========================================== 
// 🛠️ SETUP
// ========================================== 
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n🚀 STARTING AIRTEL 2G TRACKER...");

  // 1. Init Pins
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Default OFF
  digitalWrite(LED_PIN, LOW);

  // 2. Init Serials
  SerialGPS.begin(9600, SERIAL_8N1, GPS_TX, GPS_RX);
  SerialGSM.begin(9600, SERIAL_8N1, SIM_TX, SIM_RX);

  // 3. Init Modem
  Serial.println("⏳ Initializing SIM800L...");
  if (!modem.restart()) {
    Serial.println("❌ Modem Failed to Start (Check Power)");
  } else {
    Serial.println("✅ Modem Started");
  }

  // 4. Connect to Network (Airtel)
  Serial.print("⏳ Connecting to Airtel...");
  
  // Increase timeout for 2G network search
  if (!modem.waitForNetwork(60000L)) {
    Serial.println(" ❌ Network Not Found (Check Antenna)");
  } else {
    Serial.println(" ✅ Network Connected!");
    
    Serial.print("⏳ Connecting to GPRS (APN: ");
    Serial.print(apn);
    Serial.print(")...");
    
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      Serial.println(" ❌ GPRS Failed");
    } else {
      Serial.println(" ✅ GPRS Connected!");
    }
  }
}

// ========================================== 
// 🔄 LOOP
// ========================================== 
void loop() {
  // 1. Always feed GPS data
  while (SerialGPS.available() > 0) {
    gps.encode(SerialGPS.read());
  }

  // 2. Main Connection Logic
  if (millis() - lastUploadTime > uploadInterval) {
    
    if (modem.isGprsConnected()) {
      sendDataToServer();
    } else {
      Serial.println("⚠️ Link lost. Reconnecting...");
      modem.gprsConnect(apn, gprsUser, gprsPass);
    }
    
    lastUploadTime = millis();
  }
}

// ========================================== 
// 📡 SERVER COMMUNICATION
// ========================================== 
void sendDataToServer() {
  Serial.print("\n📤 Syncing... ");

  // 1. Build JSON Payload
  StaticJsonDocument<256> doc;
  
  // Use 0.0 if GPS is not locked yet
  if (gps.location.isValid()) {
    doc["lat"] = gps.location.lat();
    doc["lng"] = gps.location.lng();
    doc["speed"] = gps.speed.kmph();
  } else {
    doc["lat"] = 0.0;
    doc["lng"] = 0.0;
    doc["speed"] = 0.0;
  }
  
  // Send current hardware state (Feedback loop)
  doc["relay_status"] = digitalRead(RELAY_PIN);
  doc["led_status"] = digitalRead(LED_PIN);

  String payload;
  serializeJson(doc, payload);

  // 2. Send POST Request
  http.beginRequest();
  http.post(endpoint);
  http.sendHeader("Content-Type", "application/json");
  http.sendHeader("Content-Length", payload.length());
  // IMPORTANT: Keep-Alive helps prevent reconnecting every time
  http.sendHeader("Connection", "keep-alive"); 
  http.beginBody();
  http.print(payload);
  http.endRequest();

  // 3. Read Response
  int status = http.responseStatusCode();
  String body = http.responseBody();

  if (status == 200) {
    Serial.println("✅ OK");
    parseResponse(body);
  } else {
    Serial.print("❌ Error: ");
    Serial.println(status);
  }
}

// ========================================== 
// 🧠 COMMAND PROCESSING
// ========================================== 
void parseResponse(String response) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, response);

  if (!error) {
    if (doc.containsKey("relay_cmd")) {
      int cmd = doc["relay_cmd"];
      
      // Only switch if state is different (prevents glitching)
      if (digitalRead(RELAY_PIN) != cmd) {
        Serial.print("   >>> SWITCHING RELAY: ");
        Serial.println(cmd ? "ON" : "OFF");
        
        digitalWrite(RELAY_PIN, cmd);
        digitalWrite(LED_PIN, cmd);
      }
    }
  }
}