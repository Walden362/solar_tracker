#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Arduino.h>
#include <utility>
#include <Adafruit_HMC5883_U.h>
#include <Adafruit_Sensor.h>

// =====================================================
// DEEP SLEEP EINSTELLUNGEN
// =====================================================
#define uS_TO_S_FACTOR 1000000ULL  // Umrechnungsfaktor Mikrosekunden zu Sekunden
#define TIME_TO_SLEEP  300         // Schlafenszeit in Sekunden (5 Minuten = 300)

// =====================================================
// HMC5883 MAGNETOMETER
// =====================================================
Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);

// =====================================================
// WLAN / ThingSpeak
// =====================================================
const char* ssid = "iPhone von Noah";
const char* password = "123456789";
String apiKey = "KP3PLZAHAREB1I9U";

// =====================================================
// HORIZONTALER MOTOR
// =====================================================
const int DIR_H  = 26;
const int STEP_H = 25;
const int EN_H   = 12; // NEU: Enable-Pin für horizontalen Motor (Bitte Pin anpassen!)
const int steps_horizontal = 800;

// =====================================================
// VERTIKALER MOTOR
// =====================================================
const int DIR_V  = 27;
const int STEP_V = 14;
const int EN_V   = 13; // NEU: Enable-Pin für vertikalen Motor (Bitte Pin anpassen!)
const int steps_vertical = 200;

// =====================================================
// SENSOR & ENDSCHALTER PINS
// =====================================================
const int OL = 32;
const int OR = 33;
const int UL = 34;
const int UR = 35;
const int ENDSCHALTER_PIN = 15; // Bitte anpassen

// =====================================================
// ADC
// =====================================================
const float referenzSpannung = 5.0;
const int adcMax = 4095;

// =====================================================
// GLOBALE VARIABLEN
// =====================================================
int bestStepH = 0;
int bestSumH  = 0;
int bestStepV = 0;
int bestSumV  = 0;

float finalPositionH = 0;
float finalPositionV = 0;

int bestOL = 0, bestOR = 0, bestUL = 0, bestUR = 0;

// =====================================================
// SCHRITTFUNKTIONEN
// =====================================================
void makeStepHorizontal() {
  digitalWrite(STEP_H, HIGH);
  delayMicroseconds(500);
  digitalWrite(STEP_H, LOW);
  delayMicroseconds(500);
}

void makeStepVertical() {
  digitalWrite(STEP_V, HIGH);
  delayMicroseconds(500);
  digitalWrite(STEP_V, LOW);
  delayMicroseconds(500);
}

// =====================================================
// REFERENZFAHRT (HOMING)
// =====================================================
void fahreBisEndschalter() {
  Serial.println("Fahre horizontal zum Endschalter (Nullpunkt)...");
  digitalWrite(DIR_H, LOW); 
  while(digitalRead(ENDSCHALTER_PIN) == LOW) {
    makeStepHorizontal();
    delay(2);
  }
  Serial.println("Endschalter erreicht! Horizontale Position genullt.");
}

// =====================================================
// SENSOR MITTELN
// =====================================================
int readSensor(int pin) {
  long sum = 0;
  for(int i = 0; i < 10; i++) sum += analogRead(pin);
  return sum / 10;
}

// =====================================================
// WLAN VERBINDEN
// =====================================================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Verbinde WLAN");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWLAN verbunden!");
}

// =====================================================
// THINGSPEAK SENDEN
// =====================================================
void sendToThingSpeak(float angleH, float angleV) {
  float voltOL = bestOL * (referenzSpannung / adcMax);
  float voltOR = bestOR * (referenzSpannung / adcMax);
  float voltUL = bestUL * (referenzSpannung / adcMax);
  float voltUR = bestUR * (referenzSpannung / adcMax);

  HTTPClient http;
  String url = "http://api.thingspeak.com/update?api_key=" + apiKey +
               "&field1=" + String(angleH, 2) + "&field2=" + String(angleV, 2) +
               "&field3=" + String(bestSumH) + "&field4=" + String(bestSumV) +
               "&field5=" + String(voltOL, 3) + "&field6=" + String(voltOR, 3) +
               "&field7=" + String(voltUL, 3) + "&field8=" + String(voltUR, 3);

  http.begin(url);
  int code = http.GET();
  
  Serial.println("\nThingSpeak Upload beendet. HTTP Code: " + String(code));
  http.end();
}

// =====================================================
// SCAN FUNKTION
// =====================================================
std::pair<float, float> findBestPosition() {
  sensors_event_t event; 
  mag.getEvent(&event);

  float heading = atan2(event.magnetic.y, event.magnetic.x) + 3.00; // declinationAngle
  if(heading < 0) heading += 2*PI;
  if(heading > 2*PI) heading -= 2*PI;
  float headingDegrees = heading * 180/M_PI;  

  // --- HORIZONTALER SCAN ---
  bestStepH = 0; bestSumH = 0;
  digitalWrite(DIR_H, HIGH);
  Serial.println("\nHORIZONTAL SCAN");

  for(int i = 0; i < steps_horizontal; i++) {
    makeStepHorizontal();
    delay(5);

    int sum = readSensor(OL) + readSensor(OR) + readSensor(UL) + readSensor(UR);
    if(sum > bestSumH) {
      bestSumH = sum; bestStepH = i;
      bestOL = readSensor(OL); bestOR = readSensor(OR);
      bestUL = readSensor(UL); bestUR = readSensor(UR);
    }
  }

  int stepsBackH = steps_horizontal - bestStepH;
  digitalWrite(DIR_H, LOW);
  for(int i = 0; i < stepsBackH; i++) {
    makeStepHorizontal();
    delay(5);
  }

  float bestAngleHorizontalToNorth = headingDegrees + (bestStepH * 0.45);
  delay(1000);

  // --- VERTIKALER SCAN ---
  bestStepV = 0; bestSumV = 0;
  digitalWrite(DIR_V, LOW);
  Serial.println("\nVERTICAL SCAN");

  for(int i = 0; i < steps_vertical; i++) {
    makeStepVertical();
    delay(5);

    int sum = readSensor(OL) + readSensor(OR) + readSensor(UL) + readSensor(UR);
    if(sum > bestSumV) {
      bestSumV = sum; bestStepV = i;
      bestOL = readSensor(OL); bestOR = readSensor(OR);
      bestUL = readSensor(UL); bestUR = readSensor(UR);
    }
  }

  int stepsBackV = steps_vertical - bestStepV;
  digitalWrite(DIR_V, HIGH);
  for(int i = 0; i < stepsBackV; i++) {
    makeStepVertical();
    delay(5);
  }

  return {bestAngleHorizontalToNorth, bestStepV * 0.45};
}

void setup() {
  Serial.begin(115200);

  // Pins initialisieren
  pinMode(STEP_H, OUTPUT); pinMode(DIR_H, OUTPUT);
  pinMode(STEP_V, OUTPUT); pinMode(DIR_V, OUTPUT);
  
  // NEU: Enable-Pins als Ausgang definieren
  pinMode(EN_H, OUTPUT);   pinMode(EN_V, OUTPUT);
  
  // NEU: Motortreiber direkt aktivieren (LOW = Aktiv)
  digitalWrite(EN_H, LOW);
  digitalWrite(EN_V, LOW);

  pinMode(ENDSCHALTER_PIN, INPUT_PULLUP);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  delay(1000); 

  // Sensor Check
  if(!mag.begin()) {
    Serial.println("Ooops, no HMC5883 detected ... Check your wiring!");
    
    // NEU: Auch im Fehlerfall Treiber vor dem Sleep ausschalten
    digitalWrite(EN_H, HIGH);
    digitalWrite(EN_V, HIGH);
    
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }
  
  Serial.println("\n=== SOLAR TRACKER AUFGEWACHT ===");
  
  // 1. WLAN während der ADC Messung ausschalten
  WiFi.mode(WIFI_OFF);

  // 2. Horizontalen Motor auf Nullpunkt fahren
  fahreBisEndschalter();
  delay(1000);

  // 3. Scan durchführen (Motoren fahren zur besten Position)
  std::pair<float, float> currentPosition = findBestPosition();
  finalPositionH = currentPosition.first;
  finalPositionV = currentPosition.second;

  Serial.print("Bester Horizontaler Winkel: "); Serial.println(finalPositionH);
  Serial.print("Bester Vertikaler Winkel: "); Serial.println(finalPositionV);

  // 4. WLAN einschalten und senden
  connectWiFi();
  sendToThingSpeak(finalPositionH, finalPositionV);
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // 5. Vertikalen Motor absenken
  if(bestStepV > 0) {
    Serial.println("Senke vertikalen Motor ab (fahre auf Startposition 0)...");
    digitalWrite(DIR_V, HIGH); 
    for(int i = 0; i < bestStepV; i++) {
      makeStepVertical();
      delay(5);
    }
  }

  // NEU: 6. Treiber komplett stromlos schalten (HIGH = Deaktiviert)
  Serial.println("Deaktiviere Motortreiber...");
  digitalWrite(EN_H, HIGH);
  digitalWrite(EN_V, HIGH);

  // 7. Ab in den Deep Sleep
  Serial.println("Alle Aufgaben erledigt. Gehe für 5 Minuten schlafen (Deep Sleep)...");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  
  Serial.flush(); 
  esp_deep_sleep_start();
}

void loop() {
  // Bleibt leer
}