// icm_compass.h — robust auto-address + accel/gyro helpers + mag fallback

#ifndef ICM_COMPASS_H
#define ICM_COMPASS_H

#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

class ICMCompass {
private:
  Adafruit_ICM20948 icm;
  Adafruit_Sensor *magSensor = nullptr;

  float mx = 0.0f, my = 0.0f, mz = 0.0f;
  float heading = 0.0f;
  uint8_t used_addr = 0x00;
  bool inited = false;

  void ensureBus(TwoWire *w, uint32_t hz){
    w->begin(8, 9);        // SDA=9, SCL=8 nel tuo wiring
    w->setClock(hz);
  }

  bool tryBegin(TwoWire *w, uint8_t addr){
    ensureBus(w, 100000);          // start slow
    if (!icm.begin_I2C(addr, w)) return false;

    // Config come nel rawdump
    icm.setAccelRange(ICM20948_ACCEL_RANGE_2_G);
    icm.setGyroRange(ICM20948_GYRO_RANGE_250_DPS);
    icm.setMagDataRate(AK09916_MAG_DATARATE_100_HZ);

    magSensor = icm.getMagnetometerSensor();

    used_addr = addr;
    inited = true;
    ensureBus(w, 400000);          // then speed up
    return true;
  }

  // Fallback: lettura diretta EXT_SENS_DATA (magnetometro AK09916)
  bool readMagRaw(float &mxOut, float &myOut, float &mzOut){
    uint8_t buf[6] = {0};
    // 0x3B..0x40 contengono i dati mag se lo slave interno è configurato
    Wire.beginTransmission(used_addr);
    Wire.write(0x3B);
    if (Wire.endTransmission(false) != 0) return false;
    int n = Wire.requestFrom((int)used_addr, 6);
    if (n != 6) return false;

    int16_t rx = (Wire.read() | (Wire.read() << 8));
    int16_t ry = (Wire.read() | (Wire.read() << 8));
    int16_t rz = (Wire.read() | (Wire.read() << 8));

    mxOut = (float)rx;
    myOut = (float)ry;
    mzOut = (float)rz;
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

  // Read just the magnetometer (con fallback)
  void read(){
    if (!inited) return;
    sensors_event_t mag;

    bool ok = false;
    if (magSensor) {
      magSensor->getEvent(&mag);
      mx = mag.magnetic.x;
      my = mag.magnetic.y;
      mz = mag.magnetic.z;
      ok = true;
    } 
    else {
      float rx, ry, rz;
      if (readMagRaw(rx, ry, rz)) {
        mx = rx; my = ry; mz = rz;
        ok = true;
      }
    }

    if (ok) {
      float h = atan2f(my, mx) * 180.0f / (float)M_PI;
      if (h < 0) h += 360.0f;
      heading = h;
    }
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
