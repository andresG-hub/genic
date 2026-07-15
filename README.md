# GENIC — Diagnóstico espectral de patógenos en frutas (LilyGO T-Display + AS7265x)

Sistema de clasificación de estado sanitario de frutas (sana / botrytis /
antracnosis / podrida) a partir de las **18 bandas espectrales** (410–940 nm)
del sensor **SparkFun AS7265x**, con dos modos de trabajo: diagnóstico en tiempo
real y captura de dataset controlada desde web local y/o app móvil vía Firebase.

**Hardware:** placa **LilyGO T-Display** (ESP32 + pantalla ST7789 IPS 1.14"
240×135, 2 botones). La HMI se navega con los botones físicos y muestra el
resultado con un gráfico espectral en color. Ver **[CONEXIONES.md](CONEXIONES.md)**
para el cableado del sensor y la configuración de la pantalla.

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
  firmware_diagnostico.ino   Firmware principal (HMI, modos, WebServer, Firebase)
  config.h                   Credenciales, pines, parámetros (EDITAR ANTES DE COMPILAR)
  clasificador.h             Modelo embebido (árbol de decisión + tabla de umbrales)
  pantalla.h                 Interfaz HMI para la pantalla TFT (TFT_eSPI)
  pagina_web.h               Interfaz web embebida (SPA en PROGMEM)
firebase/
  reglas.json                Reglas de Realtime Database
  estructura_ejemplo.json    Estructura de datos de ejemplo
python/
  export_arbol.py            Entrena y exporta tu árbol sklearn a C++
CONEXIONES.md                Esquema de cableado + configuración de TFT_eSPI
```

---

## 1) Firmware ESP32 (LilyGO T-Display)

### Librerías necesarias (Arduino Library Manager)
- **SparkFun AS7265X Arduino Library**
- **ArduinoJson** (≥ 7.x)
- **TFT_eSPI** (Bodmer) — configurada para la T-Display (ver [CONEXIONES.md](CONEXIONES.md))

Placa: **ESP32 Dev Module**. Sensor por I2C en **SDA=21 / SCL=22**. La pantalla
ST7789 y los botones (GPIO 0 y 35) son de la propia placa.

> ⚠️ **Paso obligatorio:** activa el perfil de la T-Display en TFT_eSPI
> (`Setup25_TTGO_T_Display`). Si no lo haces, la pantalla se queda en blanco.
> Instrucciones en [CONEXIONES.md](CONEXIONES.md).

### Antes de compilar
Edita `config.h`:
- `WIFI_MODO` → `WIFI_MODO_STA` (router, necesario para Firebase) o `WIFI_MODO_AP` (sin internet).
- `STA_SSID` / `STA_PASSWORD` o `AP_SSID` / `AP_PASSWORD`.
- `USAR_FIREBASE`, `FB_HOST` (ya apunta a `genic-76302`), `FB_AUTH` (déjalo `""` si tus reglas están abiertas).

### HMI — navegación por botones
La pantalla muestra: **Splash → Menú** (Diagnóstico / Entrenamiento / Info).

| Pantalla      | BTN1 (GPIO0)      | BTN2 (GPIO35)   |
|---------------|-------------------|-----------------|
| Menú          | Mover selección   | Elegir          |
| Diagnóstico   | Volver            | **MEDIR**       |
| Resultado     | Volver            | Medir de nuevo  |
| Entrenamiento | Salir             | Medir local     |

El **Resultado** muestra el diagnóstico en color (verde/violeta/naranja/rojo)
más un **gráfico de las 18 bandas** (UV cian, VIS verde, NIR rojo) y las métricas
NDVI / R-G / pigmento.

### Atajos por Serial (115200 baudios)
`1` diagnóstico · `2` entrenamiento · `i` info · `x` menú · `m` medir ·
`h` ayuda · **`M` emite el JSON por Serial — compatible con `colector_espectral.py`**.

> El comando `M` sigue funcionando igual, así que tu pipeline Python → CSV no cambia.

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

### 3.1 Proyecto (ya configurado: `genic-76302`)
- **databaseURL:** `https://genic-76302-default-rtdb.firebaseio.com` (ya está en `config.h`).
- **Reglas:** actualmente abiertas (`.read`/`.write: true`), por lo que el ESP32
  no necesita `auth` → `FB_AUTH` se deja `""` en `config.h`.

> ⚠️ Con reglas abiertas cualquiera con la URL puede leer/escribir. Sirve para
> prototipar, pero **antes de exponerlo** activa **Authentication** y restringe
> `.read`/`.write` por `uid` (y entonces pon un token/secret en `FB_AUTH`).
>
> Nota: el `apiKey` de la config web de Firebase **no es un secreto** (va en los
> clientes; la seguridad la dan las reglas). El que nunca debe filtrarse es el
> *database secret* / token de servidor.

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

// Config real del proyecto genic-76302
const firebaseConfig = {
  apiKey: "AIzaSyB1r3EcCmgLDwYv19v6T-ZxbhYbABtYlW4",
  authDomain: "genic-76302.firebaseapp.com",
  databaseURL: "https://genic-76302-default-rtdb.firebaseio.com",
  projectId: "genic-76302",
  storageBucket: "genic-76302.firebasestorage.app",
  messagingSenderId: "331511723864",
  appId: "1:331511723864:web:0cef3b3853da733bdb572d",
  measurementId: "G-1YZH5JF8NW"
};
const app = initializeApp(firebaseConfig);
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
val db = FirebaseDatabase.getInstance("https://genic-76302-default-rtdb.firebaseio.com")

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
- Placa **LilyGO T-Display**; sensor AS7265x por I2C en **GPIO 21 (SDA) / 22 (SCL)**.
  No uses 8/9 en el ESP32 clásico (van a la flash). Detalle en [CONEXIONES.md](CONEXIONES.md).
- Mantén la fruta a distancia constante del sensor y usa siempre el bulbo
  (`takeMeasurementsWithBulb`) para que las mediciones sean comparables.
- Las bandas se leen **calibradas** y en orden por longitud de onda; ese orden
  es idéntico en el CSV, en el firmware y en el script de export.
- I2C a 100 kHz por estabilidad del AS7265x.

## Pendiente / próximos pasos
- **Firestore para dataset + modelos ML:** hoy el firmware publica cada medición
  en **Realtime Database** (`/mediciones`), que encaja como canal en tiempo real.
  Si quieres que el *dataset* y los *modelos* vivan en **Firestore** (como planteaste),
  falta definir el esquema de colecciones. Recomendación: que una **Cloud Function**
  copie cada nuevo `/mediciones/{id}` de RTDB a una colección de Firestore, dejando
  al ESP32 simple. Comparte la estructura de Firestore y lo integramos.
