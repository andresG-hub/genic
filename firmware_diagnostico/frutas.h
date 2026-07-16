// ============================================================================
//  frutas.h  -  Catalogo de frutas y estados (fuente unica de la verdad)
// ----------------------------------------------------------------------------
//  Los 'id' deben coincidir EXACTAMENTE con:
//    - el desplegable de la web (pagina_web.h)
//    - la coleccion "frutas" de Firestore (firebase/init_firestore.py)
//    - la app movil
//  Los 'nombre' van en ASCII (sin acentos) porque la fuente del TFT no dibuja
//  caracteres acentuados. En la web/app se usan los nombres bonitos con tildes.
//
//  Prioridad: frutas de EXPORTACION colombianas.
// ============================================================================
#ifndef FRUTAS_H
#define FRUTAS_H

struct OpcionSel {
  const char* id;
  const char* nombre;   // para el TFT (ASCII)
};

// 13 frutas (>=10). Las primeras son las principales de exportacion de Colombia.
static const OpcionSel FRUTAS[] = {
  { "aguacate",    "Aguacate Hass" },
  { "banano",      "Banano" },
  { "uchuva",      "Uchuva" },
  { "gulupa",      "Gulupa" },
  { "maracuya",    "Maracuya" },
  { "pitahaya",    "Pitahaya" },
  { "granadilla",  "Granadilla" },
  { "mango",       "Mango" },
  { "lima_tahiti", "Lima Tahiti" },
  { "pina",        "Pina" },
  { "papaya",      "Papaya" },
  { "fresa",       "Fresa" },
  { "arandano",    "Arandano" },
};
static const int NUM_FRUTAS = sizeof(FRUTAS) / sizeof(FRUTAS[0]);

// Estados / etiquetas de entrenamiento (clases del modelo)
static const OpcionSel ESTADOS_OPC[] = {
  { "sana",        "Sana" },
  { "botrytis",    "Botrytis" },
  { "antracnosis", "Antracnosis" },
  { "podrida",     "Podrida" },
};
static const int NUM_ESTADOS = sizeof(ESTADOS_OPC) / sizeof(ESTADOS_OPC[0]);

#endif // FRUTAS_H
