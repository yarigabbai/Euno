#ifndef NMEA_PARSER_H
#define NMEA_PARSER_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>

// Variabili esterne (globali) definite altrove (in .ino)
extern WiFiUDP udp;
extern IPAddress clientIP;
extern unsigned int clientPort;

extern bool externalBearingEnabled;
extern int lastValidExtBearing;
extern unsigned long lastExtBearingTime;

extern bool motorControllerState;

// Funzioni
void parseNMEA(
  String nmea, 
  TFT_eSPI &tft,
  bool &motorControllerState,
  bool &externalBearingEnabled,
  int &lastValidExtBearing,
  unsigned long &lastExtBearingTime
);

void handleCommandAP(String command);

// Helper
int getValue(String nmea, String field);
String getStringValue(String nmea, String field);
String getFieldNMEA(String nmea, int index);

#endif
