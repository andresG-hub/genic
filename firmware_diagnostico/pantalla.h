// ============================================================================
//  pantalla.h  -  Interfaz HMI para LilyGO T-Display (ST7789 240x135)
//  Adafruit_GFX + Adafruit_ST7789. Pines en el sketch (config.h), sin editar
//  archivos de libreria.
// ----------------------------------------------------------------------------
//  Librerias: "Adafruit GFX Library" + "Adafruit ST7735 and ST7789 Library"
// ============================================================================
#ifndef PANTALLA_H
#define PANTALLA_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "clasificador.h"
#include "logo_genic.h"

// Resolucion en horizontal (rotation 1/3)
#define SCR_W 240
#define SCR_H 135

// Colores RGB565
#define C_BLACK    0x0000
#define C_WHITE    0xFFFF
#define C_GREEN    0x07E0
#define C_RED      0xF800
#define C_BLUE     0x001F
#define C_CYAN     0x07FF
#define C_MAGENTA  0xF81F
#define C_YELLOW   0xFFE0
#define C_ORANGE   0xFD20
#define C_NAVY     0x000F
#define C_DGREY    0x2124   // gris muy oscuro (footer)
#define C_ITEM     0x18E3   // gris item de menu no seleccionado
#define C_DARKGREY 0x7BEF

// Color de acento de marca (#ffc95c) para contrastes y alertas
#define C_ALERT    0xFE4B

// Color por diagnostico
inline uint16_t colorDiag(Diagnostico d) {
  switch (d) {
    case SANA:        return C_GREEN;
    case BOTRYTIS:    return C_MAGENTA;   // moho gris -> violeta
    case ANTRACNOSIS: return C_ORANGE;
    case PODRIDA:     return C_RED;
    default:          return C_DARKGREY;
  }
}

// ---------------------------------------------------------------------------
//  Helpers de texto (Adafruit_GFX no tiene alineacion "datum")
// ---------------------------------------------------------------------------
inline void txtCentro(Adafruit_ST7789& tft, const String& s, int16_t cx, int16_t cy,
                      uint8_t size, uint16_t fg) {
  tft.setTextSize(size);
  tft.setTextColor(fg);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - w / 2, cy - h / 2);
  tft.print(s);
}

inline void txtIzq(Adafruit_ST7789& tft, const String& s, int16_t x, int16_t cy,
                   uint8_t size, uint16_t fg) {
  tft.setTextSize(size);
  tft.setTextColor(fg);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(x, cy - h / 2);
  tft.print(s);
}

inline void txtDer(Adafruit_ST7789& tft, const String& s, int16_t xr, int16_t cy,
                   uint8_t size, uint16_t fg) {
  tft.setTextSize(size);
  tft.setTextColor(fg);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(xr - w, cy - h / 2);
  tft.print(s);
}

// ---------------------------------------------------------------------------
//  Componentes reutilizables
// ---------------------------------------------------------------------------
inline void uiHeader(Adafruit_ST7789& tft, const String& titulo, bool wifiOk) {
  tft.fillRect(0, 0, SCR_W, 20, C_NAVY);
  txtIzq(tft, titulo, 6, 10, 2, C_WHITE);
  tft.fillCircle(SCR_W - 12, 10, 5, wifiOk ? C_GREEN : C_ALERT);
}

inline void uiFooter(Adafruit_ST7789& tft, const String& b1, const String& b2) {
  tft.fillRect(0, SCR_H - 16, SCR_W, 16, C_DGREY);
  if (b1.length()) txtIzq(tft, "B1 " + b1, 4, SCR_H - 8, 1, C_ALERT);
  if (b2.length()) txtDer(tft, b2 + " B2", SCR_W - 4, SCR_H - 8, 1, C_ALERT);
}

// Grafico de barras de las 18 bandas (color por grupo UV/VIS/NIR)
inline void uiEspectro(Adafruit_ST7789& tft, const float* b, int x, int y, int w, int h) {
  tft.drawRect(x, y, w, h, C_DARKGREY);
  float mx = 1e-6f;
  for (int i = 0; i < 18; i++) if (b[i] > mx) mx = b[i];
  int bw = (w - 2) / 18;
  for (int i = 0; i < 18; i++) {
    int bh = (int)((h - 2) * (b[i] / mx));
    if (bh < 1)     bh = 1;
    if (bh > h - 2) bh = h - 2;
    uint16_t col = (i < 6) ? C_CYAN : (i < 12) ? C_GREEN : C_RED;
    tft.fillRect(x + 1 + i * bw, y + h - 1 - bh, bw - 1, bh, col);
  }
}

// ---------------------------------------------------------------------------
//  Pantallas completas
// ---------------------------------------------------------------------------
// Splash: dibuja el logo GENIC a pantalla completa (240x135)
inline void uiSplash(Adafruit_ST7789& tft) {
  tft.drawRGBBitmap(0, 0, LOGO_GENIC, LOGO_W, LOGO_H);
}

static const char* MENU_ITEMS[] = { "Diagnostico", "Entrenamiento", "WiFi", "Info" };
static const int    MENU_N      = 4;

inline void uiMenu(Adafruit_ST7789& tft, int sel, bool wifiOk) {
  tft.fillScreen(C_BLACK);
  uiHeader(tft, "Menu principal", wifiOk);
  const int top = 24;
  const int footerTop = SCR_H - 16;
  const int step = (footerTop - top) / MENU_N;   // reparte el espacio
  const int boxH = step - 3;
  for (int i = 0; i < MENU_N; i++) {
    int y = top + i * step;
    bool s = (i == sel);
    tft.fillRoundRect(12, y, SCR_W - 24, boxH, 4, s ? C_ALERT : C_ITEM);
    txtCentro(tft, MENU_ITEMS[i], SCR_W / 2, y + boxH / 2, 2, s ? C_BLACK : C_WHITE);
  }
  uiFooter(tft, "Mover", "Elegir");
}

inline void uiPromptMedir(Adafruit_ST7789& tft, const String& fruta, bool wifiOk) {
  tft.fillScreen(C_BLACK);
  uiHeader(tft, "Diagnostico", wifiOk);
  txtCentro(tft, "Coloca la fruta", SCR_W / 2, 44, 2, C_WHITE);
  txtCentro(tft, "sobre el sensor", SCR_W / 2, 66, 2, C_WHITE);
  txtCentro(tft, "Fruta: " + fruta, SCR_W / 2, 92, 2, C_CYAN);
  uiFooter(tft, "Volver", "MEDIR");
}

inline void uiMidiendo(Adafruit_ST7789& tft) {
  tft.fillScreen(C_BLACK);
  txtCentro(tft, "Midiendo...", SCR_W / 2, SCR_H / 2, 3, C_ALERT);
}

inline void uiResultado(Adafruit_ST7789& tft, Diagnostico d, const Features& f,
                        const float* b, const String& fruta, bool wifiOk) {
  tft.fillScreen(C_BLACK);
  uiHeader(tft, "Resultado", wifiOk);

  // Banda de diagnostico
  uint16_t c = colorDiag(d);
  tft.fillRoundRect(8, 23, SCR_W - 16, 28, 6, c);
  txtCentro(tft, DIAG_NOMBRE[d], SCR_W / 2, 37, 3, C_BLACK);

  // Grafico espectral de las 18 bandas
  uiEspectro(tft, b, 8, 56, SCR_W - 16, 34);

  // Metricas
  char buf[48];
  snprintf(buf, sizeof(buf), "NDVI %.2f  R/G %.2f  pig %.2f",
           f.ndvi, f.ratioRG, f.pigmento);
  txtCentro(tft, buf, SCR_W / 2, 100, 1, C_WHITE);
  txtCentro(tft, fruta, SCR_W / 2, 110, 1, C_CYAN);

  uiFooter(tft, "Menu", "Medir");
}

inline void uiEntrenamiento(Adafruit_ST7789& tft, const String& ip, bool wifiOk,
                            bool fbOk, const String& fruta, const String& estado) {
  tft.fillScreen(C_BLACK);
  uiHeader(tft, "Entrenamiento", wifiOk);
  int y = 32;
  txtIzq(tft, "IP: " + (ip.length() ? ip : String("--")), 8, y, 2, C_WHITE); y += 22;
  txtIzq(tft, String("Firebase: ") + (fbOk ? "ON" : "OFF"), 8, y, 2, fbOk ? C_GREEN : C_ALERT); y += 22;
  txtIzq(tft, "Fruta:  " + fruta, 8, y, 2, C_CYAN);  y += 18;
  txtIzq(tft, "Estado: " + estado, 8, y, 2, C_CYAN);
  uiFooter(tft, "Salir", "Medir");
}

inline void uiInfo(Adafruit_ST7789& tft, bool wifiOk) {
  tft.fillScreen(C_BLACK);
  uiHeader(tft, "Info", wifiOk);
  int y = 30;
  txtIzq(tft, "GENIC - AS7265x 18 bandas", 8, y, 1, C_WHITE); y += 14;
  txtIzq(tft, "410-940 nm  (UV/VIS/NIR)", 8, y, 1, C_WHITE);  y += 14;
  txtIzq(tft, "Clases: sana/botrytis/", 8, y, 1, C_WHITE);    y += 14;
  txtIzq(tft, "antracnosis/podrida", 8, y, 1, C_WHITE);       y += 14;
  txtIzq(tft, "Modo 2: web + Firebase RTDB", 8, y, 1, C_WHITE);
  uiFooter(tft, "Volver", "");
}

// ---------------------------------------------------------------------------
//  Pantallas del gestor WiFi
// ---------------------------------------------------------------------------
inline void uiWifiInfo(Adafruit_ST7789& tft, bool conectado, const String& ssid,
                       const String& ip) {
  tft.fillScreen(C_BLACK);
  uiHeader(tft, "WiFi", conectado);
  if (conectado) {
    txtIzq(tft, "Conectado a:", 8, 34, 1, C_WHITE);
    txtIzq(tft, ssid, 8, 52, 2, C_GREEN);
    txtIzq(tft, "IP: " + ip, 8, 76, 2, C_CYAN);
    txtIzq(tft, "B2 = reconfigurar red", 8, 100, 1, C_WHITE);
  } else {
    txtCentro(tft, "Sin conexion WiFi", SCR_W / 2, 44, 2, C_ALERT);
    txtCentro(tft, "Pulsa B2 para", SCR_W / 2, 72, 1, C_WHITE);
    txtCentro(tft, "configurar por movil", SCR_W / 2, 86, 1, C_WHITE);
  }
  uiFooter(tft, "Volver", "Configurar");
}

inline void uiWifiPortal(Adafruit_ST7789& tft, const String& apSsid, const String& url) {
  tft.fillScreen(C_BLACK);
  uiHeader(tft, "Configurar WiFi", false);
  txtCentro(tft, "1) Conecta tu movil a:", SCR_W / 2, 30, 1, C_WHITE);
  txtCentro(tft, apSsid, SCR_W / 2, 46, 2, C_ALERT);
  txtCentro(tft, "2) Abre esta pagina:", SCR_W / 2, 68, 1, C_WHITE);
  txtCentro(tft, url, SCR_W / 2, 84, 2, C_CYAN);
  txtCentro(tft, "Elige tu red y clave", SCR_W / 2, 104, 1, C_WHITE);
  uiFooter(tft, "Cancelar", "");
}

inline void uiWifiResultado(Adafruit_ST7789& tft, bool ok, const String& msg) {
  tft.fillScreen(C_BLACK);
  uiHeader(tft, "WiFi", ok);
  txtCentro(tft, ok ? "Conectado!" : "Fallo", SCR_W / 2, 46, 3, ok ? C_GREEN : C_ALERT);
  txtCentro(tft, msg, SCR_W / 2, 84, 1, C_WHITE);
  uiFooter(tft, "Volver", "");
}

#endif // PANTALLA_H
