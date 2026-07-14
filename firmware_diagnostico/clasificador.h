// ============================================================================
//  clasificador.h  -  Modelo ML embebido (arbol de decision / umbrales)
// ----------------------------------------------------------------------------
//  El AS7265x entrega 18 bandas. Aqui las usamos SIEMPRE ordenadas por
//  longitud de onda (nm), que es el mismo orden del CSV que genera
//  colector_espectral.py:
//
//   idx : nm  : canal SparkFun
//    0  : 410 : A        6  : 560 : G        12 : 730 : T
//    1  : 435 : B        7  : 585 : H        13 : 760 : U
//    2  : 460 : C        8  : 610 : R        14 : 810 : V
//    3  : 485 : D        9  : 645 : I        15 : 860 : W
//    4  : 510 : E       10  : 680 : S        16 : 900 : K
//    5  : 535 : F       11  : 705 : J        17 : 940 : L
//
//  Grupos:  UV = idx 0..5 (410-535) | VIS = idx 6..11 (560-705) | NIR = 12..17
// ============================================================================
#ifndef CLASIFICADOR_H
#define CLASIFICADOR_H

#include <Arduino.h>

// Indices de banda por longitud de onda (para leer el arbol facilmente)
enum Banda {
  B410=0, B435, B460, B485, B510, B535,
  B560, B585, B610, B645, B680, B705,
  B730, B760, B810, B860, B900, B940
};

// Longitudes de onda en nm, en orden
static const uint16_t WAVELENGTHS[18] = {
  410,435,460,485,510,535,560,585,610,645,680,705,730,760,810,860,900,940
};

// Clases de salida
enum Diagnostico { SANA=0, BOTRYTIS=1, ANTRACNOSIS=2, PODRIDA=3, DESCONOCIDO=4 };

static const char* DIAG_NOMBRE[5] = {
  "SANA", "BOTRYTIS", "ANTRACNOSIS", "PODRIDA", "DESCONOCIDO"
};

// ---------------------------------------------------------------------------
//  Features derivadas (indices espectrales) — mas robustas que la reflectancia
//  cruda porque normalizan la intensidad total de iluminacion.
// ---------------------------------------------------------------------------
struct Features {
  float ndvi;      // (NIR810 - RED680)/(NIR810 + RED680)  -> vigor/clorofila
  float ratioRG;   // RED680 / GREEN560                    -> pardeamiento
  float pigmento;  // (410 + 435) / (560 + 585)            -> antocianinas/UV
  float nirRatio;  // 940 / 730                            -> contenido de agua
  float grisTotal; // suma de las 18 bandas                -> intensidad global
};

inline Features calcularFeatures(const float* b) {
  Features f;
  float nir = b[B810], red = b[B680], green = b[B560];
  f.ndvi     = (nir + red)   > 1e-6f ? (nir - red) / (nir + red)   : 0.0f;
  f.ratioRG  = green         > 1e-6f ?  red / green                : 0.0f;
  f.pigmento = (b[B560]+b[B585]) > 1e-6f
               ? (b[B410]+b[B435]) / (b[B560]+b[B585])             : 0.0f;
  f.nirRatio = b[B730]       > 1e-6f ?  b[B940] / b[B730]          : 0.0f;
  float s = 0; for (int i=0;i<18;i++) s += b[i];
  f.grisTotal = s;
  return f;
}

// ---------------------------------------------------------------------------
//  MODELO 1: ARBOL DE DECISION (plantilla)
// ----------------------------------------------------------------------------
//  ESTO ES UNA PLANTILLA con umbrales de ejemplo basados en el comportamiento
//  espectral tipico de estas patologias. DEBES reemplazar los umbrales/reglas
//  por el arbol que exportes desde sklearn con tu dataset real
//  (ver export_arbol.py en el README).
//
//  Racional espectral (guia, no absoluto):
//   - SANA:        NDVI alto, pigmento fuerte (fruta intensa), reflectancia
//                  NIR sana alta.
//   - BOTRYTIS:    moho gris -> sube reflectancia difusa en VIS medio,
//                  cae NDVI moderadamente, ratioRG se mantiene o baja.
//   - ANTRACNOSIS: lesiones oscuras hundidas -> ratioRG sube (pardeamiento),
//                  cae reflectancia global y NDVI.
//   - PODRIDA:     colapso de tejido -> reflectancia global muy baja, NDVI
//                  bajo, nirRatio alterado por perdida de agua.
// ---------------------------------------------------------------------------
inline Diagnostico clasificarArbol(const float* b) {
  Features f = calcularFeatures(b);

  // Nodo raiz: intensidad global (tejido colapsado refleja muy poco)
  if (f.grisTotal < 8000.0f) {
    // Muy poca reflectancia -> tejido muy dañado
    if (f.ndvi < 0.10f) return PODRIDA;
    return ANTRACNOSIS;
  }

  // Rama de tejido con estructura -> discriminar por NDVI y pardeamiento
  if (f.ndvi >= 0.55f) {
    // Buen vigor. Confirmar con pigmento (fruta sana suele tener color intenso)
    if (f.pigmento >= 0.35f) return SANA;
    // Vigor alto pero pigmento bajo: sospecha de micelio superficial (Botrytis)
    return BOTRYTIS;
  }

  // NDVI intermedio/bajo con tejido presente
  if (f.ratioRG >= 1.25f) {
    // Fuerte pardeamiento rojizo/marron -> lesiones de antracnosis
    return ANTRACNOSIS;
  }

  if (f.ndvi < 0.25f) {
    // Poco vigor y poco pardeamiento -> avance hacia podredumbre
    return PODRIDA;
  }

  // Reflectancia difusa tipica del moho gris
  return BOTRYTIS;
}

// ---------------------------------------------------------------------------
//  MODELO 2: TABLA DE UMBRALES (alternativa simple y editable a mano)
// ----------------------------------------------------------------------------
//  Se elige la clase cuya "firma" de features este mas cerca (distancia L1
//  normalizada). Util para calibrar rapido con pocos datos. Rellena estos
//  centroides con la media de tus muestras por clase.
// ---------------------------------------------------------------------------
struct CentroideClase {
  Diagnostico clase;
  float ndvi, ratioRG, pigmento, nirRatio; // firma promedio de la clase
};

// Centroides de EJEMPLO — reemplaza con la media real de tu CSV por clase.
static const CentroideClase CENTROIDES[] = {
  { SANA,        0.72f, 0.85f, 0.55f, 1.05f },
  { BOTRYTIS,    0.45f, 0.95f, 0.30f, 1.10f },
  { ANTRACNOSIS, 0.30f, 1.45f, 0.40f, 1.00f },
  { PODRIDA,     0.12f, 1.10f, 0.25f, 0.85f },
};
static const int NUM_CENTROIDES = sizeof(CENTROIDES)/sizeof(CENTROIDES[0]);

inline Diagnostico clasificarUmbrales(const float* b) {
  Features f = calcularFeatures(b);
  float mejorDist = 1e9f;
  Diagnostico mejor = DESCONOCIDO;
  for (int i = 0; i < NUM_CENTROIDES; i++) {
    const CentroideClase& c = CENTROIDES[i];
    // distancia L1 con pesos (todas las features en rangos comparables)
    float d = fabsf(f.ndvi     - c.ndvi)     * 1.5f
            + fabsf(f.ratioRG  - c.ratioRG)  * 1.0f
            + fabsf(f.pigmento - c.pigmento) * 1.0f
            + fabsf(f.nirRatio - c.nirRatio) * 0.8f;
    if (d < mejorDist) { mejorDist = d; mejor = c.clase; }
  }
  return mejor;
}

// ---------------------------------------------------------------------------
//  Punto de entrada unico del clasificador.
//  Cambia USAR_ARBOL para elegir el metodo activo.
// ---------------------------------------------------------------------------
#define USAR_ARBOL 1   // 1 = arbol de decision | 0 = tabla de umbrales

inline Diagnostico clasificar(const float* bandas) {
#if USAR_ARBOL
  return clasificarArbol(bandas);
#else
  return clasificarUmbrales(bandas);
#endif
}

#endif // CLASIFICADOR_H
