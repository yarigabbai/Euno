#include "websocket_ota.h"
#include <Arduino.h>
#include <Update.h>
#include "index_html.h"
#include "nmea_parser.h"
#include "tft_touch.h"

#define EUNO_IS_AP
#include "euno_debugAP.h"

// Dichiarazioni extern per variabili definite nel file principale
extern Parameter params[NUM_PARAMS];
extern WebSocketsServer webSocket;
extern WiFiUDP udp;

// Variabili per la gestione OTA
IPAddress serverIP;
unsigned int serverPort;
bool otaForClientInProgress = false;
IPAddress otaClientIP;
unsigned int otaClientPort;
size_t otaReceived = 0;
uint32_t otaTotalSize = 0;
unsigned long otaStartTime = 0;

// Costanti per l'OTA
#define OTA_CHUNK_SIZE 1024
#define OTA_DELAY_MS 20
#define OTA_TIMEOUT 300000 // 5 minuti timeout

void forwardOtaToClient(uint8_t *payload, size_t length) {
    if(length > 0 && otaForClientInProgress) {
        udp.beginPacket(otaClientIP, otaClientPort);
        udp.write(payload, length);
        udp.endPacket();
        delay(OTA_DELAY_MS); // Ritardo tra i pacchetti
    }
}

void initWebServers(WebServer &server, WebSocketsServer &webSocket) {
    server.on("/", HTTP_GET, [&server]() {
        server.send_P(200, "text/html", INDEX_HTML);
    });

    server.on("/update", HTTP_GET, [&server]() {
        handleUpdate(server);
    });
    
    server.on("/update", HTTP_POST, [&server]() {
        handleUpdate(server);
    }, [&server]() {
        handleUploadChunk(server);
    });

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
    static uint32_t otaSize = 0;
    static uint32_t otaReceived = 0;
    static bool otaInProgress = false;

    if (type == WStype_TEXT) {
        String msg = String((char*)payload, length);
        
        if (msg.startsWith("$OTA_AP:")) {
            otaSize = msg.substring(8).toInt();
            otaReceived = 0;
            otaInProgress = true;
            
            if (!Update.begin(otaSize, U_FLASH)) {
                Update.printError(Serial);
                webSocket.sendTXT(clientNum, "OTA_ERROR:Begin failed");
                return;
            }
            webSocket.sendTXT(clientNum, "OTA_PROGRESS:0");
        }
        else if (msg.startsWith("$OTA_CLIENT:")) {
            otaForClientInProgress = true;
            otaClientIP = webSocket.remoteIP(clientNum);
            otaClientPort = serverPort;
            
            String cmd = "OTA_START:" + msg.substring(12);
            udp.beginPacket(otaClientIP, otaClientPort);
            udp.write((const uint8_t*)cmd.c_str(), cmd.length());
            udp.endPacket();
            
            webSocket.sendTXT(clientNum, "OTA_CLIENT_STARTED");
        }
        else if (msg == "OTA_END") {
            if (otaForClientInProgress) {
                String cmd = "OTA_END";
                udp.beginPacket(otaClientIP, otaClientPort);
                udp.write((const uint8_t*)cmd.c_str(), cmd.length());
                udp.endPacket();
                otaForClientInProgress = false;
            } else {
                if (Update.end(true)) {
                    webSocket.sendTXT(clientNum, "OTA_PROGRESS:100");
                    webSocket.sendTXT(clientNum, "OTA_COMPLETE_AP");
                    delay(1000);
                    ESP.restart();
                } else {
                    Update.printError(Serial);
                    webSocket.sendTXT(clientNum, "OTA_ERROR:End failed");
                }
            }
        }
        else if (msg.startsWith("SET:")) {
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
        else {
            handleCommandAP(msg);
        }
    }
 else if (type == WStype_BIN) {
        if (otaInProgress) {
            size_t bytesWritten = Update.write(payload, length);
            if (bytesWritten != length) {
                Update.printError(Serial);
                webSocket.sendTXT(clientNum, "OTA_ERROR:Write failed");
                otaInProgress = false;
                return;
            }

            otaReceived += length;
            
            // Invia progresso
            int progress = (otaReceived * 100) / otaSize;
            static int lastProgress = 0;
            if (progress >= lastProgress + 5 || progress == 100) {
                webSocket.sendTXT(clientNum, "OTA_PROGRESS:" + String(progress));
                lastProgress = progress;
            }

            // Se completato
            if (otaReceived >= otaSize) {
                if (Update.end(true)) {
                    webSocket.sendTXT(clientNum, "OTA_COMPLETE");
                    delay(1000);
                    ESP.restart();
                } else {
                    Update.printError(Serial);
                    webSocket.sendTXT(clientNum, "OTA_ERROR:End failed");
                }
                otaInProgress = false;
            }
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
            delay(1000);
            ESP.restart();
        }
    }
}

void handleUploadChunk(WebServer &server) {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        debugLog("OTA AP Start: " + upload.filename);
        otaReceived = 0;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
        otaReceived += upload.currentSize;
    }
    else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            debugLog("OTA AP Success: " + String(upload.totalSize) + " bytes");
            webSocket.broadcastTXT("OTA_COMPLETE_AP");
            delay(1000);
            ESP.restart();
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
        otaReceived = 0;
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
    // Invia direttamente via UDP al client OTA
    forwardOtaToClient(upload.buf, upload.currentSize);
    otaReceived += upload.currentSize;
}

    else if (upload.status == UPLOAD_FILE_END) {
        String cmd = "OTA_END:" + String(upload.totalSize);
        webSocket.broadcastTXT(cmd);
        debugLog("OTA Client Complete: " + String(upload.totalSize) + " bytes");
        webSocket.broadcastTXT("OTA_COMPLETE_CLIENT");
        delay(1000);
        ESP.restart();
    }
}

void handleOTAData(String msg) {
    // Implementazione esistente
}

void sendOtaStatus(String status, int progress) {
    // Implementazione esistente
}