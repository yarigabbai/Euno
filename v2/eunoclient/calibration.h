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

// ──────────────────────────────────────────────────────────────────────
// Utility
static inline float wrap360(float a){ a = fmodf(a,360.0f); if(a<0)a+=360.0f; return a; }
// differenza angolare a→b in [-180,+180]
static inline float angdiff(float a, float b){ return fmodf((a-b+540.0f),360.0f)-180.0f; }

// ──────────────────────────────────────────────────────────────────────
// Reset min/max per hard-iron (identico)
void resetCalibrationData() {
  minX =  32767.0f; minY =  32767.0f; minZ =  32767.0f;
  maxX = -32768.0f; maxY = -32768.0f; maxZ = -32768.0f;
  Serial.println("DEBUG: Calibration data reset");
}

// ──────────────────────────────────────────────────────────────────────
/*
  Calibrazione hard-iron: raccoglie min/max XYZ per ~10 s.
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

    // Salva come int16_t in RAM + EEPROM
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
  getCorrectedHeading() — COMPASS **senza tilt compensation**
  - Usa solo piano XY: heading = atan2( (My - offY), (Mx - offX) ).
  - NIENTE pitch/roll, NIENTE rotazioni del vettore magnetico.
  - Media circolare delle letture raccolte in finestra di 500 ms.
  - Pubblica un valore **ogni 500 ms**; tra un publish e l’altro restituisce l’ultimo.
  - Z viene usato SOLO per scartare outlier evidenti (non per ruotare il vettore).
*/
int getCorrectedHeading() {
  // Stato per media a finestra 500 ms
  static float sumCos = 0.0f;
  static float sumSin = 0.0f;
  static uint16_t sampleCount = 0;
  static unsigned long lastPublish = 0;
  static int lastOutputDeg = 0;
  static bool initialized = false;

  // 1) leggi magnetometro
  compass.read();
  float mx = compass.getX() - (float)compassOffsetX;
  float my = compass.getY() - (float)compassOffsetY;
  float mz = compass.getZ() - (float)compassOffsetZ;

  // 2) heading su piano XY (NO tilt-comp)
  float headingDeg = atan2f(my, mx) * 180.0f / (float)M_PI;
  if (headingDeg < 0.0f) headingDeg += 360.0f;

  // 3) piccolo “gate” su Z: se Z è anomalo rispetto a XY (impulsi/EMI), rigetta il campione
  // soglia semplice: se |mz| > 3 * media(|mx|,|my|) e |mx|+|my| è piccolo, scarta (protezione dolce)
  float absmx = fabsf(mx), absmy = fabsf(my), absmz = fabsf(mz);
  float xyMean = 0.5f * (absmx + absmy);
  bool reject = (xyMean < 1.0f && absmz > 3.0f * fmaxf(1.0f, xyMean)); // numeri in µT: 1.0 è safe floor
  if (!reject) {
    // 4) accumula per media circolare
    float rad = headingDeg * (float)M_PI / 180.0f;
    sumCos += cosf(rad);
    sumSin += sinf(rad);
    sampleCount++;
  }

  // 5) publish ogni 500 ms (2 Hz)
  unsigned long now = millis();

  if (!initialized) {
    // primo valore: pubblica subito
    float avgRad = atan2f(sumSin, sumCos);
    float avgDeg = avgRad * 180.0f / (float)M_PI;
    if (avgDeg < 0.0f) avgDeg += 360.0f;
    // offset software (C‑GPS)
    avgDeg = fmodf(avgDeg + (float)headingOffset, 360.0f);
    if (avgDeg < 0.0f) avgDeg += 360.0f;

    lastOutputDeg = (int)lroundf(avgDeg);
    lastPublish = now;
    initialized = true;

    // reset finestra
    sumCos = 0.0f; sumSin = 0.0f; sampleCount = 0;
    return lastOutputDeg;
  }

  if (now - lastPublish >= 500UL) {
    if (sampleCount > 0) {
      float avgRad = atan2f(sumSin, sumCos);
      float avgDeg = avgRad * 180.0f / (float)M_PI;
      if (avgDeg < 0.0f) avgDeg += 360.0f;

      // offset software (C‑GPS)
      avgDeg = fmodf(avgDeg + (float)headingOffset, 360.0f);
      if (avgDeg < 0.0f) avgDeg += 360.0f;

      lastOutputDeg = (int)lroundf(avgDeg);
    }
    // reset finestra 500 ms
    sumCos = 0.0f; sumSin = 0.0f; sampleCount = 0;
    lastPublish = now;
  }

  // ritorna sempre l’ultimo valore pubblicato (stabile)
  return lastOutputDeg;
}

#endif // CALIBRATION_H
