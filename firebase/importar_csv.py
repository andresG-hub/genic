#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
importar_csv.py
------------------------------------------------------------------------------
Sube los CSV de colector_espectral.py al dataset de Firestore, en
    frutas/{fruta}/mediciones/{id_muestra}
Detecta automaticamente las 18 columnas de banda (410..940), 'estado',
'id_muestra' y 'timestamp'. Usa escritura por lotes (batch) e incrementa
n_mediciones en el documento de la fruta.

Requisitos:
    pip install firebase-admin
    Clave de servicio (serviceAccount.json) desde:
      Firebase Console > Config del proyecto > Cuentas de servicio

Uso:
    # una fruta para todo el CSV:
    python importar_csv.py dataset_fresas.csv --fruta fresa

    # varios CSV, tomando la fruta de una columna 'fruta' del propio CSV:
    python importar_csv.py *.csv

    # clave y base de datos personalizadas:
    python importar_csv.py d.csv --fruta mango --service clave.json --database "(default)"
"""
import argparse
import csv
import glob
import os
import re
import sys

import firebase_admin
from firebase_admin import credentials, firestore

NM = [410, 435, 460, 485, 510, 535, 560, 585, 610,
      645, 680, 705, 730, 760, 810, 860, 900, 940]


def mapear_columnas(fieldnames):
    band, idc, estadoc, tc, frutac = {}, None, None, None, None
    for h in fieldnames:
        hl = h.lower()
        for nm in NM:
            if re.search(r'(^|[^0-9])' + str(nm) + r'([^0-9]|$)', h):
                band[nm] = h
        if idc is None and 'id' in hl:
            idc = h
        if estadoc is None and re.search(r'estado|label|clase', hl):
            estadoc = h
        if tc is None and re.search(r'time|fecha|timestamp', hl):
            tc = h
        if frutac is None and re.search(r'fruta|fruit', hl):
            frutac = h
    return band, idc, estadoc, tc, frutac


def san_id(s):
    s = re.sub(r'[\/#?\s]+', '_', str(s)).strip('_')[:150]
    return s or None


def importar(db, path, fruta_fija):
    with open(path, newline='', encoding='utf-8-sig') as f:
        reader = csv.DictReader(f)
        band, idc, estadoc, tc, frutac = mapear_columnas(reader.fieldnames)
        if not band:
            print(f"  [!] {path}: no se detectaron columnas de banda. Encabezados: {reader.fieldnames}")
            return 0
        print(f"  {os.path.basename(path)}: {len(band)} bandas detectadas")

        batch = db.batch()
        conteo_por_fruta = {}
        pend = total = 0
        for i, row in enumerate(reader, 1):
            fruta = fruta_fija or (row.get(frutac, "").strip().lower() if frutac else "")
            if not fruta:
                print(f"  [!] fila {i}: sin fruta (usa --fruta o agrega columna 'fruta')")
                continue
            bandas = {}
            for nm, col in band.items():
                try:
                    bandas[str(nm)] = float(row[col])
                except (ValueError, TypeError):
                    pass
            idm = san_id(row[idc]) if idc else f"csv_{i}"
            doc = {
                "estado": (row.get(estadoc, "").strip() if estadoc else ""),
                "id_muestra": (row.get(idc, "").strip() if idc else idm),
                "timestamp": (row.get(tc, "").strip() if tc else ""),
                "fuente": "csv",
                "importado": firestore.SERVER_TIMESTAMP,
                "bandas": bandas,
            }
            ref = db.collection("frutas").document(fruta).collection("mediciones").document(idm)
            batch.set(ref, doc)
            conteo_por_fruta[fruta] = conteo_por_fruta.get(fruta, 0) + 1
            pend += 1; total += 1
            if pend >= 400:          # limite seguro por lote (max 500)
                batch.commit(); batch = db.batch(); pend = 0
        if pend:
            batch.commit()

        # actualiza n_mediciones por fruta
        for fruta, n in conteo_por_fruta.items():
            db.collection("frutas").document(fruta).set(
                {"n_mediciones": firestore.Increment(n)}, merge=True)
        print(f"  -> {total} mediciones subidas ({conteo_por_fruta})")
        return total


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", nargs="+", help="archivo(s) CSV (admite comodines)")
    ap.add_argument("--fruta", help="id de fruta para TODO el CSV (ej: fresa, mango)")
    ap.add_argument("--service", default="serviceAccount.json", help="clave de servicio")
    ap.add_argument("--database", default=None, help="id de base (por defecto (default))")
    args = ap.parse_args()

    if not os.path.exists(args.service):
        sys.exit(f"[ERROR] No encuentro la clave de servicio '{args.service}'. "
                 f"Descargala de Firebase Console > Cuentas de servicio.")

    cred = credentials.Certificate(args.service)
    firebase_admin.initialize_app(cred)
    try:
        db = firestore.client(database_id=args.database) if args.database else firestore.client()
    except TypeError:
        db = firestore.client()   # SDK antiguo: solo base (default)

    archivos = []
    for patron in args.csv:
        archivos.extend(glob.glob(patron))
    if not archivos:
        sys.exit("[ERROR] No se encontraron CSV con ese patron.")

    print(f"Importando {len(archivos)} archivo(s)...")
    total = sum(importar(db, a, args.fruta) for a in archivos)
    print(f"\n[OK] {total} mediciones subidas a Firestore (frutas/*/mediciones).")


if __name__ == "__main__":
    main()
