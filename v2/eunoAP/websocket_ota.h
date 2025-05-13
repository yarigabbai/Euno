
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


#ifndef WEBSOCKET_OTA_H
#define WEBSOCKET_OTA_H

#define OTA_BUFFER_SIZE 1024
#define OTA_TIMEOUT 300000 // 5 minuti timeout
#pragma once

#include "FS.h"
using fs::FS; 

#include <WebServer.h>
#include <WebSocketsServer.h>
#define FW_VERSION "1.2.1"

extern IPAddress serverIP;
extern unsigned int serverPort;
extern WiFiUDP udp;

#include "tft_touch.h"

extern Parameter params[];
extern WebSocketsServer webSocket;

// Variabili per OTA duale
extern bool otaForClientInProgress;
extern IPAddress otaClientIP;
extern unsigned int otaClientPort;
extern size_t otaReceived;

// Prototipi funzioni
void initWebServers(WebServer &server, WebSocketsServer &webSocket);
void onWsEvent(uint8_t clientNum, WStype_t type, uint8_t * payload, size_t length);
void handleUpdate(WebServer &server); 
void handleUploadChunk(WebServer &server);
void handleClientUpdate(WebServer &server);
void handleClientUploadChunk(WebServer &server);
void handleOTAData(String msg);
void sendOtaStatus(String status, int progress = -1);
void forwardOtaToClient(uint8_t *payload, size_t length);

#endif