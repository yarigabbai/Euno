/*
  EUNO Autopilot – © 2025 Yari Gabbai

  Licensed under CC BY-NC 4.0:
  Creative Commons Attribution-NonCommercial 4.0 International
*/

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

// Aggiungiamo i prototipi per tilt
inline void getTiltAngles(float &pitch, float &roll);
inline float compensateTilt(float mx, float my, float mz, float pitch, float roll);

// ──────────────────────────────────────────────────────────────────────
// Utility
static inline float wrap360(float a){ a = fmodf(a,360.0f); if(a<0)a+=360.0f; return a; }
static inline float angdiff(float a, float b){ return fmodf((a-b+540.0f),360.0f)-180.0f; }

// ──────────────────────────────────────────────────────────────────────
// Reset min/max per hard-iron
void resetCalibrationData() {
  minX =  32767.0f; minY =  32767.0f; minZ =  32767.0f;
  maxX = -32768.0f; maxY = -32768.0f; maxZ = -32768.0f;
  Serial.println("DEBUG: Calibration data reset");
}

// ──────────────────────────────────────────────────────────────────────
/*
  Calibrazione hard-iron: raccoglie min/max XYZ per ~20 s.
  Alla fine salva offset X/Y/Z in EEPROM (int16_t) @0..5.
*/
void performCalibration(unsigned long currentMillis) {
  if (motorControllerState) {
    Serial.println("Motore attivo, calibrazione sospesa.");
    return;
  }

  if (currentMillis - calibrationStartTime < 20000UL) {
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

    compassOffsetX = (int16_t)lroundf(offXf);
    compassOffsetY = (int16_t)lroundf(offYf);
    compassOffsetZ = (int16_t)lroundf(offZf);

    EEPROM.put<int16_t>(0, compassOffsetX);
    EEPROM.put<int16_t>(2, compassOffsetY);
    EEPROM.put<int16_t>(4, compassOffsetZ);
    EEPROM.commit();
    delay(100);

    Serial.println("DEBUG: Calibration complete and offsets saved.");
  }
}

// ──────────────────────────────────────────────────────────────────────
/*
  getCorrectedHeading() — COMPASS con tilt compensation
*/
int getCorrectedHeading() {
  static float sumCos = 0.0f;
  static float sumSin = 0.0f;
  static uint16_t sampleCount = 0;
  static unsigned long lastPublish = 0;
  static int lastOutputDeg = 0;
  static bool initialized = false;

  compass.read();
  float mx = compass.getX() - (float)compassOffsetX;
  float my = compass.getY() - (float)compassOffsetY;
  float mz = compass.getZ() - (float)compassOffsetZ;

  float pitch, roll;
  getTiltAngles(pitch, roll);

  float headingRad = compensateTilt(mx, my, mz, pitch, roll);
  float headingDeg = headingRad * 180.0f / (float)M_PI;
  if (headingDeg < 0.0f) headingDeg += 360.0f;

  float absmx = fabsf(mx), absmy = fabsf(my), absmz = fabsf(mz);
  float xyMean = 0.5f * (absmx + absmy);
  bool reject = (xyMean < 1.0f && absmz > 3.0f * fmaxf(1.0f, xyMean));

  if (!reject) {
    sumCos += cosf(headingRad);
    sumSin += sinf(headingRad);
    sampleCount++;
  }

  unsigned long now = millis();

  if (!initialized) {
    float avgRad = atan2f(sumSin, sumCos);
    float avgDeg = avgRad * 180.0f / (float)M_PI;
    if (avgDeg < 0.0f) avgDeg += 360.0f;
    avgDeg = fmodf(avgDeg + (float)headingOffset, 360.0f);
    if (avgDeg < 0.0f) avgDeg += 360.0f;

    lastOutputDeg = (int)lroundf(avgDeg);
    lastPublish = now;
    initialized = true;

    sumCos = 0.0f; sumSin = 0.0f; sampleCount = 0;
    return lastOutputDeg;
  }

  if (now - lastPublish >= 500UL) {
    if (sampleCount > 0) {
      float avgRad = atan2f(sumSin, sumCos);
      float avgDeg = avgRad * 180.0f / (float)M_PI;
      if (avgDeg < 0.0f) avgDeg += 360.0f;
      avgDeg = fmodf(avgDeg + (float)headingOffset, 360.0f);
      if (avgDeg < 0.0f) avgDeg += 360.0f;

      lastOutputDeg = (int)lroundf(avgDeg);
    }
    sumCos = 0.0f; sumSin = 0.0f; sampleCount = 0;
    lastPublish = now;
  }

  return lastOutputDeg;
}

// ──────────────────────────────────────────────────────────────────────
// Implementazioni funzioni tilt
// ──────────────────────────────────────────────────────────────────────
inline void getTiltAngles(float &pitch, float &roll) {
  sensors_event_t acc;
  if (compass.getAccelEvent(acc)) {
    pitch = atan2f(-acc.acceleration.x,
                   sqrtf(acc.acceleration.y*acc.acceleration.y + acc.acceleration.z*acc.acceleration.z));
    roll  = atan2f(acc.acceleration.y, acc.acceleration.z);
  } else {
    pitch = 0;
    roll  = 0;
  }
}

inline float compensateTilt(float mx, float my, float mz, float pitch, float roll) {
  float xh = mx * cosf(pitch) + mz * sinf(pitch);
  float yh = mx * sinf(roll) * sinf(pitch) + my * cosf(roll) - mz * sinf(roll) * cosf(pitch);
  return atan2f(yh, xh);
}

#endif // CALIBRATION_H
