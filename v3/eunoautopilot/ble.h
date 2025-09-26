#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

// TikTokBLE — client NimBLE per anello TikTok HID (ESP32/ESP32-S3)
// - Scansione servizio HID 0x1812
// - Protocol Mode -> Report (0x2A4E = 0x01)
// - HID Control Point -> Exit Suspend (0x2A4A = 0x00)
// - Subscribe a tutte le caratteristiche notificabili del servizio HID (e fallback su tutto il device)
// - Decodifica tasti secondo i tuoi log
// - Unico hook: onPeunoCmd(const char*) che emette $PEUNO,CMD,DELTA=... / TOGGLE=1

class TikTokBLE {
public:
  // Hook per inoltrare nel tuo pipeline PEUNO (impostalo nel setup del .ino)
  // Esempio:
  //   ble.onPeunoCmd = [](const char* line){
  //     extern EunoNetwork net;        // usa il nome REALE della tua istanza
  //     net.onUdpLine(String(line));   // stesso ingresso dei comandi da rete
  //   };
  std::function<void(const char*)> onPeunoCmd = nullptr;

  void begin() {
    if (started) return;
    started = true;

    NimBLEDevice::init("");
    scan = NimBLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new MyScanCallbacks(this));
    scan->setInterval(45);
    scan->setWindow(30);
    scan->setActiveScan(true);

#if defined(SERIAL_DEBUG_BLE)
    Serial.println("[BLE] start (HID scan)");
#endif
  }

  void loop() {
    if (!connected && !hasPending) {
      scan->start(5, false);   // finestra breve, non blocca
      delay(80);
    }
    if (hasPending && !connected) {
      hasPending = false;
      connectToHID(pendingAddr);
    }
  }

private:
  // ===== stato
  bool started    = false;
  bool connected  = false;

  NimBLEScan*   scan       = nullptr;
  NimBLEClient* client     = nullptr;
  NimBLEAddress pendingAddr;
  bool          hasPending = false;

  unsigned long lastPressTime = 0;
  const unsigned long debounceDelay = 200;

  // ===== decodifica tasti (come nello sketch che funzionava)
  static const char* decodeKey(const uint8_t* data, size_t len) {
    if (len < 2) return "??";

    // release: tutti 0x00
    bool allZero = true;
    for (size_t i = 0; i < len; i++) {
      if (data[i] != 0x00) { allZero = false; break; }
    }
    if (allZero) return "RELEASE";

    // pattern osservati
    if (len >= 2) {
      if (data[0] == 0x07 && data[1] == 0x06) {
        if (len >= 5 && data[4] == 0x80) return "RIGHT";
        return "UP";
      }
      if (data[0] == 0x03 && data[1] == 0x05) return "DOWN";
      if (data[0] == 0x03 && data[1] == 0x04) return "LEFT";
      if (data[0] == 0x07 && data[1] == 0x07) return "CENTER";
    }

#if defined(SERIAL_DEBUG_BLE)
    Serial.print("UNKNOWN: ");
    for (size_t i = 0; i < len && i < 16; i++) Serial.printf("%02X ", data[i]);
    Serial.println();
#endif
    return "UNKNOWN";
  }

  // ===== notify (stampa RAW + key e inoltra in formato PEUNO)
  static void notifyCB(NimBLERemoteCharacteristic* /*c*/, uint8_t* data, size_t len, bool) {
    if (!self) return;

    const char* key = decodeKey(data, len);

#if defined(SERIAL_DEBUG_BLE)
    Serial.printf("[BLE] RAW(%u): ", (unsigned)len);
    for (size_t i=0; i<len && i<16; ++i) Serial.printf("%02X ", data[i]);
    Serial.printf("→ %s\n", key);
#endif

    if (strcmp(key, "UNKNOWN") == 0 || strcmp(key, "RELEASE") == 0) return;

    unsigned long now = millis();
    if (now - self->lastPressTime < self->debounceDelay) return;
    self->lastPressTime = now;

    if (!self->onPeunoCmd) return;

    // Inoltra in pipeline PEUNO
    if      (!strcmp(key, "UP"))     self->onPeunoCmd("$PEUNO,CMD,DELTA=+10");
    else if (!strcmp(key, "DOWN"))   self->onPeunoCmd("$PEUNO,CMD,DELTA=-10");
    else if (!strcmp(key, "RIGHT"))  self->onPeunoCmd("$PEUNO,CMD,DELTA=+1");
    else if (!strcmp(key, "LEFT"))   self->onPeunoCmd("$PEUNO,CMD,DELTA=-1");
    else if (!strcmp(key, "CENTER")) self->onPeunoCmd("$PEUNO,CMD,TOGGLE=1");
  }

  // ===== connessione robusta a HID + fallback
  void connectToHID(const NimBLEAddress& addr) {
#if defined(SERIAL_DEBUG_BLE)
    Serial.printf("[CONN] Mi connetto a %s\n", addr.toString().c_str());
#endif

    client = NimBLEDevice::createClient();
    if (!client->connect(addr)) {
#if defined(SERIAL_DEBUG_BLE)
      Serial.println("[ERR] Connessione fallita");
#endif
      NimBLEDevice::deleteClient(client);
      client = nullptr;
      return;
    }
#if defined(SERIAL_DEBUG_BLE)
    Serial.println("[CONN] Connesso!");
#endif

    // --- Servizio HID 0x1812
    NimBLERemoteService* hid = client->getService(NimBLEUUID((uint16_t)0x1812));
    if (hid) {
#if defined(SERIAL_DEBUG_BLE)
      Serial.println("[HID] Servizio HID trovato (0x1812)");
#endif
      // (A) Protocol Mode -> Report (0x2A4E = 0x01)
      if (auto pm = hid->getCharacteristic(NimBLEUUID((uint16_t)0x2A4E))) {
        uint8_t mode = 0x01; // Report mode
        pm->writeValue(&mode, 1, true);
#if defined(SERIAL_DEBUG_BLE)
        Serial.println("[HID] Protocol Mode = REPORT");
#endif
      }
      // (B) HID Control Point -> Exit Suspend (0x2A4A = 0x00)
      if (auto hcp = hid->getCharacteristic(NimBLEUUID((uint16_t)0x2A4A))) {
        uint8_t v = 0x00; // 0x00 Exit Suspend
        hcp->writeValue(&v, 1, true);
#if defined(SERIAL_DEBUG_BLE)
        Serial.println("[HID] Control Point = EXIT SUSPEND");
#endif
      }

      // (C) Subscribe a TUTTE le caratteristiche notificabili del servizio HID
      bool subscribed = false;
      auto chars = hid->getCharacteristics(true);
      for (auto c : *chars) {
        if (c->canNotify() || c->canIndicate()) {
          if (c->subscribe(true, notifyCB)) {
            if (auto cccd = c->getDescriptor(NimBLEUUID((uint16_t)0x2902))) {
              uint8_t val[2] = {0x01, 0x00};
              cccd->writeValue(val, 2, true);
            }
#if defined(SERIAL_DEBUG_BLE)
            Serial.printf("[HID] Subscribed → %s\n", c->getUUID().toString().c_str());
#endif
            subscribed = true;
            connected  = true;
            // NON break: alcuni device inviano su più report/char
          }
        }
      }
      if (subscribed) return;
    } else {
#if defined(SERIAL_DEBUG_BLE)
      Serial.println("[HID] Servizio non trovato → fallback");
#endif
    }

    // (D) Fallback: subscribe a tutte le Notify dell'intero device
    auto svcs = client->getServices(true);
    for (auto s : *svcs) {
      auto cs = s->getCharacteristics(true);
      for (auto c : *cs) {
        if (c->canNotify() || c->canIndicate()) {
          if (c->subscribe(true, notifyCB)) {
            if (auto cccd = c->getDescriptor(NimBLEUUID((uint16_t)0x2902))) {
              uint8_t val[2] = {0x01, 0x00};
              cccd->writeValue(val, 2, true);
            }
#if defined(SERIAL_DEBUG_BLE)
            Serial.printf("[FBK] Subscribed → svc=%s char=%s\n",
                          s->getUUID().toString().c_str(),
                          c->getUUID().toString().c_str());
#endif
            connected = true;
            // continuo senza break: talvolta serve più di una char
          }
        }
      }
    }

#if defined(SERIAL_DEBUG_BLE)
    if (!connected) Serial.println("[BLE] Nessuna caratteristica notificabile trovata");
#endif
  }

  // ===== scan HID
  class MyScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  public:
    explicit MyScanCallbacks(TikTokBLE* parent) : p(parent) {}
    void onResult(NimBLEAdvertisedDevice* dev) override {
      if (p->connected || p->hasPending) return;
      if (dev->isAdvertisingService(NimBLEUUID((uint16_t)0x1812))) {
        p->pendingAddr = dev->getAddress();
        p->hasPending  = true;
#if defined(SERIAL_DEBUG_BLE)
        Serial.printf("[SCAN] Trovato TikTok Ring: %s (%s)\n",
                      dev->getName().c_str(),
                      dev->getAddress().toString().c_str());
#endif
      }
    }
  private:
    TikTokBLE* p;
  };

  // puntatore statico per notifyCB
  static TikTokBLE* self;

public:
  TikTokBLE() { self = this; }
};

// definizione del puntatore statico
TikTokBLE* TikTokBLE::self = nullptr;
