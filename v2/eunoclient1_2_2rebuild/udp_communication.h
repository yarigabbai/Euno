#ifndef UDP_COMMUNICATION_H
#define UDP_COMMUNICATION_H

#include <WiFiUdp.h>
#include <WiFi.h>
#include <TinyGPSPlus.h>
#include "calibration.h"

#define EUNO_IS_CLIENT
#include "euno_debug.h"

extern WiFiUDP udp;
extern IPAddress serverIP;
extern unsigned int serverPort;
extern char incomingPacket[255];

extern int headingCommand;
extern int currentHeading;
extern bool useGPSHeading;
extern bool motorControllerState;
extern TinyGPSPlus gps;
extern bool externalBearingEnabled;
extern bool calibrationMode;
extern unsigned long calibrationStartTime;
extern int compassOffsetX;

extern int V_min, V_max, E_min, E_max, E_tol, T_min, T_max;
extern bool otaInProgress;
extern uint32_t otaSize;
extern uint32_t otaReceived;

void updateConfig(String command);
void handleOTAData(String command);

void setupWiFi(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    debugLog("\nConnesso al WiFi");
    udp.begin(serverPort);
}

void sendNMEAData(int currentHeading, int headingCommand, int error, TinyGPSPlus gps) {
    String nmeaData = "$AUTOPILOT,";
    nmeaData += "HEADING=" + String(currentHeading) + ",";
    nmeaData += "COMMAND=" + String(headingCommand) + ",";
    nmeaData += "ERROR=" + String(error) + ",";
    nmeaData += "GPS_HEADING=" + (gps.course.isValid() ? String(gps.course.deg()) : "N/A") + ",";
    nmeaData += "GPS_SPEED=" + (gps.speed.isValid() ? String(gps.speed.knots()) : "N/A") + ",";
    nmeaData += "E_min=" + String(E_min) + ",";
    nmeaData += "E_max=" + String(E_max) + ",";
    nmeaData += "E_tol=" + String(E_tol) + ",";
    nmeaData += "T_min=" + String(T_min) + ",";
    nmeaData += "T_max=" + String(T_max) + "*";
    
    udp.beginPacket(serverIP, serverPort);
    udp.write((const uint8_t*)nmeaData.c_str(), nmeaData.length());
    udp.endPacket();
    debugLog("DEBUG(UDP): Dati NMEA inviati -> " + nmeaData);
}

void handleCommandClient(String command) {
    debugLog("DEBUG(UDP): Comando ricevuto -> " + command);

    if (command.startsWith("CMD:")) {
        if (externalBearingEnabled) {
            int newHeading = command.substring(4).toInt();
            headingCommand = newHeading;
            debugLog("DEBUG(UDP): Nuovo headingCommand da CMD: " + String(headingCommand));
        } else {
            debugLog("DEBUG(UDP): CMD ricevuto ma externalBearingEnabled è disabilitato.");
        }
    }
    else if (command.startsWith("OTA_")) {
        handleOTAData(command);
    }
    else if (command == "ACTION:-1") {
        headingCommand -= 1;
        if (headingCommand < 0) headingCommand += 360;
        debugLog("DEBUG(UDP): Comando: -1 grado");
    }
    else if (command == "ACTION:+1") {
        headingCommand += 1;
        if (headingCommand >= 360) headingCommand -= 360;
        debugLog("DEBUG(UDP): Comando: +1 grado");
    }
    else if (command == "ACTION:CAL") {
        debugLog("DEBUG(UDP): Calibrazione bussola avviata.");
        calibrationMode = true;
        calibrationStartTime = millis();
        resetCalibrationData();
    }
    else if (command == "ACTION:GPS") {
        useGPSHeading = !useGPSHeading;
        debugLog("DEBUG(UDP): Cambio sorgente heading: " + String(useGPSHeading ? "GPS" : "Bussola"));
    }
    else if (command == "ACTION:-10") {
        headingCommand -= 10;
        if (headingCommand < 0) headingCommand += 360;
        debugLog("DEBUG(UDP): Comando: -10 gradi");
    }
    else if (command == "ACTION:+10") {
        headingCommand += 10;
        if (headingCommand >= 360) headingCommand -= 360;
        debugLog("DEBUG(UDP): Comando: +10 gradi");
    }
    else if (command == "ACTION:TOGGLE") {
        motorControllerState = !motorControllerState;
        if (motorControllerState) {
            headingCommand = currentHeading;
            debugLog("DEBUG(UDP): Motor controller acceso. Heading resettato.");
        } else {
            debugLog("DEBUG(UDP): Motor controller spento.");
        }
    }
    else if (command == "ACTION:C-GPS") {
        if (gps.course.isValid()) {
            int gpsHeading = (int)gps.course.deg();
            compass.read();
            int currentCompassHeading = getCorrectedHeading();
            compassOffsetX = (gpsHeading - currentCompassHeading + 360) % 360;
            Serial.print("DEBUG(UDP): Offset bussola impostato tramite GPS: ");
            debugLog(compassOffsetX);
            EEPROM.write(0, compassOffsetX & 0xFF);
            EEPROM.write(1, (compassOffsetX >> 8) & 0xFF);
            EEPROM.commit();
        } else {
            debugLog("DEBUG(UDP): Impossibile impostare l'offset, GPS non valido.");
        }
    }
    else if (command == "EXT_BRG_ENABLED") {
        externalBearingEnabled = true;
        debugLog("DEBUG(UDP): External Bearing abilitato.");
    }
    else if (command == "EXT_BRG_DISABLED") {
        externalBearingEnabled = false;
        debugLog("DEBUG(UDP): External Bearing disabilitato.");
    }
    else if (command.startsWith("SET:")) {
        updateConfig(command);
    }
}

void handleOTAData(String command) {
    if(command.startsWith("OTA_START:")) {
        otaInProgress = true;
        otaSize = command.substring(10).toInt();
        otaReceived = 0;
        if(!Update.begin(otaSize)) {
            debugLog("OTA Begin Failed");
            otaInProgress = false;
        }
    }
    else if(command.startsWith("OTA_DATA:")) {
        if(otaInProgress) {
            String data = command.substring(9);
            size_t written = Update.write((uint8_t*)data.c_str(), data.length());
            if(written != data.length()) {
                debugLog("OTA Write Failed");
                Update.end(false);
                otaInProgress = false;
            }
            otaReceived += data.length();
        }
    }
    else if(command.startsWith("OTA_END:")) {
        if(otaInProgress) {
            if(Update.end(true)) {
                debugLog("OTA Success, rebooting...");
                delay(1000);
                ESP.restart();
            } else {
                debugLog("OTA End Failed");
            }
            otaInProgress = false;
        }
    }
}

#endif