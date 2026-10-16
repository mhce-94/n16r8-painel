#include "painel.h"

static const int T_CLK = 42;
static const int T_CS = 1;
static const int T_DIN = 2;
static const int T_DO = 41;
static const int Z_MIN = 400;

struct Bruto {
  int x;
  int y;
  int z;
};

static int cantoX[4];
static int cantoY[4];

static uint8_t pulso(uint8_t dado) {
  uint8_t lido = 0;
  for (int i = 0; i < 8; i++) {
    digitalWrite(T_DIN, (dado & 0x80) ? HIGH : LOW);
    dado <<= 1;
    digitalWrite(T_CLK, HIGH);
    delayMicroseconds(1);
    lido <<= 1;
    if (digitalRead(T_DO)) lido |= 1;
    digitalWrite(T_CLK, LOW);
    delayMicroseconds(1);
  }
  return lido;
}

static uint16_t pulso16(uint16_t dado) {
  return (uint16_t)((pulso(dado >> 8) << 8) | pulso(dado & 0xFF));
}

void toqueIniciar() {
  pinMode(T_CLK, OUTPUT);
  pinMode(T_DIN, OUTPUT);
  pinMode(T_CS, OUTPUT);
  pinMode(T_DO, INPUT);
  digitalWrite(T_CS, HIGH);
  digitalWrite(T_CLK, LOW);
}

static Bruto lerBruto() {
  Bruto p{0, 0, 0};
  digitalWrite(T_CS, LOW);
  pulso(0xB1);
  int z1 = pulso16(0xC1) >> 3;
  int z = z1 + 4095;
  int z2 = pulso16(0x91) >> 3;
  z -= z2;
  if (z >= Z_MIN && z < 8000) {
    pulso16(0x91);
    int a1 = pulso16(0xD1) >> 3;
    int b1 = pulso16(0x91) >> 3;
    int a2 = pulso16(0xD1) >> 3;
    int b2 = pulso16(0x91) >> 3;
    p.x = (abs(a1 - a2) < 80) ? (a1 + a2) / 2 : a2;
    p.y = (abs(b1 - b2) < 80) ? (b1 + b2) / 2 : b2;
    p.z = z;
  }
  pulso(0x90);
  digitalWrite(T_CS, HIGH);
  return p;
}

bool toqueLer(int &x, int &y, int &z) {
  Bruto p = lerBruto();
  z = p.z;
  if (p.z < Z_MIN) return false;
  int brutoX = aj.tTroca ? p.y : p.x;
  int brutoY = aj.tTroca ? p.x : p.y;
  int dx = aj.tX1 - aj.tX0;
  int dy = aj.tY1 - aj.tY0;
  if (abs(dx) < 80 || abs(dy) < 80) {
    x = 0;
    y = 0;
    return true;
  }
  x = 28 + (int)((int32_t)(brutoX - aj.tX0) * (TELA_W - 56) / dx);
  y = 36 + (int)((int32_t)(brutoY - aj.tY0) * (TELA_H - 72) / dy);
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x >= TELA_W) x = TELA_W - 1;
  if (y >= TELA_H) y = TELA_H - 1;
  return true;
}

void toqueGuardarCanto(int indice, int x, int y) {
  if (indice < 0 || indice > 3) return;
  cantoX[indice] = x;
  cantoY[indice] = y;
}

bool toqueFecharMapa() {
  int aEsq = (cantoX[0] + cantoX[3]) / 2;
  int aDir = (cantoX[1] + cantoX[2]) / 2;
  int bEsq = (cantoY[0] + cantoY[3]) / 2;
  int bDir = (cantoY[1] + cantoY[2]) / 2;
  bool troca = abs(bDir - bEsq) > abs(aDir - aEsq);
  auto eixoX = [&](int i) { return troca ? cantoY[i] : cantoX[i]; };
  auto eixoY = [&](int i) { return troca ? cantoX[i] : cantoY[i]; };
  int x0 = (eixoX(0) + eixoX(3)) / 2;
  int x1 = (eixoX(1) + eixoX(2)) / 2;
  int y0 = (eixoY(0) + eixoY(1)) / 2;
  int y1 = (eixoY(2) + eixoY(3)) / 2;
  if (abs(x1 - x0) < 80 || abs(y1 - y0) < 80) return false;
  aj.tTroca = troca ? 1 : 0;
  aj.tX0 = x0;
  aj.tX1 = x1;
  aj.tY0 = y0;
  aj.tY1 = y1;
  aj.tOk = 1;
  return true;
}

bool toqueBruto(int &x, int &y, int &z) {
  Bruto p = lerBruto();
  x = p.x;
  y = p.y;
  z = p.z;
  return p.z >= Z_MIN;
}
