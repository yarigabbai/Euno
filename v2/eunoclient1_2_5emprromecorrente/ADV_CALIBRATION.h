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



#ifndef ADV_CALIBRATION_H
#define ADV_CALIBRATION_H

#include <Arduino.h>
#include <math.h>
#include <QMC5883LCompass.h>

#define ADV_SECTORS 36
#define SMOOTHING_FACTOR 0.2f

struct SectorCalibration {
    float compassX;
    float compassY;
    float compassZ;
    float gyroAngle;
    bool calibrated;
};

static SectorCalibration advCalData[ADV_SECTORS];
static int advCalibratedCount = 0;
static bool advCalibrationMode = false;

// Helper functions
static inline int norm360(float deg) {
    int v = (int)round(deg);
    v %= 360;
    if (v < 0) v += 360;
    return v;
}

static inline float circDiff(float a, float b) {
    float d = fmodf((a - b + 540.0f), 360.0f) - 180.0f;
    return d;
}

static inline int sectorIndex(int degrees) {
    return (degrees % 360) / 10;
}

// Start calibration
static inline void startAdvancedCalibration() {
    advCalibratedCount = 0;
    advCalibrationMode = true;
    for (int i = 0; i < ADV_SECTORS; i++) {
        advCalData[i].calibrated = false;
        advCalData[i].compassX = 0;
        advCalData[i].compassY = 0;
        advCalData[i].compassZ = 0;
        advCalData[i].gyroAngle = i * 10.0f;
    }
    Serial.println("[CAL] Advanced calibration started");
}

// Nuova versione con 4 parametri
static inline void updateAdvancedCalibration(float gyroDeg, float compassX, float compassY, float compassZ) {
    if (!advCalibrationMode) return;

    gyroDeg = norm360(gyroDeg);
    int idx = sectorIndex((int)gyroDeg);

    if (!advCalData[idx].calibrated) {
        advCalData[idx].compassX = compassX;
        advCalData[idx].compassY = compassY;
        advCalData[idx].compassZ = compassZ;
        advCalData[idx].calibrated = true;
        advCalibratedCount++;
    } else {
        advCalData[idx].compassX = SMOOTHING_FACTOR * compassX + 
                                 (1 - SMOOTHING_FACTOR) * advCalData[idx].compassX;
        advCalData[idx].compassY = SMOOTHING_FACTOR * compassY + 
                                 (1 - SMOOTHING_FACTOR) * advCalData[idx].compassY;
        advCalData[idx].compassZ = SMOOTHING_FACTOR * compassZ + 
                                 (1 - SMOOTHING_FACTOR) * advCalData[idx].compassZ;
    }

    if (advCalibratedCount >= ADV_SECTORS) {
        advCalibrationMode = false;
        Serial.println("[CAL] Advanced calibration complete");
    }
}

// Versione legacy compatibile (2 parametri)
static inline void updateAdvancedCalibration(float gyroDeg, float compassDeg) {
    float compassX = cos(compassDeg * M_PI / 180.0f);
    float compassY = sin(compassDeg * M_PI / 180.0f);
    updateAdvancedCalibration(gyroDeg, compassX, compassY, 0);
}

// Resto del codice rimane invariato...
static inline int getAdvancedHeading(float compassX, float compassY, float compassZ) {
    // ... implementazione esistente ...
}

static inline int getAdvancedHeading(float compassDegRaw) {
    float compassX = cos(compassDegRaw * M_PI / 180.0f);
    float compassY = sin(compassDegRaw * M_PI / 180.0f);
    return getAdvancedHeading(compassX, compassY, 0);
}

static inline bool isAdvancedCalibrationMode() { return advCalibrationMode; }
static inline bool isAdvancedCalibrationComplete() { return advCalibratedCount >= ADV_SECTORS; }

#endif // ADV_CALIBRATION_H