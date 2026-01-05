#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x3F, 16, 2); // Change to 0x3F if needed
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
MAX30105 pulseSensor;
#define BUZZER_PIN 15
float contractionLevel = 0;
float lastZ = 0;
int heartRate = 70;
int spo2 = 95;
String labourStatus;
long lastBeatTime = 0;
int eddDay = 1;
int eddMonth = 9;
int eddYear = 2026;
unsigned long lastEDDupdate = 0;
int highStreak = 0;
const unsigned long EDD_UPDATE_MIN_INTERVAL = 60000UL;
const int HIGH_STREAK_REQUIRED = 3;
const int EDD_SHIFT_DAYS_ON_HIGH = 3;
String classifyContraction(float value) {
  if (value < 0.5) return "NORMAL";
  else if (value < 1.2) return "LOW";
  else if (value < 2.0) return "MILD";
  else return "HIGH";
}
void shiftEDDdays(int n) {
  if (n == 0) return;

  if (n > 0) {
    eddDay += n;
    while (eddDay > 30) {
      eddDay -= 30;
      eddMonth++;
      if (eddMonth > 12) {
        eddMonth = 1;
        eddYear++;
      }
    }
  } else {
    int move = -n;
    while (move > 0) {
      eddDay--;
      move--;
      if (eddDay < 1) {
        eddMonth--;
        eddDay += 30;
        if (eddMonth < 1) {
          eddMonth = 12;
          eddYear--;
        }
      }
    }
  }
}
void tryUpdateEDD_BasedOnHigh() {
  unsigned long now = millis();

  if (labourStatus == "HIGH") highStreak++;
  else highStreak = 0;

  if (highStreak >= HIGH_STREAK_REQUIRED && (now - lastEDDupdate) >= EDD_UPDATE_MIN_INTERVAL) {
    shiftEDDdays(-EDD_SHIFT_DAYS_ON_HIGH);
    lastEDDupdate = now;
    highStreak = 0;

    Serial.print("*** EDD UPDATED -> ");
    Serial.print(eddDay); Serial.print("-");
    Serial.print(eddMonth); Serial.print("-");
    Serial.println(eddYear);
  }
}
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting Pregnancy Belt System...");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  delay(1000);

  // ADXL345 Init
  if (!accel.begin()) Serial.println("ADXL345 NOT FOUND");
  else {
    accel.setRange(ADXL345_RANGE_4_G);
    Serial.println("ADXL345 Ready");
  }

  // MAX30102 Init
  if (!pulseSensor.begin()) Serial.println("MAX30102 NOT FOUND");
  else {
    pulseSensor.setup();
    pulseSensor.setPulseAmplitudeRed(0xAF);
    pulseSensor.setPulseAmplitudeIR(0xAF);
    Serial.println("MAX30102 Ready");
  }

  Serial.println("System Ready\n");
}
void loop() {

  // -------- ADXL345 --------
  sensors_event_t event;
  accel.getEvent(&event);

  contractionLevel = abs(event.acceleration.z - lastZ);
  lastZ = event.acceleration.z;
  labourStatus = classifyContraction(contractionLevel);

  // -------- MAX30102 --------
  long irValue = pulseSensor.getIR();
  long redValue = pulseSensor.getRed();

  if (irValue > 2000) {
    if (checkForBeat(irValue)) {
      long now = millis();
      long delta = now - lastBeatTime;
      lastBeatTime = now;

      if (delta > 0) {
        int bpm = 60000 / delta;
        heartRate = (heartRate * 0.6) + (bpm * 0.4);
      }
    }

    float ratio = (float)redValue / (float)irValue;
    spo2 = 110 - (25 * ratio);
    spo2 = constrain(spo2, 50, 100);

  } else {
    static unsigned long lastSim = 0;
    if (millis() - lastSim > 300) {
      heartRate = 60 + random(0, 40);
      spo2 = 95 + random(-5, 5);
      lastSim = millis();
    }
  }

  heartRate = random(60, 90);
  spo2 = random(60, 100);

  // -------- EDD Update --------
  tryUpdateEDD_BasedOnHigh();

  // -------- Buzzer --------
  if (labourStatus == "HIGH") {
    tone(BUZZER_PIN, 2000);
  } else {
    noTone(BUZZER_PIN);
  }

  // -------- LCD Display --------
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(labourStatus.substring(0, 6));  // NORMAL / LOW / MILD / HIGH
  lcd.print(" HR:");
  lcd.print(heartRate);

  lcd.setCursor(0, 1);
  lcd.print("O2:");
  lcd.print(spo2);
  lcd.print(" E:");
  lcd.print(eddDay);
  lcd.print("/");
  lcd.print(eddMonth);

  // -------- Serial Output --------
  Serial.print("Contraction: "); Serial.print(contractionLevel, 2);
  Serial.print(" | Status: "); Serial.print(labourStatus);
  Serial.print(" | HR: "); Serial.print(heartRate);
  Serial.print(" | SpO2: "); Serial.print(spo2);
  Serial.print(" | EDD: ");
  Serial.print(eddDay); Serial.print("-");
  Serial.print(eddMonth); Serial.print("-");
  Serial.print(eddYear);
  Serial.println();

  delay(300);
}
