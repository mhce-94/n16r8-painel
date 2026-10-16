#include "painel.h"

#include <WebServer.h>
#include <WiFi.h>

static WebServer servidor(80);

static String html(const String &entrada) {
  String saida;
  saida.reserve(entrada.length() + 8);
  for (unsigned i = 0; i < entrada.length(); i++) {
    char c = entrada[i];
    if (c == '&') saida += F("&amp;");
    else if (c == '<') saida += F("&lt;");
    else if (c == '"') saida += F("&quot;");
    else saida += c;
  }
  return saida;
}

static void cabeca(String &pagina, const char *sub) {
  pagina.reserve(16000);
  pagina += F("<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>");
  pagina += F("<title>Painel</title><style>");
  pagina += F("body{margin:0;background:#070a10;color:#f4f7fb;font:16px/1.4 Segoe UI,sans-serif}");
  pagina += F("main{max-width:1040px;margin:0 auto;padding:22px 18px 48px}");
  pagina += F("h1{font-size:28px;margin:0}h1 b{color:#f5c518}");
  pagina += F(".sub{color:#96a4b4;margin:4px 0 0}");
  pagina += F(".topo{display:flex;justify-content:space-between;align-items:flex-end;gap:16px;margin-bottom:14px}");
  pagina += F(".card{background:#161e2c;border-radius:16px;padding:14px}");
  pagina += F("h2{font-size:12px;letter-spacing:.14em;text-transform:uppercase;color:#f5c518;margin:0 0 10px}");
  pagina += F(".grade{display:grid;grid-template-columns:300px 1fr;gap:14px}");
  pagina += F(".cols{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-top:14px}");
  pagina += F(".pills{display:flex;flex-wrap:wrap;gap:8px}");
  pagina += F(".pill{display:flex;gap:8px;align-items:center;background:#0b1018;border-radius:999px;padding:8px 12px}");
  pagina += F(".lista{display:grid;grid-template-columns:1fr 1fr;gap:0 14px}");
  pagina += F("input[type=text],input[type=password],input[type=number]{width:100%;box-sizing:border-box;background:#0b1018;color:#fff;border:0;border-radius:10px;padding:11px;font:inherit;margin:2px 0 10px}");
  pagina += F(".horas{display:flex;gap:8px;align-items:center}.horas input{width:76px;text-align:center}");
  pagina += F("button,.btn{background:#f5c518;color:#141414;font-weight:750;border:0;border-radius:12px;padding:11px 16px;font:inherit;cursor:pointer;text-decoration:none;display:inline-block}");
  pagina += F(".ghost{background:#2a3544;color:#f4f7fb}");
  pagina += F(".acoes{display:flex;flex-wrap:wrap;gap:10px;margin-top:14px;align-items:center}");
  pagina += F("a.link{color:#79d4ff;font-weight:700;text-decoration:none}");
  pagina += F(".hint{color:#96a4b4;font-size:13px;margin:0 0 8px}");
  pagina += F(".provas{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-top:12px}");
  pagina += F(".prova{background:#0b1018;border-radius:12px;padding:10px 12px}");
  pagina += F(".ok{color:#3dcc86;font-weight:750}.ruim{color:#ff5d6c;font-weight:750}");
  pagina += F("@media(max-width:860px){.grade,.cols,.lista,.provas{grid-template-columns:1fr}}");
  pagina += F("</style><main><div class=topo><div><h1>Painel<b> TV</b></h1><p class=sub>");
  pagina += sub;
  pagina += F("</p></div></div>");
}

static void campo(String &pagina, const char *dica, const char *nome, const char *valor) {
  pagina += F("<p class=hint>");
  pagina += dica;
  pagina += F("</p><input type=text name=");
  pagina += nome;
  pagina += F(" value=\"");
  pagina += html(valor);
  pagina += F("\">");
}

static void paginaPainel(const ProvaApi *provas, int nprovas) {
  String pagina;
  String sub = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  if (climaOk) {
    sub += " · ";
    sub += String(tempC, 0);
    sub += "° ";
    sub += nomeClima();
  }
  cabeca(pagina, sub.c_str());
  pagina += F("<div class=card><div class=topo><div><h2>Conexões</h2>");
  pagina += F("<p class=hint>Testa da placa cada serviço que ela usa. Leva alguns segundos.</p></div>");
  pagina += F("<a class=btn href=/teste>Testar APIs</a></div>");
  if (nprovas > 0) {
    pagina += F("<div class=provas>");
    for (int i = 0; i < nprovas; i++) {
      pagina += F("<div class=prova><b class=");
      pagina += provas[i].ok ? F("ok>ok") : F("ruim>falhou");
      pagina += F("</b><div>");
      pagina += html(provas[i].nome);
      pagina += F("</div><div class=hint>");
      pagina += html(provas[i].detalhe);
      pagina += F("</div></div>");
    }
    pagina += F("</div>");
  }
  pagina += F("</div><form method=POST action=/salvar><div class=grade>");
  pagina += F("<div class=card><h2>Apps no menu</h2><div class=pills>");
  static const char *nomes[] = {"Clima", "Cotações", "Lua", "Órbita", "Radar", "Feriados", "Valorant", "Luz"};
  for (int i = 0; i < 8; i++) {
    pagina += F("<label class=pill><input type=checkbox name=t value=");
    pagina += i;
    if (aj.telas & (1 << i)) pagina += F(" checked");
    pagina += F("> ");
    pagina += nomes[i];
    pagina += F("</label>");
  }
  pagina += F("</div><p class=hint>Segundos parado até o descanso</p><input type=number name=ciclo min=20 max=240 value='");
  pagina += aj.cicloSeg;
  pagina += F("'></div><div class=card><h2>Listas</h2><div class=lista>");
  campo(pagina, "Cidade do clima", "cidade", aj.cidade);
  campo(pagina, "Câmbio em real", "fx", aj.fx);
  campo(pagina, "Criptos", "cripto", aj.cripto);
  campo(pagina, "Ações BR", "br", aj.br);
  campo(pagina, "Ações EUA", "eua", aj.eua);
  campo(pagina, "Gráfico", "par", aj.par);
  campo(pagina, "Riot ID (Nome#tag)", "riot", aj.riot);
  campo(pagina, "Região: br, na, eu, ap, kr ou latam", "vreg", aj.vreg);
  campo(pagina, "Chave HenrikDev", "vkey", aj.vkey);
  pagina += F("</div></div></div><div class=cols><div class=card><h2>Noite</h2>");
  pagina += F("<label class=pill><input type=checkbox name=noite value=1");
  if (aj.noiteOn) pagina += F(" checked");
  pagina += F("> Descanso escuro nesse horário</label>");
  pagina += F("<div class=horas>Das <input type=number name=n1 min=0 max=23 value='");
  pagina += aj.noite1;
  pagina += F("'> às <input type=number name=n2 min=0 max=23 value='");
  pagina += aj.noite2;
  pagina += F("'></div></div><div class=card><h2>Atualização</h2>");
  pagina += F("<div class=horas>Clima <input type=number name=cmin min=1 max=180 value='");
  pagina += aj.climaMin;
  pagina += F("'> min</div><div class=horas>Cotações <input type=number name=qmin min=1 max=180 value='");
  pagina += aj.cotacaoMin;
  pagina += F("'> min</div></div></div>");
  pagina += F("<div class=acoes><button>Salvar</button>");
  pagina += F("<a class='btn ghost' href=/wifi>Trocar rede</a></div></form>");
  pagina += F("<form method=POST action=/calibrar class=acoes><button class=ghost>Calibrar a caneta</button></form>");
  pagina += F("</main>");
  servidor.send(200, "text/html", pagina);
}

static void handlePainel() { paginaPainel(nullptr, 0); }

static void handleTeste() {
  if (WiFi.status() != WL_CONNECTED) {
    servidor.send(200, "text/html", "<!doctype html><meta charset=utf-8><p>A placa ainda não está na rede de casa.</p><p><a href='/'>Voltar</a></p>");
    return;
  }
  ProvaApi provas[8];
  int n = dadosTestar(provas, 8);
  painelSujo = true;
  paginaPainel(provas, n);
}

static void copiarLista(char *dest, int n, const String &valor, const char *queda) {
  String v = valor;
  v.trim();
  v.toUpperCase();
  v.replace(" ", "");
  if (!v.length()) v = queda;
  strncpy(dest, v.c_str(), n - 1);
  dest[n - 1] = 0;
}

static void handleSalvar() {
  String cidade = servidor.arg("cidade");
  cidade.trim();
  if (cidade.length()) {
    strncpy(aj.cidade, cidade.c_str(), sizeof(aj.cidade) - 1);
    aj.cidade[sizeof(aj.cidade) - 1] = 0;
    dadosGeo(aj.cidade);
  }
  copiarLista(aj.fx, sizeof(aj.fx), servidor.arg("fx"), "USD,EUR,GBP");
  copiarLista(aj.cripto, sizeof(aj.cripto), servidor.arg("cripto"), "BTC,ETH,SOL");
  copiarLista(aj.br, sizeof(aj.br), servidor.arg("br"), "PETR4,VALE3");
  copiarLista(aj.eua, sizeof(aj.eua), servidor.arg("eua"), "AAPL,NVDA,MSFT");
  copiarLista(aj.par, sizeof(aj.par), servidor.arg("par"), "USD-BRL");
  uint8_t telas = 0;
  for (int i = 0; i < servidor.args(); i++) {
    if (servidor.argName(i) == "t") {
      int id = servidor.arg(i).toInt();
      if (id >= 0 && id < 8) telas |= (1 << id);
    }
  }
  if (telas) aj.telas = telas;
  int ciclo = servidor.arg("ciclo").toInt();
  if (ciclo >= 20 && ciclo <= 240) aj.cicloSeg = (uint8_t)ciclo;
  aj.noiteOn = servidor.hasArg("noite") ? 1 : 0;
  int n1 = servidor.arg("n1").toInt();
  int n2 = servidor.arg("n2").toInt();
  if (n1 >= 0 && n1 <= 23) aj.noite1 = (uint8_t)n1;
  if (n2 >= 0 && n2 <= 23) aj.noite2 = (uint8_t)n2;
  int cmin = servidor.arg("cmin").toInt();
  int qmin = servidor.arg("qmin").toInt();
  if (cmin >= 1 && cmin <= 180) aj.climaMin = (uint8_t)cmin;
  if (qmin >= 1 && qmin <= 180) aj.cotacaoMin = (uint8_t)qmin;
  String riot = servidor.arg("riot");
  riot.trim();
  if (riot.indexOf('#') < 1) riot = "Unh0ly#420";
  strncpy(aj.riot, riot.c_str(), sizeof(aj.riot) - 1);
  aj.riot[sizeof(aj.riot) - 1] = 0;
  String regiao = servidor.arg("vreg");
  regiao.trim();
  regiao.toLowerCase();
  if (regiao != "br" && regiao != "na" && regiao != "eu" && regiao != "ap" && regiao != "kr" && regiao != "latam")
    regiao = "br";
  strncpy(aj.vreg, regiao.c_str(), sizeof(aj.vreg) - 1);
  aj.vreg[sizeof(aj.vreg) - 1] = 0;
  String chave = servidor.arg("vkey");
  chave.trim();
  strncpy(aj.vkey, chave.c_str(), sizeof(aj.vkey) - 1);
  aj.vkey[sizeof(aj.vkey) - 1] = 0;
  ajustesSalvar();
  dadosZerar();
  dadosAgendar();
  painelSujo = true;
  servidor.sendHeader("Location", "/", true);
  servidor.send(303, "text/plain", "Salvo");
}

static void handleWifi() {
  int count = WiFi.scanNetworks();
  String pagina;
  cabeca(pagina, "Redes 2,4 GHz");
  pagina += F("<p class=hint>Escolha a rede e escreva a senha.</p>");
  int mostradas = 0;
  for (int i = 0; i < count; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length() || ssid == "Painel-N16R8") continue;
    String seguro = html(ssid);
    pagina += F("<form class=card method=POST action=/wifi><b>");
    pagina += seguro;
    pagina += F("</b><span class=hint> ");
    pagina += WiFi.RSSI(i);
    pagina += F(" dBm</span><input type=hidden name=ssid value=\"");
    pagina += seguro;
    pagina += F("\"><input type=password name=pass placeholder=Senha><button>Conectar</button></form>");
    mostradas++;
  }
  if (!mostradas) pagina += F("<p class=hint>Nenhuma rede encontrada.</p>");
  pagina += F("<p><a href='/'>Painel</a></p></main>");
  servidor.send(200, "text/html", pagina);
  WiFi.scanDelete();
}

static void handleWifiSalvar() {
  String ssid = servidor.arg("ssid");
  String senha = servidor.arg("pass");
  if (!ssid.length() || ssid.length() > 32 || senha.length() > 64) {
    servidor.send(400, "text/plain", "Rede invalida");
    return;
  }
  strncpy(aj.ssid, ssid.c_str(), sizeof(aj.ssid) - 1);
  strncpy(aj.senha, senha.c_str(), sizeof(aj.senha) - 1);
  aj.ssid[sizeof(aj.ssid) - 1] = 0;
  aj.senha[sizeof(aj.senha) - 1] = 0;
  ajustesSalvar();
  servidor.send(200, "text/html", "<!doctype html><meta charset=utf-8><p>Conectando. A tela mostra o novo endereço.</p>");
  delay(300);
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(aj.ssid, aj.senha);
  delay(200);
  ESP.restart();
}

static void handleStatus() {
  char buf[240];
  snprintf(buf, sizeof(buf), "clima %s %.1f %s\nhttp %d\nurl %s\n", climaOk ? "ok" : "sem", tempC, nomeClima(), ultimoHttp, ultimaUrl);
  servidor.send(200, "text/plain", buf);
}

static void handleCalibrar() {
  aj.tOk = 0;
  ajustesSalvar();
  servidor.send(200, "text/html", "<!doctype html><meta charset=utf-8><p>Toque as cruzes na tela.</p>");
  delay(300);
  ESP.restart();
}

void webIniciar() {
  servidor.on("/", HTTP_GET, []() {
    if (WiFi.status() == WL_CONNECTED) handlePainel();
    else handleWifi();
  });
  servidor.on("/salvar", HTTP_POST, handleSalvar);
  servidor.on("/wifi", HTTP_GET, handleWifi);
  servidor.on("/wifi", HTTP_POST, handleWifiSalvar);
  servidor.on("/teste", HTTP_GET, handleTeste);
  servidor.on("/status", HTTP_GET, handleStatus);
  servidor.on("/calibrar", HTTP_POST, handleCalibrar);
  servidor.begin();
}

void webAtender() { servidor.handleClient(); }
