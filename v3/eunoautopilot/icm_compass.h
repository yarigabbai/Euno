#ifndef ICM_COMPASS_H
#define ICM_COMPASS_H

#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

// ==================================================
// CONFIGURAZIONE ORIENTAMENTO (swap/inversioni assi)
// ==================================================
// Dai tuoi dump, l’asse Y NON va invertito: rimettiamo +1.
#define MAG_INV_X   (+1)
#define MAG_INV_Y   (-1)   // <— cambiato da -1 a +1
#define MAG_INV_Z   (+1)

// Se in futuro noti accelerometro/gyro “girati” nel case, regoli qui:
#define ACC_INV_X   (+1)
#define ACC_INV_Y   (-1)
#define ACC_INV_Z   (+1)

#define GYR_INV_X   (-1)
#define GYR_INV_Y   (+1)
#define GYR_INV_Z   (-1)

// ==================================================

class ICMCompass {
private:
  Adafruit_ICM20948 icm;
  float mx = 0.0f, my = 0.0f, mz = 0.0f;
  float heading = 0.0f;
  uint8_t used_addr = 0;

public:
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

  bool beginAuto(TwoWire *wire = &Wire) {
    return begin(0x68, wire);
  }

  // compatibilità
  uint8_t getAddress() const { return used_addr; }

  void read() {
    // Lettura MAG affidabile via sensore dedicato Adafruit (AK09916)
    sensors_event_t mag;
    icm.getMagnetometerSensor()->getEvent(&mag);

    // Applica inversioni definite sopra
    mx = mag.magnetic.x * MAG_INV_X;
    my = mag.magnetic.y * MAG_INV_Y;
    mz = mag.magnetic.z * MAG_INV_Z;

    // Heading matematico: 0°=Est, +90°=Nord; la UI ruota già (deg-90) nel disegno
    float h = atan2f(my, mx) * 180.0f / (float)M_PI;
    if (h < 0.0f) h += 360.0f;
    heading = h;
  }

  // ===== Accessori magnetometro =====
  float getX() { return mx; }
  float getY() { return my; }
  float getZ() { return mz; }
  float getHeading() { return heading; }

  // ===== Accelerometro =====
  bool getAccelEvent(sensors_event_t &out) {
    auto s = icm.getAccelerometerSensor();
    if (!s) return false;
    s->getEvent(&out);
    out.acceleration.x *= ACC_INV_X;
    out.acceleration.y *= ACC_INV_Y;
    out.acceleration.z *= ACC_INV_Z;
    return true;
  }

  // ===== Giroscopio =====
  bool getGyroEvent(sensors_event_t &out) {
    auto s = icm.getGyroSensor();
    if (!s) return false;
    s->getEvent(&out);
    out.gyro.x *= GYR_INV_X;
    out.gyro.y *= GYR_INV_Y;
    out.gyro.z *= GYR_INV_Z;
    return true;
  }
};

#endif // ICM_COMPASS_H
