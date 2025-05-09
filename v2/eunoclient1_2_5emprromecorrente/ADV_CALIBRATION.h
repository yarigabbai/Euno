#ifndef ADV_CALIBRATION_H
#define ADV_CALIBRATION_H

#include <Arduino.h>
#include <math.h>
#include <QMC5883LCompass.h>

#define ADV_SECTORS 36   // 360° / 10° sectors

struct SectorCalibration {
    float deviation;     // saved deviation (±180°)
    bool calibrated;     // true if sector is calibrated
};

static SectorCalibration advCalData[ADV_SECTORS];
static int advCalibratedCount = 0;
static bool advCalibrationMode = false;

// Helper: Normalize angle to 0-359°
static inline int norm360(float deg) {
    int v = (int)round(deg);
    v %= 360;
    if (v < 0) v += 360;
    return v;
}

// Helper: Circular difference (-180° to +180°)
static inline float circDiff(float a, float b) {
    float d = fmodf((a - b + 540.0f), 360.0f) - 180.0f;
    return d;
}

// Get sector index (0-35)
static inline int sectorIndex(int degrees) {
    return (degrees % 360) / 10;
}

// Start calibration procedure
static inline void startAdvancedCalibration() {
    advCalibratedCount = 0;
    advCalibrationMode = true;
    for (int i = 0; i < ADV_SECTORS; i++) {
        advCalData[i].calibrated = false;
        advCalData[i].deviation = 0.0f;
    }
    Serial.println("[CAL] Advanced calibration started");
}

// Update calibration with reference data (compatible with original .ino calls)
static inline void updateAdvancedCalibration(float gyroDeg, float compassDeg) {
    if (!advCalibrationMode) return;

    gyroDeg = norm360(gyroDeg);
    compassDeg = norm360(compassDeg);

    int idx = sectorIndex((int)gyroDeg);
    float dev = circDiff(compassDeg, gyroDeg);

    if (!advCalData[idx].calibrated) {
        advCalData[idx].deviation = dev;
        advCalData[idx].calibrated = true;
        advCalibratedCount++;
        
        Serial.printf("[CAL] Sector %02d: Gyro=%.1f° Comp=%.1f° Dev=%.2f° (%d/36)\n",
                     idx, gyroDeg, compassDeg, dev, advCalibratedCount);
    }

    if (advCalibratedCount >= ADV_SECTORS) {
        advCalibrationMode = false;
        Serial.println("[CAL] Advanced calibration complete");
    }
}

// Get calibrated heading (compatible with original .ino calls)
static inline int getAdvancedHeading(float compassDegRaw) {
    float compassDeg = norm360(compassDegRaw);
    int idx = sectorIndex((int)compassDeg);

    Serial.printf("[ADV] Raw=%.1f° Sector=%02d ", compassDeg, idx);

    if (!advCalData[idx].calibrated) {
        Serial.println("Uncalibrated → Using raw");
        return (int)compassDeg;
    }

    float corr = compassDeg - advCalData[idx].deviation;
    int out = norm360(corr);

    Serial.printf("Calibrated (Dev=%.2f° Corr=%.1f°) → %d°\n",
                 advCalData[idx].deviation, corr, out);
    
    return out;
}

// Compatibility functions
static inline bool isAdvancedCalibrationMode() { return advCalibrationMode; }
static inline bool isAdvancedCalibrationComplete() { return advCalibratedCount >= ADV_SECTORS; }

#endif // ADV_CALIBRATION_H