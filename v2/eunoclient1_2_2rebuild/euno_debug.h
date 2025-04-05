#ifndef EUNO_DEBUG_H
#define EUNO_DEBUG_H

#include <WiFiUdp.h>
#include <Arduino.h>

// Queste variabili devono essere dichiarate nel tuo sketch principale
extern WiFiUDP udp;
extern IPAddress serverIP;
extern unsigned int serverPort;

// Funzione principale per loggare messaggi da AP o Client
inline void debugLog(String msg) {
  Serial.println(msg);
  
  // Determina se il messaggio proviene dal CLIENT o dall'AP
  #ifdef IS_CLIENT
    String fullMsg = "LOG: [CLIENT] " + msg;
  #else
    String fullMsg = "LOG: [AP] " + msg;
  #endif

  udp.beginPacket(serverIP, serverPort);
  udp.write((const uint8_t*) fullMsg.c_str(), fullMsg.length());
  udp.endPacket();
}

// Alias opzionale
inline void sendLog(String msg) {
  debugLog(msg);
}

#endif
