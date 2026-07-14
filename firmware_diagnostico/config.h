// ============================================================================
//  config.h  -  Configuracion global del sistema de diagnostico de patogenos
//  ESP32-S3 + AS7265x (18 bandas, 410-940 nm)
// ----------------------------------------------------------------------------
//  Edita este archivo con tus credenciales antes de compilar.
//  NO subas este archivo con credenciales reales a un repo publico.
// ============================================================================
#ifndef CONFIG_H
#define CONFIG_H

// ---------------------------------------------------------------------------
//  1) HARDWARE / PINES
// ---------------------------------------------------------------------------
// I2C compartido por el AS7265x y el OLED SSD1306
#define PIN_SDA            8
#define PIN_SCL            9
#define I2C_FREQ_HZ        100000UL   // el AS7265x es sensible; 100 kHz es seguro

// OLED SSD1306 (opcional). Comenta la linea para desactivar el display.
#define USE_OLED
#define OLED_ANCHO         128
#define OLED_ALTO          64
#define OLED_DIR_I2C       0x3C       // 0x3C o 0x3D segun el modulo

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
//  3) FIREBASE REALTIME DATABASE (REST API)
// ---------------------------------------------------------------------------
// Activa/desactiva Firebase. Si esta en 0, el Modo Entrenamiento funciona
// solo con la web local (sin internet).
#define USAR_FIREBASE      1

// URL de tu Realtime Database SIN la barra final, ej:
//   https://mi-proyecto-default-rtdb.firebaseio.com
#define FB_HOST            "https://TU-PROYECTO-default-rtdb.firebaseio.com"

// Database secret (Configuracion > Cuentas de servicio > Secretos de BD)
// o un idToken si usas Auth. Para prototipo se usa el secret legado.
#define FB_AUTH            "TU_DATABASE_SECRET"

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
