#include "websocket_ota.h"
#include <Arduino.h>
#include <Update.h>
#include "index_html.h"
#include "nmea_parser.h"
#include "tft_touch.h"

#define EUNO_IS_AP
#include "euno_debugAP.h"

extern Parameter params[];
extern WebSocketsServer webSocket;

size_t otaReceived = 0;

void initWebServers(WebServer &server, WebSocketsServer &webSocket) {
  server.on("/", HTTP_GET, [&server]() {
    server.send_P(200, "text/html", INDEX_HTML);
  });

  // Endpoint OTA AP
  server.on("/update", HTTP_GET, [&server]() {
    handleUpdate(server);
  });
  server.on("/update", HTTP_POST, [&server]() {
    handleUpdate(server);
  }, [&server]() {
    handleUploadChunk(server);
  });

  // Endpoint OTA Client
  server.on("/update_client", HTTP_GET, [&server]() {
    server.send(200, "text/html",
      "<h2>Carica firmware Client</h2>"
      "<form method='POST' action='/update_client' enctype='multipart/form-data'>"
      "<input type='file' name='update'><input type='submit' value='Aggiorna Client'></form>"
      "<div id='otaProgress' style='margin-top:20px;'></div>");
  });
  server.on("/update_client", HTTP_POST, [&server]() {
    handleClientUpdate(server);
  }, [&server]() {
    handleClientUploadChunk(server);
  });

  server.begin();
  debugLog("HTTP server avviato su porta 80 (OTA).");

  webSocket.begin();
  webSocket.onEvent(onWsEvent);
  debugLog("WebSocket server avviato su porta 81.");
}

void onWsEvent(uint8_t clientNum, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_CONNECTED) {
    IPAddress ip = webSocket.remoteIP(clientNum);
    Serial.printf("[WS] Client %u connesso da %s\n", clientNum, ip.toString().c_str());
  }
  else if (type == WStype_DISCONNECTED) {
    Serial.printf("[WS] Client %u disconnesso\n", clientNum);
  }
  else if (type == WStype_TEXT) {
    String msg = String((char*)payload, length);
    Serial.printf("[WS] Client %u -> %s\n", clientNum, msg.c_str());

    if (msg.startsWith("SET:")) {
      handleCommandAP(msg);
      String paramPart = msg.substring(4);
      webSocket.sendTXT(clientNum, "PARAM_UPDATED:" + paramPart);
    }
    else if (msg == "GET_PARAMS") {
      for (int i = 0; i < NUM_PARAMS; i++) {
        String p = "PARAM_UPDATED:" + String(params[i].name) + "=" + String(params[i].value);
        webSocket.sendTXT(clientNum, p);
      }
    }
    else if (msg == "OTA_END") {
      if (Update.end(true)) {
        debugLog("[WS] OTA Client completato con successo.");
        webSocket.sendTXT(clientNum, "OTA_COMPLETE_CLIENT");
        delay(1000);
        ESP.restart();
      } else {
        Update.printError(Serial);
      }
    }
    else {
      handleCommandAP(msg);
    }
  }
  else if (type == WStype_BIN) {
    Serial.printf("[WS] Ricevuti %u byte di OTA Client\n", length);

    if (otaReceived == 0) {
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
        return;
      }
    }

    if (Update.write(payload, length) != length) {
      debugLog("[WS] Errore scrittura OTA Client");
      Update.printError(Serial);
    }

    otaReceived += length;

    if (otaReceived > 0 && otaReceived % 4096 == 0) {
      int perc = (otaReceived * 100) / Update.size();
      String progressMsg = "OTA_PROGRESS:" + String(perc);
      webSocket.sendTXT(clientNum, progressMsg);
    }
  }
}

void handleUpdate(WebServer &server) {
  if (server.method() == HTTP_GET) {
    server.send(200, "text/html",
      "<h2>Caricamento bin via POST su /update</h2>"
      "<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='update'><input type='submit' value='Aggiorna'></form>");
  }
  else if (server.method() == HTTP_POST) {
    bool success = !Update.hasError();
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", success ? "Aggiornamento OK, riavvio..." : "Aggiornamento FALLITO");
    if (success) {
      webSocket.broadcastTXT("OTA_COMPLETE_AP");
    }
    delay(1000);
    ESP.restart();
  }
}

void handleUploadChunk(WebServer &server) {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("OTA AP Start: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("OTA AP Success: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

void handleClientUpdate(WebServer &server) {
  if (server.method() == HTTP_POST) {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "Firmware client ricevuto, inviando...");
    String cmd = "OTA_START:" + String(Update.size());
    webSocket.broadcastTXT(cmd);
  }
}

void handleClientUploadChunk(WebServer &server) {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    debugLog("OTA Client Start");
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    String cmd = "OTA_DATA:";
    cmd += String((char*)upload.buf, upload.currentSize);
    webSocket.broadcastTXT(cmd);
  }
  else if (upload.status == UPLOAD_FILE_END) {
    String cmd = "OTA_END:" + String(upload.totalSize);
    webSocket.broadcastTXT(cmd);
    debugLog("OTA Client Complete");

    webSocket.broadcastTXT("OTA_COMPLETE_CLIENT");
    delay(1000);
    ESP.restart();
   otaReceived = 0;

  }
}
