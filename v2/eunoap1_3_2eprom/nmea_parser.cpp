#include "nmea_parser.h"
#include <math.h> // per round()
#include "tft_touch.h"       // per updateDataBox e updateDataBoxColor
#include "websocket_ota.h"   // se serve
#define EUNO_IS_AP
#include "euno_debugAP.h"


// Se usi "headingSourceLabel" per la label di heading
String headingSourceLabel = "Heading";  

// Variabili globali esterne
extern TFT_eSPI tft;
extern int menuMode;
extern int currentParamIndex;
extern Parameter params[];     
extern bool motorControllerState;
extern int V_min, V_max, E_min, E_max, E_tol, T_min, T_max;
extern void saveParamsToEEPROM();

// Variabili da ap_main.ino
extern bool externalBearingEnabled;
extern IPAddress clientIP;
extern unsigned int clientPort;
extern WiFiUDP udp;
extern int lastValidExtBearing;
extern unsigned long lastExtBearingTime;
extern WebSocketsServer webSocket;
int headingSourceIndex = 0; // 0 = Compass, 1 = Fusion, 2 = Experimental
String headingLabelText = "H.Compass"; // testo da mostrare nel box heading


// ===============================
// parseNMEA: elabora i messaggi in arrivo
// ===============================
void parseNMEA(String nmea, 
               TFT_eSPI &tft,
               bool &motorControllerState,
               bool &externalBearingEnabled,
               int &lastValidExtBearing,
               unsigned long &lastExtBearingTime)
{
  Serial.println("DEBUG: parseNMEA() chiamata con: " + nmea);

  if (nmea.startsWith("$AUTOPILOT,")) {
    // =======================
    // Esempio: $AUTOPILOT,HEADING=123,COMMAND=140,ERROR=17,GPS_HEADING=...,GPS_SPEED=...
    // =======================
    int heading = getValue(nmea, "HEADING");
    int command = getValue(nmea, "COMMAND");
    int error   = getValue(nmea, "ERROR");
    String gpsHeading = getStringValue(nmea, "GPS_HEADING");
    String gpsSpeed   = getStringValue(nmea, "GPS_SPEED");

    // Se vuoi che la label “Heading” rifletta headingSourceLabel


updateDataBox(tft, 0, String(heading));



    // Aggiorna label e valore “Cmd”
    // Se EXT BRG è abilitato, la label diventa “CMD BRG”
    infoLabels[1] = externalBearingEnabled ? "CMD BRG" : "Cmd";
    updateDataBox(tft, 1, String(command));

    // Se EXTBRG attivo, colora riquadro Cmd in verde
    if (externalBearingEnabled) {
      updateDataBoxColor(tft, 1, String(command), 0x07E0 /*TFT_GREEN*/);
    }

    // Errore
    updateDataBox(tft, 2, String(error));
    // GPS heading
    updateDataBox(tft, 3, gpsHeading);
    // GPS speed
    updateDataBox(tft, 4, gpsSpeed);
    // Box 5 vuoto
    //updateDataBox(tft, 5, "");
  }
  else if (nmea.startsWith("$GPRMB")) {
    // =======================
    // Elaborazione bearing esterno (da un messaggio $GPRMB)
    // =======================
    if (externalBearingEnabled) {
      Serial.println("DEBUG(Display): Modalità bearing esterno attiva. Elaboro $GPRMB.");

      String bearingStr = getFieldNMEA(nmea, 11);
      int starPos = bearingStr.indexOf('*');
      if (starPos != -1) bearingStr = bearingStr.substring(0, starPos);

      int roundedBearing = round(bearingStr.toFloat());
      Serial.printf("DEBUG(Display): Bearing arrotondato: %d\n", roundedBearing);

      if (roundedBearing >= 0 && roundedBearing < 360) {
        lastValidExtBearing = roundedBearing;
        lastExtBearingTime = millis();

        // Colora in verde la label CMD
        updateDataBoxColor(tft, 1, String(lastValidExtBearing), 0x07E0 /*TFT_GREEN*/);

        // Invia al client: "CMD:xxx"
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
    // =======================
    // Aggiornamento OTA
    // =======================
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

 else if (nmea.startsWith("MOTOR:")) {
  if (nmea == "MOTOR:ON") {
    motorControllerState = true;
    updateMainButtonONOFF(tft, true);   // ✅ aggiorna tasto ON/OFF
  } else if (nmea == "MOTOR:OFF") {
    motorControllerState = false;
    updateMainButtonONOFF(tft, false);  // ✅ aggiorna tasto ON/OFF
  }
}


  else if (nmea.startsWith("$PARAM_UPDATE,")) {
    // =======================
    // Aggiornamento parametri
    // =======================
    String fullField = getFieldNMEA(nmea, 1);
    String param = fullField.substring(0, fullField.indexOf('='));
    String value = fullField.substring(fullField.indexOf('=') + 1);
   // updateDataBox(tft, 5, param + "=" + value);
    delay(2000);
    //updateDataBox(tft, 5, motorControllerState ? "ON" : "OFF");
  }
else if (nmea.startsWith("$HEADING_SOURCE,")) {
    String modeStr = getStringValue(nmea, "MODE");

    if (modeStr == "COMPASS") {
        headingSourceIndex = 0;
        infoLabels[0] = "H.Compass";
    }
    else if (modeStr == "FUSION") {
        headingSourceIndex = 1;
        infoLabels[0] = "H.Gyro";
    }
    else if (modeStr == "EXPERIMENTAL") {
        headingSourceIndex = 2;
        infoLabels[0] = "H.Expmt";
    }
    else {
        headingSourceIndex = -1;
        infoLabels[0] = "Heading";
    }

    // Forza il ridisegno completo del riquadro
    int boxW = tft.width() / 3;
    int boxH = staticAreaHeight / 2;
    int x = 0; // Prima colonna
    int y = 0; // Prima riga
    
    // Pulisci l'area della label
    tft.fillRect(x + 2, y + 2, boxW - 4, 14, TFT_BLACK);
    
    // Ridisegna la label
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(infoLabels[0], x + 2, y + 2);
    
    // Aggiorna il valore (usa il valore corrente o "↻" temporaneo)
    updateDataBox(tft, 0, "↻");
}
}



extern int V_min, V_max, E_min, E_max, E_tol, T_min, T_max;
extern void saveParamsToEEPROM();

// ===============================
// handleCommandAP: gestisce i comandi in arrivo dal touchscreen
// ===============================
void handleCommandAP(String command) {
  debugLog("DEBUG(AP): handleCommandAP -> " + command);

  // 1) Se è un settaggio parametri (tipo "SET:V_min=100")
  if (command.startsWith("SET:")) {
    int eqPos = command.indexOf('=');
    if (eqPos > 4) {
      String param = command.substring(4, eqPos);
      String val = command.substring(eqPos + 1);
      int ival = val.toInt();

      // Aggiorna param
      for (int i = 0; i < NUM_PARAMS; i++) {
        if (param == params[i].name) {
          params[i].value = ival;
          // Sincronizza anche la variabile globale
if      (param == "V_min") V_min = ival;
else if (param == "V_max") V_max = ival;
else if (param == "E_min") E_min = ival;
else if (param == "E_max") E_max = ival;
else if (param == "E_tolleranza") E_tol = ival;
else if (param == "T_min") T_min = ival;
else if (param == "T_max") T_max = ival;

// Salva tutto in EEPROM
saveParamsToEEPROM();

          break;
        }
      }

      // Notifica
      String msg = "PARAM_UPDATED:" + param + "=" + val;
      webSocket.broadcastTXT(msg);
      udp.beginPacket(clientIP, clientPort);
      udp.print(command);
      udp.endPacket();
    }
  }

  // 2) Se è un'azione specifica
  else if (command == "ACTION:CAL") {
    // Se vuoi la logica in AP
    // else forward al client
    udp.beginPacket(clientIP, clientPort);
    udp.print(command);
    udp.endPacket();
  }
  else if (command == "ACTION:GPS") {
    // Se vuoi che AP faccia qualcosa:
    // es: debugLog("AP: Tasto GPS premuto!");
    // headingSourceMode = 1; (se lo gestisci in AP)
    // Oppure:
    udp.beginPacket(clientIP, clientPort);
    udp.print(command);
    udp.endPacket();
  }
  else if (command == "ACTION:C-GPS") {
    // forward o logica sul AP
    udp.beginPacket(clientIP, clientPort);
    udp.print(command);
    udp.endPacket();
  }
  else if (command == "ACTION:-1" || command == "ACTION:+1" ||
           command == "ACTION:-10" || command == "ACTION:+10") {
    // forward o logica su AP
    udp.beginPacket(clientIP, clientPort);
    udp.print(command);
    udp.endPacket();
  }
  else if (command == "ACTION:EXT_BRG") {
    // Se vuoi abilitarlo sul AP
    // externalBearingEnabled = !externalBearingEnabled;
    // ...
    // Oppure forward
    udp.beginPacket(clientIP, clientPort);
    udp.print(command);
    udp.endPacket();
  }
  else if (command == "ACTION:TOGGLE") {
    // Toggle ON/OFF autopilota
    // se vuoi farlo in AP
    // motorControllerState = !motorControllerState;
    // Oppure forward
    udp.beginPacket(clientIP, clientPort);
    udp.print(command);
    udp.endPacket();
  }

  // 3) Altrimenti, forward generico
  else {
    udp.beginPacket(clientIP, clientPort);
    udp.print(command);
    udp.endPacket();
  }
}

// ===============================
// FUNZIONI DI SUPPORTO
// ===============================
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
