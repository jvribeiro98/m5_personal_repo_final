#pragma once
// ============================================================
// M5 PERSONAL - TELAS
// Todas as telas da nova interface. Incluido pelo .ino depois
// das variaveis globais e dos prototipos de negocio.
//
// Regras de navegacao (iguais em todas as telas):
//   A curto = OK / acao   B curto = proximo   C curto = anterior
//   B longo = voltar      C longo = voltar (no inicio: desligar)
// ============================================================

#include "ui_core.h"

// Prototipos do .ino usados aqui e nao declarados antes.
bool trainingHorseAlreadyChosen(uint8_t horseId, uint8_t beforeSlot);

// ------------------------------------------------------------
// APPS DA TELA INICIAL (ordem do carrossel)
// ------------------------------------------------------------
enum class HomeApp : uint8_t { AI = 0, REMOTE, TEAM, MOUSE, WIFI, CLOCK, SETTINGS };
constexpr uint8_t HOME_APP_COUNT = 7;

struct HomeAppInfo {
  const char* name;
  ui::Icon icon;
  uint16_t accent;
};

const HomeAppInfo HOME_APPS[HOME_APP_COUNT] = {
  {"Assistente",   ui::Icon::SPARK,     ui::VIOLET},
  {"Controle",     ui::Icon::REMOTE,    ui::ORANGE},
  {"Team Penning", ui::Icon::HORSESHOE, ui::GREEN},
  {"Air Mouse",    ui::Icon::CURSOR,    ui::CYAN},
  {"Wi-Fi",        ui::Icon::WIFI,      ui::BLUE},
  {"Relogio",      ui::Icon::CLOCK,     ui::INDIGO},
  {"Ajustes",      ui::Icon::GEAR,      ui::MUTED},
};

// ------------------------------------------------------------
// ESTADO DA UI
// ------------------------------------------------------------
struct UiState {
  ui::Smooth homePos;          // posicao do carrossel
  ui::ListState list;          // lista da tela atual
  ui::GridState grid;          // grade da tela atual
  ui::Smooth cellX, cellY;     // realce animado (grade da boiada)
  ui::Smooth carousel;         // seletor horizontal (cavalos)
  ui::Tween enter;             // entrada da tela / dialogo
  ui::RollState roll, roll2;   // numeros grandes
  ui::Bounce bounce;           // feedback de acao
  ui::Tween transition;        // deslize entre telas
  int8_t transitionDir = 1;    // +1 avanca, -1 volta
  Screen lastScreen = Screen::MAIN;
  bool firstFrame = true;
  bool soundEnabled = true;
  float voiceLevel = 0.0f;     // 0..1 nivel do microfone
  uint32_t lastActionAt = 0;
};
UiState uiState;

static M5Canvas uiPrevCanvas(&M5.Display);
static bool uiPrevCanvasReady = false;

// ------------------------------------------------------------
// SONS DE INTERFACE (curtos; nunca durante o microfone)
// ------------------------------------------------------------
inline bool uiCanBeep() {
  return uiState.soundEnabled && M5.Speaker.isEnabled() && screen != Screen::VOICE_AI;
}
inline void uiClick()   { if (uiCanBeep()) M5.Speaker.tone(2300, 8); }
inline void uiConfirm() { if (uiCanBeep()) M5.Speaker.tone(2700, 22); }
inline void uiError()   { if (uiCanBeep()) M5.Speaker.tone(320, 90); }

// ------------------------------------------------------------
// HELPERS DE ESTADO
// ------------------------------------------------------------
inline ui::StatusInfo uiStatus(uint16_t accent) {
  ui::StatusInfo s;
  s.wifi = WiFi.status() == WL_CONNECTED;
  s.battery = getBatteryLevelCached();
  s.charging = isBatteryChargingCached();
  s.time = clockIsValid() ? clockTimeText() : "";
  s.accent = accent;
  return s;
}

inline uint16_t screenAccent(Screen s) {
  switch (s) {
    case Screen::WIFI_MENU: case Screen::WIFI_SCANNING: case Screen::WIFI_NETWORKS:
    case Screen::WIFI_KEYBOARD: case Screen::WIFI_CONNECTING: case Screen::WIFI_RESULT:
    case Screen::WIFI_AP_INFO: case Screen::WIFI_WEBUI_NETWORK: case Screen::WIFI_SAVED_LIST:
    case Screen::WIFI_SAVED_DETAIL: case Screen::WIFI_DELETE_CONFIRM:
      return ui::BLUE;
    case Screen::IR_TYPES: case Screen::TV_LIST: case Screen::AC_LIST:
    case Screen::TV_REMOTE: case Screen::TV_NAV:
      return ui::ORANGE;
    case Screen::AC_REMOTE:
      return ui::CYAN;
    case Screen::TEAM_MENU: case Screen::TEAM_CATTLE_LIMIT: case Screen::TEAM_CATTLE_COUNTER:
    case Screen::TEAM_CATTLE_RESET_CONFIRM: case Screen::TEAM_TRAIN_COUNT:
    case Screen::TEAM_TRAIN_SELECT_HORSE: case Screen::TEAM_TRAIN_ACTIVE:
    case Screen::TEAM_TRAIN_END_CONFIRM: case Screen::TEAM_TRAIN_SUMMARY:
    case Screen::TEAM_TRAIN_HISTORY: case Screen::TEAM_TRAIN_HISTORY_DETAIL:
      return ui::GREEN;
    case Screen::SETTINGS_MENU: case Screen::SETTINGS_BRIGHTNESS:
    case Screen::SETTINGS_SLEEP:
      return ui::MUTED;
    case Screen::SETTINGS_CLOCK:
      return ui::INDIGO;
    case Screen::MOUSE:
      return ui::CYAN;
    case Screen::VOICE_AI:
      return ui::VIOLET;
    default:
      return ui::INDIGO;
  }
}

// Profundidade de navegacao: define se a transicao avanca ou volta.
inline uint8_t screenDepth(Screen s) {
  switch (s) {
    case Screen::MAIN: return 0;
    case Screen::WIFI_MENU: case Screen::IR_TYPES: case Screen::TEAM_MENU:
    case Screen::SETTINGS_MENU: case Screen::MOUSE: case Screen::VOICE_AI:
      return 1;
    case Screen::WIFI_SCANNING: case Screen::WIFI_NETWORKS: case Screen::WIFI_AP_INFO:
    case Screen::WIFI_WEBUI_NETWORK: case Screen::WIFI_SAVED_LIST:
    case Screen::TV_LIST: case Screen::AC_LIST:
    case Screen::TEAM_CATTLE_LIMIT: case Screen::TEAM_TRAIN_COUNT: case Screen::TEAM_TRAIN_HISTORY:
    case Screen::SETTINGS_BRIGHTNESS: case Screen::SETTINGS_CLOCK: case Screen::SETTINGS_SLEEP:
      return 2;
    case Screen::WIFI_KEYBOARD: case Screen::WIFI_CONNECTING: case Screen::WIFI_RESULT:
    case Screen::WIFI_SAVED_DETAIL: case Screen::TV_REMOTE: case Screen::AC_REMOTE:
    case Screen::TEAM_CATTLE_COUNTER: case Screen::TEAM_TRAIN_SELECT_HORSE:
    case Screen::TEAM_TRAIN_HISTORY_DETAIL:
      return 3;
    case Screen::WIFI_DELETE_CONFIRM: case Screen::TV_NAV: case Screen::TEAM_CATTLE_RESET_CONFIRM:
    case Screen::TEAM_TRAIN_ACTIVE:
      return 4;
    case Screen::TEAM_TRAIN_END_CONFIRM: case Screen::TEAM_TRAIN_SUMMARY:
      return 5;
  }
  return 1;
}

inline bool isDialogScreen(Screen s) {
  return s == Screen::WIFI_DELETE_CONFIRM || s == Screen::TEAM_CATTLE_RESET_CONFIRM || s == Screen::TEAM_TRAIN_END_CONFIRM;
}

// Tela "por baixo" de um dialogo.
inline Screen dialogParent(Screen s) {
  switch (s) {
    case Screen::WIFI_DELETE_CONFIRM: return Screen::WIFI_SAVED_DETAIL;
    case Screen::TEAM_CATTLE_RESET_CONFIRM: return Screen::TEAM_CATTLE_COUNTER;
    case Screen::TEAM_TRAIN_END_CONFIRM: return Screen::TEAM_TRAIN_ACTIVE;
    default: return s;
  }
}

inline void ensurePrevCanvas() {
  if (!uiPrevCanvasReady) {
    uiPrevCanvas.setColorDepth(16);
    if (psramFound()) uiPrevCanvas.setPsram(true);
    uiPrevCanvas.createSprite(ui::W, ui::H);
    uiPrevCanvasReady = true;
  }
}

// Chamado quando a tela muda: zera animacoes locais e inicia o deslize.
inline void uiOnScreenChanged(Screen from, Screen to) {
  uiState.list.reset();
  uiState.grid.reset();
  uiState.cellX.init = uiState.cellY.init = false;
  uiState.carousel.init = false;
  uiState.roll.shown = uiState.roll2.shown = INT32_MIN;
  uiState.enter.start(isDialogScreen(to) ? 260 : 320);
  uiState.transitionDir = screenDepth(to) < screenDepth(from) ? -1 : 1;
  if (!uiState.firstFrame && !isDialogScreen(to) && !isDialogScreen(from) && from != Screen::WIFI_KEYBOARD) {
    ensurePrevCanvas();
    uiCanvas.pushSprite(&uiPrevCanvas, 0, 0);
    uiState.transition.start(260);
  } else {
    uiState.transition.stop();
  }
}

// ------------------------------------------------------------
// RODAPE POR TELA
// ------------------------------------------------------------
inline void drawFooterFor(M5Canvas& d, Screen s, uint16_t accent) {
  using ui::Hint;
  switch (s) {
    case Screen::MAIN: {
      Hint h[] = {{"A", "Abrir"}, {"B", "Prox"}, {"C", "Ant"}, {"C+", "Desligar"}};
      ui::footer(d, h, 4, accent); return;
    }
    case Screen::WIFI_SCANNING: case Screen::WIFI_CONNECTING: {
      Hint h[] = {{"...", "Aguarde"}};
      ui::footer(d, h, 1, ui::SURFACE3); return;
    }
    case Screen::WIFI_RESULT: case Screen::SETTINGS_SLEEP: case Screen::TEAM_TRAIN_SUMMARY: {
      Hint h[] = {{"A", "OK"}, {"B+", "Voltar"}};
      ui::footer(d, h, 2, accent); return;
    }
    case Screen::WIFI_AP_INFO: {
      Hint h[] = {{"B+", "Sair do modo setup"}};
      ui::footer(d, h, 1, accent); return;
    }
    case Screen::WIFI_WEBUI_NETWORK: {
      Hint h[] = {{"A", "Alternar"}, {"C", "Voltar"}};
      ui::footer(d, h, 2, accent); return;
    }
    case Screen::TV_REMOTE: case Screen::TV_NAV: case Screen::AC_REMOTE: {
      Hint h[] = {{"A", "Enviar"}, {"B", "Prox"}, {"C", "Ant"}, {"B+", "Voltar"}};
      ui::footer(d, h, 4, accent); return;
    }
    case Screen::TEAM_CATTLE_LIMIT: case Screen::TEAM_TRAIN_COUNT: case Screen::SETTINGS_BRIGHTNESS: {
      Hint h[] = {{"A", s == Screen::SETTINGS_BRIGHTNESS ? "Salvar" : "Confirmar"}, {"B", "+"}, {"C", "-"}};
      ui::footer(d, h, 3, accent); return;
    }
    case Screen::TEAM_TRAIN_SELECT_HORSE: {
      Hint h[] = {{"A", "Escolher"}, {"B", "Prox"}, {"C", "Ant"}};
      ui::footer(d, h, 3, accent); return;
    }
    case Screen::TEAM_CATTLE_COUNTER: {
      const bool last = cattleRemainingCount() <= 1;
      Hint h[] = {{"A", last ? "Reiniciar" : "Marcar"}, {"B", "Prox"}, {"C", "Ant"}, {"A+", "Zerar"}};
      ui::footer(d, h, 4, accent); return;
    }
    case Screen::TEAM_TRAIN_ACTIVE: {
      Hint h[] = {{"A", "+1"}, {"C", "-1"}, {"B", "Cavalo"}, {"A+", "Fim"}};
      ui::footer(d, h, 4, accent); return;
    }
    case Screen::TEAM_TRAIN_HISTORY_DETAIL: {
      Hint h[] = {{"A", "Voltar"}, {"B", "Prox"}, {"C", "Ant"}};
      ui::footer(d, h, 3, accent); return;
    }
    case Screen::SETTINGS_CLOCK: {
      Hint h[] = {{"A", "Sincronizar"}, {"B", "Estilo"}, {"C", "Voltar"}};
      ui::footer(d, h, 3, accent); return;
    }
    case Screen::MOUSE: case Screen::VOICE_AI: case Screen::WIFI_KEYBOARD:
    case Screen::WIFI_DELETE_CONFIRM: case Screen::TEAM_CATTLE_RESET_CONFIRM: case Screen::TEAM_TRAIN_END_CONFIRM:
      return;
    default: {
      Hint h[] = {{"A", "OK"}, {"B", "Prox"}, {"C", "Ant"}, {"B+", "Voltar"}};
      ui::footer(d, h, 4, accent); return;
    }
  }
}

// ------------------------------------------------------------
// TELA INICIAL - CARROSSEL DE APPS
// ------------------------------------------------------------
inline String homeAppStatus(HomeApp app) {
  switch (app) {
    case HomeApp::AI:
      return voiceInputMode == VoiceInputMode::ALEXA ? "Maos-livres  'Ei M5'" : "Aperte para falar";
    case HomeApp::REMOTE:
      return String(TV_COUNT) + " TVs  -  " + String(AC_COUNT) + " ar-cond.";
    case HomeApp::TEAM:
      if (trainingSession.active) return "Treino em andamento";
      if (cattleSessionInitialized) return "Boiada: faltam " + String(cattleRemainingCount());
      return "Boiada e treinos";
    case HomeApp::MOUSE:
      return isMouseConnected() ? "Conectado" : "Apontador Bluetooth";
    case HomeApp::WIFI:
      return WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "Desconectado";
    case HomeApp::CLOCK: {
      String s = clockIsValid() ? clockDayOfWeekText() + " " + clockDateText().substring(0, 5) : "Sem hora";
      if (!isnan(weatherTemperature)) s += "  " + String((int)roundf(weatherTemperature)) + "C";
      return s;
    }
    case HomeApp::SETTINGS:
      return "Brilho " + String((brightnessIndex + 1) * 20) + "%";
  }
  return "";
}

// delta = distancia (em cartoes) ate o centro; 0 = em foco.
inline void drawHomeCard(M5Canvas& d, int index, float delta, bool animate) {
  const HomeAppInfo& app = HOME_APPS[index];
  // f = 1 no foco, 0 nos vizinhos
  float f = 1.0f - fabsf(delta);
  if (f < 0) f = 0;
  const float ef = ui::easeOutCubic(f);
  const int cx = 120 + (int)(delta * 126);
  const int w = 100 + (int)(20 * ef);
  const int h = 66 + (int)(14 * ef);
  const int cy = 70;
  const int x = cx - w / 2, y = cy - h / 2;
  if (x > ui::W || x + w < 0) return;

  const uint16_t fill = ui::blend(ui::SURFACE, ui::tint(ui::SURFACE2, app.accent, 0.22f), ef);
  d.fillSmoothRoundRect(x, y, w, h, 12, fill);
  if (ef > 0.5f) d.drawRoundRect(x, y, w, h, 12, ui::blend(ui::BORDER, app.accent, (ef - 0.5f) * 1.2f));

  // icone flutuante
  float floatY = 0;
  if (animate && ef > 0.9f) { floatY = sinf(millis() / 420.0f) * 1.5f; ui::requestFrame(); }
  const int ir = 13 + (int)(4 * ef);
  const int icy = y + 24 + (int)floatY;
  d.fillSmoothCircle(cx, icy, ir + 3, ui::blend(fill, app.accent, 0.10f + 0.12f * ef));
  d.fillSmoothCircle(cx, icy, ir, ui::blend(ui::SURFACE3, ui::tint(ui::SURFACE3, app.accent, 0.5f), ef));
  ui::icon(d, app.icon, cx, icy, 14 + (int)(6 * ef), ui::blend(ui::MUTED, app.accent, ef));

  ui::font(d, ui::F_BODY());
  ui::text(d, app.name, cx, y + h - 24, ui::blend(ui::MUTED, ui::TEXT, ef), middle_center);
  if (ef > 0.35f) {
    ui::font(d, ui::F_TINY());
    ui::text(d, ui::fit(d, homeAppStatus((HomeApp)index), w - 14), cx, y + h - 10,
             ui::blend(ui::BG, ui::MUTED, (ef - 0.35f) / 0.65f), middle_center);
  }
}

void drawMain() {
  auto& d = uiCanvas;
  const HomeAppInfo& app = HOME_APPS[selected % HOME_APP_COUNT];

  // carrossel com "wrap" pelo caminho mais curto
  if (!uiState.homePos.init) uiState.homePos.snap(selected);
  else {
    float target = selected;
    const float cur = uiState.homePos.value;
    if (target - cur > HOME_APP_COUNT / 2.0f) uiState.homePos.value = cur + HOME_APP_COUNT;
    else if (cur - target > HOME_APP_COUNT / 2.0f) uiState.homePos.value = cur - HOME_APP_COUNT;
    uiState.homePos.to(target);
  }
  float pos = uiState.homePos.tick(0.34f, 0.004f);
  // normaliza para desenho
  while (pos < 0) pos += HOME_APP_COUNT;
  while (pos >= HOME_APP_COUNT) pos -= HOME_APP_COUNT;

  // brilho de fundo suave na cor do app
  d.fillSmoothCircle(120, 70, 74, ui::blend(ui::BG, app.accent, 0.05f));
  d.fillSmoothCircle(120, 70, 52, ui::blend(ui::BG, app.accent, 0.04f));

  // topo: hora + status
  ui::font(d, ui::F_BODY());
  ui::text(d, clockIsValid() ? clockTimeText() : String("M5 Personal"), 8, 10, ui::TEXT, middle_left);
  ui::statusCluster(d, uiStatus(app.accent));

  const bool animate = millis() - lastUserActivityAt < 6000;
  // vizinhos primeiro (de fora para dentro), foco por ultimo
  const int center = (int)lroundf(pos);
  const int order[] = {-2, 2, -1, 1, 0};
  for (int o = 0; o < 5; ++o) {
    const int k = order[o];
    const int idx = ((center + k) % HOME_APP_COUNT + HOME_APP_COUNT) % HOME_APP_COUNT;
    drawHomeCard(d, idx, (center + k) - pos, animate);
  }

  ui::dots(d, 120, 112, HOME_APP_COUNT, pos, app.accent);
}

// ------------------------------------------------------------
// LISTAS GENERICAS
// ------------------------------------------------------------
inline void drawListScreen(const String& title, uint16_t accent, const ui::ListItem* items, int count, const String& subtitle = "") {
  auto& d = uiCanvas;
  ui::header(d, title, accent, uiStatus(accent), subtitle);
  ui::list(d, uiState.list, items, count, selected, accent);
}

inline void drawEmptyState(M5Canvas& d, ui::Icon ic, const String& title, const String& hint, uint16_t accent) {
  d.fillSmoothCircle(120, 58, 18, ui::tint(ui::SURFACE2, accent, 0.2f));
  ui::icon(d, ic, 120, 58, 20, accent);
  ui::font(d, ui::F_BODY());
  ui::text(d, title, 120, 88, ui::TEXT, middle_center);
  ui::font(d, ui::F_TINY());
  ui::text(d, hint, 120, 104, ui::MUTED, middle_center);
}

// ---------------- WIFI ----------------
void drawWifiMenu() {
  const bool on = WiFi.status() == WL_CONNECTED;
  ui::ListItem items[] = {
    {"Conectar a rede", "buscar", ui::Icon::WIFI},
    {"Modo setup (AP)", "web", ui::Icon::GLOBE},
    {"Web UI na rede", webUiMode == WebUiMode::LAN ? "ATIVA" : "off", ui::Icon::LIST},
    {"Redes salvas", String(savedNetworkCount), ui::Icon::KEY},
  };
  drawListScreen("Wi-Fi", ui::BLUE, items, 4, on ? WiFi.SSID() : "desconectado");
}

void drawWifiScanning() {
  auto& d = uiCanvas;
  ui::header(d, "Wi-Fi", ui::BLUE, uiStatus(ui::BLUE), "procurando");
  ui::spinner(d, 120, 62, 24, 4, ui::BLUE);
  ui::icon(d, ui::Icon::WIFI, 120, 62, 18, ui::BLUE);
  ui::font(d, ui::F_BODY());
  ui::text(d, "Procurando redes...", 120, 100, ui::TEXT, middle_center);
}

void drawWifiNetworks() {
  auto& d = uiCanvas;
  if (!scannedNetworkCount) {
    ui::header(d, "Redes", ui::BLUE, uiStatus(ui::BLUE));
    drawEmptyState(d, ui::Icon::WIFI, "Nenhuma rede", "Segure B para voltar", ui::BLUE);
    return;
  }
  static ui::ListItem items[MAX_SCANNED_NETWORKS];
  for (uint8_t i = 0; i < scannedNetworkCount; ++i) {
    items[i].title = scannedSsids[i];
    items[i].value = (scannedSavedIndex[i] != 255 ? "salva " : "") + wifiSignalLabel(scannedRssi[i]);
    items[i].ic = scannedSavedIndex[i] != 255 ? ui::Icon::KEY : ui::Icon::WIFI;
    items[i].iconColor = scannedRssi[i] > -60 ? ui::GREEN : (scannedRssi[i] > -75 ? ui::YELLOW : ui::RED);
    items[i].danger = false;
  }
  drawListScreen("Redes", ui::BLUE, items, scannedNetworkCount, String(scannedNetworkCount) + " encontradas");
}

void drawWifiConnecting() {
  auto& d = uiCanvas;
  ui::header(d, "Conectando", ui::BLUE, uiStatus(ui::BLUE));
  ui::card(d, 8, 26, 224, 90, ui::SURFACE, 12);
  ui::spinner(d, 36, 60, 18, 3, ui::BLUE);
  ui::icon(d, ui::Icon::WIFI, 36, 60, 14, ui::BLUE);
  ui::font(d, ui::F_BODY());
  ui::text(d, ui::fit(d, wifiPendingSsid, 150), 66, 50, ui::TEXT, middle_left);
  ui::font(d, ui::F_TINY());
  const uint32_t el = millis() - wifiConnectStartedAt;
  ui::text(d, "Autenticando...  " + String(el / 1000) + "s", 66, 70, ui::MUTED, middle_left);
  ui::bar(d, 66, 90, 150, 5, el / (float)WIFI_CONNECT_TIMEOUT_MS, ui::BLUE, ui::SURFACE3);
  ui::requestFrame();
}

void drawWifiResult() {
  auto& d = uiCanvas;
  const bool ok = wifiResultTitle == "CONECTADO";
  const uint16_t c = ok ? ui::GREEN : ui::RED;
  ui::header(d, "Wi-Fi", c, uiStatus(c));
  const float k = uiState.enter.back();
  const int r = (int)(24 * k);
  d.fillSmoothCircle(120, 56, r + 6, ui::blend(ui::BG, c, 0.12f));
  d.fillSmoothCircle(120, 56, r, ui::tint(ui::SURFACE2, c, 0.35f));
  if (k > 0.3f) ui::icon(d, ok ? ui::Icon::CHECK : ui::Icon::CLOSE, 120, 56, (int)(26 * k), c);
  ui::font(d, ui::F_TITLE());
  ui::text(d, ok ? "Conectado" : "Nao conectou", 120, 94, ui::TEXT, middle_center);
  ui::font(d, ui::F_TINY());
  String detail = wifiResultDetail;
  if (ok && WiFi.status() == WL_CONNECTED) detail = WiFi.SSID() + "  -  " + WiFi.localIP().toString();
  ui::text(d, ui::fit(d, detail, 220), 120, 110, ui::MUTED, middle_center);
}

inline void infoRow(M5Canvas& d, int y, ui::Icon ic, const String& label, const String& value, uint16_t accent) {
  ui::icon(d, ic, 22, y, 13, accent);
  ui::font(d, ui::F_TINY());
  ui::text(d, label, 36, y, ui::MUTED, middle_left);
  ui::font(d, ui::F_BODY());
  ui::text(d, value, 226, y, ui::TEXT, middle_right);
}

void drawWifiApInfo() {
  auto& d = uiCanvas;
  ui::header(d, "Modo setup", ui::BLUE, uiStatus(ui::BLUE), "AP ativo");
  ui::card(d, 8, 25, 224, 92, ui::SURFACE, 12);
  infoRow(d, 40, ui::Icon::WIFI, "Rede", WIFI_SETUP_SSID, ui::BLUE);
  infoRow(d, 66, ui::Icon::LOCK, "Senha", WIFI_SETUP_PASSWORD, ui::BLUE);
  infoRow(d, 92, ui::Icon::GLOBE, "Acesse", "192.168.4.1", ui::CYAN);
  // pulso indicando transmissao
  const float p = ui::pulse(1600);
  d.fillSmoothCircle(222, 34, 3, ui::blend(ui::SURFACE3, ui::GREEN, p));
  ui::requestFrame();
}

void drawWifiWebUiNetwork() {
  auto& d = uiCanvas;
  const bool on = webUiMode == WebUiMode::LAN;
  const bool wifi = WiFi.status() == WL_CONNECTED;
  ui::header(d, "Web UI", ui::BLUE, uiStatus(ui::BLUE), wifi ? WiFi.SSID() : "sem wi-fi");
  ui::card(d, 8, 25, 224, 92, ui::SURFACE, 12);
  static ui::Smooth sw;
  sw.to(on ? 1.0f : 0.0f);
  const float k = sw.tick(0.3f);
  ui::toggle(d, 186, 40, k, ui::GREEN);
  ui::font(d, ui::F_BODY());
  ui::text(d, on ? "Painel ativo" : "Painel desligado", 20, 48, ui::TEXT, middle_left);
  ui::font(d, ui::F_TINY());
  ui::text(d, "Controle pelo navegador na mesma rede", 20, 66, ui::MUTED, middle_left);
  ui::icon(d, ui::Icon::GLOBE, 26, 96, 13, wifi ? ui::CYAN : ui::DIM);
  ui::font(d, ui::F_BODY());
  ui::text(d, wifi ? "http://" + WiFi.localIP().toString() : "Conecte ao Wi-Fi primeiro", 40, 96, wifi ? ui::CYAN : ui::MUTED, middle_left);
}

void drawWifiSavedList() {
  auto& d = uiCanvas;
  if (!savedNetworkCount) {
    ui::header(d, "Redes salvas", ui::BLUE, uiStatus(ui::BLUE));
    drawEmptyState(d, ui::Icon::KEY, "Nenhuma rede salva", "Conecte-se a uma rede para salvar", ui::BLUE);
    return;
  }
  static ui::ListItem items[MAX_SAVED_NETWORKS];
  for (uint8_t i = 0; i < savedNetworkCount; ++i) {
    items[i].title = savedNetworks[i].ssid;
    items[i].ic = ui::Icon::WIFI;
    items[i].danger = false;
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == savedNetworks[i].ssid) { items[i].value = "atual"; items[i].iconColor = ui::GREEN; }
    else if (savedNetworks[i].health == SavedNetworkHealth::WARNING) { items[i].value = "falhou"; items[i].iconColor = ui::YELLOW; }
    else if (savedNetworks[i].health == SavedNetworkHealth::VERIFIED) { items[i].value = "ok"; items[i].iconColor = ui::BLUE; }
    else { items[i].value = ""; items[i].iconColor = ui::MUTED; }
  }
  drawListScreen("Redes salvas", ui::BLUE, items, savedNetworkCount, String(savedNetworkCount) + "/" + String(MAX_SAVED_NETWORKS));
}

void drawWifiSavedDetail() {
  if (wifiSelectedSavedIndex < 0 || wifiSelectedSavedIndex >= savedNetworkCount) {
    screen = Screen::WIFI_SAVED_LIST;
    selected = 0;
    return;
  }
  const SavedNetwork& n = savedNetworks[wifiSelectedSavedIndex];
  ui::ListItem items[] = {
    {"Conectar", "", ui::Icon::WIFI},
    {"Editar nome", "", ui::Icon::PENCIL},
    {"Editar senha", "", ui::Icon::KEY},
    {"Excluir rede", "", ui::Icon::TRASH, 0, true},
  };
  drawListScreen(n.ssid, ui::BLUE, items, 4, savedNetworkHealthText(n));
}

void drawWifiDeleteConfirm() {
  auto& d = uiCanvas;
  drawWifiSavedDetail();
  ui::dialog(d, uiState.enter, "Excluir rede?", savedNetworks[wifiSelectedSavedIndex].ssid + " sera removida das redes salvas.",
             "Excluir", ui::RED, ui::Icon::TRASH);
}

// ---------------- CONTROLE IR ----------------
void drawIrTypes() {
  ui::ListItem items[] = {
    {"Televisores", String(TV_COUNT) + " aparelhos", ui::Icon::TV},
    {"Ar-condicionado", String(AC_COUNT) + " aparelhos", ui::Icon::SNOW, ui::CYAN},
  };
  drawListScreen("Controle", ui::ORANGE, items, 2);
}

void drawTvList() {
  static ui::ListItem items[TV_COUNT];
  for (uint8_t i = 0; i < TV_COUNT; ++i) {
    items[i].title = televisions[i].name;
    items[i].value = televisions[i].code[0] ? "" : "sem codigos";
    items[i].ic = ui::Icon::TV;
  }
  drawListScreen("Televisores", ui::ORANGE, items, TV_COUNT);
}

void drawAcList() {
  static ui::ListItem items[AC_COUNT];
  for (uint8_t i = 0; i < AC_COUNT; ++i) {
    const AcState& s = airConditioners[i].state;
    items[i].title = airConditioners[i].name;
    items[i].value = s.power ? String(s.temp) + "C ligado" : "desligado";
    items[i].ic = ui::Icon::SNOW;
    items[i].iconColor = s.power ? ui::CYAN : ui::MUTED;
  }
  drawListScreen("Ar-condicionado", ui::CYAN, items, AC_COUNT);
}

void drawTvRemote(bool navigationPage) {
  auto& d = uiCanvas;
  const TvDevice& tv = televisions[activeTv];
  ui::header(d, tv.name, ui::ORANGE, uiStatus(ui::ORANGE), navigationPage ? "navegacao" : "controle");
  static const ui::GridButton mainBtns[8] = {
    {ui::Icon::POWER, "Ligar", ""}, {ui::Icon::MUTE, "Mudo", ""}, {ui::Icon::PLUS, "Vol +", ""}, {ui::Icon::MINUS, "Vol -", ""},
    {ui::Icon::UP, "Canal +", ""}, {ui::Icon::DOWN, "Canal -", ""}, {ui::Icon::SOURCE, "Entrada", ""}, {ui::Icon::MENU, "Navegar", ""},
  };
  static const ui::GridButton nav[8] = {
    {ui::Icon::UP, "Cima", ""}, {ui::Icon::DOWN, "Baixo", ""}, {ui::Icon::LEFT, "Esq", ""}, {ui::Icon::RIGHT, "Dir", ""},
    {ui::Icon::OK, "OK", ""}, {ui::Icon::BACK, "Voltar", ""}, {ui::Icon::HOME, "Inicio", ""}, {ui::Icon::LIST, "Menu", ""},
  };
  ui::grid(d, uiState.grid, navigationPage ? nav : mainBtns, 8, 4, selected, 6, 26, 55, 44, 3, ui::ORANGE);
}

void drawAcRemote() {
  auto& d = uiCanvas;
  const AcDevice& device = airConditioners[activeAc];
  const AcState& s = device.state;
  const uint16_t accent = s.power ? ui::CYAN : ui::MUTED;
  ui::header(d, device.name, ui::CYAN, uiStatus(ui::CYAN), s.power ? "ligado" : "desligado");

  // painel esquerdo: temperatura com arco
  ui::card(d, 6, 24, 92, 94, ui::SURFACE, 12);
  static ui::Smooth tempArc;
  tempArc.to((s.temp - 16) / 14.0f);
  const float k = tempArc.tick(0.25f, 0.003f);
  ui::arc(d, 52, 66, 36, 5, k, accent, ui::SURFACE3);
  ui::rollNumber(d, uiState.roll, s.temp, 50, 64, ui::F_BIG(), s.power ? ui::TEXT : ui::MUTED, 34);
  ui::font(d, ui::F_TINY());
  ui::text(d, "C", 72, 56, ui::MUTED, middle_left);
  ui::text(d, String(acModeName(s.mode)), 52, 84, accent, middle_center);
  // chips de estado
  int cx = 12;
  if (s.swing) { ui::chip(d, cx, 104, "swing", ui::tint(ui::SURFACE3, ui::CYAN, 0.4f), ui::TEXT, 4); cx += ui::chipWidth(d, "swing", 4) + 3; }
  if (s.turbo) { ui::chip(d, cx, 104, "turbo", ui::tint(ui::SURFACE3, ui::ORANGE, 0.5f), ui::TEXT, 4); cx += ui::chipWidth(d, "turbo", 4) + 3; }
  if (s.sleepMinutes) { ui::chip(d, cx, 104, sleepName(device), ui::tint(ui::SURFACE3, ui::INDIGO, 0.5f), ui::TEXT, 4); }

  // grade compacta a direita (2 x 4)
  const ui::GridButton btns[8] = {
    {ui::Icon::MINUS, "Temp", ""}, {ui::Icon::PLUS, "Temp", ""},
    {ui::Icon::SNOW, "Modo", String(acModeName(s.mode))}, {ui::Icon::FAN, "Vento", String(acFanName(s.fan))},
    {ui::Icon::SWING, "Swing", s.swing ? "on" : "off"}, {ui::Icon::BOLT, "Turbo", s.turbo ? "on" : "off"},
    {ui::Icon::MOON, "Sleep", sleepName(device)}, {ui::Icon::POWER, s.power ? "Desligar" : "Ligar", ""},
  };
  ui::grid(d, uiState.grid, btns, 8, 2, selected, 104, 24, 65, 21, 3, ui::CYAN, true);
}

// ---------------- TEAM PENNING ----------------
void drawTeamMenu() {
  ui::ListItem items[] = {
    {"Boiada", cattleSessionInitialized ? "faltam " + String(cattleRemainingCount()) : "bois sorteados", ui::Icon::FLAG},
    {trainingSession.active ? "Treino ativo" : "Novo treino", trainingSession.active ? "em andamento" : "cavalos e passadas", ui::Icon::TIMER, trainingSession.active ? ui::YELLOW : (uint16_t)0},
    {"Historico", "treinos salvos", ui::Icon::HISTORY},
  };
  drawListScreen("Team Penning", ui::GREEN, items, 3);
}

// Seletor numerico grande e centralizado.
inline void drawNumberPicker(const String& title, const String& caption, int value, const String& unit,
                             int minV, int maxV, uint16_t accent, ui::Icon ic) {
  auto& d = uiCanvas;
  ui::header(d, title, accent, uiStatus(accent));
  ui::card(d, 8, 25, 224, 92, ui::SURFACE, 12);
  ui::font(d, ui::F_TINY());
  ui::text(d, caption, 120, 36, ui::MUTED, middle_center);
  ui::icon(d, ui::Icon::LEFT, 40, 70, 16, ui::DIM);
  ui::icon(d, ui::Icon::RIGHT, 200, 70, 16, ui::DIM);
  ui::rollNumber(d, uiState.roll, value, 120 - (unit.length() ? 10 : 0), 68, ui::F_HUGE(), accent, 40);
  if (unit.length()) {
    ui::font(d, ui::F_BODY());
    ui::text(d, unit, 120 + 22, 76, ui::MUTED, middle_left);
  }
  d.fillSmoothCircle(56, 70, 3, ui::SURFACE3);
  // pontos de faixa
  const int n = maxV - minV + 1;
  if (n <= 12) ui::dots(d, 120, 100, n, value - minV, accent);
  else ui::bar(d, 60, 100, 120, 4, (value - minV) / (float)(n - 1), accent);
  ui::icon(d, ic, 24, 36, 12, accent);
}

void drawCattleLimit() {
  drawNumberPicker("Boiada", "Quantos bois na arena?", cattleMaxNumber + 1, "bois", 1, 10, ui::GREEN, ui::Icon::FLAG);
}

void drawCattleCounter() {
  auto& d = uiCanvas;
  const uint8_t remaining = cattleRemainingCount();
  String sub = "faltam " + String(remaining) + "/" + String(cattleMaxNumber + 1);
  if (remaining == 1) sub = "ultimo boi";
  else if (remaining == 0) sub = "boiada zerada";
  const uint16_t accent = remaining <= 1 ? ui::YELLOW : ui::GREEN;
  ui::header(d, "Boiada", ui::GREEN, uiStatus(ui::GREEN), sub);

  // painel esquerdo: boi da vez
  ui::card(d, 6, 24, 92, 94, ui::tint(ui::SURFACE, accent, 0.08f), 12);
  ui::font(d, ui::F_TINY());
  if (remaining == 0) {
    ui::text(d, "BOIADA", 52, 36, ui::MUTED, middle_center);
    ui::icon(d, ui::Icon::CHECK, 52, 66, 30, ui::GREEN);
    ui::text(d, "zerada", 52, 96, ui::GREEN, middle_center);
  } else {
    ui::text(d, remaining == 1 ? "ULTIMO BOI" : "BOI DA VEZ", 52, 36, ui::MUTED, middle_center);
    const float sc = uiState.bounce.scale();
    ui::rollNumber(d, uiState.roll, cattleSelectedNumber, 52, 66, ui::F_HUGE(), accent, 44);
    if (sc > 1.01f) d.drawCircle(52, 66, (int)(30 * sc), ui::blend(ui::BG, accent, 1.3f - sc));
    ui::font(d, ui::F_TINY());
    ui::text(d, remaining == 1 ? "A: reiniciar" : "A: marcar", 52, 100, ui::blend(ui::MUTED, accent, 0.5f), middle_center);
  }

  // grade da arena (2 x 5)
  ui::card(d, 104, 24, 130, 94, ui::SURFACE, 12);
  const int cw = 22, ch = 30, gap = 3, gx = 110, gy = 34;
  const int selCol = cattleSelectedNumber % 5, selRow = cattleSelectedNumber / 5;
  uiState.cellX.to(selCol); uiState.cellY.to(selRow);
  const float fx = uiState.cellX.tick(0.45f, 0.01f), fy = uiState.cellY.tick(0.45f, 0.01f);
  if (remaining > 0) {
    const int hx = gx + (int)(fx * (cw + gap)), hy = gy + (int)(fy * (ch + gap));
    d.fillSmoothRoundRect(hx - 2, hy - 2, cw + 4, ch + 4, 7, accent);
  }
  for (uint8_t i = 0; i <= min<uint8_t>(9, cattleMaxNumber); ++i) {
    const int x = gx + (i % 5) * (cw + gap), y = gy + (i / 5) * (ch + gap);
    const bool drawn = isCattleDrawn(i);
    const bool sel = i == cattleSelectedNumber && remaining > 0;
    d.fillSmoothRoundRect(x, y, cw, ch, 5, drawn ? ui::BG : (sel ? ui::tint(ui::SURFACE2, accent, 0.5f) : ui::SURFACE2));
    ui::font(d, ui::F_BODY());
    ui::text(d, String(i), x + cw / 2, y + ch / 2, drawn ? ui::DIM : (sel ? ui::TEXT : ui::MUTED), middle_center);
    if (drawn) d.drawWideLine(x + 4, y + ch / 2, x + cw - 4, y + ch / 2, 0.8f, ui::RED);
  }
  // barra de progresso da boiada
  const int total = cattleMaxNumber + 1;
  ui::bar(d, 110, 108, 118, 4, (total - remaining) / (float)total, ui::GREEN, ui::SURFACE3);
}

void drawCattleResetConfirm() {
  drawCattleCounter();
  ui::dialog(uiCanvas, uiState.enter, "Zerar a boiada?", "Todos os bois voltam a ficar disponiveis.", "Zerar", ui::YELLOW, ui::Icon::FLAG);
}

void drawTrainingCount() {
  drawNumberPicker("Novo treino", "Quantos cavalos vao treinar?", trainingSetupCount, trainingSetupCount == 1 ? "cavalo" : "cavalos",
                   1, MAX_TRAIN_HORSES, ui::GREEN, ui::Icon::TIMER);
}

void drawTrainingSelectHorse() {
  auto& d = uiCanvas;
  ui::header(d, "Escolha o cavalo", ui::GREEN, uiStatus(ui::GREEN), String(trainingSetupSlot + 1) + " de " + String(trainingSetupCount));
  ui::card(d, 8, 25, 224, 92, ui::SURFACE, 12);
  ui::font(d, ui::F_TINY());
  ui::text(d, "Posicao " + String(trainingSetupSlot + 1), 120, 36, ui::MUTED, middle_center);

  uiState.carousel.to(trainingSetupCandidate);
  const float pos = uiState.carousel.tick(0.35f, 0.004f);
  d.setClipRect(10, 46, 220, 44);
  for (int i = 0; i < MAX_TRAIN_HORSES; ++i) {
    float f = 1.0f - fabsf(pos - i);
    if (f < 0) f = 0;
    const int cx = 120 + (int)((i - pos) * 96);
    const bool taken = trainingHorseAlreadyChosen(i, trainingSetupSlot);
    ui::font(d, ui::F_BODY());
    const String name = TRAIN_HORSE_NAMES[i];
    const int w = d.textWidth(name) + 28 + (taken ? 14 : 0);
    const uint16_t fill = taken ? ui::SURFACE : ui::blend(ui::SURFACE2, ui::tint(ui::SURFACE3, ui::GREEN, 0.6f), f);
    d.fillSmoothRoundRect(cx - w / 2, 52, w, 32, 16, fill);
    if (f > 0.6f && !taken) d.drawRoundRect(cx - w / 2, 52, w, 32, 16, ui::blend(ui::SURFACE3, ui::GREEN, f));
    ui::text(d, name, cx - (taken ? 7 : 0), 68, taken ? ui::DIM : ui::blend(ui::MUTED, ui::TEXT, f), middle_center);
    if (taken) ui::icon(d, ui::Icon::CHECK, cx + w / 2 - 14, 68, 10, ui::DIM);
  }
  d.clearClipRect();
  ui::dots(d, 120, 100, MAX_TRAIN_HORSES, pos, ui::GREEN);
}

// Painel "cavalo + passadas" usado no treino ativo, resumo e historico.
inline void drawHorsePanel(M5Canvas& d, const char* horseName, int idx, int count, int passes, uint16_t accent,
                           const char* leftCaption, const char* rightCaption, bool bounce) {
  ui::card(d, 6, 24, 104, 94, ui::SURFACE, 12);
  ui::font(d, ui::F_TINY());
  ui::text(d, leftCaption, 58, 36, ui::MUTED, middle_center);
  d.fillSmoothCircle(58, 60, 16, ui::tint(ui::SURFACE3, accent, 0.3f));
  ui::icon(d, ui::Icon::HORSESHOE, 58, 60, 20, accent);
  ui::font(d, ui::F_BODY());
  ui::text(d, ui::fit(d, horseName, 96), 58, 88, ui::TEXT, middle_center);
  if (count > 1) ui::dots(d, 58, 104, count, idx, accent);

  ui::card(d, 116, 24, 118, 94, ui::tint(ui::SURFACE, accent, 0.06f), 12);
  ui::text(d, rightCaption, 175, 36, ui::MUTED, middle_center, ui::F_TINY());
  const float sc = bounce ? uiState.bounce.scale() : 1.0f;
  ui::rollNumber(d, uiState.roll2, passes, 175, 70, ui::F_HUGE(), accent, 44);
  if (sc > 1.01f) d.drawCircle(175, 70, (int)(34 * sc), ui::blend(ui::BG, accent, 1.3f - sc));
}

void drawTrainingActive() {
  auto& d = uiCanvas;
  const uint8_t i = trainingSession.currentHorse;
  ui::header(d, "Treino", ui::GREEN, uiStatus(ui::GREEN), String(i + 1) + "/" + String(trainingSession.horseCount));
  drawHorsePanel(d, TRAIN_HORSE_NAMES[trainingSession.horseIds[i]], i, trainingSession.horseCount,
                 trainingSession.passes[i], ui::GREEN, "CAVALO ATUAL", "PASSADAS", true);
  ui::font(d, ui::F_TINY());
  ui::text(d, "A +1   C -1", 175, 104, ui::blend(ui::MUTED, ui::GREEN, 0.5f), middle_center);
}

void drawTrainingEndConfirm() {
  drawTrainingActive();
  ui::dialog(uiCanvas, uiState.enter, "Encerrar treino?", "Os dados serao salvos no historico com a data de hoje.", "Salvar", ui::GREEN, ui::Icon::CHECK);
}

// Resumo com grafico de barras das passadas de cada cavalo.
inline void drawTrainingRecord(const TrainingRecord& r, uint8_t horse, const String& title, uint16_t accent) {
  auto& d = uiCanvas;
  if (!r.valid || !r.horseCount) {
    ui::header(d, title, accent, uiStatus(accent));
    drawEmptyState(d, ui::Icon::HISTORY, "Sem dados", "Nenhum treino registrado", accent);
    return;
  }
  const uint8_t i = min<uint8_t>(horse, r.horseCount - 1);
  ui::header(d, title, accent, uiStatus(accent), String(r.date));

  // esquerda: cavalo selecionado + total
  ui::card(d, 6, 24, 100, 94, ui::SURFACE, 12);
  ui::font(d, ui::F_TINY());
  ui::text(d, "CAVALO", 56, 36, ui::MUTED, middle_center);
  ui::font(d, ui::F_BODY());
  ui::text(d, ui::fit(d, TRAIN_HORSE_NAMES[r.horseIds[i]], 90), 56, 52, ui::TEXT, middle_center);
  ui::rollNumber(d, uiState.roll2, r.passes[i], 56, 80, ui::F_BIG(), accent, 34);
  ui::font(d, ui::F_TINY());
  ui::text(d, "passadas", 56, 104, ui::MUTED, middle_center);

  // direita: grafico
  ui::card(d, 112, 24, 122, 94, ui::SURFACE, 12);
  int maxP = 1, total = 0;
  for (uint8_t h = 0; h < r.horseCount; ++h) { maxP = max<int>(maxP, r.passes[h]); total += r.passes[h]; }
  ui::text(d, "TOTAL " + String(total), 173, 36, ui::MUTED, middle_center, ui::F_TINY());
  const int n = r.horseCount;
  const int bw = min(22, (104 - (n - 1) * 6) / n);
  const int x0 = 173 - (n * bw + (n - 1) * 6) / 2;
  for (uint8_t h = 0; h < n; ++h) {
    const int bh = max(3, (int)(52.0f * r.passes[h] / maxP));
    const int x = x0 + h * (bw + 6);
    const bool sel = h == i;
    d.fillSmoothRoundRect(x, 100 - bh, bw, bh, 3, sel ? accent : ui::tint(ui::SURFACE3, accent, 0.25f));
    ui::text(d, String(TRAIN_HORSE_NAMES[r.horseIds[h]]).substring(0, 3), x + bw / 2, 108, sel ? ui::TEXT : ui::DIM, middle_center, ui::F_TINY());
  }
}

void drawTrainingSummary() {
  drawTrainingRecord(trainingHistory[0], trainingSummaryHorse, "Treino salvo", ui::GREEN);
}

void drawTrainingHistory() {
  static ui::ListItem items[MAX_TRAIN_HISTORY];
  for (uint8_t i = 0; i < MAX_TRAIN_HISTORY; ++i) {
    const TrainingRecord& r = trainingHistory[i];
    items[i].title = r.valid ? "Treino " + String(i + 1) : "Vazio";
    items[i].value = r.valid ? String(r.date) + "  " + String(r.horseCount) + " cav" : "";
    items[i].ic = ui::Icon::HISTORY;
    items[i].iconColor = r.valid ? (uint16_t)0 : ui::DIM;
  }
  drawListScreen("Historico", ui::GREEN, items, MAX_TRAIN_HISTORY);
}

void drawTrainingHistoryDetail() {
  drawTrainingRecord(trainingHistory[trainingHistoryRecord], trainingHistoryHorse, "Treino " + String(trainingHistoryRecord + 1), ui::GREEN);
}

// ---------------- AJUSTES ----------------
void drawSettingsMenu() {
  ui::ListItem items[] = {
    {"Brilho", String((brightnessIndex + 1) * 20) + "%", ui::Icon::BRIGHT, ui::YELLOW},
    {"Relogio", clockIsValid() ? clockTimeText() : "sem hora", ui::Icon::CLOCK, ui::INDIGO},
    {"Descanso de tela", "3 + 10 min", ui::Icon::MOON, ui::VIOLET},
    {"Sons de toque", uiState.soundEnabled ? "ligado" : "desligado", ui::Icon::SOUND, ui::GREEN},
  };
  drawListScreen("Ajustes", ui::MUTED, items, 4);
}

void drawSettingsBrightness() {
  auto& d = uiCanvas;
  ui::header(d, "Brilho", ui::YELLOW, uiStatus(ui::YELLOW));
  ui::card(d, 8, 25, 224, 92, ui::SURFACE, 12);
  const int pct = (brightnessIndex + 1) * 20;
  static ui::Smooth level;
  level.to(pct / 100.0f);
  const float k = level.tick(0.3f, 0.003f);
  d.fillSmoothCircle(46, 62, 22, ui::blend(ui::SURFACE2, ui::YELLOW, 0.15f + 0.35f * k));
  ui::icon(d, ui::Icon::SUN, 46, 62, 12 + (int)(14 * k), ui::blend(ui::MUTED, ui::YELLOW, k));
  ui::rollNumber(d, uiState.roll, pct, 140, 56, ui::F_HUGE(), ui::TEXT, 40);
  ui::font(d, ui::F_BODY());
  ui::text(d, "%", 176, 62, ui::MUTED, middle_left);
  ui::bar(d, 84, 92, 130, 6, k, ui::YELLOW);
  for (int i = 0; i < 5; ++i) d.fillSmoothCircle(84 + 6 + i * (118 / 4), 95, 1, i <= brightnessIndex ? ui::BG : ui::DIM);
}

void drawSettingsSleep() {
  auto& d = uiCanvas;
  ui::header(d, "Descanso de tela", ui::VIOLET, uiStatus(ui::VIOLET));
  ui::card(d, 8, 25, 224, 42, ui::SURFACE, 10);
  ui::icon(d, ui::Icon::CLOCK, 28, 46, 16, ui::INDIGO);
  ui::font(d, ui::F_BODY());
  ui::text(d, "3 min sem uso", 48, 38, ui::TEXT, middle_left);
  ui::font(d, ui::F_TINY());
  ui::text(d, "Mostra o relogio com brilho baixo", 48, 55, ui::MUTED, middle_left);
  ui::card(d, 8, 72, 224, 42, ui::SURFACE, 10);
  ui::icon(d, ui::Icon::MOON, 28, 93, 16, ui::VIOLET);
  ui::font(d, ui::F_BODY());
  ui::text(d, "+10 min no relogio", 48, 85, ui::TEXT, middle_left);
  ui::font(d, ui::F_TINY());
  ui::text(d, "Tela apaga; qualquer botao acorda", 48, 102, ui::MUTED, middle_left);
}

// ---------------- RELOGIO / WATCHFACE ----------------
inline void drawWeatherGlyph(M5Canvas& d, int cx, int cy, int s, float temp, bool connected) {
  if (!connected && isnan(temp)) { ui::icon(d, ui::Icon::CLOUD, cx, cy, s, ui::DIM); return; }
  if (!isnan(temp) && temp >= 22.0f) ui::icon(d, ui::Icon::SUN, cx, cy, s, ui::YELLOW);
  else ui::icon(d, ui::Icon::CLOUD, cx, cy, s, ui::CYAN);
}

// styles: 0 AURORA, 1 MINIMAL, 2 PAINEL
void drawWatchface(M5Canvas& d, uint8_t style) {
  time_t now = time(nullptr);
  tm value;
  localtime_r(&now, &value);
  const bool valid = clockIsValid();
  const int sec = value.tm_sec;
  const bool wifi = WiFi.status() == WL_CONNECTED;
  const int battery = getBatteryLevelCached();
  const bool charging = isBatteryChargingCached();
  const String hhmm = valid ? clockTimeText() : "--:--";
  char secBuf[4];
  snprintf(secBuf, sizeof(secBuf), "%02d", sec);
  const String tempStr = isnan(weatherTemperature) ? "--" : String((int)roundf(weatherTemperature)) + "C";

  d.fillScreen(ui::BG);

  if (style == 0) {
    // AURORA: brilho colorido + hora grande a esquerda
    const float p = ui::pulse(6000);
    d.fillSmoothCircle(196, 44, 62, ui::blend(ui::BG, ui::INDIGO, 0.10f + 0.04f * p));
    d.fillSmoothCircle(212, 30, 40, ui::blend(ui::BG, ui::VIOLET, 0.12f));
    d.fillSmoothCircle(176, 66, 28, ui::blend(ui::BG, ui::CYAN, 0.10f));
    ui::font(d, ui::F_CLOCK());
    ui::text(d, hhmm, 8, 66, ui::TEXT, middle_left);
    const int tw = d.textWidth(hhmm);
    ui::font(d, ui::F_BODY());
    ui::text(d, valid ? String(secBuf) : String(""), 12 + tw, 76, ui::INDIGO, middle_left);
    ui::font(d, ui::F_BODY());
    ui::text(d, valid ? clockDayOfWeekText() + "  " + clockDateText().substring(0, 5) : "Sem hora", 10, 100, ui::MUTED, middle_left);
    // clima
    drawWeatherGlyph(d, 176, 100, 16, weatherTemperature, wifi);
    ui::font(d, ui::F_BODY());
    ui::text(d, tempStr, 190, 100, ui::TEXT, middle_left);
    ui::font(d, ui::F_TINY());
    ui::text(d, ui::fit(d, weatherCity.length() ? weatherCity : "", 90), 232, 116, ui::MUTED, middle_right);
    // status discreto
    ui::batteryPill(d, 236, 4, battery, charging);
    ui::wifiBars(d, 172, 5, wifi, ui::INDIGO);
    // linha de segundos
    ui::bar(d, 8, 124, 224, 3, sec / 59.0f, ui::INDIGO, ui::SURFACE2);
  } else if (style == 1) {
    // MINIMAL: hora centralizada, nada mais que o essencial
    ui::font(d, ui::F_CLOCK());
    ui::text(d, hhmm, 120, 58, ui::TEXT, middle_center);
    ui::font(d, ui::F_BODY());
    ui::text(d, valid ? clockDayOfWeekText() + ", " + clockDateText().substring(0, 5) : "Sem hora", 120, 92, ui::MUTED, middle_center);
    ui::arc(d, 120, 120, 40, 2, sec / 60.0f, ui::DIM, ui::SURFACE);
    d.fillRect(0, 122, 240, 13, ui::BG);
    ui::font(d, ui::F_TINY());
    ui::text(d, String(battery) + "%" + (charging ? "+" : ""), 120, 112, ui::DIM, middle_center);
  } else {
    // PAINEL: hora + dados
    ui::card(d, 6, 6, 148, 78, ui::SURFACE, 12);
    ui::font(d, ui::F_CLOCK());
    ui::text(d, hhmm, 80, 40, ui::TEXT, middle_center);
    ui::font(d, ui::F_TINY());
    ui::text(d, valid ? clockDayOfWeekText() + "  " + clockDateText() : "Sem hora", 80, 72, ui::MUTED, middle_center);
    // bateria em arco
    ui::card(d, 160, 6, 74, 78, ui::SURFACE, 12);
    const uint16_t bc = charging ? ui::CYAN : (battery <= 20 ? ui::RED : ui::GREEN);
    ui::arc(d, 197, 42, 26, 5, battery / 100.0f, bc, ui::SURFACE3);
    ui::font(d, ui::F_BODY());
    ui::text(d, String(battery), 197, 40, ui::TEXT, middle_center);
    ui::font(d, ui::F_TINY());
    ui::text(d, charging ? "carregando" : "bateria", 197, 76, ui::MUTED, middle_center);
    // linha inferior: wifi, clima, memoria
    ui::card(d, 6, 90, 228, 38, ui::SURFACE, 12);
    ui::wifiBars(d, 16, 102, wifi, ui::BLUE);
    ui::font(d, ui::F_TINY());
    ui::text(d, ui::fit(d, wifi ? WiFi.SSID() : "offline", 60), 34, 109, wifi ? ui::TEXT : ui::MUTED, middle_left);
    drawWeatherGlyph(d, 112, 109, 14, weatherTemperature, wifi);
    ui::text(d, tempStr, 124, 109, ui::TEXT, middle_left);
    ui::text(d, String(ESP.getFreeHeap() / 1024) + "k livre", 226, 109, ui::DIM, middle_right);
    ui::bar(d, 12, 122, 216, 2, sec / 59.0f, ui::INDIGO, ui::SURFACE3);
  }
}

void drawSettingsClock() {
  drawWatchface(uiCanvas, currentWatchfaceStyle);
}

// ---------------- AIR MOUSE ----------------
constexpr int MOUSE_RETICLE_X = 138;
constexpr int MOUSE_RETICLE_Y = 20;
constexpr int MOUSE_RETICLE_S = 96;
static M5Canvas mouseReticle(&M5.Display);
static bool mouseReticleReady = false;

inline void drawReticleInto(M5Canvas& c, int dotX, int dotY, uint16_t dotColor, float trail) {
  const int cx = MOUSE_RETICLE_S / 2, cy = MOUSE_RETICLE_S / 2;
  c.fillScreen(ui::BG);
  c.fillSmoothCircle(cx, cy, 44, ui::SURFACE);
  c.drawCircle(cx, cy, 44, ui::BORDER);
  c.drawCircle(cx, cy, 22, ui::SURFACE3);
  c.drawFastHLine(cx - 40, cy, 80, ui::SURFACE3);
  c.drawFastVLine(cx, cy - 40, 80, ui::SURFACE3);
  if (trail > 0.05f) c.drawWideLine(cx, cy, dotX, dotY, 1.2f, ui::blend(ui::SURFACE3, ui::CYAN, trail * 0.6f));
  c.fillSmoothCircle(dotX, dotY, 7, ui::blend(ui::BG, dotColor, 0.35f));
  c.fillSmoothCircle(dotX, dotY, 4, dotColor);
}

const char* mouseStatusText() {
  if (!M5.Imu.isEnabled()) return "Sensor indisponivel";
  if (!isMouseConnected()) return bleMouse.isConnected() ? "Conectando..." : "Pareie no Bluetooth";
  if (!mouseCalibrated) return "Mantenha parado";
  return "Pronto para usar";
}

void drawMouseScreen() {
  auto& d = uiCanvas;
  d.fillScreen(ui::BG);
  const bool ready = isMouseConnected() && mouseCalibrated;
  const uint16_t accent = ready ? ui::GREEN : ui::CYAN;
  ui::font(d, ui::F_TITLE());
  ui::text(d, "Air Mouse", 8, 12, ui::TEXT, middle_left);
  ui::icon(d, ui::Icon::BLUETOOTH, 118, 12, 13, isMouseConnected() ? ui::CYAN : ui::DIM);

  // estado (sem animacao continua: o loop do mouse e sensivel a latencia)
  d.fillSmoothRoundRect(6, 26, 122, 22, 11, ui::tint(ui::SURFACE2, accent, 0.25f));
  d.fillSmoothCircle(18, 37, 6, ui::blend(ui::BG, accent, 0.35f));
  d.fillSmoothCircle(18, 37, 3, accent);
  ui::font(d, ui::F_TINY());
  ui::text(d, mouseStatusText(), 30, 37, ui::TEXT, middle_left);

  // atalhos
  const struct { const char* k; const char* v; } rows[] = {
    {"A", "clique esquerdo"}, {"B", "clique direito"}, {"A seg.", "arrastar"}, {"C", "sair    C seg.: calibrar"},
  };
  for (int i = 0; i < 4; ++i) {
    const int y = 60 + i * 13;
    ui::font(d, ui::F_TINY());
    const int kw = d.textWidth(rows[i].k) + 6;
    d.fillSmoothRoundRect(8, y - 5, kw, 10, 3, ui::SURFACE3);
    ui::text(d, rows[i].k, 8 + kw / 2, y, ui::TEXT, middle_center);
    ui::text(d, rows[i].v, 8 + kw + 5, y, ui::MUTED, middle_left);
  }
  // barra de calibracao
  ui::bar(d, 8, 118, 120, 4, mouseCalibrated ? 1.0f : 0.0f, ui::GREEN, ui::SURFACE3);

  // reticulo estatico (o loop atualiza so o quadro do reticulo)
  if (!mouseReticleReady) {
    mouseReticle.setColorDepth(16);
    mouseReticle.setPsram(false);   // RAM interna: push rapido no caminho do mouse
    mouseReticle.createSprite(MOUSE_RETICLE_S, MOUSE_RETICLE_S);
    mouseReticleReady = true;
  }
  drawReticleInto(mouseReticle, MOUSE_RETICLE_S / 2, MOUSE_RETICLE_S / 2, accent, 0);
  mouseReticle.pushSprite(&d, MOUSE_RETICLE_X, MOUSE_RETICLE_Y);
  ui::font(d, ui::F_TINY());
  ui::text(d, "GIRO", MOUSE_RETICLE_X + MOUSE_RETICLE_S / 2, 124, ui::DIM, middle_center);
}

// Progresso de calibracao desenhado direto no display (caminho rapido do loop).
inline void drawMouseCalibrationBar(int pct) {
  const int w = 120 * constrain(pct, 0, 100) / 100;
  M5.Display.fillRect(8, 118, 120, 4, ui::SURFACE3);
  if (w > 0) M5.Display.fillRect(8, 118, w, 4, ui::CYAN);
}

void updateMouseSearchingDots() {
  // Compatibilidade: a tela inteira e redesenhada quando o estado muda.
  redraw = true;
}

void updateMouseCrosshair(float vx, float vy, uint16_t ballColor) {
  if (!mouseReticleReady) return;
  const int cx = MOUSE_RETICLE_S / 2, cy = MOUSE_RETICLE_S / 2;
  mouseDotX = constrain(cx + (int)(vx * 0.5f), cx - 38, cx + 38);
  mouseDotY = constrain(cy + (int)(vy * 0.5f), cy - 38, cy + 38);
  const float mag = ui::clamp01(sqrtf(vx * vx + vy * vy) / 60.0f);
  drawReticleInto(mouseReticle, mouseDotX, mouseDotY, ballColor == ui::YELLOW ? ui::YELLOW : ui::GREEN, mag);
  M5.Display.startWrite();
  mouseReticle.pushSprite(MOUSE_RETICLE_X, MOUSE_RETICLE_Y);
  M5.Display.endWrite();
}

// ---------------- ASSISTENTE DE VOZ ----------------
int countWrappedTextLines(int maxW, const String& text) {
  ui::font(uiCanvas, ui::F_BODY());
  return ui::wrappedLines(uiCanvas, maxW, text);
}

void drawWrappedTextCanvas(M5Canvas& c, int x, int y, int maxW, int maxLines, int startLine, const String& text, uint16_t color) {
  ui::font(c, ui::F_BODY());
  ui::wrapped(c, x, y, maxW, 16, maxLines, startLine, text, color);
}

void drawWrappedText(int x, int y, int maxW, int maxLines, int startLine, const String& text, uint16_t color) {
  drawWrappedTextCanvas(uiCanvas, x, y, maxW, maxLines, startLine, text, color);
}

constexpr int VOICE_TEXT_W = 216;

void drawVoiceAiScreen() {
  auto& d = uiCanvas;
  d.fillScreen(ui::BG);
  const uint16_t A = ui::VIOLET, B = ui::CYAN;
  const bool alexa = voiceInputMode == VoiceInputMode::ALEXA;

  // topo
  uint16_t dotCol = voiceBridgeConnected ? ui::GREEN : ui::RED;
  if (voxSpeechDetected) dotCol = ui::YELLOW;
  d.fillSmoothCircle(10, 10, 3, dotCol);
  ui::font(d, ui::F_TINY());
  ui::text(d, voiceState == VoiceState::RESULT ? "Resposta  -  " + voiceActiveAgent : String("Assistente"), 18, 10, ui::MUTED, middle_left);
  const int bat = getBatteryLevelCached();
  ui::batteryPill(d, 236, 4, bat, isBatteryChargingCached());
  int chipX = 236 - (26 + (int)String(bat).length() * 6) - 4;
  const char* modeTxt = alexa ? "EI M5" : "PTT";
  chipX -= ui::chipWidth(d, modeTxt);
  ui::chip(d, chipX, 4, modeTxt, ui::tint(ui::SURFACE2, alexa ? ui::GREEN : ui::CYAN, 0.35f), ui::TEXT);
  if (voiceState == VoiceState::RESULT && voxSpeechDetected) {
    chipX -= ui::chipWidth(d, "ouvindo") + 3;
    ui::chip(d, chipX, 4, "ouvindo", ui::tint(ui::SURFACE2, ui::YELLOW, 0.4f), ui::TEXT);
  }

  ui::Hint hints[3] = {{"A", "Falar"}, {"B", alexa ? "Modo PTT" : "Modo Ei M5"}, {"C", "Voltar"}};

  if (voiceState == VoiceState::RESULT) {
    ui::card(d, 6, 20, 228, 98, ui::SURFACE, 12);
    int y = 26;
    if (voiceTranscription.length()) {
      ui::font(d, ui::F_TINY());
      ui::text(d, ui::fit(d, "Voce: " + voiceTranscription, VOICE_TEXT_W), 14, y + 4, ui::YELLOW, middle_left);
      d.drawFastHLine(14, y + 12, VOICE_TEXT_W, ui::BORDER);
      y += 16;
    }
    const int maxLines = voiceTranscription.length() ? 4 : 5;
    ui::font(d, ui::F_BODY());
    const int total = ui::wrappedLines(d, VOICE_TEXT_W, voiceResultBody);
    ui::wrapped(d, 14, y, VOICE_TEXT_W, 16, maxLines, voiceScrollLine, voiceResultBody, ui::TEXT);
    if (total > maxLines) {
      const int trackY = 28, trackH = 84;
      d.fillSmoothRoundRect(229, trackY, 2, trackH, 1, ui::SURFACE3);
      const int th = max(10, trackH * maxLines / total);
      const int ty = trackY + (trackH - th) * voiceScrollLine / max(1, total - maxLines);
      d.fillSmoothRoundRect(229, ty, 2, th, 1, A);
      hints[1] = {"B", "Descer"};
      hints[2] = {"C", voiceScrollLine > 0 ? "Subir" : "Voltar"};
    }
    ui::footer(d, hints, 3, A);
    return;
  }

  // orbe a esquerda, conteudo a direita
  const int ox = 58, oy = 68;
  if (voiceState == VoiceState::IDLE) {
    ui::orb(d, ox, oy, 24, 0.0f, A, B, false);
    ui::font(d, ui::F_TITLE());
    ui::text(d, alexa ? "Diga \"Ei M5\"" : "Aperte A e fale", 112, 40, ui::TEXT, middle_left);
    ui::font(d, ui::F_TINY());
    ui::text(d, alexa ? "ou aperte A para falar" : "aperte A de novo para enviar", 112, 56, ui::MUTED, middle_left);
    const char* tips[] = {"\"desligue o ar\"", "\"volume mais alto\"", "\"toque jazz\""};
    for (int i = 0; i < 3; ++i) ui::chip(d, 112, 70 + i * 15, tips[i], ui::SURFACE2, ui::MUTED);
    if (!voiceBridgeConnected) ui::chip(d, 112, 70 + 45, "PC bridge offline", ui::tint(ui::SURFACE2, ui::RED, 0.35f), ui::TEXT);
  } else if (voiceState == VoiceState::LISTENING) {
    const float level = uiState.voiceLevel;
    ui::orb(d, ox, oy, 24, voxSpeechDetected ? 0.4f + 0.6f * level : 0.15f * level, voxSpeechDetected ? ui::GREEN : A, B, false);
    ui::font(d, ui::F_TITLE());
    ui::text(d, voxSpeechDetected ? "Ouvindo..." : (alexa ? "Aguardando \"Ei M5\"" : "Gravando..."), 112, 38, ui::TEXT, middle_left);
    ui::waveBars(d, 170, 70, 13, 3, 40, voxSpeechDetected ? level : level * 0.35f, millis(), A, B);
    if (voxSpeechDetected && voxSilenceStart > 0) {
      const float k = ui::clamp01((millis() - voxSilenceStart) / (float)VOX_SILENCE_COOLDOWN_MS);
      ui::bar(d, 112, 100, 116, 4, k, ui::YELLOW, ui::SURFACE3);
      ui::font(d, ui::F_TINY());
      ui::text(d, "enviando ao silenciar", 112, 110, ui::MUTED, middle_left);
    } else {
      ui::font(d, ui::F_TINY());
      ui::text(d, alexa ? "fale naturalmente" : "A envia  -  B cancela", 112, 104, ui::MUTED, middle_left);
    }
    hints[0] = {"A", "Enviar"};
    hints[1] = {"B", "Cancelar"};
  } else { // THINKING
    ui::orb(d, ox, oy, 22, 0.5f, B, A, true);
    ui::font(d, ui::F_TITLE());
    ui::text(d, "Pensando...", 112, 40, ui::TEXT, middle_left);
    if (voiceTranscription.length()) {
      ui::font(d, ui::F_TINY());
      ui::wrapped(d, 112, 54, 120, 10, 3, 0, "\"" + voiceTranscription + "\"", ui::YELLOW);
    }
    ui::font(d, ui::F_TINY());
    ui::text(d, "processando no PC", 112, 104, ui::MUTED, middle_left);
    // pontinhos animados
    for (int i = 0; i < 3; ++i) {
      const float p = ui::pulse(900, i * 200);
      d.fillSmoothCircle(214 + i * 7, 104, 2, ui::blend(ui::DIM, B, p));
    }
    hints[0] = {"A", "Falar"};
    hints[1] = {"B", "Aguarde"};
  }
  ui::footer(d, hints, 3, A);
}

// ---------------- TECLADO ----------------
// Visual do teclado modal (a logica de teclas continua em wifiKeyboard()).
inline void drawKeyboardFrame(const String& title, const String& text, bool masked, bool caps, int x, int y,
                              const char keys[4][12][2], uint16_t accent) {
  auto& d = uiCanvas;
  d.fillScreen(ui::BG);
  // acoes
  const char* actions[] = {"OK", caps ? "abc" : "ABC", "<x", "esp", "sair"};
  const ui::Icon actIcons[] = {ui::Icon::CHECK, ui::Icon::NONE, ui::Icon::BACK, ui::Icon::NONE, ui::Icon::CLOSE};
  const int aw = 44, ax0 = 4, ay = 3;
  for (int i = 0; i < 5; ++i) {
    const bool active = y == -1 && x == i;
    const int ax = ax0 + i * (aw + 3);
    d.fillSmoothRoundRect(ax, ay, aw, 17, 8, active ? accent : ui::SURFACE2);
    const uint16_t fg = active ? ui::BG : ui::TEXT;
    if (actIcons[i] != ui::Icon::NONE) {
      ui::icon(d, actIcons[i], ax + 12, ay + 8, 10, fg);
      ui::font(d, ui::F_TINY());
      ui::text(d, actions[i], ax + 21, ay + 8, fg, middle_left);
    } else {
      ui::font(d, ui::F_TINY());
      ui::text(d, actions[i], ax + aw / 2, ay + 8, fg, middle_center);
    }
  }
  // campo de texto
  d.fillSmoothRoundRect(4, 24, 232, 22, 6, ui::SURFACE);
  d.drawRoundRect(4, 24, 232, 22, 6, ui::blend(ui::BORDER, accent, 0.6f));
  ui::font(d, ui::F_TINY());
  ui::text(d, ui::fit(d, title, 120), 10, 30, ui::MUTED, middle_left);
  ui::text(d, String(text.length()) + "/63", 230, 30, ui::DIM, middle_right);
  String shown;
  if (masked) { for (size_t i = 0; i < text.length(); ++i) shown += '*'; }
  else shown = text;
  ui::font(d, ui::F_BODY());
  const int maxW = 200;
  while (shown.length() && d.textWidth(shown) > maxW) shown.remove(0, 1);
  ui::text(d, shown, 10, 40, ui::TEXT, middle_left);
  const int caretX = 10 + d.textWidth(shown) + 1;
  if ((millis() / 500) % 2 == 0) d.fillRect(caretX, 33, 2, 13, accent);
  // teclas
  const int keyW = 20, keyH = 20, sy = 52;
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 12; ++col) {
      const int kx = col * keyW, ky = sy + row * keyH;
      const bool active = y == row && x == col;
      if (active) d.fillSmoothRoundRect(kx + 1, ky + 1, keyW - 2, keyH - 2, 5, accent);
      else d.fillSmoothRoundRect(kx + 1, ky + 1, keyW - 2, keyH - 2, 5, ui::SURFACE);
      ui::font(d, ui::F_BODY());
      ui::text(d, String(keys[row][col][caps ? 1 : 0]), kx + keyW / 2, ky + keyH / 2, active ? ui::BG : ui::TEXT, middle_center);
    }
  }
  M5.Display.setRotation(3);
  d.pushSprite(0, 0);
}

// ---------------- BOOT / DESLIGAR ----------------
void showBootIntro() {
  ensureUiCanvas();
  auto& d = uiCanvas;
  M5.Display.setRotation(3);
  if (M5.Speaker.isEnabled()) {
    M5.Speaker.tone(1046, 40); delay(45);
    M5.Speaker.tone(1568, 60); delay(60);
    M5.Speaker.tone(2093, 110);
  }
  const uint32_t t0 = millis();
  const uint32_t total = 1500;
  while (true) {
    const uint32_t el = millis() - t0;
    if (el > total) break;
    d.fillScreen(ui::BG);
    const float k1 = ui::easeOutBack(ui::clamp01(el / 600.0f));            // orbe
    const float k2 = ui::easeOutCubic(ui::clamp01((el - 350.0f) / 500.0f)); // texto
    const float k3 = ui::clamp01((el - 700.0f) / 700.0f);                   // barra
    const int ox = 70 - (int)(0 * k2), oy = 62;
    if (k1 > 0) ui::orb(d, ox, oy, (int)(26 * k1), 0.3f, ui::VIOLET, ui::CYAN, false);
    if (k2 > 0) {
      const int tx = 112 + (int)((1 - k2) * 30);
      ui::font(d, ui::F_H2());
      ui::text(d, "M5", tx, 50, ui::blend(ui::BG, ui::TEXT, k2), middle_left);
      ui::font(d, ui::F_TITLE());
      ui::text(d, "PERSONAL", tx, 72, ui::blend(ui::BG, ui::INDIGO, k2), middle_left);
      ui::font(d, ui::F_TINY());
      ui::text(d, "mais que um stick", tx, 88, ui::blend(ui::BG, ui::MUTED, k2), middle_left);
    }
    if (k3 > 0) ui::bar(d, 60, 116, 120, 3, k3, ui::INDIGO, ui::SURFACE2);
    d.pushSprite(0, 0);
    delay(16);
  }
}

void showPowerOff() {
  ensureUiCanvas();
  auto& d = uiCanvas;
  const uint32_t t0 = millis();
  while (millis() - t0 < 420) {
    const float k = 1.0f - ui::easeOutCubic((millis() - t0) / 420.0f);
    d.fillScreen(ui::BG);
    ui::orb(d, 120, 58, (int)(22 * k) + 1, 0.0f, ui::VIOLET, ui::CYAN, false);
    ui::font(d, ui::F_BODY());
    ui::text(d, "Ate a proxima.", 120, 104, ui::blend(ui::BG, ui::MUTED, k), middle_center);
    d.pushSprite(0, 0);
    delay(16);
  }
  d.fillScreen(ui::BG);
  d.pushSprite(0, 0);
}

// ------------------------------------------------------------
// DESPACHO E COMPOSICAO DO QUADRO
// ------------------------------------------------------------
inline void drawScreenBody(Screen s) {
  switch (s) {
    case Screen::MAIN:                drawMain(); break;
    case Screen::WIFI_MENU:           drawWifiMenu(); break;
    case Screen::WIFI_SCANNING:       drawWifiScanning(); break;
    case Screen::WIFI_NETWORKS:       drawWifiNetworks(); break;
    case Screen::WIFI_KEYBOARD:       break;
    case Screen::WIFI_CONNECTING:     drawWifiConnecting(); break;
    case Screen::WIFI_RESULT:         drawWifiResult(); break;
    case Screen::WIFI_AP_INFO:        drawWifiApInfo(); break;
    case Screen::WIFI_WEBUI_NETWORK:  drawWifiWebUiNetwork(); break;
    case Screen::WIFI_SAVED_LIST:     drawWifiSavedList(); break;
    case Screen::WIFI_SAVED_DETAIL:   drawWifiSavedDetail(); break;
    case Screen::WIFI_DELETE_CONFIRM: drawWifiDeleteConfirm(); break;
    case Screen::IR_TYPES:            drawIrTypes(); break;
    case Screen::TV_LIST:             drawTvList(); break;
    case Screen::AC_LIST:             drawAcList(); break;
    case Screen::TV_REMOTE:           drawTvRemote(false); break;
    case Screen::TV_NAV:              drawTvRemote(true); break;
    case Screen::AC_REMOTE:           drawAcRemote(); break;
    case Screen::TEAM_MENU:           drawTeamMenu(); break;
    case Screen::TEAM_CATTLE_LIMIT:   drawCattleLimit(); break;
    case Screen::TEAM_CATTLE_COUNTER: drawCattleCounter(); break;
    case Screen::TEAM_CATTLE_RESET_CONFIRM: drawCattleResetConfirm(); break;
    case Screen::TEAM_TRAIN_COUNT:    drawTrainingCount(); break;
    case Screen::TEAM_TRAIN_SELECT_HORSE: drawTrainingSelectHorse(); break;
    case Screen::TEAM_TRAIN_ACTIVE:   drawTrainingActive(); break;
    case Screen::TEAM_TRAIN_END_CONFIRM: drawTrainingEndConfirm(); break;
    case Screen::TEAM_TRAIN_SUMMARY:  drawTrainingSummary(); break;
    case Screen::TEAM_TRAIN_HISTORY:  drawTrainingHistory(); break;
    case Screen::TEAM_TRAIN_HISTORY_DETAIL: drawTrainingHistoryDetail(); break;
    case Screen::SETTINGS_MENU:       drawSettingsMenu(); break;
    case Screen::SETTINGS_BRIGHTNESS: drawSettingsBrightness(); break;
    case Screen::SETTINGS_CLOCK:      drawSettingsClock(); break;
    case Screen::SETTINGS_SLEEP:      drawSettingsSleep(); break;
    case Screen::MOUSE:               drawMouseScreen(); break;
    case Screen::VOICE_AI:            drawVoiceAiScreen(); break;
  }
}

void drawScreen() {
  ensureUiCanvas();
  ui::frame().beginFrame();

  if (screen != uiState.lastScreen) {
    uiOnScreenChanged(uiState.lastScreen, screen);
    uiState.lastScreen = screen;
  }

  uiCanvas.setRotation(0);
  uiCanvas.fillScreen(ui::BG);
  drawScreenBody(screen);
  drawFooterFor(uiCanvas, screen, screenAccent(screen));
  ui::drawToast(uiCanvas);

  M5.Display.setRotation(3);
  if (uiState.transition.active) {
    const float t = uiState.transition.out();
    const int dir = uiState.transitionDir;
    M5.Display.startWrite();
    if (dir > 0) {
      // avanca: tela antiga recua com paralaxe, nova entra pela direita
      uiPrevCanvas.pushSprite(&M5.Display, (int)(-ui::W * 0.35f * t), 0);
      uiCanvas.pushSprite(&M5.Display, (int)(ui::W * (1.0f - t)), 0);
    } else {
      // volta: nova aparece por baixo (esquerda), antiga sai pela direita
      uiCanvas.pushSprite(&M5.Display, (int)(-ui::W * 0.35f * (1.0f - t)), 0);
      uiPrevCanvas.pushSprite(&M5.Display, (int)(ui::W * t), 0);
    }
    M5.Display.endWrite();
    ui::requestFrame();
  } else {
    uiCanvas.pushSprite(0, 0);
  }
  uiState.firstFrame = false;
  redraw = false;
}

// Relogio de descanso (screensaver): quadro completo, chamado a cada segundo.
void drawClockScreen(bool fullClear) {
  (void)fullClear;
  ensureUiCanvas();
  uiCanvas.setRotation(0);
  drawWatchface(uiCanvas, currentWatchfaceStyle);
  M5.Display.setRotation(3);
  uiCanvas.pushSprite(0, 0);
}
