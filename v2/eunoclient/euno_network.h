#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiUdp.h>
#include <Update.h>
#include <functional>

// ===================== CONFIG RETE =====================
struct EunoNetConfig {
  // Priorità STA: 1) EUNOAP/password  2) credenziali OpenPlotter da EEPROM
  String sta1_ssid = "EUNOAP";
  String sta1_pass = "password";
  String sta2_ssid = "";      // riempita da UI -> EEPROM
  String sta2_pass = "";      // riempita da UI -> EEPROM

  String ap_ssid  = "EunoAutopilot";
  String ap_pass  = "password";

  uint16_t udp_in_port  = 10110; // NMEA IN (OP/PC -> client)
  uint16_t udp_out_port = 10110; // NMEA OUT (client -> OP/PC)
};

enum EunoLinkMode { LINK_STA, LINK_AP };

// ===================== CLASSE NETWORK =====================
class EunoNetwork {
public:
  EunoNetConfig cfg;
  EunoLinkMode  mode = LINK_AP;

  WebServer         server{80};
  WebSocketsServer  ws{81};
  WiFiUDP           udp;

  IPAddress peerOP;       // IP OP/PC per OUT (riempito quando riceviamo)
  IPAddress lastWSIP;     // IP ultimo web client connesso (UI)
  bool wsReady = false;

  // stato
  String ipStr;
  String mdnsName = "euno-client";
  unsigned long lastHello = 0;

  // --------- INIT ---------
  void begin(){
    Serial.begin(115200);
    delay(50);

    // tenta STA 1 -> STA 2 -> fallback AP
    if (trySTA(cfg.sta1_ssid.c_str(), cfg.sta1_pass.c_str())) {
      mode = LINK_STA;
    } else if (cfg.sta2_ssid.length() && trySTA(cfg.sta2_ssid.c_str(), cfg.sta2_pass.c_str())) {
      mode = LINK_STA;
    } else {
      beginAP();
    }

    // mDNS
    if (MDNS.begin(mdnsName.c_str())) {
      Serial.println("[NET] mDNS: http://" + mdnsName + ".local");
    }

    // UDP
    udp.begin(cfg.udp_in_port);
    Serial.printf("[NET] UDP IN @ %u\n", cfg.udp_in_port);

    // HTTP + WS
    mountHTTP();
    ws.begin();
    ws.onEvent([this](uint8_t num, WStype_t type, uint8_t * payload, size_t len){
      onWsEvent(num, type, payload, len);
    });

    Serial.println("[NET] Ready. Mode=" + String(mode==LINK_STA?"STA":"AP") + " IP=" + ipStr);
  }

  // --------- LOOP ---------
  void loop(){
    server.handleClient();
    ws.loop();

    // Hello periodico verso console web
    if (millis() - lastHello > 1000 && wsReady) {
      ws.broadcastTXT(String("LOG:") + "Net=" + (mode==LINK_STA?"STA":"AP") + " IP=" + ipStr);
      lastHello = millis();
    }

    // UDP IN (NMEA / PEUNO)
    int p = udp.parsePacket();
    if (p > 0){
      char buf[512];
      int n = udp.read(buf, sizeof(buf) - 1);
      if (n < 0) n = 0;
      buf[n] = 0;
      peerOP = udp.remoteIP(); // memorizza chi ci parla
      onUdpLine(String(buf));
    }
  }

  // --------- INVII ---------
  void sendUDP(const String& line){           // verso OP/PC
    if (peerOP) udp.beginPacket(peerOP, cfg.udp_out_port);
    else        udp.beginPacket(IPAddress(255,255,255,255), cfg.udp_out_port);
    udp.print(line);
    udp.endPacket();
  }

  // Fix compatibilità: la tua lib WebSocketsServer espone broadcastTXT(String&) (non-const);
  // usiamo l’overload con buffer per accettare const String& in sicurezza.
  void sendWS(const String& msg){
    if (wsReady) ws.broadcastTXT((const uint8_t*)msg.c_str(), msg.length());
  }

  // --------- CALLBACK da collegare dal tuo .ino ---------
  // 1) chiamata quando arriva una riga NMEA/PEUNO via UDP
  std::function<void(const String&)> onUdpLine = [](const String&){};
  // 2) chiamata quando arriva un comando dalla UI WebSocket
  std::function<void(const String&)> onUiCommand = [](const String&){};
  // 3) per fornire telemetria alla UI (chiama sendWS(...) dal tuo loop)

private:
  bool trySTA(const char* ssid, const char* pass){
    if (!ssid || !ssid[0]) return false;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    Serial.printf("[NET] Try STA → %s ...\n", ssid);
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000){
      delay(200);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED){
      ipStr = WiFi.localIP().toString();
      Serial.println("\n[NET] STA OK @ " + ipStr);
      return true;
    }
    Serial.println("\n[NET] STA FAIL");
    return false;
  }

  void beginAP(){
    WiFi.mode(WIFI_AP);
    WiFi.softAP(cfg.ap_ssid.c_str(), cfg.ap_pass.c_str());
    mode  = LINK_AP;
    ipStr = WiFi.softAPIP().toString();
    Serial.println("[NET] AP @ " + ipStr + " SSID=" + cfg.ap_ssid);
  }

  void mountHTTP(){
  // HTML in chiaro dal tuo index_html.h
  extern const char INDEX_HTML[] PROGMEM;

  // Root: UI
  server.on("/", HTTP_GET, [this](){
    server.send_P(200, "text/html", INDEX_HTML);
  });

  // Ping diagnostico (per capire se il server risponde)
  server.on("/ping", HTTP_GET, [this](){
    server.send(200, "text/plain", "pong");
  });

  // API Web: salvataggio credenziali OP
  server.on("/api/net", HTTP_POST, [this](){
    if (!server.hasArg("ssid") || !server.hasArg("pass")){
      server.send(400, "text/plain", "Missing ssid/pass"); return;
    }
    String s = server.arg("ssid"), p = server.arg("pass");
    saveOpenPlotterCreds(s, p);
    server.send(200, "text/plain", "OK, riavvio in STA");
    delay(300);
    ESP.restart();
  });

  // 404: logga cosa chiede il browser (utile se va su https o su /qualcosa)
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
      ws.sendTXT(num, "LOG:UI connected");
      return;
    }
    if (type == WStype_TEXT){
      String s((char*)payload, len);
      // accettiamo direttamente frasi $PEUNO,CMD,... dalla UI
      onUiCommand(s);
    }
  }

  // EEPROM: salvataggio credenziali OpenPlotter (implementata nel .cpp)
  void saveOpenPlotterCreds(const String& ssid, const String& pass);
};

// ============= Dichiarazione globale helper (implementata nel .cpp) =============
bool EUNO_LOAD_OP_CREDS(EunoNetwork& net);
