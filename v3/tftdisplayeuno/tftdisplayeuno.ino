/*
  EUNO TFT — WebSocket client verso Autopilot
  - Nessun ESP-NOW
  - TFT + Touch (box 3×2 in alto, menu 3×2 in basso)
  - Telemetria/comandi via WebSocket
  - Si collega all’AP dell’autopilot ("EunoAutopilot"/"password")
*/

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <EEPROM.h>

#include "screen_config.h"

// ===================== OGGETTI GLOBALI =====================
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
WebSocketsClient ws;   // client WebSocket

bool motorControllerState = false;
bool externalBearingEnabled = false;

int menuMode = 0;
int currentParamIndex = 0;
Parameter params[NUM_PARAMS] = {
  {"V_min",     100,   0, 255},
  {"V_max",     150,   0, 255},
  {"E_min",      20,   0, 360},
  {"E_max",      80,   0, 360},
  {"Deadband",   10,   0,  50},
  {"T_risposta",  8,   3,  12},
  {"T_pause",     0,   0,   9},
};

String pendingAction = "";
int pendingButtonType = 0;
ButtonActionState buttonActionState = BAS_IDLE;
unsigned long buttonActionTimestamp = 0;
unsigned long lastTouchTime = 0;

// ===================== CALLBACK WS =====================
void onWsEvent(WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String line((char*)payload, length);
    Serial.println("[TFT] WS RX: " + line);

    if (line.startsWith("$AUTOPILOT,")) {
      // --- heading (compatibilità: HEADING o HDG)
      int heading = kvGetInt(line, "HEADING");
      if (heading < 0) heading = kvGetInt(line, "HDG");
      if (heading >= 0) uiUpdateBox(tft, 0, String(heading));

      // --- command
      int cmd = kvGetInt(line, "COMMAND");
      if (cmd >= 0) {
        uiSetCmdLabel(externalBearingEnabled);
        uiUpdateBox(tft, 1, String(cmd));
        if (externalBearingEnabled) uiUpdateBoxColor(tft, 1, String(cmd), TFT_GREEN);
      }

      // --- error
      int err = kvGetInt(line, "ERROR");
      if (err < 0) err = kvGetInt(line, "ERR"); // compatibilità WS
      if (err != INT_MIN) uiUpdateBox(tft, 2, String(err));

      // --- GPS
      String gpsH = kvGetStr(line, "GPS_HEADING");
      String gpsS = kvGetStr(line, "GPS_SPEED");
      if (gpsH.length()) uiUpdateBox(tft, 3, gpsH);
      if (gpsS.length()) uiUpdateBox(tft, 4, gpsS);

      // --- mode
      String mode = kvGetStr(line, "MODE");
      if (mode.length()) uiApplyHeadingModeLabel(tft, mode);

      // --- motore
      String m = kvGetStr(line, "MOTOR");
      if (m == "ON")  { motorControllerState = true;  uiUpdateMainOnOff(tft, true); }
      if (m == "OFF") { motorControllerState = false; uiUpdateMainOnOff(tft, false); }

      // --- external bearing
      String ext = kvGetStr(line, "EXTBRG");
      if (ext.length()) {
        externalBearingEnabled = (ext == "ON" || ext == "1");
        uiSetCmdLabel(externalBearingEnabled);
      }
    }
    else if (line.startsWith("$PARAM_UPDATE,")) {
      String nv = getFieldCSV(line, 1);
      int eq = nv.indexOf('=');
      if (eq > 0) {
        String name = nv.substring(0, eq);
        int val = nv.substring(eq+1).toInt();
        for (int i=0;i<NUM_PARAMS;i++) {
          if (name == params[i].name) { params[i].value = val; break; }
        }
        uiFlashInfo(tft, name + "=" + String(val));
      }
    }
  }
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(50);
  EEPROM.begin(2048);

  uiBeginDisplay(tft);
  uiBeginTouch(touchscreen);
  uiDrawStatic(tft, motorControllerState, externalBearingEnabled);
  uiDrawMenu(tft, menuMode, params, currentParamIndex, motorControllerState);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin("EunoAutopilot","password");
  Serial.println("[TFT] Connecting to EunoAutopilot...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\n[TFT] Connected, IP=" + WiFi.localIP().toString());

  ws.begin("192.168.4.1", 81, "/");   // WebSocket dell’autopilot
  ws.onEvent(onWsEvent);
  ws.setReconnectInterval(5000);
  Serial.println("[TFT] WebSocket started");
}

// ===================== LOOP =====================
void loop() {
  ws.loop();

  uiCheckTouch(
    tft, touchscreen,
    menuMode, motorControllerState,
    currentParamIndex, params,
    pendingAction, pendingButtonType,
    buttonActionState, buttonActionTimestamp, lastTouchTime
  );

  if (uiConsumeAction(pendingAction)) {
    String cmd;
    if      (pendingAction == "ACTION:-1")     cmd = "$PEUNO,CMD,DELTA=-1";
    else if (pendingAction == "ACTION:+1")     cmd = "$PEUNO,CMD,DELTA=+1";
    else if (pendingAction == "ACTION:-10")    cmd = "$PEUNO,CMD,DELTA=-10";
    else if (pendingAction == "ACTION:+10")    cmd = "$PEUNO,CMD,DELTA=+10";
    else if (pendingAction == "ACTION:TOGGLE") cmd = "$PEUNO,CMD,TOGGLE=1";
    else if (pendingAction == "ACTION:CAL")    cmd = "$PEUNO,CMD,CAL=MAG";
    else if (pendingAction == "ACTION:CAL-GYRO") cmd = "$PEUNO,CMD,CAL=GYRO";
    else if (pendingAction == "ACTION:GPS")    cmd = "$PEUNO,CMD,MODE=FUSION";
    else if (pendingAction == "ACTION:C-GPS")  cmd = "$PEUNO,CMD,CAL=C-GPS";
    else if (pendingAction == "ACTION:EXT_BRG") {
      externalBearingEnabled = !externalBearingEnabled;
      cmd = String("$PEUNO,CMD,EXTBRG=") + (externalBearingEnabled ? "ON":"OFF");
      uiSetCmdLabel(externalBearingEnabled);
      uiUpdateBoxColor(tft, 1, externalBearingEnabled ? "ON" : "OFF",
                       externalBearingEnabled ? TFT_GREEN : TFT_RED);
    }
    else if (pendingAction.startsWith("SET:")) {
      String rest = pendingAction.substring(4);
      cmd = "$PEUNO,CMD,SET," + rest;
    }

    if (cmd.length()) {
      ws.sendTXT(cmd);
      Serial.println("[TFT] Sent: " + cmd);
    }
  }
}
