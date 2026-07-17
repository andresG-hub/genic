#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
init_firestore.py
------------------------------------------------------------------------------
Crea el catalogo de frutas (vacio) en Firestore del proyecto genic-76302.
Deja una coleccion "frutas" con un documento por cada fruta (sin mediciones
todavia) y una coleccion "etiquetas" con los estados/clases del modelo.

Los 'id' coinciden con firmware_diagnostico/frutas.h y con el desplegable web.
Prioridad: frutas de exportacion colombianas.

Requisitos:
    pip install firebase-admin
    Descarga la clave de servicio desde:
      Firebase Console > Configuracion del proyecto > Cuentas de servicio
      > "Generar nueva clave privada"  -> guardala como serviceAccount.json

Uso:
    python init_firestore.py [serviceAccount.json]

Nota: Firestore NO guarda colecciones vacias; la subcoleccion
"frutas/{id}/mediciones" aparecera cuando se agregue la primera medicion.
"""
import os
import sys
import firebase_admin
from firebase_admin import credentials, firestore

# (id, nombre bonito, emoji, es_exportacion)
FRUTAS = [
    ("aguacate",    "Aguacate Hass", "🥑", True),
    ("banano",      "Banano",        "🍌", True),
    ("uchuva",      "Uchuva",        "🟠", True),
    ("gulupa",      "Gulupa",        "🟣", True),
    ("maracuya",    "Maracuyá",      "🟡", True),
    ("pitahaya",    "Pitahaya",      "🐉", True),
    ("granadilla",  "Granadilla",    "🟠", True),
    ("mango",       "Mango",         "🥭", True),
    ("lima_tahiti", "Lima Tahití",   "🍋", True),
    ("pina",        "Piña",          "🍍", True),
    ("papaya",      "Papaya",        "🍈", True),
    ("fresa",       "Fresa",         "🍓", False),
    ("arandano",    "Arándano",      "🫐", False),
]

ESTADOS = [
    ("sana",        "Sana",        "#22c55e"),
    ("botrytis",    "Botrytis",    "#a855f7"),
    ("antracnosis", "Antracnosis", "#f59e0b"),
    ("podrida",     "Podrida",     "#ef4444"),
]


def main():
    sa = sys.argv[1] if len(sys.argv) > 1 else "serviceAccount.json"

    if not os.path.exists(sa):
        print("=" * 68)
        print(f"[ERROR] No encontre la clave de servicio: '{sa}'")
        print("=" * 68)
        print("Este script necesita la clave privada de Firebase. Pasos:")
        print("  1) Firebase Console (proyecto genic-76302)")
        print("     https://console.firebase.google.com/project/genic-76302/settings/serviceaccounts/adminsdk")
        print("  2) Pulsa 'Generar nueva clave privada' -> descarga un .json")
        print("  3) Guardalo en esta carpeta como 'serviceAccount.json'")
        print("     (o pasa la ruta:  python init_firestore.py C:\\ruta\\a\\clave.json )")
        print()
        print("ALTERNATIVA SIN CLAVE: abre 'init_firestore.html' en el navegador")
        print("(usa el firebaseConfig web y no requiere descargar nada).")
        sys.exit(1)

    cred = credentials.Certificate(sa)
    firebase_admin.initialize_app(cred)
    db = firestore.client()

    for i, (fid, nombre, emoji, exp) in enumerate(FRUTAS):
        db.collection("frutas").document(fid).set({
            "nombre": nombre,
            "emoji": emoji,
            "exportacion": exp,
            "activa": True,
            "orden": i,
            "n_mediciones": 0,
            "creado": firestore.SERVER_TIMESTAMP,
        }, merge=True)
        print(f"  [frutas] {fid:<12} {emoji} {nombre}")

    for i, (eid, nombre, color) in enumerate(ESTADOS):
        db.collection("etiquetas").document(eid).set({
            "nombre": nombre,
            "color": color,
            "orden": i,
        }, merge=True)
        print(f"  [etiquetas] {eid}")

    print(f"\n[OK] {len(FRUTAS)} frutas y {len(ESTADOS)} etiquetas creadas en Firestore.")
    print("Las mediciones se guardaran en frutas/{id}/mediciones/{autoId}.")


if __name__ == "__main__":
    main()
