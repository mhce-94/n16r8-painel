"""Atlas 4 bits da Segoe UI para a tela 240x320."""

from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

FONTE = Path(r"C:\Windows\Fonts")
SAIDA = Path(__file__).resolve().parents[1] / "src" / "fonte.h"
CHARS = (
    " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
    "0123456789.,%+-/:°º'()&"
    "áàâãéêíóôõúçÁÀÂÃÉÊÍÓÔÕÚÇ"
)
TAMANHOS = [
    ("segoeui.ttf", 14),
    ("segoeui.ttf", 18),
    ("segoeuib.ttf", 22),
    ("segoeuib.ttf", 40),
    ("segoeuib.ttf", 56),
]


def glifo(fonte, char):
    if char == " ":
        return int(round(fonte.getlength(" "))), 0, 0, 0, 0, b""
    caixa = fonte.getbbox(char)
    if caixa is None:
        return int(round(fonte.getlength(char) or 4)), 0, 0, 0, 0, b""
    ascent = fonte.getmetrics()[0]
    esq, topo, dir_, baixo = caixa
    largura = max(1, dir_ - esq)
    altura = max(1, baixo - topo)
    imagem = Image.new("L", (largura, altura), 0)
    ImageDraw.Draw(imagem).text((-esq, -topo), char, font=fonte, fill=255)
    pixels = np.asarray(imagem, dtype=np.uint8)
    nibbles = [(int(v) * 15 + 127) // 255 for v in pixels.reshape(-1)]
    if len(nibbles) % 2:
        nibbles.append(0)
    bruto = bytes(((nibbles[i] << 4) | nibbles[i + 1]) for i in range(0, len(nibbles), 2))
    avanco = max(largura, int(round(fonte.getlength(char))))
    return avanco, largura, altura, int(esq), int(topo), bruto


def main():
    bits = bytearray()
    metas = []
    linhas = []
    ascensos = []
    for arquivo, tamanho in TAMANHOS:
        fonte = ImageFont.truetype(str(FONTE / arquivo), tamanho)
        ascenso, descida = fonte.getmetrics()
        ascensos.append(ascenso)
        linhas.append(ascenso + descida)
        linha = []
        for char in CHARS:
            avanco, largura, altura, dx, dy, bruto = glifo(fonte, char)
            linha.append((len(bits), largura, altura, avanco, dx, dy))
            bits += bruto
        metas.append(linha)

    amostra = Image.new("RGB", (240, 120), (8, 12, 18))
    desenho = ImageDraw.Draw(amostra)
    y = 4
    for arquivo, tamanho in (("segoeuib.ttf", 56), ("segoeui.ttf", 18)):
        fonte = ImageFont.truetype(str(FONTE / arquivo), tamanho)
        desenho.text((8, y), "18:42  23°", font=fonte, fill=(244, 247, 251))
        y += tamanho + 6
    amostra.save(Path(__file__).resolve().parent / "preview_fonte.png")

    with SAIDA.open("w", encoding="utf-8", newline="\n") as arquivo:
        arquivo.write("#pragma once\n#include <stdint.h>\n\n")
        arquivo.write(f"static const int FONTE_N = {len(CHARS)};\n")
        arquivo.write("static const uint16_t FONTE_CP[FONTE_N] = {\n  ")
        arquivo.write(",".join(str(ord(c)) for c in CHARS))
        arquivo.write("\n};\n")
        arquivo.write("static const uint8_t FONTE_LINHA[5] = {" + ",".join(str(v) for v in linhas) + "};\n")
        arquivo.write("static const uint8_t FONTE_SOBE[5] = {" + ",".join(str(v) for v in ascensos) + "};\n")
        arquivo.write("struct FonteGlifo { uint32_t off; uint8_t w, h, adv; int8_t dx, dy; };\n")
        arquivo.write("static const FonteGlifo FONTE_GLIFO[5][FONTE_N] = {\n")
        for linha in metas:
            arquivo.write("  {\n")
            for off, largura, altura, avanco, dx, dy in linha:
                arquivo.write(f"    {{{off},{largura},{altura},{avanco},{dx},{dy}}},\n")
            arquivo.write("  },\n")
        arquivo.write("};\n")
        arquivo.write(f"static const uint8_t FONTE_BITS[{len(bits)}] = {{\n")
        for inicio in range(0, len(bits), 20):
            fatia = ",".join(f"0x{b:02X}" for b in bits[inicio : inicio + 20])
            arquivo.write(f"  {fatia},\n")
        arquivo.write("};\n")
    print(f"{SAIDA} {len(bits)} bytes")


if __name__ == "__main__":
    main()
