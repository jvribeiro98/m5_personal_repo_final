#pragma once
// ============================================================
// M5 PERSONAL - UI CORE
// Design system da nova interface: tema, animacao, icones
// vetoriais e widgets reutilizaveis. Nao depende do estado do
// aplicativo; tudo e passado por parametro.
//
// Tela: 240x135 (landscape), canvas RGB565 em PSRAM.
// ============================================================

#include <M5Unified.h>
#include <math.h>

namespace ui {

// ------------------------------------------------------------
// GEOMETRIA
// ------------------------------------------------------------
constexpr int W = 240;
constexpr int H = 135;
constexpr int HEADER_H = 20;   // barra superior (titulo + status)
constexpr int FOOTER_H = 14;   // dicas de botao
constexpr int CONTENT_Y = HEADER_H + 2;
constexpr int CONTENT_H = H - HEADER_H - FOOTER_H - 4;

// ------------------------------------------------------------
// CORES (RGB565)
// ------------------------------------------------------------
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t BG       = rgb(4, 5, 10);
constexpr uint16_t SURFACE  = rgb(16, 18, 28);
constexpr uint16_t SURFACE2 = rgb(26, 30, 44);
constexpr uint16_t SURFACE3 = rgb(38, 43, 62);
constexpr uint16_t BORDER   = rgb(44, 50, 70);
constexpr uint16_t TEXT     = rgb(242, 244, 255);
constexpr uint16_t MUTED    = rgb(140, 148, 178);
constexpr uint16_t DIM      = rgb(84, 90, 116);
constexpr uint16_t WHITE    = 0xFFFF;

constexpr uint16_t BLUE     = rgb(77, 163, 255);
constexpr uint16_t INDIGO   = rgb(124, 156, 255);
constexpr uint16_t VIOLET   = rgb(186, 110, 255);
constexpr uint16_t CYAN     = rgb(61, 210, 255);
constexpr uint16_t TEAL     = rgb(52, 220, 200);
constexpr uint16_t ORANGE   = rgb(255, 150, 56);
constexpr uint16_t GREEN    = rgb(64, 220, 140);
constexpr uint16_t RED      = rgb(255, 92, 122);
constexpr uint16_t YELLOW   = rgb(255, 204, 80);
constexpr uint16_t PINK     = rgb(255, 120, 200);

// Mistura linear entre duas cores (t=0 -> a, t=1 -> b).
inline uint16_t blend(uint16_t a, uint16_t b, float t) {
  if (t <= 0.0f) return a;
  if (t >= 1.0f) return b;
  const int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  const int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  const int r = ar + (int)((br - ar) * t);
  const int g = ag + (int)((bg - ag) * t);
  const int bl = ab + (int)((bb - ab) * t);
  return (uint16_t)((r << 11) | (g << 5) | bl);
}
// Escurece em direcao ao fundo (k=1 -> cor original).
inline uint16_t dim(uint16_t c, float k) { return blend(BG, c, k); }
// Tinta o painel com a cor de destaque (k pequeno = sutil).
inline uint16_t tint(uint16_t base, uint16_t accent, float k) { return blend(base, accent, k); }

// ------------------------------------------------------------
// FONTES
// ------------------------------------------------------------
inline const lgfx::IFont* F_TINY()  { return &fonts::Font0; }              // 6x8 - dicas, status
inline const lgfx::IFont* F_BODY()  { return &fonts::Font2; }              // 16px - texto padrao
inline const lgfx::IFont* F_TITLE() { return &fonts::FreeSansBold9pt7b; }  // titulos
inline const lgfx::IFont* F_H2()    { return &fonts::FreeSansBold12pt7b; } // subtitulo forte
inline const lgfx::IFont* F_BIG()   { return &fonts::FreeSansBold18pt7b; } // numeros medios
inline const lgfx::IFont* F_HUGE()  { return &fonts::FreeSansBold24pt7b; } // numeros grandes
inline const lgfx::IFont* F_CLOCK() { return &fonts::DejaVu40; }           // relogio

inline void font(M5Canvas& d, const lgfx::IFont* f, float size = 1.0f) {
  d.setFont(f);
  d.setTextSize(size);
}

// Texto com cor de frente e fundo transparente.
inline void text(M5Canvas& d, const String& s, int x, int y, uint16_t color,
                 lgfx::textdatum_t datum = lgfx::textdatum_t::top_left, const lgfx::IFont* f = nullptr, float size = 1.0f) {
  if (f) font(d, f, size);
  d.setTextDatum(datum);
  d.setTextColor(color);
  d.drawString(s, x, y);
}

// Corta texto para caber em maxW, acrescentando "..." se preciso.
inline String fit(M5Canvas& d, const String& s, int maxW) {
  if (d.textWidth(s) <= maxW) return s;
  String out = s;
  while (out.length() > 1 && d.textWidth(out + "...") > maxW) out.remove(out.length() - 1);
  return out + "...";
}

// ------------------------------------------------------------
// EASING E ANIMACAO
// ------------------------------------------------------------
inline float clamp01(float t) { return t < 0 ? 0 : (t > 1 ? 1 : t); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float easeOutCubic(float t) { t = 1.0f - clamp01(t); return 1.0f - t * t * t; }
inline float easeOutQuint(float t) { t = 1.0f - clamp01(t); return 1.0f - t * t * t * t * t; }
inline float easeInOutCubic(float t) { t = clamp01(t); return t < 0.5f ? 4 * t * t * t : 1 - powf(-2 * t + 2, 3) / 2; }
inline float easeOutBack(float t) {
  t = clamp01(t);
  const float c1 = 1.70158f, c3 = c1 + 1.0f;
  const float u = t - 1.0f;
  return 1.0f + c3 * u * u * u + c1 * u * u;
}
inline float easeOutElastic(float t) {
  t = clamp01(t);
  if (t == 0 || t == 1) return t;
  const float c4 = (2 * (float)M_PI) / 3;
  return powf(2, -10 * t) * sinf((t * 10 - 0.75f) * c4) + 1;
}
// Onda 0..1 com periodo em ms (para respiracao/pulso).
inline float pulse(uint32_t periodMs, uint32_t phaseMs = 0) {
  const float p = ((millis() + phaseMs) % periodMs) / (float)periodMs;
  return 0.5f - 0.5f * cosf(p * 2 * (float)M_PI);
}

// Estado global de quadro: quem anima pede o proximo frame.
struct FrameState {
  uint32_t lastFrameAt = 0;
  float dt = 16.0f;         // ms desde o ultimo quadro (limitado)
  bool framePending = false;
  void beginFrame() {
    const uint32_t now = millis();
    dt = lastFrameAt ? (float)min<uint32_t>(now - lastFrameAt, 60) : 16.0f;
    lastFrameAt = now;
    framePending = false;
  }
};
inline FrameState& frame() { static FrameState f; return f; }
inline void requestFrame() { frame().framePending = true; }
inline bool framePending() { return frame().framePending; }

// Suavizador exponencial (segue o alvo com inercia). Usa dt do quadro.
struct Smooth {
  float value = 0.0f;
  float target = 0.0f;
  bool init = false;
  void snap(float v) { value = target = v; init = true; }
  void to(float v) { if (!init) snap(v); target = v; }
  // speed = fracao percorrida a cada 16 ms (0.2 lento, 0.4 rapido)
  float tick(float speed = 0.3f, float eps = 0.02f) {
    if (!init) return value;
    const float k = 1.0f - powf(1.0f - speed, frame().dt / 16.0f);
    value += (target - value) * k;
    if (fabsf(target - value) <= eps) value = target;
    else requestFrame();
    return value;
  }
  bool settled() const { return value == target; }
};

// Tween temporizado (0..1) com easing.
struct Tween {
  uint32_t startAt = 0;
  uint16_t durMs = 0;
  bool active = false;
  void start(uint16_t ms) { startAt = millis(); durMs = ms; active = true; }
  void stop() { active = false; }
  // progresso linear 0..1; pede frame enquanto roda
  float raw() {
    if (!active) return 1.0f;
    const float x = (millis() - startAt) / (float)max<uint16_t>(1, durMs);
    if (x >= 1.0f) { active = false; return 1.0f; }
    requestFrame();
    return x;
  }
  float out() { return easeOutCubic(raw()); }
  float back() { return easeOutBack(raw()); }
};

// ------------------------------------------------------------
// PRIMITIVAS
// ------------------------------------------------------------
inline void card(M5Canvas& d, int x, int y, int w, int h, uint16_t fill = SURFACE, int r = 8, uint16_t border = 0) {
  d.fillSmoothRoundRect(x, y, w, h, r, fill);
  if (border) d.drawRoundRect(x, y, w, h, r, border);
}

// Barra de progresso arredondada.
inline void bar(M5Canvas& d, int x, int y, int w, int h, float pct, uint16_t color, uint16_t track = SURFACE3) {
  pct = clamp01(pct);
  d.fillSmoothRoundRect(x, y, w, h, h / 2, track);
  const int fw = (int)(w * pct);
  if (fw >= h) d.fillSmoothRoundRect(x, y, fw, h, h / 2, color);
  else if (fw > 0) d.fillSmoothCircle(x + h / 2, y + h / 2, h / 2, color);
}

// Arco de progresso: comeca as 12h, sentido horario.
inline void arc(M5Canvas& d, int cx, int cy, int r, int thick, float pct, uint16_t color, uint16_t track = SURFACE3) {
  pct = clamp01(pct);
  if (track) d.fillArc(cx, cy, r, r - thick, 0, 360, track);
  if (pct > 0.002f) d.fillArc(cx, cy, r, r - thick, 270, 270 + 360.0f * pct, color);
}

// Spinner (arco girando).
inline void spinner(M5Canvas& d, int cx, int cy, int r, int thick, uint16_t color, uint32_t periodMs = 900) {
  const float a = ((millis() % periodMs) / (float)periodMs) * 360.0f;
  d.fillArc(cx, cy, r, r - thick, 0, 360, SURFACE3);
  d.fillArc(cx, cy, r, r - thick, a, a + 100, color);
  d.fillArc(cx, cy, r, r - thick, a + 100, a + 130, dim(color, 0.45f));
  requestFrame();
}

// Pontos indicadores de pagina.
inline void dots(M5Canvas& d, int cx, int y, int count, float pos, uint16_t accent) {
  const int gap = 9;
  const int x0 = cx - (count - 1) * gap / 2;
  for (int i = 0; i < count; ++i) {
    float f = 1.0f - fabsf(pos - i);
    if (f < 0) f = 0;
    const int w = 4 + (int)(8 * f);
    d.fillSmoothRoundRect(x0 + i * gap - w / 2, y, w, 4, 2, blend(SURFACE3, accent, f));
  }
}

// Interruptor liga/desliga com animacao.
inline void toggle(M5Canvas& d, int x, int y, float on, uint16_t accent) {
  const int w = 30, h = 16;
  d.fillSmoothRoundRect(x, y, w, h, h / 2, blend(SURFACE3, accent, on));
  const int kx = x + 8 + (int)((w - 16) * on);
  d.fillSmoothCircle(kx, y + h / 2, 5, blend(MUTED, WHITE, on));
}

// Pilula com texto pequeno.
inline void chip(M5Canvas& d, int x, int y, const String& label, uint16_t bg, uint16_t fg, int padX = 6) {
  font(d, F_TINY());
  const int w = d.textWidth(label) + padX * 2;
  d.fillSmoothRoundRect(x, y, w, 12, 6, bg);
  text(d, label, x + w / 2, y + 6, fg, middle_center);
}
inline int chipWidth(M5Canvas& d, const String& label, int padX = 6) {
  font(d, F_TINY());
  return d.textWidth(label) + padX * 2;
}

// Texto com quebra de linha simples (fonte atual). Retorna linhas desenhadas.
inline int wrapped(M5Canvas& d, int x, int y, int maxW, int lineH, int maxLines, int startLine,
                   const String& textIn, uint16_t color) {
  d.setTextDatum(top_left);
  d.setTextColor(color);
  int curX = x, line = 0;
  String word;
  const int spaceW = d.textWidth(" ");
  for (size_t i = 0; i <= textIn.length(); ++i) {
    const char ch = i < textIn.length() ? textIn[i] : ' ';
    if (ch == ' ' || ch == '\n') {
      if (word.length()) {
        const int ww = d.textWidth(word);
        if (curX + ww > x + maxW && curX > x) { curX = x; ++line; }
        if (line >= startLine && line - startLine < maxLines) d.drawString(word, curX, y + (line - startLine) * lineH);
        curX += ww + spaceW;
        word = "";
      }
      if (ch == '\n') { curX = x; ++line; }
    } else {
      word += ch;
    }
  }
  return line + 1;
}
inline int wrappedLines(M5Canvas& d, int maxW, const String& textIn) {
  int curX = 0, line = 0;
  String word;
  const int spaceW = d.textWidth(" ");
  for (size_t i = 0; i <= textIn.length(); ++i) {
    const char ch = i < textIn.length() ? textIn[i] : ' ';
    if (ch == ' ' || ch == '\n') {
      if (word.length()) {
        const int ww = d.textWidth(word);
        if (curX + ww > maxW && curX > 0) { curX = 0; ++line; }
        curX += ww + spaceW;
        word = "";
      }
      if (ch == '\n') { curX = 0; ++line; }
    } else {
      word += ch;
    }
  }
  return line + 1;
}

// ------------------------------------------------------------
// ICONES VETORIAIS
// Desenhados em coordenadas normalizadas em torno de (cx, cy)
// com tamanho s (lado do quadrado). Tracos escalam com s.
// ------------------------------------------------------------
enum class Icon : uint8_t {
  NONE,
  SPARK,      // IA
  REMOTE,     // controle
  HORSESHOE,  // team penning
  CURSOR,     // air mouse
  WIFI,       // conexao
  CLOCK,      // relogio
  GEAR,       // ajustes
  POWER,
  VOLUME,
  MUTE,
  PLUS,
  MINUS,
  UP, DOWN, LEFT, RIGHT,
  CHECK,
  CLOSE,
  TRASH,
  PENCIL,
  HOME,
  BACK,
  MENU,
  SOURCE,     // entrada (HDMI/AV)
  SNOW,
  FAN,
  SUN,
  MOON,
  SWING,
  BOLT,
  THERMO,
  TV,
  BLUETOOTH,
  MIC,
  BRIGHT,
  SOUND,
  LOCK,
  GLOBE,
  LIST,
  FLAG,
  HISTORY,
  KEY,
  OK,
  TIMER,
  CLOUD
};

inline void icon(M5Canvas& d, Icon id, int cx, int cy, int s, uint16_t c) {
  const float u = s / 24.0f;            // unidade: icones desenhados em grade 24
  const float th = max(0.9f, 1.15f * u); // meia-espessura do traco (drawWideLine usa raio)
  auto X = [&](float v) { return (int)lroundf(cx + v * u); };
  auto Y = [&](float v) { return (int)lroundf(cy + v * u); };
  auto L = [&](float x0, float y0, float x1, float y1) { d.drawWideLine(X(x0), Y(y0), X(x1), Y(y1), th, c); };
  auto R = [&](float v) { return max(1, (int)lroundf(v * u)); };

  switch (id) {
    case Icon::NONE: break;

    case Icon::SPARK: {
      // estrela de 4 pontas + brilho pequeno
      d.fillTriangle(X(0), Y(-10), X(3), Y(-3), X(-3), Y(-3), c);
      d.fillTriangle(X(0), Y(10), X(3), Y(3), X(-3), Y(3), c);
      d.fillTriangle(X(-10), Y(0), X(-3), Y(-3), X(-3), Y(3), c);
      d.fillTriangle(X(10), Y(0), X(3), Y(-3), X(3), Y(3), c);
      d.fillSmoothCircle(X(0), Y(0), R(3.2f), c);
      d.fillSmoothCircle(X(8), Y(-8), R(1.6f), c);
      break;
    }
    case Icon::REMOTE: {
      d.fillSmoothRoundRect(X(-5), Y(-11), R(10), R(22), R(3), c);
      d.fillSmoothCircle(X(0), Y(-5), R(2.2f), BG);
      d.fillRect(X(-2.5f), Y(1), R(5), R(1.6f), BG);
      d.fillRect(X(-2.5f), Y(4.5f), R(5), R(1.6f), BG);
      d.fillRect(X(-2.5f), Y(8), R(5), R(1.6f), BG);
      break;
    }
    case Icon::HORSESHOE: {
      // ferradura: arco aberto embaixo com pregos
      d.fillArc(X(0), Y(0), R(10), R(6.2f), 135, 405, c);
      d.fillSmoothCircle(X(-8), Y(7), R(1.2f), BG);
      d.fillSmoothCircle(X(8), Y(7), R(1.2f), BG);
      d.fillSmoothCircle(X(-8), Y(1), R(1.2f), BG);
      d.fillSmoothCircle(X(8), Y(1), R(1.2f), BG);
      d.fillSmoothCircle(X(-5), Y(-6), R(1.2f), BG);
      d.fillSmoothCircle(X(5), Y(-6), R(1.2f), BG);
      d.fillSmoothCircle(X(0), Y(-8), R(1.2f), BG);
      break;
    }
    case Icon::CURSOR: {
      d.fillTriangle(X(-7), Y(-9), X(7), Y(1), X(-1), Y(2), c);
      d.fillTriangle(X(-7), Y(-9), X(-1), Y(2), X(-5), Y(5), c);
      d.drawWideLine(X(0), Y(1), X(5), Y(9), th * 1.4f, c);
      break;
    }
    case Icon::WIFI: {
      d.fillArc(X(0), Y(6), R(14), R(11.5f), 225, 315, c);
      d.fillArc(X(0), Y(6), R(9), R(6.5f), 225, 315, c);
      d.fillSmoothCircle(X(0), Y(6), R(2.4f), c);
      break;
    }
    case Icon::CLOCK: {
      d.fillArc(X(0), Y(0), R(10.5f), R(8.5f), 0, 360, c);
      L(0, -1, 0, -6);
      L(0, 0, 4.5f, 2.5f);
      d.fillSmoothCircle(X(0), Y(0), R(1.4f), c);
      break;
    }
    case Icon::GEAR: {
      for (int i = 0; i < 8; ++i) {
        const float a = i * (float)M_PI / 4;
        d.drawWideLine(X(cosf(a) * 5), Y(sinf(a) * 5), X(cosf(a) * 10), Y(sinf(a) * 10), th * 1.6f, c);
      }
      d.fillSmoothCircle(X(0), Y(0), R(6.5f), c);
      d.fillSmoothCircle(X(0), Y(0), R(2.6f), BG);
      break;
    }
    case Icon::POWER: {
      d.fillArc(X(0), Y(1), R(9), R(9 - 2.2f), 300, 600, c);
      L(0, -10, 0, -1);
      break;
    }
    case Icon::VOLUME:
    case Icon::MUTE: {
      d.fillRect(X(-9), Y(-3), R(4), R(6), c);
      d.fillTriangle(X(-6), Y(0), X(0), Y(-7), X(0), Y(7), c);
      if (id == Icon::VOLUME) {
        d.fillArc(X(0), Y(0), R(6), R(6 - 2), 315, 405, c);
        d.fillArc(X(0), Y(0), R(10), R(10 - 2), 315, 405, c);
      } else {
        L(4, -4, 10, 4);
        L(10, -4, 4, 4);
      }
      break;
    }
    case Icon::SOUND: {
      d.fillRect(X(-9), Y(-3), R(4), R(6), c);
      d.fillTriangle(X(-6), Y(0), X(0), Y(-7), X(0), Y(7), c);
      d.fillArc(X(0), Y(0), R(6), R(6 - 2), 315, 405, c);
      break;
    }
    case Icon::PLUS:  L(-7, 0, 7, 0); L(0, -7, 0, 7); break;
    case Icon::MINUS: L(-7, 0, 7, 0); break;
    case Icon::UP:    L(-6, 3, 0, -3); L(0, -3, 6, 3); break;
    case Icon::DOWN:  L(-6, -3, 0, 3); L(0, 3, 6, -3); break;
    case Icon::LEFT:  L(3, -6, -3, 0); L(-3, 0, 3, 6); break;
    case Icon::RIGHT: L(-3, -6, 3, 0); L(3, 0, -3, 6); break;
    case Icon::CHECK: L(-8, 0, -2, 6); L(-2, 6, 8, -6); break;
    case Icon::CLOSE: L(-6, -6, 6, 6); L(6, -6, -6, 6); break;
    case Icon::OK: {
      d.fillArc(X(0), Y(0), R(10), R(8), 0, 360, c);
      L(-4.5f, 0, -1.5f, 3.5f); L(-1.5f, 3.5f, 5, -3.5f);
      break;
    }
    case Icon::TRASH: {
      d.fillRect(X(-8), Y(-7), R(16), R(2), c);
      d.fillRect(X(-3), Y(-10), R(6), R(2), c);
      d.fillSmoothRoundRect(X(-6), Y(-4), R(12), R(14), R(1.5f), c);
      d.fillRect(X(-3), Y(-1), R(1.5f), R(8), BG);
      d.fillRect(X(1.5f), Y(-1), R(1.5f), R(8), BG);
      break;
    }
    case Icon::PENCIL: {
      d.drawWideLine(X(-7), Y(7), X(5), Y(-5), th * 2.4f, c);
      d.drawWideLine(X(-7), Y(7), X(5), Y(-5), th * 0.8f, BG);
      d.fillTriangle(X(-9), Y(9), X(-8), Y(5), X(-5), Y(8), c);
      L(6, -6, 8, -8);
      break;
    }
    case Icon::HOME: {
      d.fillTriangle(X(-11), Y(-1), X(0), Y(-10), X(11), Y(-1), c);
      d.fillRect(X(-7), Y(-2), R(14), R(11), c);
      d.fillRect(X(-2), Y(3), R(4), R(6), BG);
      break;
    }
    case Icon::BACK: L(-7, 0, 7, 0); L(-7, 0, -1, -6); L(-7, 0, -1, 6); break;
    case Icon::MENU: L(-8, -6, 8, -6); L(-8, 0, 8, 0); L(-8, 6, 8, 6); break;
    case Icon::LIST: {
      for (int i = -1; i <= 1; ++i) {
        d.fillSmoothCircle(X(-8), Y(i * 6), R(1.6f), c);
        L(-4, i * 6, 8, i * 6);
      }
      break;
    }
    case Icon::SOURCE: {
      d.drawRoundRect(X(-10), Y(-7), R(20), R(14), R(2), c);
      d.drawRoundRect(X(-9), Y(-6), R(18), R(12), R(2), c);
      L(-9, 0, 2, 0); L(2, 0, -2, -4); L(2, 0, -2, 4);
      break;
    }
    case Icon::SNOW: {
      for (int i = 0; i < 3; ++i) {
        const float a = i * (float)M_PI / 3;
        L(cosf(a) * -9, sinf(a) * -9, cosf(a) * 9, sinf(a) * 9);
      }
      d.fillSmoothCircle(X(0), Y(0), R(2), c);
      break;
    }
    case Icon::FAN: {
      for (int i = 0; i < 3; ++i) {
        const float a = i * 120.0f;
        d.fillArc(X(0), Y(0), R(10), R(4), a, a + 70, c);
      }
      d.fillSmoothCircle(X(0), Y(0), R(3), c);
      d.fillSmoothCircle(X(0), Y(0), R(1.2f), BG);
      break;
    }
    case Icon::SUN:
    case Icon::BRIGHT: {
      d.fillSmoothCircle(X(0), Y(0), R(4.5f), c);
      for (int i = 0; i < 8; ++i) {
        const float a = i * (float)M_PI / 4;
        L(cosf(a) * 7, sinf(a) * 7, cosf(a) * 10, sinf(a) * 10);
      }
      break;
    }
    case Icon::MOON: {
      d.fillSmoothCircle(X(0), Y(0), R(9), c);
      d.fillSmoothCircle(X(4), Y(-3), R(7.5f), BG);
      break;
    }
    case Icon::SWING: {
      for (int i = -1; i <= 1; ++i) {
        const float y = i * 6.0f;
        d.drawBezier(X(-9), Y(y), X(-3), Y(y - 4), X(3), Y(y + 4), X(9), Y(y), c);
      }
      break;
    }
    case Icon::BOLT: {
      d.fillTriangle(X(2), Y(-11), X(-7), Y(2), X(0), Y(2), c);
      d.fillTriangle(X(-2), Y(11), X(7), Y(-2), X(0), Y(-2), c);
      d.fillRect(X(-2), Y(-2), R(4), R(4), c);
      break;
    }
    case Icon::THERMO: {
      d.fillSmoothRoundRect(X(-3), Y(-11), R(6), R(15), R(3), c);
      d.fillSmoothCircle(X(0), Y(6), R(5), c);
      d.fillSmoothCircle(X(0), Y(6), R(2.5f), BG);
      d.fillRect(X(-1), Y(-6), R(2), R(11), BG);
      break;
    }
    case Icon::TV: {
      d.fillSmoothRoundRect(X(-11), Y(-8), R(22), R(14), R(2.5f), c);
      d.fillRect(X(-9), Y(-6), R(18), R(10), BG);
      d.fillRect(X(-5), Y(7), R(10), R(2), c);
      break;
    }
    case Icon::BLUETOOTH: {
      L(0, -11, 0, 11);
      L(0, -11, 6, -5); L(6, -5, -6, 5);
      L(0, 11, 6, 5);   L(6, 5, -6, -5);
      break;
    }
    case Icon::MIC: {
      d.fillSmoothRoundRect(X(-4), Y(-11), R(8), R(14), R(4), c);
      d.fillArc(X(0), Y(-1), R(8), R(6), 0, 180, c);
      L(0, 7, 0, 11); L(-4, 11, 4, 11);
      break;
    }
    case Icon::LOCK: {
      d.fillSmoothRoundRect(X(-8), Y(-1), R(16), R(12), R(2.5f), c);
      d.fillArc(X(0), Y(-3), R(6), R(4), 180, 360, c);
      d.fillSmoothCircle(X(0), Y(4), R(1.6f), BG);
      break;
    }
    case Icon::GLOBE: {
      d.fillArc(X(0), Y(0), R(10), R(8), 0, 360, c);
      L(-9, 0, 9, 0);
      d.drawEllipseArc(X(0), Y(0), R(4.5f), R(3), R(10), R(8), 0, 360, c);
      break;
    }
    case Icon::FLAG: {
      L(-7, -11, -7, 11);
      d.fillTriangle(X(-6), Y(-10), X(9), Y(-5), X(-6), Y(0), c);
      break;
    }
    case Icon::HISTORY: {
      d.fillArc(X(0), Y(0), R(10), R(8), 300, 600, c);
      d.fillTriangle(X(-10), Y(-8), X(-4), Y(-4), X(-11), Y(-1), c);
      L(0, -5, 0, 0); L(0, 0, 4, 2);
      break;
    }
    case Icon::KEY: {
      d.fillArc(X(-4), Y(0), R(6), R(3.5f), 0, 360, c);
      L(1, 0, 10, 0); L(7, 0, 7, 4); L(10, 0, 10, 3);
      break;
    }
    case Icon::TIMER: {
      d.fillArc(X(0), Y(1), R(9.5f), R(7.5f), 0, 360, c);
      d.fillRect(X(-3), Y(-12), R(6), R(2), c);
      L(0, 1, 0, -4); L(0, 1, 3, 3);
      break;
    }
    case Icon::CLOUD: {
      d.fillSmoothCircle(X(-4), Y(2), R(5), c);
      d.fillSmoothCircle(X(3), Y(0), R(6.5f), c);
      d.fillSmoothRoundRect(X(-9), Y(2), R(19), R(6), R(3), c);
      break;
    }
  }
}

// ------------------------------------------------------------
// STATUS: WIFI + BATERIA + HORA (canto superior direito)
// ------------------------------------------------------------
struct StatusInfo {
  bool wifi = false;
  int battery = 100;
  bool charging = false;
  String time;      // "" se relogio invalido
  uint16_t accent = INDIGO;
};

inline void wifiBars(M5Canvas& d, int x, int y, bool connected, uint16_t on, uint16_t off = DIM) {
  // 3 arcos, 12x9 px
  d.fillArc(x + 6, y + 9, 9, 7, 225, 315, connected ? on : off);
  d.fillArc(x + 6, y + 9, 6, 4, 225, 315, connected ? on : off);
  d.fillSmoothCircle(x + 6, y + 9, 1, connected ? on : off);
}

inline void batteryPill(M5Canvas& d, int xRight, int y, int level, bool charging) {
  level = constrain(level, 0, 100);
  uint16_t col = GREEN;
  if (charging) col = CYAN;
  else if (level <= 15) col = RED;
  else if (level <= 35) col = YELLOW;
  font(d, F_TINY());
  const String pct = String(level);
  const int tw = d.textWidth(pct);
  const int w = tw + 26;
  const int x = xRight - w;
  d.fillSmoothRoundRect(x, y, w, 12, 6, SURFACE2);
  // corpo da bateria
  d.drawRoundRect(x + 5, y + 3, 12, 6, 1, MUTED);
  d.fillRect(x + 17, y + 5, 1, 2, MUTED);
  const int fw = map(level, 0, 100, 0, 8);
  if (fw > 0) d.fillRect(x + 7, y + 5, fw, 2, col);
  if (charging) icon(d, Icon::BOLT, x + 11, y + 6, 8, WHITE);
  text(d, pct, x + w - 5, y + 6, col, middle_right);
}

inline void statusCluster(M5Canvas& d, const StatusInfo& s, int xRight = W - 4, int y = 4) {
  batteryPill(d, xRight, y, s.battery, s.charging);
  const int bw = 26 + (int)(String(s.battery).length() * 6);
  wifiBars(d, xRight - bw - 16, y + 1, s.wifi, s.accent);
}

// Cabecalho: titulo a esquerda, status a direita.
inline void header(M5Canvas& d, const String& title, uint16_t accent, const StatusInfo& s, const String& subtitle = "") {
  d.fillRect(0, 0, W, HEADER_H, BG);
  d.fillSmoothRoundRect(4, 4, 3, 12, 1, accent);
  font(d, F_TITLE());
  const String shownTitle = fit(d, title, subtitle.length() ? 96 : 140);
  const int tw = d.textWidth(shownTitle);
  text(d, shownTitle, 12, 10, TEXT, middle_left);
  if (subtitle.length()) {
    font(d, F_TINY());
    text(d, fit(d, subtitle, 150 - tw - 8), 12 + tw + 8, 11, MUTED, middle_left);
  }
  statusCluster(d, s);
  d.drawGradientHLine(0, HEADER_H, W, accent, BG);
}

// ------------------------------------------------------------
// RODAPE DE DICAS: ate 3 acoes com a letra do botao.
// ------------------------------------------------------------
struct Hint { const char* key; const char* label; };

inline void footer(M5Canvas& d, const Hint* hints, int count, uint16_t accent) {
  const int y = H - FOOTER_H;
  d.fillRect(0, y, W, FOOTER_H, BG);
  font(d, F_TINY());
  int x = 6;
  for (int i = 0; i < count; ++i) {
    if (!hints[i].label || !hints[i].label[0]) continue;
    const String key = hints[i].key;
    const int kw = d.textWidth(key) + 6;
    d.fillSmoothRoundRect(x, y + 2, kw, 10, 3, i == 0 ? accent : SURFACE3);
    text(d, key, x + kw / 2, y + 7, i == 0 ? BG : TEXT, middle_center);
    x += kw + 3;
    text(d, hints[i].label, x, y + 7, MUTED, middle_left);
    x += d.textWidth(hints[i].label) + 10;
  }
}

// ------------------------------------------------------------
// TOAST (mensagem breve deslizando de baixo)
// ------------------------------------------------------------
struct ToastState {
  String message;
  uint32_t shownAt = 0;
  uint16_t duration = 900;
  uint16_t color = GREEN;
  Icon ic = Icon::NONE;
  bool active() const { return message.length() && (int32_t)(shownAt + duration + 220 - millis()) > 0; }
};
inline ToastState& toastState() { static ToastState t; return t; }
inline void showToast(const String& msg, uint16_t durationMs = 900, uint16_t color = GREEN, Icon ic = Icon::NONE) {
  auto& t = toastState();
  t.message = msg; t.shownAt = millis(); t.duration = durationMs; t.color = color; t.ic = ic;
  requestFrame();
}
inline bool toastActive() { return toastState().active(); }

inline void drawToast(M5Canvas& d) {
  auto& t = toastState();
  if (!t.active()) return;
  const uint32_t el = millis() - t.shownAt;
  float k = 1.0f;
  if (el < 220) k = easeOutBack(el / 220.0f);
  else if (el > t.duration) k = 1.0f - easeOutCubic((el - t.duration) / 220.0f);
  requestFrame();
  font(d, F_BODY());
  const int tw = d.textWidth(t.message);
  const int iconW = t.ic != Icon::NONE ? 18 : 0;
  const int w = min(W - 16, tw + 24 + iconW);
  const int h = 24;
  const int x = (W - w) / 2;
  const int y = H - 5 - (int)(h * k);
  d.fillSmoothRoundRect(x, y, w, h, 12, blend(SURFACE2, t.color, 0.18f));
  d.drawRoundRect(x, y, w, h, 12, blend(SURFACE3, t.color, 0.5f));
  int tx = x + w / 2 + iconW / 2;
  if (t.ic != Icon::NONE) icon(d, t.ic, x + 14, y + h / 2, 14, t.color);
  text(d, fit(d, t.message, w - 20 - iconW), tx, y + h / 2, TEXT, middle_center);
}

// ------------------------------------------------------------
// LISTA COM SELECAO ANIMADA
// ------------------------------------------------------------
struct ListItem {
  String title;
  String value;    // texto a direita (opcional)
  Icon ic = Icon::NONE;
  uint16_t iconColor = 0;   // 0 -> usa accent
  bool danger = false;
};

struct ListState {
  Smooth sel;      // indice selecionado suavizado
  Smooth scroll;   // primeiro item visivel (fracionario)
  int lastSelected = -1;
  void reset() { sel.init = false; scroll.init = false; lastSelected = -1; }
};

// Desenha lista na area de conteudo. rowH recomendado 30 (3 linhas visiveis).
inline void list(M5Canvas& d, ListState& st, const ListItem* items, int count, int selected,
                 uint16_t accent, int y0 = CONTENT_Y, int rowH = 31, int visible = 3) {
  if (count <= 0) return;
  selected = constrain(selected, 0, count - 1);
  if (st.lastSelected != selected) {
    st.lastSelected = selected;
    st.sel.to(selected);
    float first = st.scroll.init ? st.scroll.target : 0;
    if (selected < first) first = selected;
    if (selected > first + visible - 1) first = selected - (visible - 1);
    st.scroll.to(first);
  }
  const float selF = st.sel.tick(0.42f, 0.01f);
  const float first = st.scroll.tick(0.38f, 0.01f);

  const int areaH = visible * rowH;
  d.setClipRect(0, y0, W, areaH);
  const int xL = 6, wL = W - 12 - (count > visible ? 6 : 0);

  // destaque animado (atras dos itens)
  const int hy = y0 + (int)((selF - first) * rowH);
  d.fillSmoothRoundRect(xL, hy + 1, wL, rowH - 3, 8, tint(SURFACE2, accent, 0.22f));
  d.fillSmoothRoundRect(xL, hy + 6, 3, rowH - 13, 1, accent);

  for (int i = 0; i < count; ++i) {
    const int y = y0 + (int)((i - first) * rowH);
    if (y + rowH < y0 || y > y0 + areaH) continue;
    float f = 1.0f - fabsf(selF - i);
    if (f < 0) f = 0;
    const uint16_t iconC = items[i].danger ? RED : (items[i].iconColor ? items[i].iconColor : accent);
    int tx = xL + 12;
    if (items[i].ic != Icon::NONE) {
      d.fillSmoothCircle(xL + 20, y + rowH / 2 - 1, 10, blend(SURFACE2, tint(SURFACE3, iconC, 0.35f), f));
      icon(d, items[i].ic, xL + 20, y + rowH / 2 - 1, 13, blend(MUTED, iconC, f));
      tx = xL + 38;
    }
    font(d, F_BODY());
    const int valueW = items[i].value.length() ? min(90, (int)d.textWidth(items[i].value) + 8) : 0;
    const uint16_t tc = items[i].danger ? blend(dim(RED, 0.7f), RED, f) : blend(MUTED, TEXT, f);
    text(d, fit(d, items[i].title, xL + wL - 10 - valueW - tx), tx, y + rowH / 2 - 1, tc, middle_left);
    if (items[i].value.length()) {
      font(d, F_TINY());
      text(d, items[i].value, xL + wL - 10, y + rowH / 2 - 1, blend(DIM, accent, f), middle_right);
    }
  }
  d.clearClipRect();

  // barra de rolagem
  if (count > visible) {
    const int trackX = W - 5, trackY = y0 + 2, trackH = areaH - 4;
    d.fillSmoothRoundRect(trackX, trackY, 2, trackH, 1, SURFACE2);
    const int thumbH = max(10, trackH * visible / count);
    const int thumbY = trackY + (int)((trackH - thumbH) * (first / max(1, count - visible)));
    d.fillSmoothRoundRect(trackX, thumbY, 2, thumbH, 1, accent);
  }
}

// ------------------------------------------------------------
// GRADE DE BOTOES (controle remoto)
// ------------------------------------------------------------
struct GridButton {
  Icon ic;
  const char* label;
  String value;   // opcional (ex.: "23C", "AUTO")
};

struct GridState {
  Smooth sx, sy;
  int lastSelected = -1;
  uint32_t pressedAt = 0;
  int pressedIndex = -1;
  void press(int idx) { pressedAt = millis(); pressedIndex = idx; requestFrame(); }
  void reset() { sx.init = false; sy.init = false; lastSelected = -1; pressedIndex = -1; }
};

inline void grid(M5Canvas& d, GridState& st, const GridButton* btns, int count, int cols, int selected,
                 int x0, int y0, int cellW, int cellH, int gap, uint16_t accent, bool compact = false) {
  const int rows = (count + cols - 1) / cols;
  const int sc = selected % cols, sr = selected / cols;
  if (st.lastSelected != selected) {
    st.lastSelected = selected;
    st.sx.to(sc); st.sy.to(sr);
  }
  const float fx = st.sx.tick(0.45f, 0.01f), fy = st.sy.tick(0.45f, 0.01f);

  // moldura animada de foco
  const int hx = x0 + (int)(fx * (cellW + gap)), hy = y0 + (int)(fy * (cellH + gap));
  d.fillSmoothRoundRect(hx - 2, hy - 2, cellW + 4, cellH + 4, 9, tint(SURFACE2, accent, 0.5f));

  for (int i = 0; i < count; ++i) {
    const int c = i % cols, r = i / cols;
    const int x = x0 + c * (cellW + gap), y = y0 + r * (cellH + gap);
    float f = 1.0f - max(fabsf(fx - c), fabsf(fy - r));
    if (f < 0) f = 0;
    // flash de pressionado
    float press = 0;
    if (st.pressedIndex == i) {
      const uint32_t el = millis() - st.pressedAt;
      if (el < 220) { press = 1.0f - el / 220.0f; requestFrame(); }
      else st.pressedIndex = -1;
    }
    uint16_t fill = blend(SURFACE, tint(SURFACE2, accent, 0.25f), f);
    fill = blend(fill, accent, press * 0.85f);
    const int shrink = (int)(press * 2);
    d.fillSmoothRoundRect(x + shrink, y + shrink, cellW - shrink * 2, cellH - shrink * 2, 7, fill);
    const uint16_t fg = press > 0.4f ? BG : blend(MUTED, TEXT, f);
    const uint16_t ic = press > 0.4f ? BG : blend(DIM, accent, f);
    if (compact) {
      icon(d, btns[i].ic, x + 13, y + cellH / 2, 13, ic);
      font(d, F_TINY());
      if (btns[i].value.length()) {
        text(d, btns[i].label, x + 25, y + cellH / 2 - 6, fg, middle_left);
        text(d, btns[i].value, x + 25, y + cellH / 2 + 5, blend(DIM, accent, max(f, press)), middle_left);
      } else {
        text(d, btns[i].label, x + 25, y + cellH / 2, fg, middle_left);
      }
    } else {
      icon(d, btns[i].ic, x + cellW / 2, y + cellH / 2 - 6, 15, ic);
      font(d, F_TINY());
      text(d, btns[i].label, x + cellW / 2, y + cellH - 8, fg, middle_center);
    }
  }
  (void)rows;
}

// ------------------------------------------------------------
// DIALOGO DE CONFIRMACAO
// ------------------------------------------------------------
inline void dialog(M5Canvas& d, Tween& enter, const String& title, const String& body,
                   const char* confirmLabel, uint16_t accent, Icon ic = Icon::NONE) {
  const float k = enter.back();
  d.fillRectAlpha(0, 0, W, H, 150, BG);
  const int w = 212, h = 88;
  const int x = (W - w) / 2, y = 22 + (int)((1.0f - k) * 30);
  d.fillSmoothRoundRect(x, y, w, h, 12, SURFACE2);
  d.drawRoundRect(x, y, w, h, 12, blend(BORDER, accent, 0.5f));
  int tx = x + 14;
  if (ic != Icon::NONE) {
    d.fillSmoothCircle(x + 24, y + 22, 13, tint(SURFACE3, accent, 0.35f));
    icon(d, ic, x + 24, y + 22, 16, accent);
    tx = x + 44;
  }
  font(d, F_TITLE());
  text(d, fit(d, title, w - (tx - x) - 10), tx, y + 22, TEXT, middle_left);
  font(d, F_TINY());
  wrapped(d, x + 14, y + 40, w - 28, 10, 2, 0, body, MUTED);
  // botoes
  const int by = y + h - 26;
  d.fillSmoothRoundRect(x + 14, by, 92, 18, 9, accent);
  font(d, F_TINY());
  text(d, String("A  ") + confirmLabel, x + 14 + 46, by + 9, BG, middle_center);
  d.fillSmoothRoundRect(x + w - 14 - 84, by, 84, 18, 9, SURFACE3);
  text(d, "B/C  Cancelar", x + w - 14 - 42, by + 9, TEXT, middle_center);
}

// ------------------------------------------------------------
// NUMERO GRANDE COM ROLAGEM (troca de valor animada)
// ------------------------------------------------------------
struct RollState {
  int shown = INT32_MIN;
  int previous = INT32_MIN;
  uint32_t changedAt = 0;
  int dir = 1;
  void set(int v) {
    if (shown == INT32_MIN) { shown = previous = v; return; }
    if (v != shown) { previous = shown; dir = v > shown ? 1 : -1; shown = v; changedAt = millis(); requestFrame(); }
  }
};

inline void rollNumber(M5Canvas& d, RollState& st, int value, int cx, int cy, const lgfx::IFont* f,
                       uint16_t color, int clipH = 44) {
  st.set(value);
  const uint32_t el = millis() - st.changedAt;
  const float t = el < 260 ? easeOutCubic(el / 260.0f) : 1.0f;
  if (t < 1.0f) requestFrame();
  font(d, f);
  d.setClipRect(0, cy - clipH / 2, W, clipH);
  if (t < 1.0f) {
    const int off = (int)((1.0f - t) * clipH * st.dir);
    text(d, String(st.previous), cx, cy - st.dir * clipH + off, blend(color, BG, t), middle_center);
    text(d, String(st.shown), cx, cy + off, blend(BG, color, t), middle_center);
  } else {
    text(d, String(st.shown), cx, cy, color, middle_center);
  }
  d.clearClipRect();
}

// Bounce curto (escala) para feedback de acao. Retorna fator de escala 1..1.25.
struct Bounce {
  uint32_t at = 0;
  void trigger() { at = millis(); requestFrame(); }
  float scale() {
    const uint32_t el = millis() - at;
    if (el >= 320) return 1.0f;
    requestFrame();
    return 1.0f + 0.25f * (1.0f - easeOutElastic(el / 320.0f));
  }
};

// ------------------------------------------------------------
// ORBE (assistente de IA): aneis suaves que respiram/pulsam.
// energy 0..1 controla intensidade; hue define cor principal.
// ------------------------------------------------------------
inline void orb(M5Canvas& d, int cx, int cy, int r, float energy, uint16_t colA, uint16_t colB, bool spinning = false) {
  const float breath = pulse(2600);
  const float rr = r * (0.92f + 0.08f * breath + 0.18f * energy);
  // halo externo em camadas
  for (int i = 4; i >= 1; --i) {
    const float k = i / 4.0f;
    d.fillSmoothCircle(cx, cy, (int)(rr * (1.0f + 0.32f * k)), blend(BG, blend(colA, colB, k), 0.10f + 0.06f * energy * (1 - k)));
  }
  // corpo
  d.fillSmoothCircle(cx, cy, (int)rr, blend(colA, colB, 0.35f + 0.3f * breath));
  d.fillSmoothCircle(cx - (int)(rr * 0.22f), cy - (int)(rr * 0.24f), (int)(rr * 0.42f), blend(blend(colA, colB, 0.5f), WHITE, 0.35f));
  d.fillSmoothCircle(cx + (int)(rr * 0.18f), cy + (int)(rr * 0.2f), (int)(rr * 0.5f), blend(blend(colA, colB, 0.6f), colB, 0.5f));
  d.fillSmoothCircle(cx - (int)(rr * 0.3f), cy - (int)(rr * 0.35f), (int)max(1.0f, rr * 0.14f), blend(WHITE, colA, 0.2f));
  if (spinning) {
    const float a = (millis() % 1400) / 1400.0f * 360.0f;
    d.fillArc(cx, cy, (int)(rr * 1.35f), (int)(rr * 1.35f) - 2, a, a + 120, blend(BG, colA, 0.85f));
    d.fillArc(cx, cy, (int)(rr * 1.55f), (int)(rr * 1.55f) - 1, 360 - a, 360 - a + 60, blend(BG, colB, 0.6f));
  }
  requestFrame();
}

// Barras de onda de audio centradas em (cx, cy).
inline void waveBars(M5Canvas& d, int cx, int cy, int count, int gap, int maxH, float level, uint32_t phaseMs, uint16_t colA, uint16_t colB) {
  const int total = count * 4 + (count - 1) * gap;
  const int x0 = cx - total / 2;
  for (int i = 0; i < count; ++i) {
    const float env = 1.0f - fabsf((i - (count - 1) / 2.0f) / (count / 2.0f)) * 0.75f;
    const float wave = 0.5f + 0.5f * sinf((phaseMs / 90.0f) + i * 0.9f);
    int h = (int)(maxH * env * (0.15f + 0.85f * wave * (0.25f + 0.75f * level)));
    h = max(3, h);
    const uint16_t c = blend(colA, colB, i / (float)(count - 1));
    d.fillSmoothRoundRect(x0 + i * (4 + gap), cy - h / 2, 4, h, 2, c);
  }
  requestFrame();
}

} // namespace ui
