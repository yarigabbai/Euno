#include <Arduino_GFX_Library.h>
#include <lvgl.h>

#define LCD_CS   45
#define LCD_SCK  47
#define LCD_D0   21
#define LCD_D1   48
#define LCD_D2   40
#define LCD_D3   39
#define LCD_RST  -1   // Se non usato, altrimenti imposta un pin
#define LCD_BL   1

Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
Arduino_GFX *gfx = new Arduino_AXS15231B(bus, LCD_RST, 0, false, 320, 480);

/* LVGL display buffer */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[320 * 10]; // Buffer per LVGL

/* Funzione per flush (aggiornamento schermo) */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)color_p, w, h);
  lv_disp_flush_ready(disp);
}

void setup() {
  Serial.begin(115200);
  
  /* Inizializza retroilluminazione */
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  /* Inizializza display */
  gfx->begin();
  gfx->fillScreen(BLACK);
  
  /* Inizializza LVGL */
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, 320 * 10);

  /* Configura driver display LVGL */
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 320;
  disp_drv.ver_res = 480;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /* Crea una label di test */
  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "Hello LVGL!");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void loop() {
  lv_timer_handler(); // Mantieni attivo LVGL
  delay(5);
}