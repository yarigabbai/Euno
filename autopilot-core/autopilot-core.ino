/*
Copyright (C) 2024 Yari gabbai Euno Autopilot

This software is licensed under the Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0).

- You are free to use, modify, and distribute this code **for non-commercial purposes only**.
- You must **attribute** the original author in any derivative work.
- **Commercial use is strictly prohibited** without explicit permission from the author.

Unauthorized commercial use, redistribution, or modification for profit may lead to legal consequences.

For full license details, visit:
https://creativecommons.org/licenses/by-nc/4.0/legalcode
*/



// ============================================================
// Integrated Autopilot with Calibration, UDP, EEPROM, and Manual Commands
// ============================================================

#include <Wire.h>
#include <EEPROM.h>
#include <QMC5883LCompass.h>
#include <TinyGPSPlus.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <math.h>

// ============================================================
// EEPROM FUNCTIONS
// ============================================================
void saveParameterToEEPROM(int address, int value) {
  EEPROM.write(address, value & 0xFF);
  EEPROM.write(address + 1, (value >> 8) & 0xFF);
  EEPROM.commit();
}

int readParameterFromEEPROM(int address) {
  return EEPROM.read(address) | (EEPROM.read(address + 1) << 8);
}

// ============================================================
// GLOBAL VARIABLE DEFINITIONS
// (Necessary for any "extern" declared in other files)
// ============================================================

// Calibration variables
bool calibrationMode = false;
unsigned long calibrationStartTime = 0;
float minX = 32767, minY = 32767, minZ = 32767;
float maxX = -32768, maxY = -32768, maxZ = -32768;
int compassOffsetX = 0, compassOffsetY = 0, compassOffsetZ = 0;
QMC5883LCompass compass;

// UDP and communication variables
WiFiUDP udp;
IPAddress serverIP(192, 168, 4, 1);
unsigned int serverPort = 4210;
char incomingPacket[255];

int headingCommand = 0;
int currentHeading = 0;
bool useGPSHeading = false;
bool motorControllerState = false;
TinyGPSPlus gps;
bool externalBearingEnabled = false;

// Configurable parameters (stored in EEPROM):
// EEPROM addresses:
//   V_min: 10-11, V_max: 12-13, E_min: 14-15, E_max: 16-17,
//   E_tol: 18-19, T_min: 20-21, T_max: 22-23
int V_min = 100;   // Minimum speed (PWM 0-255)
int V_max = 255;   // Maximum speed (PWM 0-255)
int E_min = 5;     // Minimum error threshold for proportional speed
int E_max = 40;    // Maximum error threshold for proportional speed
int E_tol = 1;     // Tolerance (error tolerance)
int T_min = 4;     // Lower threshold for error delta
int T_max = 10;    // Upper threshold for error delta

// Control state variables
int errore_precedente = 0;
int delta_errore_precedente = 0;
int direzione_attuatore = 0;  // 1: extension, -1: retraction, 0: stop

// ============================================================
// CALIBRATION FUNCTIONS
// ============================================================
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
        Serial.print("DEBUG: Calibration in progress... X: ");
        Serial.print(x);
        Serial.print(" Y: ");
        Serial.print(y);
        Serial.print(" Z: ");
        Serial.println(z);
    } else {
        calibrationMode = false;
        float offsetX = (maxX + minX) / 2.0;
        float offsetY = (maxY + minY) / 2.0;
        float offsetZ = (maxZ + minZ) / 2.0;
        compassOffsetX = (int)offsetX;
        compassOffsetY = (int)offsetY;
        compassOffsetZ = (int)offsetZ;
        // Save the offsets in EEPROM at addresses 0-5 (reserved for calibration)
        EEPROM.write(0, compassOffsetX & 0xFF);
        EEPROM.write(1, (compassOffsetX >> 8) & 0xFF);
        EEPROM.write(2, compassOffsetY & 0xFF);
        EEPROM.write(3, (compassOffsetY >> 8) & 0xFF);
        EEPROM.write(4, compassOffsetZ & 0xFF);
        EEPROM.write(5, (compassOffsetZ >> 8) & 0xFF);
        EEPROM.commit();
        Serial.println("DEBUG: Calibration complete and offset saved.");
        Serial.print("DEBUG: Offset X: ");
        Serial.println(compassOffsetX);
        Serial.print("DEBUG: Offset Y: ");
        Serial.println(compassOffsetY);
        Serial.print("DEBUG: Offset Z: ");
        Serial.println(compassOffsetZ);
    }
}

int getCorrectedHeading() {
    compass.read();
    float correctedX = compass.getX() - compassOffsetX;
    float correctedY = compass.getY() - compassOffsetY;
    float heading = atan2(correctedY, correctedX) * 180.0 / M_PI;
    if (heading < 0) {
        heading += 360;
    }
    int roundedHeading = (int)round(heading);
    Serial.printf("DEBUG: Calculated corrected heading: %d\n", roundedHeading);
    return roundedHeading;
}

// ============================================================
// UDP & WiFi COMMUNICATION FUNCTIONS
// ============================================================
void setupWiFi(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");
    udp.begin(serverPort);
    Serial.printf("DEBUG(UDP): UDP client started. Port: %d\n", serverPort);
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
    Serial.println("DEBUG(UDP): NMEA data sent -> " + nmeaData);
}

// ============================================================
// updateConfig() FUNCTION
// Accepts SET commands sent by the AP, with names such as "V_min", "V_max",
// "E_min", "E_max", "E_tolleranza", "T.S.min", "T.S.max"
// ============================================================
void updateConfig(String command) {
    Serial.println("DEBUG: updateConfig() -> " + command);
    if (!command.startsWith("SET:")) return;
    // Extract the parameter and the value
    int eqPos = command.indexOf('=');
    if(eqPos == -1) return;
    String param = command.substring(4, eqPos);
    int value = command.substring(eqPos + 1).toInt();
    
    if (param == "V_min") {
        V_min = value;
        saveParameterToEEPROM(10, V_min);
        Serial.printf("DEBUG: V_min updated to: %d\n", V_min);
    } else if (param == "V_max") {
        V_max = value;
        saveParameterToEEPROM(12, V_max);
        Serial.printf("DEBUG: V_max updated to: %d\n", V_max);
    } else if (param == "E_min") {
        E_min = value;
        saveParameterToEEPROM(14, E_min);
        Serial.printf("DEBUG: E_min updated to: %d\n", E_min);
    } else if (param == "E_max") {
        E_max = value;
        saveParameterToEEPROM(16, E_max);
        Serial.printf("DEBUG: E_max updated to: %d\n", E_max);
    } else if (param == "E_tolleranza") {  // Accepts "E_tolleranza" as the name
        E_tol = value;
        saveParameterToEEPROM(18, E_tol);
        Serial.printf("DEBUG: E_tol (tolerance) updated to: %d\n", E_tol);
    } else if (param == "T.S.min") {  // Accepts "T.S.min" as the name for T_min
        T_min = value;
        saveParameterToEEPROM(20, T_min);
        Serial.printf("DEBUG: T_min updated to: %d\n", T_min);
    } else if (param == "T.S.max") {  // Accepts "T.S.max" as the name for T_max
        T_max = value;
        saveParameterToEEPROM(22, T_max);
        Serial.printf("DEBUG: T_max updated to: %d\n", T_max);
    } else {
        Serial.println("DEBUG: Unknown parameter: " + param);
    }
}

// ============================================================
// handleCommand() FUNCTION
// Processes incoming UDP commands
// ============================================================
void handleCommand(String command) {
    if (command == "ACTION:-1") {
        if (motorControllerState) {
            headingCommand -= 1;
            if (headingCommand < 0) headingCommand += 360;
            Serial.println("DEBUG(UDP): Command: -1 degree (MC ON)");
        } else {
            // MC OFF: move actuator in retraction direction at V_max for 700ms
            Serial.println("DEBUG(UDP): Command: -1 manual, actuator moves at V_max for 700ms");
            retractMotor(V_max);
            delay(700);
            stopMotor();
        }
    } else if (command == "ACTION:+1") {
        if (motorControllerState) {
            headingCommand += 1;
            if (headingCommand >= 360) headingCommand -= 360;
            Serial.println("DEBUG(UDP): Command: +1 degree (MC ON)");
        } else {
            Serial.println("DEBUG(UDP): Command: +1 manual, actuator moves at V_max for 700ms");
            extendMotor(V_max);
            delay(700);
            stopMotor();
        }
    } else if (command == "ACTION:-10") {
        if (motorControllerState) {
            headingCommand -= 10;
            if (headingCommand < 0) headingCommand += 360;
            Serial.println("DEBUG(UDP): Command: -10 degrees (MC ON)");
        } else {
            Serial.println("DEBUG(UDP): Command: -10 manual, actuator moves at V_max for 700ms");
            retractMotor(V_max);
            delay(700);
            stopMotor();
        }
    } else if (command == "ACTION:+10") {
        if (motorControllerState) {
            headingCommand += 10;
            if (headingCommand >= 360) headingCommand -= 360;
            Serial.println("DEBUG(UDP): Command: +10 degrees (MC ON)");
        } else {
            Serial.println("DEBUG(UDP): Command: +10 manual, actuator moves at V_max for 700ms");
            extendMotor(V_max);
            delay(700);
            stopMotor();
        }
    } else if (command == "ACTION:TOGGLE") {
        motorControllerState = !motorControllerState;
        if (motorControllerState) {
            headingCommand = currentHeading;
            Serial.println("DEBUG(UDP): Motor controller ON. Heading reset.");
        } else {
            Serial.println("DEBUG(UDP): Motor controller OFF.");
        }
    } else if (command == "ACTION:CAL") {
        Serial.println("DEBUG(UDP): Compass calibration started.");
        calibrationMode = true;
        calibrationStartTime = millis();
        resetCalibrationData();
    } else if (command == "ACTION:GPS") {
        useGPSHeading = !useGPSHeading;
        Serial.println("DEBUG(UDP): Changing heading source: " + String(useGPSHeading ? "GPS" : "Compass"));
    } else if (command == "ACTION:C-GPS") {
        if (gps.course.isValid()) {
            int gpsHeading = (int)gps.course.deg();
            compass.read();
            int currentCompassHeading = getCorrectedHeading();
            compassOffsetX = (gpsHeading - currentCompassHeading + 360) % 360;
            Serial.print("DEBUG(UDP): Compass offset set via GPS: ");
            Serial.println(compassOffsetX);
            EEPROM.write(0, compassOffsetX & 0xFF);
            EEPROM.write(1, (compassOffsetX >> 8) & 0xFF);
            EEPROM.commit();
        } else {
            Serial.println("DEBUG(UDP): Unable to set offset, invalid GPS.");
        }
    } else if (command == "EXT_BRG_ENABLED") {
        externalBearingEnabled = true;
        Serial.println("DEBUG(UDP): External Bearing enabled.");
    } else if (command == "EXT_BRG_DISABLED") {
        externalBearingEnabled = false;
        Serial.println("DEBUG(UDP): External Bearing disabled.");
    } else if (command.startsWith("SET:")) {
        updateConfig(command);
    }
}

// ============================================================
// MOTOR CONTROL FUNCTIONS (PWM)
// ============================================================
void extendMotor(int speed) {
    // RPWM = pin 3, LPWM = pin 46
    analogWrite(3, speed);
    analogWrite(46, 0);
    Serial.printf("DEBUG: Actuator extending at speed: %d\n", speed);
}

void retractMotor(int speed) {
    analogWrite(3, 0);
    analogWrite(46, speed);
    Serial.printf("DEBUG: Actuator retracting at speed: %d\n", speed);
}

void stopMotor() {
    analogWrite(3, 0);
    analogWrite(46, 0);
    Serial.println("DEBUG: Motor stopped.");
}

// ============================================================
// AUTOPILOT CONTROL ALGORITHM FUNCTIONS
// ============================================================

// Calculates the difference between the current heading and the command target
int calculateDifference(int heading, int command) {
    int diff = (command - heading + 360) % 360;
    if (diff > 180) diff -= 360;
    return diff;
}

// Calculates the proportional speed based on the error
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

// Calculates the correction speed and direction based on the current and desired headings
int calcola_velocita_e_verso(int rotta_attuale, int rotta_desiderata) {
    int errore = rotta_desiderata - rotta_attuale;
    errore = (errore + 180) % 360 - 180;
    
    // Check tolerance first: if the absolute error is within tolerance, stop the motor
    if (abs(errore) <= E_tol) {
        errore_precedente = errore;
        return 0;
    }
    
    int delta_errore = errore - errore_precedente;
    
    if (errore > errore_precedente) {
        direzione_attuatore = (errore > 0) ? 1 : -1;
        errore_precedente = errore;
        delta_errore_precedente = delta_errore;
        Serial.println("DEBUG: Increasing error delta, keeping current direction.");
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
    if (velocita > 0) {
        extendMotor(velocita);
    } else if (velocita < 0) {
        retractMotor(-velocita);
    } else {
        stopMotor();
    }
}

// ============================================================
// SENSOR READING FUNCTION
// ============================================================
void readSensors() {
    if (useGPSHeading && gps.course.isValid()) {
        currentHeading = (int)gps.course.deg();
        Serial.println("DEBUG: Using GPS heading.");
    } else {
        compass.read();
        currentHeading = getCorrectedHeading();
        Serial.println("DEBUG: Using compass heading.");
    }
}

// ============================================================
// SETUP() AND LOOP()
// ============================================================
void setup() {
    Serial.begin(115200);
    Serial.println("DEBUG: Starting autopilot setup...");
    EEPROM.begin(512);

    // Load parameters from EEPROM (if they exist)
    V_min = readParameterFromEEPROM(10);
    V_max = readParameterFromEEPROM(12);
    E_min = readParameterFromEEPROM(14);
    E_max = readParameterFromEEPROM(16);
    E_tol = readParameterFromEEPROM(18);
    T_min = readParameterFromEEPROM(20);
    T_max = readParameterFromEEPROM(22);
    Serial.printf("DEBUG: Loaded parameters: V_min=%d, V_max=%d, E_min=%d, E_max=%d, E_tol=%d, T_min=%d, T_max=%d\n",
                  V_min, V_max, E_min, E_max, E_tol, T_min, T_max);

    // Initialize the compass
    Wire.begin(8, 9);
    compass.init();
    Serial.println("DEBUG: Compass initialized.");

    // Initialize GPS on Serial2
    Serial2.begin(9600, SERIAL_8N1, 16, 17);
    Serial.println("DEBUG: GPS initialized (Serial2).");

    // Initial heading reading
    compass.read();
    currentHeading = getCorrectedHeading();
    headingCommand = currentHeading;
    Serial.printf("DEBUG: Initial heading = %d\n", currentHeading);

    // Start the WiFi connection
    setupWiFi("ESP32_AP", "password");
    Serial.println("DEBUG: WiFi configured.");
    Serial.println("DEBUG: Setup complete. Awaiting commands...");
}

void loop() {
    // Process incoming UDP commands
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
        int len = udp.read(incomingPacket, 255);
        if (len > 0) {
            incomingPacket[len] = '\0';
        }
        String cmd = String(incomingPacket);
        Serial.printf("DEBUG: UDP command received: %s\n", incomingPacket);
        handleCommand(cmd);
    }

    // Read GPS data
    while (Serial2.available()) {
        char c = Serial2.read();
        gps.encode(c);
    }

    // Update heading (use GPS if valid, otherwise use compass)
    readSensors();

    // Calculate the difference between current heading and target command
    int diff = calculateDifference(currentHeading, headingCommand);
    sendNMEAData(currentHeading, headingCommand, diff, gps);

    // If the motor controller is active, apply the control algorithm
    if (motorControllerState) {
        int velocita_correzione = calcola_velocita_e_verso(currentHeading, headingCommand);
        gestisci_attuatore(velocita_correzione);
    } else {
        // In manual mode (MC OFF), if no manual command has been sent (already handled in handleCommand)
        stopMotor();
    }

    // If calibration is in progress
    if (calibrationMode) {
        performCalibration(millis());
    }

    delay(1000);
}
