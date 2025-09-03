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

/* ─────────────────────────────────────────────────────────────────────
   CONFIG RUNTIME (modifica qui se necessario)
   ─────────────────────────────────────────────────────────────────────
   - COMPASS_USE_TILT:     1 = usa tilt compensation (consigliato a bordo)
                           0 = disattiva tilt (debug o prove a banco)
   - COMPASS_USE_SOFTIRON: 1 = applica correzione di scala da min/max
                           0 = solo offset hard-iron
   - COMPASS_INVERT_CW:    1 = se ruotando in senso orario l’heading diminuisce,
                               forza inversione (heading = 360 - heading)
                           0 = normale
   - COMPASS_PUB_MS:       finestra media in ms (telemetria ogni X ms)
   - COMPASS_MIN_XY_uT:    soglia minima campo XY per accettare il campione
   - COMPASS_Z_REJ_RATIO:  rapporto Z/XY oltre cui scartare il campione
*/
#define COMPASS_USE_TILT      1
#define COMPASS_USE_SOFTIRON  1
#define COMPASS_INVERT_CW     0

#define COMPASS_PUB_MS        500UL
#define COMPASS_MIN_XY_uT     0.8f
#define COMPASS_Z_REJ_RATIO   3.0f

// Se vuoi “smorzare” ulteriormente i cambi, alza questo fattore (0..1)
#define COMPASS_SMOOTH_ALPHA  0.0f   // 0 = nessun low-pass extra sull’uscita

/* ─────────────────────────────────────────────────────────────────────
   VARIABILI GLOBALI (dichiarate nello sketch .ino)
   ───────────────────────────────────────────────────────────────────── */
extern bool calibrationMode;
extern unsigned long calibrationStartTime;
extern float minX, minY, minZ;
extern float maxX, maxY, maxZ;
extern int16_t compassOffsetX, compassOffsetY, compassOffsetZ;  // EEPROM @0..5
extern ICMCompass compass;
extern bool motorControllerState;
extern int headingOffset;  // EEPROM @6..7 (offset software C-GPS)

/* ─────────────────────────────────────────────────────────────────────
   PROTOTIPI
   ───────────────────────────────────────────────────────────────────── */
void resetCalibrationData();
void performCalibration(unsigned long currentMillis);
int  getCorrectedHeading();

inline void getTiltAngles(float &pitch, float &roll);
inline float compensateTilt(float mx, float my, float mz, float pitch, float roll);

/* ─────────────────────────────────────────────────────────────────────
   UTILITY
   ───────────────────────────────────────────────────────────────────── */
static inline float wrap360(float a){ a = fmodf(a,360.0f); if(a<0)a+=360.0f; return a; }
static inline float angdiff(float a, float b){ return fmodf((a-b+540.0f),360.0f)-180.0f; }

/* ─────────────────────────────────────────────────────────────────────
   CALIBRAZIONE: reset min/max per hard-iron
   ───────────────────────────────────────────────────────────────────── */
unsigned long lastCalibrationLogTime = 0;

void resetCalibrationData() {
  minX =  32767.0f; minY =  32767.0f; minZ =  32767.0f;
  maxX = -32768.0f; maxY = -32768.0f; maxZ = -32768.0f;
  Serial.println("DEBUG: Calibration data reset");
}

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

/* ─────────────────────────────────────────────────────────────────────
   CORREZIONI “SOFT-IRON” da min/max (scale per assi)
   - opzionale: normalizza le scale X/Y/Z per rendere l’ellissoide ≈ cerchio
   ───────────────────────────────────────────────────────────────────── */
static inline void applySoftIron(float &mx, float &my, float &mz){
#if COMPASS_USE_SOFTIRON
  float rX = (maxX - minX); if (rX < 1e-3f) rX = 1e-3f;
  float rY = (maxY - minY); if (rY < 1e-3f) rY = 1e-3f;
  float rZ = (maxZ - minZ); if (rZ < 1e-3f) rZ = 1e-3f;
  // scala per eguagliare le ampiezze: media delle tre
  float rAvg = (rX + rY + rZ) / 3.0f;
  mx *= (rAvg / rX);
  my *= (rAvg / rY);
  mz *= (rAvg / rZ);
#else
  (void)mx; (void)my; (void)mz;
#endif
}

/* ─────────────────────────────────────────────────────────────────────
   HEADING COMPASS CORRETTO
   - Legge ICM, applica hard/soft iron, compensazione tilt (se abilitata),
     converte in convenzione bussola (0°=Nord, 90°=Est, CW),
     media vettoriale su finestra COMPASS_PUB_MS
   ───────────────────────────────────────────────────────────────────── */
int getCorrectedHeading() {
  static float sumCos = 0.0f;
  static float sumSin = 0.0f;
  static uint16_t sampleCount = 0;

  static unsigned long lastPublish = 0;
  static int lastOutputDeg = 0;
  static bool initialized = false;

  // 1) Lettura sensore + hard-iron
  compass.read();
  float mx = compass.getX() - (float)compassOffsetX;
  float my = compass.getY() - (float)compassOffsetY;
  float mz = compass.getZ() - (float)compassOffsetZ;

  // 2) Soft-iron (opzionale)
  applySoftIron(mx, my, mz);

  // 3) Tilt compensation (opzionale)
  float headingRadMath; // heading matematico: 0=asse X (Est), CCW+
#if COMPASS_USE_TILT
  float pitch, roll;
  getTiltAngles(pitch, roll);
  // proietta nel piano orizzontale
  float xh = mx * cosf(pitch) + mz * sinf(pitch);
  float yh = mx * sinf(roll) * sinf(pitch) + my * cosf(roll) - mz * cosf(pitch) * sinf(roll);
  headingRadMath = atan2f(yh, xh);
#else
  headingRadMath = atan2f(my, mx);
#endif

  // 4) Converti in convenzione bussola (0=N, 90=E, orario)
  float headingDegMath = headingRadMath * 180.0f / (float)M_PI; // 0=Est, CCW+
  if (headingDegMath < 0.0f) headingDegMath += 360.0f;

  // Compass: 0=N (90° a Est), CW+  ->  headingCW = (90 - headingMath) mod 360
  float headingCW = wrap360(90.0f - headingDegMath);

#if COMPASS_INVERT_CW
  // Se ruotando in senso orario l’heading diminuisce, forza inversione
  headingCW = wrap360(360.0f - headingCW);
#endif

  // Applica offset software (es. C-GPS) alla FINE
  headingCW = wrap360(headingCW + (float)headingOffset);

  // 5) REJECTION: scarta campioni poco affidabili (XY troppo piccolo o Z troppo grande)
  float absmx = fabsf(mx), absmy = fabsf(my), absmz = fabsf(mz);
  float xyMean = 0.5f * (absmx + absmy);
  bool reject = (xyMean < COMPASS_MIN_XY_uT && absmz > COMPASS_Z_REJ_RATIO * fmaxf(COMPASS_MIN_XY_uT, xyMean));

  if (!reject) {
    float hRad = headingCW * (float)M_PI / 180.0f;
    sumCos += cosf(hRad);
    sumSin += sinf(hRad);
    sampleCount++;
  }

  unsigned long now = millis();

  // 6) Primo output: evita valori casuali
  if (!initialized) {
    if (sampleCount >= 3) { // attendo almeno 3 campioni
      float avgRad = atan2f(sumSin, sumCos);
      float avgDeg = wrap360(avgRad * 180.0f / (float)M_PI);

      // low-pass opzionale
      if (COMPASS_SMOOTH_ALPHA > 0.0f) {
        lastOutputDeg = (int)lroundf(wrap360(COMPASS_SMOOTH_ALPHA * avgDeg +
                                             (1.0f - COMPASS_SMOOTH_ALPHA) * (float)lastOutputDeg));
      } else {
        lastOutputDeg = (int)lroundf(avgDeg);
      }

      lastPublish = now;
      initialized = true;
      sumCos = 0.0f; sumSin = 0.0f; sampleCount = 0;
      return lastOutputDeg;
    } else {
      // Finché non ho abbastanza campioni, restituisco l’istante (meno stabile)
      return (int)lroundf(headingCW);
    }
  }

  // 7) Pubblica ogni COMPASS_PUB_MS con media vettoriale
  if (now - lastPublish >= COMPASS_PUB_MS) {
    if (sampleCount > 0) {
      float avgRad = atan2f(sumSin, sumCos);
      float avgDeg = wrap360(avgRad * 180.0f / (float)M_PI);

      if (COMPASS_SMOOTH_ALPHA > 0.0f) {
        lastOutputDeg = (int)lroundf(wrap360(COMPASS_SMOOTH_ALPHA * avgDeg +
                                             (1.0f - COMPASS_SMOOTH_ALPHA) * (float)lastOutputDeg));
      } else {
        lastOutputDeg = (int)lroundf(avgDeg);
      }
    } else {
      // Nessun campione valido → fai pass-through dell’ultimo istante letto
      lastOutputDeg = (int)lroundf(headingCW);
    }
    sumCos = 0.0f; sumSin = 0.0f; sampleCount = 0;
    lastPublish = now;
  }

  return lastOutputDeg;
}

/* ─────────────────────────────────────────────────────────────────────
   Tilt helpers
   ───────────────────────────────────────────────────────────────────── */
inline void getTiltAngles(float &pitch, float &roll) {
  sensors_event_t acc;
  if (compass.getAccelEvent(acc)) {
    // NB: formula in convenzione classica per ICM20948/Adafruit
    pitch = atan2f(-acc.acceleration.x,
                   sqrtf(acc.acceleration.y*acc.acceleration.y + acc.acceleration.z*acc.acceleration.z));
    roll  = atan2f(acc.acceleration.y, acc.acceleration.z);
  } else {
    pitch = 0.0f;
    roll  = 0.0f;
  }
}

// Mantengo per compatibilità, ma NON usata direttamente nel nuovo flusso
inline float compensateTilt(float mx, float my, float mz, float pitch, float roll) {
  float xh = mx * cosf(pitch) + mz * sinf(pitch);
  float yh = mx * sinf(roll) * sinf(pitch) + my * cosf(roll) - mz * cosf(pitch) * sinf(roll);
  return atan2f(yh, xh);
}

#endif // CALIBRATION_H
