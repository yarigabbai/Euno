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
#include <stdint.h>

// ── LOG stato calibrazione
unsigned long lastCalibrationLogTime = 0;

// ── Variabili globali (dichiarate nello sketch .ino)
extern bool calibrationMode;
extern unsigned long calibrationStartTime;
extern float minX, minY, minZ;
extern float maxX, maxY, maxZ;
extern int16_t compassOffsetX, compassOffsetY, compassOffsetZ;  // EEPROM @0..5
extern ICMCompass compass;
extern bool motorControllerState;
extern int headingOffset;  // EEPROM @6..7 (offset software C-GPS)

// ── Prototipi
void resetCalibrationData();
void performCalibration(unsigned long currentMillis);
int  getCorrectedHeading();

// ──────────────────────────────────────────────────────────────────────
// Reset min/max per hard-iron (resta identico)
// ──────────────────────────────────────────────────────────────────────
void resetCalibrationData() {
  minX =  32767.0f;
  minY =  32767.0f;
  minZ =  32767.0f;
  maxX = -32768.0f;
  maxY = -32768.0f;
  maxZ = -32768.0f;
  Serial.println("DEBUG: Calibration data reset");
}

// ──────────────────────────────────────────────────────────────────────
/*
  Calibrazione hard-iron: raccoglie min/max XYZ per ~10 s.
  Alla fine salva offset X/Y/Z in EEPROM (int16_t) @0..5.
*/
// ──────────────────────────────────────────────────────────────────────
void performCalibration(unsigned long currentMillis) {
  if (motorControllerState) {
    debugLog("Motore attivo, calibrazione sospesa.");
    return;
  }

  if (currentMillis - calibrationStartTime < 10000UL) {
    compass.read();
    float x = compass.getX();
    float y = compass.getY();
    float z = compass.getZ();

    if (x < minX) minX = x; if (x > maxX) maxX = x;
    if (y < minY) minY = y; if (y > maxY) maxY = y;
    if (z < minZ) minZ = z; if (z > maxZ) maxZ = z;

    if (currentMillis - lastCalibrationLogTime > 500UL) {
      lastCalibrationLogTime = currentMillis;
      Serial.printf("DEBUG: Calibration in progress... X: %.2f Y: %.2f Z: %.2f\n", x, y, z);
    }

  } else {
    calibrationMode = false;

    // Hard-iron offsets
    float offXf = 0.5f * (maxX + minX);
    float offYf = 0.5f * (maxY + minY);
    float offZf = 0.5f * (maxZ + minZ);

    // Salva come int16_t in RAM + EEPROM
    compassOffsetX = (int16_t)lroundf(offXf);
    compassOffsetY = (int16_t)lroundf(offYf);
    compassOffsetZ = (int16_t)lroundf(offZf);

    EEPROM.put<int16_t>(0, compassOffsetX);
    EEPROM.put<int16_t>(2, compassOffsetY);
    EEPROM.put<int16_t>(4, compassOffsetZ);
    EEPROM.commit();
    delay(100);

    debugLog("DEBUG: Calibration complete and offsets saved.");
  }
}

// ──────────────────────────────────────────────────────────────────────
/*
  getCorrectedHeading()
  - Usa offset hard-iron X/Y/Z.
  - Se l’accelerometro è disponibile, effettua una *tilt compensation leggera*
    (roll/pitch da accel, poi ruota il vettore mag in orizzontale).
  - Se l’accel non è disponibile, fallback al piano XY classico.
  - Applica headingOffset (C-GPS) e normalizza 0..359.
*/
// ──────────────────────────────────────────────────────────────────────
int getCorrectedHeading() {
  // 1) leggi ultimo magnetometro e applica offset hard-iron
  compass.read();
  float mx = compass.getX() - (float)compassOffsetX;
  float my = compass.getY() - (float)compassOffsetY;
  float mz = compass.getZ() - (float)compassOffsetZ;

  // 2) prova a leggere accelerometro (per roll/pitch)
  sensors_event_t acc;
  bool haveAcc = compass.getAccelEvent(acc); // usa icm_compass.h
  float headingDeg;

  if (haveAcc && !isnan(acc.acceleration.x) && !isnan(acc.acceleration.y) && !isnan(acc.acceleration.z)) {
    // ── pitch / roll dal solo accelerometro
    // Nota: stesse formule che usi già nella fusion (consistenti col resto).
    float ax = acc.acceleration.x;
    float ay = acc.acceleration.y;
    float az = acc.acceleration.z;

    // Evita divisioni strane quando il modulo è "in caduta libera" (norma ≈ 0)
    float gnorm = sqrtf(ax*ax + ay*ay + az*az);
    if (gnorm < 1e-3f) {
      // Fallback piano XY
      headingDeg = atan2f(my, mx) * 180.0f / (float)M_PI;
    } else {
      // Pitch / Roll (rad)
      float pitch = atanf(-ax / sqrtf(ay*ay + az*az));
      float roll  = atanf( ay / az );

      // ── ruota il vettore magnetico per compensare inclinazione
      // Formule standard:
      // Xh = mx*cos(pitch) + mz*sin(pitch)
      // Yh = mx*sin(roll)*sin(pitch) + my*cos(roll) - mz*sin(roll)*cos(pitch)
      float cp = cosf(pitch), sp = sinf(pitch);
      float cr = cosf(roll),  sr = sinf(roll);

      float Xh = mx * cp + mz * sp;
      float Yh = mx * sr * sp + my * cr - mz * sr * cp;

      // Manteniamo la stessa convenzione segno del calcolo “piano” (atan2(Y, X))
      headingDeg = atan2f(Yh, Xh) * 180.0f / (float)M_PI;
    }
  } else {
    // ── fallback se accel non disponibile
    headingDeg = atan2f(my, mx) * 180.0f / (float)M_PI;
  }

  if (headingDeg < 0.0f) headingDeg += 360.0f;

  // 3) Offset software C-GPS e normalizzazione 0..359
  headingDeg = fmodf(headingDeg + (float)headingOffset, 360.0f);
  if (headingDeg < 0.0f) headingDeg += 360.0f;

  return (int)lroundf(headingDeg);
}

#endif // CALIBRATION_H
