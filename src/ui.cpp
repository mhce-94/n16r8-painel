#include "painel.h"
#include "fonte.h"

#include <TFT_eSPI.h>
#include <WiFi.h>
#include <math.h>

TFT_eSPI tft;
TFT_eSprite frame(&tft);

static uint16_t bg, ink, muted, gold, up, down, card, cyan;
static uint8_t paginaLista[6] = {0, 0, 0, 0, 0, 0};

struct Area {
  int x, y, w, h, id;
};
static Area areas[12];
static int abaCot = 0;
static int areasN = 0;

uint16_t uiCor(uint8_t r, uint8_t g, uint8_t b) { return frame.color565(r, g, b); }

static int lerUtf8(const char *&p) {
  uint8_t c = (uint8_t)*p++;
  if (c < 0x80) return c;
  if ((c & 0xE0) == 0xC0 && *p) {
    uint8_t c2 = (uint8_t)*p++;
    return ((c & 0x1F) << 6) | (c2 & 0x3F);
  }
  if ((c & 0xF0) == 0xE0 && p[0] && p[1]) {
    uint8_t c2 = (uint8_t)*p++;
    uint8_t c3 = (uint8_t)*p++;
    return ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
  }
  return c;
}

static int acharCp(int cp) {
  for (int i = 0; i < FONTE_N; i++)
    if (FONTE_CP[i] == cp) return i;
  return -1;
}

static void pintar(int px, int py, uint16_t cor, int alfa) {
  if (px < 0 || py < 0 || px >= TELA_W || py >= TELA_H || alfa <= 0) return;
  if (alfa >= 15) {
    frame.drawPixel(px, py, cor);
    return;
  }
  uint16_t dst = frame.readPixel(px, py);
  int dr = (dst >> 11) & 31, dg = (dst >> 5) & 63, db = dst & 31;
  int sr = (cor >> 11) & 31, sg = (cor >> 5) & 63, sb = cor & 31;
  int r = dr + (sr - dr) * alfa / 15;
  int g = dg + (sg - dg) * alfa / 15;
  int b = db + (sb - db) * alfa / 15;
  frame.drawPixel(px, py, (r << 11) | (g << 5) | b);
}

int uiTexto(int x, int y, const char *t, int tam, uint16_t cor) {
  if (!t) return x;
  if (tam < 0) tam = 0;
  if (tam > 4) tam = 4;
  const char *p = t;
  while (*p) {
    int cp = lerUtf8(p);
    int id = acharCp(cp);
    if (id < 0) {
      x += 4;
      continue;
    }
    const FonteGlifo &g = FONTE_GLIFO[tam][id];
    int inicio = x + g.dx;
    for (int row = 0; row < g.h; row++) {
      for (int col = 0; col < g.w; col++) {
        int i = row * g.w + col;
        uint8_t byte = FONTE_BITS[g.off + (i >> 1)];
        int alfa = (i & 1) ? (byte & 0x0F) : (byte >> 4);
        pintar(inicio + col, y + g.dy + row, cor, alfa);
      }
    }
    x += g.adv;
  }
  return x;
}

int uiLargura(const char *t, int tam) {
  if (!t) return 0;
  if (tam < 0) tam = 0;
  if (tam > 4) tam = 4;
  int w = 0;
  const char *p = t;
  while (*p) {
    int cp = lerUtf8(p);
    int id = acharCp(cp);
    w += id < 0 ? 4 : FONTE_GLIFO[tam][id].adv;
  }
  return w;
}

static int centro(const char *t, int tam) { return (TELA_W - uiLargura(t, tam)) / 2; }

void uiIniciar() {
  tft.init();
  tft.setRotation(0);
  tft.setSwapBytes(false);
  frame.setColorDepth(16);
  frame.setAttribute(PSRAM_ENABLE, true);
  void *ok = frame.createSprite(TELA_W, TELA_H);
  Serial.printf("sprite %s psram %u\n", ok ? "ok" : "falhou", (unsigned)ESP.getPsramSize());
  bg = frame.color565(7, 10, 16);
  ink = frame.color565(244, 247, 251);
  muted = frame.color565(150, 164, 180);
  gold = frame.color565(245, 197, 24);
  up = frame.color565(61, 204, 134);
  down = frame.color565(255, 93, 108);
  card = frame.color565(22, 32, 46);
  cyan = frame.color565(121, 212, 255);
}

static void rodape(int tela) {
  uint16_t faixa = tela == TELA_VAL ? uiCor(15, 25, 35) : bg;
  frame.fillRect(0, 296, TELA_W, 24, faixa);
  String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  uiTexto(8, 298, ip.c_str(), 0, muted);
  const char *t = "descanso";
  int tw = uiLargura(t, 0);
  frame.fillRoundRect(TELA_W - 14 - tw, 300, tw + 10, 16, 6, card);
  uiTexto(TELA_W - 9 - tw, 298, t, 0, gold);
}

static void sol(int x, int y, uint16_t cor) {
  frame.fillCircle(x + 26, y + 26, 11, cor);
  for (int i = 0; i < 8; i++) {
    float a = i * 0.785398f;
    frame.drawLine(x + 26 + (int)(cosf(a) * 15), y + 26 + (int)(sinf(a) * 15), x + 26 + (int)(cosf(a) * 23),
                   y + 26 + (int)(sinf(a) * 23), cor);
  }
}

static void nuvem(int x, int y, uint16_t cor) {
  frame.fillCircle(x + 16, y + 30, 11, cor);
  frame.fillCircle(x + 30, y + 24, 15, cor);
  frame.fillCircle(x + 46, y + 30, 11, cor);
  frame.fillRect(x + 14, y + 30, 34, 12, cor);
}

static void icone(int x, int y, bool noite) {
  int c = climaCod;
  uint16_t cinza = noite ? frame.color565(78, 84, 94) : frame.color565(210, 220, 232);
  uint16_t solCor = noite ? frame.color565(110, 88, 36) : gold;
  uint16_t gotaCor = noite ? frame.color565(64, 90, 104) : cyan;
  bool chuva = (c >= 51 && c <= 67) || (c >= 80 && c <= 82) || c >= 95;
  bool nublado = c >= 2;
  if (c <= 1) sol(x, y, solCor);
  if (nublado) nuvem(x, y + (c <= 2 ? 8 : 0), cinza);
  if (c <= 2 && c > 1) sol(x + 18, y - 6, solCor);
  if (chuva) {
    uint16_t gota = (c >= 95 && !noite) ? gold : gotaCor;
    for (int i = 0; i < 3; i++) frame.drawLine(x + 18 + i * 12, y + 46, x + 14 + i * 12, y + 56, gota);
  }
}

static void pctDe(const Cotacao *q, char *saida, int n, uint16_t *cor) {
  if (!q || !q->ok) {
    saida[0] = 0;
    *cor = muted;
    return;
  }
  snprintf(saida, n, "%+.2f%%", q->pct100 / 100.0f);
  for (char *p = saida; *p; p++)
    if (*p == '.') *p = ',';
  *cor = q->pct100 >= 0 ? up : down;
}

static void climaMini(int x, int y, int cod) {
  uint16_t cinza = frame.color565(210, 220, 232);
  bool chuva = (cod >= 51 && cod <= 67) || (cod >= 80 && cod <= 82) || cod >= 95;
  bool nublado = cod >= 2;
  if (cod <= 1) frame.fillCircle(x + 8, y + 8, 5, gold);
  if (nublado) {
    frame.fillCircle(x + 7, y + 12, 5, cinza);
    frame.fillCircle(x + 14, y + 10, 6, cinza);
    frame.fillRect(x + 6, y + 12, 12, 5, cinza);
  }
  if (cod > 1 && cod <= 2) frame.fillCircle(x + 16, y + 5, 4, gold);
  if (chuva) {
    uint16_t gota = cod >= 95 ? gold : cyan;
    frame.drawLine(x + 8, y + 18, x + 6, y + 23, gota);
    frame.drawLine(x + 14, y + 18, x + 12, y + 23, gota);
  }
}

static void telaClima() {
  frame.fillSprite(bg);
  uiTexto(12, 6, aj.cidade, 1, gold);
  char data[20];
  dataTexto(data, sizeof(data));
  uiTexto(TELA_W - 12 - uiLargura(data, 0), 10, data, 0, muted);
  if (!climaOk) {
    uiTexto(centro("buscando o clima", 1), 140, "buscando o clima", 1, muted);
    return;
  }
  icone(4, 28, false);
  char graus[12];
  snprintf(graus, sizeof(graus), "%.0f", tempC);
  int x = uiTexto(78, 36, graus, 3, ink);
  uiTexto(x + 2, 42, "°", 2, gold);
  uiTexto(78, 92, nomeClima(), 1, muted);
  char faixa[24] = "--";
  if (tempMin > -80 && tempMax > -80) snprintf(faixa, sizeof(faixa), "%.0f° a %.0f°", tempMin, tempMax);
  char hum[24] = "";
  if (umidade >= 0) snprintf(hum, sizeof(hum), "umidade %d%%", umidade);
  uiTexto(12, 112, hum[0] ? hum : "umidade --", 0, cyan);
  uiTexto(130, 112, faixa, 0, ink);
  if (solNasce[0] && solPoe[0]) {
    char sol[32];
    snprintf(sol, sizeof(sol), "sol %s  %s", solNasce, solPoe);
    uiTexto(12, 132, sol, 0, gold);
  }
  if (diasN > 0 && dias[0].chuva >= 0) {
    char chuva[20];
    snprintf(chuva, sizeof(chuva), "chuva %d%%", dias[0].chuva);
    uiTexto(150, 132, chuva, 0, cyan);
  }
  uiTexto(12, 156, "proximos dias", 0, muted);
  int linhas = 0;
  for (int i = 1; i < diasN && linhas < 5; i++, linhas++) {
    int y = 176 + linhas * 22;
    uiTexto(12, y, dias[i].rotulo, 0, ink);
    climaMini(72, y - 4, dias[i].cod);
    char frio[12], quente[12];
    snprintf(frio, sizeof(frio), "%d°", dias[i].tmin);
    snprintf(quente, sizeof(quente), "%d°", dias[i].tmax);
    uiTexto(108, y, frio, 0, muted);
    uiTexto(148, y, quente, 0, ink);
    if (dias[i].chuva >= 0) {
      char p[8];
      snprintf(p, sizeof(p), "%d%%", dias[i].chuva);
      uiTexto(190, y, p, 0, cyan);
    }
  }
}

static const char *listaDe(int tela) {
  if (tela == TELA_FX) return aj.fx;
  if (tela == TELA_CRIPTO) return aj.cripto;
  if (tela == TELA_BR) return aj.br;
  if (tela == TELA_US) return aj.eua;
  return "";
}

static void desenharLista(int tela, int y0) {
  char sims[6][12];
  int n = dadosDividir(listaDe(tela), sims, 6);
  if (!n) {
    uiTexto(centro("lista vazia", 1), y0 + 48, "lista vazia", 1, muted);
    return;
  }
  int slot = (286 - y0) / 46;
  if (slot > 5) slot = 5;
  if (slot < 1) slot = 1;
  int linhas = n > slot ? slot : n;
  int inicio = paginaLista[tela] % n;
  for (int i = 0; i < linhas; i++) {
    const char *rotulo = sims[(inicio + i) % n];
    char base[12];
    strcpy(base, rotulo);
    if (tela == TELA_CRIPTO) dadosBaseCripto(rotulo, base, sizeof(base));
    int y = y0 + i * 46;
    frame.fillRoundRect(10, y, 220, 42, 10, card);
    uiTexto(20, y + 10, base, 1, gold);
    const Cotacao *q = cotacaoDe(base);
    const char *preco = (q && q->ok) ? q->preco : "--";
    char pct[16];
    uint16_t cor = muted;
    pctDe(q, pct, sizeof(pct), &cor);
    int pw = uiLargura(preco, 1);
    uiTexto(220 - pw, y + 2, preco, 1, ink);
    if (pct[0]) {
      int cw = uiLargura(pct, 0);
      uiTexto(220 - cw, y + 24, pct, 0, cor);
    }
  }
}

static void desenharGrafico(int y0) {
  char par[16];
  strncpy(par, aj.par, sizeof(par) - 1);
  par[sizeof(par) - 1] = 0;
  for (char *p = par; *p; p++)
    if (*p == '-') *p = '/';
  uiTexto(12, y0, par, 1, gold);
  char sim[8];
  strncpy(sim, aj.par, 7);
  sim[7] = 0;
  char *traco = strchr(sim, '-');
  if (!traco) traco = strchr(sim, '/');
  if (traco) *traco = 0;
  for (char *p = sim; *p; p++)
    if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);
  const Cotacao *q = cotacaoDe(sim);
  int x = uiTexto(12, y0 + 28, (q && q->ok) ? q->preco : "--", 2, ink);
  char pct[16];
  uint16_t cor = muted;
  pctDe(q, pct, sizeof(pct), &cor);
  if (pct[0]) uiTexto(x + 8, y0 + 36, pct, 0, cor);
  if (serieN < 2) {
    uiTexto(centro("sem série ainda", 1), y0 + 90, "sem série ainda", 1, muted);
    return;
  }
  float mn = serie[0], mx = serie[0];
  for (int i = 1; i < serieN; i++) {
    if (serie[i] < mn) mn = serie[i];
    if (serie[i] > mx) mx = serie[i];
  }
  float faixa = mx - mn;
  if (faixa < 0.0001f) faixa = 1;
  uint16_t linha = serie[serieN - 1] >= serie[0] ? up : down;
  int x0 = 16, gy = y0 + 78, w = 208, h = 150;
  if (gy + h > 286) h = 286 - gy;
  frame.drawLine(x0, gy + h, x0 + w, gy + h, frame.color565(40, 52, 68));
  int prevX = 0, prevY = 0;
  for (int i = 0; i < serieN; i++) {
    int px = x0 + i * w / (serieN - 1);
    int py = gy + h - (int)((serie[i] - mn) / faixa * (h - 8)) - 4;
    if (i) {
      frame.drawLine(prevX, prevY, px, py, linha);
      frame.drawLine(prevX, prevY + 1, px, py + 1, linha);
    }
    prevX = px;
    prevY = py;
  }
  frame.fillCircle(prevX, prevY, 3, linha);
}

static int telaDaAba() {
  static const int ids[] = {TELA_FX, TELA_CRIPTO, TELA_BR, TELA_US, TELA_PAR};
  if (abaCot < 0 || abaCot > 4) abaCot = 0;
  return ids[abaCot];
}

static void telaCotacoes() {
  frame.fillSprite(bg);
  static const char *abas[] = {"FX", "Cripto", "BR", "EUA", "Graf"};
  for (int i = 0; i < 5; i++) {
    int x = 4 + i * 47;
    if (i == abaCot) frame.fillRoundRect(x, 6, 44, 22, 8, card);
    uint16_t cor = i == abaCot ? gold : muted;
    uiTexto(x + (44 - uiLargura(abas[i], 0)) / 2, 8, abas[i], 0, cor);
  }
  if (abaCot == 4) desenharGrafico(36);
  else desenharLista(telaDaAba(), 36);
}

static int quebrar(const char *t, int tam, int maxW, char linhas[][48], int maxL) {
  int n = 0;
  const char *p = t ? t : "";
  char atual[48] = "";
  while (*p && n < maxL) {
    while (*p == ' ') p++;
    if (!*p) break;
    const char *ini = p;
    while (*p && *p != ' ') p++;
    int len = (int)(p - ini);
    if (len > 46) len = 46;
    char palavra[48];
    memcpy(palavra, ini, len);
    palavra[len] = 0;
    char teste[48];
    if (atual[0]) snprintf(teste, sizeof(teste), "%s %s", atual, palavra);
    else snprintf(teste, sizeof(teste), "%s", palavra);
    if (uiLargura(teste, tam) <= maxW && atual[0]) strncpy(atual, teste, sizeof(atual) - 1);
    else if (!atual[0]) strncpy(atual, palavra, sizeof(atual) - 1);
    else {
      strncpy(linhas[n], atual, 47);
      linhas[n][47] = 0;
      n++;
      strncpy(atual, palavra, sizeof(atual) - 1);
    }
    atual[sizeof(atual) - 1] = 0;
  }
  if (atual[0] && n < maxL) {
    strncpy(linhas[n], atual, 47);
    linhas[n][47] = 0;
    n++;
  }
  return n;
}

static void telaDescanso() {
  bool noite = eNoite();
  uint16_t fundo = noite ? frame.color565(2, 3, 5) : bg;
  uint16_t horaCor = noite ? frame.color565(118, 126, 136) : ink;
  uint16_t suave = noite ? frame.color565(72, 78, 88) : muted;
  uint16_t acento = noite ? frame.color565(110, 88, 36) : gold;
  frame.fillSprite(fundo);
  char data[20];
  dataTexto(data, sizeof(data));
  uiTexto(centro(data, 0), 16, data, 0, suave);
  char hora[8];
  relogioTexto(hora, sizeof(hora));
  uiTexto(centro(hora, 4), 36, hora, 4, horaCor);
  if (climaOk) {
    icone(28, 128, noite);
    char graus[12];
    snprintf(graus, sizeof(graus), "%.0f", tempC);
    int x = uiTexto(108, 140, graus, 3, horaCor);
    uiTexto(x + 2, 146, "°", 2, acento);
    uiTexto(108, 196, nomeClima(), 0, suave);
  } else {
    uiTexto(centro("buscando o clima", 0), 168, "buscando o clima", 0, suave);
  }
  char linhas[3][48];
  int n = quebrar(frase, 0, 216, linhas, 3);
  int y = 232;
  for (int i = 0; i < n; i++) {
    uiTexto(12, y, linhas[i], 0, noite ? suave : ink);
    y += 20;
  }
  if (fraseAutor[0]) {
    char ass[44];
    snprintf(ass, sizeof(ass), "- %s", fraseAutor);
    uiTexto(TELA_W - 12 - uiLargura(ass, 0), 294, ass, 0, suave);
  }
}

static void marcar(int x, int y, int w, int h, int id) {
  if (areasN >= 12) return;
  areas[areasN++] = {x, y, w, h, id};
}

static void iconeMenu(int id, int x, int y) {
  int cx = x + 14;
  int cy = y + 14;
  if (id == TELA_CLIMA) {
    frame.fillCircle(cx, cy, 5, gold);
    for (int i = 0; i < 6; i++) {
      float a = i * 1.0472f;
      frame.drawLine(cx + (int)(cosf(a) * 8), cy + (int)(sinf(a) * 8), cx + (int)(cosf(a) * 12), cy + (int)(sinf(a) * 12), gold);
    }
  } else if (id == TELA_FX) {
    frame.fillRect(x + 4, y + 16, 5, 8, cyan);
    frame.fillRect(x + 11, y + 10, 5, 14, gold);
    frame.fillRect(x + 18, y + 6, 5, 18, up);
  } else if (id == TELA_VAL) {
    frame.fillTriangle(cx, y + 2, x + 26, cy, cx, y + 26, uiCor(255, 70, 85));
    frame.fillTriangle(x + 2, cy, cx, y + 2, cx, y + 26, uiCor(180, 40, 55));
  } else if (id == TELA_LUA) {
    frame.fillCircle(cx, cy, 10, frame.color565(236, 230, 206));
    frame.fillCircle(cx + 6, cy - 2, 8, frame.color565(12, 18, 28));
  } else if (id == TELA_ORBITA) {
    frame.drawCircle(cx, cy, 10, cyan);
    frame.fillCircle(cx, cy, 3, frame.color565(70, 110, 150));
    frame.fillCircle(cx + 9, cy - 6, 3, gold);
  } else if (id == TELA_RADAR) {
    frame.drawCircle(cx, cy, 5, up);
    frame.drawCircle(cx, cy, 10, up);
    frame.fillTriangle(x + 8, cy, x + 22, cy - 4, x + 22, cy + 4, ink);
  } else if (id == TELA_FERIADO) {
    frame.fillRoundRect(x + 6, y + 4, 16, 18, 3, ink);
    frame.fillRect(x + 6, y + 4, 16, 5, down);
    frame.fillRect(x + 9, y + 12, 10, 2, card);
    frame.fillRect(x + 9, y + 16, 7, 2, card);
  } else if (id == TELA_LUZ) {
    frame.fillCircle(cx, cy, 6, gold);
    frame.drawLine(cx, y + 2, cx, y + 6, gold);
    frame.drawLine(cx - 8, cy, cx - 4, cy, gold);
    frame.drawLine(cx + 4, cy, cx + 8, cy, gold);
    frame.drawLine(cx - 6, cy - 6, cx - 3, cy - 3, gold);
    frame.drawLine(cx + 3, cy + 3, cx + 6, cy + 6, gold);
    frame.drawLine(cx + 6, cy - 6, cx + 3, cy - 3, gold);
    frame.drawLine(cx - 3, cy + 3, cx - 6, cy + 6, gold);
  }
}

static void telaMenu() {
  areasN = 0;
  frame.fillSprite(bg);
  frame.fillRect(0, 0, TELA_W, 3, gold);
  uiTexto(12, 8, "Menu", 1, gold);
  String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  uiTexto(TELA_W - 12 - uiLargura(ip.c_str(), 0), 12, ip.c_str(), 0, muted);

  static const int ids[] = {TELA_CLIMA, TELA_FX, TELA_VAL, TELA_LUA, TELA_ORBITA, TELA_RADAR, TELA_FERIADO, TELA_LUZ};
  static const int bits[] = {0, 1, 6, 2, 3, 4, 5, 7};
  static const char *nomes[] = {"Clima", "Cotações", "Valorant", "Lua", "Órbita", "Radar", "Feriados", "Luz"};
  int n = 0;
  int usar[8];
  for (int i = 0; i < 8; i++) {
    if (aj.telas & (1 << bits[i])) usar[n++] = i;
  }
  if (!n) usar[n++] = 0;
  const int bh = 50;
  const int gap = 6;
  int y0 = 38;
  for (int i = 0; i < n; i++) {
    int item = usar[i];
    int col = i % 2;
    int row = i / 2;
    int x = 10 + col * 112;
    int y = y0 + row * (bh + gap);
    frame.fillRoundRect(x, y, 106, bh, 12, card);
    frame.fillCircle(x + 22, y + 25, 15, frame.color565(12, 18, 28));
    iconeMenu(ids[item], x + 8, y + 11);
    uiTexto(x + 42, y + 16, nomes[item], 0, ink);
    marcar(x, y, 106, bh, ids[item]);
  }
  int linhas = (n + 1) / 2;
  int by = y0 + linhas * (bh + gap) + 2;
  if (by > 270) by = 270;
  frame.fillRoundRect(10, by, 220, 36, 12, frame.color565(32, 42, 58));
  uiTexto(10 + (220 - uiLargura("Descanso", 1)) / 2, by + 6, "Descanso", 1, gold);
  marcar(10, by, 220, 36, TELA_DESCANSO);
}

static void telaWifi(const char *linha) {
  frame.fillSprite(bg);
  uiTexto(centro("Painel", 2), 36, "Painel", 2, gold);
  uiTexto(centro(linha, 1), 78, linha, 1, ink);
  bool casa = WiFi.status() == WL_CONNECTED;
  const char *rede = casa ? aj.ssid : "Painel-N16R8";
  uiTexto(centro(rede, 1), 112, rede, 1, cyan);
  String ip = casa ? WiFi.localIP().toString() : String("192.168.4.1");
  uiTexto(centro(ip.c_str(), 3), 150, ip.c_str(), 3, ink);
  const char *dica = casa ? "abra esse endereço" : "entre nesse Wi-Fi";
  uiTexto(centro(dica, 0), 214, dica, 0, muted);
  const char *dica2 = casa ? "e configure as telas" : "e abra o endereço";
  uiTexto(centro(dica2, 0), 234, dica2, 0, muted);
}

static void parStat(int x, int w, int y, const char *num, const char *letra, uint16_t cor) {
  int nw = uiLargura(num, 1);
  int lw = uiLargura(letra, 0);
  int sx = x + (w - (nw + 4 + lw)) / 2;
  uiTexto(sx, y, num, 1, cor);
  uiTexto(sx + nw + 4, y + 6, letra, 0, muted);
}

static void tempoAtras(time_t quando, char *dest, int n) {
  time_t agora = time(nullptr);
  if (quando < 1700000000 || agora < quando) {
    dest[0] = 0;
    return;
  }
  int s = (int)(agora - quando);
  if (s < 90) snprintf(dest, n, "agora");
  else if (s < 3600) snprintf(dest, n, "há %d min", s / 60);
  else if (s < 86400) snprintf(dest, n, "há %d h", s / 3600);
  else snprintf(dest, n, "há %d d", s / 86400);
}

static void telaValorant() {
  uint16_t fundo = uiCor(15, 25, 35);
  uint16_t cartao = uiCor(27, 39, 51);
  uint16_t vermelho = uiCor(255, 70, 85);
  frame.fillSprite(fundo);
  frame.fillRect(0, 0, TELA_W, 3, vermelho);

  if (!val.temRank && !val.temPartida) {
    const char *msg = val.erro[0] ? val.erro : "buscando";
    uiTexto(centro(msg, 1), 118, msg, 1, ink);
    if (aj.riot[0]) uiTexto(centro(aj.riot, 0), 156, aj.riot, 0, muted);
    if (!aj.vkey[0]) uiTexto(centro("cole a chave na dashboard", 0), 182, "cole a chave na dashboard", 0, muted);
    return;
  }

  uiTexto(12, 8, "VALORANT", 0, vermelho);
  char tempo[20];
  tempoAtras(val.quando, tempo, sizeof(tempo));
  if (tempo[0]) uiTexto(TELA_W - 12 - uiLargura(tempo, 0), 8, tempo, 0, muted);

  const char *nome = val.nome[0] ? val.nome : aj.riot;
  int nx = uiTexto(12, 28, nome, 2, ink);
  if (val.tag[0] && nx < 188) uiTexto(nx + 6, 40, val.tag, 0, muted);

  frame.setSwapBytes(true);
  if (valEmblema && valEmblemaW > 0) frame.pushImage(14, 62, valEmblemaW, valEmblemaH, valEmblema);
  if (val.rank[0]) {
    int tam = uiLargura(val.rank, 1) <= 146 ? 1 : 0;
    uiTexto(78, tam == 1 ? 66 : 74, val.rank, tam, ink);
  }
  if (val.rr >= 0) {
    char rr[16];
    snprintf(rr, sizeof(rr), "%d RR", val.rr);
    uiTexto(78, 94, rr, 1, ink);
  }
  if (val.delta != -999) {
    char delta[12];
    snprintf(delta, sizeof(delta), "%+d", val.delta);
    uint16_t cor = val.delta > 0 ? up : (val.delta < 0 ? down : muted);
    uiTexto(TELA_W - 14 - uiLargura(delta, 1), 94, delta, 1, cor);
  }

  frame.fillRoundRect(10, 124, 220, 166, 10, cartao);
  if (valFoto && valFotoW > 0) {
    frame.pushImage(18, 132, valFotoW, valFotoH, valFoto);
  }
  frame.setSwapBytes(false);
  if (!(valFoto && valFotoW > 0) && val.agente[0]) {
    frame.fillRoundRect(18, 132, 80, 80, 8, uiCor(12, 18, 28));
    char letra[2] = {val.agente[0], 0};
    uiTexto(18 + (80 - uiLargura(letra, 2)) / 2, 152, letra, 2, vermelho);
  }
  if (val.agente[0]) {
    int aw = uiLargura(val.agente, 0);
    int ax = aw <= 80 ? 18 + (80 - aw) / 2 : 18;
    uiTexto(ax, 214, val.agente, 0, ink);
  }

  if (!val.temPartida) {
    uiTexto(108, 170, "sem competitiva", 0, muted);
    return;
  }
  if (val.fila[0]) uiTexto(108, 130, val.fila, 0, muted);
  if (val.temPlacar) {
    const char *res = val.nossos == val.deles ? "Empate" : (val.venceu ? "Vitória" : "Derrota");
    uiTexto(108, 150, res, 1, val.venceu && val.nossos != val.deles ? up : (val.nossos == val.deles ? muted : down));
  }
  if (val.mapa[0]) uiTexto(108, 176, val.mapa, 1, ink);
  if (val.temPlacar) {
    char a[8], b[8];
    snprintf(a, sizeof(a), "%d", val.nossos);
    snprintf(b, sizeof(b), "%d", val.deles);
    uint16_t corNossa = val.venceu ? up : down;
    int x = uiTexto(108, 202, a, 2, corNossa);
    x = uiTexto(x + 8, 212, "-", 1, muted);
    uiTexto(x + 8, 202, b, 2, muted);
  }

  char k[8], d[8], a[8];
  snprintf(k, sizeof(k), "%d", val.kills);
  snprintf(d, sizeof(d), "%d", val.deaths);
  snprintf(a, sizeof(a), "%d", val.assists);
  parStat(12, 72, 238, k, "K", ink);
  parStat(84, 72, 238, d, "D", down);
  parStat(156, 72, 238, a, "A", cyan);
  char info[48];
  snprintf(info, sizeof(info), "HS %d%%   ACS %d   ADR %d", val.hsPct, val.acs, val.adr);
  uiTexto((TELA_W - uiLargura(info, 0)) / 2, 268, info, 0, muted);
}

static float faseLua() {
  time_t agora = time(nullptr);
  if (agora < 1700000000) return 0.5f;
  double dias = ((double)agora - 947182440.0) / 86400.0;
  double f = fmod(dias, 29.530588853) / 29.530588853;
  if (f < 0) f += 1;
  return (float)f;
}

static const char *nomeFase(float f) {
  if (f < 0.03f || f > 0.97f) return "Lua nova";
  if (f < 0.22f) return "Crescente";
  if (f < 0.28f) return "Quarto crescente";
  if (f < 0.47f) return "Gibosa";
  if (f < 0.53f) return "Lua cheia";
  if (f < 0.72f) return "Gibosa minguante";
  if (f < 0.78f) return "Quarto minguante";
  return "Minguante";
}

static void luaDisco(int cx, int cy, int r, float fase) {
  uint16_t luz = frame.color565(236, 230, 206);
  uint16_t sombra = frame.color565(18, 26, 38);
  uint16_t borda = frame.color565(120, 116, 96);
  for (int y = -r; y <= r; y++) {
    if ((y & 3) == 0) yield();
    for (int x = -r; x <= r; x++) {
      float nx = x / (float)r;
      float ny = y / (float)r;
      float d2 = nx * nx + ny * ny;
      if (d2 > 1.f) continue;
      float nz = sqrtf(1.f - d2);
      float ang = (0.5f - fase) * 6.2831853f;
      float dot = (-nx) * sinf(ang) + nz * cosf(ang);
      frame.drawPixel(cx + x, cy + y, dot > 0.08f ? luz : (dot > -0.05f ? borda : sombra));
    }
  }
}

static void telaLua() {
  frame.fillSprite(bg);
  uiTexto(12, 8, "Lua", 1, gold);
  float fase = faseLua();
  luaDisco(120, 118, 52, fase);
  const char *nome = nomeFase(fase);
  uiTexto(centro(nome, 1), 184, nome, 1, ink);
  int luz = (int)((1.f - cosf(fase * 6.2831853f)) * 50.f + 0.5f);
  if (luz < 0) luz = 0;
  if (luz > 100) luz = 100;
  char pct[24];
  snprintf(pct, sizeof(pct), "%d%% iluminada", luz);
  uiTexto(centro(pct, 0), 214, pct, 0, muted);
  float ate = fase <= 0.5f ? 0.5f - fase : 1.5f - fase;
  int dias = (int)(ate * 29.53f + 0.5f);
  char quando[32];
  if (fase > 0.47f && fase < 0.53f) snprintf(quando, sizeof(quando), "cheia agora");
  else snprintf(quando, sizeof(quando), "cheia em %d dias", dias);
  uiTexto(centro(quando, 0), 236, quando, 0, gold);
  const char *dica = (fase < 0.5f) ? "aqui a luz cresce pela esquerda" : "a sombra volta pela esquerda";
  uiTexto(centro(dica, 0), 262, dica, 0, muted);
}

static void telaOrbita() {
  frame.fillSprite(bg);
  uiTexto(12, 8, "Órbita", 1, gold);
  if (!ceu.ok) {
    uiTexto(centro("buscando a estação", 1), 140, "buscando a estação", 1, muted);
    return;
  }
  int cx = 58, cy = 92, raio = 34;
  frame.fillCircle(cx, cy, raio, frame.color565(16, 36, 58));
  frame.drawCircle(cx, cy, raio, cyan);
  frame.fillCircle(cx, cy, 3, gold);
  float la1 = (float)atof(aj.lat) * 0.0174533f;
  float dlon = (ceu.lon - (float)atof(aj.lon)) * 0.0174533f;
  float la2 = ceu.lat * 0.0174533f;
  float by = sinf(dlon) * cosf(la2);
  float bx = cosf(la1) * sinf(la2) - sinf(la1) * cosf(la2) * cosf(dlon);
  float brng = atan2f(by, bx);
  int solo = ceu.km;
  int alcance = solo > 4000 ? raio - 4 : (int)(solo / 4000.f * (raio - 6));
  if (alcance < 8) alcance = 8;
  int dx = (int)(sinf(brng) * alcance);
  int dy = (int)(-cosf(brng) * alcance);
  frame.fillCircle(cx + dx, cy + dy, 4, ink);
  uiTexto(108, 48, "ISS", 1, gold);
  char linha[28];
  snprintf(linha, sizeof(linha), "a %d km", ceu.km);
  uiTexto(108, 76, linha, 0, ink);
  snprintf(linha, sizeof(linha), "%d km de altura", ceu.alt);
  uiTexto(108, 98, linha, 0, muted);
  snprintf(linha, sizeof(linha), "%d km/h", ceu.vel);
  uiTexto(108, 118, linha, 0, cyan);
  if (ceu.pessoas > 0) {
    char titulo[28];
    snprintf(titulo, sizeof(titulo), "%d a bordo", ceu.pessoas);
    uiTexto(12, 148, titulo, 0, gold);
    for (int i = 0; i < ceu.pessoas && i < 6; i++) uiTexto(12, 170 + i * 18, ceu.nome[i], 0, ink);
  }
  if (ceu.fora > 0) {
    char extra[28];
    snprintf(extra, sizeof(extra), "+ %d na Tiangong", ceu.fora);
    uiTexto(12, 278, extra, 0, muted);
  }
}

static void textoAlt(int metros, char *dest, int n) {
  if (metros <= 0) snprintf(dest, n, "--");
  else if (metros >= 1000) {
    snprintf(dest, n, "%.1f km", metros / 1000.f);
    for (char *p = dest; *p; p++)
      if (*p == '.') *p = ',';
  } else snprintf(dest, n, "%d m", metros);
}

static void telaRadar() {
  frame.fillSprite(bg);
  uiTexto(12, 4, "Radar", 0, gold);
  uiTexto(TELA_W - 12 - uiLargura("80 km", 0), 4, "80 km", 0, muted);

  const int cx = 120;
  const int cy = 116;
  const int raio = 90;
  const float alcance = 80.f;
  uint16_t grade = frame.color565(26, 72, 46);
  uint16_t fosforo = frame.color565(120, 230, 140);
  frame.fillCircle(cx, cy, raio, frame.color565(4, 14, 10));
  frame.drawCircle(cx, cy, raio, grade);
  frame.drawCircle(cx, cy, raio * 2 / 3, grade);
  frame.drawCircle(cx, cy, raio / 3, grade);
  frame.drawLine(cx - raio, cy, cx + raio, cy, grade);
  frame.drawLine(cx, cy - raio, cx, cy + raio, grade);

  float giro = (millis() % 3600) / 3600.f * 6.2831853f;
  for (int i = 14; i >= 0; i--) {
    float a = giro - i * 0.045f;
    int verde = 18 + (14 - i) * 8;
    uint16_t cor = frame.color565(8, verde, 12);
    int x2 = cx + (int)(sinf(a) * (raio - 2));
    int y2 = cy - (int)(cosf(a) * (raio - 2));
    frame.drawLine(cx, cy, x2, y2, cor);
  }
  uiTexto(cx - 4, cy - raio + 6, "N", 0, fosforo);

  frame.fillCircle(cx, cy, 3, gold);
  for (int i = 0; i < voosN; i++) {
    float a = voos[i].rumo * 0.0174533f;
    float t = voos[i].km / alcance;
    if (t > 1.f) t = 1.f;
    int px = cx + (int)(sinf(a) * t * (raio - 10));
    int py = cy - (int)(cosf(a) * t * (raio - 10));
    uint16_t cor = i == 0 ? gold : fosforo;
    frame.fillCircle(px, py, i == 0 ? 4 : 3, cor);
    if (voos[i].proa >= 0) {
      float h = voos[i].proa * 0.0174533f;
      frame.drawLine(px, py, px + (int)(sinf(h) * 11), py - (int)(cosf(h) * 11), cor);
    }
  }

  if (!voosOk) uiTexto(centro("buscando", 0), 214, "buscando", 0, muted);
  else if (!voosN) uiTexto(centro("céu limpo por aqui", 0), 214, "céu limpo por aqui", 0, muted);
  else {
    for (int i = 0; i < voosN && i < 2; i++) {
      int y = 212 + i * 36;
      char alt[16], dist[16], linha[40];
      textoAlt(voos[i].alt, alt, sizeof(alt));
      if (voos[i].km < 1) snprintf(dist, sizeof(dist), "aqui");
      else snprintf(dist, sizeof(dist), "%d km", voos[i].km);
      uiTexto(12, y, voos[i].nome, 1, i == 0 ? gold : ink);
      if (voos[i].vel > 0) snprintf(linha, sizeof(linha), "%s   %s   %d km/h", dist, alt, voos[i].vel);
      else snprintf(linha, sizeof(linha), "%s   %s", dist, alt);
      uiTexto(12, y + 18, linha, 0, muted);
    }
  }
}

static void telaFeriados() {
  frame.fillSprite(bg);
  uiTexto(12, 8, "Feriados", 1, gold);
  if (!feriadosOk || feriadosN < 1) {
    uiTexto(centro("buscando o calendário", 1), 140, "buscando o calendário", 1, muted);
    return;
  }
  char grande[16];
  if (feriados[0].dias == 0) snprintf(grande, sizeof(grande), "hoje");
  else if (feriados[0].dias == 1) snprintf(grande, sizeof(grande), "amanhã");
  else snprintf(grande, sizeof(grande), "%d dias", feriados[0].dias);
  uiTexto(centro(grande, 2), 40, grande, 2, gold);
  int tam = uiLargura(feriados[0].nome, 1) <= 220 ? 1 : 0;
  uiTexto(centro(feriados[0].nome, tam), 82, feriados[0].nome, tam, ink);
  uiTexto(centro(feriados[0].quando, 0), 108, feriados[0].quando, 0, muted);
  for (int i = 1; i < feriadosN; i++) {
    int y = 132 + (i - 1) * 36;
    frame.fillRoundRect(10, y, 220, 32, 8, card);
    uiTexto(18, y + 6, feriados[i].nome, 0, ink);
    char lado[20];
    if (feriados[i].dias == 0) snprintf(lado, sizeof(lado), "hoje");
    else snprintf(lado, sizeof(lado), "%d d", feriados[i].dias);
    uiTexto(222 - uiLargura(lado, 0), y + 6, lado, 0, gold);
  }
}

static void telaLuz() {
  frame.fillSprite(bg);
  uiTexto(12, 8, "Luz", 1, gold);
  uiTexto(12, 32, "o ponto de luz da placa", 0, muted);
  static const int cor[] = {1, 2, 3, 4, 5, 0};
  static const uint8_t rgb[][3] = {{90, 12, 16}, {20, 140, 60}, {30, 70, 180}, {200, 140, 20}, {140, 40, 170}, {20, 24, 32}};
  static const char *nome[] = {"vermelho", "verde", "azul", "ambar", "violeta", "apagada"};
  int atual = painelLuzAtual();
  for (int i = 0; i < 6; i++) {
    int cx = (i % 2) ? 168 : 72;
    int cy = 78 + (i / 2) * 58;
    uint16_t miolo = uiCor(rgb[i][0], rgb[i][1], rgb[i][2]);
    if (atual == cor[i]) frame.fillCircle(cx, cy, 22, gold);
    frame.fillCircle(cx, cy, atual == cor[i] ? 18 : 20, miolo);
    if (cor[i] == 0) frame.drawCircle(cx, cy, 16, muted);
    uiTexto(cx - uiLargura(nome[i], 0) / 2, cy + 24, nome[i], 0, muted);
  }
  bool autoOn = atual < 0;
  frame.fillRoundRect(36, 258, 168, 28, 10, autoOn ? gold : card);
  uiTexto(36 + (168 - uiLargura("sozinho", 0)) / 2, 262, "sozinho", 0, autoOn ? bg : ink);
}

void uiDesenhar(int tela) {
  if (tela == TELA_WIFI) telaWifi(WiFi.status() == WL_CONNECTED ? "na rede" : "sem a rede de casa");
  else if (tela == TELA_DESCANSO) telaDescanso();
  else if (tela == TELA_MENU) telaMenu();
  else if (tela == TELA_VAL) telaValorant();
  else if (tela == TELA_CLIMA) telaClima();
  else if (tela == TELA_FX || tela == TELA_CRIPTO || tela == TELA_BR || tela == TELA_US || tela == TELA_PAR) telaCotacoes();
  else if (tela == TELA_LUA) telaLua();
  else if (tela == TELA_ORBITA) telaOrbita();
  else if (tela == TELA_RADAR) telaRadar();
  else if (tela == TELA_FERIADO) telaFeriados();
  else if (tela == TELA_LUZ) telaLuz();
  else if (tela == TELA_NOITE) telaDescanso();
  else telaCotacoes();
  if (tela != TELA_WIFI && tela != TELA_DESCANSO && tela != TELA_MENU && tela != TELA_NOITE) rodape(tela);
  frame.pushSprite(0, 0);
}

int uiToque(int tela, int x, int y) {
  if (tela == TELA_DESCANSO || tela == TELA_NOITE) return TELA_MENU;
  if (tela == TELA_WIFI) return -1;
  if (tela == TELA_MENU) {
    for (int i = 0; i < areasN; i++) {
      Area a = areas[i];
      if (x >= a.x && x < a.x + a.w && y >= a.y && y < a.y + a.h) return a.id;
    }
    return -1;
  }
  if (tela == TELA_LUZ) {
    static const int cor[] = {1, 2, 3, 4, 5, 0};
    for (int i = 0; i < 6; i++) {
      int cx = (i % 2) ? 168 : 72;
      int cy = 78 + (i / 2) * 58;
      int dx = x - cx;
      int dy = y - cy;
      if (dx * dx + dy * dy <= 26 * 26) {
        painelLuz(cor[i]);
        painelSujo = true;
        return -1;
      }
    }
    if (y >= 250 && y < 290) {
      painelLuz(-1);
      painelSujo = true;
      return -1;
    }
  }
  if (tela == TELA_FX || tela == TELA_PAR || tela == TELA_CRIPTO || tela == TELA_BR || tela == TELA_US) {
    if (y < 34) {
      int i = (x - 4) / 47;
      if (i >= 0 && i < 5) {
        abaCot = i;
        painelSujo = true;
      }
      return -1;
    }
  }
  int tw = uiLargura("descanso", 0);
  if (y >= 290 && x >= TELA_W - 20 - tw) return TELA_DESCANSO;
  if (abaCot < 4 && (tela == TELA_FX || tela == TELA_CRIPTO || tela == TELA_BR || tela == TELA_US || tela == TELA_PAR)) {
    int lista = telaDaAba();
    char sims[6][12];
    if (dadosDividir(listaDe(lista), sims, 6) > 5) {
      uiAvancarPagina(lista);
      painelSujo = true;
    }
  }
  return -1;
}

void uiAvancarPagina(int tela) {
  if (tela >= 0 && tela < 6) paginaLista[tela]++;
}

void uiCalibrar(int passo, int z) {
  frame.fillSprite(bg);
  uiTexto(centro("Toque a cruz", 2), 120, "Toque a cruz", 2, ink);
  char num[16];
  snprintf(num, sizeof(num), "%d de 4", passo + 1);
  uiTexto(centro(num, 1), 156, num, 1, muted);
  char pressao[20];
  snprintf(pressao, sizeof(pressao), "pressao %d", z < 0 ? 0 : z);
  uiTexto(centro(pressao, 0), 190, pressao, 0, muted);
  uiTexto(centro("ou segure BOOT", 0), 214, "ou segure BOOT", 0, muted);
  static const int ax[4] = {28, 212, 212, 28};
  static const int ay[4] = {36, 36, 284, 284};
  int x = ax[passo];
  int y = ay[passo];
  frame.drawCircle(x, y, 16, down);
  frame.drawLine(x - 24, y, x + 24, y, down);
  frame.drawLine(x, y - 24, x, y + 24, down);
  frame.pushSprite(0, 0);
}

void uiBoot(const char *linha) {
  frame.fillSprite(bg);
  uiTexto(centro("Painel", 2), 80, "Painel", 2, gold);
  uiTexto(centro(linha, 1), 130, linha, 1, ink);
  if (aj.ssid[0]) uiTexto(centro(aj.ssid, 0), 170, aj.ssid, 0, cyan);
  frame.pushSprite(0, 0);
}
