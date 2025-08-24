#pragma once
#include <esp_now.h>
#include <WiFi.h>

// Trasportiamo solo testo (frasi NMEA / $PEUNO,CMD,...) con ack semplice
struct ESPNOWMsg {
  uint16_t id;         // msg id
  uint8_t  len;        // len data
  char     data[200];  // riga
} __attribute__((packed));

class EunoEspNow {
public:
  bool paired = false;
  uint8_t peerMac[6] = {0};  // MAC del display (da salvare via UI in futuro)
  uint16_t nextId = 1;
  unsigned long lastAck = 0;
  bool hasDisplay = false;

  std::function<void(const String&)> onLine = [](const String&){};
void loop(){}  // no-op per compatibilità con il tuo loop()

  bool begin(){
    WiFi.mode(WIFI_AP_STA); // compatibile con net
    if (esp_now_init()!=ESP_OK){ Serial.println("[ENOW] init fail"); return false; }
esp_now_register_recv_cb(&EunoEspNow::onRecvThunk);    esp_now_register_send_cb(&EunoEspNow::onSendThunk);
    instance = this;
    return true;
  }

  bool addPeer(const uint8_t mac[6]){
    memcpy(peerMac, mac, 6);
    esp_now_peer_info_t p = {};
    memcpy(p.peer_addr, mac, 6);
    p.channel = 0; p.encrypt = false;
    if (esp_now_add_peer(&p)==ESP_OK){ paired = true; return true; }
    return false;
  }

  // invia una riga (telemetria o comando)
  void sendLine(const String& s){
    if (!paired) return;
    ESPNOWMsg m{};
    m.id = nextId++;
    m.len = (uint8_t)min( (int)s.length(), (int)sizeof(m.data)-1 );
    memcpy(m.data, s.c_str(), m.len);
    esp_now_send(peerMac, (uint8_t*)&m, sizeof(ESPNOWMsg));
  }

private:
  static EunoEspNow* instance;
 static void onRecvThunk(const esp_now_recv_info* info, const uint8_t* data, int len){
  if (!instance) return;
  if (len==(int)sizeof(ESPNOWMsg)){
    auto* m = (const ESPNOWMsg*)data;
    ((EunoEspNow*)instance)->hasDisplay = true;
    // Se ti serve il MAC sorgente: info->src_addr (6 byte)
    ((EunoEspNow*)instance)->onLine(String(m->data, m->len));
  }
}

  static void onSendThunk(const uint8_t* mac, esp_now_send_status_t status){
    if (!instance) return;
    if (status==ESP_NOW_SEND_SUCCESS) instance->lastAck = millis();
  }
};
inline EunoEspNow* EunoEspNow::instance = nullptr;
