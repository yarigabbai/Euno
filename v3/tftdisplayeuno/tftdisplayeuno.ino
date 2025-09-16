/*
  EUNO TFT — solo UDP
  - TFT + Touch (box 3×2 in alto, menu 3×2 in basso)
  - Telemetria via UDP ($AUTOPILOT,...)
  - Comandi via UDP ($PEUNO,CMD,...)
*/

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <EEPROM.h>
#include <WiFiUdp.h>

#include "screen_config.h"

// ===================== OGGETTI GLOBALI =====================
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
WiFiUDP udp;

IPAddress autopilotIP(192,168,4,1);   // IP dell’autopilota in modalità AP
const int autopilotPort = 10110;      // porta NMEA UDP

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

  udp.begin(10110); // avvia UDP locale
  Serial.println("[TFT] UDP started on port 10110");
}

// ===================== LOOP =====================
void loop() {
  // 🔎 Debug touch raw
  if (touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    Serial.printf("[TOUCH RAW] x=%d y=%d z=%d\n", p.x, p.y, p.z);
  }

  // Controllo tocco con UI
  uiCheckTouch(
    tft, touchscreen,
    menuMode, motorControllerState,
    currentParamIndex, params,
    pendingAction, pendingButtonType,
    buttonActionState, buttonActionTimestamp, lastTouchTime
  );
// Controllo azioni da touch
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
    udp.beginPacket(autopilotIP, autopilotPort);
    udp.print(cmd);
    udp.endPacket();
    Serial.println("[TFT] Sent UDP: " + cmd);
  }
}

  // Se è stata generata un’azione → traducila in NMEA e invia via UDP
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
      udp.beginPacket(autopilotIP, autopilotPort);
      udp.print(cmd);
      udp.endPacket();
      Serial.println("[TFT] Sent UDP: " + cmd);
    }
  }

  // Ricezione telemetria via UDP
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char buf[512];
    int len = udp.read(buf, sizeof(buf)-1);
    if (len > 0) buf[len] = 0;
    String line(buf);
    Serial.println("[TFT] RX UDP: " + line);

    if (line.startsWith("$AUTOPILOT,")) {
      int heading = kvGetInt(line, "HEADING");
      if (heading < 0) heading = kvGetInt(line, "HDG");
      if (heading >= 0) uiUpdateBox(tft, 0, String(heading));

      int cmd = kvGetInt(line, "COMMAND");
      if (cmd >= 0) uiUpdateBox(tft, 1, String(cmd));

      int err = kvGetInt(line, "ERROR");
      if (err != INT_MIN) uiUpdateBox(tft, 2, String(err));

      String gpsH = kvGetStr(line, "GPS_HEADING");
      String gpsS = kvGetStr(line, "GPS_SPEED");
      if (gpsH.length()) uiUpdateBox(tft, 3, gpsH);
      if (gpsS.length()) uiUpdateBox(tft, 4, gpsS);

      String mode = kvGetStr(line, "MODE");
      if (mode.length()) uiApplyHeadingModeLabel(tft, mode);

      String m = kvGetStr(line, "MOTOR");
      if (m == "ON")  { motorControllerState = true;  uiUpdateMainOnOff(tft, true); }
      if (m == "OFF") { motorControllerState = false; uiUpdateMainOnOff(tft, false); }
    }
  }
}
