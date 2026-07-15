// ============================================================================
//  firmware_diagnostico.ino
//  Sistema de diagnostico de patogenos en frutas  -  proyecto GENIC
//  PLACA: LilyGO T-Display (ESP32 + ST7789 1.14" 240x135)
//  Sensor: AS7265x (18 bandas) | WiFi + WebServer + Firebase RTDB
// ----------------------------------------------------------------------------
//  NAVEGACION POR BOTONES (HMI en pantalla):
//    BTN1 (GPIO0)  -> Mover seleccion / Volver
//    BTN2 (GPIO35) -> Seleccionar / MEDIR
//
//  ATAJOS SERIE (115200):
//    1=Diagnostico  2=Entrenamiento  i=Info  x=Menu  m=Medir  h=Ayuda
//    M -> emite JSON por Serial compatible con colector_espectral.py
//
//  LIBRERIAS (Library Manager):
//    - SparkFun AS7265X Arduino Library
//    - ArduinoJson (>= 7.x)
//    - TFT_eSPI  (configurada para T-Display: ver CONEXIONES.md)
//  Placa: "ESP32 Dev Module" (o "TTGO LoRa32-OLED" NO; usar ESP32 Dev Module)
// ============================================================================

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <SparkFun_AS7265X.h>
#include <TFT_eSPI.h>

// Diagnostico de arranque: motivo de reinicio + desactivar brownout
#include "esp_system.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "config.h"
#include "clasificador.h"
#include "pantalla.h"
#include "pagina_web.h"

// ---------------------------------------------------------------------------
//  Objetos globales
// ---------------------------------------------------------------------------
AS7265X   as7265x;
TFT_eSPI  tft = TFT_eSPI();
WebServer server(WEB_PORT);

// Estados de la HMI
enum Estado { SPLASH, MENU, DIAG_PROMPT, DIAG_RESULT, TRAIN, INFO };
Estado estado = SPLASH;

int    menuSel   = 0;
bool   sensorOk  = false;
bool   webActiva = false;
String cfgFruta  = "fresa";
String cfgEstado = ESTADO_SANA;
String ipActual  = "";

float       bandas[NUM_BANDAS];
Diagnostico ultimoDiag = DESCONOCIDO;
Features    ultimasFeat;
uint32_t    contador = 0;

unsigned long splashT     = 0;
unsigned long ultimoPollFb = 0;

// Botones (activos en LOW por pull-up)
bool     b1Prev = HIGH, b2Prev = HIGH;
uint32_t b1t = 0, b2t = 0;

// ---------------------------------------------------------------------------
//  Utilidades comunes
// ---------------------------------------------------------------------------
bool wifiConectado() { return WiFi.status() == WL_CONNECTED; }
bool wifiActivo()    { return wifiConectado() || (WIFI_MODO == WIFI_MODO_AP && webActiva); }
bool firebaseOn()    { return (USAR_FIREBASE == 1) && wifiConectado(); }

String siguienteId() { return "m" + String(millis()) + "_" + String(contador++); }

bool leerBotonFlanco(uint8_t pin, bool& prev, uint32_t& t) {
  bool now = digitalRead(pin);
  bool pulsado = false;
  if (prev == HIGH && now == LOW && millis() - t > 180) { pulsado = true; t = millis(); }
  prev = now;
  return pulsado;
}

// ---------------------------------------------------------------------------
//  Sensor
// ---------------------------------------------------------------------------
bool iniciarSensor() {
  if (!as7265x.begin()) {
    Serial.println(F("[ERROR] AS7265x no detectado. Revisa I2C SDA=21 SCL=22."));
    return false;
  }
  as7265x.setGain(AS_GAIN);
  as7265x.setMeasurementMode(AS_MODE);
  as7265x.setIntegrationCycles(AS_INT_TIME);
  as7265x.setBulbCurrent(AS_BULB_CURRENT, AS7265x_LED_WHITE);
  as7265x.disableBulb(AS7265x_LED_WHITE);
  Serial.println(F("[OK] AS7265x inicializado."));
  return true;
}

void leerBandas(float* b) {
  as7265x.takeMeasurementsWithBulb();
  b[0]  = as7265x.getCalibratedA();  // 410
  b[1]  = as7265x.getCalibratedB();  // 435
  b[2]  = as7265x.getCalibratedC();  // 460
  b[3]  = as7265x.getCalibratedD();  // 485
  b[4]  = as7265x.getCalibratedE();  // 510
  b[5]  = as7265x.getCalibratedF();  // 535
  b[6]  = as7265x.getCalibratedG();  // 560
  b[7]  = as7265x.getCalibratedH();  // 585
  b[8]  = as7265x.getCalibratedR();  // 610
  b[9]  = as7265x.getCalibratedI();  // 645
  b[10] = as7265x.getCalibratedS();  // 680
  b[11] = as7265x.getCalibratedJ();  // 705
  b[12] = as7265x.getCalibratedT();  // 730
  b[13] = as7265x.getCalibratedU();  // 760
  b[14] = as7265x.getCalibratedV();  // 810
  b[15] = as7265x.getCalibratedW();  // 860
  b[16] = as7265x.getCalibratedK();  // 900
  b[17] = as7265x.getCalibratedL();  // 940
}

// ---------------------------------------------------------------------------
//  JSON
// ---------------------------------------------------------------------------
void construirDoc(JsonDocument& doc, const float* b, const String& id, bool conDiag) {
  doc["id"]     = id;
  doc["fruta"]  = cfgFruta;
  doc["estado"] = cfgEstado;
  doc["t_ms"]   = millis();
  if (conDiag) doc["diagnostico"] = DIAG_NOMBRE[clasificar(b)];
  JsonObject uv = doc["UV"].to<JsonObject>();
  for (int i = 0; i < 6; i++)   uv[String(WAVELENGTHS[i])]  = b[i];
  JsonObject vis = doc["VIS"].to<JsonObject>();
  for (int i = 6; i < 12; i++)  vis[String(WAVELENGTHS[i])] = b[i];
  JsonObject nir = doc["NIR"].to<JsonObject>();
  for (int i = 12; i < 18; i++) nir[String(WAVELENGTHS[i])] = b[i];
}

void construirDocFirebase(JsonDocument& doc, const float* b) {
  doc["fruta"]       = cfgFruta;
  doc["estado"]      = cfgEstado;
  doc["t_ms"]        = millis();
  doc["diagnostico"] = DIAG_NOMBRE[clasificar(b)];
  JsonObject bb = doc["bandas"].to<JsonObject>();
  for (int i = 0; i < NUM_BANDAS; i++) bb[String(WAVELENGTHS[i])] = b[i];
}

void medirYEnviarSerial() {
  if (!sensorOk) { Serial.println(F("{\"error\":\"sensor no disponible\"}")); return; }
  leerBandas(bandas);
  JsonDocument doc;
  construirDoc(doc, bandas, siguienteId(), true);
  serializeJson(doc, Serial);
  Serial.println();
}

// ---------------------------------------------------------------------------
//  FIREBASE (REST) - auth opcional (reglas abiertas => sin ?auth=)
// ---------------------------------------------------------------------------
#if USAR_FIREBASE
String fbUrl(const String& path) {
  String u = String(FB_HOST) + path + ".json";
  if (strlen(FB_AUTH) > 0) u += String("?auth=") + FB_AUTH;
  return u;
}

int fbSend(const char* method, const String& path, const String& body, String& resp) {
  if (WiFi.status() != WL_CONNECTED) return -1;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, fbUrl(path))) return -2;
  http.addHeader("Content-Type", "application/json");
  int code;
  if      (strcmp(method, "GET")  == 0) code = http.GET();
  else if (strcmp(method, "PUT")  == 0) code = http.PUT(body);
  else if (strcmp(method, "POST") == 0) code = http.POST(body);
  else                                  code = http.sendRequest(method, body);
  resp = http.getString();
  http.end();
  return code;
}

void fbLeerConfigRemota() {
  String resp;
  if (fbSend("GET", "/config", "", resp) == 200 && resp.length() > 2) {
    JsonDocument doc;
    if (deserializeJson(doc, resp) == DeserializationError::Ok) {
      if (!doc["fruta"].isNull())  cfgFruta  = doc["fruta"].as<String>();
      if (!doc["estado"].isNull()) cfgEstado = doc["estado"].as<String>();
    }
  }
}

String fbPublicarMedicion(const float* b) {
  JsonDocument doc; construirDocFirebase(doc, b);
  String body; serializeJson(doc, body);
  String resp;
  int code = fbSend("POST", "/mediciones", body, resp);
  Serial.printf("[FB] POST /mediciones -> %d\n", code);
  if (code == 200) {
    JsonDocument r;
    if (deserializeJson(r, resp) == DeserializationError::Ok)
      return r["name"].as<String>();
  }
  return "";
}

void fbInicializar() {
  String r;
  fbSend("PUT", "/comando", "\"ESPERA\"", r);
  Serial.println(F("[FB] /comando inicializado en ESPERA."));
}

// Escucha /comando; si es "MEDIR" mide, publica y resetea a "ESPERA"
void fbPoll() {
  if (millis() - ultimoPollFb < FB_POLL_INTERVAL) return;
  ultimoPollFb = millis();
  String resp;
  if (fbSend("GET", "/comando", "", resp) != 200) return;
  resp.trim();
  if (resp == "\"MEDIR\"") {
    Serial.println(F("[FB] Comando MEDIR recibido."));
    fbLeerConfigRemota();
    leerBandas(bandas);
    ultimoDiag  = clasificar(bandas);
    ultimasFeat = calcularFeatures(bandas);
    String id = fbPublicarMedicion(bandas);
    String r; fbSend("PUT", "/comando", "\"ESPERA\"", r);
    Serial.printf("[FB] Publicada id=%s diag=%s\n", id.c_str(), DIAG_NOMBRE[ultimoDiag]);
    if (estado == TRAIN) uiEntrenamiento(tft, ipActual, wifiActivo(), firebaseOn(), cfgFruta, cfgEstado);
  }
}
#endif // USAR_FIREBASE

// ---------------------------------------------------------------------------
//  WIFI
// ---------------------------------------------------------------------------
String iniciarWiFi() {
#if WIFI_MODO == WIFI_MODO_AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[WiFi] AP '%s' -> http://%s\n", AP_SSID, ip.toString().c_str());
  return ip.toString();
#else
  WiFi.mode(WIFI_STA);
  WiFi.begin(STA_SSID, STA_PASSWORD);
  Serial.print(F("[WiFi] Conectando"));
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) { delay(400); Serial.print('.'); }
  Serial.println();
  if (wifiConectado()) {
    Serial.printf("[WiFi] STA -> http://%s\n", WiFi.localIP().toString().c_str());
    return WiFi.localIP().toString();
  }
  Serial.println(F("[WiFi] Sin conexion."));
  return "";
#endif
}

// ---------------------------------------------------------------------------
//  ENDPOINTS WEB
// ---------------------------------------------------------------------------
void handleRoot()   { server.send_P(200, "text/html", PAGINA_HTML); }

void handleEstado() {
  JsonDocument doc;
  doc["sensor_ok"]   = sensorOk;
  doc["wifi_ok"]     = wifiActivo();
  doc["ip"]          = ipActual;
  doc["firebase_ok"] = firebaseOn();
  doc["fruta"]       = cfgFruta;
  doc["estado"]      = cfgEstado;
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleConfig() {
  if (server.hasArg("fruta"))  cfgFruta  = server.arg("fruta");
  if (server.hasArg("estado")) cfgEstado = server.arg("estado");
  Serial.printf("[WEB] Config: fruta=%s estado=%s\n", cfgFruta.c_str(), cfgEstado.c_str());
#if USAR_FIREBASE
  JsonDocument doc; doc["fruta"] = cfgFruta; doc["estado"] = cfgEstado;
  String body; serializeJson(doc, body); String r;
  fbSend("PUT", "/config", body, r);
#endif
  if (estado == TRAIN) uiEntrenamiento(tft, ipActual, wifiActivo(), firebaseOn(), cfgFruta, cfgEstado);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleMedir() {
  if (!sensorOk) { server.send(503, "application/json", "{\"error\":\"sensor\"}"); return; }
  leerBandas(bandas);
  ultimoDiag  = clasificar(bandas);
  ultimasFeat = calcularFeatures(bandas);
  JsonDocument doc;
  construirDoc(doc, bandas, siguienteId(), true);
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
#if USAR_FIREBASE
  fbPublicarMedicion(bandas);
#endif
  Serial.println(F("[WEB] Medicion desde /medir"));
}

void iniciarServidorWeb() {
  server.on("/",       HTTP_GET,  handleRoot);
  server.on("/estado", HTTP_GET,  handleEstado);
  server.on("/medir",  HTTP_GET,  handleMedir);
  server.on("/config", HTTP_POST, handleConfig);
  server.onNotFound([]() { server.send(404, "text/plain", "404"); });
  server.begin();
  webActiva = true;
  Serial.println(F("[WEB] Servidor iniciado."));
}

// ---------------------------------------------------------------------------
//  RENDER SEGUN ESTADO
// ---------------------------------------------------------------------------
void renderActual() {
  switch (estado) {
    case SPLASH:      uiSplash(tft); break;
    case MENU:        uiMenu(tft, menuSel, wifiActivo()); break;
    case DIAG_PROMPT: uiPromptMedir(tft, cfgFruta, wifiActivo()); break;
    case DIAG_RESULT: uiResultado(tft, ultimoDiag, ultimasFeat, bandas, cfgFruta, wifiActivo()); break;
    case TRAIN:       uiEntrenamiento(tft, ipActual, wifiActivo(), firebaseOn(), cfgFruta, cfgEstado); break;
    case INFO:        uiInfo(tft, wifiActivo()); break;
  }
}

void cambiarEstado(Estado nuevo) { estado = nuevo; renderActual(); }

// Realiza medicion + clasificacion y muestra el resultado
void ejecutarDiagnostico() {
  if (!sensorOk) { Serial.println(F("Sensor no disponible.")); return; }
  uiMidiendo(tft);
  leerBandas(bandas);
  ultimoDiag  = clasificar(bandas);
  ultimasFeat = calcularFeatures(bandas);
  Serial.printf("[DIAG] %s  NDVI=%.3f R/G=%.3f pig=%.3f total=%.0f\n",
                DIAG_NOMBRE[ultimoDiag], ultimasFeat.ndvi, ultimasFeat.ratioRG,
                ultimasFeat.pigmento, ultimasFeat.grisTotal);
  cambiarEstado(DIAG_RESULT);
}

void entrarEntrenamiento() {
  uiEntrenamiento(tft, "conectando...", false, false, cfgFruta, cfgEstado);
  ipActual = iniciarWiFi();
  iniciarServidorWeb();
#if USAR_FIREBASE
  if (wifiConectado()) fbInicializar();
#endif
  cambiarEstado(TRAIN);
  Serial.printf("[MODO 2] Web en http://%s  (x para salir)\n", ipActual.c_str());
}

void salirDeEntrenamiento() {
  if (webActiva) { server.stop(); webActiva = false; }
  cambiarEstado(MENU);
}

// ---------------------------------------------------------------------------
//  MANEJO DE BOTONES POR ESTADO
// ---------------------------------------------------------------------------
void manejarBotones() {
  bool b1 = leerBotonFlanco(PIN_BTN1, b1Prev, b1t);   // navegar / volver
  bool b2 = leerBotonFlanco(PIN_BTN2, b2Prev, b2t);   // seleccionar / medir
  if (!b1 && !b2) return;

  switch (estado) {
    case SPLASH:
      cambiarEstado(MENU);
      break;

    case MENU:
      if (b1) { menuSel = (menuSel + 1) % MENU_N; renderActual(); }
      if (b2) {
        if      (menuSel == 0) cambiarEstado(DIAG_PROMPT);
        else if (menuSel == 1) entrarEntrenamiento();
        else                   cambiarEstado(INFO);
      }
      break;

    case DIAG_PROMPT:
      if (b1) cambiarEstado(MENU);
      if (b2) ejecutarDiagnostico();
      break;

    case DIAG_RESULT:
      if (b1) cambiarEstado(MENU);
      if (b2) ejecutarDiagnostico();
      break;

    case TRAIN:
      if (b1) salirDeEntrenamiento();
      if (b2) {                       // medicion local manual
        uiMidiendo(tft);
        leerBandas(bandas);
        ultimoDiag  = clasificar(bandas);
        ultimasFeat = calcularFeatures(bandas);
#if USAR_FIREBASE
        fbPublicarMedicion(bandas);
#endif
        uiResultado(tft, ultimoDiag, ultimasFeat, bandas, cfgFruta, wifiActivo());
        delay(2200);
        renderActual();
      }
      break;

    case INFO:
      if (b1) cambiarEstado(MENU);
      break;
  }
}

// ---------------------------------------------------------------------------
//  ATAJOS SERIE
// ---------------------------------------------------------------------------
void manejarSerial() {
  if (!Serial.available()) return;
  char c = Serial.read();
  if (c == '\n' || c == '\r' || c == ' ') return;

  if (c == 'M') { medirYEnviarSerial(); return; }   // colector Python
  switch (c) {
    case '1': cambiarEstado(DIAG_PROMPT); break;
    case '2': entrarEntrenamiento(); break;
    case 'i': cambiarEstado(INFO); break;
    case 'x': if (estado == TRAIN) salirDeEntrenamiento(); else cambiarEstado(MENU); break;
    case 'm': if (estado == DIAG_PROMPT || estado == DIAG_RESULT) ejecutarDiagnostico(); break;
    case 'h':
      Serial.println(F("1=Diag 2=Train i=Info x=Menu m=Medir M=JSON-serial"));
      break;
    default: break;
  }
}

// ---------------------------------------------------------------------------
//  DIAGNOSTICO DE ARRANQUE
// ---------------------------------------------------------------------------
const char* motivoReset(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "Encendido normal (power-on)";
    case ESP_RST_SW:        return "Reinicio por software";
    case ESP_RST_PANIC:     return "PANIC / excepcion (crash de codigo)";
    case ESP_RST_INT_WDT:   return "Watchdog de interrupciones";
    case ESP_RST_TASK_WDT:  return "Watchdog de tarea (algo se bloqueo)";
    case ESP_RST_WDT:       return "Watchdog generico";
    case ESP_RST_BROWNOUT:  return "BROWNOUT (caida de tension / alimentacion)";
    case ESP_RST_DEEPSLEEP: return "Salida de deep sleep";
    case ESP_RST_EXT:       return "Reset externo (boton)";
    default:                return "Desconocido";
  }
}

// ---------------------------------------------------------------------------
//  SETUP / LOOP
// ---------------------------------------------------------------------------
void setup() {
  // Desactiva el detector de brownout ANTES de nada. Evita reinicios por los
  // picos de corriente del bulbo del sensor o de la radio WiFi cuando la
  // alimentacion USB es debil (causa muy comun de "se reinicia solo").
  // El nombre del registro cambia segun la version del core ESP32:
  //   ESP32 clasico (2.0.x) -> RTC_CNTL_BROWN_OUT_REG
#if defined(RTC_CNTL_BROWN_OUT_REG)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#elif defined(RTC_CNTL_BROWNOUT_REG)
  WRITE_PERI_REG(RTC_CNTL_BROWNOUT_REG, 0);
#endif

  Serial.begin(115200);
  delay(300);
  Serial.printf("\n\n[BOOT] GENIC arrancando...\n[BOOT] Motivo del ultimo reinicio: %s\n",
                motivoReset(esp_reset_reason()));

  // Retroiluminacion del TFT
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);

  // Pantalla
  Serial.println(F("[BOOT] Iniciando TFT..."));
  tft.init();
  tft.setRotation(1);          // horizontal 240x135
  tft.fillScreen(TFT_BLACK);
  Serial.println(F("[BOOT] TFT OK."));

  // Botones
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT);     // GPIO35 es input-only (pull-up en la placa)

  // I2C + sensor
  Serial.println(F("[BOOT] Iniciando I2C + AS7265x..."));
  Wire.begin(PIN_SDA, PIN_SCL, I2C_FREQ_HZ);
  sensorOk = iniciarSensor();
  Serial.printf("[BOOT] Sensor: %s\n", sensorOk ? "OK" : "NO detectado (se continua)");
  if (!sensorOk) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Sensor AS7265x", SCR_W / 2, 55, 2);
    tft.drawString("no detectado", SCR_W / 2, 75, 2);
    delay(1500);
  }

  // Splash inicial
  splashT = millis();
  cambiarEstado(SPLASH);
  Serial.println(F("GENIC listo. Botones: B1=navegar B2=seleccionar. 'h'=ayuda serie."));
}

void loop() {
  // Salida automatica del splash
  if (estado == SPLASH && millis() - splashT > 1800) cambiarEstado(MENU);

  manejarBotones();
  manejarSerial();

  if (estado == TRAIN) {
    if (webActiva) server.handleClient();
#if USAR_FIREBASE
    if (wifiConectado()) fbPoll();
#endif
  }
}
