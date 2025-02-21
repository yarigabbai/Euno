// ===================
// File: udp_communication.h
// ===================

#ifndef UDP_COMMUNICATION_H
#define UDP_COMMUNICATION_H

#include <WiFiUdp.h>
#include <WiFi.h>
#include <TinyGPSPlus.h>
#include "calibration.h"

// Variabili esterne (definite nel main)
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

// *** Aggiunto: variabili per i parametri ***
extern int V_min, V_max, E_min, E_max, E_tol, T_min, T_max;

// Dichiarazione di updateConfig (definita nel main)
void updateConfig(String command);

// Setup WiFi (questa funzione può essere utilizzata se preferisci gestirlo qui)
void setupWiFi(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnesso al WiFi");

    udp.begin(serverPort);
    Serial.printf("DEBUG(UDP): Client UDP avviato. Porta: %d\n", serverPort);
}

// Invia dati NMEA al display (AP) tramite UDP, includendo anche i parametri
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
    Serial.println("DEBUG(UDP): Dati NMEA inviati -> " + nmeaData);
}

// Gestione dei comandi UDP standard
void handleCommand(String command) {
    if (command == "ACTION:-1") {
        headingCommand -= 1;
        if (headingCommand < 0) headingCommand += 360;
        Serial.println("DEBUG(UDP): Comando: -1 grado");
    } else if (command == "ACTION:+1") {
        headingCommand += 1;
        if (headingCommand >= 360) headingCommand -= 360;
        Serial.println("DEBUG(UDP): Comando: +1 grado");
    } else if (command == "ACTION:CAL") {
        Serial.println("DEBUG(UDP): Calibrazione bussola avviata.");
        calibrationMode = true;
        calibrationStartTime = millis();
        resetCalibrationData();
    } else if (command == "ACTION:GPS") {
        useGPSHeading = !useGPSHeading;
        Serial.println("DEBUG(UDP): Cambio sorgente heading: " + String(useGPSHeading ? "GPS" : "Bussola"));
    } else if (command == "ACTION:-10") {
        headingCommand -= 10;
        if (headingCommand < 0) headingCommand += 360;
        Serial.println("DEBUG(UDP): Comando: -10 gradi");
    } else if (command == "ACTION:+10") {
        headingCommand += 10;
        if (headingCommand >= 360) headingCommand -= 360;
        Serial.println("DEBUG(UDP): Comando: +10 gradi");
    } else if (command == "ACTION:TOGGLE") {
        motorControllerState = !motorControllerState;
        if (motorControllerState) {
            headingCommand = currentHeading;
            Serial.println("DEBUG(UDP): Motor controller acceso. Heading resettato.");
        } else {
            Serial.println("DEBUG(UDP): Motor controller spento.");
        }
    } else if (command == "ACTION:C-GPS") {
        if (gps.course.isValid()) {
            int gpsHeading = (int)gps.course.deg();
            compass.read();
            int currentCompassHeading = getCorrectedHeading();
            compassOffsetX = (gpsHeading - currentCompassHeading + 360) % 360;
            Serial.print("DEBUG(UDP): Offset bussola impostato tramite GPS: ");
            Serial.println(compassOffsetX);
            EEPROM.write(0, compassOffsetX & 0xFF);
            EEPROM.write(1, (compassOffsetX >> 8) & 0xFF);
            EEPROM.commit();
        } else {
            Serial.println("DEBUG(UDP): Impossibile impostare l'offset, GPS non valido.");
        }
    } else if (command == "EXT_BRG_ENABLED") {
        externalBearingEnabled = true;
        Serial.println("DEBUG(UDP): External Bearing abilitato.");
    } else if (command == "EXT_BRG_DISABLED") {
        externalBearingEnabled = false;
        Serial.println("DEBUG(UDP): External Bearing disabilitato.");
    }
    else if (command.startsWith("SET:")) {
        updateConfig(command);
    }
}

#endif

