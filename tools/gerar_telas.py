"""Gera duas telas 240x320 em RGB565, no limite do ILI9341."""

from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

W = 240
H = 320
ESCALA = 4
FONTE = Path(r"C:\Windows\Fonts")
SAIDA = Path(__file__).resolve().parents[1] / "src" / "telas.h"
PREVIEW = Path(__file__).resolve().parent


def esfera(img, cx, cy, raio, cor, luz):
    altura, largura = img.shape[:2]
    y, x = np.mgrid[0:altura, 0:largura]
    dx = (x - cx) / raio
    dy = (y - cy) / raio
    dist = dx * dx + dy * dy
    mascara = dist <= 1.0
    nz = np.zeros_like(dx)
    nz[mascara] = np.sqrt(np.clip(1.0 - dist[mascara], 0.0, 1.0))
    lx, ly, lz = luz
    difuso = np.clip(dx * lx + dy * ly + nz * lz, 0.0, 1.0)
    hx, hy, hz = lx, ly, lz + 1.0
    norma = np.sqrt(hx * hx + hy * hy + hz * hz)
    hx, hy, hz = hx / norma, hy / norma, hz / norma
    especular = np.clip(dx * hx + dy * hy + nz * hz, 0.0, 1.0) ** 70
    ambiente = 0.28
    frio = np.array([0.72, 0.80, 0.95], dtype=np.float32)
    albedo = np.array(cor, dtype=np.float32)
    corpo = albedo * (ambiente + 0.78 * difuso)[..., None]
    corpo += albedo * frio * (1.0 - difuso)[..., None] * 0.22
    corpo += especular[..., None] * 0.85
    img[mascara] = np.clip(corpo[mascara], 0.0, 1.0)


def sombra(img, cx, cy, rx, ry, forca):
    altura, largura = img.shape[:2]
    y, x = np.mgrid[0:altura, 0:largura]
    dist = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
    queda = np.clip(1.0 - dist, 0.0, 1.0) ** 1.6
    img *= (1.0 - queda * forca)[..., None]


def imagem_rgb():
    s = ESCALA
    altura, largura = H * s, W * s
    y = np.linspace(0.0, 1.0, altura)[:, None]
    x = np.linspace(0.0, 1.0, largura)[None, :]
    topo = np.array([236, 241, 246], dtype=np.float32) / 255.0
    base = np.array([186, 198, 210], dtype=np.float32) / 255.0
    vert = (topo * (1.0 - y[..., None]) + base * y[..., None]).astype(np.float32)
    img = np.broadcast_to(vert, (altura, largura, 3)).copy()
    centro = np.exp(-(((x - 0.42) ** 2) / 0.18 + ((y - 0.28) ** 2) / 0.22))
    img += centro[..., None] * np.array([0.10, 0.09, 0.07], dtype=np.float32)
    img = np.clip(img, 0.0, 1.0)

    sombra(img, 120 * s, 214 * s, 78 * s, 16 * s, 0.38)
    sombra(img, 178 * s, 214 * s, 40 * s, 10 * s, 0.28)
    sombra(img, 62 * s, 214 * s, 36 * s, 9 * s, 0.26)

    luz = (-0.46, -0.62, 0.63)
    luz = luz / np.linalg.norm(luz)
    esfera(img, 118 * s, 132 * s, 68 * s, (0.97, 0.97, 0.98), luz)
    esfera(img, 184 * s, 162 * s, 34 * s, (0.90, 0.24, 0.18), luz)
    esfera(img, 58 * s, 170 * s, 30 * s, (0.12, 0.38, 0.92), luz)

    foto = Image.fromarray(np.rint(np.clip(img, 0.0, 1.0) * 255).astype(np.uint8), "RGB")
    foto = foto.resize((W, H), Image.Resampling.LANCZOS)
    desenho = ImageDraw.Draw(foto)
    desenho.line((16, 292, 224, 292), fill=(24, 24, 24), width=1)
    for i, marca in enumerate(range(16, 225, 26)):
        alto = 10 if i % 2 == 0 else 6
        desenho.line((marca, 292, marca, 292 - alto), fill=(24, 24, 24), width=1)
    return np.asarray(foto, dtype=np.float32)


def quebra(desenho, texto, fonte, largura):
    linhas = []
    atual = ""
    for palavra in texto.split():
        tentativa = palavra if not atual else atual + " " + palavra
        if desenho.textlength(tentativa, font=fonte) <= largura:
            atual = tentativa
        else:
            if atual:
                linhas.append(atual)
            atual = palavra
    if atual:
        linhas.append(atual)
    return linhas


def escreve(desenho, texto, fonte, x, y, largura, fill, espaco=3):
    for linha in quebra(desenho, texto, fonte, largura):
        desenho.text((x, y), linha, font=fonte, fill=fill)
        caixa = desenho.textbbox((x, y), linha, font=fonte)
        y = caixa[3] + espaco
    return y


def texto_rgb():
    tela = Image.new("RGB", (W, H), (255, 255, 255))
    desenho = ImageDraw.Draw(tela)
    fina = ImageFont.truetype(str(FONTE / "segoeuil.ttf"), 62)
    forte = ImageFont.truetype(str(FONTE / "segoeuib.ttf"), 18)
    tinta = (16, 16, 16)
    cinza = (90, 90, 90)

    desenho.text((16, 8), "Segoe UI", font=ImageFont.truetype(str(FONTE / "segoeui.ttf"), 11), fill=cinza)
    desenho.text((14, 24), "Aa", font=fina, fill=tinta)
    desenho.line((16, 100, 224, 100), fill=(214, 214, 214), width=1)
    y = escreve(desenho, "Hambúrguer", forte, 16, 106, 208, tinta, 1)
    y += 6
    for tamanho in (16, 13, 11, 9):
        fonte = ImageFont.truetype(str(FONTE / "segoeui.ttf"), tamanho)
        linha = f"{tamanho}  A raposa ágil"
        desenho.text((16, y), linha, font=fonte, fill=tinta)
        y = desenho.textbbox((16, y), linha, font=fonte)[3] + 4
    y += 4
    miuda = ImageFont.truetype(str(FONTE / "segoeui.ttf"), 12)
    y = escreve(desenho, "Ácido, gráfico, ação, mão e ç.", miuda, 16, y, 208, tinta, 2)
    escreve(desenho, "ABC abc 123    g j y á ã õ", miuda, 16, y + 2, 208, tinta, 2)
    desenho.rectangle((0, 286, W, H), fill=(12, 12, 12))
    claro = ImageFont.truetype(str(FONTE / "segoeui.ttf"), 15)
    desenho.text((16, 296), "luz sobre o escuro", font=claro, fill=(255, 255, 255))
    return np.asarray(tela, dtype=np.float32)


def rgb565(imagem):
    pixels = imagem.astype(np.float64).copy()
    altura, largura = pixels.shape[:2]
    saida = np.zeros((altura, largura), dtype=np.uint16)
    for y in range(altura):
        esquerda = y % 2 == 0
        xs = range(largura) if esquerda else range(largura - 1, -1, -1)
        passo = 1 if esquerda else -1
        for x in xs:
            antigo = pixels[y, x]
            r = int(np.clip(np.rint(antigo[0] / 255.0 * 31.0), 0, 31))
            g = int(np.clip(np.rint(antigo[1] / 255.0 * 63.0), 0, 63))
            b = int(np.clip(np.rint(antigo[2] / 255.0 * 31.0), 0, 31))
            novo = np.array([r * 255.0 / 31.0, g * 255.0 / 63.0, b * 255.0 / 31.0])
            saida[y, x] = (r << 11) | (g << 5) | b
            erro = antigo - novo
            xn = x + passo
            if 0 <= xn < largura:
                pixels[y, xn] += erro * (7.0 / 16.0)
            if y + 1 < altura:
                pixels[y + 1, x] += erro * (5.0 / 16.0)
                xa = x - passo
                if 0 <= xa < largura:
                    pixels[y + 1, xa] += erro * (3.0 / 16.0)
                if 0 <= xn < largura:
                    pixels[y + 1, xn] += erro * (1.0 / 16.0)
    return saida


def gravar(arquivo, nome, pixels):
    arquivo.write(f"static const uint16_t {nome}[{W * H}] = {{\n")
    plano = pixels.reshape(-1)
    for inicio in range(0, plano.size, 12):
        fatia = ",".join(f"0x{valor:04X}" for valor in plano[inicio : inicio + 12])
        arquivo.write(f"  {fatia},\n")
    arquivo.write("};\n\n")


def previa(pixels, caminho):
    r = ((pixels >> 11) & 31).astype(np.uint16) * 255 // 31
    g = ((pixels >> 5) & 63).astype(np.uint16) * 255 // 63
    b = (pixels & 31).astype(np.uint16) * 255 // 31
    rgb = np.dstack([r, g, b]).astype(np.uint8)
    Image.fromarray(rgb, "RGB").save(caminho)


def main():
    print("imagem")
    foto = rgb565(imagem_rgb())
    print("texto")
    letras = rgb565(texto_rgb())
    SAIDA.parent.mkdir(parents=True, exist_ok=True)
    with SAIDA.open("w", encoding="utf-8", newline="\n") as arquivo:
        arquivo.write("#pragma once\n#include <stdint.h>\n\n")
        gravar(arquivo, "telaImagem", foto)
        gravar(arquivo, "telaTexto", letras)
    previa(foto, PREVIEW / "preview_imagem.png")
    previa(letras, PREVIEW / "preview_texto.png")
    print(SAIDA)


if __name__ == "__main__":
    main()
