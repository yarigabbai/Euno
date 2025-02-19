/*
Copyright (C) 2024 Yari Gabbai Euno Autopilot

This software is licensed under the Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0).

- You are free to use, modify, and distribute this code **for non-commercial purposes only**.
- You must **attribute** the original author in any derivative work.
- **Commercial use is strictly prohibited** without explicit permission from the author.

Unauthorized commercial use, redistribution, or modification for profit may lead to legal consequences.

For full license details, visit:
https://creativecommons.org/licenses/by-nc/4.0/legalcode
*/







#include <Wire.h>
#include <EEPROM.h>
#include <QMC5883LCompass.h>
#include <TinyGPSPlus.h>
#include "calibration.h"        // Funzioni: getCorrectedHeading(), resetCalibrationData(), performCalibration(), ecc.
#include "udp_communication.h"  // Funzioni: setupWiFi(), handleCommand(), sendNMEAData(), ecc.
#include <math.h>
#include <string.h>  // Per strcmp

// Conversioni
#define DEG_TO_RAD 0.017453292519943295
#define RAD_TO_DEG 57.29577951308232

// --------------------
// Funzioni per EEPROM
// --------------------
void saveParameterToEEPROM(int address, int value) {
  EEPROM.write(address, value & 0xFF);
  EEPROM.write(address + 1, (value >> 8) & 0xFF);
  EEPROM.commit();
}

int readParameterFromEEPROM(int address) {
  return EEPROM.read(address) | (EEPROM.read(address + 1) << 8);
}

// --------------------
// Variabili Globali
// --------------------
QMC5883LCompass compass;
TinyGPSPlus gps;

// Pin per il controllo dell'attuatore (PWM)
const int RPWM = 3;
const int LPWM = 46;

// Variabili WiFi (UDP)
WiFiUDP udp;
unsigned int localUdpPort = 4210;
char incomingPacket[255];
IPAddress serverIP(192, 168, 4, 1);
unsigned int serverPort = 4210;

// Variabili per l'autopilota
int headingCommand;  // Comando target (da GPS, bussola o comando esterno)
int currentHeading;  // Heading corrente (lettura bussola o GPS)
int compassOffsetX = 0, compassOffsetY = 0, compassOffsetZ = 0;
bool motorControllerState = false;
bool useGPSHeading = false;
bool externalBearingEnabled = false; // Aggiornato tramite comando "ACTION:EXT_BRG"

// Variabili per la calibrazione
bool calibrationMode = false;
unsigned long calibrationStartTime;
float minX = 32767, minY = 32767, minZ = 32767;
float maxX = -32768, maxY = -32768, maxZ = -32768;

// Parametri configurabili (salvati in EEPROM)
// Indirizzi: V_min:10-11, V_max:12-13, E_min:14-15, E_max:16-17, E_tol:18-19, T_min:20-21, T_max:22-23.
int V_min = 100;
int V_max = 255;
int E_min = 5;
int E_max = 40;
int E_tol = 1;
int T_min = 4;
int T_max = 10;

// Variabili di stato per il controllo autopilota
int errore_precedente = 0;
int delta_errore_precedente = 0;
int direzione_attuatore = 0;  // 1: estensione, -1: ritrazione, 0: fermo

// Timing non bloccante per il ciclo principale
unsigned long previousLoopMillis = 0;
const unsigned long loopInterval = 1000;  // 1000 ms

// --------------------
// Eliminato: Funzione optimizeGPS()
// --------------------
// Non vengono inviati comandi PMTK e il GPS utilizza i parametri di default.

// --------------------
// Lettura sensori: usa GPS se disponibile, altrimenti la bussola mediata su 3 letture
// --------------------
void readSensors() {
  if (useGPSHeading && gps.course.isValid()) {
    currentHeading = (int) gps.course.deg();
    Serial.println("DEBUG: Uso heading da GPS");
  } else {
    // Media dei dati della bussola: 3 campioni al secondo
    static unsigned long lastCompassUpdate = 0;
    static float sumSin = 0, sumCos = 0;
    static int sampleCount = 0;
    if (millis() - lastCompassUpdate >= 333) {  // circa 3 campioni al secondo
      lastCompassUpdate = millis();
      int newHeading = getCorrectedHeading(); // Funzione da calibration.h
      float rad = newHeading * DEG_TO_RAD;
      sumSin += sin(rad);
      sumCos += cos(rad);
      sampleCount++;
      if (sampleCount >= 3) {
        float avgSin = sumSin / sampleCount;
        float avgCos = sumCos / sampleCount;
        float avgRad = atan2(avgSin, avgCos);
        if (avgRad < 0) avgRad += 2 * PI;
        currentHeading = (int)(avgRad * RAD_TO_DEG);
        sampleCount = 0;
        sumSin = 0;
        sumCos = 0;
      }
    }
    Serial.println("DEBUG: Uso heading da bussola (media 3 letture)");
  }
}

// --------------------
// Calcola differenza tra heading e comando target
// --------------------
int calculateDifference(int heading, int command) {
  int diff = (command - heading + 360) % 360;
  if (diff > 180) diff -= 360;
  return diff;
}

// --------------------
// Calcolo velocità proporzionale
// --------------------
int calcola_velocita_proporzionale(int errore) {
  int absErr = abs(errore);
  if (absErr <= E_min) {
    return V_min;
  } else if (absErr >= E_max) {
    return V_max;
  } else {
    return V_min + (V_max - V_min) * ((absErr - E_min) / (float)(E_max - E_min));
  }
}

// --------------------
// Calcola velocità di correzione e direzione (logica autopilota)
// --------------------
int calcola_velocita_e_verso(int rotta_attuale, int rotta_desiderata) {
  int errore = rotta_desiderata - rotta_attuale;
  errore = (errore + 180) % 360 - 180;
  int delta_errore = errore - errore_precedente;
  
  if (errore > errore_precedente) {
    direzione_attuatore = (errore > 0) ? 1 : -1;
    errore_precedente = errore;
    delta_errore_precedente = delta_errore;
    Serial.println("DEBUG: Delta errore crescente, mantengo direzione attuale.");
    return calcola_velocita_proporzionale(errore) * direzione_attuatore;
  }
  
  if (abs(errore) < E_tol) {
    direzione_attuatore = 0;
  }
  else if (abs(delta_errore) >= T_min && abs(delta_errore) <= T_max) {
    direzione_attuatore = 0;
  }
  else if (abs(delta_errore) < T_min) {
    direzione_attuatore = (errore > 0) ? 1 : -1;
  }
  else if (abs(delta_errore) > T_max) {
    direzione_attuatore = -direzione_attuatore;
  }
  
  int vel = calcola_velocita_proporzionale(errore);
  int velocita_correzione = vel * direzione_attuatore;
  errore_precedente = errore;
  delta_errore_precedente = delta_errore;
  return velocita_correzione;
}

// --------------------
// Gestione attuatore (PWM)
// --------------------
void gestisci_attuatore(int velocita) {
  if (velocita > 0) {
    extendMotor(velocita);
  } else if (velocita < 0) {
    retractMotor(-velocita);
  } else {
    stopMotor();
  }
}

// --------------------
// Funzioni di controllo del motore (PWM)
// --------------------
void extendMotor(int speed) {
  analogWrite(RPWM, speed);
  analogWrite(LPWM, 0);
  Serial.printf("DEBUG: Estensione attuatore con velocità: %d\n", speed);
}

void retractMotor(int speed) {
  analogWrite(RPWM, 0);
  analogWrite(LPWM, speed);
  Serial.printf("DEBUG: Ritrazione attuatore con velocità: %d\n", speed);
}

void stopMotor() {
  analogWrite(RPWM, 0);
  analogWrite(LPWM, 0);
  Serial.println("DEBUG: Motore fermato.");
}

// --------------------
// updateConfig(): aggiorna i parametri in EEPROM
// --------------------
void updateConfig(String command) {
  Serial.println("DEBUG: updateConfig() -> " + command);
  if (command.startsWith("SET:E_tol=")) {
    int newE_tol = command.substring(10).toInt();
    E_tol = newE_tol;
    saveParameterToEEPROM(18, E_tol);
    Serial.printf("DEBUG: E_tol aggiornato a: %d\n", E_tol);
  }
  else if (command.startsWith("SET:T_min=")) {
    int newT_min = command.substring(10).toInt();
    T_min = newT_min;
    saveParameterToEEPROM(20, T_min);
    Serial.printf("DEBUG: T_min aggiornato a: %d\n", T_min);
  }
  else if (command.startsWith("SET:T_max=")) {
    int newT_max = command.substring(10).toInt();
    T_max = newT_max;
    saveParameterToEEPROM(22, T_max);
    Serial.printf("DEBUG: T_max aggiornato a: %d\n", T_max);
  }
  // Altri aggiornamenti eventuali...
}

// --------------------
// setup()
// --------------------
void setup() {
  Serial.begin(115200);
  Serial.println("DEBUG: Avvio setup autopilota...");
  
  EEPROM.begin(512);
  
  // Carica parametri da EEPROM
  V_min   = readParameterFromEEPROM(10);
  V_max   = readParameterFromEEPROM(12);
  E_min   = readParameterFromEEPROM(14);
  E_max   = readParameterFromEEPROM(16);
  E_tol   = readParameterFromEEPROM(18);
  T_min   = readParameterFromEEPROM(20);
  T_max   = readParameterFromEEPROM(22);
  Serial.printf("DEBUG: Parametri caricati: V_min=%d, V_max=%d, E_min=%d, E_max=%d\n", V_min, V_max, E_min, E_max);
  
  // Inizializza la bussola (I2C su pin 8 e 9)
  Wire.begin(8, 9);
  compass.init();
  Serial.println("DEBUG: Bussola inizializzata.");
  
  // Inizializza il GPS su Serial2 con baud rate a 38400 (valore di default per il tuo modulo)
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println("DEBUG: GPS inizializzato (Serial2).");
  
  // Lettura iniziale dell'heading dalla bussola
  compass.read();
  currentHeading = getCorrectedHeading();
  headingCommand = currentHeading;
  Serial.printf("DEBUG: Heading iniziale = %d\n", currentHeading);
  
  // Avvia la connessione WiFi (definita in udp_communication.h)
  setupWiFi("ESP32_AP", "password");
  Serial.println("DEBUG: WiFi configurato.");
  
  Serial.println("DEBUG: Setup completato. Attendo comandi...");
}

// --------------------
// loop()
// --------------------
void loop() {
  Serial.println("DEBUG: Inizio loop");
  
  // Gestione UDP
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    Serial.printf("DEBUG: UDP packet ricevuto. Dimensione: %d bytes\n", packetSize);
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = '\0';
      Serial.printf("DEBUG: Dati UDP ricevuti: %s\n", incomingPacket);
    }
    String cmd = String(incomingPacket);
    Serial.println("DEBUG: Comando ricevuto da AP: " + cmd);
    if (cmd.startsWith("CMD:")) {
      if (externalBearingEnabled) {
        Serial.println("DEBUG: Ricevuto comando bearing (CMD:).");
        String bearingStr = cmd.substring(4);
        Serial.println("DEBUG: Valore bearing estratto come stringa: " + bearingStr);
        int extBearing = bearingStr.toInt();
        Serial.printf("DEBUG: Valore bearing convertito: %d\n", extBearing);
        headingCommand = extBearing;
        Serial.printf("DEBUG: headingCommand aggiornato a: %d\n", headingCommand);
      } else {
        Serial.println("DEBUG: CMD ricevuto ma externalBearingEnabled è disabilitato, comando ignorato.");
      }
    } else {
      Serial.println("DEBUG: Elaboro comando UDP standard: " + cmd);
      handleCommand(cmd);
    }
  } else {
    Serial.println("DEBUG: Nessun pacchetto UDP ricevuto in questo loop.");
  }
  
  // Gestione GPS (non bloccante)
  while (Serial2.available()) {
    char c = Serial2.read();
    gps.encode(c);
  }
  if (gps.course.isValid()) {
    Serial.printf("DEBUG: GPS valido. Heading: %.2f\n", gps.course.deg());
  } else {
    Serial.println("DEBUG: GPS non valido.");
  }
  
  // Aggiorna currentHeading (lettura bussola o GPS)
  readSensors();
  Serial.printf("DEBUG: currentHeading aggiornato: %d\n", currentHeading);
  Serial.printf("DEBUG: headingCommand attuale: %d\n", headingCommand);
  
  // Calcola la differenza (errore)
  int diff = calculateDifference(currentHeading, headingCommand);
  Serial.printf("DEBUG: Errore calcolato (diff): %d\n", diff);
  
  // Invia dati NMEA al display
  sendNMEAData(currentHeading, headingCommand, diff, gps);
  
  // Gestione attuatore
  if (motorControllerState) {
    int velocita_correzione = calcola_velocita_e_verso(currentHeading, headingCommand);
    Serial.printf("DEBUG: Velocità di correzione calcolata: %d\n", velocita_correzione);
    gestisci_attuatore(velocita_correzione);
  } else {
    Serial.println("DEBUG: Motor controller spento, fermo l'attuatore.");
    stopMotor();
  }
  
  // Calibrazione, se attiva
  if (calibrationMode) {
    Serial.println("DEBUG: In calibrazione...");
    performCalibration(millis());
  }
  
  Serial.println("DEBUG: Fine loop\n");
  delay(1000);
}
