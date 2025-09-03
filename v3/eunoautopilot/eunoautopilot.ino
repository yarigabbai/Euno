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
#include <ESPmDNS.h>
#include <TinyGPSPlus.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <esp_wifi.h>     // <-- usi esp_wifi_set_channel(...)
#include <math.h>
#include "sensor_fusion.h"  // Include il nostro modulo sensor fusion
#include <Update.h>
#include <stdint.h>
#include "ADV_CALIBRATION.h"

#define EUNO_IS_CLIENT
#include "euno_debug.h"
#include "calibration.h"

// ‼️ forward-declarations ‼️
#include "nmea_client.h"   // serve il tipo EunoCmdAPI
EunoCmdAPI eunoCmdApi;     // istanza globale per il parser comandi

void handleAdvancedCalibrationCommand(const String& cmd);
void handleCommandClient(String command);    // <-- usato prima della definizione
void updateConfig(String command);           // <-- usato prima della definizione
void sendHeadingSource(int mode);            // <-- usato prima della definizione
int  applyAdvCalibration(float x, float y);  // definita in fondo

#define FW_VERSION "1.2.1-CLIENT"
#include "icm_compass.h"

#include "euno_network.h"   // STA/AP + mDNS + HTTP + WS + UDP + EEPROM(OP)
#include "nmea_client.h"    // parser NMEA + $PEUNO,CMD

EunoNetwork net;

EunoCmdAPI  api;

// Parser unico per linee in ingresso (UDP/WS/ESP-NOW)
inline void EUNO_PARSE(const String& s){ parseNMEAClientLine(s, api); }

int externalBearingDeg = -1;  // ultimo bearing esterno valido (per telemetria/UI)

// gg### VARIABILI GLOBALI CONDIVISE ###
WiFiUDP udp;
IPAddress serverIP(192, 168, 4, 1);
unsigned int serverPort = 4210;
char incomingPacket[255];
int headingSourceMode = 0;  // 0 = COMPASS, 1 = FUSION, 2 = EXPERIMENTAL, 3 = ADV
int headingOffset = 0;      // Offset software per la bussola (impostato con C-GPS)
float smoothedSpeed = 0.0;
int T_pause = 0;            // 0..9 (0..900 ms)

unsigned long motorPhaseStartTime = 0;
bool motorPhaseActive = false;  // false = fase di pausa
int lastErrors[3] = {999, 999, 999};
int erroreIndex = 0;
bool shouldStopMotor = false;

// EEPROM helpers
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
// Legge 16 bit e applica fallback + clamp in caso di EEPROM non inizializzata (0xFFFF) o fuori-range
int readParamOrDefault(int addr, int defVal, int minV, int maxV) {
  int low  = EEPROM.read(addr);
  int high = EEPROM.read(addr + 1);
  int v = (high << 8) | low;
  if (v == 0xFFFF) return defVal;   // EEPROM “vuota” o non scritta
  if (v < minV)   return defVal;    // fuori range → torna default (o puoi scegliere minV)
  if (v > maxV)   return maxV;      // clamp superiore
  return v;
}

// Legge headingOffset (uint16) con fallback a 0 se 0xFFFF
int readHeadingOffsetOr0() {
  int low  = EEPROM.read(6);
  int high = EEPROM.read(7);
  int v = (high << 8) | low;
  return (v == 0xFFFF) ? 0 : v;
}

// ### VARIABILI OTA CLIENT ###
bool otaInProgress = false;
uint32_t otaSize = 0;
uint32_t otaReceived = 0;
unsigned long lastOtaUpdateTime = 0;
int otaProgress = 0;

// ### FUNZIONI OTA CLIENT ###
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

// ### FUNZIONI EEPROM ###
void saveParameterToEEPROM(int address, int value) {
  EEPROM.write(address, value & 0xFF);
  EEPROM.write(address + 1, (value >> 8) & 0xFF);
  EEPROM.commit();
}

// ### VARIABILI GLOBALI ###
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
  if (v==1)        handleCommandClient("ACTION:+1");
  else if (v==-1)  handleCommandClient("ACTION:-1");
  else if (v==10)  handleCommandClient("ACTION:+10");
  else if (v==-10) handleCommandClient("ACTION:-10");
  else {
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
  headingCommand = brg;
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
int T_risposta = 10; // sec

// Variabili di controllo
float errore_precedente = 0;
int direzione_attuatore = 0;

// ### WIFI/UDP ###
void setupWiFi(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
WiFi.persistent(true);

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

// [FILE CLIENT] invio heading source
void sendHeadingSource(int mode) {
  String modeStr;
  if (mode == 0) modeStr = "COMPASS";
  else if (mode == 1) modeStr = "FUSION";
  else if (mode == 2) modeStr = "EXPERIMENTAL";
  else if (mode == 3) modeStr = "ADV";
  else modeStr = "UNKNOWN";

  String msg = "$HEADING_SOURCE,MODE=" + modeStr + "*";

  udp.beginPacket(serverIP, serverPort);
  udp.write((const uint8_t*)msg.c_str(), msg.length());
  udp.endPacket();
  // enow.sendLine(msg);
   net.sendWS(msg);
  Serial.println("Inviato heading source -> " + msg);
}

// ### GESTIONE COMANDI ###
void updateConfig(String command) {
  if (!command.startsWith("SET:")) return;
  int eqPos = command.indexOf('=');
  if (eqPos == -1) return;
  String param = command.substring(4, eqPos);
  int value = command.substring(eqPos + 1).toInt();

  if (param == "V_min") {
    V_min = value; saveParameterToEEPROM(10, V_min);
  } else if (param == "V_max") {
    V_max = value; saveParameterToEEPROM(12, V_max);
  } else if (param == "E_min") {
    E_min = value; saveParameterToEEPROM(14, E_min);
  } else if (param == "E_max") {
    E_max = value; saveParameterToEEPROM(16, E_max);
  } else if (param == "Deadband") {
    E_tol = value; saveParameterToEEPROM(18, E_tol);
  } else if (param == "T_pause") {
    T_pause = value; writeParameterToEEPROM(24, T_pause);
  } else if (param == "T_risposta") {
    T_risposta = constrain(value, 3, 12); writeParameterToEEPROM(26, T_risposta);
  }

  String confirmMsg = "$PARAM_UPDATE," + param + "=" + String(value) + "*";
  udp.beginPacket(serverIP, serverPort);
  udp.write((const uint8_t*)confirmMsg.c_str(), confirmMsg.length());
  udp.endPacket();
  //enow.sendLine(confirmMsg);
}

void extendMotor(int speed) { analogWrite(3, speed);  analogWrite(46, 0); }
void retractMotor(int speed){ analogWrite(3, 0);      analogWrite(46, speed); }
void stopMotor()             { analogWrite(3, 0);      analogWrite(46, 0); }

void handleAdvancedCalibrationCommand(const String& cmd) {
  if (cmd == "ADV_CANCEL") { advCalibrationMode = false; }
}

void resetAllEEPROM() {
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0xFF);
  EEPROM.commit();
  debugLog("EEPROM azzerata!");
}

void handleCommandClient(String command) {
  handleAdvancedCalibrationCommand(command);
  debugLog("DEBUG(UDP): Comando ricevuto -> " + command);
  
if (command.startsWith("$PEUNO,CMD,")) {
  String cmd = command.substring(12);

  // Mappa comandi DELTA
  if      (cmd == "DELTA=+1")   { headingCommand = (headingCommand + 1) % 360; }
  else if (cmd == "DELTA=-1")   { headingCommand = (headingCommand + 359) % 360; }
  else if (cmd == "DELTA=+10")  { headingCommand = (headingCommand + 10) % 360; }
  else if (cmd == "DELTA=-10")  { headingCommand = (headingCommand + 350) % 360; }

  // Toggle motore
  else if (cmd == "TOGGLE=1") {
    motorControllerState = !motorControllerState;
    if (motorControllerState) {
      headingCommand = currentHeading;
      udp.beginPacket(serverIP, serverPort); udp.print("MOTOR:ON");  udp.endPacket();
      net.sendWS("MOTOR:ON");
    } else {
      udp.beginPacket(serverIP, serverPort); udp.print("MOTOR:OFF"); udp.endPacket();
      net.sendWS("MOTOR:OFF");
    }
  }

  // Calibrazioni
  else if (cmd == "CAL=MAG")   { calibrationMode = true; calibrationStartTime = millis(); resetCalibrationData(); }
  else if (cmd == "CAL=GYRO")  { performSensorFusionCalibration(); }
  else if (cmd == "CAL=C-GPS") { handleCommandClient("ACTION:C-GPS"); } // già definito sotto

  // Cambia modalità heading
  else if (cmd == "MODE=FUSION") {
    headingSourceMode = 1; 
    sendHeadingSource(headingSourceMode);
  }

  // Bearing esterno
  else if (cmd.startsWith("EXTBRG=")) {
    if (cmd.endsWith("ON"))  externalBearingEnabled = true;
    if (cmd.endsWith("OFF")) externalBearingEnabled = false;
  }

  // Set parametri (es. $PEUNO,CMD,SET,T_pause=3)
  else if (cmd.startsWith("SET,")) {
    updateConfig("SET:" + cmd.substring(4));
  }

  return; // blocca qui per non processare due volte
}


  if (command == "GET_FW_VERSION") {
    String reply = String("FW_VERSION_CLIENT:") + FW_VERSION;
    udp.beginPacket(serverIP, serverPort);
    udp.write((const uint8_t*)reply.c_str(), reply.length());
    Serial.println("Client: Inviata risposta firmware al server!");
    udp.endPacket();
    return;
  }

  if (command.startsWith("CMD:")) {
    if (externalBearingEnabled) {
      int newBearing = command.substring(4).toInt();
      headingCommand = newBearing;
      debugLog("Nuovo heading command: " + String(headingCommand));
    } else {
      debugLog("Ricevuto CMD ma bearing esterno disabilitato.");
    }
  }

  // OTA
  if (command.startsWith("OTA_")) { handleOTAData(command); return; }

  // Motore / setpoint
  if (command == "ACTION:-1") {
    if (motorControllerState) {
      headingCommand = (headingCommand + 359) % 360;
    } else { retractMotor(V_max); delay(700); stopMotor(); }
  }
  else if (command == "ACTION:+1") {
    if (motorControllerState) {
      headingCommand = (headingCommand + 1) % 360;
    } else { extendMotor(V_max); delay(700); stopMotor(); }
  }
  else if (command == "ACTION:-10") {
    if (motorControllerState) {
      headingCommand = (headingCommand + 350) % 360;
    } else { retractMotor(V_max); delay(700); stopMotor(); }
  }
  else if (command == "ACTION:+10") {
    if (motorControllerState) {
      headingCommand = (headingCommand + 10) % 360;
      if (headingCommand>=360) headingCommand-=360;
    } else { extendMotor(V_max); delay(700); stopMotor(); }
  }
  else if (command == "ACTION:TOGGLE") {
    motorControllerState = !motorControllerState;
    if (motorControllerState) {
      headingCommand = currentHeading;
      udp.beginPacket(serverIP, serverPort); udp.print("MOTOR:ON");  udp.endPacket();
//enow.sendLine("MOTOR:ON");
    } else {
      udp.beginPacket(serverIP, serverPort); udp.print("MOTOR:OFF"); udp.endPacket();
     // enow.sendLine("MOTOR:OFF");
    }
  }
  else if (command == "ACTION:CAL") {
    if (!calibrationMode) {
      calibrationMode = true;
      calibrationStartTime = millis();
      resetCalibrationData();
      debugLog("CAL: Inizio calibrazione. Muovi il sensore per 10s...");
    }
  }
  else if (command == "ACTION:GPS") {
    headingSourceMode = (headingSourceMode + 1) % 4;
    sendHeadingSource(headingSourceMode);
    useGPSHeading = (headingSourceMode == 1);

    if (headingSourceMode == 0)          headingCommand = getCorrectedHeading();
    else if (headingSourceMode == 1)     headingCommand = (int)round(getFusedHeading());
    else if (headingSourceMode == 2)     headingCommand = (int)round(getExperimentalHeading());
    else /* ADV */ {
      compass.read();
      headingCommand = applyAdvCalibration(compass.getX(), compass.getY());
    }
    debugLog("Comando allineato al nuovo heading: " + String(headingCommand));
  }
  else if (command == "ACTION:C-GPS") {
    if (gps.course.isValid()) {
      int gpsHeading = (int)gps.course.deg();
      compass.read();
      float rawX = compass.getX() - compassOffsetX;
      float rawY = compass.getY() - compassOffsetY;
      int compassHeading = (int)(atan2(rawY, rawX) * 180.0 / M_PI);
      if (compassHeading < 0) compassHeading += 360;
      headingOffset = (gpsHeading - compassHeading + 360) % 360;
      EEPROM.write(6, headingOffset & 0xFF);
      EEPROM.write(7, (headingOffset >> 8) & 0xFF);
      EEPROM.commit();
      debugLog("C-GPS: Offset bussola aggiornato = " + String(headingOffset));
    } else {
      debugLog("C-GPS fallito: GPS non valido");
    }
  }
  else if (command == "ACTION:ADV") {
    headingSourceMode = 3; sendHeadingSource(3);
    compass.read();
    headingCommand = applyAdvCalibration(compass.getX(), compass.getY());
    debugLog("DEBUG(ADV): headingCommand = " + String(headingCommand));
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
      advTable[advPointCount].headingDeg = headingCommand;  // o currentHeading
      debugLog("ADV: Salvato punto #" + String(advPointCount) +
               " → X=" + String(rawX) + " Y=" + String(rawY) +
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
  }
  else if (command == "EXT_BRG_DISABLED") {
    externalBearingEnabled = false;
  }
  else if (command.startsWith("SET:")) {
    updateConfig(command);
  }
}

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

// ### ALGORITMO AUTOPILOTA ###
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

// Nuova logica a 3 stati – corregge anche con errore negativo
int calcola_velocita_e_verso(int rotta_attuale, int rotta_desiderata) {
  float errore = (float)((rotta_desiderata - rotta_attuale + 540) % 360 - 180);
  float verso  = (errore >= 0.0f) ? 1.0f : -1.0f;

  if (fabs(errore) <= E_tol) {
    errore_precedente = errore;
    return 0;
  }

  float deltaAmp = fabs(errore_precedente) - fabs(errore);
  errore_precedente = errore;

  float velocita_target = fabs(errore) / (float)T_risposta;
  float margine = velocita_target * 0.20f;
  int pwm = abs(calcola_velocita_proporzionale((int)errore));

  if (fabs(deltaAmp - velocita_target) <= margine) {
    return 0;                 // FERMA
  } else if (deltaAmp < velocita_target) {
    return pwm * verso;       // CONTINUA
  } else {
    return pwm * -verso;      // INVERTI
  }
}

void gestisci_attuatore(int velocita) {
  if (velocita > 0) extendMotor(velocita);
  else if (velocita < 0) retractMotor(-velocita);
  else stopMotor();
}

// ### LETTURA SENSORI ###
void readSensors() {
  compass.read();
  int headingCompass = getCorrectedHeading();

  if (useGPSHeading && gps.speed.isValid() && gps.course.isValid()) {
    currentHeading = (int)round(getFusedHeading());
  } else {
    currentHeading = headingCompass;
  }
}

// Ritorna l'heading da pubblicare in base al mode attivo
int getHeadingByMode(){
    // Ottieni tilt corrente
    float pitch, roll;
    sensors_event_t acc;
    if (compass.getAccelEvent(acc)) {
        pitch = atan2f(-acc.acceleration.x, sqrtf(acc.acceleration.y*acc.acceleration.y + 
                                                acc.acceleration.z*acc.acceleration.z));
        roll = atan2f(acc.acceleration.y, acc.acceleration.z);
        pitch -= accPitchOffset;
        roll -= accRollOffset;
    }
    
    int hdgC = getCorrectedHeading(); // Ora già compensato
    
    int hdgF = (int)round(getFusedHeading());
    updateExperimental(hdgF);
    int hdgE = (int)round(getExperimentalHeading());
    
    int hdgA = hdgC;
    if (isAdvancedCalibrationComplete()){
        compass.read();
        float mx = compass.getX(), my = compass.getY(), mz = compass.getZ();
        // Applica tilt compensation anche all'ADV
        float headingRad = compensateTilt(mx, my, mz, pitch, roll);
        hdgA = headingRad * 180.0f / M_PI;
        if (hdgA < 0) hdgA += 360;
    }

    switch(headingSourceMode){
        case 0:  return hdgC;
        case 1:  return hdgF;
        case 2:  return hdgE;
        case 3:  return hdgA;
        default: return hdgF;
    }
}

// ### SETUP E LOOP ###
void setup() {
  Serial.begin(115200);
 EEPROM.begin(2048);
loadAdvCalibrationFromEEPROM();

// 1) Leggi offset SIGNED PRIMA di stampare
EEPROM.get<int16_t>(0, compassOffsetX);
EEPROM.get<int16_t>(2, compassOffsetY);
EEPROM.get<int16_t>(4, compassOffsetZ);

// 2) Leggi headingOffset con fallback (0 se 0xFFFF)
headingOffset = readHeadingOffsetOr0();

// 3) Ora stampa: prima i raw bytes, poi i valori letti
Serial.printf("EEPROM 0..5 →  %02X %02X  %02X %02X  %02X %02X\n",
              EEPROM.read(0), EEPROM.read(1),
              EEPROM.read(2), EEPROM.read(3),
              EEPROM.read(4), EEPROM.read(5));
Serial.printf("Offset letti  →  X=%d  Y=%d  Z=%d\n",
              compassOffsetX, compassOffsetY, compassOffsetZ);
Serial.printf("HeadingOffset (deg) → %d\n", headingOffset);

// Default/range: adatta se usi altri limiti nella UI
V_min      = readParamOrDefault(10, /*def*/100, /*min*/0,   /*max*/255);
V_max      = readParamOrDefault(12, /*def*/255, /*min*/0,   /*max*/255);
E_min      = readParamOrDefault(14, /*def*/5,   /*min*/0,   /*max*/180);
E_max      = readParamOrDefault(16, /*def*/40,  /*min*/0,   /*max*/180);
E_tol      = readParamOrDefault(18, /*def*/1,   /*min*/0,   /*max*/20);
T_pause    = readParamOrDefault(24, /*def*/0,   /*min*/0,   /*max*/9);
T_risposta = readParamOrDefault(26, /*def*/10,  /*min*/3,   /*max*/12);
Serial.printf("[PARAM] Vmin=%d Vmax=%d Emin=%d Emax=%d Etol=%d Tpause=%d Trisp=%d\n",
              V_min, V_max, E_min, E_max, E_tol, T_pause, T_risposta);

  // I2C bussola
  Wire.begin(8, 9);
  Wire.setClock(400000);

  if (!compass.beginAuto(&Wire)) {
    debugLog("ICM-20948 non trovato su 0x68/0x69! Controlla wiring.");
    while (true) delay(10);
  }
  Serial.printf("ICM-20948 inizializzato su indirizzo 0x%02X\n", compass.getAddress());
  compass.read();

  // GPS
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
//dns
if (MDNS.begin("euno-client")) {
  Serial.println("mDNS responder started: http://euno-client.local");
} else {
  Serial.println("Error setting up MDNS responder!");
}
  // Heading iniziale
  compass.read();
  int headingCompass = getCorrectedHeading();
  currentHeading = headingCompass;
  headingCommand = currentHeading;
  headingGyro = headingCompass;
  headingExperimental = headingCompass;

  // === Rete/UI: STA(EUNOAP→OP) con fallback AP; mDNS, HTTP(/), WS(:81), UDP(:10110)
  EUNO_LOAD_OP_CREDS(net);
  net.cfg.sta1_ssid = "";
  net.cfg.sta1_pass = "";

  net.begin();


  net.onUdpLine   = [](const String& s){ EUNO_PARSE(s); };
  net.onUiCommand = [](const String& s){
    Serial.println("[WS RX] " + s);   // <--- debug: stampa i comandi che arrivano dal TFT via WS
    EUNO_PARSE(s);
  };




//   // ESP-NOW opzionale verso TFT
//   enow.begin();
// //enow.clearPairing();

//   // Comandi dal TFT via ESP-NOW → parser
//   enow.onLine = [](const String& s){
//     parseNMEAClientLine(s, eunoCmdApi);
//   };

//   enow.startAutoPairing("EUNO");
//   enow.addBroadcastPeer();

  // Collega callback API
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
  // enow.loop();

net.loop();

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
        udp.beginPacket(serverIP, serverPort); udp.print("EXPCAL_DONE"); udp.endPacket();
        debugLog("Risposta EXPCAL_DONE inviata all’AP");
      }
    }

    // Aggiorna heading corrente
    if (useGPSHeading && gps.speed.isValid() && gps.course.isValid()) {
      currentHeading = (int)round(getFusedHeading());
    } else {
      compass.read();
      currentHeading = getCorrectedHeading();
    }
  }

  // Aggiornamento sensori ad alta frequenza
  updateSensorFusion();

  // gestione loop adv (non blocca)
  if (isAdvancedCalibrationMode()) {
    compass.read();
    updateAdvancedCalibration(headingGyro, compass.getX(), compass.getY(), compass.getZ());
    if (isAdvancedCalibrationComplete()) {
      debugLog("DEBUG: Calibrazione ADVANCED completata");
    }
  }

  // Gestione comandi UDP in arrivo
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = '\0';
      String command = String(incomingPacket);
      debugLog(String("DEBUG(Client) UDP -> ") + command);
      handleCommandClient(command);
    }
  }

  // Lettura dati GPS
  while (Serial2.available()) {
    char c = Serial2.read();
    gps.encode(c);
  }

  // Operazioni a 1Hz
  static unsigned long last1HzUpdate = 0;
  if (currentMillis - last1HzUpdate >= 1000) {
    last1HzUpdate = currentMillis;

    int headingCompass = getCorrectedHeading();
    float headingFusion = getFusedHeading();
    updateExperimental(headingFusion);
    float headingExp = getExperimentalHeading();

    if (headingSourceMode == 0) {
      currentHeading = getCorrectedHeading();
    } else if (headingSourceMode == 1) {
      currentHeading = round(getFusedHeading());
    } else if (headingSourceMode == 2) {
      currentHeading = round(getExperimentalHeading());
    } else if (headingSourceMode == 3) {
      compass.read();
      currentHeading = applyAdvCalibration(compass.getX(), compass.getY()); // HEADING=
    }

    int diff = calculateDifference(currentHeading, headingCommand);
    sendNMEAData(currentHeading, headingCommand, diff, gps);

    currentHeading = (currentHeading + 360) % 360;
    diff = calculateDifference(currentHeading, headingCommand);
    sendNMEAData(currentHeading, headingCommand, diff, gps);
  }

  // Controllo motore a duty ciclico
  if (motorControllerState) {
    int pauseTime = T_pause * 100;
    int activeTime = 1000 - pauseTime;
    if (activeTime < 200) activeTime = 200;

    unsigned long now = millis();

    if (motorPhaseActive) {
      if (now - motorPhaseStartTime >= activeTime) {
        stopMotor();
        motorPhaseActive = false;
        motorPhaseStartTime = now;
      }
    } else {
      if (now - motorPhaseStartTime >= pauseTime) {
        int errore = abs(calculateDifference(currentHeading, headingCommand));

        lastErrors[erroreIndex % 3] = errore;
        erroreIndex++;

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

        if (shouldStopMotor) {
          stopMotor();
          motorPhaseActive = false;
          motorPhaseStartTime = now;
        } else {
          int velocita_correzione = calcola_velocita_e_verso(currentHeading, headingCommand);
          gestisci_attuatore(velocita_correzione);
          motorPhaseActive = true;
          motorPhaseStartTime = now;
        }
      }
    }
  }

  // Gestione calibrazione se attiva
  if (calibrationMode) {
    performCalibration(millis());
  }
// Nel loop principale, aggiungi:
static unsigned long lastTiltDebug = 0;
if (millis() - lastTiltDebug > 2000) {
    lastTiltDebug = millis();
    
    float pitch, roll;
    getTiltAngles(pitch, roll);
    
    Serial.printf("Tilt: Pitch=%.1f°, Roll=%.1f°\n", 
                 pitch * 180.0/M_PI, roll * 180.0/M_PI);
    Serial.printf("Offsets: Pitch=%.3f, Roll=%.3f rad\n", 
                 accPitchOffset, accRollOffset);
}
  // === TELEMETRIA $AUTOPILOT (WS + ESP-NOW + UDP HDT) =====================
  static unsigned long _lastTel = 0;
  if (millis() - _lastTel >= 200) {

    // 1) Heading “di controllo” e errore
    int hdgOut = getHeadingByMode();
    int err    = calculateDifference(hdgOut, headingCommand);

    // 2) Compass per UI smussato (1 Hz, media circolare)
    static float          hdgC_smoothed = NAN;
    static unsigned long  hdgC_last     = 0;
    int   hdgC_raw = getCorrectedHeading();
    unsigned long now = millis();
    if (isnan(hdgC_smoothed)) { hdgC_smoothed = hdgC_raw; hdgC_last = now; }
    if (now - hdgC_last >= 1000) {
      float diff = fmodf((hdgC_raw - hdgC_smoothed + 540.0f), 360.0f) - 180.0f;
      hdgC_smoothed = fmodf(hdgC_smoothed + 0.3f * diff + 360.0f, 360.0f);
      hdgC_last = now;
    }
    int hdgC = (int)lroundf(hdgC_smoothed);

    // 3) altri heading
    int hdgF = (int)round(getFusedHeading());
    updateExperimental(hdgF);
    int hdgE = (int)round(getExperimentalHeading());
    int hdgA = hdgC;
    if (isAdvancedCalibrationComplete()) {
      compass.read();
      hdgA = applyAdvCalibrationInterp3D(
        compass.getX(), compass.getY(), compass.getZ()
      );
    }



    // 5) ESP-NOW per TFT
  // 4) WebSocket/UI – usa HEADING e ERROR come nel TFT
String telem = String("$AUTOPILOT")
             + ",HEADING=" + String(hdgOut)
             + ",COMMAND=" + String(headingCommand)
             + ",ERROR="   + String(err)
             + ",GPS_HEADING=" + (gps.course.isValid() ? String((int)gps.course.deg()) : "N/A")
             + ",GPS_SPEED="   + (gps.speed.isValid()  ? String(gps.speed.knots(),1)    : "N/A")
             + ",MODE="    + String(headingSourceMode)
             + ",MOTOR="   + String(motorControllerState ? "ON" : "OFF");
net.sendWS(telem);

Serial.println("[DEBUG] sendWS: " + telem);

    // 6) NMEA UDP (HDT)
    String hdt = String("$HDT,") + String(hdgOut) + ",T";
    net.sendUDP(hdt);

    _lastTel = millis();
  }
} // <-- chiude void loop()

// ------------------------------------------------------------
// Calibrazione avanzata 2D (nearest neighbor su tabella ADV)
// ------------------------------------------------------------
int applyAdvCalibration(float x, float y) {
  if (advPointCount == 0) {
    float heading = atan2(y, x) * 180.0f / M_PI;
    if (heading < 0) heading += 360.0f;
    return (int)lroundf(heading);
  }

  float bestDist = 1e9f;
  int   bestHeading = 0;

  for (int i = 0; i < advPointCount; ++i) {
    float dx = x - advTable[i].rawX;
    float dy = y - advTable[i].rawY;
    float dist = sqrtf(dx*dx + dy*dy);
    if (dist < bestDist) {
      bestDist    = dist;
      bestHeading = advTable[i].headingDeg;
    }
  }

  bestHeading %= 360;
  if (bestHeading < 0) bestHeading += 360;
  return bestHeading;
}
