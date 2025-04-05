#pragma once
#ifndef EUNO_DEBUGAP_H
#define EUNO_DEBUGAP_H

#include <WiFiUdp.h>
#include <Arduino.h>

// Variabili globali (dichiarate nel main .ino)
extern WiFiUDP udp;
extern IPAddress clientIP;
extern unsigned int clientPort;

// Funzione semplice per log
inline void debugLog(String msg) {
  Serial.println(msg);  // stampa locale

  String fullMsg = "LOG: [AP] " + msg;  // etichetta AP
  udp.beginPacket(clientIP, clientPort);
  udp.write((const uint8_t*) fullMsg.c_str(), fullMsg.length());
  udp.endPacket();
}

#endif
