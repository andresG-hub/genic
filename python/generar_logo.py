#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
generar_logo.py
------------------------------------------------------------------------------
Convierte genicLCD.bmp (24-bit) en un array RGB565 en PROGMEM ya escalado al
tamano de la pantalla de la T-Display (240x135), listo para dibujar con
Adafruit_GFX: tft.drawRGBBitmap(0, 0, LOGO_GENIC, LOGO_W, LOGO_H);

Uso:
    python generar_logo.py ../assets/genicLCD.bmp \
                           ../firmware_diagnostico/logo_genic.h
Sin dependencias externas (BMP parseado a mano + downscale por promedio).
"""
import sys

TW, TH = 240, 135  # tamano destino (pantalla apaisada)


def leer_bmp(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:2] != b"BM":
        sys.exit("No es un BMP valido")
    offset = int.from_bytes(data[10:14], "little")
    w      = int.from_bytes(data[18:22], "little")
    h      = int.from_bytes(data[22:26], "little")
    bpp    = int.from_bytes(data[28:30], "little")
    comp   = int.from_bytes(data[30:34], "little")
    if bpp != 24 or comp != 0:
        sys.exit(f"Solo soporto BMP 24-bit sin comprimir (bpp={bpp}, comp={comp})")
    row_padded = (w * 3 + 3) & ~3
    return data, offset, w, h, row_padded


def escalar(path):
    data, offset, w, h, row_padded = leer_bmp(path)

    def px(x, y):                      # y con origen ARRIBA (BMP es bottom-up)
        row = h - 1 - y
        base = offset + row * row_padded + x * 3
        return data[base + 2], data[base + 1], data[base]  # r, g, b

    salida = []
    for ty in range(TH):
        y0 = ty * h // TH
        y1 = max(y0 + 1, (ty + 1) * h // TH)
        for tx in range(TW):
            x0 = tx * w // TW
            x1 = max(x0 + 1, (tx + 1) * w // TW)
            rs = gs = bs = n = 0
            for yy in range(y0, y1):
                for xx in range(x0, x1):
                    r, g, b = px(xx, yy)
                    rs += r; gs += g; bs += b; n += 1
            r, g, b = rs // n, gs // n, bs // n
            salida.append(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
    return salida


def escribir_header(vals, out_path):
    lineas = []
    lineas.append("// Autogenerado por python/generar_logo.py  -  NO editar a mano")
    lineas.append("// Logo GENIC escalado a %dx%d (RGB565) para Adafruit_GFX." % (TW, TH))
    lineas.append("#ifndef LOGO_GENIC_H")
    lineas.append("#define LOGO_GENIC_H")
    lineas.append("#include <Arduino.h>")
    lineas.append("#define LOGO_W %d" % TW)
    lineas.append("#define LOGO_H %d" % TH)
    lineas.append("static const uint16_t LOGO_GENIC[%d] PROGMEM = {" % (TW * TH))
    for i in range(0, len(vals), 12):
        chunk = ", ".join("0x%04X" % v for v in vals[i:i + 12])
        lineas.append("  " + chunk + ",")
    lineas.append("};")
    lineas.append("#endif // LOGO_GENIC_H")
    with open(out_path, "w") as f:
        f.write("\n".join(lineas) + "\n")


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "../assets/genicLCD.bmp"
    dst = sys.argv[2] if len(sys.argv) > 2 else "../firmware_diagnostico/logo_genic.h"
    vals = escalar(src)
    escribir_header(vals, dst)
    print("[OK] %d pixeles -> %s (%d KB aprox en flash)" %
          (len(vals), dst, len(vals) * 2 // 1024))


if __name__ == "__main__":
    main()
