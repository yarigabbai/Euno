#include "nmea_parser.h"
#include <math.h> // per round()
#include "tft_touch.h" // per updateDataBox e updateDataBoxColor
#include "websocket_ota.h"

#define EUNO_IS_AP
#include "euno_debugAP.h"



extern TFT_eSPI tft;
extern int menuMode;
extern int currentParamIndex;
extern Parameter params[];
extern bool motorControllerState;

// Queste variabili arrivano da ap_main.ino
extern bool externalBearingEnabled;
extern IPAddress clientIP;
extern unsigned int clientPort;
extern WiFiUDP udp;
extern int lastValidExtBearing;
extern unsigned long lastExtBearingTime;
extern WebSocketsServer webSocket;

void parseNMEA(String nmea, TFT_eSPI &tft,
               bool &motorControllerState,
               bool &externalBearingEnabled,
               int &lastValidExtBearing,
               unsigned long &lastExtBearingTime) {

  Serial.println("DEBUG: parseNMEA() chiamata con: " + nmea);

  if (nmea.startsWith("$AUTOPILOT,")) {
    int heading = getValue(nmea, "HEADING");
    int command = getValue(nmea, "COMMAND");
    int error   = getValue(nmea, "ERROR");
    String gpsHeading = getStringValue(nmea, "GPS_HEADING");
    String gpsSpeed   = getStringValue(nmea, "GPS_SPEED");

    updateDataBox(tft, 0, String(heading));
    if (!externalBearingEnabled) updateDataBox(tft, 1, String(command));
    updateDataBox(tft, 2, String(error));
    updateDataBox(tft, 3, gpsHeading);
    updateDataBox(tft, 4, gpsSpeed);
    updateDataBox(tft, 5, "");
  }

  else if (nmea.startsWith("$GPRMB")) {
    if (externalBearingEnabled) {
      Serial.println("DEBUG(Display): Modalità bearing esterno attiva. Elaboro $GPRMB.");

      String bearingStr = getFieldNMEA(nmea, 11);
      int starPos = bearingStr.indexOf('*');
      if (starPos != -1) bearingStr = bearingStr.substring(0, starPos);

      Serial.println("DEBUG(Display): Bearing estratto da $GPRMB: " + bearingStr);
      int roundedBearing = round(bearingStr.toFloat());
      Serial.printf("DEBUG(Display): Bearing arrotondato: %d\n", roundedBearing);

      if (roundedBearing >= 0 && roundedBearing < 360) {
        lastValidExtBearing = roundedBearing;
        lastExtBearingTime = millis();

        // Valore valido: verde
        updateDataBoxColor(tft, 1, String(lastValidExtBearing), TFT_GREEN);

        String cmdData = "CMD:" + String(lastValidExtBearing);
        udp.beginPacket(clientIP, clientPort);
        udp.print(cmdData);
        udp.endPacket();
        Serial.println("DEBUG(Display): Inviato comando -> " + cmdData);
      } else {
        Serial.println("DEBUG(Display): Bearing non valido: " + String(roundedBearing));
      }
    }
  }

  else if (nmea.startsWith("$OTA_STATUS,")) {
    String status = getStringValue(nmea, "STATUS");
    int progress = getValue(nmea, "PROGRESS");

    if (status == "START") {
      updateDataBox(tft, 5, "OTA START");
    }
    else if (status.startsWith("FAILED")) {
      updateDataBox(tft, 5, "OTA FAILED");
    }
    else if (status == "IN_PROGRESS") {
      updateDataBox(tft, 5, "OTA " + String(progress) + "%");
    }
    else if (status == "COMPLETED") {
      updateDataBox(tft, 5, "OTA DONE");
    }
  }

  else if (nmea.startsWith("$PARAM_UPDATE,")) {
    String fullField = getFieldNMEA(nmea, 1);
    String param = fullField.substring(0, fullField.indexOf('='));
    String value = fullField.substring(fullField.indexOf('=') + 1);
    updateDataBox(tft, 5, param + "=" + value);
    delay(2000);
    updateDataBox(tft, 5, motorControllerState ? "ON" : "OFF");
  }
}

void handleCommandAP(String command) {
  if (command.startsWith("SET:")) {
    int eqPos = command.indexOf('=');
    if (eqPos > 4) {
      String param = command.substring(4, eqPos);
      String val = command.substring(eqPos + 1);
      int ival = val.toInt();

      for (int i = 0; i < NUM_PARAMS; i++) {
        if (param == params[i].name) {
          params[i].value = ival;
          break;
        }
      }

      // WebSocket + UDP
      String msg = "PARAM_UPDATED:" + param + "=" + val;
      webSocket.broadcastTXT(msg);
      udp.beginPacket(clientIP, clientPort);
      udp.print(command);
      udp.endPacket();
    }
  } else {
    udp.beginPacket(clientIP, clientPort);
    udp.print(command);
    udp.endPacket();
  }
}

int getValue(String nmea, String field) {
  int start = nmea.indexOf(field + "=");
  if (start == -1) return -1;
  int end = nmea.indexOf(",", start);
  if (end == -1) end = nmea.length();
  return nmea.substring(start + field.length() + 1, end).toInt();
}

String getStringValue(String nmea, String field) {
  int start = nmea.indexOf(field + "=");
  if (start == -1) return "N/A";
  int end = nmea.indexOf(",", start);
  if (end == -1) end = nmea.length();
  return nmea.substring(start + field.length() + 1, end);
}

String getFieldNMEA(String nmea, int index) {
  int commaCount = 0, startPos = 0;
  for (int i = 0; i < nmea.length(); i++) {
    if (nmea[i] == ',') {
      if (commaCount == index) return nmea.substring(startPos, i);
      commaCount++;
      startPos = i + 1;
    }
  }
  return nmea.substring(startPos);
}
