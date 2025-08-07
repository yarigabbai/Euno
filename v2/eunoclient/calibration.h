/*
  EUNO Autopilot – © 2025 Yari Gabbai

  Licensed under CC BY-NC 4.0:
  Creative Commons Attribution-NonCommercial 4.0 International

  You are free to use, modify, and share this code
  for NON-COMMERCIAL purposes only.

  You must credit the original author:
  Yari Gabbai / EUNO Autopilot

  Full license text:
  https://creativecommons.org/licenses/by-nc/4.0/legalcode
*/


// ===================
// File: calibration.h
// ===================

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "icm_compass.h"
#include <EEPROM.h>
#include <math.h>
unsigned long lastCalibrationLogTime = 0;
// Variabili globali (definite nel main)
extern bool calibrationMode;
extern unsigned long calibrationStartTime;
extern float minX, minY, minZ;
extern float maxX, maxY, maxZ;
#include <stdint.h>
extern int16_t compassOffsetX, compassOffsetY, compassOffsetZ;
extern ICMCompass compass;
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

     // alla fine di performCalibration(), dopo aver calcolato compassOffsetX/Y/Z:
EEPROM.put<int16_t>(0, compassOffsetX);
EEPROM.put<int16_t>(2, compassOffsetY);
EEPROM.put<int16_t>(4, compassOffsetZ);
EEPROM.commit();
delay(100);                 // piccola pausa per sicurezza
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

