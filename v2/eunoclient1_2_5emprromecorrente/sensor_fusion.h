#ifndef SENSOR_FUSION_H
#define SENSOR_FUSION_H

#include <Wire.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <math.h>
#define EUNO_IS_CLIENT
#include "euno_debug.h"

// ======================================================================
// DICHIARAZIONI DI VARIABILI ESTERNE (definite nello sketch .ino)
// ======================================================================
extern TinyGPSPlus gps;       // GPS gestito esternamente
extern float smoothedSpeed;    // Velocità GPS filtrata (dichiarata e inizializzata nel .ino)
extern int headingOffset;      // Offset (software) bussola, se serve
int getCorrectedHeading();     // Funzione definita nel .ino per leggere la bussola corretta

// ======================================================================
// VARIABILI GLOBALI DI SENSOR FUSION
// ======================================================================
bool gyroInitialized = false;          // Indica se è stato inizializzato il gyro
bool sensorFusionCalibrated = false;   // Indica se abbiamo già fatto la calibrazione iniziale
bool initialGPSOffsetSet = false;      // Indica se abbiamo sincronizzato il gyro al GPS la prima volta
bool expInitialized = false;           // Per l’heading sperimentale

float accPitchOffset = 0;     // Offset accelerometro per tilt pitch
float accRollOffset  = 0;     // Offset accelerometro per tilt roll
float gyroOffsetZ    = 0;     // Offset di zero del giroscopio (asse Z)

// Heading principali:
float headingGyro = 0.0;         // Il valore “fuso” da IMU + GPS
float headingExperimental = 0.0; // Filtro sperimentale aggiuntivo
float getAdvancedHeading();  // nuovo algoritmo “ADV”

// Buffer per correzione media GPS
float gpsHeadingBuffer[5];
int   gpsHeadingIndex  = 0;

// Tempi di riferimento
unsigned long prevMicros     = 0;
unsigned long lastCorrection = 0;

// ======================================================================
// COSTANTI E PARAMETRI DI CALIBRAZIONE
// ======================================================================
const float alphaSpeed            = 0.2;   // coeff. per la media esponenziale della velocità
const float gyroCalibrationFactor = 0.5;   // fattore di calibrazione giroscopio
const float correctionFactor      = 0.1;   // fattore di correzione verso heading GPS
const unsigned long CORRECTION_INTERVAL = 10000; // ms fra correzioni verso GPS

// ======================================================================
// FUNZIONI DI SUPPORTO
// ======================================================================

// Ritorna la differenza angolare in range -180°..+180°
inline float angleDifference(float a, float b) {
  float d = fmod((a - b + 540.0), 360.0) - 180.0;
  return d;
}

// Legge i valori dell'accelerometro (e.g. su MPU6050 all’indirizzo 0x68)
inline void readAccelerometer(float &ax, float &ay, float &az) {
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 6);
  if (Wire.available() >= 6) {
    int16_t rawX = (Wire.read() << 8) | Wire.read();
    int16_t rawY = (Wire.read() << 8) | Wire.read();
    int16_t rawZ = (Wire.read() << 8) | Wire.read();
    ax = (float)rawX / 16384.0;
    ay = (float)rawY / 16384.0;
    az = (float)rawZ / 16384.0;
  }
}

// Legge il valore del giroscopio asse Z (e.g. su MPU6050)
inline float readGyroZ() {
  Wire.beginTransmission(0x68);
  Wire.write(0x47);
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 2);
  if (Wire.available() >= 2) {
    int16_t rawZ = (Wire.read() << 8) | Wire.read();
    return (float)rawZ;
  }
  return 0.0;
}

// Calibra inclinazione (pitch/roll) con l'accelerometro
inline void calibrateTilt() {
  debugLog("DEBUG: Calibrating accelerometer tilt...");
  float pitchSum = 0;
  float rollSum  = 0;
  const int samples = 500;
  for (int i = 0; i < samples; i++) {
    float ax, ay, az;
    readAccelerometer(ax, ay, az);
    pitchSum += atan2(-ax, sqrt(ay * ay + az * az));
    rollSum  += atan2(ay, az);
    delay(5);
  }
  accPitchOffset = pitchSum / samples;
  accRollOffset  = rollSum  / samples;
  debugLog("DEBUG: Accelerometer tilt calibrated.");
}

// Calibra lo zero del giroscopio sull’asse Z
inline void calibrateGyro() {
  debugLog("DEBUG: Calibrating gyro...");
  float sum = 0;
  const int samples = 500;
  for (int i = 0; i < samples; i++) {
    sum += readGyroZ();
    delay(2);
  }
  gyroOffsetZ = sum / samples;
  debugLog("DEBUG: Gyro offset Z: " + String(gyroOffsetZ));
}
inline void performSensorFusionCalibration() {
  debugLog("DEBUG: Calibrating accelerometer tilt...");
  calibrateTilt();

  debugLog("DEBUG: Calibrating gyro...");
  calibrateGyro();

  headingGyro = getCorrectedHeading();
  debugLog("DEBUG: headingGyro init from compass: " + String(headingGyro));

  prevMicros = micros();
  lastCorrection = millis();

  sensorFusionCalibrated = true;
  gyroInitialized = true;

  for (int i = 0; i < 5; i++) gpsHeadingBuffer[i] = headingGyro;
  gpsHeadingIndex = 0;

  debugLog("DEBUG: Sensor Fusion calibration completed.");
}

// Aggiorna la velocità GPS con filtro esponenziale
inline void updateSmoothedSpeed() {
  if (gps.speed.isValid()) {
    float nuova = gps.speed.knots();
    if (smoothedSpeed == 0.0) {
      smoothedSpeed = nuova;
    } else {
      smoothedSpeed = alphaSpeed * nuova + (1.0 - alphaSpeed) * smoothedSpeed;
    }
  }
}

// ======================================================================
// FUNZIONI PRINCIPALI DI SENSOR FUSION
// ======================================================================
inline void updateSensorFusion() {
  updateSmoothedSpeed();

  if (!sensorFusionCalibrated) {
    calibrateTilt();
    calibrateGyro();
    headingGyro = getCorrectedHeading();
    debugLog("DEBUG: headingGyro init from compass: " + String(headingGyro));

    prevMicros = micros();
    lastCorrection = millis();

    sensorFusionCalibrated = true;
    gyroInitialized        = true;
    for (int i = 0; i < 5; i++) gpsHeadingBuffer[i] = headingGyro;
    gpsHeadingIndex = 0;
    debugLog("DEBUG: Sensor Fusion calibration completed.");
  }

  unsigned long nowMicros = micros();
  float dt = (nowMicros - prevMicros) / 1000000.0;
  prevMicros = nowMicros;
  if (dt <= 0) return;

  float rawGz = readGyroZ();
  float rateZ = -((rawGz - gyroOffsetZ) / 65.5) * gyroCalibrationFactor;
  headingGyro += rateZ * dt;
  headingGyro = fmod(headingGyro + 360.0, 360.0);

  if (gps.speed.isValid() && gps.speed.knots() > 2 && gps.course.isValid()) {
    float currentGPSHeading = gps.course.deg();
    if (!initialGPSOffsetSet) {
      headingGyro = currentGPSHeading;
      initialGPSOffsetSet = true;
      for (int i = 0; i < 5; i++) gpsHeadingBuffer[i] = currentGPSHeading;
      gpsHeadingIndex = 0;
      debugLog("DEBUG: Fusion offset reset to GPS heading: " + String(currentGPSHeading));
    } else {
      gpsHeadingBuffer[gpsHeadingIndex] = currentGPSHeading;
      gpsHeadingIndex = (gpsHeadingIndex + 1) % 5;
    }

    if (millis() - lastCorrection >= CORRECTION_INTERVAL) {
      float sumGPS = 0;
      for (int i = 0; i < 5; i++) sumGPS += gpsHeadingBuffer[i];
      float avgGPS = sumGPS / 5.0;
      float diff = angleDifference(avgGPS, headingGyro);
      if (fabs(diff) > 1.0) {
        headingGyro += correctionFactor * diff;
        headingGyro = fmod(headingGyro + 360.0, 360.0);
      }
      lastCorrection = millis();
    }
  }
}

inline float getFusedHeading() {
  return headingGyro;
}

// ======================================================================
// EXPERIMENTAL HEADING (parametrico con alpha dinamico)
// ======================================================================
inline void updateExperimental(float input) {
  const float alphaFast = 0.8f;   // rapido per grandi differenze
  const float alphaSlow = 0.3f;   // più lento per piccoli aggiustamenti
  const float thresh   = 10.0f;   // soglia in gradi

  if (!expInitialized) {
    headingExperimental = input;
    expInitialized = true;
    return;
  }
  float diff = angleDifference(input, headingExperimental);
  float alpha = (fabs(diff) > thresh) ? alphaFast : alphaSlow;
  headingExperimental += alpha * diff;
  headingExperimental = fmod(headingExperimental + 360.0f, 360.0f);
}

inline float getExperimentalHeading() {
  return headingExperimental;
}
inline float getAdvancedHeading() {
    // per ora ricicliamo experimental, qui puoi mettere il tuo algoritmo ADV
    return headingExperimental;
}


#endif // SENSOR_FUSION_H
