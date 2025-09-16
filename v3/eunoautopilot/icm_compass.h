// icm_compass.h — robust auto‑address + accel/gyro helpers

#ifndef ICM_COMPASS_H
#define ICM_COMPASS_H

#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

class ICMCompass {
private:
  Adafruit_ICM20948 icm;
  float mx = 0.0f, my = 0.0f, mz = 0.0f;
  float heading = 0.0f;
  uint8_t used_addr = 0x00;
  bool inited = false;

  void ensureBus(TwoWire *w, uint32_t hz){
    w->begin(8, 9);        // ESP32‑S3 default pins in your project
    w->setClock(hz);
  }

  bool tryBegin(TwoWire *w, uint8_t addr){
    ensureBus(w, 100000);          // start slow for "problematic" boards
    if (!icm.begin_I2C(addr, w)) return false;
    used_addr = addr;
    inited = true;
    ensureBus(w, 400000);          // then speed up
    return true;
  }

public:
  // Try the provided address (default 0x68). Returns true on success.
  bool begin(uint8_t addr = 0x68, TwoWire *w = &Wire){
    inited = false;
    return tryBegin(w, addr);
  }

  // Auto scan: 0x68 then 0x69.
  bool beginAuto(TwoWire *w = &Wire){
    if (tryBegin(w, 0x68)) return true;
    delay(2);
    if (tryBegin(w, 0x69)) return true;
    return false;
  }

  bool isInitialized() const { return inited; }
  uint8_t getAddress() const { return used_addr; }

  // Read just the magnetometer (cheaper than getEvent for all sensors)
  void read(){
    if (!inited) return;
    sensors_event_t mag;
    auto m = icm.getMagnetometerSensor();
    if (!m) return;
    m->getEvent(&mag);

    mx = mag.magnetic.x;
    my = mag.magnetic.y;
    mz = mag.magnetic.z;

    float h = atan2f(my, mx) * 180.0f / (float)M_PI;
    if (h < 0) h += 360.0f;
    heading = h;
  }

  // Helpers for fusion code
  bool getAccelEvent(sensors_event_t &out){
    if (!inited) return false;
    auto s = icm.getAccelerometerSensor();
    if (!s) return false;
    s->getEvent(&out);
    return true;
  }
  bool getGyroEvent(sensors_event_t &out){
    if (!inited) return false;
    auto s = icm.getGyroSensor();
    if (!s) return false;
    s->getEvent(&out);
    return true;
  }

  // Accessors
  float getX() const { return mx; }
  float getY() const { return my; }
  float getZ() const { return mz; }
  float getHeading() const { return heading; }
};

#endif // ICM_COMPASS_H