#pragma once

#include <Arduino.h>

static const int TELA_W = 240;
static const int TELA_H = 320;

enum TelaId : uint8_t {
  TELA_CLIMA = 0,
  TELA_FX = 1,
  TELA_PAR = 2,
  TELA_CRIPTO = 3,
  TELA_BR = 4,
  TELA_US = 5,
  TELA_VAL = 6,
  TELA_NOITE = 7,
  TELA_WIFI = 8,
  TELA_MENU = 9,
  TELA_DESCANSO = 10,
  TELA_LUA = 11,
  TELA_ORBITA = 12,
  TELA_RADAR = 13,
  TELA_FERIADO = 14,
  TELA_LUZ = 15
};

struct Ajustes {
  char cidade[32];
  char lat[16];
  char lon[16];
  char fx[48];
  char cripto[48];
  char br[64];
  char eua[48];
  char par[16];
  char ssid[33];
  char senha[65];
  char riot[32];
  char vreg[8];
  char vkey[64];
  uint8_t telas;
  uint8_t cicloSeg;
  uint8_t noiteOn;
  uint8_t noite1;
  uint8_t noite2;
  uint8_t climaMin;
  uint8_t cotacaoMin;
  int32_t tX0, tX1, tY0, tY1;
  uint8_t tTroca;
  uint8_t tOk;
};

extern Ajustes aj;

void ajustesPadrao();
void ajustesCarregar();
void ajustesSalvar();

struct Cotacao {
  char sim[12];
  char preco[16];
  int pct100;
  bool ok;
};

extern Cotacao cotacoes[16];
extern float serie[20];
extern uint8_t serieN;
extern float tempC;
extern float tempMin;
extern float tempMax;
extern int climaCod;
extern int umidade;
extern bool climaOk;
extern int ultimoHttp;
extern char ultimaUrl[120];
extern char frase[160];
extern char fraseAutor[40];

struct DiaClima {
  int cod;
  int tmin;
  int tmax;
  int chuva;
  char rotulo[10];
};

extern DiaClima dias[6];
extern int diasN;
extern char solNasce[6];
extern char solPoe[6];

struct VooTela {
  char nome[12];
  char pais[16];
  int alt;
  int km;
  int rumo;
  int proa;
  int vel;
};

extern VooTela voos[5];
extern int voosN;
extern bool voosOk;

struct CeuTela {
  bool ok;
  int km, alt, vel, pessoas, fora;
  float lat, lon;
  char nome[6][20];
};

extern CeuTela ceu;

struct FeriadoTela {
  char quando[16];
  char nome[32];
  int dias;
};

extern FeriadoTela feriados[5];
extern int feriadosN;
extern bool feriadosOk;

struct ValTela {
  bool temRank;
  bool temPartida;
  bool temPlacar;
  bool venceu;
  int tier;
  int rr;
  int delta;
  int kills, deaths, assists;
  int hsPct, acs, adr;
  int nossos, deles;
  time_t quando;
  char nome[20];
  char tag[8];
  char agente[20];
  char agenteId[40];
  char mapa[24];
  char fila[24];
  char rank[28];
  char erro[40];
};

extern ValTela val;
extern uint16_t *valFoto;
extern int valFotoW, valFotoH;
extern uint16_t *valEmblema;
extern int valEmblemaW, valEmblemaH;

bool valorantAtualizar();
void valorantZerar();

const Cotacao *cotacaoDe(const char *sim);
void dadosZerar();
bool dadosClima();
bool dadosGeo(const char *nome);
void dadosAgendar();
void dadosTick();
void dadosOlhar(int tela);
int dadosDividir(const char *csv, char saida[][12], int maxN);
void dadosBaseCripto(const char *sim, char *base, int n);

bool horaCerta(int *h, int *m, int *s);
bool eNoite();
void relogioTexto(char *saida, int n);
void dataTexto(char *saida, int n);
const char *nomeClima();
const char *nomeClimaCod(int cod);

struct ProvaApi {
  char nome[20];
  bool ok;
  int codigo;
  char detalhe[48];
};

int dadosTestar(ProvaApi *saida, int max);

void uiIniciar();
int uiTexto(int x, int y, const char *t, int tam, uint16_t cor);
int uiLargura(const char *t, int tam);
void uiDesenhar(int tela);
int uiToque(int tela, int x, int y);
void uiCalibrar(int passo, int z);
void uiBoot(const char *linha);
void uiAvancarPagina(int tela);
extern bool painelSujo;
uint16_t uiCor(uint8_t r, uint8_t g, uint8_t b);

void toqueIniciar();
bool toqueLer(int &x, int &y, int &z);
bool toqueBruto(int &x, int &y, int &z);
void toqueGuardarCanto(int indice, int x, int y);
bool toqueFecharMapa();

void webIniciar();
void webAtender();

void painelLuz(int cor);
int painelLuzAtual();
