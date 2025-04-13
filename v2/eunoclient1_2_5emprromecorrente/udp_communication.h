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

int readParameterFromEEPROM(int address) {
  return EEPROM.read(address) | (EEPROM.read(address + 1) << 8);
}

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

// Aggiunta delle funzioni per la gestione della EEPROM
#include <EEPROM.h>
#define EEPROM_SIZE 64

int readParameterFromEEPROM(int addr) {
    int low  = EEPROM.read(addr);
    int high = EEPROM.read(addr + 1);
    return (high << 8) | low;
}

void writeParameterToEEPROM(int addr, int value) {
    EEPROM.write(addr,     value & 0xFF);
    EEPROM.write(addr + 1, (value >> 8) & 0xFF);
    EEPROM.commit();
}

void updateConfig(String command) {
    int sep = command.indexOf('=');
    if (sep == -1) return;

    String key = command.substring(4, sep);
    int val = command.substring(sep + 1).toInt();

    if (key == "V_min")      { V_min = val; writeParameterToEEPROM(10, val); }
    else if (key == "V_max") { V_max = val; writeParameterToEEPROM(12, val); }
    else if (key == "E_min") { E_min = val; writeParameterToEEPROM(14, val); }
    else if (key == "E_max") { E_max = val; writeParameterToEEPROM(16, val); }
    else if (key == "E_tol") { E_tol = val; writeParameterToEEPROM(18, val); }
    else if (key == "T_min") { T_min = val; writeParameterToEEPROM(20, val); }
    else if (key == "T_max") { T_max = val; writeParameterToEEPROM(22, val); }

    debugLog("DEBUG(UDP): Parametro aggiornato: " + key + " = " + String(val));
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
