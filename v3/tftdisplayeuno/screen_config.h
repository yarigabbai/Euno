#ifndef SCREEN_CONFIG_H
#define SCREEN_CONFIG_H

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Arduino.h>

// ============ PIN TOUCH (identici ai tuoi) ============
#define XPT2046_IRQ    36
#define XPT2046_MOSI   32
#define XPT2046_MISO   39
#define XPT2046_CLK    25
#define XPT2046_CS     33

// ============ UI / Layout invariato ============
static const int staticAreaHeight = 120;       // fascia superiore 3×2
static const unsigned long touchDebounceDelay = 150;
static const int sliderX = 10;
static const int sliderWidth = 320 - 20;

enum ButtonActionState { BAS_IDLE, BAS_HIGHLIGHT, BAS_ACTION_SENT };

struct Parameter {
  const char* name;
  int value;
  int minValue;
  int maxValue;
};
#define NUM_PARAMS 7

// Label 3×2 superiore (box 0..5)
static String infoLabels[6] = {"Heading","Cmd","Err","GPS","Spd",""};

// ============ COLORI HELP ============
#ifndef TFT_GREEN
#define TFT_GREEN 0x07E0
#endif
#ifndef TFT_RED
#define TFT_RED   0xF800
#endif
#ifndef TFT_CYAN
#define TFT_CYAN  0x07FF
#endif
#ifndef TFT_WHITE
#define TFT_WHITE 0xFFFF
#endif
#ifndef TFT_BLACK
#define TFT_BLACK 0x0000
#endif
#ifndef TFT_DARKGREY
#define TFT_DARKGREY 0x7BEF
#endif

// ============ PROTOTIPI FUNZIONI UI ============
void uiBeginDisplay(TFT_eSPI& tft);
void uiBeginTouch(XPT2046_Touchscreen& ts);
void uiDrawStatic(TFT_eSPI& tft, bool motorOn, bool extBrg);
void uiUpdateBox(TFT_eSPI& tft, int index, const String& value);
void uiUpdateBoxColor(TFT_eSPI& tft, int index, const String& value, uint16_t color);
void uiUpdateMainOnOff(TFT_eSPI& tft, bool isOn);
void uiDrawMenu(TFT_eSPI& tft, int menuMode, Parameter params[], int currentParamIndex, bool motorOn);
void uiSetCmdLabel(bool externalBearingEnabled);
void uiApplyHeadingModeLabel(TFT_eSPI& tft, const String& mode);
void uiFlashInfo(TFT_eSPI& tft, const String& msg);

void uiCheckTouch(
  TFT_eSPI &tft,
  XPT2046_Touchscreen &touchscreen,
  int &menuMode,
  bool &motorControllerState,
  int &currentParamIndex,
  Parameter params[],      // array dei parametri
  String &pendingAction,   // output: es. "ACTION:+1", "SET:V_min=120", "MENU:SWITCH"
  int &pendingButtonType,
  ButtonActionState &buttonActionState,
  unsigned long &buttonActionTimestamp,
  unsigned long &lastTouchTime
);
bool uiConsumeAction(String& pendingAction);

// ============ IMPLEMENTAZIONI ============

// Inizializza TFT
void uiBeginDisplay(TFT_eSPI& tft){
  tft.init();
  tft.setRotation(1); // come già usi
  tft.fillScreen(TFT_BLACK);
}

// Inizializza touch
void uiBeginTouch(XPT2046_Touchscreen& ts){
  SPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI);
  ts.begin();
  ts.setRotation(1);
}

// Layout fisso: 6 box superiori (3×2)
// box 0..4 = valori, box 5 = “EUNO autopilot”
void uiDrawStatic(TFT_eSPI& tft, bool motorOn, bool externalBearingEnabled){
  tft.fillRect(0, 0, tft.width(), staticAreaHeight, TFT_BLACK);
  int rows = 2, cols = 3;
  int boxW = tft.width() / cols;
  int boxH = staticAreaHeight / rows;

  for (int i=0;i<6;i++){
    int x = (i % cols)*boxW, y = (i / cols)*boxH;

    if (i == 5){
      uint16_t colMC = TFT_BLACK; // fisso
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
      // label
      tft.setTextDatum(TL_DATUM);
      tft.setTextSize(1);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      String label = infoLabels[i];
      if (i==1 && externalBearingEnabled) label = "CMD BRG";
      tft.drawString(label, x+2, y+2);
      // value
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(3);
      tft.setTextColor(0xFFE0, TFT_BLACK);
      tft.drawString("N/A", x + boxW/2, y + boxH/2);
    }
  }
}

// Aggiorna singolo box (0..4)
void uiUpdateBox(TFT_eSPI& tft, int index, const String& value){
  if (index<0 || index>4) return;
  int rows=2, cols=3;
  int boxW = tft.width()/cols;
  int boxH = staticAreaHeight/rows;
  int x=(index%cols)*boxW, y=(index/cols)*boxH;

  // bordo e contenuto
  tft.fillRoundRect(x, y, boxW, boxH, 5, TFT_BLACK);
  tft.drawRoundRect(x, y, boxW, boxH, 5, TFT_WHITE);

  // ripeti label sopra
  tft.fillRect(x+2, y+2, boxW-4, 14, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(infoLabels[index], x+2, y+2);

  // valore
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(3);
  tft.setTextColor(0xFFE0, TFT_BLACK);
  tft.drawString(value, x + boxW/2, y + boxH/2);
}

// Variante colorata (per CMD verde quando EXTBRG è attivo)
void uiUpdateBoxColor(TFT_eSPI& tft, int index, const String& value, uint16_t color){
  if (index<0 || index>4) return;
  int rows=2, cols=3;
  int boxW = tft.width()/cols;
  int boxH = staticAreaHeight/rows;
  int x=(index%cols)*boxW, y=(index/cols)*boxH;

  // background
  tft.fillRoundRect(x, y, boxW, boxH, 5, TFT_BLACK);
  tft.drawRoundRect(x, y, boxW, boxH, 5, TFT_WHITE);

  // label
  tft.fillRect(x+2, y+2, boxW-4, 14, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(infoLabels[index], x+2, y+2);

  // value
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(3);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(value, x + boxW/2, y + boxH/2);
}

// ON/OFF grande (tasto nel menu, ma qui ridisegniamo box terzo tasto del menu)
void uiUpdateMainOnOff(TFT_eSPI& tft, bool isOn){
  // lo disegniamo nel menu quando viene ridisegnato
  (void)tft; (void)isOn;
}

// Disegna il menu (3×2) in basso
static const char* mainButtonLabels[6]  = {"-1","+1","ON/OFF","-10","+10","MENU"};
static const char* mainButtonActions[6] = {"ACTION:-1","ACTION:+1","ACTION:TOGGLE","ACTION:-10","ACTION:+10","MENU:SWITCH"};

static const char* secondButtonLabels[6]  = {"CALIB","SOURCE","OFFset","SET","EXTBRG","MENU"};
static const char* secondButtonActions[6] = {"ACTION:CAL","ACTION:GPS","ACTION:C-GPS","IMP","ACTION:EXT_BRG","MENU:SWITCH"};

static const char* thirdButtonLabels[6]  = {"Firmware","GYROCAL","Foo","ADVCAL","Baz","MENU"};
static const char* thirdButtonActions[6] = {"FIRMWARE","ACTION:CAL-GYRO","ACTION:FOO","ACTION:EXPCAL","ACTION:BAZ","MENU:SWITCH"};

void uiDrawMenu(TFT_eSPI& tft, int menuMode, Parameter params[], int currentParamIndex, bool motorOn){
  tft.fillRect(0, staticAreaHeight, tft.width(), tft.height()-staticAreaHeight, TFT_BLACK);

  const int rows=2, cols=3;
  const int btnW = tft.width()/cols;
  const int btnH = (tft.height()-staticAreaHeight)/rows;

  auto drawGrid = [&](const char* labels[6]){
    for(int i=0;i<6;i++){
      int x=(i%cols)*btnW, y=staticAreaHeight+(i/cols)*btnH;
      tft.fillRoundRect(x, y, btnW, btnH, 10, TFT_DARKGREY);
      tft.drawRoundRect(x, y, btnW, btnH, 10, TFT_WHITE);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
      tft.drawString(labels[i], x+btnW/2, y+btnH/2);
    }
  };

  if (menuMode==0) {
    drawGrid(mainButtonLabels);
  } else if (menuMode==1) {
    drawGrid(secondButtonLabels);
  } else if (menuMode==3) {
    drawGrid(thirdButtonLabels);
  } else if (menuMode==2) {
    // Schermata parametri: header + slider (identica geometria)
    const int headerH = 50;
    tft.fillRoundRect(0, staticAreaHeight, tft.width(), headerH, 5, 0x000F /*navy*/);
    String hdr = String(params[currentParamIndex].name)+": "+String(params[currentParamIndex].value);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(hdr, tft.width()/2, staticAreaHeight + headerH/2);

    // pulsante NEXT a destra
    const int nextW = 80;
    tft.fillRoundRect(tft.width()-nextW-10, staticAreaHeight+5, nextW, headerH-10, 5, 0x03E0 /*green*/);
    tft.drawRoundRect(tft.width()-nextW-10, staticAreaHeight+5, nextW, headerH-10, 5, TFT_WHITE);
    tft.drawString("NEXT", tft.width()-nextW/2-10, staticAreaHeight + headerH/2);

    // slider sotto
    int lowerY = staticAreaHeight + headerH;
    int sliderY = lowerY + 30;
    float ratio = (float)(params[currentParamIndex].value - params[currentParamIndex].minValue) /
                  (float)(params[currentParamIndex].maxValue - params[currentParamIndex].minValue);
    int knobX = sliderX + (int)(sliderWidth * ratio);
    // track
    tft.fillRoundRect(sliderX, sliderY-4, sliderWidth, 8, 4, TFT_DARKGREY);
    tft.fillRoundRect(sliderX, sliderY-4, knobX - sliderX, 8, 4, TFT_CYAN);
    // knob
    tft.fillCircle(knobX, sliderY, 10, TFT_WHITE);
    tft.drawCircle(knobX, sliderY, 10, TFT_DARKGREY);
    tft.fillCircle(knobX, sliderY, 6, TFT_CYAN);
  }
}

// Cambia la label “Cmd” → “CMD BRG” quando EXTBRG ON
void uiSetCmdLabel(bool externalBearingEnabled){
  infoLabels[1] = externalBearingEnabled ? "CMD BRG" : "Cmd";
}

// Applica la label a seconda della sorgente heading
void uiApplyHeadingModeLabel(TFT_eSPI& tft, const String& mode){
  String m = mode;
  String label = "Heading";
  if      (m=="COMPASS")      label = "H.Compass";
  else if (m=="FUSION")       label = "H.Gyro";
  else if (m=="EXPERIMENTAL") label = "H.Expmt";
  else if (m=="ADV")          label = "H.Advanced";
  infoLabels[0] = label;

  // ridisegno angolo label del box 0
  int cols=3, rows=2;
  int boxW = tft.width()/cols, boxH = staticAreaHeight/rows;
  int x=0, y=0;
  tft.fillRect(x+2, y+2, boxW-4, 14, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(label, x+2, y+2);
}

// piccolo banner temporaneo nel box 5 (righe di stato)
void uiFlashInfo(TFT_eSPI& tft, const String& msg){
  int cols=3, rows=2;
  int boxW = tft.width()/cols, boxH = staticAreaHeight/rows;
  int x=(5%cols)*boxW, y=(5/cols)*boxH; // box 5
  tft.fillRoundRect(x, y, boxW, boxH, 5, TFT_BLACK);
  tft.drawRoundRect(x, y, boxW, boxH, 5, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(msg, x + boxW/2, y + boxH/2);
}

// Touch → genera pendingAction
void uiCheckTouch(
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
){
  // gestione transizioni highlight → esecuzione → redraw
  if (buttonActionState == BAS_HIGHLIGHT && (millis() - buttonActionTimestamp >= 100)) {
    // esecuzione differita di “NEXT” e MENU switch
    if (menuMode == 2 && pendingButtonType == 2 && pendingAction == "NEXT") {
      // invia SET corrente (verrà mappato nell’INO in $PEUNO,CMD,SET,...)
      pendingAction = String("SET:") + params[currentParamIndex].name + "=" + String(params[currentParamIndex].value);
      // passa al successivo
      currentParamIndex++;
      if (currentParamIndex >= NUM_PARAMS) { menuMode = 0; currentParamIndex = 0; }
    } else if (pendingAction == "MENU:SWITCH") {
      // 0 → 1 → 3 → 0
      if      (menuMode==0) menuMode=1;
      else if (menuMode==1) menuMode=3;
      else                  menuMode=0;
    }
    buttonActionState = BAS_ACTION_SENT;
    buttonActionTimestamp = millis();
  }
  else if (buttonActionState == BAS_ACTION_SENT && (millis() - buttonActionTimestamp >= 200)) {
    uiDrawMenu(tft, menuMode, params, currentParamIndex, motorControllerState);
    buttonActionState = BAS_IDLE;
  }
  if (buttonActionState != BAS_IDLE) return;

  // lettura touch
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    if (p.z < 20 || p.x < 100 || p.y < 100) return;
    if (millis() - lastTouchTime < touchDebounceDelay) return;
    lastTouchTime = millis();

    // mappatura identica
    int x = map(p.x, 200, 3700, 0, tft.width());
    int y = map(p.y, 240, 3800, 0, tft.height());

    const int rows=2, cols=3;
    const int btnW = tft.width()/cols;
    const int btnH = (tft.height()-staticAreaHeight)/rows;

    if (menuMode==0 || menuMode==1 || menuMode==3){
      // premi uno dei 6 pulsanti grid
      for (int i=0;i<6;i++){
        int bx=(i%cols)*btnW, by=staticAreaHeight+(i/cols)*btnH;
        if (x>=bx && x<=bx+btnW && y>=by && y<=by+btnH){
          tft.fillRoundRect(bx, by, btnW, btnH, 10, TFT_WHITE);
          // mappa azione
          if (menuMode==0) pendingAction = mainButtonActions[i];
          else if (menuMode==1) pendingAction = secondButtonActions[i];
          else pendingAction = thirdButtonActions[i];
          pendingButtonType = 0;
          buttonActionState = BAS_HIGHLIGHT;
          buttonActionTimestamp = millis();
          break;
        }
      }
    } else if (menuMode==2){
      // NEXT area (header)
      const int headerH=50;
      int leftW = (2*tft.width())/3;
      int rightW = tft.width()-leftW;
      if (y>=staticAreaHeight && y<staticAreaHeight+headerH){
        if (x >= tft.width()-80-10) {
          pendingAction="NEXT";
          pendingButtonType=2;
          buttonActionState=BAS_HIGHLIGHT;
          buttonActionTimestamp=millis();
          return;
        }
      }
      // SLIDER area
      int lowerY = staticAreaHeight + headerH;
      int sliderY = lowerY + 30;
      if (y >= sliderY-16 && y <= sliderY+16 && x>=sliderX && x<=sliderX+sliderWidth){
        float ratio = (float)(x - sliderX)/sliderWidth;
        if (ratio<0) ratio=0; if (ratio>1) ratio=1;
        int nv = params[currentParamIndex].minValue +
                 (int)round(ratio * (params[currentParamIndex].maxValue - params[currentParamIndex].minValue));
        params[currentParamIndex].value = nv;
        uiDrawMenu(tft, menuMode, params, currentParamIndex, motorControllerState);
      }
    }
  }
}

bool uiConsumeAction(String& pendingAction){
  if (pendingAction.length()==0) return false;
  // ritorna true e azzera
  // (la vera evasione è fatta nel loop dello sketch)
  return (pendingAction.length() ? (pendingAction = "", true) : false);
}

// ==== piccoli helper CSV/KV per la telemetria ====
inline String getFieldCSV(const String& line, int index){
  int c=-1, start=0;
  for (int i=0;i<(int)line.length();i++){
    if (line[i]==','){ c++; if (c==index-1){ start=i+1; } else if (c==index){ return line.substring(start, i); } }
  }
  return line.substring(start);
}
inline int kvGetInt(const String& line, const String& key){
  int s = line.indexOf(key+"=");
  if (s<0) return INT_MIN;
  int e = line.indexOf(",", s);
  if (e<0) e = line.length();
  return line.substring(s+key.length()+1, e).toInt();
}
inline String kvGetStr(const String& line, const String& key){
  int s = line.indexOf(key+"=");
  if (s<0) return "";
  int e = line.indexOf(",", s);
  if (e<0) e = line.length();
  return line.substring(s+key.length()+1, e);
}

#endif // SCREEN_CONFIG_H
