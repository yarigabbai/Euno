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

#ifndef SENSOR_FUSION_H
#define SENSOR_FUSION_H

#include <Wire.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <math.h>
#define EUNO_IS_CLIENT
#include "euno_debug.h"
#include "icm_compass.h"  // serve per conoscere la classe

extern ICMCompass compass;

// ======================================================================
// DICHIARAZIONI DI VARIABILI ESTERNE (definite nello sketch .ino)
// ======================================================================
extern TinyGPSPlus gps;       // GPS gestito esternamente
extern float smoothedSpeed;   // Velocità GPS filtrata (dichiarata e inizializzata nel .ino)
extern int headingOffset;     // Offset (software) bussola, se serve
int getCorrectedHeading();    // Funzione definita nel .ino per leggere la bussola corretta

// ======================================================================
// VARIABILI GLOBALI DI SENSOR FUSION
// ======================================================================
bool gyroInitialized = false;          
bool sensorFusionCalibrated = false;   
bool initialGPSOffsetSet = false;      
bool expInitialized = false;           

float accPitchOffset = 0;     
float accRollOffset  = 0;     
float gyroOffsetZ    = 0;     

// Heading principali (in gradi):
float headingGyro = 0.0;         
float headingExperimental = 0.0; 
float getAdvancedHeading();      

// Buffer per correzione media GPS
float gpsHeadingBuffer[5];
int   gpsHeadingIndex  = 0;

// Tempi di riferimento
unsigned long prevMicros     = 0;
unsigned long lastCorrection = 0;

// ======================================================================
// COSTANTI E PARAMETRI DI CALIBRAZIONE
// ======================================================================
const float alphaSpeed            = 0.2;    
const float gyroCalibrationFactor = 1.0;    
const float correctionFactor      = 0.1;    
const unsigned long CORRECTION_INTERVAL = 10000; // ms

// ======================================================================
// FUNZIONI DI SUPPORTO
// ======================================================================

// Differenza angolare in range -180°..+180°
inline float angleDifference(float a, float b) {
  float d = fmod((a - b + 540.0f), 360.0f) - 180.0f;
  return d;
}

// ACC: riempi davvero ax, ay, az (in m/s^2)
inline void readAccelerometer(float &ax, float &ay, float &az) {
  sensors_event_t acc;
  if (compass.getAccelEvent(acc)) {
    ax = acc.acceleration.x;
    ay = acc.acceleration.y;
    az = acc.acceleration.z;
  } else {
    ax = ay = az = NAN;
  }
}

// GYRO Z: ritorna **rad/s**
inline float readGyroZ() {
  sensors_event_t g;
  if (compass.getGyroEvent(g)) {
    return g.gyro.z; // rad/s
  }
  return NAN;
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
    pitchSum += atan2f(-ax, sqrtf(ay*ay + az*az));
    rollSum  += atan2f(ay, az);
    delay(5);
  }
  
  accPitchOffset = pitchSum / samples;
  accRollOffset  = rollSum  / samples;
  
  debugLog("DEBUG: Tilt calibrated. Pitch offset: " + String(accPitchOffset, 4) + 
           " rad, Roll offset: " + String(accRollOffset, 4) + " rad");
}

// Calibra lo zero del giroscopio sull’asse Z (**rad/s**)
inline void calibrateGyro() {
  debugLog("DEBUG: Calibrating gyro...");
  float sum = 0;
  const int samples = 500;
  for (int i = 0; i < samples; i++) {
    float gz = readGyroZ();
    if (!isnan(gz)) sum += gz;
    delay(2);
  }
  gyroOffsetZ = sum / samples;  // rad/s
  debugLog("DEBUG: Gyro offset Z (rad/s): " + String(gyroOffsetZ, 6));
}

inline void performSensorFusionCalibration() {
  calibrateTilt();
  calibrateGyro();

  headingGyro = getCorrectedHeading();   // gradi
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
    if (smoothedSpeed == 0.0f) smoothedSpeed = nuova;
    else smoothedSpeed = alphaSpeed * nuova + (1.0f - alphaSpeed) * smoothedSpeed;
  }
}

// ======================================================================
// FUNZIONI PRINCIPALI DI SENSOR FUSION
// ======================================================================
inline void updateSensorFusion() {
  updateSmoothedSpeed();

  unsigned long nowMicros = micros();
  float dt = (nowMicros - prevMicros) / 1000000.0f;
  prevMicros = nowMicros;
  if (dt <= 0) return;

  // ---- INTEGRAZIONE GYRO (ICM20948 → rad/s) ----
  float rawGz = readGyroZ();               
  if (!isnan(rawGz)) {
    float gz_rad = (rawGz - gyroOffsetZ);  
    float rateZ_deg = (gz_rad * 180.0f / M_PI) * gyroCalibrationFactor; 
    headingGyro += rateZ_deg * dt;         
    headingGyro = fmodf(headingGyro + 360.0f, 360.0f);
  }

  // ---- CORREZIONE LENTA VERSO GPS (se > 2 kn e valido) ----
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
      float avgGPS = sumGPS / 5.0f;
      float diff = angleDifference(avgGPS, headingGyro);
      if (fabsf(diff) > 1.0f) {
        headingGyro += correctionFactor * diff;
        headingGyro = fmodf(headingGyro + 360.0f, 360.0f);
      }
      lastCorrection = millis();
    }
  }
}

inline float getFusedHeading() {
  return headingGyro; // gradi
}

// ======================================================================
// EXPERIMENTAL HEADING
// ======================================================================
inline void updateExperimental(float input) {
  const float alphaFast = 0.8f;   
  const float alphaSlow = 0.3f;   
  const float thresh   = 10.0f;   

  if (!expInitialized) {
    headingExperimental = input;
    expInitialized = true;
    return;
  }
  float diff = angleDifference(input, headingExperimental);
  float alpha = (fabsf(diff) > thresh) ? alphaFast : alphaSlow;
  headingExperimental += alpha * diff;
  headingExperimental = fmodf(headingExperimental + 360.0f, 360.0f);
}

inline float getExperimentalHeading() {
  return headingExperimental;
}

#endif // SENSOR_FUSION_H
