// ============================================================================
//  config.h  -  Configuracion global del sistema de diagnostico de patogenos
//  PLACA: LilyGO T-Display (ESP32 + ST7789 1.14" 240x135) + AS7265x (18 bandas)
// ----------------------------------------------------------------------------
//  Edita este archivo con tus credenciales antes de compilar.
//  NO subas este archivo con credenciales reales a un repo publico.
// ============================================================================
#ifndef CONFIG_H
#define CONFIG_H

// ---------------------------------------------------------------------------
//  1) HARDWARE / PINES  (LilyGO T-Display)
// ---------------------------------------------------------------------------
//  El display ST7789 usa SPI en pines FIJOS de la placa. Esos pines se
//  configuran EN LA LIBRERIA TFT_eSPI (ver CONEXIONES.md), no aqui:
//     TFT_MOSI=19  TFT_SCLK=18  TFT_CS=5  TFT_DC=16  TFT_RST=23  TFT_BL=4
//
//  El AS7265x se conecta por I2C en los pines LIBRES 21/22.
//  (En el ESP32 clasico NO uses 8/9: estan unidos a la memoria flash.)
#define PIN_SDA            21
#define PIN_SCL            22
#define I2C_FREQ_HZ        100000UL   // el AS7265x es sensible; 100 kHz es seguro

// Botones fisicos integrados de la T-Display
//   BTN1 (GPIO 0)  -> NAVEGAR / VOLVER   (tiene pull-up interno)
//   BTN2 (GPIO 35) -> SELECCIONAR / MEDIR (input-only, pull-up en placa)
#define PIN_BTN1           0
#define PIN_BTN2           35

// Retroiluminacion del TFT (la controla TFT_eSPI via TFT_BL=4)
#define PIN_TFT_BL         4

// ---------------------------------------------------------------------------
//  2) WIFI
// ---------------------------------------------------------------------------
// Modo de red para el Modo Entrenamiento:
//   WIFI_MODO_AP  -> el ESP32 crea su propia red (sin internet, web local)
//   WIFI_MODO_STA -> el ESP32 se conecta a tu router (necesario para Firebase)
#define WIFI_MODO_AP       0
#define WIFI_MODO_STA      1
#define WIFI_MODO          WIFI_MODO_STA   // <-- cambia segun tu caso

// Credenciales STA (tu router / hotspot)
#define STA_SSID           "TU_SSID"
#define STA_PASSWORD       "TU_PASSWORD"

// Credenciales del Access Point que crea el ESP32 (modo AP)
#define AP_SSID            "DiagnosticoFrutas"
#define AP_PASSWORD        "espectral2024"     // minimo 8 caracteres

// Puerto del servidor web embebido
#define WEB_PORT           80

// ---------------------------------------------------------------------------
//  3) FIREBASE REALTIME DATABASE (REST API)  -  proyecto: genic-76302
// ---------------------------------------------------------------------------
// Activa/desactiva Firebase. Si esta en 0, el Modo Entrenamiento funciona
// solo con la web local (sin internet).
#define USAR_FIREBASE      1

// URL de tu Realtime Database SIN la barra final.
#define FB_HOST            "https://genic-76302-default-rtdb.firebaseio.com"

// Autenticacion:
//   - Si tus reglas estan abiertas (".read"/".write": true) deja "" (vacio).
//   - Si usas seguridad, pon aqui el database secret o un idToken de Auth.
#define FB_AUTH            ""

// Cada cuanto (ms) el ESP32 consulta el nodo /comando en Firebase
#define FB_POLL_INTERVAL   1500

// ---------------------------------------------------------------------------
//  4) SENSOR / MEDICION
// ---------------------------------------------------------------------------
// Numero de bandas espectrales del AS7265x
#define NUM_BANDAS         18

// Tiempo de integracion e ingesta del bulbo (ver datasheet AS7265x)
// GAIN: 0=1x, 1=3.7x, 2=16x, 3=64x
#define AS_GAIN            3
// Modo de medicion: 3 = todos los canales continuos
#define AS_MODE            3
// Tiempo de integracion (x2.8ms). 64 => ~180ms por lectura
#define AS_INT_TIME        64

// Corriente del LED bulbo (LED blanco) durante la medicion:
// 0=12.5mA, 1=25mA, 2=50mA, 3=100mA
#define AS_BULB_CURRENT    2

// ---------------------------------------------------------------------------
//  5) ETIQUETAS (deben coincidir con tu dataset / app movil / Firebase)
// ---------------------------------------------------------------------------
// Estados posibles (clases del modelo)
#define ESTADO_SANA        "sana"
#define ESTADO_BOTRYTIS    "botrytis"
#define ESTADO_ANTRACNOSIS "antracnosis"
#define ESTADO_PODRIDA     "podrida"

// Frutas soportadas (informativas; la app movil define el dropdown)
// "fresa", "mango", "uva", "arandano", ...

#endif // CONFIG_H
