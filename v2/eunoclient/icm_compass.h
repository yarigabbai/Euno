// icm_compass.h

#ifndef ICM_COMPASS_H
#define ICM_COMPASS_H

#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

class ICMCompass {
private:
  Adafruit_ICM20948 icm;
  float mx = 0.0, my = 0.0, mz = 0.0;
  float heading = 0.0;

public:
sensors_event_t getAccelEvent() {
  sensors_event_t accel;
  icm.getAccelerometerSensor()->getEvent(&accel);
  return accel;
}

sensors_event_t getGyroEvent() {
  sensors_event_t gyro;
  icm.getGyroSensor()->getEvent(&gyro);
  return gyro;
}

  bool begin(uint8_t addr = 0x68, TwoWire *wire = &Wire) {
    wire->begin(8, 9); // SDA=8, SCL=9 per ESP32-S3
    wire->setClock(400000);
    return icm.begin_I2C(addr, wire);
  }

  void read() {
    sensors_event_t mag;
if (icm.getMagnetometerSensor()) {
    icm.getMagnetometerSensor()->getEvent(&mag);
}

    if (!isnan(mag.magnetic.x) && !isnan(mag.magnetic.y) && !isnan(mag.magnetic.z)) {
      mx = mag.magnetic.x;
      my = mag.magnetic.y;
      mz = mag.magnetic.z;
    } else {
      Serial.println("Magnetometro non valido");
      return;
    }

    float h = atan2(my, mx) * 180.0 / M_PI;
    if (h < 0) h += 360.0;
    heading = h;
  }

  float getX() { return mx; }
  float getY() { return my; }
  float getZ() { return mz; }
  float getHeading() { return heading; }
};

#endif // ICM_COMPASS_H
