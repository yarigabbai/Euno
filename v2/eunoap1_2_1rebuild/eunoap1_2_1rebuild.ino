/**********************************************************
 * CODICE AP COMPLETO - VERSIONE DEFINITIVA
 * 1 fatto, 2 fatto, 3 fatto, 6 fatto

 **********************************************************/

// Includi prima FS con namespace esplicito
#include "FS.h"
using fs::FS; 

// Poi le altre librerie standard
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>
#include <string.h>
#include <Update.h>

// Modifica l'include di WebServer per forzare il namespace corretto

#include <WebServer.h>
#include <WebSocketsServer.h>

// Include dei file locali
#include "tft_touch.h"
#include "nmea_parser.h"
#include "websocket_ota.h"
#include "index_html.h"

#define EUNO_IS_AP
#include "euno_debugAP.h"


// Configurazione rete
const char* ssid = "EUNO AP";
const char* password = "password";
IPAddress apIP(192,168,4,1);

// Comunicazione UDP
WiFiUDP udp;
unsigned int localUdpPort = 4210;
char incomingPacket[255];
IPAddress clientIP(192,168,4,2);
unsigned int clientPort = 4210;

// Display e touch
TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// Variabili di stato
bool motorControllerState = false;
bool externalBearingEnabled = false;
int lastValidExtBearing = -1;
unsigned long lastExtBearingTime = 0;
const unsigned long EXT_BRG_TIMEOUT = 5000;
#define NUM_PARAMS 8
// Menu e parametri
int menuMode = 0;
int currentParamIndex = 0;
Parameter params[NUM_PARAMS] = {
  { "V_min", 100, 0, 255 },
  { "V_max", 255, 0, 255 },
  { "E_min", 5, 0, 100 },
  { "E_max", 40, 0, 100 },
  { "E_tolleranza", 1, 0, 10 },
  { "T.S.min", 4, 0, 20 },
  { "T_max", 10, 0, 20 },
  { "T_pause", 0, 0, 9 }  // Valore da 0 a 9 (0–900 ms)

};

ButtonActionState buttonActionState = BAS_IDLE;
unsigned long buttonActionTimestamp = 0;
String pendingAction = "";
int pendingButtonType = 0;
unsigned long lastTouchTime = 0;

// Server web e WebSocket
WebServer server(80);
WebSocketsServer webSocket(81);

void setup() {
  Serial.begin(115200);
  
  // Inizializzazione display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  // Inizializzazione touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  if(!touchscreen.begin(touchscreenSPI)) {
    Serial.println("Touchscreen init ERROR!");
  } else {
    Serial.println("Touchscreen init SUCCESS.");
  }
  touchscreen.setRotation(1);

  // Configurazione Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255,255,255,0));
  
  // Avvio servizi
  MDNS.begin("euno");
  udp.begin(localUdpPort);
  initWebServers(server, webSocket);

  // Stampa info connessione
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.printf("UDP server on port %d\n", localUdpPort);

  // Disegno iniziale
 drawStaticLayout(tft, motorControllerState, externalBearingEnabled);
  drawMenu(tft, menuMode, params, currentParamIndex, motorControllerState);
}

void loop() {
  // Gestione touch
  checkTouch(
    tft, touchscreen,
    menuMode, motorControllerState,
    currentParamIndex, params,
    pendingAction, pendingButtonType,
    buttonActionState, buttonActionTimestamp,
    lastTouchTime
  );

  // Gestione bearing esterno
  if(externalBearingEnabled) {
    unsigned long now = millis();
    if(now - lastExtBearingTime > EXT_BRG_TIMEOUT) {
    updateDataBoxColor(tft, 1, "NP", TFT_RED);
    } else if(lastValidExtBearing != -1) {
      String cmdData = "CMD:" + String(lastValidExtBearing);
      udp.beginPacket(clientIP, clientPort);
      udp.print(cmdData);
      udp.endPacket();
    }
  }

  // Ricezione UDP
  int packetSize = udp.parsePacket();
  if(packetSize) {
    clientIP = udp.remoteIP();
    clientPort = udp.remotePort();
    int len = udp.read(incomingPacket, 255);
    if(len > 0) {
      incomingPacket[len] = 0;
      String msg = String(incomingPacket);
      if (msg.startsWith("LOG:")) {
  Serial.println("AP ha ricevuto log dal client:");
  Serial.println(msg);
}

      parseNMEA(
        msg,
        tft, motorControllerState,
        externalBearingEnabled, lastValidExtBearing, lastExtBearingTime
      );
      webSocket.broadcastTXT(msg);
    }
  }

  // Gestione server
  webSocket.loop();
  server.handleClient();
}