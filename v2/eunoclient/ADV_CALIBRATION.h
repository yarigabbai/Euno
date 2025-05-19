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
#include <EEPROM.h>
#include <QMC5883LCompass.h>

// ---------- CONFIGURAZIONE ----------

#define ADV_SECTORS 72          // Numero di settori (es. 72 => 1 punto ogni 5°)
#define SMOOTHING_FACTOR 0.2f   // Fattore smoothing per calibrazione
#define ADV_EEPROM_ADDR 100     // Indirizzo EEPROM dove parte la tabella ADV

struct AdvCalPoint {
    float rawX;
    float rawY;
    float rawZ;
    int headingDeg;
    bool calibrated;
};

static AdvCalPoint advTable[ADV_SECTORS];
static int advPointCount = 0;
static bool advCalibrationMode = false;

// ---------- FUNZIONI UTILITY ----------

// Normalizza 0..359
static inline int norm360(float deg) {
    int v = (int)round(deg);
    v %= 360;
    if (v < 0) v += 360;
    return v;
}

// Indice settore per una data angolazione
static inline int sectorIndex(int deg) {
    return norm360(deg) / (360 / ADV_SECTORS);
}

// ---------- CALIBRAZIONE ----------

// Avvia la calibrazione ADV
static inline void startAdvancedCalibration() {
    advPointCount = 0;
    advCalibrationMode = true;
    for (int i = 0; i < ADV_SECTORS; i++) {
        advTable[i].rawX = 0;
        advTable[i].rawY = 0;
        advTable[i].rawZ = 0;
        advTable[i].headingDeg = i * (360 / ADV_SECTORS);
        advTable[i].calibrated = false;
    }
    Serial.println("[CAL] Advanced calibration started");
}

// Aggiorna la tabella durante la calibrazione (X/Y/Z e heading reale)
static inline void updateAdvancedCalibration(float gyroDeg, float compassX, float compassY, float compassZ) {
    if (!advCalibrationMode) return;

    int idx = sectorIndex((int)gyroDeg);

    if (!advTable[idx].calibrated) {
        advTable[idx].rawX = compassX;
        advTable[idx].rawY = compassY;
        advTable[idx].rawZ = compassZ;
        advTable[idx].headingDeg = (int)gyroDeg;
        advTable[idx].calibrated = true;
        advPointCount++;
    } else {
        advTable[idx].rawX = SMOOTHING_FACTOR * compassX + (1 - SMOOTHING_FACTOR) * advTable[idx].rawX;
        advTable[idx].rawY = SMOOTHING_FACTOR * compassY + (1 - SMOOTHING_FACTOR) * advTable[idx].rawY;
        advTable[idx].rawZ = SMOOTHING_FACTOR * compassZ + (1 - SMOOTHING_FACTOR) * advTable[idx].rawZ;
        advTable[idx].headingDeg = (int)gyroDeg;
    }

    if (advPointCount >= ADV_SECTORS) {
        advCalibrationMode = false;
        Serial.println("[CAL] Advanced calibration complete");
    }
}

// Stato calibrazione
static inline bool isAdvancedCalibrationMode() { return advCalibrationMode; }
static inline bool isAdvancedCalibrationComplete() { return advPointCount >= ADV_SECTORS; }

// ---------- APPLICAZIONE DELLA CALIBRAZIONE ----------

// Interpolazione tra i 2 punti 3D più vicini
static inline int applyAdvCalibrationInterp3D(float x, float y, float z) {
    if (advPointCount < 2) return 0;

    int idx1 = -1, idx2 = -1;
    float minDist1 = 1e9, minDist2 = 1e9;

    for (int i = 0; i < ADV_SECTORS; i++) {
        if (!advTable[i].calibrated) continue;
        float dx = x - advTable[i].rawX;
        float dy = y - advTable[i].rawY;
        float dz = z - advTable[i].rawZ;
        float dist = sqrt(dx * dx + dy * dy + dz * dz);
        if (dist < minDist1) {
            minDist2 = minDist1; idx2 = idx1;
            minDist1 = dist;    idx1 = i;
        } else if (dist < minDist2) {
            minDist2 = dist; idx2 = i;
        }
    }
    if (idx2 == -1) return advTable[idx1].headingDeg;
    float w1 = 1.0 / (minDist1 + 1e-6);
    float w2 = 1.0 / (minDist2 + 1e-6);
 // Trasforma gli heading in vettori
    // MEDIA ANGOLARE CIRCOLARE:
    float h1 = advTable[idx1].headingDeg;
    float h2 = advTable[idx2].headingDeg;
    float sumX = w1 * cos(h1 * M_PI / 180.0f) + w2 * cos(h2 * M_PI / 180.0f);
    float sumY = w1 * sin(h1 * M_PI / 180.0f) + w2 * sin(h2 * M_PI / 180.0f);
    float hdg = atan2(sumY, sumX) * 180.0f / M_PI;
    if (hdg < 0) hdg += 360.0f;
    return (int)round(hdg);


}

// Versione semplificata per azimuth (solo XY)
static inline int applyAdvCalibrationInterp2D(float compassDegRaw) {
    float compassX = cos(compassDegRaw * M_PI / 180.0f);
    float compassY = sin(compassDegRaw * M_PI / 180.0f);
    float compassZ = 0;
    return applyAdvCalibrationInterp3D(compassX, compassY, compassZ);
}

// ---------- SALVATAGGIO/CARICAMENTO EEPROM ----------

static inline void saveAdvCalibrationToEEPROM() {
    int addr = ADV_EEPROM_ADDR;
    for (int i = 0; i < ADV_SECTORS; i++) {
        EEPROM.put(addr, advTable[i].rawX);      addr += sizeof(float);
        EEPROM.put(addr, advTable[i].rawY);      addr += sizeof(float);
        EEPROM.put(addr, advTable[i].rawZ);      addr += sizeof(float);
        EEPROM.put(addr, advTable[i].headingDeg);addr += sizeof(int);
        EEPROM.put(addr, advTable[i].calibrated);addr += sizeof(bool);
    }
    EEPROM.commit();
    Serial.println("[ADV] Tabella ADV salvata su EEPROM.");
}

static inline void loadAdvCalibrationFromEEPROM() {
    int addr = ADV_EEPROM_ADDR;
    advPointCount = 0;
    for (int i = 0; i < ADV_SECTORS; i++) {
        EEPROM.get(addr, advTable[i].rawX);      addr += sizeof(float);
        EEPROM.get(addr, advTable[i].rawY);      addr += sizeof(float);
        EEPROM.get(addr, advTable[i].rawZ);      addr += sizeof(float);
        EEPROM.get(addr, advTable[i].headingDeg);addr += sizeof(int);
        EEPROM.get(addr, advTable[i].calibrated);addr += sizeof(bool);
        if (advTable[i].calibrated) advPointCount++;
    }
    Serial.print("[ADV] Tabella ADV caricata da EEPROM. Punti validi: ");
    Serial.println(advPointCount);
}

// ---------- DEBUG ----------

static inline void printAdvCalibrationTable() {
    for (int i = 0; i < ADV_SECTORS; i++) {
        if (!advTable[i].calibrated) continue;
        Serial.printf("#%02d: X=%.2f Y=%.2f Z=%.2f → heading=%d\n",
            i, advTable[i].rawX, advTable[i].rawY, advTable[i].rawZ, advTable[i].headingDeg);
    }
}

#endif // ADV_CALIBRATION_H
