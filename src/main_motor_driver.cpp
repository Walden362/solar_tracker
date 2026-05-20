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

// NEU: Endschalter Pin (Beispiel Pin 15, bitte anpassen)
const int ENDSCHALTER_PIN = 15; 

// =====================================================
// ADC
// =====================================================
const float referenzSpannung = 5.0;
const int adcMax = 4095;

// =====================================================
// BESTE POSITIONEN
// =====================================================
int bestStepH = 0;
int bestSumH  = 0;

int bestStepV = 0;
int bestSumV  = 0;

// Globale Variablen für loop()
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

// NEU: Funktion um zum Endschalter zu fahren (Referenzfahrt)
void fahreBisEndschalter()
{
  Serial.println("Fahre zum Endschalter...");
  
  // Richtung festlegen (HIGH oder LOW, je nachdem wo der Schalter sitzt)
  digitalWrite(DIR_H, LOW); 

  // Da der Schalter ein Öffner ist (NC) und INPUT_PULLUP verwendet wird:
  // Schalter NICHT gedrückt = LOW (Strom fließt nach GND)
  // Schalter gedrückt = HIGH (Kontakt offen, Pullup zieht hoch)
  while(digitalRead(ENDSCHALTER_PIN) == LOW)
  {
    makeStepHorizontal();
    delay(2); // Kurze Pause, damit der Motor nicht zu schnell dreht
  }
  
  Serial.println("Endschalter erreicht! Position genullt.");
  // Hier könntest du interne Positions-Counter auf 0 setzen, falls du welche nutzt.
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

std::pair<float, float> findBestPosition()
{
  // =====================================================
  // ZUR AUSGANGSPOSITION UND KALIBRIERUNG IN BEZUG AUF NORDEN
  // =====================================================
  
  // Fehlerbehebung: event wurde zuvor nicht deklariert und initialisiert
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

  // =====================================================
  // ZUR BESTEN HORIZONTALEN POSITION
  // =====================================================
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

  // =====================================================
  // ZUR BESTEN VERTIKALEN POSITION
  // =====================================================
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

void nachstellenPosition(float startPositionH, float startPositionV)
{
  int horizontalNachstellen = 1;
  int vertikalNachstellen = 1;

  while(horizontalNachstellen == 1 || vertikalNachstellen == 1)
  {
    int valOL = readSensor(OL);
    int valOR = readSensor(OR);
    int valUL = readSensor(UL);
    int valUR = readSensor(UR);

    int diffHorizontal = (valOL + valUL) - (valOR + valUR);
    int diffVertical = (valOL + valOR) - (valUL + valUR);

    if(diffHorizontal > 50  && horizontalNachstellen == 1)
    {
      digitalWrite(DIR_H, HIGH);
      makeStepHorizontal();
    }
    else if(diffHorizontal < -50 && horizontalNachstellen == 1)
    {
      digitalWrite(DIR_H, LOW);
      makeStepHorizontal();
    }else{
      horizontalNachstellen = 0;
    }

    if(diffVertical > 50 && vertikalNachstellen == 1)
    {
      digitalWrite(DIR_V, HIGH);
      makeStepVertical();
    }
    else if(diffVertical < -50 && vertikalNachstellen == 1)
    {
      digitalWrite(DIR_V, LOW);
      makeStepVertical();
    }else{
      vertikalNachstellen = 0;
    }
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(STEP_H, OUTPUT);
  pinMode(DIR_H, OUTPUT);

  pinMode(STEP_V, OUTPUT);
  pinMode(DIR_V, OUTPUT);

  // NEU: Endschalter als Input mit internem Pull-up Widerstand definieren
  pinMode(ENDSCHALTER_PIN, INPUT_PULLUP);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  delay(3000);

  if(!mag.begin())
  {
    Serial.println("Ooops, no HMC5883 detected ... Check your wiring!");
    while(1);
  }
  
  // NEU: Homing-Fahrt ausführen bevor der Tracker startet
  fahreBisEndschalter();
  delay(1000); // Kurz warten nach dem Nullpunkt anfahren

  Serial.println("\nSTART SOLAR TRACKER");
  Serial.println("WLAN bleibt während des Scans aus.");
  
  std::pair<float, float> startPosition = findBestPosition();
  finalPositionH = startPosition.first;
  finalPositionV = startPosition.second;
}


void loop()
{
  // =====================================================
  // ERGEBNIS
  // =====================================================
  Serial.println("\n================================");
  Serial.println("BESTE POSITION GEFUNDEN");

  // Fehlerbehebung: currentPositionH existierte nicht, nutzt jetzt finalPositionH
  Serial.print("Horizontaler Winkel: ");
  Serial.println(finalPositionH);

  Serial.print("Vertikaler Winkel: ");
  Serial.println(finalPositionV);

  Serial.println("================================");

  // =====================================================
  // WLAN ERST NACH DEM SCAN EINSCHALTEN
  // =====================================================
  connectWiFi();

  // =====================================================
  // THINGSPEAK SENDEN
  // =====================================================
  sendToThingSpeak(finalPositionH, finalPositionV);

  Serial.println("Fertig.");

  while(1);
}