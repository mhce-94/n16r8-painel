#include "painel.h"

#include <HTTPClient.h>
#include <PNGdec.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <new>

ValTela val;
uint16_t *valFoto = nullptr;
int valFotoW = 0;
int valFotoH = 0;
uint16_t *valEmblema = nullptr;
int valEmblemaW = 0;
int valEmblemaH = 0;

static const char *TABELA_RANK = "03621f52-342b-cf4e-4f86-9350a49c6d04";
static const int JSON_CAP = 120 * 1024;
static const int PNG_CAP = 120 * 1024;

static char *jsonBuf = nullptr;
static WiFiClientSecure clienteTls;
static PNG *pngDec = nullptr;
static char fotoPronta[40];
static int emblemaPronto = -999;

static uint16_t *linhaPng = nullptr;
struct AlvoPng {
  uint16_t *px;
  int dw, dh, sw, sh;
  uint32_t fundo;
};
static AlvoPng alvo;

void valorantZerar() {
  if (valFoto) free(valFoto);
  if (valEmblema) free(valEmblema);
  valFoto = nullptr;
  valEmblema = nullptr;
  valFotoW = valFotoH = 0;
  valEmblemaW = valEmblemaH = 0;
  fotoPronta[0] = 0;
  emblemaPronto = -999;
  memset(&val, 0, sizeof(val));
  val.tier = -1;
  val.rr = -1;
  val.delta = -999;
  strncpy(val.erro, "buscando", sizeof(val.erro) - 1);
}

static void copiar(char *dest, int n, const char *src) {
  if (!src) src = "";
  strncpy(dest, src, n - 1);
  dest[n - 1] = 0;
}

static bool igualCi(const char *a, const char *b) {
  if (!a || !b) return false;
  while (*a && *b) {
    char ca = *a++;
    char cb = *b++;
    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
    if (ca != cb) return false;
  }
  return *a == 0 && *b == 0;
}

static bool prefixoCi(const char *texto, const char *prefixo) {
  while (*prefixo) {
    char ca = *texto++;
    char cb = *prefixo++;
    if (!ca) return false;
    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
    if (ca != cb) return false;
  }
  return true;
}

static bool partirRiot(const char *riot, char *nome, int nn, char *tag, int nt) {
  if (!riot) return false;
  while (*riot == ' ') riot++;
  const char *hash = strrchr(riot, '#');
  if (!hash || hash == riot || !hash[1]) return false;
  int ln = (int)(hash - riot);
  while (ln > 0 && riot[ln - 1] == ' ') ln--;
  if (ln <= 0 || ln >= nn) return false;
  memcpy(nome, riot, ln);
  nome[ln] = 0;
  const char *t = hash + 1;
  while (*t == ' ') t++;
  int k = 0;
  while (*t && *t != ' ' && k < nt - 1) tag[k++] = *t++;
  tag[k] = 0;
  return k > 0;
}

static void codificar(char *dest, int n, const char *src) {
  int u = 0;
  for (const unsigned char *p = (const unsigned char *)src; *p && u < n - 4; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
      dest[u++] = (char)c;
    } else {
      snprintf(dest + u, n - u, "%%%02X", c);
      u += 3;
    }
  }
  dest[u] = 0;
}

static const char *anteriorUtil(const char *ini, const char *p) {
  const char *q = p - 1;
  while (q >= ini && (*q == ' ' || *q == '\n' || *q == '\r' || *q == '\t')) q--;
  return q >= ini ? q : nullptr;
}

static bool valorNoNivel(const char *ini, const char *fim, int nivel, const char *chave, const char **vIni, const char **vFim) {
  int depth = 0;
  bool texto = false;
  int chaveN = (int)strlen(chave);
  const char *p = ini;
  while (p < fim && *p) {
    char c = *p;
    if (texto) {
      if (c == '\\' && p + 1 < fim) {
        p += 2;
        continue;
      }
      if (c == '"') texto = false;
      p++;
      continue;
    }
    if (c == '"') {
      const char *ant = anteriorUtil(ini, p);
      bool ehChave = ant && (*ant == '{' || *ant == ',');
      const char *s = p + 1;
      const char *e = s;
      while (e < fim && *e && *e != '"') {
        if (*e == '\\' && e + 1 < fim) e += 2;
        else e++;
      }
      if (ehChave && (nivel < 0 || depth == nivel) && (int)(e - s) == chaveN && memcmp(s, chave, chaveN) == 0) {
        const char *v = e < fim ? e + 1 : e;
        while (v < fim && (*v == ' ' || *v == '\n' || *v == '\r' || *v == '\t')) v++;
        if (v >= fim || *v != ':') {
          p = e < fim ? e + 1 : fim;
          continue;
        }
        v++;
        while (v < fim && (*v == ' ' || *v == '\n' || *v == '\r' || *v == '\t')) v++;
        *vIni = v;
        if (v >= fim) return false;
        if (*v == '"') {
          const char *q = v + 1;
          while (q < fim && *q && *q != '"') {
            if (*q == '\\' && q + 1 < fim) q += 2;
            else q++;
          }
          *vFim = q < fim ? q + 1 : q;
        } else if (*v == '{' || *v == '[') {
          char fecha = *v == '{' ? '}' : ']';
          int d = 0;
          bool st = false;
          const char *q = v;
          for (; q < fim && *q; q++) {
            if (st) {
              if (*q == '\\' && q + 1 < fim) {
                q++;
                continue;
              }
              if (*q == '"') st = false;
              continue;
            }
            if (*q == '"') {
              st = true;
              continue;
            }
            if (*q == *v) d++;
            else if (*q == fecha) {
              d--;
              if (d == 0) {
                q++;
                break;
              }
            }
          }
          *vFim = q;
        } else {
          const char *q = v;
          while (q < fim && *q && *q != ',' && *q != '}' && *q != ']') q++;
          *vFim = q;
        }
        return true;
      }
      texto = true;
      p++;
      continue;
    }
    if (c == '{' || c == '[') depth++;
    else if (c == '}' || c == ']') depth--;
    p++;
  }
  return false;
}

static bool lerStr(const char *a, const char *b, const char *chave, char *dest, int n) {
  const char *vi, *vf;
  if (!valorNoNivel(a, b, 1, chave, &vi, &vf)) return false;
  while (vi < vf && (*vi == ' ' || *vi == '\n')) vi++;
  if (vi >= vf || *vi != '"') return false;
  vi++;
  int k = 0;
  while (vi < vf && *vi != '"') {
    if (*vi == '\\' && vi + 1 < vf) vi++;
    if (k < n - 1) dest[k++] = *vi;
    vi++;
  }
  dest[k] = 0;
  return k > 0;
}

static bool lerInt(const char *a, const char *b, const char *chave, int *saida) {
  const char *vi, *vf;
  if (!valorNoNivel(a, b, 1, chave, &vi, &vf)) return false;
  while (vi < vf && (*vi == ' ' || *vi == '\n')) vi++;
  if (vi >= vf) return false;
  if (!(*vi == '-' || (*vi >= '0' && *vi <= '9'))) return false;
  *saida = atoi(vi);
  return true;
}

static bool lerBool(const char *a, const char *b, const char *chave, bool *saida) {
  const char *vi, *vf;
  if (!valorNoNivel(a, b, 1, chave, &vi, &vf)) return false;
  while (vi < vf && *vi == ' ') vi++;
  if (vi + 4 <= vf && memcmp(vi, "true", 4) == 0) {
    *saida = true;
    return true;
  }
  if (vi + 5 <= vf && memcmp(vi, "false", 5) == 0) {
    *saida = false;
    return true;
  }
  return false;
}

typedef void (*ObjFn)(const char *a, const char *b, void *user);

static void cadaObjeto(const char *ini, const char *fim, ObjFn fn, void *user) {
  const char *p = ini;
  if (p < fim && *p == '[') p++;
  while (p < fim && *p) {
    while (p < fim && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',')) p++;
    if (p >= fim || *p == ']' || *p != '{') break;
    const char *comeco = p;
    int d = 0;
    bool texto = false;
    for (; p < fim && *p; p++) {
      if (texto) {
        if (*p == '\\' && p + 1 < fim) {
          p++;
          continue;
        }
        if (*p == '"') texto = false;
        continue;
      }
      if (*p == '"') {
        texto = true;
        continue;
      }
      if (*p == '{' || *p == '[') d++;
      else if (*p == '}' || *p == ']') {
        d--;
        if (d == 0) {
          p++;
          break;
        }
      }
    }
    fn(comeco, p, user);
    yield();
  }
}

static time_t epocaUtc(int y, int mo, int d, int hh, int mm, int ss) {
  y -= mo <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  int days = (int)(era * 146097 + (int)doe - 719468);
  return (time_t)days * 86400L + hh * 3600L + mm * 60L + ss;
}

static void traduzirRank(const char *en, char *dest, int n) {
  static const char *de[] = {"Ascendant", "Immortal", "Diamond", "Platinum", "Silver", "Bronze", "Radiant", "Iron", "Gold", "Unranked"};
  static const char *para[] = {"Ascendente", "Imortal", "Diamante", "Platina", "Prata", "Bronze", "Radiante", "Ferro", "Ouro", "Sem rank"};
  for (int i = 0; i < 10; i++) {
    if (prefixoCi(en, de[i])) {
      snprintf(dest, n, "%s%s", para[i], en + strlen(de[i]));
      return;
    }
  }
  copiar(dest, n, en);
}

static void traduzirFila(const char *id, const char *nome, char *dest, int n) {
  if (igualCi(id, "competitive")) copiar(dest, n, "Competitivo");
  else if (igualCi(id, "unrated")) copiar(dest, n, "Sem rank");
  else if (igualCi(id, "swiftplay")) copiar(dest, n, "Swiftplay");
  else if (igualCi(id, "spikerush")) copiar(dest, n, "Spike Rush");
  else if (igualCi(id, "deathmatch")) copiar(dest, n, "Deathmatch");
  else if (igualCi(id, "hurm") || igualCi(id, "ggteam")) copiar(dest, n, "Mata-mata");
  else if (igualCi(id, "premier")) copiar(dest, n, "Premier");
  else if (igualCi(id, "custom")) copiar(dest, n, "Personalizada");
  else if (nome && nome[0]) copiar(dest, n, nome);
  else copiar(dest, n, id);
}

struct BuscaJogador {
  char nome[20];
  char tag[8];
  bool achou;
  char team[16];
  char agente[20];
  char agenteId[40];
  char rank[28];
  int tier;
  int kills, deaths, assists, hs, corpo, pernas, score, dano;
};

static void aoJogador(const char *a, const char *b, void *user) {
  BuscaJogador *busca = (BuscaJogador *)user;
  if (busca->achou) return;
  char nome[20], tag[8];
  if (!lerStr(a, b, "name", nome, sizeof(nome))) return;
  if (!lerStr(a, b, "tag", tag, sizeof(tag))) return;
  if (!igualCi(nome, busca->nome) || !igualCi(tag, busca->tag)) return;
  busca->achou = true;
  copiar(busca->nome, sizeof(busca->nome), nome);
  copiar(busca->tag, sizeof(busca->tag), tag);
  lerStr(a, b, "team_id", busca->team, sizeof(busca->team));
  const char *vi, *vf;
  if (valorNoNivel(a, b, 1, "agent", &vi, &vf)) {
    lerStr(vi, vf, "name", busca->agente, sizeof(busca->agente));
    lerStr(vi, vf, "id", busca->agenteId, sizeof(busca->agenteId));
  }
  if (valorNoNivel(a, b, 1, "tier", &vi, &vf)) {
    char cru[28];
    if (lerStr(vi, vf, "name", cru, sizeof(cru))) traduzirRank(cru, busca->rank, sizeof(busca->rank));
    lerInt(vi, vf, "id", &busca->tier);
  }
  if (valorNoNivel(a, b, 1, "stats", &vi, &vf)) {
    lerInt(vi, vf, "kills", &busca->kills);
    lerInt(vi, vf, "deaths", &busca->deaths);
    lerInt(vi, vf, "assists", &busca->assists);
    lerInt(vi, vf, "headshots", &busca->hs);
    lerInt(vi, vf, "bodyshots", &busca->corpo);
    lerInt(vi, vf, "legshots", &busca->pernas);
    lerInt(vi, vf, "score", &busca->score);
    const char *di, *df;
    if (valorNoNivel(vi, vf, 1, "damage", &di, &df)) lerInt(di, df, "dealt", &busca->dano);
  }
}

struct BuscaTime {
  char team[16];
  bool achou;
  bool venceu;
  int nossos;
  int deles;
};

static void aoTime(const char *a, const char *b, void *user) {
  BuscaTime *busca = (BuscaTime *)user;
  if (busca->achou) return;
  char id[16];
  if (!lerStr(a, b, "team_id", id, sizeof(id))) return;
  if (!igualCi(id, busca->team)) return;
  busca->achou = true;
  lerBool(a, b, "won", &busca->venceu);
  const char *vi, *vf;
  if (valorNoNivel(a, b, 1, "rounds", &vi, &vf)) {
    lerInt(vi, vf, "won", &busca->nossos);
    lerInt(vi, vf, "lost", &busca->deles);
  }
}

static bool cortarRounds(char *buf) {
  char *a = strstr(buf, "\"rounds\":[");
  char *b = strstr(buf, "\"rounds\": [");
  char *hit = a;
  if (!hit || (b && b < hit)) hit = b;
  if (!hit) return false;
  *hit = 0;
  return true;
}

static bool uuidOk(const char *s) {
  if (!s || strlen(s) != 36) return false;
  for (int i = 0; i < 36; i++) {
    char c = s[i];
    if (c == '-') continue;
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}

static int httpLer(const char *url, bool chave, bool cortar, uint8_t *buf, int cap) {
  strncpy(ultimaUrl, url, sizeof(ultimaUrl) - 1);
  ultimaUrl[sizeof(ultimaUrl) - 1] = 0;
  clienteTls.setInsecure();
  clienteTls.setTimeout(15);
  HTTPClient http;
  http.setReuse(false);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  if (!http.begin(clienteTls, url)) {
    ultimoHttp = -1;
    return -1;
  }
  http.setUserAgent("PainelN16R8");
  http.addHeader("Accept-Encoding", "identity");
  if (chave && aj.vkey[0]) http.addHeader("Authorization", aj.vkey);
  int codigo = http.GET();
  ultimoHttp = codigo;
  int n = 0;
  if (codigo == 200) {
    int anunciado = http.getSize();
    if (!cortar && anunciado > cap) {
      http.end();
      return -2;
    }
    WiFiClient *fluxo = http.getStreamPtr();
    uint32_t inicio = millis();
    uint32_t quieto = millis();
    while (fluxo && n < cap - 1 && millis() - inicio < 20000) {
      int tem = fluxo->available();
      if (tem > 0) {
        int pegar = tem;
        if (pegar > cap - 1 - n) pegar = cap - 1 - n;
        int leu = fluxo->readBytes((char *)buf + n, pegar);
        if (leu > 0) {
          n += leu;
          quieto = millis();
          buf[n] = 0;
          if (cortar && cortarRounds((char *)buf)) {
            n = (int)strlen((char *)buf);
            break;
          }
        }
      } else if (!http.connected() && millis() - quieto > 80) {
        break;
      } else {
        delay(1);
      }
    }
  }
  buf[n] = 0;
  http.end();
  if (codigo != 200) return codigo > 0 ? -codigo : -1;
  return n;
}

static bool garantirJson() {
  if (jsonBuf) return true;
  jsonBuf = (char *)heap_caps_malloc(JSON_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!jsonBuf) jsonBuf = (char *)malloc(JSON_CAP);
  return jsonBuf != nullptr;
}

static void aplicarMmr(ValTela *dest, const char *json) {
  const char *fim = json + strlen(json);
  const char *ci, *cf;
  if (!valorNoNivel(json, fim, -1, "current", &ci, &cf)) return;
  const char *ti, *tf;
  if (valorNoNivel(ci, cf, 1, "tier", &ti, &tf)) {
    char cru[28];
    int id = -1;
    if (lerStr(ti, tf, "name", cru, sizeof(cru))) traduzirRank(cru, dest->rank, sizeof(dest->rank));
    if (lerInt(ti, tf, "id", &id)) dest->tier = id;
  }
  int rr = -1, delta = 0;
  if (lerInt(ci, cf, "rr", &rr)) dest->rr = rr;
  if (lerInt(ci, cf, "last_change", &delta)) dest->delta = delta;
  if (dest->rank[0] || dest->tier >= 0) dest->temRank = true;
}

static void aplicarPartida(ValTela *dest, const char *json, const char *nome, const char *tag) {
  const char *fim = json + strlen(json);
  const char *mi, *mf;
  if (valorNoNivel(json, fim, -1, "metadata", &mi, &mf)) {
    const char *oi, *of;
    if (valorNoNivel(mi, mf, 1, "map", &oi, &of)) lerStr(oi, of, "name", dest->mapa, sizeof(dest->mapa));
    char idFila[24] = "", nomeFila[24] = "";
    if (valorNoNivel(mi, mf, 1, "queue", &oi, &of)) {
      lerStr(oi, of, "id", idFila, sizeof(idFila));
      lerStr(oi, of, "name", nomeFila, sizeof(nomeFila));
    }
    if (!igualCi(idFila, "competitive") && !igualCi(nomeFila, "Competitive")) return;
    traduzirFila(idFila, nomeFila, dest->fila, sizeof(dest->fila));
    char quando[32];
    if (lerStr(mi, mf, "started_at", quando, sizeof(quando))) {
      int Y, M, D, h, m, s;
      if (sscanf(quando, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) == 6) dest->quando = epocaUtc(Y, M, D, h, m, s);
    }
  }
  const char *pi, *pf;
  if (!valorNoNivel(json, fim, -1, "players", &pi, &pf)) return;
  BuscaJogador jogador;
  memset(&jogador, 0, sizeof(jogador));
  jogador.tier = -1;
  copiar(jogador.nome, sizeof(jogador.nome), nome);
  copiar(jogador.tag, sizeof(jogador.tag), tag);
  cadaObjeto(pi, pf, aoJogador, &jogador);
  if (!jogador.achou) return;
  copiar(dest->nome, sizeof(dest->nome), jogador.nome);
  copiar(dest->tag, sizeof(dest->tag), jogador.tag);
  copiar(dest->agente, sizeof(dest->agente), jogador.agente);
  copiar(dest->agenteId, sizeof(dest->agenteId), jogador.agenteId);
  dest->kills = jogador.kills;
  dest->deaths = jogador.deaths;
  dest->assists = jogador.assists;
  dest->temPartida = true;
  if (!dest->temRank && (jogador.rank[0] || jogador.tier >= 0)) {
    copiar(dest->rank, sizeof(dest->rank), jogador.rank);
    dest->tier = jogador.tier;
    dest->temRank = dest->rank[0] || dest->tier >= 0;
  }
  if (!jogador.team[0]) return;
  const char *ti, *tf;
  if (!valorNoNivel(json, fim, -1, "teams", &ti, &tf)) return;
  BuscaTime time;
  memset(&time, 0, sizeof(time));
  copiar(time.team, sizeof(time.team), jogador.team);
  cadaObjeto(ti, tf, aoTime, &time);
  if (!time.achou) return;
  dest->temPlacar = true;
  dest->venceu = time.venceu;
  dest->nossos = time.nossos;
  dest->deles = time.deles;
  int rounds = time.nossos + time.deles;
  if (rounds < 1) rounds = 1;
  int tiros = jogador.hs + jogador.corpo + jogador.pernas;
  dest->hsPct = tiros > 0 ? jogador.hs * 100 / tiros : 0;
  dest->acs = jogador.score > 800 ? jogador.score / rounds : jogador.score;
  dest->adr = jogador.dano > 0 ? jogador.dano / rounds : 0;
  if (!time.venceu && time.nossos > time.deles) dest->venceu = true;
}

static int aoPng(PNGDRAW *p) {
  if (!pngDec || alvo.sw < 1 || p->y < 0 || p->y >= alvo.sh) return 1;
  if ((p->y & 15) == 0) yield();
  pngDec->getLineAsRGB565(p, linhaPng, PNG_RGB565_LITTLE_ENDIAN, alvo.fundo);
  int n = alvo.sw;
  if (p->iWidth > 0 && p->iWidth < n) n = p->iWidth;
  memcpy(alvo.px + (size_t)p->y * alvo.sw, linhaPng, (size_t)n * sizeof(uint16_t));
  return 1;
}

static void reduzir(const uint16_t *src, int sw, int sh, uint16_t *dst, int dw, int dh) {
  for (int y = 0; y < dh; y++) {
    int y0 = (int)((int32_t)y * sh / dh);
    int y1 = (int)((int32_t)(y + 1) * sh / dh);
    if (y1 <= y0) y1 = y0 + 1;
    if (y1 > sh) y1 = sh;
    for (int x = 0; x < dw; x++) {
      int x0 = (int)((int32_t)x * sw / dw);
      int x1 = (int)((int32_t)(x + 1) * sw / dw);
      if (x1 <= x0) x1 = x0 + 1;
      if (x1 > sw) x1 = sw;
      uint32_t r = 0, g = 0, b = 0, n = 0;
      for (int yy = y0; yy < y1; yy++) {
        const uint16_t *row = src + (size_t)yy * sw;
        for (int xx = x0; xx < x1; xx++) {
          uint16_t p = row[xx];
          r += (p >> 11) & 31;
          g += (p >> 5) & 63;
          b += p & 31;
          n++;
        }
      }
      if (!n) n = 1;
      dst[(size_t)y * dw + x] = (uint16_t)(((r / n) << 11) | ((g / n) << 5) | (b / n));
    }
  }
}

static bool garantirPng() {
  if (!linhaPng) {
    linhaPng = (uint16_t *)heap_caps_malloc(176 * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!linhaPng) linhaPng = (uint16_t *)malloc(176 * sizeof(uint16_t));
  }
  if (!pngDec) {
    void *mem = heap_caps_malloc(sizeof(PNG), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!mem) mem = malloc(sizeof(PNG));
    if (!mem) return false;
    pngDec = new (mem) PNG();
  }
  return linhaPng && pngDec;
}

static uint16_t cor565(uint32_t rgb) {
  int r = (rgb >> 16) & 255;
  int g = (rgb >> 8) & 255;
  int b = rgb & 255;
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static bool decodificar(uint8_t *png, int n, int dw, int dh, uint32_t fundo, uint16_t **saida, int *sw, int *sh) {
  if (!garantirPng() || n < 32) return false;
  if (pngDec->openRAM(png, n, aoPng) != PNG_SUCCESS) return false;
  int w = pngDec->getWidth();
  int h = pngDec->getHeight();
  if (w < 1 || h < 1 || w > 176 || h > 176) {
    pngDec->close();
    return false;
  }
  uint16_t *cheio = (uint16_t *)heap_caps_malloc((size_t)w * h * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!cheio) cheio = (uint16_t *)malloc((size_t)w * h * sizeof(uint16_t));
  if (!cheio) {
    pngDec->close();
    return false;
  }
  uint16_t fundoPx = cor565(fundo);
  for (int i = 0; i < w * h; i++) cheio[i] = fundoPx;
  alvo.px = cheio;
  alvo.dw = w;
  alvo.dh = h;
  alvo.sw = w;
  alvo.sh = h;
  alvo.fundo = fundo;
  int rc = pngDec->decode(nullptr, 0);
  pngDec->close();
  if (rc != PNG_SUCCESS && rc != PNG_QUIT_EARLY) {
    free(cheio);
    return false;
  }
  uint16_t *px = cheio;
  if (w != dw || h != dh) {
    px = (uint16_t *)heap_caps_malloc((size_t)dw * dh * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!px) px = (uint16_t *)malloc((size_t)dw * dh * sizeof(uint16_t));
    if (!px) {
      free(cheio);
      return false;
    }
    reduzir(cheio, w, h, px, dw, dh);
    free(cheio);
  }
  if (*saida) free(*saida);
  *saida = px;
  *sw = dw;
  *sh = dh;
  return true;
}

static bool baixarImagem(const char *url, int lado, uint32_t fundo, uint16_t **saida, int *w, int *h) {
  uint8_t *buf = (uint8_t *)heap_caps_malloc(PNG_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) buf = (uint8_t *)malloc(PNG_CAP);
  if (!buf) return false;
  int n = httpLer(url, false, false, buf, PNG_CAP);
  bool ok = n > 32 && decodificar(buf, n, lado, lado, fundo, saida, w, h);
  free(buf);
  return ok;
}

static void atualizarImagens() {
  if (uuidOk(val.agenteId) && strcmp(fotoPronta, val.agenteId) != 0) {
    char url[240];
    snprintf(url, sizeof(url),
             "https://images.weserv.nl/?url=media.valorant-api.com%%2Fagents%%2F%s%%2Fdisplayicon.png&w=160&h=160&fit=contain&bg=1b2733&output=png",
             val.agenteId);
    if (baixarImagem(url, 80, 0x1B2733, &valFoto, &valFotoW, &valFotoH)) copiar(fotoPronta, sizeof(fotoPronta), val.agenteId);
    else {
      if (valFoto) free(valFoto);
      valFoto = nullptr;
      valFotoW = valFotoH = 0;
      fotoPronta[0] = 0;
    }
  }
  if (val.tier >= 0 && val.tier <= 27 && emblemaPronto != val.tier) {
    char url[260];
    snprintf(url, sizeof(url),
             "https://images.weserv.nl/?url=media.valorant-api.com%%2Fcompetitivetiers%%2F%s%%2F%d%%2Flargeicon.png&w=112&h=112&fit=contain&bg=0f1923&output=png",
             TABELA_RANK, val.tier);
    if (baixarImagem(url, 56, 0x0F1923, &valEmblema, &valEmblemaW, &valEmblemaH)) emblemaPronto = val.tier;
    else {
      if (valEmblema) free(valEmblema);
      valEmblema = nullptr;
      valEmblemaW = valEmblemaH = 0;
      emblemaPronto = -999;
    }
  }
}

bool valorantAtualizar() {
  if (!aj.vkey[0]) {
    valorantZerar();
    copiar(val.erro, sizeof(val.erro), "falta a chave");
    return false;
  }
  char nome[20], tag[8];
  if (!partirRiot(aj.riot, nome, sizeof(nome), tag, sizeof(tag))) {
    valorantZerar();
    copiar(val.erro, sizeof(val.erro), "use Nome#tag");
    return false;
  }
  if (!aj.vreg[0]) copiar(aj.vreg, sizeof(aj.vreg), "br");
  if (!garantirJson()) {
    copiar(val.erro, sizeof(val.erro), "sem memoria");
    return false;
  }
  char nomeU[48], tagU[24];
  codificar(nomeU, sizeof(nomeU), nome);
  codificar(tagU, sizeof(tagU), tag);
  char url[180];
  snprintf(url, sizeof(url), "https://api.henrikdev.xyz/valorant/v3/mmr/%s/pc/%s/%s", aj.vreg, nomeU, tagU);
  int nMmr = httpLer(url, true, false, (uint8_t *)jsonBuf, JSON_CAP);
  int cMmr = nMmr >= 0 ? 200 : (nMmr < -1 ? -nMmr : ultimoHttp);
  bool mmrOk = false;
  ValTela novo;
  memset(&novo, 0, sizeof(novo));
  novo.tier = -1;
  novo.rr = -1;
  novo.delta = -999;
  copiar(novo.nome, sizeof(novo.nome), nome);
  copiar(novo.tag, sizeof(novo.tag), tag);
  if (nMmr > 2) {
    aplicarMmr(&novo, jsonBuf);
    mmrOk = novo.temRank;
  }
  snprintf(url, sizeof(url), "https://api.henrikdev.xyz/valorant/v4/matches/%s/pc/%s/%s?mode=competitive&size=1", aj.vreg, nomeU, tagU);
  int nMatch = httpLer(url, true, true, (uint8_t *)jsonBuf, JSON_CAP);
  int cMatch = nMatch >= 0 ? 200 : (nMatch < -1 ? -nMatch : ultimoHttp);
  bool partidaOk = false;
  if (nMatch > 2) {
    aplicarPartida(&novo, jsonBuf, nome, tag);
    partidaOk = novo.temPartida;
  }
  if (cMmr == 401 || cMmr == 403 || cMatch == 401 || cMatch == 403) {
    valorantZerar();
    copiar(val.erro, sizeof(val.erro), "chave recusada");
    return false;
  }
  if (cMmr == 404 && cMatch == 404) {
    valorantZerar();
    copiar(val.erro, sizeof(val.erro), "conta nao encontrada");
    return false;
  }
  if (!mmrOk && !partidaOk) {
    if (!val.temRank && !val.temPartida) {
      int codigo = cMatch > 0 ? cMatch : cMmr;
      if (codigo == 429) copiar(val.erro, sizeof(val.erro), "limite da API");
      else snprintf(val.erro, sizeof(val.erro), "falha %d", codigo);
    }
    return false;
  }
  if (partidaOk) novo.erro[0] = 0;
  else if (!val.temPartida) copiar(novo.erro, sizeof(novo.erro), "sem competitiva");
  if (!mmrOk && val.temRank && !novo.temRank) {
    novo.temRank = val.temRank;
    novo.tier = val.tier;
    novo.rr = val.rr;
    novo.delta = val.delta;
    copiar(novo.rank, sizeof(novo.rank), val.rank);
  }
  if (!partidaOk && val.temPartida) {
    novo.temPartida = val.temPartida;
    novo.temPlacar = val.temPlacar;
    novo.venceu = val.venceu;
    novo.kills = val.kills;
    novo.deaths = val.deaths;
    novo.assists = val.assists;
    novo.hsPct = val.hsPct;
    novo.acs = val.acs;
    novo.adr = val.adr;
    novo.nossos = val.nossos;
    novo.deles = val.deles;
    novo.quando = val.quando;
    copiar(novo.agente, sizeof(novo.agente), val.agente);
    copiar(novo.agenteId, sizeof(novo.agenteId), val.agenteId);
    copiar(novo.mapa, sizeof(novo.mapa), val.mapa);
    copiar(novo.fila, sizeof(novo.fila), val.fila);
    copiar(novo.nome, sizeof(novo.nome), val.nome);
    copiar(novo.tag, sizeof(novo.tag), val.tag);
    novo.erro[0] = 0;
  }
  val = novo;
  atualizarImagens();
  Serial.printf("valorant %s %s %d-%d http %d\n", val.agente, val.mapa, val.nossos, val.deles, ultimoHttp);
  return val.temPartida || val.temRank;
}
