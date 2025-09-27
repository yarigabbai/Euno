#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_bt.h>   // per liberare memoria BT Classic (coexist WiFi+BLE)

// TikTokBLE — client NimBLE per anello TikTok HID (ESP32/ESP32-S3)
// - Scansione servizio HID 0x1812 (asincrona, non blocca il loop)
// - Protocol Mode -> Report (0x2A4E = 0x01)
// - HID Control Point -> Exit Suspend (0x2A4A = 0x00)
// - Subscribe alle caratteristiche notificabili del servizio HID
// - Hook: onPeunoCmd(const char*) che emette $PEUNO,CMD,DELTA=... / TOGGLE=1
//
// Modifiche anti-freeze loop:
// 1) Scansione BLE asincrona (start(..., scanCompleteCB)) invece di start(5,false) bloccante
// 2) Stop immediato della scansione quando troviamo il device target (onResult → stop())
// 3) Timeout connessione ridotto (5 s) + piccoli yield() durante le fasi lente
// 4) Duplicate filter ON e finestra di scan più leggera (coexist Wi-Fi)

class TikTokBLE {
public:
  std::function<void(const char*)> onPeunoCmd = nullptr;

  void begin() {
    if (started) return;
    started = true;

    // Solo BLE: libera memoria per BT Classic (riduce contese col Wi-Fi)
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    NimBLEDevice::init("");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);          // potenza moderata
    NimBLEDevice::setMTU(69);                        // MTU contenuto

    scan = NimBLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new MyScanCallbacks(this));
    scan->setInterval(160);                          // 100 ms
    scan->setWindow(80);                             // 50 ms
    scan->setActiveScan(true);
    scan->setDuplicateFilter(true);

#if defined(SERIAL_DEBUG_BLE)
    Serial.println("[BLE] start (HID scan, async)");
#endif
  }

  void loop() {
    // Avvia la scansione UNA SOLA VOLTA in modo ASINCRONO (non blocca)
    if (!connected && !scanning && !hasPending) {
      scanning = true;
      // durata 0 = continua finché non fai stop(); callback al termine
      scan->start(0, scanCompleteCB);
#if defined(SERIAL_DEBUG_BLE)
      Serial.println("[BLE] Scansione avviata (async)");
#endif
    }

    // Se abbiamo trovato un target, interrompiamo la scansione e ci connettiamo
    if (hasPending && !connected) {
      hasPending = false;
      if (scanning) { scan->stop(); scanning = false; }
      connectToHID(pendingAddr);
      yield();
    }
  }

private:
  // ===== stato
  bool started    = false;
  bool connected  = false;
  bool scanning   = false;

  NimBLEScan*   scan       = nullptr;
  NimBLEClient* client     = nullptr;
  NimBLEAddress pendingAddr;
  bool          hasPending = false;

  unsigned long lastPressTime = 0;
  const unsigned long debounceDelay = 200;

  // ===== decodifica tasti
  static const char* decodeKey(const uint8_t* data, size_t len) {
    if (len < 2) return "??";
    bool allZero = true;
    for (size_t i = 0; i < len; i++) { if (data[i] != 0x00) { allZero = false; break; } }
    if (allZero) return "RELEASE";
    if (len >= 2) {
      if (data[0] == 0x07 && data[1] == 0x06) { if (len >= 5 && data[4] == 0x80) return "RIGHT"; return "UP"; }
      if (data[0] == 0x03 && data[1] == 0x05) return "DOWN";
      if (data[0] == 0x03 && data[1] == 0x04) return "LEFT";
      if (data[0] == 0x07 && data[1] == 0x07) return "CENTER";
    }
#if defined(SERIAL_DEBUG_BLE)
    Serial.print("UNKNOWN: "); for (size_t i = 0; i < len && i < 16; i++) Serial.printf("%02X ", data[i]); Serial.println();
#endif
    return "UNKNOWN";
  }

  // ===== notify → inoltro in formato PEUNO
  static void notifyCB(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
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

    if      (!strcmp(key, "UP"))     self->onPeunoCmd("$PEUNO,CMD,DELTA=+10");
    else if (!strcmp(key, "DOWN"))   self->onPeunoCmd("$PEUNO,CMD,DELTA=-10");
    else if (!strcmp(key, "RIGHT"))  self->onPeunoCmd("$PEUNO,CMD,DELTA=+1");
    else if (!strcmp(key, "LEFT"))   self->onPeunoCmd("$PEUNO,CMD,DELTA=-1");
    else if (!strcmp(key, "CENTER")) self->onPeunoCmd("$PEUNO,CMD,TOGGLE=1");
  }

  // ===== connessione HID + fallback leggero
  void connectToHID(const NimBLEAddress& addr) {
#if defined(SERIAL_DEBUG_BLE)
    Serial.printf("[CONN] Mi connetto a %s\n", addr.toString().c_str());
#endif
    client = NimBLEDevice::createClient();
    client->setConnectionParams(12, 12, 0, 60);  // param "tranquilli"
    client->setConnectTimeout(5);

    if (!client->connect(addr)) {
#if defined(SERIAL_DEBUG_BLE)
      Serial.println("[ERR] Connessione fallita");
#endif
      NimBLEDevice::deleteClient(client); client = nullptr; return;
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
      if (auto pm = hid->getCharacteristic(NimBLEUUID((uint16_t)0x2A4E))) {
        uint8_t mode = 0x01; pm->writeValue(&mode, 1, true);
#if defined(SERIAL_DEBUG_BLE)
        Serial.println("[HID] Protocol Mode = REPORT");
#endif
      }
      yield();

      if (auto hcp = hid->getCharacteristic(NimBLEUUID((uint16_t)0x2A4A))) {
        uint8_t v = 0x00; hcp->writeValue(&v, 1, true);
#if defined(SERIAL_DEBUG_BLE)
        Serial.println("[HID] Control Point = EXIT SUSPEND");
#endif
      }
      yield();

      bool subscribed = false;
      auto chars = hid->getCharacteristics(true);
      for (auto c : *chars) {
        if (c->canNotify() || c->canIndicate()) {
          if (c->subscribe(true, notifyCB)) {
            if (auto cccd = c->getDescriptor(NimBLEUUID((uint16_t)0x2902))) {
              uint8_t val[2] = {0x01, 0x00}; cccd->writeValue(val, 2, true);
            }
#if defined(SERIAL_DEBUG_BLE)
            Serial.printf("[HID] Subscribed → %s\n", c->getUUID().toString().c_str());
#endif
            subscribed = true; connected  = true;
          }
        }
        yield();
      }
      if (subscribed) return;
    } else {
#if defined(SERIAL_DEBUG_BLE)
      Serial.println("[HID] Servizio non trovato → fallback leggero");
#endif
    }

    // Fallback (potenzialmente costoso): una sola passata e rilasci frequenti
    auto svcs = client->getServices(true);
    for (auto s : *svcs) {
      auto cs = s->getCharacteristics(true);
      for (auto c : *cs) {
        if (c->canNotify() || c->canIndicate()) {
          if (c->subscribe(true, notifyCB)) {
            if (auto cccd = c->getDescriptor(NimBLEUUID((uint16_t)0x2902))) {
              uint8_t val[2] = {0x01, 0x00}; cccd->writeValue(val, 2, true);
            }
#if defined(SERIAL_DEBUG_BLE)
            Serial.printf("[FBK] Subscribed → svc=%s char=%s\n",
                          s->getUUID().toString().c_str(),
                          c->getUUID().toString().c_str());
#endif
            connected = true;
          }
        }
        delay(1);
      }
      delay(1);
    }

#if defined(SERIAL_DEBUG_BLE)
    if (!connected) Serial.println("[BLE] Nessuna caratteristica notificabile trovata");
#endif
  }

  // ===== scan HID (callback su ogni ADV) =====
  class MyScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  public:
    explicit MyScanCallbacks(TikTokBLE* parent) : p(parent) {}
    void onResult(NimBLEAdvertisedDevice* dev) override {
      if (p->connected || p->hasPending) return;
      if (dev->isAdvertisingService(NimBLEUUID((uint16_t)0x1812))) {
        p->pendingAddr = dev->getAddress();
        p->hasPending  = true;
        NimBLEDevice::getScan()->stop();     // stop immediato → loop farà la connect
        p->scanning = false;
#if defined(SERIAL_DEBUG_BLE)
        Serial.printf("[SCAN] TikTok Ring: %s (%s)\n",
                      dev->getName().c_str(),
                      dev->getAddress().toString().c_str());
#endif
      }
    }
  private:
    TikTokBLE* p;
  };

  // callback fine scansione (asincrona)
  static void scanCompleteCB(NimBLEScanResults) {
    if (self) self->scanning = false;
#if defined(SERIAL_DEBUG_BLE)
    Serial.println("[BLE] Scansione terminata");
#endif
  }

  static TikTokBLE* self;

public:
  TikTokBLE() { self = this; }
};

TikTokBLE* TikTokBLE::self = nullptr;
