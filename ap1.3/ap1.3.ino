#include <WiFi.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>
#include <string.h>  // Per strcmp

// Helper per troncare una stringa
String truncateString(String s, int maxChars) {
  return s.length() > maxChars ? s.substring(0, maxChars - 3) + "..." : s;
}

// ── BITMAP PER L'ICONA SETTINGS (32x32px)
// (Attualmente non usata, poiché il tasto "Imp" mostra la scritta "Imp")
const unsigned char epd_bitmap_16 [] PROGMEM = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

// ── CONFIGURAZIONE DI BASE ──
const char* ssid = "ESP32_AP";
const char* password = "password";
IPAddress apIP(192,168,4,1);

WiFiUDP udp;
unsigned int localUdpPort = 4210;
char incomingPacket[255];

IPAddress clientIP(192,168,4,2);
unsigned int clientPort = 4210;

TFT_eSPI tft = TFT_eSPI();
const int staticAreaHeight = 120;
bool motorControllerState = false;

bool externalBearingEnabled = false;
int lastValidExtBearing = -1;
unsigned long lastExtBearingTime = 0;
const unsigned long EXT_BRG_TIMEOUT = 5000;

// ── TOUCHSCREEN ──
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// ── MENU ──
int menuMode = 0; // 0 = principale, 1 = secondario, 2 = impostazioni

// Menu principale: ordine:
// Row 1: "-1", "+1", "ON/OFF"
// Row 2: "-10", "+10", "MENU"
#define NUM_MAIN_BUTTONS 6
const char* mainButtonLabels[NUM_MAIN_BUTTONS] = { "-1", "+1", "ON/OFF", "-10", "+10", "MENU" };
const char* mainButtonActions[NUM_MAIN_BUTTONS] = { "ACTION:-1", "ACTION:+1", "ACTION:TOGGLE", "ACTION:-10", "ACTION:+10", "MENU:SWITCH" };
uint16_t buttonColorsMain[NUM_MAIN_BUTTONS] = { TFT_BLUE, TFT_BLUE, TFT_ORANGE, TFT_BLUE, TFT_BLUE, TFT_ORANGE };

// Menu secondario: 6 pulsanti: {"CALIB", "GPS", "C-GPS", "Imp", "EXTBRG", "MENU"}
// I tasti CALIB, GPS, C-GPS, EXTBRG in TFT_YELLOW; "Imp" e "MENU" in TFT_BLUE.
#define NUM_SECOND_BUTTONS 6
const char* secondButtonLabels[NUM_SECOND_BUTTONS] = { "CALIB", "GPS", "C-GPS", "Imp", "EXTBRG", "MENU" };
const char* secondButtonActions[NUM_SECOND_BUTTONS] = { "ACTION:CAL", "ACTION:GPS", "ACTION:C-GPS", "IMP", "ACTION:EXT_BRG", "MENU:SWITCH" };
uint16_t buttonColorsSecond[NUM_SECOND_BUTTONS] = { TFT_YELLOW, TFT_YELLOW, TFT_YELLOW, TFT_BLUE, TFT_YELLOW, TFT_BLUE };

// ── Impostazioni (menuMode==2) ──
struct Parameter { const char* name; int value; int minVal; int maxVal; };
#define NUM_PARAMS 7
Parameter params[NUM_PARAMS] = {
  { "V_min", 100, 0, 255 },
  { "V_max", 255, 0, 255 },
  { "E_min", 5, 0, 100 },
  { "E_max", 40, 0, 100 },
  { "E_tolleranza", 1, 0, 10 },
  { "T.S.min", 4, 0, 20 },
  { "T.S.max", 10, 0, 20 }
};
int currentParamIndex = 0;
const char* paramDescriptions[NUM_PARAMS] = {
  "Min PWM speed", "Max PWM speed", "Min error threshold",
  "Max error threshold", "Error tolerance", "T.S.min", "T.S.max"
};

int sliderX = 10, sliderY = staticAreaHeight + 20, sliderWidth = 320 - 20, sliderHeight = 24; // Slider 1/5 più sottile

// Didascalie per i riquadri info (top area)
// Il box 5 (indice 5) è riservato a "EUNO autopilot" (non ha didascalia esterna)
const char* infoLabels[6] = {"Heading", "Cmd", "Err", "GPS", "Spd", ""};

// ── Debounce ──
const unsigned long touchDebounceDelay = 300;
unsigned long lastTouchTime = 0;

// ── FUNZIONI DI GRAFICA ──

// Disegna l'area statica superiore (riquadri info)
// Nel box 5 viene visualizzato "EUNO" (grande) e "autopilot" (piccolo) centrati, con sfondo verde se MC è on, rosso se off.
void drawStaticLayout() {
  tft.fillRect(0, 0, tft.width(), staticAreaHeight, TFT_BLACK);
  int rows = 2, cols = 3;
  int boxW = tft.width() / cols;
  int boxH = staticAreaHeight / rows;
  for (int i = 0; i < 6; i++){
    int x = (i % cols) * boxW, y = (i / cols) * boxH;
    if(i == 5){
      uint16_t colMC = motorControllerState ? TFT_GREEN : TFT_RED;
      tft.fillRoundRect(x, y, boxW, boxH, 5, colMC);
      tft.drawRoundRect(x, y, boxW, boxH, 5, TFT_WHITE);
      tft.setTextDatum(CC_DATUM);
      tft.setTextSize(3);
      tft.setTextColor(TFT_WHITE, colMC);
      tft.drawString("EUNO", x + boxW/2, y + boxH/2 - 10);
      tft.setTextSize(2);
      tft.drawString("autopilot", x + boxW/2, y + boxH/2 + 10);
    } else {
      tft.fillRoundRect(x, y, boxW, boxH, 5, TFT_BLACK);
      tft.drawRoundRect(x, y, boxW, boxH, 5, TFT_WHITE);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(3);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("N/A", x + boxW/2, y + boxH/2);
    }
    if(i != 5) {
      tft.setTextDatum(TL_DATUM);
      tft.setTextSize(1);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.drawString(infoLabels[i], x + 2, y + 2);
    }
  }
}

// Aggiorna il contenuto di un riquadro info
void updateDataBox(int index, String value) {
  int rows = 2, cols = 3;
  int boxW = tft.width() / cols;
  int boxH = staticAreaHeight / rows;
  int x = (index % cols) * boxW;
  int y = (index / cols) * boxH;
  if(index < 5){
    tft.fillRoundRect(x, y, boxW, boxH, 5, TFT_BLACK);
    tft.drawRoundRect(x, y, boxW, boxH, 5, TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(value, x + boxW/2, y + boxH/2);
  } else {
    uint16_t colMC = motorControllerState ? TFT_GREEN : TFT_RED;
    tft.fillRoundRect(x, y, boxW, boxH, 5, colMC);
    tft.drawRoundRect(x, y, boxW, boxH, 5, TFT_WHITE);
    tft.setTextDatum(CC_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE, colMC);
    tft.drawString("EUNO", x + boxW/2, y + boxH/2 - 10);
    tft.setTextSize(2);
    tft.drawString("autopilot", x + boxW/2, y + boxH/2 + 10);
  }
  if(index != 5) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(infoLabels[index], x + 2, y + 2);
  }
}

// Disegna il menu
void drawMenu() {
  tft.fillRect(0, staticAreaHeight, tft.width(), tft.height()-staticAreaHeight, TFT_BLACK);
  if(menuMode == 0) {
    // Menu principale:
    // Row 1: "-1", "+1", "ON/OFF"
    // Row 2: "-10", "+10", "MENU"
    int rows = 2, cols = 3;
    int btnW = tft.width() / cols;
    int btnH = (tft.height() - staticAreaHeight) / rows;
    for (int i = 0; i < NUM_MAIN_BUTTONS; i++){
      int x = (i % cols) * btnW;
      int y = staticAreaHeight + (i / cols) * btnH;
      tft.fillRoundRect(x, y, btnW, btnH, 10, buttonColorsMain[i]);
      tft.drawRoundRect(x, y, btnW, btnH, 10, TFT_WHITE);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(3);
      tft.setTextColor(TFT_BLACK, buttonColorsMain[i]);
      tft.drawString(mainButtonLabels[i], x + btnW/2 + 1, y + btnH/2 + 1);
      tft.setTextColor(TFT_WHITE, buttonColorsMain[i]);
      tft.drawString(mainButtonLabels[i], x + btnW/2, y + btnH/2);
    }
  } else if(menuMode == 1) {
    // Menu secondario: 6 pulsanti: {"CALIB", "GPS", "C-GPS", "Imp", "EXTBRG", "MENU"}
    int btnCount = NUM_SECOND_BUTTONS;
    int rowCount = 2;
    int btnPerRow = btnCount / rowCount; // 3 per riga
    int btnH = (tft.height() - staticAreaHeight) / rowCount;
    int btnW = tft.width() / btnPerRow;
    for (int i = 0; i < btnCount; i++){
      int x = (i % btnPerRow) * btnW;
      int y = staticAreaHeight + (i / btnPerRow) * btnH;
      tft.fillRoundRect(x, y, btnW, btnH, 10, buttonColorsSecond[i]);
      tft.drawRoundRect(x, y, btnW, btnH, 10, TFT_WHITE);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(3);
      tft.setTextColor(TFT_WHITE, buttonColorsSecond[i]);
      tft.drawString(secondButtonLabels[i], x + btnW/2, y + btnH/2);
    }
  } else if(menuMode == 2) {
    // Impostazioni: dividi l'area in 2 sezioni (superiore 55%, inferiore 45%)
    int settingsH = tft.height() - staticAreaHeight;
    int upperH = (settingsH * 55) / 100;
    int lowerY = staticAreaHeight + upperH;
    tft.drawFastHLine(0, lowerY, tft.width(), TFT_WHITE);
    // Sezione superiore: 2 colonne (sinistra 2/3, destra 1/3)
    int leftW = (2 * tft.width()) / 3;
    int rightW = tft.width() - leftW;
    String paramName = String(params[currentParamIndex].name);
    String shortName = (paramName == "E_tolleranza") ? "E.T." : truncateString(paramName, 10);
    String headerStr = shortName + ": " + String(params[currentParamIndex].value);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(headerStr, 10, staticAreaHeight + 20);
    tft.setTextSize(2);
    tft.drawString(paramDescriptions[currentParamIndex], 10, staticAreaHeight + 50);
    // Colonna destra: tasto NEXT come pulsante arrotondato (in blu)
    int nextX = leftW;
    int nextY = staticAreaHeight;
    tft.fillRoundRect(nextX, nextY, rightW, upperH, 10, TFT_BLUE);
    tft.drawRoundRect(nextX, nextY, rightW, upperH, 10, TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE, TFT_BLUE);
    tft.drawString("NEXT", nextX + rightW/2, nextY + upperH/2);
    // Sezione inferiore: slider che parte subito sotto NEXT e si estende fino al fondo
    int sliderY_local = lowerY;  // Inizia subito sotto NEXT
    int sliderMargin = 20;       // Solo margine orizzontale
    int sliderHeight_calc = tft.height() - lowerY; // Estende fino al fondo
    tft.drawRect(sliderX - sliderMargin, sliderY_local, sliderWidth + 2 * sliderMargin, sliderHeight_calc, TFT_WHITE);
    float ratio = (float)(params[currentParamIndex].value - params[currentParamIndex].minVal) /
                  (params[currentParamIndex].maxVal - params[currentParamIndex].minVal);
    int fillWidth = ratio * sliderWidth;
    tft.fillRect(sliderX - sliderMargin, sliderY_local, fillWidth + 2 * sliderMargin, sliderHeight_calc, TFT_GREEN);
  }
}

// ── Parsing functions per messaggi NMEA ──
int getValue(String nmea, String field) {
  int start = nmea.indexOf(field + "=");
  if(start == -1) return -1;
  int end = nmea.indexOf(",", start);
  return nmea.substring(start + field.length() + 1, end).toInt();
}
String getStringValue(String nmea, String field) {
  int start = nmea.indexOf(field + "=");
  if(start == -1) return "N/A";
  int end = nmea.indexOf(",", start);
  return nmea.substring(start + field.length() + 1, end);
}
String getFieldNMEA(String nmea, int index) {
  int commaCount = 0, startPos = 0;
  for (int i = 0; i < nmea.length(); i++){
    if(nmea[i] == ','){
      if(commaCount == index) return nmea.substring(startPos, i);
      commaCount++;
      startPos = i + 1;
    }
  }
  return nmea.substring(startPos);
}
void parseNMEA(String nmea) {
  if(nmea.startsWith("$AUTOPILOT,")) {
    int heading = getValue(nmea, "HEADING");
    int command = getValue(nmea, "COMMAND");
    int error = getValue(nmea, "ERROR");
    String gpsHeading = getStringValue(nmea, "GPS_HEADING");
    String gpsSpeed = getStringValue(nmea, "GPS_SPEED");
    updateDataBox(0, String(heading));
    if(!externalBearingEnabled) updateDataBox(1, String(command));
    updateDataBox(2, String(error));
    updateDataBox(3, gpsHeading);
    updateDataBox(4, gpsSpeed);
    updateDataBox(5, ""); // Il box 5 mostra "EUNO autopilot"
  }
  else if(nmea.startsWith("$GPRMB")) {
    if(externalBearingEnabled) {
      Serial.println("DEBUG(Display): Modalità bearing esterno attiva. Elaboro $GPRMB.");
      String bearingStr = getFieldNMEA(nmea, 11);
      int starPos = bearingStr.indexOf('*');
      if(starPos != -1) bearingStr = bearingStr.substring(0, starPos);
      Serial.println("DEBUG(Display): Bearing estratto da $GPRMB: " + bearingStr);
      int roundedBearing = round(bearingStr.toFloat());
      Serial.printf("DEBUG(Display): Bearing arrotondato: %d\n", roundedBearing);
      if(roundedBearing >= 0 && roundedBearing < 360) {
        lastValidExtBearing = roundedBearing;
        lastExtBearingTime = millis();
        updateDataBox(1, String(lastValidExtBearing));
        String cmdData = "CMD:" + String(lastValidExtBearing);
        udp.beginPacket(clientIP, clientPort);
        udp.print(cmdData);
        udp.endPacket();
        Serial.println("DEBUG(Display): Inviato comando -> " + cmdData);
      }
      else {
        Serial.println("DEBUG(Display): Bearing non valido: " + String(roundedBearing));
      }
    }
  }
}

// ── Gestione touchscreen (debounce e macchina a stati) ──
enum ButtonActionState { BAS_IDLE, BAS_HIGHLIGHT, BAS_ACTION_SENT };
ButtonActionState buttonActionState = BAS_IDLE;
unsigned long buttonActionTimestamp = 0;
String pendingAction = "";
int pendingButtonType = 0; // 0 = main, 1 = secondary, 2 = NEXT in settings

void processPendingAction() {
  if(menuMode == 2 && pendingButtonType == 2 && pendingAction == "NEXT") {
    String udpCmd = "SET:" + String(params[currentParamIndex].name) + "=" + String(params[currentParamIndex].value);
    udp.beginPacket(clientIP, clientPort);
    udp.print(udpCmd);
    udp.endPacket();
    Serial.println("DEBUG(Display): SET param inviato -> " + udpCmd);
    currentParamIndex++;
    if(currentParamIndex >= NUM_PARAMS) menuMode = 0;
  }
  else {
    if(pendingAction == "MENU:SWITCH") menuMode = (menuMode == 0) ? 1 : 0;
    else if(pendingAction == "ACTION:TOGGLE") {
      motorControllerState = !motorControllerState;
      updateDataBox(5, "");
      udp.beginPacket(clientIP, clientPort);
      udp.print("ACTION:TOGGLE");
      udp.endPacket();
      Serial.println("DEBUG(Display): Inviato comando -> ACTION:TOGGLE");
    }
    else if(pendingAction == "IMP") { 
      menuMode = 2; 
      currentParamIndex = 0; 
    }
    else {
      udp.beginPacket(clientIP, clientPort);
      udp.print(pendingAction);
      udp.endPacket();
      Serial.println("DEBUG(Display): Inviato comando -> " + pendingAction);
    }
  }
}

void checkTouch() {
  if(buttonActionState == BAS_HIGHLIGHT && (millis() - buttonActionTimestamp >= 100)) {
    processPendingAction();
    buttonActionState = BAS_ACTION_SENT;
    buttonActionTimestamp = millis();
  }
  else if(buttonActionState == BAS_ACTION_SENT && (millis() - buttonActionTimestamp >= 200)) {
    drawMenu();
    buttonActionState = BAS_IDLE;
  }
  
  if(buttonActionState != BAS_IDLE) return;
  
  if(touchscreen.tirqTouched() && touchscreen.touched()){
    TS_Point p = touchscreen.getPoint();
    if(p.z < 50 || p.x < 100 || p.y < 100) return;
    
    if(millis() - lastTouchTime < touchDebounceDelay) return;
    lastTouchTime = millis();
    
    int x = map(p.x, 200, 3700, 0, tft.width());
    int y = map(p.y, 240, 3800, 0, tft.height());
    Serial.printf("TOUCH(Display): x=%d, y=%d\n", x, y);
    
    if(menuMode == 0 || menuMode == 1) {
      if(menuMode == 0) {
        int rows = 2, cols = 3;
        int btnW = tft.width() / cols;
        int btnH = (tft.height() - staticAreaHeight) / rows;
        for(int i = 0; i < NUM_MAIN_BUTTONS; i++){
          int bx = (i % cols) * btnW;
          int by = staticAreaHeight + (i / cols) * btnH;
          if(x >= bx && x <= bx + btnW && y >= by && y <= by + btnH){
            tft.fillRoundRect(bx, by, btnW, btnH, 10, TFT_WHITE);
            delay(100);
            pendingAction = mainButtonActions[i];
            pendingButtonType = 0;
            buttonActionState = BAS_HIGHLIGHT;
            buttonActionTimestamp = millis();
            break;
          }
        }
      } else if(menuMode == 1) {
        int row1Count = 3;
        int row1H = (tft.height() - staticAreaHeight) / 2;
        int btnW1 = tft.width() / row1Count;
        bool found = false;
        for (int i = 0; i < row1Count; i++){
          int bx = i * btnW1, by = staticAreaHeight;
          if(x >= bx && x <= bx + btnW1 && y >= by && y <= by + row1H){
            tft.fillRoundRect(bx, by, btnW1, row1H, 10, TFT_WHITE);
            delay(100);
            pendingAction = secondButtonActions[i];
            pendingButtonType = 1;
            buttonActionState = BAS_HIGHLIGHT;
            buttonActionTimestamp = millis();
            found = true;
            break;
          }
        }
        if(!found) {
          int row2Count = NUM_SECOND_BUTTONS - row1Count;
          int btnW2 = tft.width() / row2Count;
          int by = staticAreaHeight + row1H;
          for (int i = 0; i < row2Count; i++){
            int bx = i * btnW2;
            if(x >= bx && x <= bx + btnW2 && y >= by && y <= by + row1H){
              tft.fillRoundRect(bx, by, btnW2, row1H, 10, TFT_WHITE);
              delay(100);
              pendingAction = secondButtonActions[row1Count + i];
              pendingButtonType = 1;
              buttonActionState = BAS_HIGHLIGHT;
              buttonActionTimestamp = millis();
              break;
            }
          }
        }
      }
    } else if(menuMode == 2) {
      int settingsH = tft.height() - staticAreaHeight;
      int upperH = (settingsH * 55) / 100;
      int lowerY = staticAreaHeight + upperH;
      int leftW = (2 * tft.width()) / 3;
      if(y >= staticAreaHeight && y < lowerY) {
        if(x >= leftW) {
          pendingAction = "NEXT";
          pendingButtonType = 2;
          buttonActionState = BAS_HIGHLIGHT;
          buttonActionTimestamp = millis();
        }
      }
      else if(y >= lowerY) {
        // La barra slider: utilizziamo la stessa regione disegnata in drawMenu()
        int sliderY_local = lowerY;  
        int sliderMargin = 20;       // Solo margine orizzontale
        int sliderHeight_calc = tft.height() - lowerY; // Regione slider: dal termine NEXT al fondo
        if(x >= sliderX - sliderMargin && x <= sliderX + sliderWidth + sliderMargin &&
           y >= sliderY_local && y <= sliderY_local + sliderHeight_calc) {
          float ratio = (float)(x - sliderX) / sliderWidth;
          int newVal = params[currentParamIndex].minVal + ratio * (params[currentParamIndex].maxVal - params[currentParamIndex].minVal);
          params[currentParamIndex].value = newVal;
          drawMenu();
        }
      }
    }
  }
}

// ── SETUP & LOOP ──
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  
  delay(100);
  
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  if(!touchscreen.begin(touchscreenSPI)) {
    Serial.println("DEBUG: Touchscreen init ERROR!");
  } else {
    Serial.println("DEBUG: Touchscreen init SUCCESS.");
  }
  touchscreen.setRotation(1);
  
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255,255,255,0));
  IPAddress IP = WiFi.softAPIP();
  Serial.print("DEBUG(Display): AP started! IP = ");
  Serial.println(IP);
  
  udp.begin(localUdpPort);
  Serial.printf("DEBUG(Display): UDP server on port %d\n", localUdpPort);
  
  drawStaticLayout();
  drawMenu();
}

void loop() {
  checkTouch();
  
  if(externalBearingEnabled) {
    unsigned long now = millis();
    if(now - lastExtBearingTime > EXT_BRG_TIMEOUT) {
      updateDataBox(1, "NP");
    } else {
      if(lastValidExtBearing != -1) {
        String cmdData = "CMD:" + String(lastValidExtBearing);
        udp.beginPacket(clientIP, clientPort);
        udp.print(cmdData);
        udp.endPacket();
        Serial.println("DEBUG(Display): Inviato comando (ciclo): " + cmdData);
      }
    }
  }
  
  int packetSize = udp.parsePacket();
  if(packetSize) {
    clientIP = udp.remoteIP();
    clientPort = udp.remotePort();
    int len = udp.read(incomingPacket, 255);
    if(len > 0) incomingPacket[len] = 0;
    String msg = String(incomingPacket);
    parseNMEA(msg);
  }
}
