// ============================================================================
//  firmware_diagnostico.ino
//  Sistema de diagnostico de patogenos en frutas
//  ESP32-S3 + AS7265x (18 bandas) + OLED SSD1306 + WebServer + Firebase RTDB
// ----------------------------------------------------------------------------
//  MENU SERIAL:
//    1 -> MODO DIAGNOSTICO   (offline: mide, clasifica, muestra en OLED/Serial)
//    2 -> MODO ENTRENAMIENTO (web local + Firebase para capturar dataset)
//    M -> (en cualquier modo) toma una medicion y emite el JSON por Serial,
//         compatible con tu colector_espectral.py
//    h -> ayuda / menu
//    x -> volver al menu principal
//
//  Requiere (Library Manager):
//    - SparkFun AS7265X Arduino Library
//    - ArduinoJson  (>= 7.x)
//    - Adafruit SSD1306 + Adafruit GFX   (si USE_OLED)
//  Placa: "ESP32S3 Dev Module"
// ============================================================================

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <SparkFun_AS7265X.h>

#include "config.h"
#include "clasificador.h"
#include "pagina_web.h"

#ifdef USE_OLED
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  Adafruit_SSD1306 oled(OLED_ANCHO, OLED_ALTO, &Wire, -1);
  bool oledOk = false;
#endif

// ---------------------------------------------------------------------------
//  Estado global
// ---------------------------------------------------------------------------
AS7265X    as7265x;
WebServer  server(WEB_PORT);

enum Modo { MENU, DIAGNOSTICO, ENTRENAMIENTO };
Modo   modoActual = MENU;

bool   sensorOk   = false;
bool   webActiva  = false;
String cfgFruta   = "fresa";
String cfgEstado  = ESTADO_SANA;

float  bandas[NUM_BANDAS];
uint32_t contador = 0;
unsigned long ultimoPollFb = 0;

// ---------------------------------------------------------------------------
//  Utilidades OLED
// ---------------------------------------------------------------------------
void oledMensaje(const String& l1, const String& l2 = "", const String& l3 = "") {
#ifdef USE_OLED
  if (!oledOk) return;
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);  oled.println(l1);
  oled.setCursor(0, 20); oled.println(l2);
  oled.setCursor(0, 40); oled.println(l3);
  oled.display();
#endif
}

void oledDiagnostico(Diagnostico d, const String& fruta) {
#ifdef USE_OLED
  if (!oledOk) return;
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print("Fruta: "); oled.println(fruta);
  oled.setTextSize(2);
  oled.setCursor(0, 26);
  oled.println(DIAG_NOMBRE[d]);
  oled.display();
#endif
}

// ---------------------------------------------------------------------------
//  Sensor
// ---------------------------------------------------------------------------
bool iniciarSensor() {
  if (!as7265x.begin()) {
    Serial.println(F("[ERROR] AS7265x no detectado en I2C. Revisa SDA=8 SCL=9."));
    return false;
  }
  as7265x.setGain(AS_GAIN);
  as7265x.setMeasurementMode(AS_MODE);
  as7265x.setIntegrationCycles(AS_INT_TIME);
  as7265x.setBulbCurrent(AS_BULB_CURRENT, AS7265x_LED_WHITE);
  as7265x.disableBulb(AS7265x_LED_WHITE);   // se enciende solo durante la medicion
  Serial.println(F("[OK] AS7265x inicializado."));
  return true;
}

// Lee las 18 bandas EN ORDEN por longitud de onda (ver clasificador.h)
void leerBandas(float* b) {
  as7265x.takeMeasurementsWithBulb();   // enciende bulbo, integra, apaga
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

String siguienteId() {
  return "m" + String(millis()) + "_" + String(contador++);
}

// ---------------------------------------------------------------------------
//  Construccion de JSON
// ---------------------------------------------------------------------------
// JSON completo (3 grupos UV/VIS/NIR) - para Serial y web
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

// JSON plano para Firebase (fruta, estado, diagnostico + bandas{nm:valor})
void construirDocFirebase(JsonDocument& doc, const float* b) {
  doc["fruta"]       = cfgFruta;
  doc["estado"]      = cfgEstado;
  doc["t_ms"]        = millis();
  doc["diagnostico"] = DIAG_NOMBRE[clasificar(b)];
  JsonObject bb = doc["bandas"].to<JsonObject>();
  for (int i = 0; i < NUM_BANDAS; i++) bb[String(WAVELENGTHS[i])] = b[i];
}

// Mide y emite JSON por Serial (compatible con colector_espectral.py)
void medirYEnviarSerial() {
  if (!sensorOk) { Serial.println(F("{\"error\":\"sensor no disponible\"}")); return; }
  leerBandas(bandas);
  JsonDocument doc;
  construirDoc(doc, bandas, siguienteId(), true);
  serializeJson(doc, Serial);
  Serial.println();
}

// ---------------------------------------------------------------------------
//  FIREBASE (REST API sobre HTTPS)
// ---------------------------------------------------------------------------
#if USAR_FIREBASE
String fbUrl(const String& path) {
  return String(FB_HOST) + path + ".json?auth=" + FB_AUTH;
}

// method: "GET" | "PUT" | "POST" | "PATCH". Devuelve codigo HTTP; resp = cuerpo.
int fbSend(const char* method, const String& path, const String& body, String& resp) {
  if (WiFi.status() != WL_CONNECTED) return -1;
  WiFiClientSecure client;
  client.setInsecure();              // prototipo: sin verificar certificado
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

// Lee /config/{fruta,estado} desde Firebase y actualiza la config local
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

// Publica una medicion en /mediciones (push) y devuelve el id generado
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

// Escucha el nodo /comando; si dice "MEDIR" ejecuta la captura y resetea a "ESPERA"
void fbPoll() {
  if (millis() - ultimoPollFb < FB_POLL_INTERVAL) return;
  ultimoPollFb = millis();

  String resp;
  int code = fbSend("GET", "/comando", "", resp);
  if (code != 200) return;
  resp.trim();
  if (resp == "\"MEDIR\"") {
    Serial.println(F("[FB] Comando MEDIR recibido."));
    oledMensaje("Firebase:", "Midiendo...");
    fbLeerConfigRemota();                 // usa fruta/estado que puso la app movil
    leerBandas(bandas);
    String id = fbPublicarMedicion(bandas);
    String r;
    fbSend("PUT", "/comando", "\"ESPERA\"", r);   // resetea el comando
    Diagnostico d = clasificar(bandas);
    oledDiagnostico(d, cfgFruta);
    Serial.printf("[FB] Medicion publicada id=%s  fruta=%s estado=%s diag=%s\n",
                  id.c_str(), cfgFruta.c_str(), cfgEstado.c_str(), DIAG_NOMBRE[d]);
  }
}

void fbInicializar() {
  String r;
  fbSend("PUT", "/comando", "\"ESPERA\"", r);   // deja el comando en reposo
  Serial.println(F("[FB] Nodo /comando inicializado en ESPERA."));
}
#endif // USAR_FIREBASE

// ---------------------------------------------------------------------------
//  WIFI
// ---------------------------------------------------------------------------
bool wifiConectado() { return WiFi.status() == WL_CONNECTED; }

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
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(400); Serial.print('.');
  }
  Serial.println();
  if (wifiConectado()) {
    Serial.printf("[WiFi] STA conectado -> http://%s\n", WiFi.localIP().toString().c_str());
    return WiFi.localIP().toString();
  }
  Serial.println(F("[WiFi] No se pudo conectar (la web local puede no estar disponible)."));
  return "";
#endif
}

// ---------------------------------------------------------------------------
//  ENDPOINTS WEB
// ---------------------------------------------------------------------------
void handleRoot() {
  server.send_P(200, "text/html", PAGINA_HTML);
}

void handleEstado() {
  JsonDocument doc;
  doc["sensor_ok"]   = sensorOk;
  doc["wifi_ok"]     = wifiConectado() || (WIFI_MODO == WIFI_MODO_AP);
  doc["ip"]          = (WIFI_MODO == WIFI_MODO_AP) ? WiFi.softAPIP().toString()
                                                   : WiFi.localIP().toString();
  doc["firebase_ok"] = (USAR_FIREBASE == 1);
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
  // Refleja la config tambien en Firebase para que la app movil la vea
  JsonDocument doc; doc["fruta"] = cfgFruta; doc["estado"] = cfgEstado;
  String body; serializeJson(doc, body); String r;
  fbSend("PUT", "/config", body, r);
#endif
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleMedir() {
  if (!sensorOk) { server.send(503, "application/json", "{\"error\":\"sensor\"}"); return; }
  leerBandas(bandas);
  JsonDocument doc;
  construirDoc(doc, bandas, siguienteId(), true);
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
#if USAR_FIREBASE
  // Publica tambien en Firebase cuando se mide desde la web (opcional)
  fbPublicarMedicion(bandas);
#endif
  Serial.println(F("[WEB] Medicion realizada desde /medir"));
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
//  MENU / MODOS
// ---------------------------------------------------------------------------
void imprimirMenu() {
  Serial.println();
  Serial.println(F("========= DIAGNOSTICO ESPECTRAL DE FRUTAS ========="));
  Serial.println(F("  1 -> MODO DIAGNOSTICO   (offline, clasifica en vivo)"));
  Serial.println(F("  2 -> MODO ENTRENAMIENTO (web + Firebase, captura datos)"));
  Serial.println(F("  M -> Medir y enviar JSON por Serial (colector Python)"));
  Serial.println(F("  h -> Mostrar este menu     x -> Volver al menu"));
  Serial.println(F("==================================================="));
  Serial.print(F("Selecciona una opcion: "));
}

void entrarDiagnostico() {
  modoActual = DIAGNOSTICO;
  Serial.println(F("\n[MODO 1] DIAGNOSTICO. Pulsa 'm' para medir, 'x' para salir."));
  oledMensaje("MODO DIAGNOSTICO", "Pulsa 'm' para", "medir una fruta");
}

void ejecutarDiagnostico() {
  if (!sensorOk) { Serial.println(F("Sensor no disponible.")); return; }
  oledMensaje("Midiendo...", "manten la fruta", "sobre el sensor");
  leerBandas(bandas);
  Diagnostico d = clasificar(bandas);
  Features f = calcularFeatures(bandas);
  Serial.println(F("\n----- RESULTADO -----"));
  Serial.printf("Fruta   : %s\n", cfgFruta.c_str());
  Serial.printf("Diag    : %s\n", DIAG_NOMBRE[d]);
  Serial.printf("NDVI=%.3f ratioRG=%.3f pigmento=%.3f nirRatio=%.3f total=%.0f\n",
                f.ndvi, f.ratioRG, f.pigmento, f.nirRatio, f.grisTotal);
  Serial.println(F("---------------------"));
  oledDiagnostico(d, cfgFruta);
}

void entrarEntrenamiento() {
  modoActual = ENTRENAMIENTO;
  Serial.println(F("\n[MODO 2] ENTRENAMIENTO / CAPTURA."));
  oledMensaje("MODO", "ENTRENAMIENTO", "Conectando WiFi...");
  String ip = iniciarWiFi();
  iniciarServidorWeb();
#if USAR_FIREBASE
  if (wifiConectado()) fbInicializar();
#endif
  oledMensaje("Web lista:", ip.length() ? ip : String("modo AP"),
              (USAR_FIREBASE ? "Firebase: ON" : "Firebase: OFF"));
  Serial.println(F("Abre la IP en el navegador. 'x' para salir."));
}

void salirAlMenu() {
  if (modoActual == ENTRENAMIENTO && webActiva) {
    server.stop();
    webActiva = false;
    // Nota: se mantiene el WiFi por si vuelves a entrar rapido.
  }
  modoActual = MENU;
  oledMensaje("Menu principal", "1=Diagnostico", "2=Entrenamiento");
  imprimirMenu();
}

// ---------------------------------------------------------------------------
//  Manejo de entrada Serial
// ---------------------------------------------------------------------------
void procesarSerial() {
  if (!Serial.available()) return;
  char c = Serial.read();
  if (c == '\n' || c == '\r' || c == ' ') return;

  // 'M' mayuscula: siempre emite JSON por Serial (compatibilidad colector)
  if (c == 'M') { medirYEnviarSerial(); return; }
  if (c == 'h') { imprimirMenu(); return; }
  if (c == 'x') { salirAlMenu(); return; }

  switch (modoActual) {
    case MENU:
      if (c == '1') entrarDiagnostico();
      else if (c == '2') entrarEntrenamiento();
      else { Serial.printf("Opcion '%c' no valida.\n", c); imprimirMenu(); }
      break;
    case DIAGNOSTICO:
      if (c == 'm') ejecutarDiagnostico();
      break;
    case ENTRENAMIENTO:
      // La interaccion es por web/Firebase; 'm' fuerza una captura local
      if (c == 'm') { leerBandas(bandas);
#if USAR_FIREBASE
        fbPublicarMedicion(bandas);
#endif
        Serial.println(F("[MODO 2] Medicion local publicada.")); }
      break;
  }
}

// ---------------------------------------------------------------------------
//  SETUP / LOOP
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin(PIN_SDA, PIN_SCL, I2C_FREQ_HZ);

#ifdef USE_OLED
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, OLED_DIR_I2C);
  if (oledOk) { oled.clearDisplay(); oled.display(); }
  else Serial.println(F("[WARN] OLED SSD1306 no detectado."));
#endif

  oledMensaje("Iniciando...", "AS7265x", "");
  sensorOk = iniciarSensor();

  oledMensaje("Listo", "1=Diagnostico", "2=Entrenamiento");
  imprimirMenu();
}

void loop() {
  procesarSerial();

  if (modoActual == ENTRENAMIENTO) {
    if (webActiva) server.handleClient();
#if USAR_FIREBASE
    if (wifiConectado()) fbPoll();
#endif
  }
}
