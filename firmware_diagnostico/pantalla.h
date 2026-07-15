// ============================================================================
//  pantalla.h  -  Interfaz HMI para LilyGO T-Display (ST7789 240x135)
// ----------------------------------------------------------------------------
//  Requiere la libreria TFT_eSPI configurada para la T-Display
//  (ver CONEXIONES.md: activar Setup25_TTGO_T_Display en User_Setup_Select.h).
//
//  Todas las funciones reciben la instancia TFT_eSPI por referencia para no
//  crear objetos globales duplicados.
// ============================================================================
#ifndef PANTALLA_H
#define PANTALLA_H

#include <TFT_eSPI.h>
#include "clasificador.h"

// Resolucion en horizontal (rotation 1/3)
#define SCR_W 240
#define SCR_H 135

// Paleta
#define COL_HEADER   TFT_NAVY
#define COL_FOOTER   0x2124            // gris muy oscuro (RGB565)
#define COL_ITEM     0x18E3            // gris item no seleccionado
#define COL_BG       TFT_BLACK

// Color por diagnostico
inline uint16_t colorDiag(Diagnostico d) {
  switch (d) {
    case SANA:        return TFT_GREEN;
    case BOTRYTIS:    return TFT_MAGENTA;   // moho gris -> violeta
    case ANTRACNOSIS: return TFT_ORANGE;
    case PODRIDA:     return TFT_RED;
    default:          return TFT_DARKGREY;
  }
}

// ---------------------------------------------------------------------------
//  Componentes reutilizables
// ---------------------------------------------------------------------------
inline void uiHeader(TFT_eSPI& tft, const String& titulo, bool wifiOk) {
  tft.fillRect(0, 0, SCR_W, 20, COL_HEADER);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, COL_HEADER);
  tft.drawString(titulo, 6, 10, 2);
  // indicador WiFi
  tft.fillCircle(SCR_W - 12, 10, 5, wifiOk ? TFT_GREEN : TFT_RED);
}

inline void uiFooter(TFT_eSPI& tft, const String& b1, const String& b2) {
  tft.fillRect(0, SCR_H - 16, SCR_W, 16, COL_FOOTER);
  tft.setTextColor(TFT_WHITE, COL_FOOTER);
  tft.setTextDatum(ML_DATUM);
  if (b1.length()) tft.drawString("B1 " + b1, 4, SCR_H - 8, 1);
  tft.setTextDatum(MR_DATUM);
  if (b2.length()) tft.drawString(b2 + " B2", SCR_W - 4, SCR_H - 8, 1);
}

// Grafico de barras de las 18 bandas (color por grupo UV/VIS/NIR)
inline void uiEspectro(TFT_eSPI& tft, const float* b, int x, int y, int w, int h) {
  tft.drawRect(x, y, w, h, TFT_DARKGREY);
  float mx = 1e-6f;
  for (int i = 0; i < 18; i++) if (b[i] > mx) mx = b[i];
  int bw = (w - 2) / 18;
  for (int i = 0; i < 18; i++) {
    int bh = (int)((h - 2) * (b[i] / mx));
    if (bh < 1)      bh = 1;
    if (bh > h - 2)  bh = h - 2;
    uint16_t col = (i < 6) ? TFT_CYAN : (i < 12) ? TFT_GREEN : TFT_RED;
    tft.fillRect(x + 1 + i * bw, y + h - 1 - bh, bw - 1, bh, col);
  }
}

// ---------------------------------------------------------------------------
//  Pantallas completas
// ---------------------------------------------------------------------------
inline void uiSplash(TFT_eSPI& tft) {
  tft.fillScreen(COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN, COL_BG);
  tft.drawString("GENIC", SCR_W / 2, 42, 4);
  tft.setTextColor(TFT_WHITE, COL_BG);
  tft.drawString("Diagnostico espectral", SCR_W / 2, 74, 2);
  tft.drawString("de frutas", SCR_W / 2, 94, 2);
}

static const char* MENU_ITEMS[] = { "Diagnostico", "Entrenamiento", "Info" };
static const int    MENU_N      = 3;

inline void uiMenu(TFT_eSPI& tft, int sel, bool wifiOk) {
  tft.fillScreen(COL_BG);
  uiHeader(tft, "Menu principal", wifiOk);
  int y = 28;
  for (int i = 0; i < MENU_N; i++) {
    bool s = (i == sel);
    tft.fillRoundRect(12, y, SCR_W - 24, 26, 5, s ? TFT_GREEN : COL_ITEM);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(s ? TFT_BLACK : TFT_WHITE);
    tft.drawString(MENU_ITEMS[i], SCR_W / 2, y + 13, 2);
    y += 31;
  }
  uiFooter(tft, "Mover", "Elegir");
}

inline void uiPromptMedir(TFT_eSPI& tft, const String& fruta, bool wifiOk) {
  tft.fillScreen(COL_BG);
  uiHeader(tft, "Diagnostico", wifiOk);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, COL_BG);
  tft.drawString("Coloca la fruta", SCR_W / 2, 44, 2);
  tft.drawString("sobre el sensor", SCR_W / 2, 64, 2);
  tft.setTextColor(TFT_CYAN, COL_BG);
  tft.drawString("Fruta: " + fruta, SCR_W / 2, 92, 2);
  uiFooter(tft, "Volver", "MEDIR");
}

inline void uiMidiendo(TFT_eSPI& tft) {
  tft.fillScreen(COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_YELLOW, COL_BG);
  tft.drawString("Midiendo...", SCR_W / 2, SCR_H / 2, 4);
}

inline void uiResultado(TFT_eSPI& tft, Diagnostico d, const Features& f,
                        const float* b, const String& fruta, bool wifiOk) {
  tft.fillScreen(COL_BG);
  uiHeader(tft, "Resultado", wifiOk);

  // Banda de diagnostico
  uint16_t c = colorDiag(d);
  tft.fillRoundRect(8, 23, SCR_W - 16, 28, 6, c);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK, c);
  tft.drawString(DIAG_NOMBRE[d], SCR_W / 2, 37, 4);

  // Grafico espectral de las 18 bandas
  uiEspectro(tft, b, 8, 56, SCR_W - 16, 34);

  // Metricas
  char buf[40];
  snprintf(buf, sizeof(buf), "NDVI %.2f  R/G %.2f  pig %.2f",
           f.ndvi, f.ratioRG, f.pigmento);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, COL_BG);
  tft.drawString(buf, SCR_W / 2, 101, 1);
  tft.setTextColor(TFT_CYAN, COL_BG);
  tft.drawString(fruta, SCR_W / 2, 111, 1);

  uiFooter(tft, "Menu", "Medir");
}

inline void uiEntrenamiento(TFT_eSPI& tft, const String& ip, bool wifiOk,
                            bool fbOk, const String& fruta, const String& estado) {
  tft.fillScreen(COL_BG);
  uiHeader(tft, "Entrenamiento", wifiOk);
  tft.setTextDatum(ML_DATUM);
  int y = 30;
  tft.setTextColor(TFT_WHITE, COL_BG);
  tft.drawString("IP: " + (ip.length() ? ip : String("--")), 8, y, 2); y += 20;
  tft.setTextColor(fbOk ? TFT_GREEN : TFT_ORANGE, COL_BG);
  tft.drawString(String("Firebase: ") + (fbOk ? "ON" : "OFF"), 8, y, 2); y += 20;
  tft.setTextColor(TFT_CYAN, COL_BG);
  tft.drawString("Fruta:  " + fruta, 8, y, 2);  y += 18;
  tft.drawString("Estado: " + estado, 8, y, 2);
  uiFooter(tft, "Salir", "Medir");
}

inline void uiInfo(TFT_eSPI& tft, bool wifiOk) {
  tft.fillScreen(COL_BG);
  uiHeader(tft, "Info", wifiOk);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, COL_BG);
  int y = 30;
  tft.drawString("GENIC - AS7265x 18 bandas", 8, y, 1); y += 14;
  tft.drawString("410-940 nm  (UV/VIS/NIR)", 8, y, 1); y += 14;
  tft.drawString("Clases: sana/botrytis/", 8, y, 1); y += 14;
  tft.drawString("antracnosis/podrida", 8, y, 1); y += 14;
  tft.drawString("Modo 2: web + Firebase RTDB", 8, y, 1);
  uiFooter(tft, "Volver", "");
}

#endif // PANTALLA_H
