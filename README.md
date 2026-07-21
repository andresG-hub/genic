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
  frutas.h                   Catálogo de frutas + estados (fuente única)
  pantalla.h                 Interfaz HMI para la pantalla TFT (Adafruit_ST7789)
  logo_genic.h               Logo del splash (RGB565 240×135, autogenerado)
  pagina_web.h               Web embebida + portal WiFi (SPA en PROGMEM)
firebase/
  reglas.json                Reglas de Realtime Database
  estructura_ejemplo.json    Estructura de datos RTDB de ejemplo
  firestore_estructura.json  Estructura del dataset en Firestore
  init_firestore.py          Crea el catálogo de frutas (Python, service account)
  init_firestore.html        Crea el catálogo de frutas (navegador, sin clave)
  importar_csv.html          Importa CSV al dataset (navegador)
  importar_csv.py            Importa CSV al dataset (Python, bulk + n_mediciones)
python/
  export_arbol.py            Entrena y exporta tu árbol sklearn a C++
  generar_logo.py            Convierte assets/genicLCD.bmp -> logo_genic.h
assets/
  genicLCD.bmp / genic_lcd.c Fuente del logo (fuera del sketch para no compilarse)
CONEXIONES.md                Esquema de cableado + librerías del display
```

---

## 1) Firmware ESP32 (LilyGO T-Display)

### Librerías necesarias (Arduino Library Manager)
- **SparkFun AS7265X Arduino Library**
- **ArduinoJson** (≥ 7.x)
- **Adafruit GFX Library**
- **Adafruit ST7735 and ST7789 Library**

Placa: **ESP32 Dev Module**. Sensor por I2C en **SDA=21 / SCL=22**. La pantalla
ST7789 y los botones (GPIO 0 y 35) son de la propia placa.

> ✅ Los pines del display están en `config.h` y se pasan al constructor de
> Adafruit_ST7789: **no hay que editar archivos de librería**. Detalles en
> [CONEXIONES.md](CONEXIONES.md).

### Antes de compilar
Edita `config.h`:
- `WIFI_MODO` → `WIFI_MODO_STA` (router, necesario para Firebase) o `WIFI_MODO_AP` (sin internet).
- `STA_SSID` / `STA_PASSWORD` o `AP_SSID` / `AP_PASSWORD`.
- `USAR_FIREBASE`, `FB_HOST` (ya apunta a `genic-76302`), `FB_AUTH` (déjalo `""` si tus reglas están abiertas).

### HMI — navegación por botones
Al encender se muestra el **logo GENIC** (splash, imagen a pantalla completa) y
luego el **Menú**: Diagnóstico / Entrenamiento / WiFi / Info.

| Pantalla      | BTN1 (GPIO0)      | BTN2 (GPIO35)     |
|---------------|-------------------|-------------------|
| Menú          | Mover selección   | Elegir            |
| Diagnóstico   | Volver            | **MEDIR**         |
| Resultado     | Volver            | Medir de nuevo    |
| Entrenamiento | Salir             | Medir local       |
| WiFi          | Volver            | Configurar (portal) |

El **Resultado** muestra el diagnóstico en color (verde/violeta/naranja/rojo)
más un **gráfico de las 18 bandas** (UV cian, VIS verde, NIR rojo) y las métricas
NDVI / R-G / pigmento. El color de acento de la interfaz es **`#ffc95c`**
(selección de menú, alertas y pistas de botones).

### Selección de fruta (y estado)
Antes de medir se elige la **fruta** en un selector tipo rueda (BTN1 = siguiente,
BTN2 = elegir; la última opción es `<< Volver`):
- **Diagnóstico:** Menú → Diagnóstico → *elige fruta* → medir.
- **Entrenamiento:** Menú → Entrenamiento → *elige fruta* → *elige estado*
  (sana/botrytis/antracnosis/podrida) → capturar.

El catálogo de frutas es único y está en `firmware_diagnostico/frutas.h` (mismos
`id` que el desplegable web y que Firestore). **Frutas de exportación colombianas:**

| id | Fruta | id | Fruta |
|----|-------|----|-------|
| `aguacate` | 🥑 Aguacate Hass | `mango` | 🥭 Mango |
| `banano` | 🍌 Banano | `lima_tahiti` | 🍋 Lima Tahití |
| `uchuva` | 🟠 Uchuva | `pina` | 🍍 Piña |
| `gulupa` | 🟣 Gulupa | `papaya` | 🍈 Papaya |
| `maracuya` | 🟡 Maracuyá | `fresa` | 🍓 Fresa |
| `pitahaya` | 🐉 Pitahaya | `arandano` | 🫐 Arándano |
| `granadilla` | 🟠 Granadilla | | |

> **Iconos:** en la web se usa **FontAwesome** (vía CDN) para los botones de
> acción, y **emoji** para las frutas. Motivo: FontAwesome *free* no incluye
> iconos de frutas tropicales (son de la versión Pro), mientras que los emoji
> se ven en todos los navegadores, en los `<option>` y también sin conexión.
> En la pantalla TFT se muestran solo los nombres (la fuente no dibuja emoji).

### Gestor WiFi (menú → WiFi)
No hay que escribir credenciales en el código. Desde el menú **WiFi → Configurar**:
1. El ESP32 crea una red abierta **`GENIC-Setup`**.
2. Conecta tu móvil a esa red y abre **`http://192.168.4.1`**.
3. Pulsa *Escanear*, elige tu red del desplegable, escribe la contraseña y *Conectar*.
4. Las credenciales se guardan en memoria (**NVS**) y el equipo **se reconecta solo**
   en cada arranque.

El logo se genera desde `assets/genicLCD.bmp` con `python/generar_logo.py`
(produce `firmware_diagnostico/logo_genic.h`, un array RGB565 240×135).

### Atajos por Serial (115200 baudios)
`1` diagnóstico · `2` entrenamiento · `w` WiFi · `i` info · `x` menú · `m` medir ·
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

## Firestore — catálogo de frutas (dataset)

El catálogo de frutas vive en **Firestore** (proyecto `genic-76302`). Tienes dos
formas de crearlo vacío; ambas generan la colección **`frutas`** (un documento por
fruta, sin mediciones aún) y **`etiquetas`** (los 4 estados).

**Opción A — Navegador (más fácil, sin clave de servicio):**
1. Abre **`firebase/init_firestore.html`** en tu navegador (doble clic).
2. Pulsa **Probar conexión** (verifica que Firestore responde) y luego **Crear catálogo**.
> Usa la **API REST** de Firestore con `fetch` (funciona abriendo el archivo como
> `file://`, sin instalar nada ni cargar el SDK). Requiere que las **reglas de
> Firestore** permitan escritura. Cada escritura muestra su estado; si sale `403`
> revisa las reglas/clave, si es error de red revisa tu internet.

**Opción B — Python (clave de servicio):**
```bash
pip install firebase-admin
# Firebase Console > Config del proyecto > Cuentas de servicio
#   > "Generar nueva clave privada"  -> guardar como serviceAccount.json
python firebase/init_firestore.py serviceAccount.json
```
Si falta la clave, el script ahora te indica exactamente qué hacer (ya no lanza un
traceback). Estructura en `firebase/firestore_estructura.json`:

```
frutas/{id}                nombre, emoji, exportacion, activa, orden, n_mediciones, creado
frutas/{id}/mediciones/{autoId}   estado, diagnostico, t, bandas{410..940}
etiquetas/{id}             nombre, color, orden
```

> Firestore no guarda colecciones vacías: la subcolección `mediciones` aparece
> cuando se añade la primera. La app móvil lee `frutas` para poblar su lista.

### Importar tus CSV previos (dataset de `colector_espectral.py`)
Sube los CSV que ya tenías al dataset. Detecta solo las 18 bandas (410–940),
`estado`, `id_muestra` y `timestamp`, y escribe en `frutas/{fruta}/mediciones/{id_muestra}`.

**Opción A — Navegador (sin instalar):** abre **`firebase/importar_csv.html`**,
elige la **fruta** del CSV (p. ej. `fresa`), selecciona el/los archivo(s) y pulsa
**Importar**. Cada fila se sube como una medición (idempotente: reimportar el
mismo `id_muestra` la actualiza, no la duplica).

**Opción B — Python (bulk):**
```bash
pip install firebase-admin
python firebase/importar_csv.py dataset_fresas.csv --fruta fresa --service serviceAccount.json
# varios archivos con la fruta en una columna del CSV:
python firebase/importar_csv.py *.csv
```
La versión Python además **incrementa `n_mediciones`** en cada documento de fruta.

> El CSV del colector no trae columna de fruta (era de fresas), por eso eliges la
> fruta al importar. Si tu CSV usa etiquetas distintas (ej. `sospechosa`), se
> guardan tal cual en `estado`; luego puedes normalizarlas.

### Cómo llegan las mediciones a Firestore
Hoy el ESP32 publica en **Realtime Database** (`/mediciones`), que es el canal en
tiempo real. Para volcar cada medición al dataset de Firestore, lo recomendado es
una **Cloud Function** que, al crearse `/mediciones/{id}` en RTDB, escriba en
`frutas/{fruta}/mediciones` e incremente `n_mediciones`. Dímelo y añado la función.

## Pendiente / próximos pasos
- **Cloud Function RTDB → Firestore** para poblar `frutas/{id}/mediciones` (arriba).
- **Modelos ML en Firestore** y sincronización con `clasificador.h`.
