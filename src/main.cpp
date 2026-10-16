#include "painel.h"

#include <Adafruit_NeoPixel.h>
#include <WiFi.h>

bool painelSujo = true;

static Adafruit_NeoPixel rgb(1, 48, NEO_GRB + NEO_KHZ800);
static int modoTela = TELA_DESCANSO;
static uint32_t acordadoAte = 0;
static bool pressionado = false;
static int origemX = 0;
static int origemY = 0;
static int ultimoX = 0;
static int ultimoY = 0;
static int segundo = -1;

static void led(uint8_t r, uint8_t g, uint8_t b) {
  rgb.setPixelColor(0, rgb.Color(r, g, b));
  rgb.show();
}

static int luzFixa = -1;
static const uint8_t luzRgb[7][3] = {
    {0, 0, 0}, {90, 6, 8}, {4, 55, 18}, {6, 18, 80}, {80, 48, 6}, {55, 8, 70}, {46, 46, 40},
};

void painelLuz(int cor) {
  if (cor < -1) cor = -1;
  if (cor > 6) cor = 6;
  luzFixa = cor;
}

int painelLuzAtual() { return luzFixa; }

static void calibrar() {
  if (aj.tOk) return;
  pinMode(0, INPUT_PULLUP);
  delay(900);
  int passo = 0;
  bool viuSolto = false;
  int estavel = 0;
  int guardadoX = 0;
  int guardadoY = 0;
  int zVisto = 0;
  uiCalibrar(0, 0);
  while (passo < 4) {
    if (digitalRead(0) == LOW) {
      delay(500);
      if (digitalRead(0) == LOW) {
        aj.tOk = 1;
        aj.tX0 = 200;
        aj.tX1 = 3800;
        aj.tY0 = 200;
        aj.tY1 = 3800;
        aj.tTroca = 0;
        ajustesSalvar();
        return;
      }
    }
    int x = 0, y = 0, z = 0;
    bool baixo = toqueBruto(x, y, z);
    if (z != zVisto) {
      zVisto = z;
      uiCalibrar(passo, z);
    }
    if (!baixo) {
      viuSolto = true;
      if (estavel >= 5) {
        toqueGuardarCanto(passo, guardadoX, guardadoY);
        passo++;
        estavel = 0;
        viuSolto = false;
        if (passo < 4) uiCalibrar(passo, 0);
      } else {
        estavel = 0;
      }
      continue;
    }
    if (!viuSolto) continue;
    if (estavel > 0 && abs(x - guardadoX) < 70 && abs(y - guardadoY) < 70) estavel++;
    else estavel = 1;
    guardadoX = x;
    guardadoY = y;
  }
  if (!toqueFecharMapa()) {
    aj.tX0 = 200;
    aj.tX1 = 3800;
    aj.tY0 = 200;
    aj.tY1 = 3800;
    aj.tTroca = 0;
    aj.tOk = 1;
  }
  ajustesSalvar();
}

static void conectar() {
  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname("painel");
  if (aj.ssid[0]) {
    uiBoot("conectando");
    WiFi.mode(WIFI_STA);
    WiFi.begin(aj.ssid, aj.senha);
    uint32_t inicio = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - inicio < 18000) {
      delay(200);
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    configTzTime("<-03>3", "pool.ntp.org", "time.nist.gov");
    led(20, 50, 40);
    Serial.print("ip ");
    Serial.println(WiFi.localIP());
    return;
  }
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Painel-N16R8");
  led(50, 28, 0);
  Serial.println("ap 192.168.4.1");
}

static int telaAgora() {
  if (WiFi.status() != WL_CONNECTED) return TELA_WIFI;
  return modoTela;
}

static void acordar() { acordadoAte = millis() + (uint32_t)aj.cicloSeg * 1000UL; }

static void gesto(int x, int y) {
  int dx = x - origemX;
  int dy = y - origemY;
  bool deslizou = abs(dx) > 22 || abs(dy) > 22;
  int tela = telaAgora();
  if (tela == TELA_DESCANSO) {
    modoTela = TELA_MENU;
    acordar();
    painelSujo = true;
    return;
  }
  if (deslizou) {
    if (tela != TELA_WIFI) acordar();
    return;
  }
  int acao = uiToque(tela, origemX, origemY);
  if (acao >= 0) {
    modoTela = acao;
    painelSujo = true;
  }
  if (modoTela != TELA_DESCANSO && tela != TELA_WIFI) acordar();
}

static void toqueTick() {
  if (!aj.tOk) return;
  int x = 0, y = 0, z = 0;
  bool baixo = toqueLer(x, y, z);
  if (baixo) {
    if (!pressionado) {
      origemX = x;
      origemY = y;
    }
    ultimoX = x;
    ultimoY = y;
    pressionado = true;
    return;
  }
  if (!pressionado) return;
  pressionado = false;
  gesto(ultimoX, ultimoY);
}

void setup() {
  Serial.begin(115200);
  rgb.begin();
  rgb.setBrightness(40);
  rgb.clear();
  rgb.show();
  ajustesCarregar();
  uiIniciar();
  toqueIniciar();
  calibrar();
  conectar();
  webIniciar();
  dadosAgendar();
  acordar();
  painelSujo = true;
  Serial.println("painel pronto");
}

void loop() {
  webAtender();
  int vista = telaAgora();
  static int vistaAntes = -2;
  if (vista != vistaAntes) {
    dadosOlhar(vista);
    vistaAntes = vista;
  }
  dadosTick();
  toqueTick();

  int marca = (int)(millis() / 1000);
  bool novoSegundo = marca != segundo;
  if (novoSegundo) segundo = marca;

  if (WiFi.status() == WL_CONNECTED && modoTela != TELA_DESCANSO && (int32_t)(millis() - acordadoAte) >= 0) {
    modoTela = TELA_DESCANSO;
    painelSujo = true;
  }

  static uint32_t radarQuadro = 0;
  if (telaAgora() == TELA_RADAR && (int32_t)(millis() - radarQuadro) >= 90) {
    radarQuadro = millis();
    painelSujo = true;
  }

  int luz = painelLuzAtual();
  if (luz >= 0) led(luzRgb[luz][0], luzRgb[luz][1], luzRgb[luz][2]);
  else if (WiFi.status() == WL_CONNECTED) led(eNoite() ? 4 : 12, eNoite() ? 8 : 28, eNoite() ? 12 : 24);
  else led(40, 22, 0);

  if (painelSujo || novoSegundo) {
    uiDesenhar(telaAgora());
    painelSujo = false;
  }
}
