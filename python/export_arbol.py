#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
export_arbol.py
------------------------------------------------------------------------------
Entrena un arbol de decision con tu CSV (el que genera colector_espectral.py)
y lo exporta como una funcion C++ if/else lista para pegar en el firmware,
sustituyendo la plantilla clasificarArbol() de clasificador.h.

El arbol trabaja sobre las 18 bandas CRUDAS en el mismo orden que usa el
firmware (indices B410..B940 -> b[0]..b[17]).

Uso:
    pip install pandas scikit-learn
    python export_arbol.py dataset.csv --salida clasificador_generado.h --profundidad 5

Requisitos del CSV (21 columnas):
    id_muestra, estado, timestamp, 410,435,460,...,940   (18 bandas)
El campo 'estado' debe contener: sana | botrytis | antracnosis | podrida
------------------------------------------------------------------------------
"""
import argparse
import sys
import pandas as pd
from sklearn.tree import DecisionTreeClassifier, _tree
from sklearn.model_selection import cross_val_score

# Orden de bandas EXACTO que usa el firmware (clasificador.h)
BANDAS_NM = [410, 435, 460, 485, 510, 535, 560, 585, 610,
             645, 680, 705, 730, 760, 810, 860, 900, 940]

# Mapa etiqueta -> enum Diagnostico del firmware
ENUM_DIAG = {
    "sana": "SANA",
    "botrytis": "BOTRYTIS",
    "antracnosis": "ANTRACNOSIS",
    "podrida": "PODRIDA",
}


def cargar_datos(csv_path):
    df = pd.read_csv(csv_path)
    cols_banda = [c for c in df.columns if str(c) in map(str, BANDAS_NM)]
    if len(cols_banda) != 18:
        # intenta por nombre exacto de nm
        cols_banda = [str(nm) for nm in BANDAS_NM if str(nm) in df.columns]
    if len(cols_banda) != 18:
        sys.exit(f"[ERROR] No encontre las 18 columnas de banda. Halladas: {cols_banda}")

    if "estado" not in df.columns:
        sys.exit("[ERROR] Falta la columna 'estado'.")

    df = df[df["estado"].isin(ENUM_DIAG.keys())].copy()
    X = df[[str(nm) for nm in BANDAS_NM]].astype(float).values
    y = df["estado"].values
    return X, y


def exportar_cpp(clf, clases, salida):
    tree = clf.tree_
    lineas = []
    lineas.append("// ==========================================================")
    lineas.append("// clasificador_generado.h  (autogenerado por export_arbol.py)")
    lineas.append("// Reemplaza el cuerpo de clasificarArbol() en clasificador.h")
    lineas.append("// b[] son las 18 bandas en orden 410..940 (ver enum Banda).")
    lineas.append("// ==========================================================")
    lineas.append("inline Diagnostico clasificarArbol(const float* b) {")

    def nombre_indice(feature_idx):
        # feature_idx corresponde a la columna nm -> indice b[i]
        return f"b[{feature_idx}]  /* {BANDAS_NM[feature_idx]}nm */"

    def recorrer(nodo, sangria):
        pad = "  " * sangria
        if tree.feature[nodo] != _tree.TREE_UNDEFINED:
            umbral = tree.threshold[nodo]
            feat = tree.feature[nodo]
            lineas.append(f"{pad}if ({nombre_indice(feat)} <= {umbral:.4f}f) {{")
            recorrer(tree.children_left[nodo], sangria + 1)
            lineas.append(f"{pad}}} else {{")
            recorrer(tree.children_right[nodo], sangria + 1)
            lineas.append(f"{pad}}}")
        else:
            valores = tree.value[nodo][0]
            clase_idx = valores.argmax()
            etiqueta = clases[clase_idx]
            enum = ENUM_DIAG.get(etiqueta, "DESCONOCIDO")
            lineas.append(f"{pad}return {enum};  // {etiqueta} "
                          f"(n={int(valores.sum())})")

    recorrer(0, 1)
    lineas.append("  return DESCONOCIDO;")
    lineas.append("}")

    with open(salida, "w") as f:
        f.write("\n".join(lineas) + "\n")
    print(f"[OK] C++ escrito en {salida}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", help="CSV del dataset (colector_espectral.py)")
    ap.add_argument("--salida", default="clasificador_generado.h")
    ap.add_argument("--profundidad", type=int, default=5,
                    help="max_depth del arbol (arboles cortos = codigo mas simple)")
    args = ap.parse_args()

    X, y = cargar_datos(args.csv)
    print(f"[INFO] Muestras: {len(y)} | Clases: {sorted(set(y))}")

    clf = DecisionTreeClassifier(max_depth=args.profundidad,
                                 min_samples_leaf=2, random_state=42)
    clf.fit(X, y)

    # Validacion cruzada (si hay suficientes muestras)
    try:
        k = min(5, min(pd.Series(y).value_counts()))
        if k >= 2:
            scores = cross_val_score(clf, X, y, cv=k)
            print(f"[INFO] Accuracy CV({k}) = {scores.mean():.3f} +/- {scores.std():.3f}")
    except Exception as e:
        print(f"[WARN] No se pudo validar: {e}")

    clases = list(clf.classes_)
    exportar_cpp(clf, clases, args.salida)
    print("[SIGUIENTE] Copia el contenido de la funcion en clasificador.h "
          "(reemplaza clasificarArbol) y recompila el firmware.")


if __name__ == "__main__":
    main()
