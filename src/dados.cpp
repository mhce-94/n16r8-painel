#include "painel.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <time.h>

Ajustes aj;
Cotacao cotacoes[16];
float serie[20];
uint8_t serieN = 0;
float tempC = 0;
float tempMin = -99;
float tempMax = -99;
int climaCod = 3;
int umidade = -1;
bool climaOk = false;
int ultimoHttp = 0;
char ultimaUrl[120] = "";
char frase[160] = "The only way to do great work is to love what you do.";
char fraseAutor[40] = "Steve Jobs";
DiaClima dias[6];
int diasN = 0;
char solNasce[6] = "";
char solPoe[6] = "";
VooTela voos[5];
int voosN = 0;
bool voosOk = false;
CeuTela ceu = {};
FeriadoTela feriados[5];
int feriadosN = 0;
bool feriadosOk = false;

static char *corpo = nullptr;
static WiFiClient clienteHttp;
static WiFiClientSecure clienteTls;
static int passo = -1;
static uint32_t proximoPasso = 0;
static uint32_t proximoClima = 0;
static uint32_t proximaLeva = 0;
static uint32_t proximoVal = 0;
static uint32_t proximaFrase = 0;
static uint32_t proximaIss = 0;
static uint32_t proximoVoo = 0;
static uint32_t proximoFer = 0;
static uint8_t cursorAcao = 0;

void ajustesPadrao() {
  memset(&aj, 0, sizeof(aj));
  strcpy(aj.cidade, "Curitiba");
  strcpy(aj.lat, "-25.43");
  strcpy(aj.lon, "-49.27");
  strcpy(aj.fx, "USD,EUR,GBP");
  strcpy(aj.cripto, "BTC,ETH,SOL");
  strcpy(aj.br, "PETR4,VALE3,ITUB4,WEGE3");
  strcpy(aj.eua, "AAPL,NVDA,MSFT");
  strcpy(aj.par, "USD-BRL");
  strcpy(aj.riot, "Unh0ly#420");
  strcpy(aj.vreg, "br");
  aj.telas = 0xFF;
  aj.cicloSeg = 120;
  aj.noiteOn = 1;
  aj.noite1 = 22;
  aj.noite2 = 7;
  aj.climaMin = 15;
  aj.cotacaoMin = 10;
}

static void copiarPref(Preferences &prefs, const char *chave, char *dest, int n, const char *queda) {
  String valor = prefs.getString(chave, queda);
  strncpy(dest, valor.c_str(), n - 1);
  dest[n - 1] = 0;
}

void ajustesCarregar() {
  ajustesPadrao();
  Preferences prefs;
  prefs.begin("painel", true);
  if (!prefs.getBool("ok", false)) {
    prefs.end();
    return;
  }
  copiarPref(prefs, "cidade", aj.cidade, sizeof(aj.cidade), aj.cidade);
  copiarPref(prefs, "lat", aj.lat, sizeof(aj.lat), aj.lat);
  copiarPref(prefs, "lon", aj.lon, sizeof(aj.lon), aj.lon);
  copiarPref(prefs, "fx", aj.fx, sizeof(aj.fx), aj.fx);
  copiarPref(prefs, "cripto", aj.cripto, sizeof(aj.cripto), aj.cripto);
  copiarPref(prefs, "br", aj.br, sizeof(aj.br), aj.br);
  copiarPref(prefs, "eua", aj.eua, sizeof(aj.eua), aj.eua);
  copiarPref(prefs, "par", aj.par, sizeof(aj.par), aj.par);
  copiarPref(prefs, "ssid", aj.ssid, sizeof(aj.ssid), "");
  copiarPref(prefs, "senha", aj.senha, sizeof(aj.senha), "");
  copiarPref(prefs, "riot", aj.riot, sizeof(aj.riot), "Unh0ly#420");
  copiarPref(prefs, "vreg", aj.vreg, sizeof(aj.vreg), "br");
  copiarPref(prefs, "vkey", aj.vkey, sizeof(aj.vkey), "");
  aj.telas = prefs.getUChar("telas", 0xFF);
  aj.cicloSeg = prefs.getUChar("ciclo", 12);
  aj.noiteOn = prefs.getUChar("noite", 1);
  aj.noite1 = prefs.getUChar("n1", 22);
  aj.noite2 = prefs.getUChar("n2", 7);
  aj.climaMin = prefs.getUChar("cmin", 15);
  aj.cotacaoMin = prefs.getUChar("qmin", 10);
  aj.tX0 = prefs.getInt("tx0", 0);
  aj.tX1 = prefs.getInt("tx1", 0);
  aj.tY0 = prefs.getInt("ty0", 0);
  aj.tY1 = prefs.getInt("ty1", 0);
  aj.tTroca = prefs.getUChar("tt", 0);
  aj.tOk = prefs.getUChar("tok", 0);
  uint8_t versao = prefs.getUChar("v", 0);
  if (versao < 2) aj.tOk = 0;
  if (versao < 3) aj.telas |= (1 << TELA_VAL);
  if (versao < 4) aj.cicloSeg = 120;
  if (versao < 5) aj.telas = 0xFF;
  if (aj.cicloSeg < 20) aj.cicloSeg = 20;
  if (aj.cicloSeg > 240) aj.cicloSeg = 240;
  if (!aj.telas) aj.telas = 0xFF;
  prefs.end();
}

void ajustesSalvar() {
  Preferences prefs;
  prefs.begin("painel", false);
  prefs.putBool("ok", true);
  prefs.putString("cidade", aj.cidade);
  prefs.putString("lat", aj.lat);
  prefs.putString("lon", aj.lon);
  prefs.putString("fx", aj.fx);
  prefs.putString("cripto", aj.cripto);
  prefs.putString("br", aj.br);
  prefs.putString("eua", aj.eua);
  prefs.putString("par", aj.par);
  prefs.putString("ssid", aj.ssid);
  prefs.putString("senha", aj.senha);
  prefs.putString("riot", aj.riot);
  prefs.putString("vreg", aj.vreg);
  prefs.putString("vkey", aj.vkey);
  prefs.putUChar("telas", aj.telas);
  prefs.putUChar("ciclo", aj.cicloSeg);
  prefs.putUChar("noite", aj.noiteOn);
  prefs.putUChar("n1", aj.noite1);
  prefs.putUChar("n2", aj.noite2);
  prefs.putUChar("cmin", aj.climaMin);
  prefs.putUChar("qmin", aj.cotacaoMin);
  prefs.putInt("tx0", aj.tX0);
  prefs.putInt("tx1", aj.tX1);
  prefs.putInt("ty0", aj.tY0);
  prefs.putInt("ty1", aj.tY1);
  prefs.putUChar("tt", aj.tTroca);
  prefs.putUChar("tok", aj.tOk);
  prefs.putUChar("v", 5);
  prefs.end();
}

static bool prepararCorpo() {
  if (corpo) return true;
  corpo = (char *)heap_caps_malloc(24000, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!corpo) corpo = (char *)malloc(24000);
  return corpo != nullptr;
}

static bool lerNumero(const char *texto, const char *chave, float *saida) {
  const char *p = texto;
  size_t n = strlen(chave);
  while (p && (p = strstr(p, chave)) != nullptr) {
    const char *v = p + n;
    if (*v == '_' || *v == '-' || (*v >= 'A' && *v <= 'Z') || (*v >= 'a' && *v <= 'z') || (*v >= '0' && *v <= '9')) {
      p = v;
      continue;
    }
    while (*v == '"' || *v == ':' || *v == ' ' || *v == '[') v++;
    if (*v == '-' || (*v >= '0' && *v <= '9')) {
      *saida = (float)atof(v);
      return true;
    }
    p = v;
  }
  return false;
}

static bool httpBuscar(const char *url, const char *post) {
  if (!prepararCorpo()) return false;
  strncpy(ultimaUrl, url, sizeof(ultimaUrl) - 1);
  ultimaUrl[sizeof(ultimaUrl) - 1] = 0;
  bool tls = strncmp(url, "https://", 8) == 0;
  HTTPClient http;
  http.setReuse(false);
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  bool abriu = false;
  if (tls) {
    clienteTls.setInsecure();
    abriu = http.begin(clienteTls, url);
  } else {
    abriu = http.begin(clienteHttp, url);
  }
  if (!abriu) {
    ultimoHttp = -1;
    return false;
  }
  http.setUserAgent("PainelN16R8");
  int codigo;
  if (post) {
    http.addHeader("Content-Type", "application/json");
    codigo = http.POST((uint8_t *)post, strlen(post));
  } else {
    codigo = http.GET();
  }
  ultimoHttp = codigo;
  int n = 0;
  if (codigo == 200) {
    WiFiClient *fluxo = http.getStreamPtr();
    uint32_t inicio = millis();
    uint32_t quieto = millis();
    while (fluxo && n < 23999 && millis() - inicio < 8000) {
      int tem = fluxo->available();
      if (tem > 0) {
        int pegar = tem;
        if (pegar > 23999 - n) pegar = 23999 - n;
        int leu = fluxo->readBytes(corpo + n, pegar);
        if (leu > 0) {
          n += leu;
          quieto = millis();
        }
      } else if (!http.connected() && millis() - quieto > 60) {
        break;
      } else {
        delay(1);
      }
    }
  }
  corpo[n] = 0;
  http.end();
  return codigo == 200 && n > 2;
}

int dadosDividir(const char *csv, char saida[][12], int maxN) {
  int n = 0;
  const char *p = csv ? csv : "";
  while (*p && n < maxN) {
    while (*p == ' ' || *p == ',') p++;
    if (!*p) break;
    int k = 0;
    while (*p && *p != ',' && k < 11) {
      char c = *p++;
      if (c >= 'a' && c <= 'z') c = (char)(c - 32);
      saida[n][k++] = c;
    }
    saida[n][k] = 0;
    if (k) n++;
    while (*p && *p != ',') p++;
  }
  return n;
}

void dadosBaseCripto(const char *sim, char *base, int n) {
  int k = 0;
  for (const char *p = sim; *p && k < n - 1; p++) {
    char c = *p;
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) base[k++] = c;
  }
  base[k] = 0;
  if (k > 4 && strcmp(base + k - 4, "USDT") == 0) base[k - 4] = 0;
}

static Cotacao *abrirCotacao(const char *sim) {
  Cotacao *livre = nullptr;
  for (int i = 0; i < 16; i++) {
    if (cotacoes[i].sim[0] && strcmp(cotacoes[i].sim, sim) == 0) return &cotacoes[i];
    if (!cotacoes[i].sim[0] && !livre) livre = &cotacoes[i];
  }
  if (!livre) return nullptr;
  strncpy(livre->sim, sim, 11);
  livre->sim[11] = 0;
  livre->ok = false;
  return livre;
}

const Cotacao *cotacaoDe(const char *sim) {
  for (int i = 0; i < 16; i++) {
    if (cotacoes[i].ok && strcmp(cotacoes[i].sim, sim) == 0) return &cotacoes[i];
  }
  return nullptr;
}

static void gravarPreco(const char *sim, float valor, float pct) {
  Cotacao *q = abrirCotacao(sim);
  if (!q) return;
  if (valor >= 1000) snprintf(q->preco, sizeof(q->preco), "%.0f", valor);
  else if (valor >= 100) snprintf(q->preco, sizeof(q->preco), "%.1f", valor);
  else if (valor >= 1) snprintf(q->preco, sizeof(q->preco), "%.2f", valor);
  else if (valor >= 0.01f) snprintf(q->preco, sizeof(q->preco), "%.4f", valor);
  else snprintf(q->preco, sizeof(q->preco), "%.6f", valor);
  for (char *p = q->preco; *p; p++)
    if (*p == '.') *p = ',';
  q->pct100 = (int)(pct * 100);
  q->ok = true;
}

void dadosZerar() {
  memset(cotacoes, 0, sizeof(cotacoes));
  serieN = 0;
  valorantZerar();
}

bool dadosGeo(const char *nome) {
  char url[160];
  int u = snprintf(url, sizeof(url), "http://geocoding-api.open-meteo.com/v1/search?name=");
  for (const char *p = nome; *p && u < (int)sizeof(url) - 4; p++) {
    unsigned char c = (unsigned char)*p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
      url[u++] = (char)c;
    } else if (u < (int)sizeof(url) - 4) {
      snprintf(url + u, 4, "%%%02X", c);
      u += 3;
    }
  }
  snprintf(url + u, sizeof(url) - u, "&count=1&language=pt");
  if (!httpBuscar(url, nullptr)) return false;
  float lat = 0, lon = 0;
  if (!lerNumero(corpo, "\"latitude\"", &lat)) return false;
  if (!lerNumero(corpo, "\"longitude\"", &lon)) return false;
  snprintf(aj.lat, sizeof(aj.lat), "%.4f", lat);
  snprintf(aj.lon, sizeof(aj.lon), "%.4f", lon);
  return true;
}

static int lerVetorNum(const char *ini, const char *chave, float *saida, int maxN) {
  if (!ini) return 0;
  char busca[48];
  snprintf(busca, sizeof(busca), "\"%s\"", chave);
  const char *p = strstr(ini, busca);
  if (!p) return 0;
  p = strchr(p, '[');
  if (!p) return 0;
  p++;
  int n = 0;
  while (*p && *p != ']' && n < maxN) {
    while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r') p++;
    if (*p == ']' || !*p) break;
    if (strncmp(p, "null", 4) == 0) {
      saida[n++] = -999;
      p += 4;
      continue;
    }
    if (*p == '-' || (*p >= '0' && *p <= '9')) {
      saida[n++] = (float)atof(p);
      while (*p && *p != ',' && *p != ']') p++;
      continue;
    }
    break;
  }
  return n;
}

static void horaIso(const char *iso, char *dest, int n) {
  dest[0] = 0;
  const char *t = iso ? strchr(iso, 'T') : nullptr;
  if (!t || (int)strlen(t) < 6 || n < 6) return;
  memcpy(dest, t + 1, 5);
  dest[5] = 0;
}

static void rotuloDia(const char *iso, char *dest, int n) {
  int y = 0, m = 0, d = 0;
  if (!iso || sscanf(iso, "%d-%d-%d", &y, &m, &d) != 3) {
    dest[0] = 0;
    return;
  }
  time_t agora = time(nullptr);
  struct tm hoje;
  localtime_r(&agora, &hoje);
  bool ehHoje = agora > 1700000000 && hoje.tm_year + 1900 == y && hoje.tm_mon + 1 == m && hoje.tm_mday == d;
  struct tm t = {};
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  t.tm_hour = 12;
  mktime(&t);
  static const char *sem[] = {"dom", "seg", "ter", "qua", "qui", "sex", "sab"};
  if (ehHoje) snprintf(dest, n, "hoje");
  else snprintf(dest, n, "%s %02d", sem[t.tm_wday < 0 ? 0 : t.tm_wday], d);
}

static int distanciaKm(float la1, float lo1, float la2, float lo2) {
  const float r = 6371.f;
  float p1 = la1 * 0.0174533f;
  float p2 = la2 * 0.0174533f;
  float dp = (la2 - la1) * 0.0174533f;
  float dl = (lo2 - lo1) * 0.0174533f;
  float a = sinf(dp / 2) * sinf(dp / 2) + cosf(p1) * cosf(p2) * sinf(dl / 2) * sinf(dl / 2);
  float c = 2 * atan2f(sqrtf(a), sqrtf(fmaxf(0.f, 1.f - a)));
  return (int)(r * c + 0.5f);
}

bool dadosClima() {
  char url[360];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
           "&current=temperature_2m,relative_humidity_2m,weather_code"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset"
           "&forecast_days=6&timezone=America%%2FSao_Paulo",
           aj.lat, aj.lon);
  if (!httpBuscar(url, nullptr)) {
    Serial.printf("clima http %d\n", ultimoHttp);
    return false;
  }
  float t = 0, cod = 3, hum = 0, tmin = 0, tmax = 0;
  if (!lerNumero(corpo, "\"temperature_2m\"", &t)) {
    Serial.printf("clima sem numero http %d\n", ultimoHttp);
    return false;
  }
  tempC = t;
  climaOk = true;
  if (lerNumero(corpo, "\"temperature_2m_min\"", &tmin)) tempMin = tmin;
  if (lerNumero(corpo, "\"temperature_2m_max\"", &tmax)) tempMax = tmax;
  if (lerNumero(corpo, "\"weather_code\"", &cod)) climaCod = (int)cod;
  if (lerNumero(corpo, "\"relative_humidity_2m\"", &hum)) umidade = (int)hum;
  const char *diario = strstr(corpo, "\"daily\"");
  float cods[6], maxs[6], mins[6], chuvas[6];
  char horas[6][20];
  char datas[6][12];
  int nc = lerVetorNum(diario, "weather_code", cods, 6);
  int nx = lerVetorNum(diario, "temperature_2m_max", maxs, 6);
  int nn = lerVetorNum(diario, "temperature_2m_min", mins, 6);
  int nq = lerVetorNum(diario, "precipitation_probability_max", chuvas, 6);
  diasN = nc;
  if (nx < diasN) diasN = nx;
  if (nn < diasN) diasN = nn;
  if (diasN > 6) diasN = 6;
  int ns = 0;
  if (diario) {
    const char *p = strstr(diario, "\"sunrise\"");
    if (p && (p = strchr(p, '['))) {
      p++;
      while (*p && *p != ']' && ns < 6) {
        while (*p == ' ' || *p == ',') p++;
        if (*p != '"') break;
        p++;
        int k = 0;
        while (*p && *p != '"' && k < 19) horas[ns][k++] = *p++;
        horas[ns][k] = 0;
        if (*p == '"') p++;
        ns++;
      }
    }
    p = strstr(diario, "\"time\"");
    int nd = 0;
    if (p && (p = strchr(p, '['))) {
      p++;
      while (*p && *p != ']' && nd < 6) {
        while (*p == ' ' || *p == ',') p++;
        if (*p != '"') break;
        p++;
        int k = 0;
        while (*p && *p != '"' && k < 11) datas[nd][k++] = *p++;
        datas[nd][k] = 0;
        if (*p == '"') p++;
        nd++;
      }
    }
    if (nd < diasN) diasN = nd;
  }
  solNasce[0] = solPoe[0] = 0;
  if (ns > 0) horaIso(horas[0], solNasce, sizeof(solNasce));
  if (diario) {
    const char *p = strstr(diario, "\"sunset\"");
    char poe[20] = "";
    if (p && (p = strchr(p, '[')) && (p = strchr(p, '"'))) {
      p++;
      int k = 0;
      while (*p && *p != '"' && k < 19) poe[k++] = *p++;
      poe[k] = 0;
      horaIso(poe, solPoe, sizeof(solPoe));
    }
  }
  for (int i = 0; i < diasN; i++) {
    dias[i].cod = (int)cods[i];
    dias[i].tmax = (int)(maxs[i] + (maxs[i] >= 0 ? 0.5f : -0.5f));
    dias[i].tmin = (int)(mins[i] + (mins[i] >= 0 ? 0.5f : -0.5f));
    dias[i].chuva = (i < nq && chuvas[i] > -100) ? (int)(chuvas[i] + 0.5f) : -1;
    rotuloDia(datas[i], dias[i].rotulo, sizeof(dias[i].rotulo));
  }
  Serial.printf("clima ok %.1f code %d dias %d\n", tempC, (float)climaCod, diasN);
  return true;
}

static void buscarCambio() {
  char sims[6][12];
  int n = dadosDividir(aj.fx, sims, 6);
  if (!n) return;
  char url[180];
  strcpy(url, "https://economia.awesomeapi.com.br/json/last/");
  for (int i = 0; i < n; i++) {
    if (i) strcat(url, ",");
    strncat(url, sims[i], 7);
    strcat(url, "-BRL");
  }
  if (!httpBuscar(url, nullptr)) return;
  for (int i = 0; i < n; i++) {
    char chave[20];
    snprintf(chave, sizeof(chave), "%sBRL", sims[i]);
    const char *p = strstr(corpo, chave);
    if (!p) continue;
    float bid = 0, pct = 0;
    if (!lerNumero(p, "\"bid\"", &bid)) continue;
    lerNumero(p, "\"pctChange\"", &pct);
    gravarPreco(sims[i], bid, pct);
  }
}

static void buscarCripto() {
  char sims[6][12];
  int n = dadosDividir(aj.cripto, sims, 6);
  if (!n) return;
  char carga[360];
  int usado = snprintf(carga, sizeof(carga), "{\"symbols\":{\"tickers\":[");
  char base[6][12];
  int listados = 0;
  for (int i = 0; i < n; i++) {
    dadosBaseCripto(sims[i], base[i], sizeof(base[i]));
    if (!base[i][0]) continue;
    int escreveu = snprintf(carga + usado, sizeof(carga) - usado, "%s\"BINANCE:%sUSDT\"", listados ? "," : "", base[i]);
    if (escreveu < 0 || usado + escreveu >= (int)sizeof(carga) - 40) break;
    usado += escreveu;
    listados++;
  }
  if (!listados) return;
  snprintf(carga + usado, sizeof(carga) - usado, "]},\"columns\":[\"close\",\"change\"]}");
  if (!httpBuscar("https://scanner.tradingview.com/crypto/scan", carga)) return;
  for (int i = 0; i < n; i++) {
    if (!base[i][0]) continue;
    char ticker[28];
    snprintf(ticker, sizeof(ticker), "BINANCE:%sUSDT", base[i]);
    const char *p = strstr(corpo, ticker);
    if (!p) continue;
    const char *d = strstr(p, "\"d\":[");
    if (!d) continue;
    d += 5;
    while (*d == ' ') d++;
    float px = (float)atof(d);
    const char *virgula = strchr(d, ',');
    if (!virgula) continue;
    gravarPreco(base[i], px, (float)atof(virgula + 1));
  }
}

static void buscarAcao(bool brasil) {
  char sims[6][12];
  int n = dadosDividir(brasil ? aj.br : aj.eua, sims, 6);
  if (!n) return;
  cursorAcao %= n;
  const char *sim = sims[cursorAcao];
  char url[160];
  snprintf(url, sizeof(url), "https://query1.finance.yahoo.com/v8/finance/chart/%s%s?interval=1d&range=1d", sim, brasil ? ".SA" : "");
  cursorAcao++;
  if (!httpBuscar(url, nullptr)) return;
  float px = 0, pct = 0;
  if (!lerNumero(corpo, "regularMarketPrice", &px)) return;
  lerNumero(corpo, "regularMarketChangePercent", &pct);
  gravarPreco(sim, px, pct);
}

static void buscarGrafico() {
  char par[16];
  strncpy(par, aj.par, sizeof(par) - 1);
  par[sizeof(par) - 1] = 0;
  for (char *p = par; *p; p++)
    if (*p == '/') *p = '-';
  char url[96];
  snprintf(url, sizeof(url), "https://economia.awesomeapi.com.br/json/daily/%s/15", par);
  if (!httpBuscar(url, nullptr)) return;
  float tmp[15];
  int n = 0;
  const char *p = corpo;
  while (n < 15 && (p = strstr(p, "\"bid\"")) != nullptr) {
    float v = 0;
    if (lerNumero(p, "\"bid\"", &v)) tmp[n++] = v;
    p += 5;
  }
  serieN = n;
  for (int i = 0; i < n; i++) serie[i] = tmp[n - 1 - i];
  if (!n) return;
  char sim[8];
  strncpy(sim, par, 7);
  sim[7] = 0;
  char *traco = strchr(sim, '-');
  if (traco) *traco = 0;
  float prev = n > 1 ? serie[n - 2] : serie[n - 1];
  float pct = prev ? (serie[n - 1] - prev) / prev * 100.0f : 0;
  gravarPreco(sim, serie[n - 1], pct);
}

static int passosDaLeva() {
  char tmp[6][12];
  return 3 + dadosDividir(aj.br, tmp, 6) + dadosDividir(aj.eua, tmp, 6);
}

static void rodarPasso(int indice) {
  char br[6][12];
  char eua[6][12];
  int nb = dadosDividir(aj.br, br, 6);
  int nu = dadosDividir(aj.eua, eua, 6);
  if (indice == 0) buscarCambio();
  else if (indice == 1) buscarCripto();
  else if (indice == 2) buscarGrafico();
  else if (indice < 3 + nb) {
    cursorAcao = indice - 3;
    buscarAcao(true);
  } else if (indice < 3 + nb + nu) {
    cursorAcao = indice - 3 - nb;
    buscarAcao(false);
  }
}

static uint32_t minutos(uint8_t valor) {
  if (valor < 1) valor = 1;
  if (valor > 180) valor = 180;
  return (uint32_t)valor * 60000UL;
}

static bool lerAspas(const char *texto, const char *chave, char *dest, int n) {
  char busca[16];
  snprintf(busca, sizeof(busca), "\"%s\"", chave);
  const char *p = strstr(texto, busca);
  if (!p) return false;
  p += strlen(busca);
  while (*p == ' ' || *p == ':') p++;
  if (*p != '"') return false;
  p++;
  int k = 0;
  while (*p && *p != '"' && k < n - 1) {
    unsigned char c = (unsigned char)*p;
    if (c == '\\' && p[1]) {
      p++;
      if (*p == 'n' || *p == 'r' || *p == 't') dest[k++] = ' ';
      else if (*p == 'u') {
        dest[k++] = '\'';
        for (int i = 0; i < 4 && p[1]; i++) p++;
      } else dest[k++] = *p;
      p++;
      continue;
    }
    if (c == '!' || c == '?') dest[k++] = '.';
    else if (c == ';') dest[k++] = ',';
    else if (c == 0xE2 && (unsigned char)p[1] == 0x80 && p[2]) {
      unsigned char t = (unsigned char)p[2];
      dest[k++] = (t == 0x99 || t == 0x98) ? '\'' : '-';
      p += 3;
      continue;
    } else dest[k++] = (char)c;
    p++;
  }
  while (k > 0 && dest[k - 1] == ' ') k--;
  dest[k] = 0;
  return k > 0;
}

static bool dadosFrase() {
  if (!httpBuscar("https://zenquotes.io/api/random", nullptr)) return false;
  char q[160];
  char a[40];
  if (!lerAspas(corpo, "q", q, sizeof(q)) || strlen(q) < 8) return false;
  strncpy(frase, q, sizeof(frase) - 1);
  frase[sizeof(frase) - 1] = 0;
  if (lerAspas(corpo, "a", a, sizeof(a))) {
    strncpy(fraseAutor, a, sizeof(fraseAutor) - 1);
    fraseAutor[sizeof(fraseAutor) - 1] = 0;
  }
  Serial.printf("frase %s\n", fraseAutor);
  return true;
}

static void limparEspaco(char *s) {
  int k = 0;
  for (int i = 0; s[i]; i++) {
    if (s[i] != ' ' || (k > 0 && s[k - 1] != ' ')) s[k++] = s[i];
  }
  while (k > 0 && s[k - 1] == ' ') k--;
  s[k] = 0;
}

static bool dadosOrbita() {
  ceu.ok = false;
  ceu.pessoas = 0;
  ceu.fora = 0;
  if (!httpBuscar("https://api.wheretheiss.at/v1/satellites/25544", nullptr)) return false;
  float lat = 0, lon = 0, alt = 0, vel = 0;
  if (!lerNumero(corpo, "\"latitude\"", &lat)) return false;
  if (!lerNumero(corpo, "\"longitude\"", &lon)) return false;
  lerNumero(corpo, "\"altitude\"", &alt);
  lerNumero(corpo, "\"velocity\"", &vel);
  ceu.lat = lat;
  ceu.lon = lon;
  ceu.alt = (int)(alt + 0.5f);
  ceu.vel = (int)(vel + 0.5f);
  int solo = distanciaKm((float)atof(aj.lat), (float)atof(aj.lon), lat, lon);
  float slant = sqrtf((float)solo * solo + alt * alt);
  ceu.km = (int)(slant + 0.5f);
  ceu.ok = true;
  if (!httpBuscar("https://www.howmanypeopleareinspacerightnow.com/peopleinspace.json", nullptr)) {
    Serial.printf("orbita iss %d km sem tripulacao\n", ceu.km);
    return true;
  }
  ceu.pessoas = 0;
  ceu.fora = 0;
  const char *p = corpo;
  while (p && *p) {
    const char *nome = strstr(p, "\"name\"");
    if (!nome) break;
    char pessoa[24];
    char lugar[48];
    if (!lerAspas(nome, "name", pessoa, sizeof(pessoa))) break;
    const char *loc = strstr(nome, "\"location\"");
    if (!loc || !lerAspas(loc, "location", lugar, sizeof(lugar))) break;
    bool iss = strstr(lugar, "International") != nullptr;
    if (iss && ceu.pessoas < 6) {
      strncpy(ceu.nome[ceu.pessoas], pessoa, 19);
      ceu.nome[ceu.pessoas][19] = 0;
      ceu.pessoas++;
    } else if (!iss) ceu.fora++;
    p = loc + 10;
  }
  Serial.printf("orbita %d km pessoas %d\n", ceu.km, ceu.pessoas);
  return true;
}

static bool dadosVoos() {
  float lat = (float)atof(aj.lat);
  float lon = (float)atof(aj.lon);
  if (lat == 0 && lon == 0) {
    lat = -25.43f;
    lon = -49.27f;
  }
  char url[200];
  snprintf(url, sizeof(url),
           "https://opensky-network.org/api/states/all?lamin=%.2f&lomin=%.2f&lamax=%.2f&lomax=%.2f",
           lat - 0.55f, lon - 0.55f, lat + 0.55f, lon + 0.55f);
  voosN = 0;
  if (!httpBuscar(url, nullptr)) {
    voosOk = false;
    return false;
  }
  const char *p = strstr(corpo, "\"states\"");
  voosOk = true;
  if (!p || !(p = strchr(p, '['))) return true;
  p++;
  VooTela tmp[8];
  int n = 0;
  while (n < 8 && p && *p && (p = strchr(p, '['))) {
    p++;
    char icao[12] = "";
    char call[16] = "";
    char pais[20] = "";
    float lonv = 0, latv = 0, alt = -1, vel = -1, proa = -1;
    bool solo = false;
    bool temPos = false;
    for (int f = 0; f < 11 && *p && *p != ']'; f++) {
      while (*p == ' ' || *p == ',') p++;
      if (strncmp(p, "null", 4) == 0) {
        p += 4;
        continue;
      }
      if (*p == '"') {
        p++;
        char buf[20];
        int k = 0;
        while (*p && *p != '"' && k < 19) buf[k++] = *p++;
        buf[k] = 0;
        if (*p == '"') p++;
        if (f == 0) strncpy(icao, buf, 11);
        if (f == 1) strncpy(call, buf, 15);
        if (f == 2) strncpy(pais, buf, 18);
      } else if (*p == 't' || *p == 'f') {
        if (f == 8) solo = *p == 't';
        while (*p && *p != ',' && *p != ']') p++;
      } else {
        float v = (float)atof(p);
        if (f == 5) lonv = v;
        if (f == 6) {
          latv = v;
          temPos = true;
        }
        if (f == 7) alt = v;
        if (f == 9) vel = v;
        if (f == 10) proa = v;
        while (*p && *p != ',' && *p != ']') p++;
      }
    }
    limparEspaco(call);
    if (!solo && temPos && n < 8) {
      const char *nome = call[0] ? call : icao;
      strncpy(tmp[n].nome, nome, 11);
      tmp[n].nome[11] = 0;
      if (strcmp(pais, "Brazil") == 0) strncpy(tmp[n].pais, "Brasil", 15);
      else {
        strncpy(tmp[n].pais, pais, 15);
        tmp[n].pais[15] = 0;
      }
      tmp[n].alt = alt > 0 ? (int)alt : 0;
      tmp[n].km = distanciaKm(lat, lon, latv, lonv);
      float la1 = lat * 0.0174533f;
      float la2 = latv * 0.0174533f;
      float dlon = (lonv - lon) * 0.0174533f;
      float norte = cosf(la1) * sinf(la2) - sinf(la1) * cosf(la2) * cosf(dlon);
      float leste = sinf(dlon) * cosf(la2);
      float graus = atan2f(leste, norte) * 57.2958f;
      if (graus < 0) graus += 360.f;
      tmp[n].rumo = (int)(graus + 0.5f) % 360;
      tmp[n].proa = proa >= 0 ? (int)(proa + 0.5f) % 360 : -1;
      tmp[n].vel = vel >= 0 ? (int)(vel * 3.6f + 0.5f) : 0;
      n++;
    }
    const char *fim = strchr(p, ']');
    p = fim ? fim + 1 : nullptr;
  }
  for (int i = 1; i < n; i++) {
    VooTela chave = tmp[i];
    int j = i;
    while (j > 0 && tmp[j - 1].km > chave.km) {
      tmp[j] = tmp[j - 1];
      j--;
    }
    tmp[j] = chave;
  }
  voosN = n > 5 ? 5 : n;
  for (int i = 0; i < voosN; i++) voos[i] = tmp[i];
  Serial.printf("voos %d\n", voosN);
  return true;
}

static int diasAte(int y, int m, int d) {
  time_t agora = time(nullptr);
  if (agora < 1700000000) return -1;
  struct tm hoje;
  localtime_r(&agora, &hoje);
  struct tm alvo = hoje;
  alvo.tm_year = y - 1900;
  alvo.tm_mon = m - 1;
  alvo.tm_mday = d;
  alvo.tm_hour = 12;
  alvo.tm_min = 0;
  alvo.tm_sec = 0;
  hoje.tm_hour = 12;
  hoje.tm_min = 0;
  hoje.tm_sec = 0;
  time_t a = mktime(&alvo);
  time_t b = mktime(&hoje);
  if (a < 0 || b < 0) return -1;
  return (int)((a - b) / 86400);
}

static bool buscarFeriadosAno(int ano) {
  char url[80];
  snprintf(url, sizeof(url), "https://brasilapi.com.br/api/feriados/v1/%d", ano);
  if (!httpBuscar(url, nullptr)) return false;
  const char *p = corpo;
  while (feriadosN < 5 && (p = strstr(p, "\"date\""))) {
    char iso[16];
    char nome[40];
    if (!lerAspas(p, "date", iso, sizeof(iso))) break;
    if (!lerAspas(p, "name", nome, sizeof(nome))) break;
    int y = 0, m = 0, d = 0;
    if (sscanf(iso, "%d-%d-%d", &y, &m, &d) == 3) {
      int falta = diasAte(y, m, d);
      if (falta >= 0) {
        static const char *sem[] = {"domingo", "segunda", "terça", "quarta", "quinta", "sexta", "sábado"};
        struct tm t = {};
        t.tm_year = y - 1900;
        t.tm_mon = m - 1;
        t.tm_mday = d;
        t.tm_hour = 12;
        mktime(&t);
        int w = t.tm_wday;
        if (w < 0 || w > 6) w = 0;
        snprintf(feriados[feriadosN].quando, sizeof(feriados[feriadosN].quando), "%02d/%02d %s", d, m, sem[w]);
        strncpy(feriados[feriadosN].nome, nome, sizeof(feriados[feriadosN].nome) - 1);
        feriados[feriadosN].nome[sizeof(feriados[feriadosN].nome) - 1] = 0;
        feriados[feriadosN].dias = falta;
        feriadosN++;
      }
    }
    p += 6;
  }
  return true;
}

static bool dadosFeriados() {
  time_t agora = time(nullptr);
  struct tm hoje;
  localtime_r(&agora, &hoje);
  int ano = agora > 1700000000 ? hoje.tm_year + 1900 : 2026;
  feriadosN = 0;
  if (!buscarFeriadosAno(ano)) {
    feriadosOk = feriadosN > 0;
    return feriadosOk;
  }
  if (!feriadosN) buscarFeriadosAno(ano + 1);
  feriadosOk = feriadosN > 0;
  Serial.printf("feriados %d\n", feriadosN);
  return feriadosOk;
}

void dadosAgendar() {
  proximoClima = millis() + 2000;
  proximaLeva = millis() + 4000;
  proximoVal = millis() + 8000;
  proximaFrase = millis() + 12000;
  proximaIss = millis() + 18000;
  proximoFer = millis() + 24000;
  proximoVoo = millis() + 32000;
  passo = -1;
}

void dadosOlhar(int tela) {
  if (tela == TELA_ORBITA) proximaIss = millis();
  else if (tela == TELA_RADAR) proximoVoo = millis();
  else if (tela == TELA_FERIADO) proximoFer = millis();
}

void dadosTick() {
  if (WiFi.status() != WL_CONNECTED) return;
  if ((int32_t)(millis() - proximoClima) > 0) {
    if (dadosClima()) proximoClima = millis() + minutos(aj.climaMin);
    else proximoClima = millis() + 60000;
  }
  if ((int32_t)(millis() - proximoVal) > 0) {
    bool ok = valorantAtualizar();
    proximoVal = millis() + (ok ? 180000UL : 45000UL);
    painelSujo = true;
  }
  if ((int32_t)(millis() - proximaFrase) > 0) {
    if (dadosFrase()) proximaFrase = millis() + 7200000UL;
    else proximaFrase = millis() + 900000UL;
    painelSujo = true;
  } else if ((int32_t)(millis() - proximaIss) > 0) {
    dadosOrbita();
    proximaIss = millis() + (ceu.ok ? 45000UL : 60000UL);
    painelSujo = true;
  } else if ((int32_t)(millis() - proximoFer) > 0) {
    dadosFeriados();
    proximoFer = millis() + (feriadosOk ? 43200000UL : 900000UL);
    painelSujo = true;
  } else if ((int32_t)(millis() - proximoVoo) > 0) {
    dadosVoos();
    proximoVoo = millis() + (voosOk ? 90000UL : 60000UL);
    painelSujo = true;
  }
  if (eNoite()) {
    passo = -1;
    return;
  }
  if (passo < 0 && (int32_t)(millis() - proximaLeva) > 0) {
    passo = 0;
    proximoPasso = millis();
  }
  if (passo >= 0 && (int32_t)(millis() - proximoPasso) > 0) {
    int total = passosDaLeva();
    if (total < 1) total = 1;
    if (passo < total) rodarPasso(passo);
    passo++;
    if (passo >= total) {
      passo = -1;
      proximaLeva = millis() + minutos(aj.cotacaoMin);
    } else {
      proximoPasso = millis() + 1200;
    }
  }
}

bool horaCerta(int *h, int *m, int *s) {
  time_t agora = time(nullptr);
  if (agora < 1700000000) return false;
  struct tm partes;
  localtime_r(&agora, &partes);
  if (h) *h = partes.tm_hour;
  if (m) *m = partes.tm_min;
  if (s) *s = partes.tm_sec;
  return true;
}

bool eNoite() {
  if (!aj.noiteOn) return false;
  int h = 0, m = 0;
  if (!horaCerta(&h, &m, nullptr)) return false;
  if (aj.noite1 == aj.noite2) return false;
  if (aj.noite1 > aj.noite2) return h >= aj.noite1 || h < aj.noite2;
  return h >= aj.noite1 && h < aj.noite2;
}

void relogioTexto(char *saida, int n) {
  int h = 0, m = 0;
  if (!horaCerta(&h, &m, nullptr)) snprintf(saida, n, "--:--");
  else snprintf(saida, n, "%02d:%02d", h, m);
}

void dataTexto(char *saida, int n) {
  time_t agora = time(nullptr);
  if (agora < 1700000000) {
    snprintf(saida, n, "--");
    return;
  }
  struct tm partes;
  localtime_r(&agora, &partes);
  static const char *semana[] = {"dom", "seg", "ter", "qua", "qui", "sex", "sab"};
  snprintf(saida, n, "%s %02d/%02d", semana[partes.tm_wday], partes.tm_mday, partes.tm_mon + 1);
}

const char *nomeClimaCod(int c) {
  if (c == 0) return "Céu limpo";
  if (c == 1) return "Quase limpo";
  if (c == 2) return "Parcial";
  if (c == 3) return "Nublado";
  if (c == 45 || c == 48) return "Neblina";
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return "Chuva";
  if (c >= 71 && c <= 77) return "Neve";
  if (c >= 95) return "Tempestade";
  return "Nublado";
}

const char *nomeClima() { return nomeClimaCod(climaCod); }

static void anotarProva(ProvaApi *item, const char *nome, bool ok, const char *detalhe) {
  strncpy(item->nome, nome, sizeof(item->nome) - 1);
  item->nome[sizeof(item->nome) - 1] = 0;
  item->ok = ok;
  item->codigo = ultimoHttp;
  strncpy(item->detalhe, detalhe, sizeof(item->detalhe) - 1);
  item->detalhe[sizeof(item->detalhe) - 1] = 0;
}

static void detalhePreco(const char *sim, char *saida, int n) {
  const Cotacao *q = cotacaoDe(sim);
  if (q && q->ok) snprintf(saida, n, "%s %s", sim, q->preco);
  else snprintf(saida, n, "codigo %d", ultimoHttp);
}

int dadosTestar(ProvaApi *saida, int max) {
  dadosZerar();
  int n = 0;
  char det[48];
  if (n < max) {
    bool ok = dadosGeo(aj.cidade[0] ? aj.cidade : "Curitiba");
    if (ok) snprintf(det, sizeof(det), "%s %s", aj.lat, aj.lon);
    else snprintf(det, sizeof(det), "codigo %d", ultimoHttp);
    anotarProva(&saida[n++], "Cidade", ok, det);
  }
  if (n < max) {
    bool ok = dadosClima();
    if (ok) snprintf(det, sizeof(det), "%.0f %s", tempC, nomeClima());
    else snprintf(det, sizeof(det), "codigo %d", ultimoHttp);
    anotarProva(&saida[n++], "Clima", ok, det);
  }
  if (n < max) {
    buscarCambio();
    char sims[6][12];
    int qn = dadosDividir(aj.fx, sims, 1);
    const char *sim = qn ? sims[0] : "USD";
    const Cotacao *q = cotacaoDe(sim);
    detalhePreco(sim, det, sizeof(det));
    anotarProva(&saida[n++], "Cambio", q && q->ok, det);
  }
  if (n < max) {
    buscarGrafico();
    if (serieN >= 2) snprintf(det, sizeof(det), "%d pontos", serieN);
    else snprintf(det, sizeof(det), "codigo %d", ultimoHttp);
    anotarProva(&saida[n++], "Grafico", serieN >= 2, det);
  }
  if (n < max) {
    buscarCripto();
    char sims[6][12];
    char base[12] = "BTC";
    if (dadosDividir(aj.cripto, sims, 1)) dadosBaseCripto(sims[0], base, sizeof(base));
    const Cotacao *q = cotacaoDe(base);
    detalhePreco(base, det, sizeof(det));
    anotarProva(&saida[n++], "Cripto", q && q->ok, det);
  }
  if (n < max) {
    cursorAcao = 0;
    buscarAcao(true);
    char sims[6][12];
    const char *sim = dadosDividir(aj.br, sims, 1) ? sims[0] : "PETR4";
    const Cotacao *q = cotacaoDe(sim);
    detalhePreco(sim, det, sizeof(det));
    anotarProva(&saida[n++], "Acao BR", q && q->ok, det);
  }
  if (n < max) {
    cursorAcao = 0;
    buscarAcao(false);
    char sims[6][12];
    const char *sim = dadosDividir(aj.eua, sims, 1) ? sims[0] : "AAPL";
    const Cotacao *q = cotacaoDe(sim);
    detalhePreco(sim, det, sizeof(det));
    anotarProva(&saida[n++], "Acao EUA", q && q->ok, det);
  }
  if (n < max) {
    bool ok = valorantAtualizar();
    if (val.temPartida) snprintf(det, sizeof(det), "%s %d-%d", val.agente, val.nossos, val.deles);
    else snprintf(det, sizeof(det), "%s", val.erro[0] ? val.erro : "falhou");
    anotarProva(&saida[n++], "Valorant", ok && val.temPartida, det);
  }
  return n;
}
