#include <Arduino_GFX_Library.h>

// ——— PIN standard per JC3248W535C-I-Y (QSPI 4-bit) ———
#define LCD_CS   45   // Chip Select
#define LCD_SCK  47   // SCLK
#define LCD_D0   21   // D0
#define LCD_D1   48   // D1
#define LCD_D2   40   // D2
#define LCD_D3   39   // D3
#define LCD_RST  -1   // Reset non collegato
#define LCD_BL   1    // Backlight
// —————————————————————————————————————————————————

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCK,
  LCD_D0, LCD_D1,
  LCD_D2, LCD_D3
);
Arduino_GFX *gfx = new Arduino_AXS15231B(
  bus,
  LCD_RST,
  1,      // rotation: landscape CW
  false,  // IPS = false
  320,480 // risoluzione
);

void setup() {
  Serial.begin(115200);
  delay(50);

  // Accendi retroilluminazione
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  // Inizializza display
  gfx->begin();
  gfx->setRotation(1);

  // Color test
  gfx->fillScreen(RED);   delay(300);
  gfx->fillScreen(GREEN); delay(300);
  gfx->fillScreen(BLUE);  delay(300);

  // Torna a nero
  gfx->fillScreen(BLACK);

  // Stampa testo con drawString()
  gfx->setTextSize(4);            // dimensione font
  gfx->setTextColor(WHITE);       // solo colore (background = nero di default)
  gfx->drawString("Test OK!", 20, 200);
}

void loop() {
  // fermo
}
