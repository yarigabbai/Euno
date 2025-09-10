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

#include <Arduino.h>
#include <math.h>
#include <TinyGPSPlus.h>

#define EUNO_IS_CLIENT
#include "euno_debug.h"
#include "icm_compass.h"   // tua classe per ICM-20948

// ====== DICHIARAZIONI ESTERNE (già nel progetto) ======================
extern ICMCompass   compass;          // dal .ino
extern TinyGPSPlus  gps;              // dal .ino
extern float        smoothedSpeed;    // nodi (dal .ino)

// Bussola tilt-compensata (tua) da calibration.h
extern int getCorrectedHeading();

// ====== VARIABILI USATE NEL .INO =====================================
// Le tieni così: il .ino le usa direttamente in setup()/loop().
// Le definiamo qui per avere una sola TU (sketch unico) senza linker errors.
extern float headingGyro;
extern float headingExperimental;

// Offsets tilt usati per debug nel tuo .ino (printf Pitch/Roll + offsets)
extern float accPitchOffset;
extern float accRollOffset;

// ====== UTILI ANGOLARI (prefisso sf_ per evitare conflitti) ==========
static inline float sf_wrap360(float a){
  while(a >= 360.0f) a -= 360.0f;
  while(a < 0.0f)    a += 360.0f;
  return a;
}
static inline float sf_wrap180(float a){
  a = fmodf(a + 180.0f, 360.0f);
  if (a < 0) a += 360.0f;
  return a - 180.0f;
}
static inline float sf_angDiff(float to, float from){
  return sf_wrap180(to - from);
}

// ====== PARAMETRI “STILE ANDROID” ====================================
static const float FUSION_HZ         = 100.0f;
static const float ALPHA_SLOW        = 0.98f;   // richiamo magnete 2%/step @100Hz
static const float ALPHA_FAST        = 0.995f;  // 0.5%/step @100Hz
static const float VEL_MIN_KN        = 1.0f;
static const float VEL_MAX_KN        = 4.0f;
static const float TURN_FAST_DEGPS   = 15.0f;
static const uint32_t GPS_CORR_MS    = 10000;
static const float    GPS_CORR_GAIN  = 0.10f;

// ====== STATO INTERNO =================================================
static bool      fusionInit        = false;
static uint32_t  lastMicrosFusion  = 0;

float headingGyro         = 0.0f;  // integrazione gyro (°)
float headingExperimental = 0.0f;  // EXPERIMENTAL = heading fuso

// bias/scala gyro (calib GYRO)
static float gyroBiasZ_radps = 0.0f;
static float gyroScale       = 1.0f;

// buffer media COG per richiamo lento GPS
static float      gpsHBuf[5]   = {0,0,0,0,0};
static int        gpsHIdx      = 0;
static bool       gpsLatched   = false;
static uint32_t   lastGpsCorr  = 0;

// offsets tilt (li esponiamo per i log che fai nel .ino)
float accPitchOffset = 0.0f;
float accRollOffset  = 0.0f;

// ====== LETTURE ICM-20948 via ICMCompass ==============================
static inline bool readGyroZ_radps(float &gz){
  sensors_event_t g;
  if (compass.getGyroEvent(g)) { gz = g.gyro.z; return true; }
  gz = NAN; return false;
}
static inline bool readAccel(float &ax, float &ay, float &az){
  sensors_event_t a;
  if (compass.getAccelEvent(a)) {
    ax = a.acceleration.x; ay = a.acceleration.y; az = a.acceleration.z; return true;
  }
  ax = ay = az = NAN; return false;
}

// ====== INIZIALIZZAZIONE =============================================
static inline void initSensorFusion(){
  float h = (float)getCorrectedHeading();
  headingGyro = headingExperimental = h;

  fusionInit       = true;
  lastMicrosFusion = micros();
  lastGpsCorr      = millis();

  for (int i=0;i<5;i++) gpsHBuf[i] = h;
  gpsHIdx    = 0;
  gpsLatched = false;

  debugLog("Fusion init: H=" + String(h));
}

// ====== CALIBRAZIONE COMPLETA (chiamata dal tuo .ino) ================
static inline void performSensorFusionCalibration(){
  // 1) stima bias gyro Z a fermo
  const int N = 500;
  double sumZ = 0; int ok = 0;
  for (int i=0;i<N;i++){
    float gz; if (readGyroZ_radps(gz)) { sumZ += gz; ok++; }
    delay(3);
  }
  if (ok>0) gyroBiasZ_radps = (float)(sumZ/ok);

  // 2) tilt offset (pitch/roll) — opzionale per debug
  double sP=0, sR=0; ok=0;
  for (int i=0;i<N;i++){
    float ax,ay,az; if (readAccel(ax,ay,az)){
      float pitch = atan2f(-ax, sqrtf(ay*ay + az*az));
      float roll  = atan2f(ay, az);
      sP += pitch; sR += roll; ok++;
    }
    delay(3);
  }
  if (ok>0){ accPitchOffset = (float)(sP/ok); accRollOffset = (float)(sR/ok); }

  // riallineo al valore bussola
  float h = (float)getCorrectedHeading();
  headingGyro = headingExperimental = h;

  debugLog("CAL FUSION: biasZ=" + String(gyroBiasZ_radps,6) +
           " pitchOff=" + String(accPitchOffset,3) +
           " rollOff="  + String(accRollOffset,3));
}

// ====== UPDATE FUSION (100 Hz) =======================================
static inline void updateSensorFusion(){
  if (!fusionInit){ initSensorFusion(); return; }

  uint32_t now = micros();
  float dt = (now - lastMicrosFusion) / 1e6f;
  lastMicrosFusion = now;
  if (dt <= 0.0f || dt > 0.2f) dt = 1.0f / FUSION_HZ;

  // 1) integrazione gyro Z
  float gz;
  if (readGyroZ_radps(gz)){
    float rate_degps = (gz - gyroBiasZ_radps) * 180.0f / (float)M_PI;
    rate_degps *= gyroScale;
    headingGyro = sf_wrap360(headingGyro + rate_degps * dt);
  }

  // 2) bussola tilt-compensata (già dalla tua pipeline)
  float hCompass = (float)getCorrectedHeading();

  // 3) alpha dinamico con velocità e booster in accostata
  float v = smoothedSpeed; // nodi
  float wVel = 0.0f;
  if (v <= VEL_MIN_KN) wVel = 0.0f;
  else if (v >= VEL_MAX_KN) wVel = 1.0f;
  else wVel = (v - VEL_MIN_KN) / (VEL_MAX_KN - VEL_MIN_KN);

  float alpha = ALPHA_SLOW + (ALPHA_FAST - ALPHA_SLOW) * wVel;

  float gz_degps = (!isnan(gz)) ? fabsf(gz) * 180.0f / (float)M_PI : 0.0f;
  if (gz_degps > TURN_FAST_DEGPS) alpha = min(alpha, ALPHA_SLOW);

  // 4) complementare: ri-ancoro dolcemente gyro verso la bussola
  float corr = (1.0f - alpha) * sf_angDiff(hCompass, headingGyro);
  headingGyro = sf_wrap360(headingGyro + corr);

  // 5) EXPERIMENTAL = heading fuso
  headingExperimental = headingGyro;

  // 6) richiamo lento a COG ogni 10s se >2 kn
  if (gps.speed.isValid() && gps.course.isValid() && gps.speed.knots() > 2.0f){
    float cog = gps.course.deg();

    if (!gpsLatched){
      headingGyro = sf_wrap360(cog);
      headingExperimental = headingGyro;
      for (int i=0;i<5;i++) gpsHBuf[i] = cog;
      gpsHIdx=0; gpsLatched=true; lastGpsCorr=millis();
      debugLog("Fusion latch to GPS COG: " + String(cog));
    } else {
      gpsHBuf[gpsHIdx] = cog;
      gpsHIdx = (gpsHIdx + 1) % 5;
      float sum=0; for (int i=0;i<5;i++) sum += gpsHBuf[i];
      float cogAvg = sum / 5.0f;

      uint32_t ms = millis();
      if (ms - lastGpsCorr >= GPS_CORR_MS){
        float d = sf_angDiff(cogAvg, headingGyro);
        headingGyro = sf_wrap360(headingGyro + GPS_CORR_GAIN * d);
        headingExperimental = headingGyro;
        lastGpsCorr = ms;
        debugLog("Fusion GPS slow corr: d=" + String(d) + " → " + String(headingGyro));
      }
    }
  }
}

// ====== API COMPATIBILI CON IL TUO .INO ===============================
static inline float getFusedHeading(){
  if (!fusionInit) initSensorFusion();
  return headingExperimental;
}

// Il tuo .ino chiama updateExperimental(hdgF): lo lasciamo come NO-OP.
static inline void updateExperimental(float /*hdgF*/){
  (void)0;
}

static inline float getExperimentalHeading(){
  if (!fusionInit) initSensorFusion();
  return headingExperimental;
}

static inline float getGyroOnlyHeading(){
  return headingGyro;
}

static inline void setGyroBiasZ_radps(float b){ gyroBiasZ_radps = b; }
static inline void setGyroScale(float s){ gyroScale = s; }

#endif // SENSOR_FUSION_H
