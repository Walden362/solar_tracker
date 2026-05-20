/*!
 * @file getCompassdata.ino
 * @brief Output the compass data
 * @copyright   Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author      [dexian.huang](952838602@qq.com)
 * @version  V1.0
 * @date  2017-7-3
 * @url https://github.com/DFRobot/DFRobot_QMC5883
 */
#include <Arduino.h>
#include <Wire.h>
#include <QMC5883LCompass.h>


QMC5883LCompass compass;

void setup() {
  Serial.begin(9600);

   Wire.begin(21, 22);

   Wire.beginTransmission(0x0D);
    if (Wire.endTransmission() == 0) {
        Serial.println("GY-271 found");
    } else {
        Serial.println("GY-271 not found");
    }
  // Kompass initialisieren
  compass.init();
  
  Serial.println("GY-271 / QMC5883L Kompass gestartet.");
}

void loop() {
  // Neue Werte vom Sensor abfragen
  compass.read();
  
  // X, Y, Z Achsenwerte auslesen
  int x = compass.getX();
  int y = compass.getY();
  int z = compass.getZ();
  
  // Berechneten Winkel holen (0 = Nord, 90 = Ost, 180 = Süd, 270 = West)
  int heading = compass.getAzimuth();
  
  // Ausgabe im Seriellen Monitor
  Serial.print("X: "); Serial.print(x);
  Serial.print(" | Y: "); Serial.print(y);
  Serial.print(" | Z: "); Serial.print(z);
  Serial.print(" -> Richtung: ");
  Serial.print(heading);
  Serial.println("°");
  
  delay(250); // Kurze Pause vor der nächsten Messung
}