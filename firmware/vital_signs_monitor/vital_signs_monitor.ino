#include <WiFi.h>
#include <ThingSpeak.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "heartRate.h"
#include "ClosedCube_MAX30205.h"
#include "config.h"   // WiFi & ThingSpeak credentials — see config.example.h

/* ===================== WiFi & ThingSpeak ===================== */
const char* ssid       = WIFI_SSID;
const char* password   = WIFI_PASSWORD;

unsigned long myChannelNumber = THINGSPEAK_CHANNEL_NUMBER;
const char* myWriteAPIKey     = THINGSPEAK_WRITE_API_KEY;

WiFiClient client;

/* ===================== I2C Pins ===================== */
#define SDA_PIN 21
#define SCL_PIN 22

/* ===================== Physiological Thresholds ===================== */
// Heart Rate (bpm)
const int HR_THRESHOLD_HIGH = 140;
const int HR_THRESHOLD_LOW  = 40;

// SpO2 (%)
const int SPO2_THRESHOLD_LOW = 50;

// Body Temperature (°C)
const float TEMP_THRESHOLD_HIGH = 44.0;
const float TEMP_THRESHOLD_LOW  = 30.0;

/* ===================== Sensors ===================== */
MAX30105 particleSensor;
ClosedCube_MAX30205 tempSensor;

/* ===================== MAX30102 Buffers ===================== */
const int BUFFER_LEN = 100;
uint32_t irBuffer[BUFFER_LEN];
uint32_t redBuffer[BUFFER_LEN];

/* ===================== Measured Values ===================== */
int32_t heartRate = -1;
int32_t spo2 = -1;
float bodyTemp = -1;

/* ===== AI Risk Probability ===== */
float aiRiskProbability = -1.0;

/* ===================== Sensor Presence Flags ===================== */
bool found_MAX30102 = false;
bool found_MAX30205 = false;

/* ===================== I2C Utility ===================== */
bool i2cExists(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

/* ============================================================= */
/*              AI: Logistic Regression (Embedded)               */
/* ============================================================= */

// Logistic Regression weights (knowledge-informed)
const float W_TEMP = -1.2;   // Low temperature increases risk
const float W_HR   =  1.5;   // High heart rate increases risk
const float W_SPO2 = -0.6;   // Low SpO2 increases risk
const float BIAS   = -5.0;

// Sigmoid activation
float sigmoid(float x) {
  return 1.0 / (1.0 + exp(-x));
}

// Hypoglycemia risk probability estimator
float hypoglycemiaProbability(float temp, float hr, float spo2) {

  // Normalize inputs to physiological ranges
  float temp_n = (temp - 37.0) / 5.0;   // Normalized body temp
  float hr_n   = (hr   - 75.0) / 40.0;  // Normalized HR
  float spo2_n = (spo2 - 95.0) / 10.0;  // Normalized SpO2

  float z = W_TEMP * temp_n +
            W_HR   * hr_n +
            W_SPO2 * spo2_n +
            BIAS;

  return sigmoid(z); // Probability [0,1]
}

/* ===================== SETUP ===================== */
void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  /* -------- I2C Scan -------- */
  Serial.println("\n--- I2C Scan ---");
  for (int i = 1; i < 127; i++) {
    if (i2cExists(i)) {
      Serial.printf("Found I2C device at 0x%02X\n", i);
    }
  }
  Serial.println("--- Scan Done ---\n");

  /* -------- MAX30102 -------- */
  if (i2cExists(0x57)) {
    found_MAX30102 = true;
    particleSensor.begin(Wire, I2C_SPEED_FAST);
    particleSensor.setup(50, 1, 2, 100, 69, 4096);
    Serial.println("MAX30102 initialized.");
  }

  /* -------- MAX30205 -------- */
  if (i2cExists(0x48)) {
    found_MAX30205 = true;
    tempSensor.begin(0x48);
    Serial.println("MAX30205 initialized.");
  }

  /* -------- WiFi -------- */
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");

  ThingSpeak.begin(client);
}

/* ===================== LOOP ===================== */
void loop() {

  bool ruleAlert = false;
  bool aiAlert   = false;

  /* -------- Read Temperature -------- */
  if (found_MAX30205) {
    bodyTemp = tempSensor.readTemperature();
    Serial.printf("Body Temp: %.2f °C\n", bodyTemp);

    if (bodyTemp < TEMP_THRESHOLD_LOW || bodyTemp > TEMP_THRESHOLD_HIGH)
      ruleAlert = true;
  }

  /* -------- Read HR & SpO2 -------- */
  if (found_MAX30102) {
    for (int i = 0; i < BUFFER_LEN; i++) {
      while (!particleSensor.available()) particleSensor.check();
      redBuffer[i] = particleSensor.getRed();
      irBuffer[i]  = particleSensor.getIR();
      particleSensor.nextSample();
    }

    int8_t validHR, validSPO2;
    maxim_heart_rate_and_oxygen_saturation(
      irBuffer, BUFFER_LEN, redBuffer,
      &spo2, &validSPO2,
      &heartRate, &validHR
    );

    Serial.printf("HR=%d (V:%d) | SPO2=%d (V:%d)\n",
                  heartRate, validHR, spo2, validSPO2);

    if (validHR && (heartRate < HR_THRESHOLD_LOW || heartRate > HR_THRESHOLD_HIGH))
      ruleAlert = true;

    if (validSPO2 && spo2 < SPO2_THRESHOLD_LOW)
      ruleAlert = true;

    if (!validHR) heartRate = -1;
    if (!validSPO2) spo2 = -1;
  }

  /* =====================================================
     AI-based Hypoglycemia Detection (Logistic Regression)
     ===================================================== */
  if (bodyTemp > 0 && heartRate > 0 && spo2 > 0) {

    aiRiskProbability = hypoglycemiaProbability(bodyTemp, heartRate, spo2);
    Serial.printf("AI Hypoglycemia Risk Probability: %.3f\n", aiRiskProbability);

    if (aiRiskProbability > 0.7) {
      aiAlert = true;
      Serial.println("AI ALERT: Possible Hypoglycemia!");
    }
  }

  /* -------- Final Alert Decision -------- */
  bool finalAlert = ruleAlert || aiAlert;

  ThingSpeak.setField(1, bodyTemp);
  ThingSpeak.setField(2, heartRate);
  ThingSpeak.setField(3, spo2);
  ThingSpeak.setField(4, finalAlert ? 1 : 0);
  ThingSpeak.setField(5, aiRiskProbability);

  Serial.println(finalAlert ? "ALERT STATUS: TRIGGERED" : "ALERT STATUS: NORMAL");

  int status = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  Serial.println(status == 200 ? "ThingSpeak update OK" : "ThingSpeak update FAILED");

  delay(20000); // ThingSpeak rate limit
}
