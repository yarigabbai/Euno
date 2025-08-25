/*
  EUNO Autopilot – © 2025 Yari Gabbai

  Licensed under CC BY-NC 4.0:
  Creative Commons Attribution-NonCommercial 4.0 International

  You are free to use, modify, and share this code
  for NON-COMMERCIAL purposes only.

  You must credit the original author:
  Yari Gabbai / EUNO Autopilot

  Full license text:
  https://creativecommons.org/licenses/by-nc/4.0/legalcode
*/



#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>
#include "index_html.h"   // contiene: const char INDEX_HTML[] PROGMEM = R"rawliteral(... )rawliteral";

#include <TinyGPSPlus.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <math.h>
#include "sensor_fusion.h"  // Include il nostro modulo sensor fusion
#include <Update.h> 
#include <stdint.h>
#include "ADV_CALIBRATION.h"    

#define EUNO_IS_CLIENT
#include "euno_debug.h"
#include "calibration.h"
// ‼️ forward-declarations ‼️
void handleAdvancedCalibrationCommand(const String& cmd);   // arriverà più avanti
int  applyAdvCalibration(float x, float y);                 // è già definita in fondo
#define FW_VERSION "1.2.1-CLIENT"
#include "icm_compass.h"

  

#include "euno_network.h"   // STA/AP + mDNS + HTTP + WS + UDP + EEPROM(OP)
#include "espnow_link.h"    // ESP-NOW verso TFT (opzionale)
#include "nmea_client.h"    // parser NMEA + $PEUNO,CMD
EunoNetwork net;
EunoEspNow  enow;
EunoCmdAPI  api;
// Parser unico per linee in ingresso (UDP/WS/ESP-NOW)
inline void EUNO_PARSE(const String& s){ parseNMEAClientLine(s, api); }
int externalBearingDeg = -1;  // ultimo bearing esterno valido (per telemetria/UI)

// gg### VARIABILI GLOBALI CONDIVISE ###
// ###########################################
WiFiUDP udp;
IPAddress serverIP(192, 168, 4, 1);
unsigned int serverPort = 4210;
char incomingPacket[255];
int headingSourceMode = 0;  // 0 = COMPASS, 1 = FUSION, 2 = EXPERIMENTAL, 3 = ADV
int headingOffset = 0;      // Offset software per la bussola (impostato con C-GPS)
float smoothedSpeed = 0.0;
int T_pause = 0;  // Valori da 0 a 9 (cioè da 0 a 900 ms)
// Variabili per gestione non bloccante dell'attuatore
unsigned long motorPhaseStartTime = 0;
bool motorPhaseActive = false;  // false = fase di pausa
int lastErrors[3] = {999, 999, 999}; // Errore molto alto iniziale
int erroreIndex = 0;
bool shouldStopMotor = false;
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
int16_t compassOffsetX = 0, compassOffsetY = 0, compassOffsetZ = 0;
ICMCompass compass;

int headingCommand = 0;
int currentHeading = 0;
bool useGPSHeading = false;
bool motorControllerState = false;
TinyGPSPlus gps;
bool externalBearingEnabled = false;
// === MAPPING CALLBACK API → LOGICA ESISTENTE (senza alterare pin/algoritmi) ===
static void api_cmdDelta_internal(int v){
  // Usa i comandi legacy già gestiti da handleCommandClient
  if (v==1)        handleCommandClient("ACTION:+1");
  else if (v==-1)  handleCommandClient("ACTION:-1");
  else if (v==10)  handleCommandClient("ACTION:+10");
  else if (v==-10) handleCommandClient("ACTION:-10");
  else {
    // fallback: aggiorna direttamente headingCommand se autopilota ON
    if (motorControllerState){
      headingCommand = (headingCommand + v) % 360;
      if (headingCommand<0) headingCommand += 360;
    }
  }
}
static void api_cmdToggle_internal(){
  handleCommandClient("ACTION:TOGGLE");
}
static void api_cmdSetParam_internal(const String& k,int val){
  // Reimpiega la tua funzione di update config
  updateConfig(String("SET:")+k+"="+String(val));
}
static void api_cmdMode_internal(const String& m){
  int mode = headingSourceMode;
  if      (m=="COMPASS")      mode = 0;
  else if (m=="FUSION")       mode = 1;
  else if (m=="EXPERIMENTAL") mode = 2;
  else if (m=="ADV")          mode = 3;
  headingSourceMode = mode;
  sendHeadingSource(mode);
}
static void api_cmdCal_internal(const String& w){
  if (w=="MAG"){
    calibrationMode = true;
    calibrationStartTime = millis();
    resetCalibrationData();
    debugLog("CAL: Hard-iron avviata.");
  } else if (w=="GYRO"){
    performSensorFusionCalibration();
    debugLog("CAL: Gyro calib eseguita.");
  } else {
    handleAdvancedCalibrationCommand(w);
  }
}
static void api_cmdExtBrg_internal(bool on){
  externalBearingEnabled = on;
}
static void api_cmdExternalBearing_internal(int brg){
  externalBearingDeg = brg;
  headingCommand = brg; // se ON, la tua logica userà questo valore
}
static void api_onOpenPlotterFrame_internal(const String& kind,const String& raw){
  debugLog(String("OP ")+kind+": "+raw);
}

unsigned long lastSensorUpdate = 0;
const unsigned long sensorUpdateInterval = 100; // ms

// Parametri configurabili
int V_min = 100;
int V_max = 255;
int E_min = 5;
int E_max = 40;
int E_tol = 1;
int T_risposta = 10; // Tempo ideale (in secondi) per calo errore. Range consigliato: 3–12


// Variabili di controllo
float errore_precedente = 0;

int direzione_attuatore = 0;




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
    nmeaData += "T_risposta=" + String(T_risposta) + ",";
    nmeaData += "T_pause=" + String(T_pause) + ",";
    nmeaData += "V_min=" + String(V_min) + ",";
    nmeaData += "V_max=" + String(V_max) + "*";
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
   else if (mode == 3) modeStr = "ADV";
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
    } else if (param == "Deadband") {
        E_tol = value;
        saveParameterToEEPROM(18, E_tol);
        debugLog("Parametro Deadband aggiornato: " + String(E_tol));
   
    }
    else if (param == "T_pause") {
    T_pause = value;
    writeParameterToEEPROM(24, T_pause);  // usa l'indirizzo 24, se non è già usato da altro
    debugLog("Parametro T_pause aggiornato: " + String(T_pause));
} else if (param == "T_risposta") {
    T_risposta = constrain(value, 3, 12);
    writeParameterToEEPROM(26, T_risposta);
    debugLog("Parametro T_risposta aggiornato: " + String(T_risposta));
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
void handleAdvancedCalibrationCommand(const String& cmd) {
    // TODO: Gestisci comandi specifici per la calibrazione avanzata
    if (cmd == "ADV_CANCEL") {
        advCalibrationMode = false;
    }
}
void resetAllEEPROM() {
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0xFF); // o 0x00
  }
  EEPROM.commit();
  debugLog("EEPROM azzerata!");
}

void handleCommandClient(String command) {
  handleAdvancedCalibrationCommand(command);
    debugLog("DEBUG(UDP): Comando ricevuto -> " + command);
    if (command == "GET_FW_VERSION") {
    String reply = String("FW_VERSION_CLIENT:") + FW_VERSION;
    udp.beginPacket(serverIP, serverPort); // o udp.remoteIP(), udp.remotePort()
    udp.write((const uint8_t*)reply.c_str(), reply.length());
        Serial.println("Client: Inviata risposta firmware al server!");
    udp.endPacket();
    return; 
}

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
    if (motorControllerState) {
        headingCommand = currentHeading;
        debugLog("DEBUG(UDP): STOCAZZOMotor controller acceso. Heading resettato.");
        udp.beginPacket(serverIP, serverPort);
        udp.print("MOTOR:ON");
        udp.endPacket();
    } else {
        debugLog("DEBUG(UDP): Motor controller spento.");
        udp.beginPacket(serverIP, serverPort);
        udp.print("MOTOR:OFF");
        udp.endPacket();  }
    }else if (command == "ACTION:CAL") {
    if (!calibrationMode) {
        calibrationMode = true;
        calibrationStartTime = millis();
        resetCalibrationData();
        debugLog("CAL: Inizio calibrazione. Attendi 10 secondi muovendo il sensore...");
    }

    } else if (command == "ACTION:GPS") {
        headingSourceMode++;
        if (headingSourceMode > 3) headingSourceMode = 0;
        
        sendHeadingSource(headingSourceMode);  // Notifica l'AP della nuova modalità
        useGPSHeading = (headingSourceMode == 1);  // Imposta il flag in base alla modalità
        
        // Allinea headingCommand al nuovo heading in base alla modalità
if (headingSourceMode == 0) {
    headingCommand = getCorrectedHeading();
} else if (headingSourceMode == 1) {
    headingCommand = (int)round(getFusedHeading());
} else if (headingSourceMode == 2) {
    headingCommand = (int)round(getExperimentalHeading());
} else if (headingSourceMode == 3) {
    compass.read();
    float rawX = compass.getX();
    float rawY = compass.getY();
    headingCommand = applyAdvCalibration(rawX, rawY);  // Funzione che devi avere nel tuo .h/.cpp
}


        
        debugLog("Comando allineato al nuovo heading: " + String(headingCommand));
    } else if (command == "ACTION:C-GPS") {
    if (gps.course.isValid()) {
        int gpsHeading = (int)gps.course.deg();

        // Leggi la bussola senza offset software
        compass.read();
        float rawX = compass.getX() - compassOffsetX;
        float rawY = compass.getY() - compassOffsetY;
        int compassHeading = (int)(atan2(rawY, rawX) * 180.0 / M_PI);
        if (compassHeading < 0) compassHeading += 360;

        // Calcola nuovo offset per allineare la bussola al GPS
        headingOffset = (gpsHeading - compassHeading + 360) % 360;

        // Salva in EEPROM
        EEPROM.write(6, headingOffset & 0xFF);
        EEPROM.write(7, (headingOffset >> 8) & 0xFF);
        EEPROM.commit();

        debugLog("C-GPS: Offset bussola aggiornato = " + String(headingOffset));
    } else {
        debugLog("C-GPS fallito: GPS non valido");
    }
}


else if (command == "ACTION:ADV") {
    headingSourceMode = 3; // Imposta la modalità ADVANCED
    sendHeadingSource(3);  // Notifica l'AP della nuova modalità

    // Leggi i valori grezzi della bussola
    compass.read();
    float rawX = compass.getX();
    float rawY = compass.getY();

    // Applica la calibrazione avanzata per ottenere l'heading calibrato
    headingCommand = applyAdvCalibration(rawX, rawY);

    debugLog("DEBUG(ADV): headingCommand aggiornato a " + String(headingCommand));
}

else if (command == "ACTION:EXPCAL") {
    debugLog("DEBUG: Avvio calibrazione ADVANCED");
    startAdvancedCalibration();  
     headingSourceMode = 3;       
    sendHeadingSource(3);     
}
else if (command == "ACTION:ADV_SAVE") {
 if (advPointCount < ADV_SECTORS) {
        compass.read();
        float rawX = compass.getX();
        float rawY = compass.getY();

        advTable[advPointCount].rawX = rawX;
        advTable[advPointCount].rawY = rawY;
        advTable[advPointCount].headingDeg = headingCommand;  // Oppure currentHeading

        debugLog("ADV: Salvato punto #" + String(advPointCount) +
                 " → X=" + String(rawX) +
                 " Y=" + String(rawY) +
                 " → heading=" + String(headingCommand));

        advPointCount++;
    } else {
        debugLog("ADV: Tabella piena");
    }
}

else if (command == "ACTION:CAL-GYRO") {
  debugLog("Avvio calibrazione completa (tilt + gyro)...");
  performSensorFusionCalibration();
}


     else if (command == "EXT_BRG_ENABLED") {
        externalBearingEnabled = true;
    } else if (command == "EXT_BRG_DISABLED") {
        externalBearingEnabled = false;
    } else if (command.startsWith("SET:")) {
        updateConfig(command);
    }
}
//───────────────────────────────────────────────
// Supporto: differenza circolare  –180 … +180
int calculateCircularError(int heading, int command) {
    int diff = (command - heading + 540) % 360 - 180;
    return diff;
}

// Supporto: velocità target proporzionale all’errore
float calcolaVelocitaTarget(float errore) {
    float absErr = abs(errore);
    if (absErr <= E_min) return V_min;
    if (absErr >= E_max) return V_max;
    return V_min + ((V_max - V_min) * (absErr - E_min)) / (float)(E_max - E_min);
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

//───────────────────────────────────────────────
// Nuova logica a 3 stati con PWM proporzionale
//───────────────────────────────────────────────
// (Il resto del file resta INVARIATO fino alla funzione calcola_velocita_e_verso)
//───────────────────────────────────────────────
// Nuova logica a 3 stati – corregge anche con errore negativo
//───────────────────────────────────────────────
int calcola_velocita_e_verso(int rotta_attuale, int rotta_desiderata) {
    // 1️⃣ Errore circolare (‑180° … +180°)
    float errore = (float)((rotta_desiderata - rotta_attuale + 540) % 360 - 180);

    // 2️⃣ Verso dell’attuatore: +1 = estendi  |  ‑1 = ritrai
    float verso = (errore >= 0.0f) ? 1.0f : -1.0f;

    // 3️⃣ Zona morta (dead‑band)
    if (fabs(errore) <= E_tol) {
        errore_precedente = errore;
        return 0;   // motore fermo
    }

    // 4️⃣ Velocità reale dell’errore (°/s) – uso solo il modulo
    float deltaAmp = fabs(errore_precedente) - fabs(errore);
    errore_precedente = errore;   // aggiorna storico

    // 5️⃣ Velocità target (°/s) per annullare l’errore in T_risposta secondi
    float velocita_target = fabs(errore) / (float)T_risposta;

    // 6️⃣ Margine accettabile (±20 %)
    float margine = velocita_target * 0.20f;

    // 7️⃣ PWM proporzionale alla grandezza dell’errore (magnitudine positiva)
    int pwm = abs(calcola_velocita_proporzionale((int)errore));  // sempre ≥0

    // 8️⃣ Decisione sui 3 stati
    if (fabs(deltaAmp - velocita_target) <= margine) {
        return 0;                   // ★ FERMA: velocità corretta
    } else if (deltaAmp < velocita_target) {
        return pwm * verso;         // ★ CONTINUA: troppo lento
    } else {
        return pwm * -verso;        // ★ INVERTI: troppo veloce (overshoot)
    }
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
    // ❶ sblocca / mantiene il QMC5883L in continuous-mode
    compass.read();
    int headingCompass = getCorrectedHeading();

    // ❷ scegli la sorgente da usare
    if (useGPSHeading && gps.speed.isValid() && gps.course.isValid()) {
        currentHeading = (int)round(getFusedHeading());
    } else {
        currentHeading = headingCompass;
    }
}
// Ritorna l'heading da pubblicare in base al mode attivo
int getHeadingByMode(){
  // calcoli di base:
  int hdgC = getCorrectedHeading();                  // COMPASS
  int hdgF = (int)round(getFusedHeading());          // FUSION
  updateExperimental(hdgF);
  int hdgE = (int)round(getExperimentalHeading());   // EXPERIMENTAL
  int hdgA = hdgC;
  if (isAdvancedCalibrationComplete()){
    compass.read();
    float rawX = compass.getX(), rawY = compass.getY();
    hdgA = applyAdvCalibration(rawX, rawY);          // ADV
  }

  switch(headingSourceMode){
    case 0:  return hdgC; // COMPASS
    case 1:  return hdgF; // FUSION
    case 2:  return hdgE; // EXPERIMENTAL
    case 3:  return hdgA; // ADV
    default: return hdgF;
  }
}

// ###########################################
// ### SETUP E LOOP ###
// ###########################################
void setup() {
    Serial.begin(115200);
    EEPROM.begin(2048);
    loadAdvCalibrationFromEEPROM();
    Serial.printf("EEPROM 0..5 →  %02X %02X  %02X %02X  %02X %02X\n",
              EEPROM.read(0), EEPROM.read(1),
              EEPROM.read(2), EEPROM.read(3),
              EEPROM.read(4), EEPROM.read(5));

Serial.printf("Offset letti  →  X=%d  Y=%d  Z=%d\n",
              compassOffsetX, compassOffsetY, compassOffsetZ);

    // Legge offset software per la bussola (C-GPS)
    headingOffset = EEPROM.read(6) | (EEPROM.read(7) << 8);
    
    // ─── Qui leggi gli offset SIGNED appena parte lo sketch ───
// legge esattamente 2 byte SIGNED in compassOffsetX/Y/Z
EEPROM.get<int16_t>(0, compassOffsetX);
EEPROM.get<int16_t>(2, compassOffsetY);
EEPROM.get<int16_t>(4, compassOffsetZ);
Serial.printf("Offset letti  →  X=%d  Y=%d  Z=%d\n",
              compassOffsetX, compassOffsetY, compassOffsetZ);

    // Carica parametri da EEPROM
    V_min = readParameterFromEEPROM(10);
    V_max = readParameterFromEEPROM(12);
    E_min = readParameterFromEEPROM(14);
    E_max = readParameterFromEEPROM(16);
    E_tol = readParameterFromEEPROM(18);
    T_pause = readParameterFromEEPROM(24);
T_risposta = readParameterFromEEPROM(26);

// Inizializza bussola
// I2C: fallo SOLO qui (niente begin/setClock nella classe)
Wire.begin(8, 9);
Wire.setClock(400000);

// prova 0x68 poi 0x69 in automatico
if (!compass.beginAuto(&Wire)) {
  debugLog("ICM-20948 non trovato su 0x68/0x69! Controlla wiring.");
  while (true) delay(10);
}
Serial.printf("ICM-20948 inizializzato su indirizzo 0x%02X\n", compass.getAddress());

// prima lettura
compass.read();



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
    //setupWiFi("EUNO AP", "password");
// === Rete/UI: STA(EUNOAP→OP) con fallback AP; mDNS, HTTP(/), WS(:81), UDP(:10110)
  EUNO_LOAD_OP_CREDS(net);
  net.begin();
  net.onUdpLine   = [](const String& s){ EUNO_PARSE(s); };
  net.onUiCommand = [](const String& s){ EUNO_PARSE(s); };

  // ESP-NOW opzionale verso TFT
  enow.begin();
  enow.onLine = [](const String& s){ EUNO_PARSE(s); };

  // Collega callback comandi alla logica esistente
  api.onDelta            = [](int v){ api_cmdDelta_internal(v); };
  api.onToggle           = [](){ api_cmdToggle_internal(); };
  api.onSetParam         = [](const String& k,int v){ api_cmdSetParam_internal(k,v); };
  api.onMode             = [](const String& m){ api_cmdMode_internal(m); };
  api.onCal              = [](const String& w){ api_cmdCal_internal(w); };
  api.onExtBrg           = [](bool on){ api_cmdExtBrg_internal(on); };
  api.onExternalBearing  = [](int brg){ api_cmdExternalBearing_internal(brg); };
  api.onOpenPlotterFrame = [](const String& kind,const String& raw){ api_onOpenPlotterFrame_internal(kind,raw); };
}

void loop() {
// Servizi di rete non bloccanti
  net.loop();
  enow.loop();

  
    unsigned long currentMillis = millis();
    // Lettura sensori ogni 100 ms
if (currentMillis - lastSensorUpdate >= sensorUpdateInterval) {
  lastSensorUpdate = currentMillis;
 if (isAdvancedCalibrationMode()) {
    compass.read();
    updateAdvancedCalibration(headingGyro, compass.getX(), compass.getY(), compass.getZ());

    if (isAdvancedCalibrationComplete()) {
        saveAdvCalibrationToEEPROM();
        debugLog("Calibrazione completata. ADV salvata su EEPROM.");

        // AGGIUNGI QUESTO BLOCCO ↓↓↓↓↓
        udp.beginPacket(serverIP, serverPort);
        udp.print("EXPCAL_DONE");
        udp.endPacket();
        debugLog("Risposta EXPCAL_DONE inviata all’AP");
    }

        }
  // 🔁 Leggi sensori e aggiorna heading corrente (ma NON inviare nulla)


  if (useGPSHeading && gps.speed.isValid() && gps.course.isValid()) {
    currentHeading = (int)round(getFusedHeading());
  } else {
    compass.read();
    currentHeading = getCorrectedHeading();
  }


}

    
    // Aggiornamento sensori ad alta frequenza
    updateSensorFusion();
    
    //gestione loop adv
    if (isAdvancedCalibrationMode()) {
   compass.read();
updateAdvancedCalibration(headingGyro, compass.getX(), compass.getY(), compass.getZ());

    if (isAdvancedCalibrationComplete()) {
        debugLog("DEBUG: Calibrazione ADVANCED completata");
        // saveAdvancedCalibrationToEEPROM(); // se vuoi salvarla in modo permanente
    }
}

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
    unsigned long lastSensorUpdate = 0;
const unsigned long sensorUpdateInterval = 100; // ms
    if (currentMillis - last1HzUpdate >= 1000) {
        last1HzUpdate = currentMillis;
        
        // Ottieni i tre heading:
        int headingCompass = getCorrectedHeading();    // dalla bussola
        float headingFusion = getFusedHeading();         // dal gyro che tende al GPS
        updateExperimental(headingFusion);
        float headingExp = getExperimentalHeading();     // fusione sperimentale
        
        // Seleziona il currentHeading in base alla modalità attiva
   if (headingSourceMode == 0) {
    currentHeading = getCorrectedHeading();
} else if (headingSourceMode == 1) {
    currentHeading = round(getFusedHeading());
} else if (headingSourceMode == 2) {
    currentHeading = round(getExperimentalHeading());
} else if (headingSourceMode == 3) {
    compass.read();
    float rawX = compass.getX();
    float rawY = compass.getY();
    currentHeading = applyAdvCalibration(rawX, rawY);  // ✅ Questo valore andrà in HEADING=
}

int diff = calculateDifference(currentHeading, headingCommand);

// ✅ Questo invia il valore aggiornato
sendNMEAData(currentHeading, headingCommand, diff, gps);

        currentHeading = (currentHeading + 360) % 360;
        
        // Calcola l'errore e invia i dati via UDP
        diff = calculateDifference(currentHeading, headingCommand);
        sendNMEAData(currentHeading, headingCommand, diff, gps);
         }
    if (motorControllerState) {
    // 1. Calcola durate in base a T_pause
    int pauseTime = T_pause * 100;
    int activeTime = 1000 - pauseTime;
    if (activeTime < 200) activeTime = 200;

    unsigned long now = millis();

    // 2. FASE ATTIVA: attuatore acceso
    if (motorPhaseActive) {
        if (now - motorPhaseStartTime >= activeTime) {
            stopMotor();                     // spegne il motore
            motorPhaseActive = false;        // passa alla pausa
            motorPhaseStartTime = now;       // reset timer
        }
    }

else {
    if (now - motorPhaseStartTime >= pauseTime) {
        // Calcola errore assoluto
        int errore = abs(calculateDifference(currentHeading, headingCommand));

        // Aggiorna lo storico degli errori
        lastErrors[erroreIndex % 3] = errore;
        erroreIndex++;

        // Solo dopo 3 campioni confrontiamo
        if (erroreIndex >= 3) {
            int e0 = lastErrors[(erroreIndex - 3) % 3];
            int e1 = lastErrors[(erroreIndex - 2) % 3];
            int e2 = lastErrors[(erroreIndex - 1) % 3];

            debugLog("DEBUG ERRORI: " + String(e0) + " → " + String(e1) + " → " + String(e2));

            if (e2 < e1 && e1 < e0) {
                shouldStopMotor = true;
                debugLog("STOP MOTORE: errore in calo per 3 cicli");
            } else {
                shouldStopMotor = false;
            }
        }

        // Se va fermato, non accende il motore
        if (shouldStopMotor) {
            stopMotor();
            motorPhaseActive = false;
            motorPhaseStartTime = now;
        } else {
            int velocita_correzione = calcola_velocita_e_verso(currentHeading, headingCommand);
            gestisci_attuatore(velocita_correzione);  // accende il motore
            motorPhaseActive = true;
            motorPhaseStartTime = now;
        }
    }
}

 
    }
       // Gestione calibrazione se attiva
 if (calibrationMode) {
    currentMillis = millis();
    performCalibration(currentMillis);
}// Telemetria verso UI (WS) + ESP-NOW e NMEA HDT via UDP
// === TELEMETRIA $AUTOPILOT (WS + ESP-NOW + UDP HDT) =====================
// Frequenza invio: ~5 Hz per i valori “di controllo” (Heading/Command/Error)
// NB: l’errore è quello dell’autopilota (niente medie). Solo il campo HDG_C
// mostrato in UI è smussato 1 Hz per rendere la bussola “ferma” a schermo.
static unsigned long _lastTel = 0;
if (millis() - _lastTel >= 200) {

  // 1) Heading “di controllo” e errore DAL CLIENT (autopilota)
  int hdgOut = getHeadingByMode();                       // selezione fonte attiva
  int err    = calculateDifference(hdgOut, headingCommand); // differenza -180..+180

  // 2) Compass per UI: SMUSSATO solo per display (1 Hz, media circolare)
  static float          hdgC_smoothed = NAN;
  static unsigned long  hdgC_last     = 0;
  int   hdgC_raw = getCorrectedHeading();                // tilt-compensated dal tuo codice
  unsigned long now = millis();
  if (isnan(hdgC_smoothed)) {
    hdgC_smoothed = hdgC_raw;
    hdgC_last     = now;
  }
  if (now - hdgC_last >= 1000) {                         // aggiorna 1 volta al secondo
    float diff = fmodf((hdgC_raw - hdgC_smoothed + 540.0f), 360.0f) - 180.0f;
    hdgC_smoothed = fmodf(hdgC_smoothed + 0.3f * diff + 360.0f, 360.0f);
    hdgC_last = now;
  }
  int hdgC = (int)lroundf(hdgC_smoothed);

  // 3) Altri heading come già usi (FUSION / EXPERIMENTAL / ADV)
  int hdgF = (int)round(getFusedHeading());              // gyro+GPS dalla tua fusion
  updateExperimental(hdgF);
  int hdgE = (int)round(getExperimentalHeading());
  int hdgA = hdgC;                                       // fallback: usa compass
  if (isAdvancedCalibrationComplete()) {
    compass.read();
    hdgA = applyAdvCalibrationInterp3D(
      compass.getX(), compass.getY(), compass.getZ()
    );
  }

  // 4) Monta la riga $AUTOPILOT completa, includendo COMMAND per allineare la UI
  String telem = String("$AUTOPILOT")
               + ",MODE="    + String(headingSourceMode)              // puoi passarlo anche come stringa se vuoi
               + ",MOTOR="   + String(motorControllerState ? "ON" : "OFF")
               + ",HDG="     + String(hdgOut)                          // heading “di controllo”
               + ",COMMAND=" + String(headingCommand)                  // allinea la UI al valore reale del client
               + ",ERR="     + String(err)                             // errore dell’AUTOPILOTA
               + ",HDG_C="   + String(hdgC)                            // compass smussato 1 Hz SOLO per display
               + ",HDG_F="   + String(hdgF)
               + ",HDG_E="   + String(hdgE)
               + ",HDG_A="   + String(hdgA);

  // 5) Invii: WS (UI), ESP-NOW (display remoto), e HDT NMEA via UDP broadcast/unicast
  net.sendWS(telem);                      // WebSocket UI (parser lato browser usa COMMAND/ERR/HDG_*)
  enow.sendLine(telem);                   // opzionale: schermo esterno via ESP-NOW
  String hdt = String("$HDT,") + String(hdgOut) + ",T";
  net.sendUDP(hdt);                       // NMEA verso OpenPlotter/PC

  _lastTel = millis();
}
// =======================================================================
}

int applyAdvCalibration(float x, float y) {
  if (advPointCount == 0) {
    // Se non ci sono punti di calibrazione, ritorna l'heading normale
    float heading = atan2(y, x) * 180.0 / M_PI;
    if (heading < 0) heading += 360;
    return (int)round(heading);
  }
  
  float bestDist = 1e9;
  int bestHeading = 0;
  
  for (int i = 0; i < advPointCount; i++) {
    float dx = x - advTable[i].rawX;
    float dy = y - advTable[i].rawY;
    float dist = sqrt(dx * dx + dy * dy);

    if (dist < bestDist) {
      bestDist = dist;
      bestHeading = advTable[i].headingDeg;
    }
  }

  return bestHeading;
}