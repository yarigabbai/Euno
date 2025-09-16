#ifndef ICM_COMPASS_H
#define ICM_COMPASS_H

#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

// ===== Inversioni assi (lascia +1 se non serve) =====
#define MAG_INV_X (+1)
#define MAG_INV_Y (+1)
#define MAG_INV_Z (+1)
#define ACC_INV_X (+1)
#define ACC_INV_Y (+1)
#define ACC_INV_Z (+1)
#define GYR_INV_X (+1)
#define GYR_INV_Y (+1)
#define GYR_INV_Z (+1)

// ===== Pin I2C fissi per ESP32-S3 =====
static const int EUNO_I2C_SDA  = 8;
static const int EUNO_I2C_SCL  = 9;
static const uint32_t EUNO_I2C_CLK_INIT = 100000; // init a 100 kHz (robusto)
static const uint32_t EUNO_I2C_CLK_RUN  = 400000; // run a 400 kHz

class ICMCompass {
private:
  Adafruit_ICM20948 icm;
  float mx = 0.0f, my = 0.0f, mz = 0.0f;
  float heading = 0.0f;
  uint8_t used_addr = 0x00;
  bool _initialized = false;

  // Rimetti SEMPRE i pin/clock sul bus prima di ogni operazione critica
  void ensureBus(TwoWire *wire, uint32_t hz){
    wire->begin(EUNO_I2C_SDA, EUNO_I2C_SCL);
    wire->setClock(hz);
  }

  // Probe ACK all'indirizzo
  bool probe(TwoWire *wire, uint8_t addr){
    ensureBus(wire, EUNO_I2C_CLK_INIT);
    wire->beginTransmission(addr);
    uint8_t e = wire->endTransmission(true);
    return (e == 0);
  }

  bool tryBeginAt(TwoWire *wire, uint8_t addr){
    ensureBus(wire, EUNO_I2C_CLK_INIT);
    if (icm.begin_I2C(addr, wire)) {
      used_addr   = addr;
      _initialized = true;
      // dopo init puoi salire a 400 kHz
      ensureBus(wire, EUNO_I2C_CLK_RUN);
      return true;
    }
    return false;
  }

public:
  // Init diretto su un indirizzo (default 0x68)
  bool begin(uint8_t addr = 0x68, TwoWire *wire = &Wire){
    _initialized = false;

    // 1) prima prova: se non risponde, evita il giro inutile della lib
    if (!probe(wire, addr)) {
      // opzionale: prova anche l'altro
      uint8_t other = (addr == 0x68) ? 0x69 : 0x68;
      if (!probe(wire, other)) {
        return false;
      }
      addr = other;
    }

    // 2) a questo punto c'è ACK: inizia davvero
    if (tryBeginAt(wire, addr)) return true;

    // 3) fallback (rarissimo, ma innocuo)
    uint8_t other = (addr == 0x68) ? 0x69 : 0x68;
    if (tryBeginAt(wire, other)) return true;

    _initialized = false;
    return false;
  }

  // Auto: prova 0x68, poi 0x69 (senza cambiare API)
  bool beginAuto(TwoWire *wire = &Wire){
    if (begin(0x68, wire)) return true;
    if (begin(0x69, wire)) return true;
    return false;
  }

  bool isInitialized() const { return _initialized; }
  uint8_t getAddress() const { return used_addr; }

  // ===== Lettura magnetometro (AK09916 via Adafruit wrapper) =====
  void read(){
    if (!_initialized) return;
    sensors_event_t mag;
    auto ms = icm.getMagnetometerSensor();
    if (!ms) return;
    ms->getEvent(&mag);
    mx = mag.magnetic.x * MAG_INV_X;
    my = mag.magnetic.y * MAG_INV_Y;
    mz = mag.magnetic.z * MAG_INV_Z;

    float h = atan2f(my, mx) * 180.0f / (float)M_PI;
    if (h < 0.0f) h += 360.0f;
    heading = h;
  }

  // ===== Accelerometro =====
  bool getAccelEvent(sensors_event_t &out){
    if (!_initialized) return false;
    auto s = icm.getAccelerometerSensor();
    if (!s) return false;
    s->getEvent(&out);
    out.acceleration.x *= ACC_INV_X;
    out.acceleration.y *= ACC_INV_Y;
    out.acceleration.z *= ACC_INV_Z;
    return true;
  }

  // ===== Giroscopio =====
  bool getGyroEvent(sensors_event_t &out){
    if (!_initialized) return false;
    auto s = icm.getGyroSensor();
    if (!s) return false;
    s->getEvent(&out);
    out.gyro.x *= GYR_INV_X;
    out.gyro.y *= GYR_INV_Y;
    out.gyro.z *= GYR_INV_Z;
    return true;
  }

  // ===== Accesso valori =====
  float getX(){ return mx; }
  float getY(){ return my; }
  float getZ(){ return mz; }
  float getHeading(){ return heading; }
};

#endif // ICM_COMPASS_H
