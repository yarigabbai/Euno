// ===================
// File: calibration.h
// ===================

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <QMC5883LCompass.h>
#include <EEPROM.h>
#include <math.h>
unsigned long lastCalibrationLogTime = 0;
// Variabili globali (definite nel main)
extern bool calibrationMode;
extern unsigned long calibrationStartTime;
extern float minX, minY, minZ;
extern float maxX, maxY, maxZ;
extern int compassOffsetX, compassOffsetY, compassOffsetZ;
extern QMC5883LCompass compass;
extern bool motorControllerState; 
extern int headingOffset;
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
    if (motorControllerState) {  // Se il motore è acceso, non procedere con la calibrazione
        debugLog("Motore attivo, calibrazione sospesa.");
        return;
    }

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

        // Stampa ogni 500ms
        if (currentMillis - lastCalibrationLogTime > 500) {
            lastCalibrationLogTime = currentMillis;
            Serial.printf("DEBUG: Calibration in progress... X: %.2f Y: %.2f Z: %.2f\n", x, y, z);
        }

    } else {
        // Termina calibrazione
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

        debugLog("DEBUG: Calibration complete and offset saved.");
    }
}


int getCorrectedHeading() {
    compass.read();
    float correctedX = compass.getX() - compassOffsetX;
    float correctedY = compass.getY() - compassOffsetY;
    float heading = atan2(correctedY, correctedX) * 180.0 / M_PI;
    if (heading < 0) heading += 360;
    // Applica l'offset software salvato (C-GPS)
    heading = fmod(heading + headingOffset, 360.0);
    return (int)round(heading);
}
#endif

