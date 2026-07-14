# Diagnóstico espectral de patógenos en frutas — ESP32-S3 + AS7265x

Sistema de clasificación de estado sanitario de frutas (sana / botrytis /
antracnosis / podrida) a partir de las **18 bandas espectrales** (410–940 nm)
del sensor **SparkFun AS7265x**, con dos modos de trabajo: diagnóstico en tiempo
real y captura de dataset controlada desde web local y/o app móvil vía Firebase.

```
ESP32  →  JSON Serial  →  Python (colector_espectral.py)  →  CSV
                                                              │
                            export_arbol.py (sklearn → C++)   │
                                     │                        ▼
                              clasificador.h  ←──────  modelo RF/SVM/árbol
                                     │
        ┌────────────────────────────┴───────────────────────────┐
        ▼                                                          ▼
  MODO 1 Diagnóstico (offline)                    MODO 2 Entrenamiento (web + Firebase)
```

## Estructura del repositorio

```
firmware_diagnostico/
  firmware_diagnostico.ino   Firmware principal (menú, modos, WebServer, Firebase)
  config.h                   Credenciales, pines, parámetros (EDITAR ANTES DE COMPILAR)
  clasificador.h             Modelo embebido (árbol de decisión + tabla de umbrales)
  pagina_web.h               Interfaz web embebida (SPA en PROGMEM)
firebase/
  reglas.json                Reglas de Realtime Database
  estructura_ejemplo.json    Estructura de datos de ejemplo
python/
  export_arbol.py            Entrena y exporta tu árbol sklearn a C++
```

---

## 1) Firmware ESP32

### Librerías necesarias (Arduino Library Manager)
- **SparkFun AS7265X Arduino Library**
- **ArduinoJson** (≥ 7.x)
- **Adafruit SSD1306** + **Adafruit GFX** (solo si usas OLED)

Placa: **ESP32S3 Dev Module**. I2C compartido en **SDA=8 / SCL=9** (sensor + OLED).

### Antes de compilar
Edita `config.h`:
- `WIFI_MODO` → `WIFI_MODO_STA` (router, necesario para Firebase) o `WIFI_MODO_AP` (sin internet).
- `STA_SSID` / `STA_PASSWORD` o `AP_SSID` / `AP_PASSWORD`.
- `USAR_FIREBASE`, `FB_HOST`, `FB_AUTH`.
- Si no tienes OLED, comenta `#define USE_OLED`.

### Menú Serial (115200 baudios)
| Tecla | Acción |
|-------|--------|
| `1`   | **Modo Diagnóstico** (offline). En este modo, `m` mide y clasifica. |
| `2`   | **Modo Entrenamiento** (levanta WiFi + web + Firebase). |
| `M`   | Mide y emite el JSON por Serial — **compatible con `colector_espectral.py`**. |
| `h`   | Muestra el menú. |
| `x`   | Vuelve al menú principal. |

> El comando `M` sigue funcionando igual que en tu firmware actual, así que tu
> pipeline Python → CSV no cambia.

### JSON que emite el sensor
```json
{
  "id": "m84213_0",
  "fruta": "fresa",
  "estado": "botrytis",
  "t_ms": 84213,
  "diagnostico": "BOTRYTIS",
  "UV":  { "410": 123.4, "435": 140.1, "460": 155.9, "485": 170.2, "510": 210.7, "535": 260.3 },
  "VIS": { "560": 305.8, "585": 288.4, "610": 250.0, "645": 190.6, "680": 160.2, "705": 300.9 },
  "NIR": { "730": 520.4, "760": 610.7, "810": 640.1, "860": 655.3, "900": 500.8, "940": 470.2 }
}
```

---

## 2) Página web embebida

Servida por el ESP32 en el **Modo Entrenamiento**. Al entrar, el Serial muestra
la IP (STA) o crea el AP `DiagnosticoFrutas`. Abre esa IP en el navegador.

Endpoints REST locales:
| Método | Ruta       | Descripción |
|--------|------------|-------------|
| GET    | `/`        | Página HTML (dropdowns fruta/estado, botón Medir, visor JSON). |
| GET    | `/estado`  | Estado del equipo + config actual (JSON). |
| POST   | `/config`  | Guarda `fruta` y `estado` (form-urlencoded). |
| GET    | `/medir`   | Toma medición, devuelve el JSON completo (y lo publica en Firebase si está activo). |

La web funciona **sin internet** en modo AP; Firebase es opcional.

---

## 3) Firebase Realtime Database y app móvil

### 3.1 Crear el proyecto
1. En [Firebase Console](https://console.firebase.google.com) crea un proyecto.
2. Build → **Realtime Database** → *Create Database* → modo de prueba.
3. Copia la URL (`https://TU-PROYECTO-default-rtdb.firebaseio.com`) → `FB_HOST`.
4. Configuración del proyecto → *Cuentas de servicio* → *Secretos de base de datos*
   → copia el secret → `FB_AUTH`.
5. Pega el contenido de `firebase/reglas.json` en la pestaña *Reglas*.

> Las reglas del repo están abiertas para prototipar. Para producción, activa
> **Authentication** y restringe `.read`/`.write` por `uid`.

### 3.2 Estructura de datos
```
/comando                 "MEDIR" | "ESPERA"
/config/fruta            "fresa"
/config/estado           "botrytis"
/mediciones/{pushId}/
      fruta, estado, diagnostico, t_ms
      bandas/{410..940}  valores calibrados
```

### 3.3 Protocolo (cómo dispara la medición la app)
1. La app escribe `/config/fruta` y `/config/estado`.
2. La app pone `/comando = "MEDIR"`.
3. El ESP32 (que hace *polling* cada `FB_POLL_INTERVAL` ms) detecta `"MEDIR"`,
   lee `/config`, toma la medición, la publica en `/mediciones/{pushId}` y
   resetea `/comando = "ESPERA"`.
4. La app tiene un *listener* en `/mediciones` y muestra el JSON al instante.

### 3.4 Conectar la app móvil (ejemplos)

**React Native / JS (Firebase Web SDK v9+):**
```js
import { initializeApp } from "firebase/app";
import { getDatabase, ref, set, onChildAdded, query, limitToLast } from "firebase/database";

const app = initializeApp({ databaseURL: "https://TU-PROYECTO-default-rtdb.firebaseio.com" });
const db  = getDatabase(app);

// 1) Configurar fruta + estado
async function configurar(fruta, estado) {
  await set(ref(db, "config"), { fruta, estado });
}

// 2) Disparar medición remota
async function medir() {
  await set(ref(db, "comando"), "MEDIR");
}

// 3) Escuchar nuevas mediciones en tiempo real
onChildAdded(query(ref(db, "mediciones"), limitToLast(1)), (snap) => {
  console.log("Nueva medición:", snap.val());   // { fruta, estado, diagnostico, bandas }
});
```

**Android (Kotlin):**
```kotlin
val db = FirebaseDatabase.getInstance("https://TU-PROYECTO-default-rtdb.firebaseio.com")

fun configurar(fruta: String, estado: String) {
    db.getReference("config").setValue(mapOf("fruta" to fruta, "estado" to estado))
}
fun medir() { db.getReference("comando").setValue("MEDIR") }

db.getReference("mediciones").limitToLast(1)
  .addChildEventListener(object : ChildEventListener {
      override fun onChildAdded(s: DataSnapshot, p: String?) { /* mostrar s.value */ }
      override fun onChildChanged(s: DataSnapshot, p: String?) {}
      override fun onChildRemoved(s: DataSnapshot) {}
      override fun onChildMoved(s: DataSnapshot, p: String?) {}
      override fun onCancelled(e: DatabaseError) {}
  })
```

> **Nota sobre el listener:** el ESP32 con `HTTPClient` hace *polling* del nodo
> `/comando` (no streaming), que es lo robusto para la REST API. La app móvil, en
> cambio, sí usa listeners nativos del SDK y ve las mediciones en tiempo real.

---

## 4) Reentrenar y actualizar el modelo embebido

1. Captura muestras (modo `M` → `colector_espectral.py` → CSV, o vía Firebase y
   exporta el nodo `/mediciones`).
2. Genera el árbol en C++:
   ```bash
   pip install pandas scikit-learn
   python python/export_arbol.py dataset.csv --salida clasificador_generado.h --profundidad 5
   ```
3. Copia la función `clasificarArbol()` generada dentro de `clasificador.h`
   (reemplaza la plantilla) y recompila.

- Para calibrar rápido a mano sin sklearn, usa la **tabla de umbrales**
  (`clasificarUmbrales`): pon `#define USAR_ARBOL 0` en `clasificador.h` y ajusta
  los centroides con la media de tus muestras por clase.

### ¿Y Random Forest / SVM?
El ESP32 corre bien un árbol único o una tabla. Para RF/SVM tienes dos caminos:
- Entrenar un **único árbol** que aproxime al bosque (lo que hace `export_arbol.py`).
- Usar [`emlearn`](https://github.com/emlearn/emlearn) o `micromlgen` para exportar
  el RF/SVM completo a C. Puedo integrarlo si lo necesitas.

---

## Notas de hardware
- Mantén la fruta a distancia constante del sensor y usa siempre el bulbo
  (`takeMeasurementsWithBulb`) para que las mediciones sean comparables.
- Las bandas se leen **calibradas** y en orden por longitud de onda; ese orden
  es idéntico en el CSV, en el firmware y en el script de export.
- I2C a 100 kHz por estabilidad del AS7265x.
