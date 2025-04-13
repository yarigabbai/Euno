#include <Wire.h>
#include <EEPROM.h>
#include <QMC5883LCompass.h>
#include <TinyGPSPlus.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <math.h>
#include "sensor_fusion.h"  // Include il nostro modulo sensor fusion
#include <Update.h> 
#define EUNO_IS_CLIENT
#include "euno_debug.h"

// ###########################################
// ### VARIABILI GLOBALI CONDIVISE ###
// ###########################################
WiFiUDP udp;
IPAddress serverIP(192, 168, 4, 1);
unsigned int serverPort = 4210;
char incomingPacket[255];
int headingSourceMode = 0;  // 0 = COMPASS, 1 = FUSION, 2 = EXPERIMENTAL
int headingOffset = 0;      // Offset software per la bussola (impostato con C-GPS)
float smoothedSpeed = 0.0;

//monitor corrente  su pin 19 e 20 implementare l10 letture in un minuto, poin media e 
//poi 3 volte quella media ferma il motore in quella direzione

//eeprrom
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
    debugLog("OTA Status: " + status);
}

void handleOTAData(String command) {
    if (command.startsWith("OTA_START:")) {
        otaInProgress = true;
        otaSize = command.substring(10).toInt();
        otaReceived = 0;
        otaProgress = 0;
        lastOtaUpdateTime = millis();
        
        Serial.printf("OTA Started. Size: %u\n", otaSize);
        sendOtaStatus("START", 0);
        
        if(!Update.begin(otaSize)) {
            debugLog("OTA Begin Failed");
            sendOtaStatus("FAILED:BEGIN");
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
                sendOtaStatus("FAILED:WRITE");
                otaInProgress = false;
                return;
            }
            otaReceived += data.length();
            
            int newProgress = (otaReceived * 100) / otaSize;
            if(newProgress != otaProgress || millis() - lastOtaUpdateTime >= 1000) {
                otaProgress = newProgress;
                lastOtaUpdateTime = millis();
                sendOtaStatus("IN_PROGRESS", otaProgress);
                Serial.printf("OTA Progress: %u/%u bytes (%d%%)\n", otaReceived, otaSize, otaProgress);
            }
        }
    }
    else if(command.startsWith("OTA_END:")) {
        if(otaInProgress) {
            if(Update.end(true)) {
                debugLog("OTA Success, rebooting...");
                sendOtaStatus("COMPLETED", 100);
                delay(1000);
                ESP.restart();
            } else {
                Update.printError(Serial);
                debugLog("OTA End Failed");
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
    debugLog("DEBUG: Calibration data reset");
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
        debugLog("DEBUG: Calibration complete and offset saved.");
    }
}

int getCorrectedHeading() {
    compass.read();
    float correctedX = compass.getX() - compassOffsetX;
    float correctedY = compass.getY() - compassOffsetY;
    float heading = atan2(correctedY, correctedX) * 180.0 / M_PI;
    if (heading < 0) heading += 360;
    // Applica l'offset software salvato (C-GPS)
    heading = fmod(heading + headingOffset, 360.0);
    return (int)round(heading);
}

// ###########################################
// ### FUNZIONI WIFI/UDP ###
// ###########################################
void setupWiFi(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    debugLog("\nConnected to WiFi");
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
//------------------------------------------------------
// [FILE CLIENT]  Aggiungere intorno alla riga XX
//------------------------------------------------------
void sendHeadingSource(int mode) {
    // Scegli la stringa di heading
    String modeStr;
    if (mode == 0) modeStr = "COMPASS";
    else if (mode == 1) modeStr = "FUSION";
    else if (mode == 2) modeStr = "EXPERIMENTAL";
    else modeStr = "UNKNOWN";

    // Crea messaggio tipo NMEA personalizzato
    // Esempio: "$HEADING_SOURCE,MODE=FUSION*"
    String msg = "$HEADING_SOURCE,MODE=" + modeStr + "*";

    // Invia via UDP
    udp.beginPacket(serverIP, serverPort);
    udp.write((const uint8_t*)msg.c_str(), msg.length());
    udp.endPacket();
    
    // NB: NON usiamo debugLog(...) per evitare i messaggi di debug
    Serial.println("Inviato heading source -> " + msg);
}



// ###########################################
// ### GESTIONE COMANDI ###
// ###########################################
void updateConfig(String command) {
    if (!command.startsWith("SET:")) return;
    int eqPos = command.indexOf('=');
    if (eqPos == -1) return;
    String param = command.substring(4, eqPos);
    int value = command.substring(eqPos + 1).toInt();
    
    if (param == "V_min") {
        V_min = value;
        saveParameterToEEPROM(10, V_min);
        debugLog("Parametro V_min aggiornato: " + String(V_min));
    } else if (param == "V_max") {
        V_max = value;
        saveParameterToEEPROM(12, V_max);
        debugLog("Parametro V_max aggiornato: " + String(V_max));
    } else if (param == "E_min") {
        E_min = value;
        saveParameterToEEPROM(14, E_min);
        debugLog("Parametro E_min aggiornato: " + String(E_min));
    } else if (param == "E_max") {
        E_max = value;
        saveParameterToEEPROM(16, E_max);
        debugLog("Parametro E_max aggiornato: " + String(E_max));
    } else if (param == "E_tolleranza") {
        E_tol = value;
        saveParameterToEEPROM(18, E_tol);
        debugLog("Parametro E_tolleranza aggiornato: " + String(E_tol));
    } else if (param == "T_min") {
        T_min = value;
        saveParameterToEEPROM(20, T_min);
        debugLog("Parametro T_min aggiornato: " + String(T_min));
    } else if (param == "T_max") {
        T_max = value;
        saveParameterToEEPROM(22, T_max);
        debugLog("Parametro T_max aggiornato: " + String(T_max));
    }
    
    String confirmMsg = "$PARAM_UPDATE,";
    confirmMsg += param + "=" + String(value) + "*";
    udp.beginPacket(serverIP, serverPort);
    udp.write((const uint8_t*)confirmMsg.c_str(), confirmMsg.length());
    udp.endPacket();
}

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

void handleCommandClient(String command) {
    debugLog("DEBUG(UDP): Comando ricevuto -> " + command);
    
    if (command.startsWith("CMD:")) {
        if (externalBearingEnabled) {
            int newBearing = command.substring(4).toInt();
            headingCommand = newBearing;
            Serial.print("DEBUG(UDP): Nuovo comando di bearing esterno ricevuto: ");
            debugLog("Nuovo heading command: " + String(headingCommand));
        } else {
            debugLog("DEBUG(UDP): Ricevuto comando CMD: ma la modalità bearing esterno non è abilitata.");
        }
    }
    
    // Gestione OTA
    if (command.startsWith("OTA_")) {
        handleOTAData(command);
        return;
    }
    
    // Controllo motore
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
        headingSourceMode++;
        if (headingSourceMode > 2) headingSourceMode = 0;
        
        sendHeadingSource(headingSourceMode);  // Notifica l'AP della nuova modalità
        useGPSHeading = (headingSourceMode == 1);  // Imposta il flag in base alla modalità
        
        // Allinea headingCommand al nuovo heading in base alla modalità
        if (headingSourceMode == 0) {
            headingCommand = getCorrectedHeading();
        } else if (headingSourceMode == 1) {
            headingCommand = (int)round(getFusedHeading());
        } else if (headingSourceMode == 2) {
            headingCommand = (int)round(getExperimentalHeading());
        }
        
        debugLog("Comando allineato al nuovo heading: " + String(headingCommand));
    } else if (command == "ACTION:C-GPS") {
        if (gps.course.isValid()) {
            int gpsHeading = (int)gps.course.deg();
            int compassHeading = getCorrectedHeading();  // Con offset già applicato
            headingOffset = (gpsHeading - compassHeading + 360) % 360;
            EEPROM.write(6, headingOffset & 0xFF);
            EEPROM.write(7, (headingOffset >> 8) & 0xFF);
            EEPROM.commit();
            
            debugLog("Offset bussola aggiornato da C-GPS: " + String(headingOffset));
        } else {
            debugLog("C-GPS fallito: GPS non valido");
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
    // Legge offset software per la bussola (C-GPS)
    headingOffset = EEPROM.read(6) | (EEPROM.read(7) << 8);
    
    // Carica offset bussola da EEPROM
    compassOffsetX = EEPROM.read(0) | (EEPROM.read(1) << 8);
    compassOffsetY = EEPROM.read(2) | (EEPROM.read(3) << 8);
    compassOffsetZ = EEPROM.read(4) | (EEPROM.read(5) << 8);
    
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
    int headingCompass = getCorrectedHeading();
    
    currentHeading = headingCompass;
    headingCommand = currentHeading;
    headingGyro = headingCompass;
    headingExperimental = headingCompass;
    
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
            debugLog(command);
            handleCommandClient(command);
        }
    }
    
    // Lettura dati GPS
    while (Serial2.available()) {
        char c = Serial2.read();
        gps.encode(c);
    }
    
    // Operazioni a 1Hz (ogni 1000ms)
    static unsigned long last1HzUpdate = 0;
    if (currentMillis - last1HzUpdate >= 1000) {
        last1HzUpdate = currentMillis;
        
        // Ottieni i tre heading:
        int headingCompass = getCorrectedHeading();    // dalla bussola
        float headingFusion = getFusedHeading();         // dal gyro che tende al GPS
        updateExperimental(headingFusion);
        float headingExp = getExperimentalHeading();     // fusione sperimentale
        
        // Seleziona il currentHeading in base alla modalità attiva
        if (headingSourceMode == 0) {
            currentHeading = headingCompass;
        } else if (headingSourceMode == 1) {
            currentHeading = (int)round(headingFusion);
        } else if (headingSourceMode == 2) {
            currentHeading = (int)round(headingExp);
        }
        
        // Calcola l'errore e invia i dati via UDP
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
