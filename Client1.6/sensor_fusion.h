#ifndef SENSOR_FUSION_H
#define SENSOR_FUSION_H

#include <Wire.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <math.h>

extern TinyGPSPlus gps;

// Variabili globali per la fusione sensoriale
float accPitchOffset = 0;
float accRollOffset  = 0;
float gyroOffsetZ = 0;
float headingGyro = 0.0;
float gpsHeadingBuffer[5] = {0};
int gpsHeadingIndex = 0;
bool initialGPSOffsetSet = false;

unsigned long prevMicros = 0;
unsigned long lastCorrection = 0;
bool sensorFusionCalibrated = false;

// Costanti di conversione e correzione
const float gyroCalibrationFactor = 0.5;
const float correctionFactor = 0.1;
const unsigned long CORRECTION_INTERVAL = 10000;

float angleDifference(float target, float current) {
  float diff = fmod((target - current + 540.0), 360.0) - 180.0;
  return diff;
}

void readAccelerometer(float &ax, float &ay, float &az) {
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

float readGyroZ() {
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

void calibrateTilt() {
  Serial.println("Calibrating accelerometer tilt...");
  float pitchSum = 0, rollSum = 0;
  const int samples = 500;
  for (int i = 0; i < samples; i++) {
    float ax, ay, az;
    readAccelerometer(ax, ay, az);
    pitchSum += atan2(-ax, sqrt(ay * ay + az * az));
    rollSum  += atan2(ay, az);
    delay(5);
  }
  accPitchOffset = pitchSum / samples;
  accRollOffset  = rollSum / samples;
  Serial.println("Accelerometer calibrated.");
}

void calibrateGyro() {
  Serial.println("Calibrating gyro...");
  float sum = 0;
  const int samples = 500;
  for (int i = 0; i < samples; i++) {
    sum += readGyroZ();
    delay(2);
  }
  gyroOffsetZ = sum / samples;
  Serial.print("Gyro offset Z: ");
  Serial.println(gyroOffsetZ);
}

void resetFusionOffset() {
  if (gps.course.isValid()) {
    headingGyro = gps.course.deg();
    initialGPSOffsetSet = true;
    for (int i = 0; i < 5; i++) {
      gpsHeadingBuffer[i] = headingGyro;
    }
    lastCorrection = millis();
    Serial.print("Fusion offset reset to GPS value: ");
    Serial.println(headingGyro);
  } else {
    Serial.println("Fusion offset reset failed: GPS course not valid.");
  }
}

void updateSensorFusion() {
  if (!sensorFusionCalibrated) {
    calibrateTilt();
    calibrateGyro();
    prevMicros = micros();
    lastCorrection = millis();
    sensorFusionCalibrated = true;
    Serial.println("Sensor Fusion calibration completed.");
  }
  
  unsigned long now = micros();
  float dt = (now - prevMicros) / 1000000.0;
  prevMicros = now;
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
      for (int i = 0; i < 5; i++) {
        gpsHeadingBuffer[i] = currentGPSHeading;
      }
    } else {
      gpsHeadingBuffer[gpsHeadingIndex] = currentGPSHeading;
      gpsHeadingIndex = (gpsHeadingIndex + 1) % 5;
    }
    
    if (millis() - lastCorrection >= CORRECTION_INTERVAL) {
      float avgHeading = 0;
      for (int i = 0; i < 5; i++) {
        avgHeading += gpsHeadingBuffer[i];
      }
      avgHeading /= 5.0;
      
      float diff = angleDifference(avgHeading, headingGyro);
      if (fabs(diff) > 1) {
        headingGyro += correctionFactor * diff;
        headingGyro = fmod(headingGyro + 360.0, 360.0);
      }
      lastCorrection = millis();
    }
  }
}

float getFusedHeading() {
  return headingGyro;
}

#endif