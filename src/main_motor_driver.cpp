#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Arduino.h>
#include <utility>
#include <Adafruit_HMC5883_U.h>
#include <Adafruit_Sensor.h>

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

const int steps_horizontal = 800;

// =====================================================
// VERTIKALER MOTOR
// =====================================================
const int DIR_V  = 27;
const int STEP_V = 14;

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
// BESTE POSITIONEN (Global gespeichert)
// =====================================================
int bestStepH = 0;
int bestSumH  = 0;

int bestStepV = 0;
int bestSumV  = 0;

float finalPositionH = 0;
float finalPositionV = 0;

// =====================================================
// BESTE SENSORWERTE
// =====================================================
int bestOL = 0;
int bestOR = 0;
int bestUL = 0;
int bestUR = 0;

// =====================================================
// SCHRITTFUNKTIONEN
// =====================================================
void makeStepHorizontal()
{
  digitalWrite(STEP_H, HIGH);
  delayMicroseconds(500);
  digitalWrite(STEP_H, LOW);
  delayMicroseconds(500);
}

void makeStepVertical()
{
  digitalWrite(STEP_V, HIGH);
  delayMicroseconds(500);
  digitalWrite(STEP_V, LOW);
  delayMicroseconds(500);
}

// =====================================================
// REFERENZFAHRT (HOMING)
// =====================================================
void fahreBisEndschalter()
{
  Serial.println("Fahre horizontal zum Endschalter (Nullpunkt)...");
  
  // Richtung festlegen (HIGH oder LOW, je nachdem wo der Schalter sitzt)
  digitalWrite(DIR_H, LOW); 

  while(digitalRead(ENDSCHALTER_PIN) == LOW)
  {
    makeStepHorizontal();
    delay(2);
  }
  
  Serial.println("Endschalter erreicht! Horizontale Position genullt.");
}

// =====================================================
// SENSOR MITTELN
// =====================================================
int readSensor(int pin)
{
  long sum = 0;
  for(int i = 0; i < 10; i++)
  {
    sum += analogRead(pin);
  }
  return sum / 10;
}

// =====================================================
// WLAN VERBINDEN
// =====================================================
void connectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Verbinde WLAN");

  while(WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWLAN verbunden!");
}

// =====================================================
// THINGSPEAK SENDEN
// =====================================================
void sendToThingSpeak(float angleH, float angleV)
{
  float voltOL = bestOL * (referenzSpannung / adcMax);
  float voltOR = bestOR * (referenzSpannung / adcMax);
  float voltUL = bestUL * (referenzSpannung / adcMax);
  float voltUR = bestUR * (referenzSpannung / adcMax);

  HTTPClient http;

  String url =
    "http://api.thingspeak.com/update?api_key=" + apiKey +
    "&field1=" + String(angleH, 2) +
    "&field2=" + String(angleV, 2) +
    "&field3=" + String(bestSumH) +
    "&field4=" + String(bestSumV) +
    "&field5=" + String(voltOL, 3) +
    "&field6=" + String(voltOR, 3) +
    "&field7=" + String(voltUL, 3) +
    "&field8=" + String(voltUR, 3);

  http.begin(url);
  int code = http.GET();

  Serial.println("\nThingSpeak Upload:");
  Serial.print("Horizontaler Winkel: ");  Serial.println(angleH);
  Serial.print("Vertikaler Winkel: ");    Serial.println(angleV);
  Serial.print("Beste horiz. Summe: ");   Serial.println(bestSumH);
  Serial.print("Beste vert. Summe: ");    Serial.println(bestSumV);
  Serial.print("HTTP Code: ");            Serial.println(code);

  http.end();
}

// =====================================================
// SCAN FUNKTION
// =====================================================
std::pair<float, float> findBestPosition()
{
  sensors_event_t event; 
  mag.getEvent(&event);

  float heading = atan2(event.magnetic.y, event.magnetic.x);
  float declinationAngle = 3.00;
  heading += declinationAngle;

  if(heading < 0) heading += 2*PI;
  if(heading > 2*PI) heading -= 2*PI;

  float headingDegrees = heading * 180/M_PI;  

  // =====================================================
  // HORIZONTALER SCAN
  // =====================================================
  bestStepH = 0;
  bestSumH = 0;

  digitalWrite(DIR_H, HIGH);
  Serial.println("\nHORIZONTAL SCAN");

  for(int i = 0; i < steps_horizontal; i++)
  {
    makeStepHorizontal();
    delay(5);

    int valOL = readSensor(OL);
    int valOR = readSensor(OR);
    int valUL = readSensor(UL);
    int valUR = readSensor(UR);

    int sum = valOL + valOR + valUL + valUR;
    float angleH = i * 0.45;
    float angleHorizontalToNorth = headingDegrees + angleH;

    if(sum > bestSumH)
    {
      bestSumH = sum;
      bestStepH = i;
      bestOL = valOL;
      bestOR = valOR;
      bestUL = valUL;
      bestUR = valUR;
    }
  }

  int stepsBackH = steps_horizontal - bestStepH;
  digitalWrite(DIR_H, LOW);

  for(int i = 0; i < stepsBackH; i++)
  {
    makeStepHorizontal();
    delay(5);
  }

  float bestAngleH = bestStepH * 0.45;
  float bestAngleHorizontalToNorth = headingDegrees + bestAngleH;

  delay(1000);

  // =====================================================
  // VERTIKALER SCAN
  // =====================================================
  bestStepV = 0;
  bestSumV = 0;

  digitalWrite(DIR_V, LOW);
  Serial.println("\nVERTICAL SCAN");

  for(int i = 0; i < steps_vertical; i++)
  {
    makeStepVertical();
    delay(5);

    int valOL = readSensor(OL);
    int valOR = readSensor(OR);
    int valUL = readSensor(UL);
    int valUR = readSensor(UR);

    int sum = valOL + valOR + valUL + valUR;
    float angleV = i * 0.45;

    if(sum > bestSumV)
    {
      bestSumV = sum;
      bestStepV = i;
      bestOL = valOL;
      bestOR = valOR;
      bestUL = valUL;
      bestUR = valUR;
    }
  }

  int stepsBackV = steps_vertical - bestStepV;
  digitalWrite(DIR_V, HIGH);

  for(int i = 0; i < stepsBackV; i++)
  {
    makeStepVertical();
    delay(5);
  }

  float bestAngleV = bestStepV * 0.45;

  return {bestAngleHorizontalToNorth, bestAngleV};
}

void setup()
{
  Serial.begin(115200);

  pinMode(STEP_H, OUTPUT);
  pinMode(DIR_H, OUTPUT);
  pinMode(STEP_V, OUTPUT);
  pinMode(DIR_V, OUTPUT);
  pinMode(ENDSCHALTER_PIN, INPUT_PULLUP);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  delay(3000);

  if(!mag.begin())
  {
    Serial.println("Ooops, no HMC5883 detected ... Check your wiring!");
    while(1);
  }
  
  Serial.println("\nSTART SOLAR TRACKER - Periodischer Modus aktiviert");
}


void loop()
{
  // 1. WLAN ausschalten um den ADC nicht durch Funkrauschen zu stören
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  Serial.println("\n================================");
  Serial.println("BEREITE NEUEN SCAN VOR...");

  // 2. Vertikale Achse zurück auf 0 fahren (da kein Endschalter vorhanden)
  // HIGH ist in deiner Funktion die Richtung zurück zur Startposition
  if(bestStepV > 0)
  {
    Serial.println("Fahre vertikale Achse zurück auf Startposition...");
    digitalWrite(DIR_V, HIGH); 
    for(int i = 0; i < bestStepV; i++)
    {
      makeStepVertical();
      delay(5);
    }
    bestStepV = 0; // Vertikal ist wieder auf 0
  }

  // 3. Horizontale Achse zurück auf 0 fahren (mit Endschalter)
  fahreBisEndschalter();
  delay(1000);

  // 4. SCAN DURCHFÜHREN
  std::pair<float, float> currentPosition = findBestPosition();
  finalPositionH = currentPosition.first;
  finalPositionV = currentPosition.second;

  Serial.println("\n================================");
  Serial.println("BESTE POSITION GEFUNDEN");
  Serial.print("Horizontaler Winkel (zu Nord): "); Serial.println(finalPositionH);
  Serial.print("Vertikaler Winkel: "); Serial.println(finalPositionV);
  Serial.println("================================");

  // 5. WLAN einschalten und Upload
  connectWiFi();
  sendToThingSpeak(finalPositionH, finalPositionV);

  // 6. Pause von 5 Minuten (300.000 Millisekunden)
  Serial.println("\nFertig. Warte 5 Minuten bis zum nächsten Durchlauf...");
  delay(300000); 
}