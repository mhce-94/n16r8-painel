#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

static const int W = 240;
static const int H = 320;
static const int CX = 120;
static const int CY = 150;
static const int RADAR = 74;

static const uint16_t BG = 0x0841;

TFT_eSPI tft;
TFT_eSprite frame(&tft);
Adafruit_NeoPixel rgb(1, 48, NEO_GRB + NEO_KHZ800);

struct Net {
  char ssid[18];
  int rssi;
};

static Net nets[4];
static int netCount = 0;
static bool scanning = false;
static uint32_t lastScan = 0;
static bool spriteOk = false;
static uint32_t frames = 0;
static uint32_t fpsStamp = 0;
static int fps = 0;

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static void rememberNetworks(int found) {
  netCount = 0;
  if (found <= 0) return;
  int idx[16];
  int n = found < 16 ? found : 16;
  for (int i = 0; i < n; i++) idx[i] = i;
  for (int i = 0; i < n; i++) {
    for (int j = i + 1; j < n; j++) {
      if (WiFi.RSSI(idx[j]) > WiFi.RSSI(idx[i])) {
        int tmp = idx[i];
        idx[i] = idx[j];
        idx[j] = tmp;
      }
    }
  }
  int keep = n < 4 ? n : 4;
  for (int i = 0; i < keep; i++) {
    String name = WiFi.SSID(idx[i]);
    if (name.length() == 0) name = "(oculta)";
    name.toCharArray(nets[i].ssid, sizeof(nets[i].ssid));
    nets[i].rssi = WiFi.RSSI(idx[i]);
    netCount++;
  }
}

static void pollWifi() {
  if (!scanning && millis() - lastScan > 7000) {
    WiFi.scanNetworks(true);
    scanning = true;
    lastScan = millis();
  }
  int found = WiFi.scanComplete();
  if (found >= 0) {
    rememberNetworks(found);
    WiFi.scanDelete();
    scanning = false;
  }
}

static void drawRadar(float sweep) {
  uint16_t dim = rgb565(28, 64, 96);
  uint16_t line = rgb565(120, 230, 255);
  frame.drawCircle(CX, CY, RADAR, dim);
  frame.drawCircle(CX, CY, RADAR * 2 / 3, dim);
  frame.drawCircle(CX, CY, RADAR / 3, dim);
  frame.drawLine(CX - RADAR, CY, CX + RADAR, CY, dim);
  frame.drawLine(CX, CY - RADAR, CX, CY + RADAR, dim);

  for (int i = 12; i >= 0; i--) {
    float a = sweep - i * 0.09f;
    uint8_t glow = (uint8_t)(255 - i * 18);
    uint16_t color = rgb565(glow / 5, glow, glow);
    int x2 = CX + (int)(cosf(a) * RADAR);
    int y2 = CY + (int)(sinf(a) * RADAR);
    frame.drawLine(CX, CY, x2, y2, i == 0 ? line : color);
  }
  frame.fillCircle(CX, CY, 3, line);

  for (int i = 0; i < netCount; i++) {
    float strength = (nets[i].rssi + 95) / 55.0f;
    if (strength < 0.15f) strength = 0.15f;
    if (strength > 1.0f) strength = 1.0f;
    float ang = (i + 1) * 1.35f + 0.4f;
    int px = CX + (int)(cosf(ang) * RADAR * strength);
    int py = CY + (int)(sinf(ang) * RADAR * strength);
    frame.fillCircle(px, py, 3, rgb565(255, 196, 80));
  }
}

static void drawStars(float t) {
  for (int i = 0; i < 28; i++) {
    int x = (i * 47 + 13) % W;
    int y = (i * 29 + 7) % 210;
    float tw = 0.5f + 0.5f * sinf(t * 2.0f + i);
    uint8_t v = (uint8_t)(40 + tw * 180);
    frame.drawPixel(x, y, rgb565(v, v, v + 20));
  }
}

static void drawPanel() {
  frame.setTextFont(4);
  frame.setTextColor(rgb565(245, 248, 255));
  frame.setCursor(10, 6);
  frame.print("Mario");

  frame.setTextFont(2);
  frame.setTextColor(rgb565(120, 220, 255));
  frame.setCursor(148, 14);
  frame.print("no ar");

  frame.fillRect(0, 232, W, 88, rgb565(6, 10, 22));
  frame.drawFastHLine(0, 232, W, rgb565(30, 70, 100));

  frame.setTextFont(2);
  if (netCount == 0) {
    frame.setTextColor(rgb565(150, 170, 190));
    frame.setCursor(10, 246);
    frame.print(scanning ? "ouvindo o ar..." : "sem redes ainda");
  } else {
    for (int i = 0; i < netCount; i++) {
      int y = 240 + i * 18;
      frame.setTextColor(rgb565(230, 236, 245));
      frame.setCursor(8, y);
      frame.print(nets[i].ssid);
      int bars = (nets[i].rssi + 100) / 12;
      if (bars < 1) bars = 1;
      if (bars > 5) bars = 5;
      for (int b = 0; b < 5; b++) {
        uint16_t c = b < bars ? rgb565(80, 220, 160) : rgb565(30, 40, 52);
        frame.fillRect(196 + b * 8, y + 10 - b * 2, 5, 4 + b * 2, c);
      }
    }
  }
}

static void paint(float sweep) {
  frame.fillSprite(BG);
  drawStars(sweep);
  drawRadar(sweep);
  drawPanel();
  frame.pushSprite(0, 0);
}

static void ledFor(float sweep) {
  uint16_t hue = (uint16_t)(fmodf(sweep, 6.28318f) / 6.28318f * 65535.0f);
  rgb.setPixelColor(0, rgb.ColorHSV(hue, 230, 70));
  rgb.show();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("painel vivo");

  rgb.begin();
  rgb.setBrightness(40);
  rgb.clear();
  rgb.show();

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(BG);

  frame.setColorDepth(16);
  spriteOk = frame.createSprite(W, H) != nullptr;
  if (!spriteOk) {
    tft.setTextFont(4);
    tft.setTextColor(TFT_WHITE, BG);
    tft.setCursor(10, 40);
    tft.print("Mario");
    Serial.println("sprite falhou");
  }

  Serial.printf("psram %u flash ok\n", (unsigned)ESP.getPsramSize());
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  lastScan = millis() - 7000;
  fpsStamp = millis();
}

void loop() {
  pollWifi();
  float sweep = millis() / 450.0f;

  if (spriteOk) {
    paint(sweep);
  }
  ledFor(sweep);

  frames++;
  if (millis() - fpsStamp >= 2000) {
    fps = frames / 2;
    frames = 0;
    fpsStamp = millis();
    Serial.printf("vivo fps=%d redes=%d\n", fps, netCount);
  }
}
