#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiUdp.h>
#include <Update.h>
#include <functional>
#include <esp_wifi.h>
#include <EEPROM.h>
#include "manifest_json.h"


// ===================== CONFIG RETE =====================
struct EunoNetConfig {
  // STA default (di fabbrica)
  String sta1_ssid = "EUNOAP";
  String sta1_pass = "password";
  // STA da utente (EEPROM, impostate dalla WebApp)
  String sta2_ssid = "";
  String sta2_pass = "";

  // AP sempre attivo
  String ap_ssid  = "EunoAutopilot";
  String ap_pass  = "password";

  // UDP NMEA
  uint16_t udp_in_port  = 10110;
  uint16_t udp_out_port = 10110;
};

enum EunoLinkMode { LINK_STA, LINK_AP }; // AP è sempre ON; LINK_* riflette solo lo stato della STA

// ===================== CLASSE NETWORK =====================
class EunoNetwork {
public:
  EunoNetConfig cfg;
  EunoLinkMode  mode = LINK_AP;

  WebServer         server{80};
  WebSocketsServer  ws{81};
  WiFiUDP           udp;

  IPAddress peerOP;
  IPAddress lastWSIP;
  bool wsReady = false;

  String ipStr;                    // mostra IP corrente: STA se connessa, altrimenti AP
  String mdnsName = "euno-client";
  unsigned long lastHello = 0;

  // EEPROM (semplice layout: [lenSSID][ssid...][lenPASS][pass...])
  static const int   EE_SIZE = 2048;
  static const int   EE_BASE = 512;
  static const uint8_t SSID_MAX = 32;
  static const uint8_t PASS_MAX = 64;

  // --------- INIT ---------
// euno_network.h — SOSTITUISCI TUTTO IL CORPO DI EunoNetwork::begin() CON QUESTO
void begin(){
  Serial.begin(115200);
  delay(50);

  // EEPROM: carica credenziali utente (STA2)
  EEPROM.begin(EE_SIZE);
  String eepSsid, eepPass;
  if (loadOpenPlotterCreds(eepSsid, eepPass)) {
    cfg.sta2_ssid = eepSsid;
    cfg.sta2_pass = eepPass;
    Serial.println("[NET] EEPROM creds: " + cfg.sta2_ssid);
  } else {
    Serial.println("[NET] No EEPROM creds, only defaults");
  }

  // 1) AP SEMPRE ATTIVO (UI sempre raggiungibile)
  beginAP();                              // WIFI_AP_STA
  ipStr = WiFi.softAPIP().toString();     // IP di AP finché STA non sale

  // 2) STA: prova default poi EEPROM (AP resta ON)
  bool staOk = false;
  if (trySTA(cfg.sta1_ssid.c_str(), cfg.sta1_pass.c_str())) staOk = true;
  else if (cfg.sta2_ssid.length() && trySTA(cfg.sta2_ssid.c_str(), cfg.sta2_pass.c_str())) staOk = true;
  mode = staOk ? LINK_STA : LINK_AP;

  // 3) mDNS (una sola volta, qui)
  initMDNS();

  // 4) UDP + HTTP + WS (una sola volta, qui)
  udp.begin(cfg.udp_in_port);
  Serial.printf("[NET] UDP IN @ %u\n", cfg.udp_in_port);

  mountHTTP();
  ws.begin();
  ws.onEvent([this](uint8_t num, WStype_t type, uint8_t * payload, size_t len){
    onWsEvent(num, type, payload, len);
  });

  // Info finale
  Serial.println("[NET] Ready. Mode=" + String(mode==LINK_STA?"STA":"AP") + " IP=" + ipStr);
}


  // --------- LOOP ---------
  void loop(){
    server.handleClient();
    ws.loop();

// Riconnessione STA (non bloccante, niente finestra "kickInFlight")
static unsigned long lastChk  = 0;
static unsigned long lastKick = 0;
static wl_status_t   prevSt   = WL_IDLE_STATUS;

if (millis() - lastChk > 800) {
  lastChk = millis();

  wl_status_t st = WiFi.status();

  // Aggiorna stato (edge)
  if (st != prevSt) {
    prevSt = st;
    if (st == WL_CONNECTED) {
      if (mode != LINK_STA) {
        mode = LINK_STA;
        ipStr = WiFi.localIP().toString();
        initMDNS();
        Serial.println("[NET] STA up @ " + ipStr);
      } else {
        String cur = WiFi.localIP().toString();
        if (ipStr != cur) ipStr = cur;
      }
    } else {
      mode = LINK_AP;                 // AP resta sempre ON
      ipStr = WiFi.softAPIP().toString();
      Serial.println("[NET] STA down → AP view " + ipStr);
    }
  }

 if (st != WL_CONNECTED && !wsReady) {
  if (millis() - lastKick > 60000) {
    WiFi.reconnect();
    lastKick = millis();
    Serial.println("[NET] STA reconnect kick (60s, paused while WS connected)");
  }
}
}



    // Hello periodico su WS (utile per mostrare IP e stato)
    if (millis() - lastHello > 2500 && wsReady) {
      String logMsg = String("LOG:") + "Net=" + (mode==LINK_STA?"STA":"AP") + " IP=" + ipStr;
      ws.broadcastTXT(logMsg); // la lib vuole String non-const
      lastHello = millis();
    }

    // UDP IN
    int p = udp.parsePacket();
    if (p > 0){
      char buf[512];
      int n = udp.read(buf, sizeof(buf) - 1);
      if (n < 0) n = 0;
      buf[n] = 0;
      peerOP = udp.remoteIP();
      onUdpLine(String(buf));
    }
  }

  // --------- INVII ---------
  void sendUDP(const String& line){
    if (peerOP) udp.beginPacket(peerOP, cfg.udp_out_port);
    else        udp.beginPacket(IPAddress(255,255,255,255), cfg.udp_out_port);
    udp.print(line);
    udp.endPacket();
  }

  void sendWS(const String& msg){
    if (!msg.length()) return;
    String tmp = msg;
    ws.broadcastTXT(tmp);
  }

  // --------- CALLBACK ---------
  std::function<void(const String&)> onUdpLine = [](const String&){};
  std::function<void(const String&)> onUiCommand = [](const String&){};

private:
  // ===== AP SEMPRE ATTIVO =====
  void beginAP(){
    // Manteniamo sempre AP+STA
    WiFi.mode(WIFI_AP_STA);

    // Evita power save
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    delay(50);

    // AP config
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                      IPAddress(0, 0, 0, 0),
                      IPAddress(255, 255, 255, 0));
    WiFi.softAP(cfg.ap_ssid.c_str(), cfg.ap_pass.c_str(), 1, 0, 4);

    Serial.println("[NET] AP ON  @ " + WiFi.softAPIP().toString() + " SSID=" + cfg.ap_ssid);
  }

  // ===== STA: CONNESSIONE CON AP ATTIVO =====
bool trySTA(const char* ssid, const char* pass){
 if (!ssid || !ssid[0]) return false;

// Restiamo in AP+STA
WiFi.mode(WIFI_AP_STA);
delay(10);

WiFi.persistent(false);
WiFi.setAutoReconnect(true);
WiFi.setSleep(false);
esp_wifi_set_ps(WIFI_PS_NONE);
esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
WiFi.setTxPower(WIFI_POWER_15dBm);
WiFi.setHostname(mdnsName.c_str());

Serial.printf("[NET] Connecting STA → %s (async)\n", ssid);
WiFi.begin(ssid, pass);

// Ritorna subito: il completamento è gestito nel loop() non-bloccante.
return false;
};

  // ===== mDNS =====
// euno_network.h — SOSTITUISCI TUTTA initMDNS()
void initMDNS() {
  MDNS.end();
  delay(50);

  // Evita begin se non c'è IP né in STA né in AP (caso raro)
  if (WiFi.status() != WL_CONNECTED && WiFi.softAPIP() == IPAddress(0,0,0,0)) {
    Serial.println("[NET] mDNS skipped (no IP yet)");
    return;
  }

  if (MDNS.begin(mdnsName.c_str())) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws",   "tcp", 81);
    Serial.println("[NET] mDNS: http://" + mdnsName + ".local");
  } else {
    Serial.println("[NET] mDNS Error!");
  }
}

  // ===== HTTP/WS =====
  void mountHTTP(){
    // UI principale
    extern const char INDEX_HTML[] PROGMEM;
    server.on("/", HTTP_GET, [this](){
      server.send_P(200, "text/html", INDEX_HTML);
    });

 

    // Manifest PWA
    extern const char MANIFEST_JSON[] PROGMEM;
    server.on("/manifest.json", HTTP_GET, [this](){
      server.send_P(200, "application/json", MANIFEST_JSON);
    });

    // Ping
    server.on("/ping", HTTP_GET, [this](){
      server.send(200, "text/plain", "pong");
    });

    // Salva SSID/PASS (EEPROM) e riavvia per applicare
    server.on("/api/net", HTTP_POST, [this](){
      if (!server.hasArg("ssid") || !server.hasArg("pass")){
        server.send(400, "text/plain", "Missing ssid/pass"); return;
      }
      String s = server.arg("ssid"), p = server.arg("pass");
      saveOpenPlotterCreds(s, p);
      server.send(200, "text/plain", "OK, rebooting STA");
      delay(300);
      ESP.restart();
    });

    // Scan reti Wi-Fi (STA) — async/polling
// GET /api/scan:
//  - 202 {"status":"scanning"}  → in corso
//  - 200 [ {ssid,rssi,enc}, ... ] → pronto
server.on("/api/scan", HTTP_GET, [this](){
  int st = WiFi.scanComplete();
  if (st == WIFI_SCAN_RUNNING) {
    server.send(202, "application/json", "{\"status\":\"scanning\"}");
    return;
  }
  if (st >= 0) {
    String json = "[";
    for (int i = 0; i < st; ++i) {
      if (i) json += ",";
      String ssid = WiFi.SSID(i);
      ssid.replace("\\","\\\\"); ssid.replace("\"","\\\"");
      json += "{\"ssid\":\""+ssid+"\",\"rssi\":"+String(WiFi.RSSI(i))+",\"enc\":"+String((int)WiFi.encryptionType(i))+"}";
    }
    json += "]";
    WiFi.scanDelete();
    server.send(200, "application/json", json);
    return;
  }
  // avvia ora (non blocca)
  WiFi.scanNetworks(true, true);
  server.send(202, "application/json", "{\"status\":\"started\"}");
});

    // 404
    server.onNotFound([this](){
      String path = server.uri();
      Serial.println(String("[HTTP] 404: ")+path);
      server.send(404, "text/plain", "404 " + path);
    });

    server.begin();
    Serial.println("[NET] HTTP server started");
  }

  void onWsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t len){
    if (type == WStype_CONNECTED){
      wsReady  = true;
      lastWSIP = ws.remoteIP(num);
      String hello = "LOG:UI connected";
      ws.sendTXT(num, hello);
      return;
    }
    if (type == WStype_TEXT){
      String s((char*)payload, len);
      onUiCommand(s);
    }
  }

  // ===== EEPROM: salva/carica credenziali STA utente =====
      public:

  void saveOpenPlotterCreds(const String& ssid, const String& pass){
    String S = ssid; if (S.length() > SSID_MAX) S.remove(SSID_MAX);
    String P = pass; if (P.length() > PASS_MAX) P.remove(PASS_MAX);

    int pos = EE_BASE;
    
    EEPROM.write(pos++, (uint8_t)S.length());
    for (size_t i=0; i<S.length(); ++i) EEPROM.write(pos++, S[i]);

    EEPROM.write(pos++, (uint8_t)P.length());
    for (size_t i=0; i<P.length(); ++i) EEPROM.write(pos++, P[i]);

    EEPROM.commit();
    Serial.println("[NET] STA creds saved to EEPROM");
  }

  bool loadOpenPlotterCreds(String& ssidOut, String& passOut){
    int pos = EE_BASE;
    uint8_t l1 = EEPROM.read(pos++);
    if (l1 == 0xFF || l1 == 0 || l1 > SSID_MAX) return false;
    char s1[SSID_MAX+1];
    for (uint8_t i=0;i<l1;i++) s1[i] = EEPROM.read(pos++);
    s1[l1] = 0;

    uint8_t l2 = EEPROM.read(pos++);
    if (l2 == 0xFF || l2 > PASS_MAX) return false;
    char s2[PASS_MAX+1];
    for (uint8_t i=0;i<l2;i++) s2[i] = EEPROM.read(pos++);
    s2[l2] = 0;

    ssidOut = String(s1);
    passOut = String(s2);
    return ssidOut.length()>0;
  }
};

// ============================================================================
// Helper opzionale: carica in cfg.sta2 le credenziali EEPROM
// ============================================================================
inline bool EUNO_LOAD_OP_CREDS(EunoNetwork& net){
  String s, p;
  if (net.loadOpenPlotterCreds(s, p)) {
    net.cfg.sta2_ssid = s;
    net.cfg.sta2_pass = p;
    Serial.println("[NET] Loaded STA2 from EEPROM: " + s);
    return true;
  }
  Serial.println("[NET] No STA2 in EEPROM");
  return false;
}
