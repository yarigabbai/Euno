#include "tft_touch.h"
#include <Arduino.h> 
#include "nmea_parser.h"  // per handleCommandAP() se serve in checkTouch


#define EUNO_IS_AP
#include "euno_debugAP.h"


// -------------------------------------
String truncateString(String s, int maxChars) {
  return s.length() > maxChars ? s.substring(0, maxChars - 3) + "..." : s;
}

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
      uint16_t colMC = motorControllerState ? 0x07E0 /*TFT_GREEN*/ : 0xF800 /*TFT_RED*/;
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
void updateDataBoxColor(TFT_eSPI &tft, int index, String value, uint16_t color) {
  int boxWidth = 50;
  int x = 5 + index * boxWidth;
  int y = 5;
  tft.fillRect(x, y, boxWidth - 2, 30, TFT_BLACK);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(value, x + 5, y + 5);
}
// -------------------------------------
void drawMenu(TFT_eSPI &tft, int menuMode, Parameter params[], int currentParamIndex, bool motorControllerState) {
  tft.fillRect(0, staticAreaHeight, tft.width(), tft.height()-staticAreaHeight, 0x0000);

  if(menuMode == 0) {
    // Menu principale
    int rows = 2, cols = 3;
    int btnW = tft.width() / cols;
    int btnH = (tft.height() - staticAreaHeight) / rows;
    for (int i = 0; i < NUM_MAIN_BUTTONS; i++){
      int x = (i % cols) * btnW;
      int y = staticAreaHeight + (i / cols) * btnH;
      uint16_t color = buttonColorsMain[i];
      tft.fillRoundRect(x, y, btnW, btnH, 10, color);
      tft.drawRoundRect(x, y, btnW, btnH, 10, 0xFFFF);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(3);
      // effetto ombra
      tft.setTextColor(0x0000, color);
      tft.drawString(mainButtonLabels[i], x + btnW/2 + 1, y + btnH/2 + 1);
      // testo bianco
      tft.setTextColor(0xFFFF, color);
      tft.drawString(mainButtonLabels[i], x + btnW/2, y + btnH/2);
    }
  }
  else if(menuMode == 1) {
    // Menu secondario
    int btnCount = NUM_SECOND_BUTTONS;
    int rowCount = 2;
    int btnPerRow = btnCount / rowCount;
    int btnH = (tft.height() - staticAreaHeight) / rowCount;
    int btnW = tft.width() / btnPerRow;
    for (int i = 0; i < btnCount; i++){
      int x = (i % btnPerRow) * btnW;
      int y = staticAreaHeight + (i / btnPerRow) * btnH;
      uint16_t color = buttonColorsSecond[i];
      tft.fillRoundRect(x, y, btnW, btnH, 10, color);
      tft.drawRoundRect(x, y, btnW, btnH, 10, 0xFFFF);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(3);
      tft.setTextColor(0xFFFF, color);
      tft.drawString(secondButtonLabels[i], x + btnW/2, y + btnH/2);
    }
  }
  else if(menuMode == 2) {
    // Esempio di menu “parametri”
    int settingsH = tft.height() - staticAreaHeight;
    int upperH = (settingsH * 55) / 100;
    int lowerY = staticAreaHeight + upperH;
    tft.drawFastHLine(0, lowerY, tft.width(), 0xFFFF);
    int leftW = (2 * tft.width()) / 3;
    int rightW = tft.width() - leftW;

    String paramName = String(params[currentParamIndex].name);
    String headerStr = paramName + ": " + String(params[currentParamIndex].value);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(0xFFFF, 0x0000);
    tft.drawString(headerStr, 10, staticAreaHeight + 20);

    // Tasto NEXT
    int nextX = leftW;
    int nextY = staticAreaHeight;
    tft.fillRoundRect(nextX, nextY, rightW, upperH, 10, 0x001F /* blue */);
    tft.drawRoundRect(nextX, nextY, rightW, upperH, 10, 0xFFFF);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(0xFFFF, 0x001F);
    tft.drawString("NEXT", nextX + rightW/2, nextY + upperH/2);

    // Slider
    int sliderY_local = lowerY;  
    int sliderMargin = 20;
    int sliderHeight_calc = tft.height() - lowerY;
    tft.drawRect(sliderX - sliderMargin, sliderY_local, sliderWidth + 2*sliderMargin, sliderHeight_calc, 0xFFFF);

    float ratio = (float)(params[currentParamIndex].value - params[currentParamIndex].minValue) /
                  (params[currentParamIndex].maxValue - params[currentParamIndex].minValue);
    if(ratio < 0) ratio = 0;
    if(ratio > 1) ratio = 1;
    int fillWidth = ratio * sliderWidth;
    tft.fillRect(sliderX - sliderMargin, sliderY_local, fillWidth + 2*sliderMargin, sliderHeight_calc, 0x07E0);
  }
}

// -------------------------------------
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
  if(buttonActionState == BAS_HIGHLIGHT && (millis() - buttonActionTimestamp >= 100)) {
    // processPendingAction
    if(menuMode == 2 && pendingButtonType == 2 && pendingAction == "NEXT") {
      // Spediamo param via UDP
      String udpCmd = "SET:" + String(params[currentParamIndex].name) + "=" + String(params[currentParamIndex].value);
      handleCommandAP(udpCmd);

      currentParamIndex++;
      if(currentParamIndex >= NUM_PARAMS) {
        menuMode = 0;
      }
    }
    else {
      if(pendingAction == "MENU:SWITCH") {
        menuMode = (menuMode == 0) ? 1 : 0;
      }
      else if(pendingAction == "ACTION:TOGGLE") {
        motorControllerState = !motorControllerState;
        updateDataBox(tft, 5, "");
        handleCommandAP("ACTION:TOGGLE");
      }
      else if(pendingAction == "IMP") {
        menuMode = 2;
        currentParamIndex = 0;
      }
      else {
        handleCommandAP(pendingAction);
      }
    }
    buttonActionState = BAS_ACTION_SENT;
    buttonActionTimestamp = millis();
  }
  else if(buttonActionState == BAS_ACTION_SENT && (millis() - buttonActionTimestamp >= 200)) {
    drawMenu(tft, menuMode, params, currentParamIndex, motorControllerState);
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

    if(menuMode == 0) {
      int rows = 2, cols = 3;
      int btnW = tft.width() / cols;
      int btnH = (tft.height() - staticAreaHeight) / rows;
      for(int i = 0; i < NUM_MAIN_BUTTONS; i++){
        int bx = (i % cols) * btnW;
        int by = staticAreaHeight + (i / cols) * btnH;
        if(x >= bx && x <= bx + btnW && y >= by && y <= by + btnH){
          tft.fillRoundRect(bx, by, btnW, btnH, 10, 0xFFFF);
          delay(100);
          pendingAction = mainButtonActions[i];
          pendingButtonType = 0;
          buttonActionState = BAS_HIGHLIGHT;
          buttonActionTimestamp = millis();
          break;
        }
      }
    }
    else if(menuMode == 1) {
      int row1Count = 3;
      int row1H = (tft.height() - staticAreaHeight) / 2;
      int btnW1 = tft.width() / row1Count;
      bool found = false;
      for (int i = 0; i < row1Count; i++){
        int bx = i * btnW1;
        int by = staticAreaHeight;
        if(x >= bx && x <= bx + btnW1 && y >= by && y <= by + row1H){
          tft.fillRoundRect(bx, by, btnW1, row1H, 10, 0xFFFF);
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
            tft.fillRoundRect(bx, by, btnW2, row1H, 10, 0xFFFF);
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
    else if(menuMode == 2) {
      // Tocco in "param mode"
      int settingsH = tft.height() - staticAreaHeight;
      int upperH = (settingsH * 55) / 100;
      int lowerY = staticAreaHeight + upperH;
      int leftW = (2 * tft.width()) / 3;

      if(y >= staticAreaHeight && y < lowerY) {
        if(x >= leftW) {
          // Tasto NEXT
          pendingAction = "NEXT";
          pendingButtonType = 2;
          buttonActionState = BAS_HIGHLIGHT;
          buttonActionTimestamp = millis();
        }
      }
      else if(y >= lowerY) {
        // Slider
        int sliderY_local = lowerY;  
        int sliderMargin = 20;
        int sliderHeight_calc = tft.height() - lowerY;
        if(x >= sliderX - sliderMargin && x <= sliderX + sliderWidth + sliderMargin &&
           y >= sliderY_local && y <= sliderY_local + sliderHeight_calc) {
          float ratio = (float)(x - sliderX) / sliderWidth;
          if(ratio < 0) ratio = 0;
          if(ratio > 1) ratio = 1;
          int newVal = params[currentParamIndex].minValue + ratio * (params[currentParamIndex].maxValue - params[currentParamIndex].minValue);
          params[currentParamIndex].value = newVal;
          drawMenu(tft, menuMode, params, currentParamIndex, motorControllerState);
        }
      }
    }
  }
}
