#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <EEPROM.h>
#include <functional>
#include <string.h>

/*
  EunoEspNow — CLIENT (Autopilot) — ESP-IDF v5 compat
  - Nessuna gestione WiFi qui dentro: la fa EunoNetwork (AP/STA).
  - Auto-pairing: al primo pacchetto ricevuto salva il MAC del TFT in EEPROM.
  - Dopo il primo pairing: unicast fisso e link univoco.
  - Callback: onLine(String) per consegnare le righe testuali ($AUTOPILOT..., $PEUNO,CMD,...).
  - Espone startAutoPairing(tag) e addBroadcastPeer() per compatibilità con il tuo .ino.
*/

class EunoEspNow {
public:
  using LineCb = std::function<void(const String&)>;

  LineCb  onLine = nullptr;

  bool    paired     = false;      // true se abbiamo un peer salvato/valido
  bool    hasDisplay = false;      // true se il TFT ha parlato almeno una volta
  uint8_t peerMac[6] = {0};
  const uint8_t BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

  // EEPROM: slot sicuro (non collide con 0..7, 100.., 512..)
  static constexpr int   EE_BASE   = 720;  // 720..726 = 7 byte
  static constexpr uint8_t MAGIC   = 0xE1; // magic del CLIENT

  // Singleton ptr per i thunk
  static EunoEspNow* instance;

  // ---- API pubblica ---------------------------------------------------

  // Avvio ESP-NOW — NON tocca WiFi.mode()
  bool begin() {
    if (esp_now_init() != ESP_OK) {
      Serial.println("[ENOW] init FAIL");
      return false;
    }
    esp_now_register_recv_cb(&EunoEspNow::onRecvThunk);   // firma IDF v5
    esp_now_register_send_cb(&EunoEspNow::onSendThunk);
    instance = this;

    if (loadPeerFromEEPROM()) {
      // abbiamo già un peer salvato → aggiungi peer unicast
      if (_addPeer(peerMac)) {
        paired = true;
        hasDisplay = true; // era già accoppiato in passato
        Serial.printf("[ENOW] paired from EEPROM: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      peerMac[0],peerMac[1],peerMac[2],peerMac[3],peerMac[4],peerMac[5]);
      } else {
        // fallback broadcast se addPeer fallisse
        _addPeer(BCAST);
        paired = false;
        hasDisplay = false;
        Serial.println("[ENOW] addPeer(unicast) failed, fallback to broadcast");
      }
    } else {
      // primo giro: resta in broadcast finché non ricevi qualcosa
      _addPeer(BCAST);
      paired = false;
      hasDisplay = false;
      Serial.println("[ENOW] waiting first packet for auto-pairing (broadcast ON)");
    }
    return true;
  }

  // Compatibilità: nel CLIENT non serve fare nulla,
  // ma lasciamo il metodo perché il tuo .ino lo chiama.
  void startAutoPairing(const char* tag = "EUNO") {
    (void)tag; // no-op lato client: il pairing avviene alla prima ricezione
  }

  // Per compatibilità: se nel .ino vuoi forzare il broadcast peer
  // (es. test iniziale), usa questo wrapper pubblico.
  bool addBroadcastPeer() {
    return _addPeer(BCAST);
  }

  // Coerenza con il tuo loop (no-op)
  void loop() {}

  // Invia una riga testuale
  void sendLine(const String& s) {
    const uint8_t* mac = paired ? peerMac : BCAST;
    esp_now_send(mac, (const uint8_t*)s.c_str(), s.length());
  }

  // (Opzionale) per resettare l'accoppiamento dalla tua UI
  void clearPairing() {
    EEPROM.write(EE_BASE, 0xFF);
    for (int i=0;i<6;i++) EEPROM.write(EE_BASE+1+i, 0x00);
    EEPROM.commit();
    paired = false;
    hasDisplay = false;
    memset(peerMac, 0, 6);
    // riparti in broadcast
    _addPeer(BCAST);
    Serial.println("[ENOW] pairing cleared, broadcast enabled");
  }

private:
  // ---- helpers privati -------------------------------------------------

  // Aggiunge/aggiorna peer su canale corrente (0) — compatibile con AP/STA dinamici
  bool _addPeer(const uint8_t mac[6]) {
    esp_now_peer_info_t p{};
    memcpy(p.peer_addr, mac, 6);
    p.channel = 0;       // *** IMPORTANTE: 0 = canale corrente ***
    p.encrypt = false;
    esp_now_del_peer(mac); // idempotente
    return (esp_now_add_peer(&p) == ESP_OK);
  }

  // EEPROM: salva/carica MAC
  void savePeerToEEPROM(const uint8_t mac[6]) {
    EEPROM.write(EE_BASE, MAGIC);
    for (int i=0;i<6;i++) EEPROM.write(EE_BASE+1+i, mac[i]);
    EEPROM.commit();
  }
  bool loadPeerFromEEPROM() {
    if (EEPROM.read(EE_BASE) != MAGIC) return false;
    for (int i=0;i<6;i++) peerMac[i] = EEPROM.read(EE_BASE+1+i);
    // rifiuta 00:00:00:00:00:00 e FF:FF:... per sicurezza
    bool all0=true, allF=true;
    for (int i=0;i<6;i++){ if (peerMac[i]!=0x00) all0=false; if (peerMac[i]!=0xFF) allF=false; }
    return !(all0 || allF);
  }

  // ==== CALLBACKS (IDF v5) ====
  static void onRecvThunk(const esp_now_recv_info* info, const uint8_t* data, int len) {
    if (!instance) return;

    // 1) Auto-pairing: alla PRIMA ricezione memorizza il MAC sorgente
    if (!instance->paired) {
      // ignora broadcast come peer
      const uint8_t* src = info->src_addr;
      bool isBroadcast = true;
      for (int i=0;i<6;i++) if (src[i] != 0xFF) { isBroadcast = false; break; }

      if (!isBroadcast) {
        memcpy(instance->peerMac, src, 6);
        if (instance->_addPeer(src)) {
          instance->savePeerToEEPROM(src);
          instance->paired = true;
          instance->hasDisplay = true;
          Serial.printf("[ENOW] AUTO-PAIRED with: %02X:%02X:%02X:%02X:%02X:%02X\n",
                        src[0],src[1],src[2],src[3],src[4],src[5]);
        } else {
          Serial.println("[ENOW] addPeer(unicast) failed at first packet");
        }
      }
    }

    // 2) Parsing sicuro: accetta solo payload testuale “pulito”
    //    - se pacchetto è raw string (inizia con '$' o 'E' o 'P'), lo passiamo
    //    - se qualcuno invia una struct fissa, scartiamo (evita String da binario)
    if (len <= 0) return;

    const char c0 = (char)data[0];
    bool likelyText = (c0 == '$' || c0 == 'E' || c0 == 'P'); // $AUTOPILOT / EUNO_* / $PEUNO,..
    if (!likelyText) {
      bool zeroTerm = (data[len-1] == 0);
      if (!zeroTerm) return; // evita costruire stringhe da binario
    }

    String line((const char*)data, len);
    if (line.length() && (line.endsWith("\r") || line.endsWith("\n"))) line.trim();

    if (instance->onLine) instance->onLine(line);
  }

  static void onSendThunk(const uint8_t* mac_addr, esp_now_send_status_t status) {
    (void)mac_addr; (void)status;
  }
};

// Definizione del puntatore statico
inline EunoEspNow* EunoEspNow::instance = nullptr;
