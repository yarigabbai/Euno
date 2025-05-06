#ifndef ADV_CALIBRATION_H
#define ADV_CALIBRATION_H

#include <Arduino.h>
#include <math.h>

//════════════════════════════════════════════════════════════
//  ADV‑CALIBRATION v2
//  ───────────────────────────────────────────────────────────
//  Obiettivo: misurare e compensare lo "shift" della bussola
//  in ogni settore di 10° prendendo il GYRO (o GPS) come vero
//  riferimento.
//
//  Durante la calibrazione salviamo direttamente la DEVIAZIONE
//  ◂ dev[idx] = compassRaw − gyroDeg   (±180°)
//  In uso normale applichiamo:         heading = compassRaw − dev[idx]
//════════════════════════════════════════════════════════════

#ifndef ADV_SECTORS
#define ADV_SECTORS 36   // 360 / 10
#endif

static float advDeviation[ADV_SECTORS];   // deviazione salvata (±180°)
static bool  advCalibrated[ADV_SECTORS];  // true se settore calibrato
static int   advCalibratedCount = 0;
static bool  advCalibrationMode = false;

// ──────────────────────────────────────────────────────────────
//  Helper: normalizza a 0‑359°
// ──────────────────────────────────────────────────────────────
static inline int norm360(float deg) {
    int v = (int)round(deg);
    v %= 360;
    if (v < 0) v += 360;
    return v;
}

// differenza circolare a‑b in range –180 … +180
static inline float circDiff(float a, float b) {
    float d = fmodf((a - b + 540.0f), 360.0f) - 180.0f;
    return d;  // può essere negativa
}

static inline int sectorIndex(int degrees) { return (degrees % 360) / 10; }

// ──────────────────────────────────────────────────────────────
//  1️⃣  Avvio calibrazione
// ──────────────────────────────────────────────────────────────
static inline void startAdvancedCalibration() {
    advCalibratedCount = 0;
    advCalibrationMode = true;
    for (int i = 0; i < ADV_SECTORS; i++) {
        advCalibrated[i] = false;
        advDeviation[i]  = 0.0f;
    }
    Serial.println("[CAL] iniziata: ruota la barca di 360°");
}

static inline bool isAdvancedCalibrationMode()      { return advCalibrationMode; }
static inline bool isAdvancedCalibrationComplete() { return advCalibratedCount >= ADV_SECTORS; }

// ──────────────────────────────────────────────────────────────
//  2️⃣  Durante il giro: salva la deviazione bussola‑gyro
//      ▸ gyroDeg      = heading "vero" (gyro+GPS)
//      ▸ compassDeg   = lettura grezza bussola
// ──────────────────────────────────────────────────────────────
static inline void updateAdvancedCalibration(float gyroDeg, float compassDeg) {
    if (!advCalibrationMode) return;

    gyroDeg    = norm360(gyroDeg);
    compassDeg = norm360(compassDeg);

    int   idx = sectorIndex((int)gyroDeg);
    float dev = circDiff(compassDeg, gyroDeg);   // quanto sbaglia bussola

    if (!advCalibrated[idx]) {
        advDeviation[idx]  = dev;               // salva deviazione
        advCalibrated[idx] = true;
        advCalibratedCount++;
        Serial.printf("[CAL] set=%02d  gyro=%6.1f  comp=%6.1f  dev=%7.2f  (%d/36)\n",
                      idx, gyroDeg, compassDeg, dev, advCalibratedCount);
    }

    if (advCalibratedCount >= ADV_SECTORS) {
        advCalibrationMode = false;
        Serial.println("[CAL] completata ✅ tutte le deviazioni registrate");
    }
}

// ──────────────────────────────────────────────────────────────
//  3️⃣  Uso normale: applica la correzione
// ──────────────────────────────────────────────────────────────
static inline int getAdvancedHeading(float compassDegRaw) {
    float compassDeg = norm360(compassDegRaw);
    int   idx        = sectorIndex((int)compassDeg);

    Serial.printf("[ADV] raw=%6.1f°  set=%02d  ", compassDeg, idx);

    if (!advCalibrated[idx]) {
        Serial.println("calib=✖ → grezzo");
        return (int)compassDeg;
    }

    float corr   = compassDeg - advDeviation[idx];   // rimuovi deviazione
    int   out    = norm360(corr);

    Serial.printf("calib=✔ dev=%7.2f  corr=%6.1f  → %3d°\n",
                  advDeviation[idx], corr, out);
    return out;
}

#endif // ADV_CALIBRATION_H
