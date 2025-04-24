// ===================
// File: calibration.h
// ===================

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <QMC5883LCompass.h>
#include <EEPROM.h>
#include <math.h>

// Variabili globali (definite nel main)
extern bool calibrationMode;
extern unsigned long calibrationStartTime;
extern float minX, minY, minZ;
extern float maxX, maxY, maxZ;
extern int compassOffsetX, compassOffsetY, compassOffsetZ;
extern QMC5883LCompass compass;

// Dichiarazioni delle funzioni
void resetCalibrationData();
void performCalibration(unsigned long currentMillis);
int getCorrectedHeading();

// Resetta i valori per la calibrazione
void resetCalibrationData() {
    minX = 32767;
    minY = 32767;
    minZ = 32767;
    maxX = -32768;
    maxY = -32768;
    maxZ = -32768;
    Serial.println("DEBUG: Calibration data reset");
}

// Esegue la calibrazione per 10 secondi; alla fine calcola e salva gli offset in EEPROM.
void performCalibration(unsigned long currentMillis) {
    if (currentMillis - calibrationStartTime < 10000) {
        compass.read();
        float x = compass.getX();
        float y = compass.getY();
        float z = compass.getZ();

        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
        if (z < minZ) minZ = z;
        if (z > maxZ) maxZ = z;

        Serial.print("DEBUG: Calibration in progress... X: ");
        Serial.print(x);
        Serial.print(" Y: ");
        Serial.print(y);
        Serial.print(" Z: ");
        Serial.println(z);
    } else {
        calibrationMode = false;
        
        float offsetX = (maxX + minX) / 2.0;
        float offsetY = (maxY + minY) / 2.0;
        float offsetZ = (maxZ + minZ) / 2.0;

        compassOffsetX = (int)offsetX;
        compassOffsetY = (int)offsetY;
        compassOffsetZ = (int)offsetZ;

        EEPROM.write(0, compassOffsetX & 0xFF);
        EEPROM.write(1, (compassOffsetX >> 8) & 0xFF);
        EEPROM.write(2, compassOffsetY & 0xFF);
        EEPROM.write(3, (compassOffsetY >> 8) & 0xFF);
        EEPROM.write(4, compassOffsetZ & 0xFF);
        EEPROM.write(5, (compassOffsetZ >> 8) & 0xFF);
        EEPROM.commit();

        Serial.println("DEBUG: Calibration complete and offset saved.");
        Serial.print("DEBUG: Offset X: ");
        Serial.println(compassOffsetX);
        Serial.print("DEBUG: Offset Y: ");
        Serial.println(compassOffsetY);
        Serial.print("DEBUG: Offset Z: ");
        Serial.println(compassOffsetZ);
    }
}

// Calcola l'heading corretto applicando gli offset
int getCorrectedHeading() {
    compass.read();
    float correctedX = compass.getX() - compassOffsetX;
    float correctedY = compass.getY() - compassOffsetY;
    float heading = atan2(correctedY, correctedX) * 180.0 / M_PI;
    if (heading < 0) {
        heading += 360;
    }
    int roundedHeading = (int)round(heading);
    Serial.printf("DEBUG: Calculated corrected heading: %d\n", roundedHeading);
    return roundedHeading;
}

#endif

