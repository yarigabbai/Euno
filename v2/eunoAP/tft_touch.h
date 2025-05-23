#ifndef TFT_TOUCH_H
#define TFT_TOUCH_H
#define XPT2046_IRQ    36
#define XPT2046_MOSI   32
#define XPT2046_MISO   39
#define XPT2046_CLK    25
#define XPT2046_CS     33
#include <WebSocketsServer.h>

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

extern String infoLabels[6];
#ifndef PARAMETER_DEFINED
#define PARAMETER_DEFINED

struct Parameter {
  const char* name;
  int value;
  int minValue;
  int maxValue;
};
extern Parameter params[];
extern WebSocketsServer webSocket;
extern String headingLabel;  // iniziale

void updateMainButtonONOFF(TFT_eSPI &tft, bool isOn);

// Azione pulsante
enum ButtonActionState { BAS_IDLE, BAS_HIGHLIGHT, BAS_ACTION_SENT };


#define NUM_PARAMS 7

// Didascalie per i riquadri info (top area)

void drawStaticLayout(TFT_eSPI &tft, bool motorControllerState, bool externalBearingEnabled);

void updateDataBoxColor(TFT_eSPI &tft, int index, String value, uint16_t color);

// Menu principale
#define NUM_MAIN_BUTTONS 6
static const char* mainButtonLabels[NUM_MAIN_BUTTONS] = { "-1", "+1", "ON/OFF", "-10", "+10", "MENU" };
static const char* mainButtonActions[NUM_MAIN_BUTTONS] = { "ACTION:-1", "ACTION:+1", "ACTION:TOGGLE", "ACTION:-10", "ACTION:+10", "MENU:SWITCH" };
static const uint16_t buttonColorsMain[NUM_MAIN_BUTTONS] = { 0xD69A, 0xD69A, 0xFDA0, 0xD69A, 0xD69A, 0xFDA0 };

// Menu secondario
#define NUM_SECOND_BUTTONS 6
static const char* secondButtonLabels[NUM_SECOND_BUTTONS] = { "CALIB", "SOURCE", "OFFset", "SET", "EXTBRG", "MENU" };
static const char* secondButtonActions[NUM_SECOND_BUTTONS] = { "ACTION:CAL", "ACTION:GPS", "ACTION:C-GPS", "IMP", "ACTION:EXT_BRG", "MENU:SWITCH" };
static const uint16_t buttonColorsSecond[NUM_SECOND_BUTTONS] = { 0xFE60, 0xFE60, 0xFE60, 0xFE60, 0xFE60, 0xFE60 };
// Menu terziario
#define NUM_THIRD_BUTTONS 6
static const char* thirdButtonLabels[NUM_THIRD_BUTTONS]  = { "Firmware", "AdvCal", "Foo", "ExCal", "Baz", "MENU" };
static const char* thirdButtonActions[NUM_THIRD_BUTTONS] = { "FIRMWARE", "ACTION:CAL-GYRO", "ACTION:FOO", "ACTION:EXPCAL", "ACTION:BAZ", "MENU:SWITCH" };

static const uint16_t buttonColorsThird[NUM_THIRD_BUTTONS] = { 0x03E0, 0x07FF, 0xD69A, 0xFDA0, 0xFE60, 0xFDA0 };
// Slider geometry
static const int staticAreaHeight = 120;
static const int sliderX = 10;
static const int sliderWidth = 320 - 20; 
static const unsigned long touchDebounceDelay = 150;

// -------------------------------------------------
// Funzioni
String truncateString(String s, int maxChars);
void drawStaticLayout(TFT_eSPI &tft, bool motorControllerState);
void updateDataBox(TFT_eSPI &tft, int index, String value);
void drawMenu(TFT_eSPI &tft, int menuMode, Parameter params[], int currentParamIndex, bool motorControllerState);

void checkTouch(
  TFT_eSPI &tft,
  XPT2046_Touchscreen &touchscreen,
  int &menuMode,
  bool &motorControllerState,
  int &currentParamIndex,
  Parameter params[],      // Passiamo l'array param dal .ino
  String &pendingAction,
  int &pendingButtonType,
  ButtonActionState &buttonActionState,
  unsigned long &buttonActionTimestamp,
  unsigned long &lastTouchTime
);

#endif
#endif // TFT_TOUCH_H