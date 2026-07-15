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
| **TFT** (fijo)| MOSI/SCLK/CS/DC/RST/BL | 19/18/5/16/23/4 | los gestiona TFT_eSPI |

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

## Configuración de TFT_eSPI (¡imprescindible!)

La librería **TFT_eSPI** define los pines del display en tiempo de compilación.
Hay que decirle que use el perfil de la T-Display.

### Arduino IDE
1. Instala **TFT_eSPI** (Bodmer) desde el Library Manager.
2. Abre el archivo `User_Setup_Select.h` dentro de la carpeta de la librería
   (normalmente `Documentos/Arduino/libraries/TFT_eSPI/`).
3. **Comenta** la línea del setup por defecto y **descomenta** la de la T-Display:
   ```cpp
   // #include <User_Setup.h>                           // <-- comentar esta
   #include <User_Setups/Setup25_TTGO_T_Display.h>      // <-- descomentar esta
   ```
4. Selecciona la placa **"ESP32 Dev Module"** y compila.

### PlatformIO (alternativa)
En `platformio.ini`:
```ini
[env:ttgo-t-display]
platform = espressif32
board = esp32dev
framework = arduino
build_flags =
  -DUSER_SETUP_LOADED=1
  -DST7789_DRIVER=1
  -DTFT_WIDTH=135
  -DTFT_HEIGHT=240
  -DTFT_MOSI=19
  -DTFT_SCLK=18
  -DTFT_CS=5
  -DTFT_DC=16
  -DTFT_RST=23
  -DTFT_BL=4
  -DTFT_BACKLIGHT_ON=1
  -DLOAD_GLCD=1
  -DLOAD_FONT2=1
  -DLOAD_FONT4=1
  -DSPI_FREQUENCY=40000000
lib_deps =
  bodmer/TFT_eSPI
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
