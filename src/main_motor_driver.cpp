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
#define TIME_TO_SLEEP  60         // Schlafenszeit in Sekunden (5 Minuten = 300)

// =====================================================
// SOLARPANEL
// =====================================================
const int SOLAR_OUT = 12;


// =====================================================
// HMC5883 MAGNETOMETER
// =====================================================
Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);

// =====================================================
// WLAN / ThingSpeak
// =====================================================
const char* ssid = "iPhone 16 Pro von Max"; // Bitte anpassen
const char* password = "walden36";
String apiKey = "KP3PLZAHAREB1I9U";

// =====================================================
// HORIZONTALER MOTOR
// =====================================================
const int DIR_H  = 26;
const int STEP_H = 25;
const int EN_H   = 2; // Enable-Pin für horizontalen Motor
const int steps_horizontal = 800;

// =====================================================
// VERTIKALER MOTOR
// =====================================================
const int DIR_V  = 27;
const int STEP_V = 14;
const int EN_V   = 4; // Enable-Pin für vertikalen Motor
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
    delay(5); // Etwas schneller fahren, da wir nur auf den Endschalter warten
  }
  Serial.println("Endschalter erreicht! Horizontale Position genullt.");
}

/*void fahreBisEndschalter() {
  Serial.println("Fahre horizontal zum Endschalter (Nullpunkt)...");
  
  // Richtung festlegen (Gegen den Uhrzeigersinn / zum Schalter)
  digitalWrite(DIR_H, LOW); 
  
  // WICHTIG: Ein kurzes Delay, damit der Treiber die Richtung verarbeitet
  delay(10); 

  while(digitalRead(ENDSCHALTER_PIN) == LOW) {
    // Wir erzeugen den Schritt direkt hier, ohne zusätzliche delay()-Störer
    digitalWrite(STEP_H, HIGH);
    delayMicroseconds(600); // Puls-Dauer HIGH (Leicht erhöht für mehr Kraft)
    digitalWrite(STEP_H, LOW);
    delayMicroseconds(600); // Puls-Dauer LOW (Bestimmt die Pausenzeit)
    
    // Hinweis: 600 µs + 600 µs = 1.2ms pro Schritt. 
    // Das ist minimal langsamer als dein Scan, wodurch der Motor kraftvoller 
    // läuft und den Endschalter sanfter anfährt.
  }
  
  Serial.println("Endschalter erreicht! Horizontale Position genullt.");
}*/


void readBestrahlung(){
  int adc = analogRead(SOLAR_OUT);

  float Uadc = adc * 3.3 / 4095.0;

  float Usolar = Uadc * ((220000.0 + 100000.0) / 100000.0);

  // 47 Ohm parallel zu 320k Ohm
  float Rges = (47.0 * 320000.0) / (47.0 + 320000.0);

  float I = Usolar / Rges;

  float P = Usolar * I;

  float A = 0.1 * 0.08;

  // Bestrahlungsstärke E
  float E = P / (A * 0.155);

  Serial.print("Spannung: ");
  Serial.print(Usolar);
  Serial.print(" V");

  Serial.print(" | Strom: ");
  Serial.print(I * 1000);
  Serial.print(" mA");

  Serial.print(" | Leistung: ");
  Serial.print(P);
  Serial.print(" W");

  Serial.print(" | Bestrahlungsstaerke E: ");
  Serial.print(E);
  Serial.println(" W/m^2");

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
// KOMPASS AUSLESEN
// =====================================================
void readCompass(){
  sensors_event_t event; 
  mag.getEvent(&event);

  float heading = atan2(event.magnetic.y, event.magnetic.x);
  
  float declinationAngle = 3.00*M_PI/180; // Beispiel: 3 Grad Deklination, in Bogenmaß umgerechnet
  heading += declinationAngle;
  heading += 120.00 * M_PI / 180; // Korrektur für Nordwinkel (120 Grad im Uhrzeigersinn)
  
  if(heading < 0) heading += 2*PI;
  if(heading > 2*PI) heading -= 2*PI;
   
  float headingDegrees = heading * 180/M_PI; 
  
  Serial.print("Gemessener Kompasswinkel (0° = Norden): ");
  Serial.print(headingDegrees);
  Serial.println("°");
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


// void readOutDiodes() {
//   for(int i = 0; i < 10; i++){
//     int currentOL = readSensor(OL);
//     int currentOR = readSensor(OR);
//     int currentUL = readSensor(UL);
//     int currentUR = readSensor(UR);

//     Serial.print("Aktueller Wert Oben Linnks: "); Serial.println(currentOL);
//     Serial.print("Aktueller Wert Oben Rechts: "); Serial.println(currentOR);
//     Serial.print("Aktueller Wert Unten Links: "); Serial.println(currentUL);
//     Serial.print("Aktueller Wert Unten Rechts: "); Serial.println(currentUR);
//     delay(1000);
//   }
// }

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
// =====================================================
// SCAN FUNKTION (KORRIGIERT FÜR NORD-WINKEL)
// =====================================================
std::pair<float, float> findBestPosition() {
  
  // Motoren stromlos schalten, um magnetische Störungen zu verhindern
  Serial.println("Schalte Motoren für Kompassmessung stromlos...");
  digitalWrite(EN_H, HIGH);
  digitalWrite(EN_V, HIGH);
  delay(50); // Kurz warten, bis sich Magnetfelder abgebaut haben

  // Jetzt Kompass störungsfrei auslesen
  sensors_event_t event; 
  mag.getEvent(&event);

  float heading = atan2(event.magnetic.y, event.magnetic.x) + 3.00; // declinationAngle
  if(heading < 0) heading += 2*PI;
  if(heading > 2*PI) heading -= 2*PI;
  float headingDegrees = heading * 180 / M_PI;  

  Serial.print("Gemessener Kompasswinkel (0° = Norden): ");
  Serial.print(headingDegrees);
  Serial.println("°");

  // Motoren wieder aktivieren, da nun die Bewegung startet
  digitalWrite(EN_H, LOW);
  digitalWrite(EN_V, LOW);
  delay(10); // Treiber kurz stabilisieren lassen

  // --- HORIZONTALER SCAN ---
  bestStepH = 0; bestSumH = 0;
  digitalWrite(DIR_H, HIGH);
  Serial.println("\nHORIZONTAL SCAN");

  for(int i = 0; i < steps_horizontal; i++) {
    makeStepHorizontal();
    //Bremsrampe vor der Richtungsumkehr
    if (i >= 790) {
      delay(30); 
    } 
    else if (i >= 770) {
      delay(15); 
    } 
    else {
      delay(5); 
    }

    // Werte einmalig auslesen, um sie für Summe UND Speicherung zu nutzen
    int currentOL = readSensor(OL);
    int currentOR = readSensor(OR);
    int currentUL = readSensor(UL);
    int currentUR = readSensor(UR);


    int sum = currentOL + currentOR + currentUL + currentUR;
    if(sum > bestSumH) {
      bestSumH = sum; 
      bestStepH = i;
      // Hier werden die exakt besten Werte für ThingSpeak zwischengespeichert
      bestOL = currentOL; 
      bestOR = currentOR;
      bestUL = currentUL; 
      bestUR = currentUR;
    }
  }

  delay(300); //Pause vor der Rückfahrt, damit der Motor kurz zur Ruhe kommt
  // Zurück zur besten horizontalen Position fahren
  int stepsBackH = steps_horizontal - bestStepH;
  digitalWrite(DIR_H, LOW);
  for(int i = 0; i < stepsBackH; i++) {
    makeStepHorizontal();
    delay(5);
  }

  // Hier berechnen wir den finalen horizontalen Winkel in Bezug auf Norden
  float bestAngleHorizontalToNorth = headingDegrees + (bestStepH * 0.45);
  // Falls der Winkel über 360 Grad springt, korrigieren
  if(bestAngleHorizontalToNorth >= 360.0) {
    bestAngleHorizontalToNorth -= 360.0;
  }

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
      bestSumV = sum; 
      bestStepV = i;
      // (Optional) Wenn die vertikalen Sensorwerte wichtiger sind für ThingSpeak,
      // könntest du bestOL etc. auch hier überschreiben lassen.
    }
  }

  // Zurück zur besten vertikalen Position fahren
  int stepsBackV = steps_vertical - bestStepV;
  digitalWrite(DIR_V, HIGH);
  for(int i = 0; i < stepsBackV; i++) {
    makeStepVertical();
    delay(5);
  }

  float bestAngleV = bestStepV * 0.45;
  Serial.print("Bestes horizontales Winkel zum Norden: "); Serial.println(bestAngleHorizontalToNorth);
  Serial.print("Bestes vertikales Winkel: "); Serial.println(bestAngleV);
  // HIER WAR DER FEHLER: Wir geben jetzt explizit den Nord-Winkel zurück!
  return {bestAngleHorizontalToNorth, bestAngleV};
}

void setup() {
  Serial.begin(9600);

  gpio_hold_dis((gpio_num_t)EN_H);
  gpio_hold_dis((gpio_num_t)EN_V);
  // Pins initialisieren
  pinMode(STEP_H, OUTPUT); pinMode(DIR_H, OUTPUT);
  pinMode(STEP_V, OUTPUT); pinMode(DIR_V, OUTPUT);
  pinMode(EN_H, OUTPUT);   pinMode(EN_V, OUTPUT);
  pinMode(ENDSCHALTER_PIN, INPUT);

  // Motortreiber initial aktivieren
  // Serial.println("Aktiviere Motortreiber...");
  // digitalWrite(EN_H, LOW);
  // digitalWrite(EN_V, LOW);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  delay(1000); 

  // Sensor Check
  if(!mag.begin()) {
    Serial.println("Ooops, no HMC5883 detected ... Check your wiring!");
    digitalWrite(EN_H, HIGH);
    digitalWrite(EN_V, HIGH);
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }
  
  Serial.println("\n=== SOLAR TRACKER AUFGEWACHT ===");
  // Testen des Kompasses
  //readOutDiodes();
  // 1. WLAN während der ADC Messung ausschalten
  WiFi.mode(WIFI_OFF);

  // 2. Horizontalen Motor auf Nullpunkt fahren
  fahreBisEndschalter();
  delay(1000);

  // 3. Scan durchführen (Motoren schalten sich intern kurz für den Kompass aus)
  std::pair<float, float> currentPosition = findBestPosition();
  finalPositionH = currentPosition.first;
  finalPositionV = currentPosition.second;

  Serial.print("Bester Horizontaler Winkel: "); Serial.println(finalPositionH);
  Serial.print("Bester Vertikaler Winkel: "); Serial.println(finalPositionV);

  readBestrahlung();

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

  // 6. Treiber komplett stromlos schalten vor dem Sleep
  Serial.println("Deaktiviere Motortreiber...");
  digitalWrite(EN_H, HIGH);
  digitalWrite(EN_V, HIGH);
  gpio_hold_en((gpio_num_t)EN_H);
  gpio_hold_en((gpio_num_t)EN_V);
  gpio_deep_sleep_hold_en();

  // 7. Ab in den Deep Sleep
  Serial.println("Alle Aufgaben erledigt. Gehe für 1 Minute schlafen (Deep Sleep)...");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  
  Serial.flush(); 
  esp_deep_sleep_start();
}

void loop() {
  // Bleibt leer
}