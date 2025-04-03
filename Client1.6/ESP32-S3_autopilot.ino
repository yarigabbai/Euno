#include <Wire.h>
#include <EEPROM.h>
#include <QMC5883LCompass.h>
#include <TinyGPSPlus.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <math.h>
#include "sensor_fusion.h"
#include <Update.h> 

// ###########################################
// ### VARIABILI GLOBALI CONDIVISE ###
// ###########################################
WiFiUDP udp;
IPAddress serverIP(192, 168, 4, 1);
unsigned int serverPort = 4210;
char incomingPacket[255];

// ###########################################
// ### VARIABILI OTA CLIENT ###
// ###########################################
bool otaInProgress = false;
uint32_t otaSize = 0;
uint32_t otaReceived = 0;
unsigned long lastOtaUpdateTime = 0;
int otaProgress = 0;

// ###########################################
// ### FUNZIONI OTA CLIENT ###
// ###########################################
void sendOtaStatus(String status, int progress = -1) {
    String statusMsg = "$OTA_STATUS,";
    statusMsg += "STATUS=" + status + ",";
    if(progress >= 0) {
        statusMsg += "PROGRESS=" + String(progress) + ",";
    }
    statusMsg += "RECEIVED=" + String(otaReceived) + ",";
    statusMsg += "TOTAL=" + String(otaSize) + "*";
    
    udp.beginPacket(serverIP, serverPort);
    udp.write((const uint8_t*)statusMsg.c_str(), statusMsg.length());
    udp.endPacket();
    Serial.println("OTA Status: " + status);
}

void handleOTAData(String command) {
    if(command.startsWith("OTA_START:")) {
        otaInProgress = true;
        otaSize = command.substring(10).toInt();
        otaReceived = 0;
        otaProgress = 0;
        lastOtaUpdateTime = millis();
        
        Serial.printf("OTA Started. Size: %u\n", otaSize);
        sendOtaStatus("START", 0);
        
        if(!Update.begin(otaSize)) {
            Serial.println("OTA Begin Failed");
            sendOtaStatus("FAILED:BEGIN");
            otaInProgress = false;
        }
    }
    else if(command.startsWith("OTA_DATA:")) {
        if(otaInProgress) {
            String data = command.substring(9);
            size_t written = Update.write((uint8_t*)data.c_str(), data.length());
            if(written != data.length()) {
                Serial.println("OTA Write Failed");
                Update.end(false);
                sendOtaStatus("FAILED:WRITE");
                otaInProgress = false;
                return;
            }
            otaReceived += data.length();
            
            // Calcola e invia progresso ogni 5% o 1 secondo
            int newProgress = (otaReceived * 100) / otaSize;
            if(newProgress != otaProgress || millis() - lastOtaUpdateTime >= 1000) {
                otaProgress = newProgress;
                lastOtaUpdateTime = millis();
                sendOtaStatus("IN_PROGRESS", otaProgress);
                Serial.printf("OTA Progress: %u/%u bytes (%d%%)\n", 
                            otaReceived, otaSize, otaProgress);
            }
        }
    }
    else if(command.startsWith("OTA_END:")) {
        if(otaInProgress) {
            if(Update.end(true)) {
                Serial.println("OTA Success, rebooting...");
                sendOtaStatus("COMPLETED", 100);
                delay(1000);
                ESP.restart();
            } else {
                Update.printError(Serial);
                Serial.println("OTA End Failed");
                sendOtaStatus("FAILED:END");
            }
            otaInProgress = false;
        }
    }
}

// ###########################################
// ### FUNZIONI EEPROM ###
// ###########################################
void saveParameterToEEPROM(int address, int value) {
  EEPROM.write(address, value & 0xFF);
  EEPROM.write(address + 1, (value >> 8) & 0xFF);
  EEPROM.commit();
}

int readParameterFromEEPROM(int address) {
  return EEPROM.read(address) | (EEPROM.read(address + 1) << 8);
}

// ###########################################
// ### VARIABILI GLOBALI ###
// ###########################################
bool calibrationMode = false;
unsigned long calibrationStartTime = 0;
float minX = 32767, minY = 32767, minZ = 32767;
float maxX = -32768, maxY = -32768, maxZ = -32768;
int compassOffsetX = 0, compassOffsetY = 0, compassOffsetZ = 0;
QMC5883LCompass compass;

int headingCommand = 0;
int currentHeading = 0;
bool useGPSHeading = false;
bool motorControllerState = false;
TinyGPSPlus gps;
bool externalBearingEnabled = false;

// Parametri configurabili
int V_min = 100;
int V_max = 255;
int E_min = 5;
int E_max = 40;
int E_tol = 1;
int T_min = 4;
int T_max = 10;

// Variabili di controllo
int errore_precedente = 0;
int delta_errore_precedente = 0;
int direzione_attuatore = 0;

// ###########################################
// ### FUNZIONI BUSSOLA ###
// ###########################################
void resetCalibrationData() {
    minX = 32767;
    minY = 32767;
    minZ = 32767;
    maxX = -32768;
    maxY = -32768;
    maxZ = -32768;
    Serial.println("DEBUG: Calibration data reset");
}

void performCalibration(unsigned long currentMillis) {
    if (currentMillis - calibrationStartTime < 10000) {
        compass.read();
        float x = compass.getX();
        float y = compass.getY();
        float z = compass.getZ();
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
        if (z < minZ) minZ = z;
        if (z > maxZ) maxZ = z;
    } else {
        calibrationMode = false;
        float offsetX = (maxX + minX) / 2.0;
        float offsetY = (maxY + minY) / 2.0;
        float offsetZ = (maxZ + minZ) / 2.0;
        compassOffsetX = (int)offsetX;
        compassOffsetY = (int)offsetY;
        compassOffsetZ = (int)offsetZ;
        EEPROM.write(0, compassOffsetX & 0xFF);
        EEPROM.write(1, (compassOffsetX >> 8) & 0xFF);
        EEPROM.write(2, compassOffsetY & 0xFF);
        EEPROM.write(3, (compassOffsetY >> 8) & 0xFF);
        EEPROM.write(4, compassOffsetZ & 0xFF);
        EEPROM.write(5, (compassOffsetZ >> 8) & 0xFF);
        EEPROM.commit();
        Serial.println("DEBUG: Calibration complete and offset saved.");
    }
}

int getCorrectedHeading() {
    compass.read();
    float correctedX = compass.getX() - compassOffsetX;
    float correctedY = compass.getY() - compassOffsetY;
    float heading = atan2(correctedY, correctedX) * 180.0 / M_PI;
    if (heading < 0) heading += 360;
    return (int)round(heading);
}

// ###########################################
// ### FUNZIONI WIFI/UDP ###
// ###########################################
void setupWiFi(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    Serial.println("\nConnected to WiFi");
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
}

// ###########################################
// ### GESTIONE COMANDI ###
// ###########################################
void updateConfig(String command) {
    if (!command.startsWith("SET:")) return;
    int eqPos = command.indexOf('=');
    if(eqPos == -1) return;
    String param = command.substring(4, eqPos);
    int value = command.substring(eqPos + 1).toInt();
    
    if (param == "V_min") {
        V_min = value;
        saveParameterToEEPROM(10, V_min);
        Serial.println("Parametro V_min aggiornato: " + String(V_min));
    } else if (param == "V_max") {
        V_max = value;
        saveParameterToEEPROM(12, V_max);
        Serial.println("Parametro V_max aggiornato: " + String(V_max));
    } else if (param == "E_min") {
        E_min = value;
        saveParameterToEEPROM(14, E_min);
        Serial.println("Parametro E_min aggiornato: " + String(E_min));
    } else if (param == "E_max") {
        E_max = value;
        saveParameterToEEPROM(16, E_max);
        Serial.println("Parametro E_max aggiornato: " + String(E_max));
    } else if (param == "E_tolleranza") {
        E_tol = value;
        saveParameterToEEPROM(18, E_tol);
        Serial.println("Parametro E_tolleranza aggiornato: " + String(E_tol));
    } else if (param == "T.S.min") {
        T_min = value;
        saveParameterToEEPROM(20, T_min);
        Serial.println("Parametro T.S.min aggiornato: " + String(T_min));
    } else if (param == "T.S.max") {
        T_max = value;
        saveParameterToEEPROM(22, T_max);
        Serial.println("Parametro T.S.max aggiornato: " + String(T_max));
    }
    
    // Invia conferma dell'aggiornamento
    String confirmMsg = "$PARAM_UPDATE,";
    confirmMsg += param + "=" + String(value) + "*";
    udp.beginPacket(serverIP, serverPort);
    udp.write((const uint8_t*)confirmMsg.c_str(), confirmMsg.length());
    udp.endPacket();
}

void handleCommand(String command) {
    Serial.println("DEBUG(UDP): Comando ricevuto -> " + command);
  if (command.startsWith("CMD:")) {
    if (externalBearingEnabled) {  // Accetta il comando solo se la modalità è abilitata
      int newBearing = command.substring(4).toInt();
      headingCommand = newBearing;
      Serial.print("DEBUG(UDP): Nuovo comando di bearing esterno ricevuto: ");
      Serial.println(headingCommand);
    } else {
      Serial.println("DEBUG(UDP): Ricevuto comando CMD: ma la modalità bearing esterno non è abilitata.");
    }
  }
    // Gestione OTA
    if(command.startsWith("OTA_")) {
        handleOTAData(command);
        return;
    }
    
    // Comandi esistenti
    if (command == "ACTION:-1") {
        if (motorControllerState) {
            headingCommand -= 1;
            if (headingCommand < 0) headingCommand += 360;
        } else {
            retractMotor(V_max);
            delay(700);
            stopMotor();
        }
    } else if (command == "ACTION:+1") {
        if (motorControllerState) {
            headingCommand += 1;
            if (headingCommand >= 360) headingCommand -= 360;
        } else {
            extendMotor(V_max);
            delay(700);
            stopMotor();
        }
    } else if (command == "ACTION:-10") {
        if (motorControllerState) {
            headingCommand -= 10;
            if (headingCommand < 0) headingCommand += 360;
        } else {
            retractMotor(V_max);
            delay(700);
            stopMotor();
        }
    } else if (command == "ACTION:+10") {
        if (motorControllerState) {
            headingCommand += 10;
            if (headingCommand >= 360) headingCommand -= 360;
        } else {
            extendMotor(V_max);
            delay(700);
            stopMotor();
        }
    } else if (command == "ACTION:TOGGLE") {
        motorControllerState = !motorControllerState;
        if (motorControllerState) headingCommand = currentHeading;
    } else if (command == "ACTION:CAL") {
        calibrationMode = true;
        calibrationStartTime = millis();
        resetCalibrationData();
    } else if (command == "ACTION:GPS") {
        useGPSHeading = !useGPSHeading;
    } else if (command == "ACTION:C-GPS") {
        if (gps.course.isValid()) {
            int gpsHeading = (int)gps.course.deg();
            compass.read();
            int currentCompassHeading = getCorrectedHeading();
            compassOffsetX = (gpsHeading - currentCompassHeading + 360) % 360;
            EEPROM.write(0, compassOffsetX & 0xFF);
            EEPROM.write(1, (compassOffsetX >> 8) & 0xFF);
            EEPROM.commit();
        }
    } else if (command == "EXT_BRG_ENABLED") {
        externalBearingEnabled = true;
    } else if (command == "EXT_BRG_DISABLED") {
        externalBearingEnabled = false;
    } else if (command.startsWith("SET:")) {
        updateConfig(command);
    }
}

// ###########################################
// ### CONTROLLO MOTORE ###
// ###########################################
void extendMotor(int speed) {
    analogWrite(3, speed);
    analogWrite(46, 0);
}

void retractMotor(int speed) {
    analogWrite(3, 0);
    analogWrite(46, speed);
}

void stopMotor() {
    analogWrite(3, 0);
    analogWrite(46, 0);
}

// ###########################################
// ### ALGORITMO AUTOPILOTA ###
// ###########################################
int calculateDifference(int heading, int command) {
    int diff = (command - heading + 360) % 360;
    if (diff > 180) diff -= 360;
    return diff;
}

int calcola_velocita_proporzionale(int errore) {
    int absErr = abs(errore);
    if (absErr <= E_min) return V_min;
    if (absErr >= E_max) return V_max;
    return V_min + (V_max - V_min) * ((absErr - E_min) / (float)(E_max - E_min));
}

int calcola_velocita_e_verso(int rotta_attuale, int rotta_desiderata) {
    int errore = (rotta_desiderata - rotta_attuale + 180) % 360 - 180;
    
    if (abs(errore) <= E_tol) {
        errore_precedente = errore;
        return 0;
    }
    
    int delta_errore = errore - errore_precedente;
    
    if (errore > errore_precedente) {
        direzione_attuatore = (errore > 0) ? 1 : -1;
        errore_precedente = errore;
        delta_errore_precedente = delta_errore;
        return calcola_velocita_proporzionale(errore) * direzione_attuatore;
    }
    
    if (abs(delta_errore) >= T_min && abs(delta_errore) <= T_max) {
        direzione_attuatore = 0;
    } else if (abs(delta_errore) < T_min) {
        direzione_attuatore = (errore > 0) ? 1 : -1;
    } else if (abs(delta_errore) > T_max) {
        direzione_attuatore = -direzione_attuatore;
    }
    
    int vel = calcola_velocita_proporzionale(errore);
    int velocita_correzione = vel * direzione_attuatore;
    errore_precedente = errore;
    delta_errore_precedente = delta_errore;
    return velocita_correzione;
}

void gestisci_attuatore(int velocita) {
    if (velocita > 0) extendMotor(velocita);
    else if (velocita < 0) retractMotor(-velocita);
    else stopMotor();
}

// ###########################################
// ### LETTURA SENSORI ###
// ###########################################
void readSensors() {
    if (useGPSHeading && gps.speed.isValid() && gps.course.isValid()) {
         currentHeading = (int)round(getFusedHeading());
    } else {
         compass.read();
         currentHeading = getCorrectedHeading();
    }
}

// ###########################################
// ### SETUP E LOOP ###
// ###########################################
void setup() {
    Serial.begin(115200);
    EEPROM.begin(512);

    // Carica parametri da EEPROM
    V_min = readParameterFromEEPROM(10);
    V_max = readParameterFromEEPROM(12);
    E_min = readParameterFromEEPROM(14);
    E_max = readParameterFromEEPROM(16);
    E_tol = readParameterFromEEPROM(18);
    T_min = readParameterFromEEPROM(20);
    T_max = readParameterFromEEPROM(22);

    // Inizializza bussola
    Wire.begin(8, 9);
    compass.init();

    // Inizializza GPS
    Serial2.begin(9600, SERIAL_8N1, 16, 17);

    // Lettura heading iniziale
    compass.read();
    currentHeading = getCorrectedHeading();
    headingCommand = currentHeading;

    // Configura WiFi
    setupWiFi("EUNO AP", "password");
}

void loop() {
    unsigned long currentMillis = millis();
    
    // Aggiornamento sensori ad alta frequenza
    updateSensorFusion();
    
 // Gestione comandi UDP in arrivo
int packetSize = udp.parsePacket();
if (packetSize > 0) {
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
        incomingPacket[len] = '\0';
        String command = String(incomingPacket);
        Serial.print("DEBUG(Client): Pacchetto UDP ricevuto -> ");
        Serial.println(command);
        handleCommand(command);
    }
}

    // Lettura dati GPS
    while (Serial2.available()) {
        char c = Serial2.read();
        gps.encode(c);
    }

    // Operazioni a 1Hz (intervallo 1000ms)
    static unsigned long last1HzUpdate = 0;
    if (currentMillis - last1HzUpdate >= 1000) {
        last1HzUpdate = currentMillis;
        
        // Lettura heading
        readSensors();
        
        // Calcolo errore e invio dati
        int diff = calculateDifference(currentHeading, headingCommand);
        sendNMEAData(currentHeading, headingCommand, diff, gps);
        
        // Controllo motore
        if (motorControllerState) {
            int velocita_correzione = calcola_velocita_e_verso(currentHeading, headingCommand);
            gestisci_attuatore(velocita_correzione);
        } else {
            stopMotor();
        }
    }

    // Gestione calibrazione se attiva
    if (calibrationMode) {
        performCalibration(currentMillis);
    }
}
