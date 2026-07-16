# Conexiones — LilyGO T-Display (ESP32) + AS7265x

Esquema de cableado y configuración de la librería del display para el sistema
GENIC de diagnóstico espectral.

## Placa
- **LilyGO T-Display** — ESP32 (dual-core LX6), pantalla **ST7789V IPS 1.14"
  240×135**, 2 botones programables (GPIO 0 y 35), USB-C.
- El display es **integrado**: no se cablea, solo se configura la librería.

## Pines usados

| Bloque        | Señal        | GPIO T-Display | Notas |
|---------------|--------------|----------------|-------|
| **AS7265x**   | SDA          | **21**         | I2C libre |
|               | SCL          | **22**         | I2C libre |
|               | 3V3          | 3V3            | alimentación lógica |
|               | GND          | GND            | común |
| **Botón 1**   | navegar/volver | 0            | pull-up interno |
| **Botón 2**   | medir/elegir | 35             | input-only (pull-up en placa) |
| **TFT** (fijo)| MOSI/SCLK/CS/DC/RST/BL | 19/18/5/16/23/4 | definidos en config.h |

> ⚠️ En el ESP32 clásico **no uses GPIO 8/9** para I2C: están conectados a la
> memoria flash. Por eso el sensor va en 21/22 (no en 8/9 como en el ESP32-S3).

## Diagrama de cableado del sensor

```
   LilyGO T-Display                    AS7265x (SparkFun Qwiic)
   ┌───────────────┐                   ┌──────────────────────┐
   │           3V3 ├───────────────────┤ 3V3                   │
   │           GND ├───────────────────┤ GND                   │
   │        GPIO21 ├───────────────────┤ SDA                   │
   │        GPIO22 ├───────────────────┤ SCL                   │
   │               │                   │  (bulbo LED blanco     │
   │  [BTN1]  [BTN2]                    │   integrado en placa)  │
   │  GPIO0   GPIO35                    └──────────────────────┘
   │   ST7789 240x135 (integrada)      │
   └───────────────┘
```

- El **bulbo** (LED blanco) va integrado en la placa del AS7265x; el firmware lo
  enciende solo durante la medición (`takeMeasurementsWithBulb`).
- Alimenta el sensor a **3.3 V**. Con la corriente de bulbo por defecto (50 mA)
  el pin 3V3 de la T-Display es suficiente por USB. Si subes el bulbo a 100 mA o
  notas caídas de tensión, alimenta el sensor desde una fuente 3V3 externa y une
  las masas (GND común).
- Si usas cables Qwiic/JST-SH, respeta el orden GND/3V3/SDA/SCL del conector.

## Librería del display (Adafruit — sin editar archivos)

El display usa **Adafruit_ST7789 + Adafruit_GFX**. Los pines se definen en
`config.h` y se pasan al constructor, así que **NO hay que editar ningún archivo
de librería** (ese era el paso frágil de TFT_eSPI que provocaba cuelgues).

### Arduino IDE
1. Instala desde el Library Manager:
   - **"Adafruit GFX Library"**
   - **"Adafruit ST7735 and ST7789 Library"** (arrastra también sus dependencias
     como *Adafruit BusIO* si las pide).
2. Selecciona la placa **"ESP32 Dev Module"** y compila. Listo.

El firmware ya hace el remapeo de SPI y la inicialización:
```cpp
SPI.begin(PIN_TFT_SCLK, -1, PIN_TFT_MOSI, PIN_TFT_CS);  // 18, -, 19, 5
Adafruit_ST7789 tft(&SPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST); // 5, 16, 23
tft.init(135, 240);   // panel del 1.14" (maneja los offsets)
tft.setRotation(1);   // 240x135 apaisado; si sale girado/espejado usa 3
```

> Si la imagen aparece **desplazada** unos píxeles o **girada**, cambia
> `tft.setRotation(1)` por `3` en `setup()`. Los offsets del panel 135×240 los
> resuelve `tft.init(135, 240)`.

### PlatformIO (alternativa)
```ini
[env:ttgo-t-display]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
  adafruit/Adafruit GFX Library
  adafruit/Adafruit ST7735 and ST7789 Library
  bblanchon/ArduinoJson
  sparkfun/SparkFun AS7265X Arduino Library
```

## Navegación de la HMI

| Pantalla        | BTN1 (GPIO0)      | BTN2 (GPIO35)     |
|-----------------|-------------------|-------------------|
| Menú            | Mover selección   | Elegir            |
| Diagnóstico     | Volver al menú    | **MEDIR**         |
| Resultado       | Volver al menú    | Medir de nuevo    |
| Entrenamiento   | Salir (para WiFi) | Medir local       |
| Info            | Volver al menú    | —                 |

También puedes controlar todo por Serial (115200): `1` diagnóstico, `2`
entrenamiento, `i` info, `x` menú, `m` medir, `M` emitir JSON para el colector
Python, `h` ayuda.
