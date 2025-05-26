#include "tft_touch.h"
#include <Arduino.h> 
#include "nmea_parser.h"  // per handleCommandAP() se serve in checkTouch

#define NUM_MAIN_BUTTONS 6
#define NUM_SECOND_BUTTONS 6
#define TFT_DARKGREY  0x7BEF
#define TFT_NAVY      0x000F
#define TFT_DARKGREEN 0x03E0
#define TFT_CYAN      0x07FF
#define EUNO_IS_AP
#include "euno_debugAP.h"
String infoLabels[6] = { "Heading", "Cmd", "Err", "GPS", "Spd", "" };
String headingLabel = "H.Compass";  // label attuale per il riquadro 0
extern String headingLabel;         // se serve visibile in più file
extern String lastClientFwVersion;
extern const char* FW_VERSION;
extern unsigned long lastFwRequestTime;
extern char incomingPacket[255];

// -------------------------------------
String truncateString(String s, int maxChars) {
  return s.length() > maxChars ? s.substring(0, maxChars - 3) + "..." : s;
}
void resetAllEEPROM();
// -------------------------------------
void drawStaticLayout(TFT_eSPI &tft, bool motorControllerState, bool externalBearingEnabled)
 {
  tft.fillRect(0, 0, tft.width(), staticAreaHeight, 0x0000 /*TFT_BLACK*/);
  int rows = 2, cols = 3;
  int boxW = tft.width() / cols;
  int boxH = staticAreaHeight / rows;
  for (int i = 0; i < 6; i++){
    int x = (i % cols) * boxW, y = (i / cols) * boxH;
    if(i == 5){
  uint16_t colMC = 0x0000;  // ← NERO fisso
      tft.fillRoundRect(x, y, boxW, boxH, 5, colMC);
      tft.drawRoundRect(x, y, boxW, boxH, 5, 0xFFFF /*TFT_WHITE*/);
      tft.setTextDatum(CC_DATUM);
      tft.setTextSize(3);
      tft.setTextColor(0xFFFF, colMC);
      tft.drawString("EUNO", x + boxW/2, y + boxH/2 - 10);
      tft.setTextSize(2);
      tft.drawString("autopilot", x + boxW/2, y + boxH/2 + 10);
    } else {
      tft.fillRoundRect(x, y, boxW, boxH, 5, 0x0000);
      tft.drawRoundRect(x, y, boxW, boxH, 5, 0xFFFF);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(3);
      tft.setTextColor(0xFFE0, 0x0000);
      tft.drawString("N/A", x + boxW/2, y + boxH/2);
    }
   if(i != 5) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(0x07FF /*TFT_CYAN*/, 0x0000);

  String label = infoLabels[i];
  if(i == 1 && externalBearingEnabled) label = "CMD BRG";

  tft.drawString(label, x + 2, y + 2);
}

  }
}

// -------------------------------------
void updateDataBox(TFT_eSPI &tft, int index, String value) {
  int rows = 2, cols = 3;
  int boxW = tft.width() / cols;
  int boxH = staticAreaHeight / rows;
  int x = (index % cols) * boxW;
  int y = (index / cols) * boxH;
  if(index < 5){
    // Riquadro per heading, cmd, err, GPS, spd
    tft.fillRoundRect(x, y, boxW, boxH, 5, 0x0000);
    tft.drawRoundRect(x, y, boxW, boxH, 5, 0xFFFF);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(0xFFE0, 0x0000);
    tft.drawString(value, x + boxW/2, y + boxH/2);

  // 🧽 Pulisce l’area dove viene scritta la label (in alto a sinistra)
tft.fillRect(x + 2, y + 2, boxW - 4, 14, 0x0000);

// ✍️ Ridisegna la label attuale
tft.setTextDatum(TL_DATUM);
tft.setTextSize(1);
tft.setTextColor(0x07FF, 0x0000);
tft.drawString(infoLabels[index], x + 2, y + 2);

  } else {
    // Riquadro 5: motorControllerState / autopilot on/off
    extern bool motorControllerState; // definito nel .ino
    bool mc = (value == "") ? motorControllerState : (value == "ON");
    uint16_t colMC = mc ? 0x07E0 : 0xFDA0;
    tft.fillRoundRect(x, y, boxW, boxH, 5, colMC);
    tft.drawRoundRect(x, y, boxW, boxH, 5, 0xFFFF);
    tft.setTextDatum(CC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(0xFFFF, colMC);
    tft.drawString("EUNO", x + boxW/2, y + boxH/2 - 10);
    tft.setTextSize(2);
    tft.drawString("autopilot", x + boxW/2, y + boxH/2 + 10);
  }
}
void showFirmwareVersion(TFT_eSPI &tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    int y = tft.height()/2 - 40;
    tft.setCursor(20, y);
    tft.println("AP Firmware:");
    y += 30;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(20, y);
    tft.println(FW_VERSION);

    y += 50;
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, y);
    tft.println("Client Firmware:");
    y += 30;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(20, y);
    tft.println(lastClientFwVersion);

    delay(2000);
}

void updateDataBoxColor(TFT_eSPI &tft, int index, String value, uint16_t color) {
  int boxWidth = 50;
  int x = 5 + index * boxWidth;
  int y = 5;
  tft.fillRect(x, y, boxWidth - 2, 30, TFT_BLACK);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(value, x + 5, y + 5);
}
void drawMenu(TFT_eSPI &tft, int menuMode, Parameter params[], int currentParamIndex, bool motorControllerState) {
  tft.fillRect(0, staticAreaHeight, tft.width(), tft.height() - staticAreaHeight, TFT_BLACK);

  if(menuMode == 0) {
    // Menu principale - 6 pulsanti (3x2)
    const int rows = 2;
    const int cols = 3;
    const int btnSpacing = 8;
    const int btnW = (tft.width() - (cols + 1) * btnSpacing) / cols;
    const int btnH = (tft.height() - staticAreaHeight - (rows + 1) * btnSpacing) / rows;

    for(int i = 0; i < NUM_MAIN_BUTTONS; i++) {
      int row = i / cols;
      int col = i % cols;
      int x = btnSpacing + col * (btnW + btnSpacing);
      int y = staticAreaHeight + btnSpacing + row * (btnH + btnSpacing);
      uint16_t baseColor = buttonColorsMain[i];
      
      // Ombreggiatura
      tft.fillRoundRect(x + 2, y + 2, btnW, btnH, 12, TFT_DARKGREY);
      
      // Corpo pulsante con gradiente
      tft.fillRoundRect(x, y, btnW, btnH, 12, baseColor);
      tft.fillRectVGradient(x + 3, y + 3, btnW - 6, btnH - 6, 
                          tft.color565(min(((baseColor >> 11) & 0x1F) + 5, 31),
                                       min(((baseColor >> 5) & 0x3F) + 5, 63),
                                       min((baseColor & 0x1F) + 5, 31)), 
                          baseColor);
      
      // Bordo
      tft.drawRoundRect(x, y, btnW, btnH, 12, TFT_WHITE);
      
      // Testo
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE);
      tft.drawString(mainButtonLabels[i], x + btnW/2, y + btnH/2);
    }
  }
  else if(menuMode == 1) {
    // Menu secondario - 6 pulsanti (3x2)
    const int rows = 2;
    const int cols = 3;
    const int btnSpacing = 8;
    const int btnW = (tft.width() - (cols + 1) * btnSpacing) / cols;
    const int btnH = (tft.height() - staticAreaHeight - (rows + 1) * btnSpacing) / rows;

    for(int i = 0; i < NUM_SECOND_BUTTONS; i++) {
      int row = i / cols;
      int col = i % cols;
      int x = btnSpacing + col * (btnW + btnSpacing);
      int y = staticAreaHeight + btnSpacing + row * (btnH + btnSpacing);
      uint16_t baseColor = buttonColorsSecond[i];
      
      // Effetto 3D
      tft.fillRoundRect(x + 2, y + 2, btnW, btnH, 10, TFT_DARKGREY); // Ombra
      tft.fillRoundRect(x, y, btnW, btnH, 10, baseColor); // Corpo
      
      // Highlight superiore
      uint16_t highlight = tft.color565(
        min(((baseColor >> 11) & 0x1F) + 10, 31),
        min(((baseColor >> 5) & 0x3F) + 10, 63),
        min((baseColor & 0x1F) + 10, 31)
      );
      tft.fillRect(x + 3, y + 3, btnW - 6, 8, highlight);
      
      // Bordo con effetto metallico
      tft.drawRoundRect(x, y, btnW, btnH, 10, TFT_WHITE);
      tft.drawRoundRect(x + 1, y + 1, btnW - 2, btnH - 2, 8, tft.color565(100, 100, 100));
      
      // Testo - versione per array di const char*
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(strlen(secondButtonLabels[i]) <= 6 ? 2 : 1); // Adatta dimensione testo
      tft.setTextColor(TFT_WHITE);
      tft.drawString(secondButtonLabels[i], x + btnW/2, y + btnH/2);
    }
  }
  else if(menuMode == 2) {
    // Menu parametri - Stile moderno
    const int headerHeight = 50;
    const int sliderHeight = 30;
    
    // Header
    tft.fillRoundRect(0, staticAreaHeight, tft.width(), headerHeight, 5, TFT_NAVY);
    String headerStr = String(params[currentParamIndex].name) + ": " + String(params[currentParamIndex].value);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(headerStr, tft.width()/2, staticAreaHeight + headerHeight/2);
    
    // Pulsante NEXT
    const int nextBtnWidth = 80;
    tft.fillRoundRect(tft.width() - nextBtnWidth - 10, staticAreaHeight + 5, nextBtnWidth, headerHeight - 10, 5, TFT_DARKGREEN);
    tft.drawRoundRect(tft.width() - nextBtnWidth - 10, staticAreaHeight + 5, nextBtnWidth, headerHeight - 10, 5, TFT_WHITE);
    tft.drawString("NEXT", tft.width() - nextBtnWidth/2 - 10, staticAreaHeight + headerHeight/2);
    
    // Slider
    int sliderY = staticAreaHeight + headerHeight + 20;
    float ratio = (float)(params[currentParamIndex].value - params[currentParamIndex].minValue) / 
                 (params[currentParamIndex].maxValue - params[currentParamIndex].minValue);
    int sliderPos = sliderX + (sliderWidth * ratio);
    
    // Track
    tft.fillRoundRect(sliderX, sliderY + sliderHeight/2 - 4, sliderWidth, 8, 4, TFT_DARKGREY);
    tft.fillRoundRect(sliderX, sliderY + sliderHeight/2 - 4, sliderPos - sliderX, 8, 4, TFT_CYAN);
    
    // Thumb
    tft.fillCircle(sliderPos, sliderY + sliderHeight/2, 10, TFT_WHITE);
    tft.drawCircle(sliderPos, sliderY + sliderHeight/2, 10, TFT_DARKGREY);
    tft.fillCircle(sliderPos, sliderY + sliderHeight/2, 6, TFT_CYAN);
  }

else if(menuMode == 3) {
  // Terzo menu (2×3)
  const int rows = 2, cols = 3, spacing = 8;
  int btnW = (tft.width()  - (cols+1)*spacing) / cols;
  int btnH = (tft.height() - staticAreaHeight - (rows+1)*spacing) / rows;
  for(int i = 0; i < NUM_THIRD_BUTTONS; i++) {
    int row = i/cols;
    int col = i%cols;
    int x = spacing + col*(btnW + spacing);
    int y = staticAreaHeight + spacing + row*(btnH + spacing);
    uint16_t base = buttonColorsThird[i];
    // Ombra
    tft.fillRoundRect(x+2, y+2, btnW, btnH, 12, TFT_DARKGREY);
    // Corpo
    tft.fillRoundRect(x, y, btnW, btnH, 12, base);
    // Bordo
    tft.drawRoundRect(x, y, btnW, btnH, 12, TFT_WHITE);
    // Etichetta
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(thirdButtonLabels[i], x + btnW/2, y + btnH/2);
  }
}
}
// -------------------------------------
void updateMainButtonONOFF(TFT_eSPI &tft, bool isOn) {
  const int index = 2;  // ON/OFF è il terzo tasto (in alto a destra)
  const int cols = 3;
  const int rows = 2;
  const int btnSpacing = 8;
  const int btnW = (tft.width() - (cols + 1) * btnSpacing) / cols;
  const int btnH = (tft.height() - staticAreaHeight - (rows + 1) * btnSpacing) / rows;
  int col = index % cols;
  int row = index / cols;
  int x = btnSpacing + col * (btnW + btnSpacing);
  int y = staticAreaHeight + btnSpacing + row * (btnH + btnSpacing);

  uint16_t color = isOn ? TFT_GREEN : TFT_RED;
  String label = isOn ? "ON" : "OFF";

  tft.fillRoundRect(x, y, btnW, btnH, 12, color);
  tft.drawRoundRect(x, y, btnW, btnH, 12, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, color);
  tft.drawString(label, x + btnW / 2, y + btnH / 2);
}

void requestClientFirmwareVersion() {
    udp.beginPacket(clientIP, clientPort);
    udp.print("GET_FW_VERSION");
    udp.endPacket();
    lastClientFwVersion = "Attendi...";
    lastFwRequestTime = millis();
}

void checkTouch(
  TFT_eSPI &tft,
  XPT2046_Touchscreen &touchscreen,
  int &menuMode,
  bool &motorControllerState,
  int &currentParamIndex,
  Parameter params[],
  String &pendingAction,
  int &pendingButtonType,
  ButtonActionState &buttonActionState,
  unsigned long &buttonActionTimestamp,
  unsigned long &lastTouchTime
) {
  // Variabili per NEXT e slider
  int settingsH = tft.height() - staticAreaHeight;
  int upperH = (settingsH * 55) / 100;
  int lowerY = staticAreaHeight + upperH;
  int leftW = (2 * tft.width()) / 3;
  int rightW = tft.width() - leftW;

  if (buttonActionState == BAS_HIGHLIGHT && (millis() - buttonActionTimestamp >= 100)) {
    // Esegui l’azione pendente
    if (menuMode == 2 && pendingButtonType == 2 && pendingAction == "NEXT") {
      String udpCmd = "SET:" + String(params[currentParamIndex].name) + "=" + String(params[currentParamIndex].value);
      handleCommandAP(udpCmd);
      currentParamIndex++;
      if (currentParamIndex >= NUM_PARAMS) menuMode = 0;
    } else {
      if (pendingAction == "MENU:SWITCH") {
        // Ciclo 0 → 1 → 3 → 0
if      (menuMode == 0) menuMode = 1;
else if (menuMode == 1) menuMode = 3;
else                     menuMode = 0;

      }
     else if (pendingAction == "ACTION:TOGGLE") {
  handleCommandAP("ACTION:TOGGLE");
  // Aspettiamo la conferma reale dal client
}
      else if (pendingAction == "IMP") {
        menuMode = 2;
        currentParamIndex = 0;
      }
      else if (pendingAction == "ACTION:EXT_BRG") {
        externalBearingEnabled = !externalBearingEnabled;
        String cmdToClient = externalBearingEnabled ? "EXT_BRG_ENABLED" : "EXT_BRG_DISABLED";
        udp.beginPacket(clientIP, clientPort);
        udp.print(cmdToClient);
        udp.endPacket();
        updateDataBoxColor(tft, 1, externalBearingEnabled ? "ON" : "OFF", externalBearingEnabled ? TFT_GREEN : TFT_RED);
      }
      else {
        handleCommandAP(pendingAction);  // Per +1, -1, +10, -10, GPS, C-GPS
      }
    }

    buttonActionState = BAS_ACTION_SENT;
    buttonActionTimestamp = millis();
  }

  else if (buttonActionState == BAS_ACTION_SENT && (millis() - buttonActionTimestamp >= 200)) {
    drawMenu(tft, menuMode, params, currentParamIndex, motorControllerState);
    buttonActionState = BAS_IDLE;
  }

  if (buttonActionState != BAS_IDLE) return;

  // Gestione tocco
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    if (p.z < 20 || p.x < 100 || p.y < 100) return;
    if (millis() - lastTouchTime < touchDebounceDelay) return;
    lastTouchTime = millis();

    int x = map(p.x, 200, 3700, 0, tft.width());
    int y = map(p.y, 240, 3800, 0, tft.height());
    //Serial.printf("TOUCH(Display): x=%d, y=%d\n", x, y);

    if (menuMode == 0) {
      int rows = 2, cols = 3;
      int btnW = tft.width() / cols;
      int btnH = (tft.height() - staticAreaHeight) / rows;
      for (int i = 0; i < NUM_MAIN_BUTTONS; i++) {
        int bx = (i % cols) * btnW;
        int by = staticAreaHeight + (i / cols) * btnH;
        if (x >= bx && x <= bx + btnW && y >= by && y <= by + btnH) {
          tft.fillRoundRect(bx, by, btnW, btnH, 10, 0xFFFF);
          //delay(100);
          pendingAction = mainButtonActions[i];
          pendingButtonType = 0;
          buttonActionState = BAS_HIGHLIGHT;
          buttonActionTimestamp = millis();
          break;
        }
      }
    }

    else if (menuMode == 1) {
      int row1Count = 3;
      int row1H = (tft.height() - staticAreaHeight) / 2;
      int btnW1 = tft.width() / row1Count;
      bool found = false;

      for (int i = 0; i < row1Count; i++) {
        int bx = i * btnW1;
        int by = staticAreaHeight;
        if (x >= bx && x <= bx + btnW1 && y >= by && y <= by + row1H) {
          tft.fillRoundRect(bx, by, btnW1, row1H, 10, 0xFFFF);
          //delay(100);
          pendingAction = secondButtonActions[i];
          pendingButtonType = 1;
          buttonActionState = BAS_HIGHLIGHT;
          buttonActionTimestamp = millis();
          found = true;
          break;
        }
      }

      if (!found) {
        int row2Count = NUM_SECOND_BUTTONS - row1Count;
        int btnW2 = tft.width() / row2Count;
        int by = staticAreaHeight + row1H;
        for (int i = 0; i < row2Count; i++) {
          int bx = i * btnW2;
          if (x >= bx && x <= bx + btnW2 && y >= by && y <= by + row1H) {
            tft.fillRoundRect(bx, by, btnW2, row1H, 10, 0xFFFF);
            //delay(100);
            pendingAction = secondButtonActions[row1Count + i];
            pendingButtonType = 1;
            buttonActionState = BAS_HIGHLIGHT;
            buttonActionTimestamp = millis();
            break;
          }
        }
      }
    }

    else if (menuMode == 2) {
      // NEXT
      if (y >= staticAreaHeight && y < lowerY) {
        if (x >= leftW - 10 && x <= leftW + rightW + 10) {
          pendingAction = "NEXT";
          pendingButtonType = 2;
          buttonActionState = BAS_HIGHLIGHT;
          buttonActionTimestamp = millis();
        }
      }
      // SLIDER
      else if (y >= lowerY) {
        int sliderY_local = lowerY;
        int sliderMargin = 20;
        int sliderHeight_calc = tft.height() - lowerY;
        if (x >= sliderX - sliderMargin && x <= sliderX + sliderWidth + sliderMargin &&
            y >= sliderY_local && y <= sliderY_local + sliderHeight_calc) {
          float ratio = (float)(x - sliderX) / sliderWidth;
          if (ratio < 0) ratio = 0;
          if (ratio > 1) ratio = 1;
          int newVal = params[currentParamIndex].minValue + ratio * (params[currentParamIndex].maxValue - params[currentParamIndex].minValue);
          params[currentParamIndex].value = newVal;
          drawMenu(tft, menuMode, params, currentParamIndex, motorControllerState);
        }
      }
    }
    else if(menuMode == 3) {
  // Touch sul terzo menu (2×3)
  const int rows = 2, cols = 3;
  int btnW = tft.width() / cols;
  int btnH = (tft.height() - staticAreaHeight) / rows;
  for(int i = 0; i < NUM_THIRD_BUTTONS; i++) {
    int bx = (i % cols) * btnW;
    int by = staticAreaHeight + (i/cols) * btnH;
    if(x >= bx && x <= bx + btnW && y >= by && y <= by + btnH) {
      tft.fillRoundRect(bx, by, btnW, btnH, 10, 0xFFFF);
      pendingAction     = thirdButtonActions[i];
      pendingButtonType = 1;
      buttonActionState = BAS_HIGHLIGHT;
      buttonActionTimestamp = millis();
      break;
    }
  }
}

if (pendingAction == "RESET") {
    resetAllEEPROM(); // reset EEPROM AP
    udp.beginPacket(clientIP, clientPort);
    udp.print("RESET_EEPROM");
    udp.endPacket();
    debugLog("Inviato comando RESET_EEPROM al client!");
}


  }
if (menuMode == 3 && pendingAction == "FIRMWARE") {
    requestClientFirmwareVersion();      // Invia richiesta
    showFirmwareVersion(tft);            // Mostra subito "Attendi..."

    // Attendi la risposta, processando i pacchetti UDP!
    unsigned long startWait = millis();
    while (lastClientFwVersion == "Attendi..." && millis() - startWait < 1500) {
        // Qui processa i pacchetti UDP!
        int packetSize = udp.parsePacket();
        if(packetSize) {
            clientIP = udp.remoteIP();
            clientPort = udp.remotePort();
            int len = udp.read(incomingPacket, 255);
            if(len > 0) {
                incomingPacket[len] = 0;
                String msg = String(incomingPacket);
                if (msg.startsWith("FW_VERSION_CLIENT:")) {
                    lastClientFwVersion = msg.substring(strlen("FW_VERSION_CLIENT:"));
                }
            }
        }
        delay(10); // breva pausa per non bloccare tutto
    }

    showFirmwareVersion(tft); // Ora mostra la versione vera
    delay(2000);
    menuMode = 0;
    drawStaticLayout(tft, motorControllerState, externalBearingEnabled);
    drawMenu(tft, menuMode, params, currentParamIndex, motorControllerState);
}
if (menuMode == 3 && pendingAction == "RESET") {
    resetAllEEPROM();
    // Eventuale messaggio a schermo
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(40, tft.height()/2);
    tft.println("EEPROM resettata!");
    delay(1000);
    menuMode = 0;
    drawMenu(tft, menuMode, params, currentParamIndex, motorControllerState);
    return;
}

}

