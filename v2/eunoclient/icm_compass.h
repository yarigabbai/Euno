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
  uint8_t used_addr = 0;

public:
  // utile per loggare l’indirizzo usato
  uint8_t getAddress() const { return used_addr; }

  // eventi accel/gyro con check di null
  bool getAccelEvent(sensors_event_t &out) {
    auto s = icm.getAccelerometerSensor();
    if (!s) return false;
    s->getEvent(&out);
    return true;
  }

  bool getGyroEvent(sensors_event_t &out) {
    auto s = icm.getGyroSensor();
    if (!s) return false;
    s->getEvent(&out);
    return true;
  }

  // PROVA addr passato; se fallisce prova l'altro (0x68/0x69).
  // NON chiama Wire.begin(): lo fai solo nello sketch!
  bool begin(uint8_t addr = 0x68, TwoWire *wire = &Wire) {
    if (icm.begin_I2C(addr, wire)) {
      used_addr = addr;
      return true;
    }
    uint8_t other = (addr == 0x68) ? 0x69 : 0x68;
    if (icm.begin_I2C(other, wire)) {
      used_addr = other;
      return true;
    }
    return false;
  }

  // comodo alias che tenta 0x68 poi 0x69
  bool beginAuto(TwoWire *wire = &Wire) {
    return begin(0x68, wire);
  }

  void read() {
    auto m = icm.getMagnetometerSensor();
    if (!m) {
      Serial.println("Magnetometro non presente");
      return;
    }
    sensors_event_t mag;
    m->getEvent(&mag);
    if (isnan(mag.magnetic.x) || isnan(mag.magnetic.y) || isnan(mag.magnetic.z)) {
      Serial.println("Magnetometro non valido");
      return;
    }
    mx = mag.magnetic.x;
    my = mag.magnetic.y;
    mz = mag.magnetic.z;

    float h = atan2(my, mx) * 180.0f / M_PI;
    if (h < 0) h += 360.0f;
    heading = h;
  }

  float getX() { return mx; }
  float getY() { return my; }
  float getZ() { return mz; }
  float getHeading() { return heading; }
};

#endif // ICM_COMPASS_H
