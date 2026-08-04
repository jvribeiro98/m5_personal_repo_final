#include <M5Unified.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Samsung.h>
#include <ir_Midea.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>

// ============================================================
// M5 PERSONAL - MODULO IR v0.3 CORRIGIDO
// Hardware: M5StickC Plus2
// IR interno: GPIO 19
// Orientacao: rotation 3 (emissor IR fisicamente para cima)
//
// Botoes:
//   A curto  -> selecionar / executar
//   B curto  -> proximo item
//   B longo  -> voltar ao menu anterior
//   C curto  -> item anterior
//   C longo  -> desligar o M5
// ============================================================

constexpr uint8_t IR_PIN = 19;
constexpr uint8_t BTN_C_PIN = 35;
constexpr uint32_t BTN_C_DEBOUNCE_MS = 35;
constexpr uint32_t BTN_C_HOLD_MS = 1200;

// Cores RGB565.
constexpr uint16_t UI_BG       = 0x0000;
constexpr uint16_t UI_PANEL    = 0x18E3;
constexpr uint16_t UI_BORDER   = 0x4208;
constexpr uint16_t UI_SELECTED = 0x04FF;
constexpr uint16_t UI_TEXT     = 0xFFFF;
constexpr uint16_t UI_MUTED    = 0xAD55;
constexpr uint16_t UI_GREEN    = 0x07E0;
constexpr uint16_t UI_RED      = 0xF800;
constexpr uint16_t UI_YELLOW   = 0xFFE0;

// ============================================================
// TIPOS - ficam antes de qualquer funcao para evitar problemas
// do pre-processador automatico do Arduino.
// ============================================================

enum class Screen : uint8_t {
  MAIN,
  WIFI_MENU,
  WIFI_SCANNING,
  WIFI_NETWORKS,
  WIFI_KEYBOARD,
  WIFI_CONNECTING,
  WIFI_RESULT,
  WIFI_AP_INFO,
  WIFI_WEBUI_NETWORK,
  WIFI_SAVED_LIST,
  WIFI_SAVED_DETAIL,
  WIFI_DELETE_CONFIRM,
  IR_TYPES,
  TV_LIST,
  AC_LIST,
  TV_REMOTE,
  TV_NAV,
  AC_REMOTE
};

enum class TvProtocol : uint8_t {
  SAMSUNG_32,
  NEC_32
};

enum TvCommand : uint8_t {
  TV_POWER,
  TV_MUTE,
  TV_VOL_UP,
  TV_VOL_DOWN,
  TV_CH_UP,
  TV_CH_DOWN,
  TV_INPUT,
  TV_UP,
  TV_DOWN,
  TV_LEFT,
  TV_RIGHT,
  TV_OK,
  TV_BACK,
  TV_HOME,
  TV_MENU,
  TV_COMMAND_COUNT
};

struct TvDevice {
  const char* name;
  TvProtocol protocol;
  uint64_t code[TV_COMMAND_COUNT];
};

enum class AcBrand : uint8_t {
  SAMSUNG,
  MIDEA
};

enum class AcMode : uint8_t {
  AUTO,
  COOL,
  DRY,
  FAN,
  HEAT
};

// Nao usar LOW ou HIGH aqui: sao macros do Arduino.
enum class AcFan : uint8_t {
  AUTO,
  LOW_SPEED,
  MEDIUM_SPEED,
  HIGH_SPEED
};

struct AcState {
  bool power = false;
  uint8_t temp = 23;
  AcMode mode = AcMode::COOL;
  AcFan fan = AcFan::AUTO;
  bool swing = true;
  bool turbo = false;
  uint16_t sleepMinutes = 0;
};

struct AcDevice {
  const char* name;
  AcBrand brand;
  AcState state;
};


enum class SavedNetworkHealth : uint8_t {
  UNTESTED,
  VERIFIED,
  WARNING
};

enum class SavedNetworkFailure : uint8_t {
  NONE,
  AUTH_REJECTED,
  DHCP_FAILED,
  CONNECTION_TIMEOUT,
  UNKNOWN
};

struct SavedNetwork {
  String ssid;
  String password;
  SavedNetworkHealth health = SavedNetworkHealth::UNTESTED;
  SavedNetworkFailure failure = SavedNetworkFailure::NONE;
  int32_t lastRssi = -127;
};

enum class WifiEditMode : uint8_t {
  NEW_CONNECTION,
  EDIT_SSID,
  EDIT_PASSWORD
};

enum class WifiConnectSource : uint8_t {
  PHYSICAL,
  WEB_SETUP,
  AUTO_BOOT,
  AUTO_RECONNECT,
  SAVED_MANUAL,
  EDIT_VERIFY
};

enum class WebUiMode : uint8_t {
  OFF,
  SETUP_AP,
  LAN
};

struct ButtonCState {
  bool rawPressed = false;
  bool stablePressed = false;
  bool holdSent = false;
  bool clickEvent = false;
  bool holdEvent = false;
  uint32_t changedAt = 0;
  uint32_t pressedAt = 0;

  void begin();
  void update();
  bool wasClicked() const;
  bool wasHeld() const;
};

// ============================================================
// GLOBAIS
// ============================================================

Preferences prefs;
WebServer webServer(80);

constexpr uint8_t MAX_SAVED_NETWORKS = 10;
constexpr uint8_t MAX_SCANNED_NETWORKS = 20;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t WIFI_RETRY_SCAN_MS = 20000;
constexpr char WIFI_SETUP_SSID[] = "M5-PERSONAL-SETUP";
constexpr char WIFI_SETUP_PASSWORD[] = "12345678";

SavedNetwork savedNetworks[MAX_SAVED_NETWORKS];
uint8_t savedNetworkCount = 0;

String scannedSsids[MAX_SCANNED_NETWORKS];
int32_t scannedRssi[MAX_SCANNED_NETWORKS];
uint8_t scannedSavedIndex[MAX_SCANNED_NETWORKS];
uint8_t scannedNetworkCount = 0;

WifiEditMode wifiEditMode = WifiEditMode::NEW_CONNECTION;
WifiConnectSource wifiConnectSource = WifiConnectSource::PHYSICAL;
WebUiMode webUiMode = WebUiMode::OFF;

String wifiChosenSsid;
String wifiKeyboardText;
String wifiPendingSsid;
String wifiPendingPassword;
String wifiResultTitle;
String wifiResultDetail;
int8_t wifiEditingSavedIndex = -1;
int8_t wifiSelectedSavedIndex = -1;

bool wifiConnectPending = false;
bool wifiConnecting = false;
bool wifiAutoScanPending = true;
bool webServerRunning = false;
bool lanWebUiDesired = false;

bool pendingWebSaveWithoutTest = false;
bool pendingWebEdit = false;
int8_t pendingWebEditIndex = -1;
String pendingWebOriginalSsid;
String pendingWebOriginalPassword;

uint32_t wifiConnectStartedAt = 0;
uint32_t wifiLastRetryScanAt = 0;

IRsend tvIr(IR_PIN);
IRSamsungAc samsungAc(IR_PIN);
IRMideaAC mideaAc(IR_PIN);

Screen screen = Screen::MAIN;
uint8_t selected = 0;
uint8_t activeTv = 0;
uint8_t activeAc = 0;
bool redraw = true;

String toast;
uint32_t toastUntil = 0;
ButtonCState buttonC;

TvDevice televisions[] = {
  {
    "TV Samsung", TvProtocol::SAMSUNG_32,
    {
      0xE0E040BF, 0xE0E0F00F, 0xE0E0E01F, 0xE0E0D02F,
      0xE0E048B7, 0xE0E008F7, 0xE0E0807F, 0xE0E006F9,
      0xE0E08679, 0xE0E0A659, 0xE0E046B9, 0xE0E016E9,
      0xE0E01AE5, 0xE0E09E61, 0xE0E058A7
    }
  },
  {
    "TV TCL", TvProtocol::NEC_32,
    {
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0
    }
  },
  {
    "TV LG", TvProtocol::NEC_32,
    {
      0x20DF10EF, 0x20DF906F, 0x20DF40BF, 0x20DFC03F,
      0x20DF00FF, 0x20DF807F, 0x20DFD02F, 0x20DF02FD,
      0x20DF827D, 0x20DFE01F, 0x20DF609F, 0x20DF22DD,
      0x20DF14EB, 0x20DF3EC1, 0x20DFC23D
    }
  }
};

constexpr uint8_t TV_COUNT = sizeof(televisions) / sizeof(televisions[0]);

AcDevice airConditioners[] = {
  {"Ar Samsung", AcBrand::SAMSUNG, {}},
  {"Ar Midea", AcBrand::MIDEA, {}}
};

constexpr uint8_t AC_COUNT = sizeof(airConditioners) / sizeof(airConditioners[0]);
constexpr uint8_t AC_MENU_COUNT = 8;

// ============================================================
// PROTOTIPOS EXPLICITOS
// ============================================================


void loadSavedNetworks();
void saveSavedNetworks();
int8_t findSavedNetwork(const String& ssid);
bool upsertSavedNetwork(const String& ssid, const String& password);
void deleteSavedNetwork(uint8_t index);
void sortScannedNetworksByRssi();
void scanNetworksNow();
void beginWifiConnection(const String& ssid, const String& password, WifiConnectSource source);
void processWifiConnection();
void processWifiMaintenance();
void tryAutoConnectStrongest();
void startSetupAccessPoint();
void stopSetupAccessPoint();
void startLanWebUi();
void stopLanWebUi();
void syncWebUiState();
void configureWebRoutes();
void handleWebRoot();
void handleWebWifiPage();
void handleWebIrPage();
void handleWebApiScan();
void handleWebApiConnect();
void handleWebApiStatus();
void handleWebApiWebUiToggle();
void handleWebApiTv();
void handleWebApiAc();
void handleWebApiSavedNetworks();
void handleWebApiDeleteSaved();
void handleWebApiSaveNetwork();
void handleWebApiSavedDetail();
void markSavedNetworkSuccess(const String& ssid);
void markSavedNetworkFailure(const String& ssid, SavedNetworkFailure failure);
String savedNetworkFailureText(SavedNetworkFailure failure);
String savedNetworkHealthText(const SavedNetwork& network);
String wifiSignalClass(int32_t rssi);
String wifiSignalLabel(int32_t rssi);
String wifiKeyboard(const String& title, const String& initial, bool masked, bool& cancelled);
void drawWifiMenu();
void drawWifiScanning();
void drawWifiNetworks();
void drawWifiConnecting();
void drawWifiResult();
void drawWifiApInfo();
void drawWifiWebUiNetwork();
void drawWifiSavedList();
void drawWifiSavedDetail();
void drawWifiDeleteConfirm();

void showToast(const String& message, uint16_t duration = 900);
const char* tvCommandName(TvCommand command);
void sendTvCommand(TvCommand command);
const char* acModeName(AcMode mode);
const char* acFanName(AcFan fan);
String sleepName(const AcDevice& device);
String prefKey(uint8_t device, const char* suffix);
void saveAcState(uint8_t index);
void loadAcStates();
uint8_t samsungMode(AcMode mode);
uint8_t samsungFan(AcFan fan, AcMode mode);
uint8_t mideaMode(AcMode mode);
uint8_t mideaFan(AcFan fan);
void applySamsungState(const AcState& state);
void applyMideaState(const AcState& state);
void sendAcState(const String& message);
void toggleAcPower();
void toggleAcSwing();
void toggleAcTurbo();
void cycleAcMode();
void cycleAcFan();
void cycleAcSleep();
void executeAcAction(uint8_t action);
void drawFooter();
void drawTitle(const String& title, const String& subtitle = "");
void drawListItem(uint8_t index, int y, const String& label, const String& detail = "");
void drawGridButton(uint8_t index, int x, int y, int w, int h,
                    const String& label, const String& value = "");
void drawMain();
void drawWifiMenu();
void drawWifiScanning();
void drawWifiNetworks();
void drawWifiConnecting();
void drawWifiResult();
void drawWifiApInfo();
void drawWifiWebUiNetwork();
void drawWifiSavedList();
void drawWifiSavedDetail();
void drawWifiDeleteConfirm();
void drawIrTypes();
void drawTvList();
void drawAcList();
void drawTvRemote(bool navigationPage);
void drawAcRemote();
void drawScreen();
uint8_t itemCount();
void nextItem();
void previousItem();
void goBack();
void executeSelected();

// ============================================================
// BOTAO C
// ============================================================

void ButtonCState::begin() {
  pinMode(BTN_C_PIN, INPUT);
  rawPressed = digitalRead(BTN_C_PIN) == LOW;
  stablePressed = rawPressed;
  changedAt = millis();
}

void ButtonCState::update() {
  clickEvent = false;
  holdEvent = false;

  const bool nowPressed = digitalRead(BTN_C_PIN) == LOW;
  const uint32_t now = millis();

  if (nowPressed != rawPressed) {
    rawPressed = nowPressed;
    changedAt = now;
  }

  if (now - changedAt >= BTN_C_DEBOUNCE_MS && stablePressed != rawPressed) {
    stablePressed = rawPressed;

    if (stablePressed) {
      pressedAt = now;
      holdSent = false;
    } else if (!holdSent) {
      clickEvent = true;
    }
  }

  if (stablePressed && !holdSent && now - pressedAt >= BTN_C_HOLD_MS) {
    holdSent = true;
    holdEvent = true;
  }
}

bool ButtonCState::wasClicked() const { return clickEvent; }
bool ButtonCState::wasHeld() const { return holdEvent; }

// ============================================================
// TV
// ============================================================

void showToast(const String& message, uint16_t duration) {
  toast = message;
  toastUntil = millis() + duration;
  redraw = true;
}

const char* tvCommandName(TvCommand command) {
  static const char* names[TV_COMMAND_COUNT] = {
    "POWER", "MUDO", "VOL +", "VOL -", "CAN +", "CAN -", "INPUT",
    "CIMA", "BAIXO", "ESQ", "DIR", "OK", "VOLTAR", "HOME", "MENU"
  };

  const uint8_t index = static_cast<uint8_t>(command);
  return index < TV_COMMAND_COUNT ? names[index] : "?";
}

void sendTvCommand(TvCommand command) {
  const TvDevice& tv = televisions[activeTv];
  const uint8_t index = static_cast<uint8_t>(command);

  if (index >= TV_COMMAND_COUNT) {
    showToast("COMANDO INVALIDO", 1400);
    return;
  }

  const uint64_t code = tv.code[index];
  if (code == 0) {
    showToast("CODIGO PENDENTE", 1400);
    return;
  }

  switch (tv.protocol) {
    case TvProtocol::SAMSUNG_32:
      tvIr.sendSAMSUNG(code, 32, 0);
      break;
    case TvProtocol::NEC_32:
      tvIr.sendNEC(code, 32, 0);
      break;
  }

  showToast(tvCommandName(command));
  Serial.printf("TV=%s CMD=%s CODE=0x%08llX\n",
                tv.name, tvCommandName(command), code);
}

// ============================================================
// AR-CONDICIONADO
// ============================================================

const char* acModeName(AcMode mode) {
  switch (mode) {
    case AcMode::AUTO: return "AUTO";
    case AcMode::COOL: return "FRIO";
    case AcMode::DRY:  return "SECO";
    case AcMode::FAN:  return "VENT";
    case AcMode::HEAT: return "QUENTE";
  }
  return "?";
}

const char* acFanName(AcFan fan) {
  switch (fan) {
    case AcFan::AUTO:         return "AUTO";
    case AcFan::LOW_SPEED:    return "BAIXO";
    case AcFan::MEDIUM_SPEED: return "MEDIO";
    case AcFan::HIGH_SPEED:   return "ALTO";
  }
  return "?";
}

String sleepName(const AcDevice& device) {
  if (device.state.sleepMinutes == 0) return "OFF";
  if (device.brand == AcBrand::MIDEA) return "ON";
  return String(device.state.sleepMinutes / 60) + "H";
}

String prefKey(uint8_t device, const char* suffix) {
  return "a" + String(device) + suffix;
}

void saveAcState(uint8_t index) {
  if (index >= AC_COUNT) return;

  const AcState& state = airConditioners[index].state;
  prefs.putBool(prefKey(index, "p").c_str(), state.power);
  prefs.putUChar(prefKey(index, "t").c_str(), state.temp);
  prefs.putUChar(prefKey(index, "m").c_str(), static_cast<uint8_t>(state.mode));
  prefs.putUChar(prefKey(index, "f").c_str(), static_cast<uint8_t>(state.fan));
  prefs.putBool(prefKey(index, "s").c_str(), state.swing);
  prefs.putBool(prefKey(index, "u").c_str(), state.turbo);
  prefs.putUShort(prefKey(index, "z").c_str(), state.sleepMinutes);
}

void loadAcStates() {
  prefs.begin("m5-ir", false);

  for (uint8_t i = 0; i < AC_COUNT; i++) {
    AcState& state = airConditioners[i].state;
    state.power = prefs.getBool(prefKey(i, "p").c_str(), false);
    state.temp = prefs.getUChar(prefKey(i, "t").c_str(), 23);
    state.mode = static_cast<AcMode>(prefs.getUChar(prefKey(i, "m").c_str(), 1));
    state.fan = static_cast<AcFan>(prefs.getUChar(prefKey(i, "f").c_str(), 0));
    state.swing = prefs.getBool(prefKey(i, "s").c_str(), true);
    state.turbo = prefs.getBool(prefKey(i, "u").c_str(), false);
    state.sleepMinutes = prefs.getUShort(prefKey(i, "z").c_str(), 0);

    if (state.temp < 16 || state.temp > 30) state.temp = 23;

    if (static_cast<uint8_t>(state.mode) > static_cast<uint8_t>(AcMode::HEAT)) {
      state.mode = AcMode::COOL;
    }

    if (static_cast<uint8_t>(state.fan) > static_cast<uint8_t>(AcFan::HIGH_SPEED)) {
      state.fan = AcFan::AUTO;
    }
  }
}

uint8_t samsungMode(AcMode mode) {
  switch (mode) {
    case AcMode::AUTO: return kSamsungAcAuto;
    case AcMode::COOL: return kSamsungAcCool;
    case AcMode::DRY:  return kSamsungAcDry;
    case AcMode::FAN:  return kSamsungAcFan;
    case AcMode::HEAT: return kSamsungAcHeat;
  }
  return kSamsungAcCool;
}

uint8_t samsungFan(AcFan fan, AcMode mode) {
  if (mode == AcMode::AUTO) return kSamsungAcFanAuto2;
  if (mode == AcMode::DRY) return kSamsungAcFanAuto;

  switch (fan) {
    case AcFan::AUTO:         return kSamsungAcFanAuto;
    case AcFan::LOW_SPEED:    return kSamsungAcFanLow;
    case AcFan::MEDIUM_SPEED: return kSamsungAcFanMed;
    case AcFan::HIGH_SPEED:   return kSamsungAcFanHigh;
  }
  return kSamsungAcFanAuto;
}

uint8_t mideaMode(AcMode mode) {
  switch (mode) {
    case AcMode::AUTO: return kMideaACAuto;
    case AcMode::COOL: return kMideaACCool;
    case AcMode::DRY:  return kMideaACDry;
    case AcMode::FAN:  return kMideaACFan;
    case AcMode::HEAT: return kMideaACHeat;
  }
  return kMideaACCool;
}

uint8_t mideaFan(AcFan fan) {
  switch (fan) {
    case AcFan::AUTO:         return kMideaACFanAuto;
    case AcFan::LOW_SPEED:    return kMideaACFanLow;
    case AcFan::MEDIUM_SPEED: return kMideaACFanMed;
    case AcFan::HIGH_SPEED:   return kMideaACFanHigh;
  }
  return kMideaACFanAuto;
}

void applySamsungState(const AcState& state) {
  samsungAc.setPower(state.power);
  samsungAc.setMode(samsungMode(state.mode));
  samsungAc.setTemp(state.temp);
  samsungAc.setFan(samsungFan(state.fan, state.mode));
  samsungAc.setSwing(state.swing);
  samsungAc.setPowerful(state.turbo);
  samsungAc.setQuiet(false);
  samsungAc.setBreeze(false);
  samsungAc.setEcono(false);
  samsungAc.setClean(false);
  samsungAc.setIon(false);
  samsungAc.setBeep(false);
}

void applyMideaState(const AcState& state) {
  mideaAc.setUseCelsius(true);
  mideaAc.setPower(state.power);
  mideaAc.setMode(mideaMode(state.mode));
  mideaAc.setTemp(state.temp, true);
  mideaAc.setFan(mideaFan(state.fan));
  mideaAc.setSleep(state.sleepMinutes > 0);
  mideaAc.setQuiet(false);
  mideaAc.setSwingVToggle(false);
  mideaAc.setTurboToggle(false);
}

void sendAcState(const String& message) {
  AcDevice& device = airConditioners[activeAc];
  AcState& state = device.state;

  if (device.brand == AcBrand::SAMSUNG) {
    applySamsungState(state);
    samsungAc.send();
    Serial.println(samsungAc.toString());
  } else {
    applyMideaState(state);
    mideaAc.send();
    Serial.println(mideaAc.toString());
  }

  saveAcState(activeAc);
  showToast(message);
}

void toggleAcPower() {
  AcDevice& device = airConditioners[activeAc];
  AcState& state = device.state;
  state.power = !state.power;

  if (device.brand == AcBrand::SAMSUNG) {
    applySamsungState(state);

    if (state.power) {
      samsungAc.sendOn();
      delay(120);
      applySamsungState(state);
      samsungAc.send();
    } else {
      samsungAc.sendOff();
    }
  } else {
    applyMideaState(state);
    mideaAc.send();
  }

  saveAcState(activeAc);
  showToast(state.power ? "LIGANDO" : "DESLIGANDO");
}

void toggleAcSwing() {
  AcDevice& device = airConditioners[activeAc];
  AcState& state = device.state;
  state.swing = !state.swing;
  state.power = true;

  if (device.brand == AcBrand::MIDEA) {
    applyMideaState(state);
    mideaAc.setSwingVToggle(true);
    mideaAc.send();
    mideaAc.setSwingVToggle(false);
    saveAcState(activeAc);
    showToast(state.swing ? "SWING ON" : "SWING OFF");
  } else {
    sendAcState(state.swing ? "SWING ON" : "SWING OFF");
  }
}

void toggleAcTurbo() {
  AcDevice& device = airConditioners[activeAc];
  AcState& state = device.state;
  state.turbo = !state.turbo;
  state.power = true;

  if (device.brand == AcBrand::MIDEA) {
    applyMideaState(state);
    mideaAc.setTurboToggle(true);
    mideaAc.send();
    mideaAc.setTurboToggle(false);
    saveAcState(activeAc);
    showToast(state.turbo ? "TURBO ON" : "TURBO OFF");
  } else {
    sendAcState(state.turbo ? "TURBO ON" : "TURBO OFF");
  }
}

void cycleAcMode() {
  AcState& state = airConditioners[activeAc].state;
  const uint8_t next = (static_cast<uint8_t>(state.mode) + 1) % 5;
  state.mode = static_cast<AcMode>(next);
  state.power = true;
  state.turbo = false;

  if (state.mode == AcMode::AUTO || state.mode == AcMode::DRY) {
    state.fan = AcFan::AUTO;
  }

  sendAcState("MODO " + String(acModeName(state.mode)));
}

void cycleAcFan() {
  AcState& state = airConditioners[activeAc].state;

  if (state.mode == AcMode::AUTO || state.mode == AcMode::DRY) {
    state.fan = AcFan::AUTO;
    showToast("FAN AUTOMATICO", 1200);
    return;
  }

  const uint8_t next = (static_cast<uint8_t>(state.fan) + 1) % 4;
  state.fan = static_cast<AcFan>(next);
  state.power = true;
  state.turbo = false;
  sendAcState("FAN " + String(acFanName(state.fan)));
}

void cycleAcSleep() {
  AcDevice& device = airConditioners[activeAc];
  AcState& state = device.state;
  state.power = true;

  if (device.brand == AcBrand::MIDEA) {
    state.sleepMinutes = state.sleepMinutes ? 0 : 60;
    sendAcState(String("SLEEP ") + (state.sleepMinutes ? "ON" : "OFF"));
    return;
  }

  switch (state.sleepMinutes) {
    case 0:   state.sleepMinutes = 60;  break;
    case 60:  state.sleepMinutes = 120; break;
    case 120: state.sleepMinutes = 240; break;
    default:  state.sleepMinutes = 0;   break;
  }

  applySamsungState(state);
  samsungAc.setSleepTimer(state.sleepMinutes);
  samsungAc.sendExtended();
  saveAcState(activeAc);
  showToast("SLEEP " + sleepName(device));
}

void executeAcAction(uint8_t action) {
  AcState& state = airConditioners[activeAc].state;

  switch (action) {
    case 0:
      if (state.temp > 16) state.temp--;
      state.power = true;
      sendAcState("TEMP -");
      break;
    case 1:
      if (state.temp < 30) state.temp++;
      state.power = true;
      sendAcState("TEMP +");
      break;
    case 2: cycleAcMode(); break;
    case 3: cycleAcFan(); break;
    case 4: toggleAcSwing(); break;
    case 5: toggleAcTurbo(); break;
    case 6: cycleAcSleep(); break;
    case 7: toggleAcPower(); break;
    default: break;
  }
}


// ============================================================
// WIFI / REDES SALVAS / WEB UI
// ============================================================

String wifiPrefKey(uint8_t index, const char* suffix) {
  return "n" + String(index) + suffix;
}

void loadSavedNetworks() {
  prefs.begin("wifi-nets", true);
  savedNetworkCount = prefs.getUChar("count", 0);
  if (savedNetworkCount > MAX_SAVED_NETWORKS) savedNetworkCount = MAX_SAVED_NETWORKS;

  for (uint8_t i = 0; i < savedNetworkCount; i++) {
    savedNetworks[i].ssid = prefs.getString(wifiPrefKey(i, "s").c_str(), "");
    savedNetworks[i].password = prefs.getString(wifiPrefKey(i, "p").c_str(), "");
    savedNetworks[i].health = static_cast<SavedNetworkHealth>(
      prefs.getUChar(wifiPrefKey(i, "h").c_str(), static_cast<uint8_t>(SavedNetworkHealth::UNTESTED))
    );
    savedNetworks[i].failure = static_cast<SavedNetworkFailure>(
      prefs.getUChar(wifiPrefKey(i, "f").c_str(), static_cast<uint8_t>(SavedNetworkFailure::NONE))
    );
    savedNetworks[i].lastRssi = prefs.getInt(wifiPrefKey(i, "r").c_str(), -127);
  }
  prefs.end();

  // Compacta entradas vazias para evitar buracos.
  uint8_t writeIndex = 0;
  for (uint8_t readIndex = 0; readIndex < savedNetworkCount; readIndex++) {
    if (savedNetworks[readIndex].ssid.length()) {
      if (writeIndex != readIndex) savedNetworks[writeIndex] = savedNetworks[readIndex];
      writeIndex++;
    }
  }
  savedNetworkCount = writeIndex;

  prefs.begin("wifi-cfg", true);
  lanWebUiDesired = prefs.getBool("webui", false);
  prefs.end();
}

void saveSavedNetworks() {
  prefs.begin("wifi-nets", false);
  prefs.clear();
  prefs.putUChar("count", savedNetworkCount);

  for (uint8_t i = 0; i < savedNetworkCount; i++) {
    prefs.putString(wifiPrefKey(i, "s").c_str(), savedNetworks[i].ssid);
    prefs.putString(wifiPrefKey(i, "p").c_str(), savedNetworks[i].password);
    prefs.putUChar(wifiPrefKey(i, "h").c_str(), static_cast<uint8_t>(savedNetworks[i].health));
    prefs.putUChar(wifiPrefKey(i, "f").c_str(), static_cast<uint8_t>(savedNetworks[i].failure));
    prefs.putInt(wifiPrefKey(i, "r").c_str(), savedNetworks[i].lastRssi);
  }
  prefs.end();
}

void saveWebUiPreference() {
  prefs.begin("wifi-cfg", false);
  prefs.putBool("webui", lanWebUiDesired);
  prefs.end();
}

int8_t findSavedNetwork(const String& ssid) {
  for (uint8_t i = 0; i < savedNetworkCount; i++) {
    if (savedNetworks[i].ssid == ssid) return static_cast<int8_t>(i);
  }
  return -1;
}

bool upsertSavedNetwork(const String& ssid, const String& password) {
  if (!ssid.length()) return false;

  int8_t existing = findSavedNetwork(ssid);
  if (existing >= 0) {
    savedNetworks[existing].password = password;
    savedNetworks[existing].health = SavedNetworkHealth::UNTESTED;
    savedNetworks[existing].failure = SavedNetworkFailure::NONE;
    saveSavedNetworks();
    return true;
  }

  if (savedNetworkCount >= MAX_SAVED_NETWORKS) return false;

  savedNetworks[savedNetworkCount].ssid = ssid;
  savedNetworks[savedNetworkCount].password = password;
  savedNetworks[savedNetworkCount].health = SavedNetworkHealth::UNTESTED;
  savedNetworks[savedNetworkCount].failure = SavedNetworkFailure::NONE;
  savedNetworks[savedNetworkCount].lastRssi = -127;
  savedNetworkCount++;
  saveSavedNetworks();
  return true;
}

void deleteSavedNetwork(uint8_t index) {
  if (index >= savedNetworkCount) return;

  const bool deletingCurrent =
      WiFi.status() == WL_CONNECTED && WiFi.SSID() == savedNetworks[index].ssid;

  for (uint8_t i = index; i + 1 < savedNetworkCount; i++) {
    savedNetworks[i] = savedNetworks[i + 1];
  }

  if (savedNetworkCount) savedNetworkCount--;
  savedNetworks[savedNetworkCount].ssid = "";
  savedNetworks[savedNetworkCount].password = "";
  saveSavedNetworks();

  if (deletingCurrent) {
    WiFi.disconnect(true, false);
    wifiAutoScanPending = true;
  }
}


void markSavedNetworkSuccess(const String& ssid) {
  int8_t index = findSavedNetwork(ssid);
  if (index < 0) return;
  savedNetworks[index].health = SavedNetworkHealth::VERIFIED;
  savedNetworks[index].failure = SavedNetworkFailure::NONE;
  savedNetworks[index].lastRssi = WiFi.RSSI();
  saveSavedNetworks();
}

void markSavedNetworkFailure(const String& ssid, SavedNetworkFailure failure) {
  int8_t index = findSavedNetwork(ssid);
  if (index < 0) return;

  // Rede fora do alcance não é erro e nunca recebe triângulo.
  if (failure == SavedNetworkFailure::NONE) return;

  savedNetworks[index].health = SavedNetworkHealth::WARNING;
  savedNetworks[index].failure = failure;
  saveSavedNetworks();
}

String savedNetworkFailureText(SavedNetworkFailure failure) {
  switch (failure) {
    case SavedNetworkFailure::AUTH_REJECTED:
      return "A autenticação foi rejeitada. Confira a senha.";
    case SavedNetworkFailure::DHCP_FAILED:
      return "A rede aceitou a conexão, mas o M5 não recebeu um endereço IP.";
    case SavedNetworkFailure::CONNECTION_TIMEOUT:
      return "A rede estava visível, mas a tentativa não terminou dentro do tempo esperado.";
    case SavedNetworkFailure::UNKNOWN:
      return "A última tentativa falhou, mas o M5 não conseguiu identificar a causa.";
    default:
      return "";
  }
}

String savedNetworkHealthText(const SavedNetwork& network) {
  if (network.health == SavedNetworkHealth::VERIFIED) return "verified";
  if (network.health == SavedNetworkHealth::WARNING) return "warning";
  return "untested";
}

String wifiSignalClass(int32_t rssi) {
  if (rssi >= -60) return "good";
  if (rssi >= -75) return "medium";
  return "weak";
}

String wifiSignalLabel(int32_t rssi) {
  if (rssi >= -60) return "Boa";
  if (rssi >= -75) return "Média";
  return "Fraca";
}

void sortScannedNetworksByRssi() {
  for (uint8_t i = 0; i < scannedNetworkCount; i++) {
    for (uint8_t j = i + 1; j < scannedNetworkCount; j++) {
      if (scannedRssi[j] > scannedRssi[i]) {
        String ssidTmp = scannedSsids[i];
        scannedSsids[i] = scannedSsids[j];
        scannedSsids[j] = ssidTmp;

        int32_t rssiTmp = scannedRssi[i];
        scannedRssi[i] = scannedRssi[j];
        scannedRssi[j] = rssiTmp;

        uint8_t savedTmp = scannedSavedIndex[i];
        scannedSavedIndex[i] = scannedSavedIndex[j];
        scannedSavedIndex[j] = savedTmp;
      }
    }
  }
}

void scanNetworksNow() {
  const Screen returnScreen = screen;
  screen = Screen::WIFI_SCANNING;
  redraw = true;
  drawScreen();

  if (webUiMode == WebUiMode::SETUP_AP) {
    WiFi.mode(WIFI_AP_STA);
  } else {
    WiFi.mode(WIFI_STA);
  }

  const int found = WiFi.scanNetworks(false, true);
  scannedNetworkCount = 0;

  if (found > 0) {
    for (int i = 0; i < found && scannedNetworkCount < MAX_SCANNED_NETWORKS; i++) {
      String ssid = WiFi.SSID(i);
      if (!ssid.length()) continue;

      bool duplicate = false;
      for (uint8_t j = 0; j < scannedNetworkCount; j++) {
        if (scannedSsids[j] == ssid) {
          duplicate = true;
          if (WiFi.RSSI(i) > scannedRssi[j]) scannedRssi[j] = WiFi.RSSI(i);
          break;
        }
      }
      if (duplicate) continue;

      scannedSsids[scannedNetworkCount] = ssid;
      scannedRssi[scannedNetworkCount] = WiFi.RSSI(i);
      int8_t saved = findSavedNetwork(ssid);
      scannedSavedIndex[scannedNetworkCount] = saved >= 0 ? static_cast<uint8_t>(saved) : 255;
      scannedNetworkCount++;
    }
  }

  WiFi.scanDelete();
  sortScannedNetworksByRssi();

  if (returnScreen == Screen::WIFI_MENU || returnScreen == Screen::WIFI_SCANNING) {
    screen = Screen::WIFI_NETWORKS;
    selected = 0;
  } else {
    screen = returnScreen;
  }
  redraw = true;
}

void beginWifiConnection(const String& ssid, const String& password, WifiConnectSource source) {
  wifiPendingSsid = ssid;
  wifiPendingPassword = password;
  wifiConnectSource = source;
  wifiConnectPending = true;
}


void processWifiConnection() {
  if (wifiConnectPending && !wifiConnecting) {
    wifiConnectPending = false;
    wifiConnecting = true;
    wifiConnectStartedAt = millis();

    if (webUiMode == WebUiMode::SETUP_AP) WiFi.mode(WIFI_AP_STA);
    else WiFi.mode(WIFI_STA);

    WiFi.begin(wifiPendingSsid.c_str(), wifiPendingPassword.c_str());
    screen = Screen::WIFI_CONNECTING;
    redraw = true;
  }

  if (!wifiConnecting) return;

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnecting = false;
    bool savedOk = true;

    if (wifiConnectSource == WifiConnectSource::EDIT_VERIFY &&
        wifiEditingSavedIndex >= 0 &&
        wifiEditingSavedIndex < savedNetworkCount) {
      SavedNetwork replacement;
      replacement.ssid = wifiPendingSsid;
      replacement.password = wifiPendingPassword;
      replacement.health = SavedNetworkHealth::VERIFIED;
      replacement.failure = SavedNetworkFailure::NONE;
      replacement.lastRssi = WiFi.RSSI();
      savedNetworks[wifiEditingSavedIndex] = replacement;
      saveSavedNetworks();
      wifiEditingSavedIndex = -1;
    } else {
      int8_t existing = findSavedNetwork(wifiPendingSsid);
      if (existing >= 0) {
        savedNetworks[existing].password = wifiPendingPassword;
        savedNetworks[existing].health = SavedNetworkHealth::VERIFIED;
        savedNetworks[existing].failure = SavedNetworkFailure::NONE;
        savedNetworks[existing].lastRssi = WiFi.RSSI();
        saveSavedNetworks();
      } else {
        savedOk = upsertSavedNetwork(wifiPendingSsid, wifiPendingPassword);
        markSavedNetworkSuccess(wifiPendingSsid);
      }
    }

    wifiResultTitle = "CONECTADO";
    wifiResultDetail = wifiPendingSsid;

    if (wifiConnectSource == WifiConnectSource::WEB_SETUP) {
      lanWebUiDesired = true;
      saveWebUiPreference();
    }

    stopSetupAccessPoint();
    syncWebUiState();

    if (!savedOk) showToast("LIMITE DE 10 REDES", 1800);

    screen = Screen::WIFI_RESULT;
    redraw = true;
    return;
  }

  if (millis() - wifiConnectStartedAt >= WIFI_CONNECT_TIMEOUT_MS) {
    wifiConnecting = false;

    const wl_status_t finalStatus = WiFi.status();
    const bool networkWasVisible = findSavedNetwork(wifiPendingSsid) >= 0 ||
                                   wifiConnectSource == WifiConnectSource::EDIT_VERIFY ||
                                   wifiConnectSource == WifiConnectSource::WEB_SETUP ||
                                   wifiConnectSource == WifiConnectSource::PHYSICAL;

    SavedNetworkFailure failure = SavedNetworkFailure::UNKNOWN;
    if (finalStatus == WL_CONNECT_FAILED) failure = SavedNetworkFailure::AUTH_REJECTED;
    else if (finalStatus == WL_NO_SSID_AVAIL) failure = SavedNetworkFailure::NONE;
    else if (finalStatus == WL_IDLE_STATUS) failure = SavedNetworkFailure::CONNECTION_TIMEOUT;

    WiFi.disconnect(false, false);

    // Ausência no scan / fora de alcance não vira alerta.
    if (networkWasVisible && failure != SavedNetworkFailure::NONE) {
      markSavedNetworkFailure(wifiPendingSsid, failure);
    }

    wifiResultTitle = "FALHA";
    wifiResultDetail = failure == SavedNetworkFailure::NONE ? "REDE INDISPONIVEL" : "VERIFIQUE A REDE";
    wifiEditingSavedIndex = -1;
    pendingWebEdit = false;
    pendingWebEditIndex = -1;
    screen = Screen::WIFI_RESULT;
    redraw = true;
  }
}


void tryAutoConnectStrongest() {
  if (!savedNetworkCount || wifiConnecting || WiFi.status() == WL_CONNECTED) return;

  WiFi.mode(WIFI_STA);
  int found = WiFi.scanNetworks(false, true);

  int8_t bestSaved = -1;
  int32_t bestRssi = -1000;

  if (found > 0) {
    for (int i = 0; i < found; i++) {
      int8_t saved = findSavedNetwork(WiFi.SSID(i));
      if (saved >= 0 && WiFi.RSSI(i) > bestRssi) {
        bestRssi = WiFi.RSSI(i);
        bestSaved = saved;
      }
    }
  }

  WiFi.scanDelete();

  if (bestSaved >= 0) {
    beginWifiConnection(
      savedNetworks[bestSaved].ssid,
      savedNetworks[bestSaved].password,
      WifiConnectSource::AUTO_RECONNECT
    );
  }
}

void processWifiMaintenance() {
  if (wifiAutoScanPending && !wifiConnecting) {
    wifiAutoScanPending = false;
    tryAutoConnectStrongest();
    wifiLastRetryScanAt = millis();
  }

  if (WiFi.status() != WL_CONNECTED && !wifiConnecting &&
      millis() - wifiLastRetryScanAt >= WIFI_RETRY_SCAN_MS) {
    wifiLastRetryScanAt = millis();
    tryAutoConnectStrongest();
  }

  syncWebUiState();
}

String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  value.replace("\"", "&quot;");
  return value;
}

String jsonEscape(String value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  value.replace("\n", "\\n");
  return value;
}


String webShell(const String& title, const String& body) {
  return String(F(
R"HTML(<!doctype html><html lang="pt-BR"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0a0d13"><title>M5 Personal</title>
<style>
:root{color-scheme:dark;--bg:#090c12;--panel:#121824;--panel2:#181f2d;--line:#283247;--text:#f6f7fb;--muted:#98a3b8;--accent:#8b5cf6;--accent2:#4f46e5;--green:#2dd47b;--yellow:#f7c948;--red:#ff5d68}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at top,#171126 0,#090c12 42%);color:var(--text);font:15px system-ui,-apple-system,Segoe UI,sans-serif;min-height:100vh}
main{width:min(820px,100%);margin:auto;padding:14px 14px 34px}.topbar{height:54px;display:grid;grid-template-columns:90px 1fr 90px;align-items:center;position:sticky;top:0;z-index:20;background:rgba(9,12,18,.9);backdrop-filter:blur(12px)}
.topbar h1{font-size:18px;text-align:center;margin:0}.nav{border:0;background:transparent;color:var(--muted);padding:10px;text-decoration:none;font-weight:700}.nav.right{text-align:right}.nav.disabled{opacity:.25;pointer-events:none}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}.card{background:linear-gradient(145deg,var(--panel2),var(--panel));border:1px solid var(--line);border-radius:18px;padding:16px;box-shadow:0 12px 28px rgba(0,0,0,.18)}
.tile{min-height:126px;display:flex;flex-direction:column;justify-content:space-between;text-decoration:none;color:var(--text);transition:.16s}.tile:active{transform:scale(.98)}
.icon{width:48px;height:48px;border-radius:15px;display:grid;place-items:center;background:rgba(139,92,246,.16);color:#bda7ff}.icon svg{width:27px;height:27px}.tile h2,.card h2{font-size:17px;margin:12px 0 3px}.sub{color:var(--muted);font-size:13px}
.row{display:flex;align-items:center;justify-content:space-between;gap:12px}.stack{display:grid;gap:10px}.btn{border:1px solid var(--line);background:#20283a;color:var(--text);border-radius:13px;padding:12px 14px;font-weight:750;text-align:center;text-decoration:none;cursor:pointer}.btn.primary{background:linear-gradient(135deg,var(--accent),var(--accent2));border-color:transparent}.btn.danger{background:rgba(255,93,104,.12);color:#ff9aa2}.btn.small{padding:8px 11px;font-size:12px}
input,select{width:100%;background:#0f1420;color:var(--text);border:1px solid var(--line);border-radius:12px;padding:12px}.field{display:grid;gap:6px}.field label{font-size:12px;color:var(--muted)}
.signal{display:flex;align-items:center;gap:8px;font-weight:800}.bars{display:flex;align-items:flex-end;gap:2px;height:20px}.bars i{display:block;width:4px;border-radius:3px;background:#3b4252}.bars i:nth-child(1){height:6px}.bars i:nth-child(2){height:10px}.bars i:nth-child(3){height:15px}.bars i:nth-child(4){height:20px}.signal.good .bars i{background:var(--green)}.signal.medium .bars i:nth-child(-n+3){background:var(--yellow)}.signal.weak .bars i:first-child{background:var(--red)}
.badge{font-size:11px;padding:5px 8px;border-radius:99px;background:#242c3d;color:var(--muted)}.badge.ok{background:rgba(45,212,123,.12);color:var(--green)}.badge.warn{background:rgba(247,201,72,.12);color:var(--yellow)}
.notice{border-left:3px solid var(--yellow);padding:12px;background:rgba(247,201,72,.08);border-radius:10px}.footer-note{margin-top:18px;color:var(--muted);font-size:12px;text-align:center}
.remote{max-width:390px;margin:14px auto;background:linear-gradient(180deg,#211b2c,#111520);border:1px solid #343047;border-radius:28px;padding:18px;box-shadow:0 24px 55px rgba(0,0,0,.35)}.remote-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}.remote .btn{min-height:48px}.round{border-radius:50%;aspect-ratio:1}.dpad{display:grid;grid-template:55px 55px 55px/55px 55px 55px;justify-content:center;margin:14px}.dpad button{border:0;background:#6422a7;color:white;font-size:20px}.dpad .up{grid-column:2}.dpad .left{grid-row:2}.dpad .ok{grid-row:2;grid-column:2;border-radius:50%;background:#7a31bd}.dpad .right{grid-row:2;grid-column:3}.dpad .down{grid-row:3;grid-column:2}
.ir-led{width:12px;height:12px;border-radius:50%;background:#3b2430;box-shadow:0 0 0 4px rgba(255,65,92,.08);transition:.12s}.ir-led.flash{background:#ff405c;box-shadow:0 0 18px 7px rgba(255,64,92,.65)}
.ac-display{text-align:center;background:#d8eef4;color:#172128;border-radius:18px;padding:18px;margin-bottom:14px}.temp{font-size:58px;font-weight:800}.mode-tabs{display:grid;grid-template-columns:repeat(5,1fr);gap:5px}.mode-tabs button{font-size:11px;padding:9px 3px}
.modal{display:none;position:fixed;inset:0;background:rgba(0,0,0,.66);z-index:40;padding:18px}.modal.open{display:grid;place-items:center}.modal-box{width:min(460px,100%);background:var(--panel);border:1px solid var(--line);border-radius:20px;padding:18px}
.toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%) translateY(30px);opacity:0;background:#151b28;border:1px solid var(--line);padding:10px 14px;border-radius:99px;transition:.2s;pointer-events:none}.toast.show{opacity:1;transform:translateX(-50%) translateY(0)}
@media(max-width:520px){.grid{grid-template-columns:repeat(2,minmax(0,1fr))}.tile{min-height:112px}.topbar{grid-template-columns:76px 1fr 76px}}
</style></head><body><main>
<div class="topbar"><a class="nav" href="javascript:history.back()">← Voltar</a><h1>)HTML"
  )) + htmlEscape(title) +
  F(R"HTML(</h1><a class="nav right" href="/">Início</a></div>)HTML") +
  body +
  F(R"HTML(<div id="toast" class="toast"></div>
<script>
function toastMessage(t){const e=document.getElementById('toast');if(!e)return;e.textContent=t;e.classList.add('show');setTimeout(()=>e.classList.remove('show'),1300)}
function flashIr(){const e=document.getElementById('irLed');if(!e)return;e.classList.add('flash');setTimeout(()=>e.classList.remove('flash'),260)}
async function api(url,options){try{const r=await fetch(url,options);const j=await r.json();if(j.ok===false)toastMessage(j.message||'Falha');return j}catch(e){toastMessage('Falha de comunicação');return {ok:false}}}
</script></main></body></html>)HTML");
}


void sendJson(bool ok, const String& message, const String& extra = "") {
  String body = "{\"ok\":";
  body += ok ? "true" : "false";
  body += ",\"message\":\"" + jsonEscape(message) + "\"";
  if (extra.length()) body += "," + extra;
  body += "}";
  webServer.send(ok ? 200 : 400, "application/json", body);
}


void handleWebRoot() {
  if (webUiMode == WebUiMode::SETUP_AP) {
    String body =
      "<div class='card stack'><div class='row'><div><h2>Conectar o M5 ao Wi-Fi</h2>"
      "<div class='sub'>Escolha uma rede e informe a senha.</div></div>"
      "<div class='signal medium'><div class='bars'><i></i><i></i><i></i><i></i></div></div></div>"
      "<button class='btn primary' onclick='scan()'>Escanear redes</button>"
      "<div id='nets' class='stack'></div><div id='setupStatus' class='sub'>Aguardando...</div></div>"
      "<script>"
      "async function scan(){setupStatus.textContent='Escaneando...';const j=await api('/api/wifi/scan');nets.innerHTML='';"
      "(j.networks||[]).forEach(n=>{const b=document.createElement('button');b.className='btn';b.textContent=n.ssid+'  '+n.rssi+' dBm'+(n.saved?' • salva':'');b.onclick=()=>choose(n);nets.appendChild(b)});setupStatus.textContent=(j.networks||[]).length+' redes encontradas'}"
      "function choose(n){const p=prompt('Senha de '+n.ssid,n.saved?'(senha salva)':'');if(p!==null)connect(n.ssid,p,n.saved)}"
      "async function connect(s,p,reuse){setupStatus.textContent='Conectando...';const b='ssid='+encodeURIComponent(s)+'&password='+encodeURIComponent(p)+'&reuse='+(reuse?'1':'0');"
      "const j=await api('/api/wifi/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b});if(j.ok)poll()}"
      "function poll(){const t=setInterval(async()=>{const j=await api('/api/status');setupStatus.textContent=j.message||'';if(j.connected){clearInterval(t);setupStatus.textContent='Conectado. Abra http://'+j.ip}},900)}"
      "</script>";
    webServer.send(200, "text/html; charset=utf-8", webShell("Configurar Wi-Fi", body));
    return;
  }

  String body =
    "<div class='grid'>"
    "<a class='card tile' href='/infrared'><div class='icon'>"
    "<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'><path d='M9 18h6M10 22h4M8.5 14.5A6 6 0 1 1 15.5 14.5C14.5 15.5 14 16 14 18h-4c0-2-.5-2.5-1.5-3.5Z'/></svg>"
    "</div><div><h2>Infravermelho</h2><div class='sub'>TVs e ar-condicionado</div></div></a>"
    "<a class='card tile' href='/wifi'><div class='icon'>"
    "<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'><path d='M3 8.5a14 14 0 0 1 18 0M6.5 12a9 9 0 0 1 11 0M10 15.5a4 4 0 0 1 4 0'/><circle cx='12' cy='19' r='1'/></svg>"
    "</div><div><h2>Wi-Fi</h2><div class='sub'>Conexão e redes salvas</div></div></a>"
    "<div class='card tile'><div class='icon'>"
    "<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'><path d='M12 3v18M3 12h18'/></svg>"
    "</div><div><h2>Próximos módulos</h2><div class='sub'>Estrutura pronta para expansão</div></div></div>"
    "</div>";
  webServer.send(200, "text/html; charset=utf-8", webShell("M5 Personal", body));
}



void handleWebWifiPage() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  const int32_t rssi = connected ? WiFi.RSSI() : -127;
  const String signalClass = connected ? wifiSignalClass(rssi) : "weak";
  const String ssid = connected ? htmlEscape(WiFi.SSID()) : "Desconectado";

  String list;
  for (uint8_t i = 0; i < savedNetworkCount; i++) {
    String badge = "<span class='badge'>Não testada</span>";
    if (savedNetworks[i].health == SavedNetworkHealth::VERIFIED)
      badge = "<span class='badge ok'>Verificada</span>";
    else if (savedNetworks[i].health == SavedNetworkHealth::WARNING)
      badge = "<span class='badge warn'>⚠ Atenção</span>";

    list += "<div class='card'><div class='row'><div><b>" + htmlEscape(savedNetworks[i].ssid) +
      "</b><div class='sub'>" + String(savedNetworks[i].lastRssi > -127 ? String(savedNetworks[i].lastRssi) + " dBm" : "Sem leitura recente") +
      "</div></div>" + badge + "</div>";

    if (savedNetworks[i].health == SavedNetworkHealth::WARNING) {
      const String reason = savedNetworkFailureText(savedNetworks[i].failure);
      if (reason.length()) list += "<div class='notice' style='margin-top:10px'>" + htmlEscape(reason) + "</div>";
    }

    list += "<div class='row' style='margin-top:12px'>"
      "<button class='btn small' onclick='editNet(" + String(i) + ")'>Editar</button>"
      "<button class='btn small danger' onclick='deleteNet(" + String(i) + ")'>Excluir</button></div></div>";
  }

  if (!list.length()) list = "<div class='card sub'>Nenhuma rede salva.</div>";

  String checked = lanWebUiDesired ? " checked" : "";
  String body =
    "<div class='card'><div class='row'><div><div class='sub'>Rede atual</div><h2>" + ssid + "</h2></div>"
    "<div class='signal " + signalClass + "'><div class='bars'><i></i><i></i><i></i><i></i></div>"
    "<span>" + String(connected ? String(rssi) + " dBm" : "--") + "</span></div></div></div>"
    "<div class='card'><div class='row'><div><b>Web UI na rede</b><div class='sub'>" +
    String(webUiMode == WebUiMode::LAN ? "Ativa neste endereço" : "Desativada") +
    "</div></div><button class='btn small' onclick='toggleWeb()'>" +
    String(lanWebUiDesired ? "Desativar" : "Ativar") + "</button></div></div>"
    "<div class='row' style='margin:18px 0 8px'><h2 style='margin:0'>Redes salvas</h2>"
    "<button class='btn small primary' onclick='openAdd()'>+ Adicionar</button></div>" + list +
    "<div id='netModal' class='modal'><div class='modal-box stack'><div class='row'><h2 id='modalTitle'>Adicionar rede</h2>"
    "<button class='btn small' onclick='closeModal()'>Fechar</button></div>"
    "<input id='netIndex' type='hidden' value='-1'><div class='field'><label>SSID</label><input id='netSsid'></div>"
    "<div class='field'><label>Senha</label><input id='netPassword' type='password'></div>"
    "<label class='row' style='justify-content:flex-start'><input id='netTest' type='checkbox' checked style='width:auto'>"
    "<span>Testar novo SSID/senha antes de salvar</span></label>"
    "<button class='btn primary' onclick='saveNet()'>Salvar</button><div id='saveStatus' class='sub'></div></div></div>"
    "<script>"
    "function openAdd(){netIndex.value=-1;netSsid.value='';netPassword.value='';modalTitle.textContent='Adicionar rede';netModal.classList.add('open')}"
    "function editNet(i){netIndex.value=i;const cards=" + String(savedNetworkCount) + ";fetch('/api/wifi/saved/detail?index='+i).then(r=>r.json()).then(j=>{netSsid.value=j.ssid||'';netPassword.value='';modalTitle.textContent='Editar rede';netModal.classList.add('open')})}"
    "function closeModal(){netModal.classList.remove('open')}"
    "async function saveNet(){saveStatus.textContent='Salvando...';const b='index='+encodeURIComponent(netIndex.value)+'&ssid='+encodeURIComponent(netSsid.value)+'&password='+encodeURIComponent(netPassword.value)+'&test='+(netTest.checked?'1':'0');"
    "const j=await api('/api/wifi/saved/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b});saveStatus.textContent=j.message||'';if(j.ok&&!netTest.checked)setTimeout(()=>location.reload(),500);if(j.ok&&netTest.checked)pollSave()}"
    "function pollSave(){const t=setInterval(async()=>{const j=await api('/api/status');saveStatus.textContent=j.message||'';if(!j.connecting){clearInterval(t);setTimeout(()=>location.reload(),500)}},800)}"
    "async function deleteNet(i){if(!confirm('Excluir esta rede salva?'))return;const j=await api('/api/wifi/saved/delete?index='+i,{method:'POST'});if(j.ok)location.reload()}"
    "async function toggleWeb(){const j=await api('/api/webui/toggle',{method:'POST'});toastMessage(j.message||'');setTimeout(()=>location.reload(),500)}"
    "</script>";

  webServer.send(200, "text/html; charset=utf-8", webShell("Wi-Fi", body));
}



void handleWebIrPage() {
  String body =
    "<div class='grid'>"
    "<a class='card tile' href='/infrared/tv'><div class='icon'>"
    "<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'><rect x='3' y='5' width='18' height='13' rx='2'/><path d='m9 22 3-4 3 4'/></svg>"
    "</div><div><h2>TVs</h2><div class='sub'>Escolha a marca e abra o controle</div></div></a>"
    "<a class='card tile' href='/infrared/ac'><div class='icon'>"
    "<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'><rect x='3' y='4' width='18' height='8' rx='2'/><path d='M7 16c0 2 2 2 2 4M12 16v4M17 16c0 2-2 2-2 4'/></svg>"
    "</div><div><h2>Ar-condicionado</h2><div class='sub'>Samsung e Midea</div></div></a>"
    "</div><div class='footer-note'>Para melhor funcionamento, deixe seu M5 perto do dispositivo e controle de onde quiser com seu celular.</div>";
  webServer.send(200, "text/html; charset=utf-8", webShell("Infravermelho", body));
}


void handleWebApiScan() {
  if (webUiMode != WebUiMode::SETUP_AP && WiFi.status() == WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
  } else {
    WiFi.mode(WIFI_AP_STA);
  }

  int found = WiFi.scanNetworks(false, true);
  String json = "{\"networks\":[";
  bool first = true;

  for (int i = 0; i < found && i < 30; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    if (!first) json += ",";
    first = false;

    int8_t saved = findSavedNetwork(ssid);
    json += "{\"ssid\":\"" + jsonEscape(ssid) + "\",\"rssi\":" +
            String(WiFi.RSSI(i)) + ",\"saved\":" + (saved >= 0 ? "true" : "false") + "}";
  }

  json += "]}";
  WiFi.scanDelete();
  webServer.send(200, "application/json", json);
}

void handleWebApiConnect() {
  if (!webServer.hasArg("ssid")) {
    sendJson(false, "SSID ausente");
    return;
  }

  String ssid = webServer.arg("ssid");
  String password = webServer.arg("password");
  bool reuse = webServer.arg("reuse") == "1";
  ssid.trim();

  if (!ssid.length()) {
    sendJson(false, "SSID vazio");
    return;
  }

  if (reuse) {
    int8_t saved = findSavedNetwork(ssid);
    if (saved < 0) {
      sendJson(false, "Rede nao esta salva");
      return;
    }
    password = savedNetworks[saved].password;
  }

  beginWifiConnection(ssid, password, WifiConnectSource::WEB_SETUP);
  sendJson(true, "Conexao iniciada. Acompanhe na tela do M5.");
}

void handleWebApiStatus() {
  bool connected = WiFi.status() == WL_CONNECTED;
  String message;

  if (connected) message = "Conectado em " + WiFi.SSID();
  else if (wifiConnecting || wifiConnectPending) message = "Conectando em " + wifiPendingSsid;
  else message = "Desconectado";

  String json = "{\"connected\":";
  json += connected ? "true" : "false";
  json += ",\"message\":\"" + jsonEscape(message) + "\",\"ip\":\"";
  json += connected ? WiFi.localIP().toString() : "";
  json += "\",\"webui\":";
  json += webUiMode == WebUiMode::LAN ? "true" : "false";
  json += "}";
  webServer.send(200, "application/json", json);
}

void handleWebApiWebUiToggle() {
  if (WiFi.status() != WL_CONNECTED) {
    sendJson(false, "Conecte ao Wi-Fi primeiro");
    return;
  }

  lanWebUiDesired = !lanWebUiDesired;
  saveWebUiPreference();
  syncWebUiState();
  sendJson(true, lanWebUiDesired ? "Web UI ativada" : "Web UI desativada");
}

void handleWebApiTv() {
  if (!webServer.hasArg("device") || !webServer.hasArg("cmd")) {
    sendJson(false, "Parametros ausentes");
    return;
  }

  int device = webServer.arg("device").toInt();
  int command = webServer.arg("cmd").toInt();

  if (device < 0 || device >= TV_COUNT || command < 0 || command >= TV_COMMAND_COUNT) {
    sendJson(false, "Comando invalido");
    return;
  }

  activeTv = device;
  if (televisions[activeTv].code[command] == 0) {
    sendJson(false, "Codigo IR pendente");
    return;
  }

  sendTvCommand(static_cast<TvCommand>(command));
  sendJson(true, String(televisions[activeTv].name) + " - " +
                 tvCommandName(static_cast<TvCommand>(command)));
}

void handleWebApiAc() {
  if (!webServer.hasArg("device") || !webServer.hasArg("action")) {
    sendJson(false, "Parametros ausentes");
    return;
  }

  int device = webServer.arg("device").toInt();
  int action = webServer.arg("action").toInt();

  if (device < 0 || device >= AC_COUNT || action < 0 || action >= AC_MENU_COUNT) {
    sendJson(false, "Comando invalido");
    return;
  }

  activeAc = device;
  executeAcAction(action);
  sendJson(true, String(airConditioners[activeAc].name) + " atualizado");
}

void handleWebApiSavedNetworks() {
  String json = "{\"networks\":[";
  for (uint8_t i = 0; i < savedNetworkCount; i++) {
    if (i) json += ",";
    json += "{\"index\":" + String(i) + ",\"ssid\":\"" +
            jsonEscape(savedNetworks[i].ssid) + "\"}";
  }
  json += "]}";
  webServer.send(200, "application/json", json);
}

void handleWebApiDeleteSaved() {
  if (!webServer.hasArg("index")) {
    sendJson(false, "Indice ausente");
    return;
  }

  int index = webServer.arg("index").toInt();
  if (index < 0 || index >= savedNetworkCount) {
    sendJson(false, "Rede invalida");
    return;
  }

  deleteSavedNetwork(index);
  sendJson(true, "Rede excluida");
}


void handleWebTvPage() {
  String body =
    "<div class='card stack'><div class='field'><label>Dispositivo</label><select id='device'>"
    "<option value='0'>TV Samsung</option><option value='2'>TV LG</option>"
    "</select></div><button class='btn primary' onclick='openRemote()'>Abrir controle</button></div>"
    "<div id='remoteBox' class='remote' style='display:none'><div class='row'><b id='remoteName'>TV</b><div id='irLed' class='ir-led'></div></div>"
    "<div class='remote-grid' style='margin-top:14px'><button class='btn' onclick='sendTv(12)'>↩</button><button class='btn' onclick='sendTv(14)'>✱</button><button class='btn' onclick='sendTv(13)'>⌂</button></div>"
    "<div class='dpad'><button class='up' onclick='sendTv(7)'>⌃</button><button class='left' onclick='sendTv(9)'>‹</button><button class='ok' onclick='sendTv(11)'>OK</button><button class='right' onclick='sendTv(10)'>›</button><button class='down' onclick='sendTv(8)'>⌄</button></div>"
    "<div class='remote-grid'><button class='btn' onclick='sendTv(1)'>Mudo</button><button class='btn' onclick='sendTv(0)'>Power</button><button class='btn' onclick='sendTv(6)'>Input</button>"
    "<button class='btn' onclick='sendTv(2)'>Vol +</button><button class='btn' onclick='sendTv(3)'>Vol −</button><button class='btn' onclick='sendTv(4)'>Can +</button><button class='btn' onclick='sendTv(5)'>Can −</button></div></div>"
    "<script>function openRemote(){remoteBox.style.display='block';remoteName.textContent=device.options[device.selectedIndex].text}"
    "async function sendTv(c){const j=await api('/api/ir/tv?device='+device.value+'&cmd='+c);if(j.ok)flashIr()}</script>";
  webServer.send(200, "text/html; charset=utf-8", webShell("TVs", body));
}

void handleWebAcPage() {
  String body =
    "<div class='card stack'><div class='field'><label>Dispositivo</label><select id='device'>"
    "<option value='0'>Ar Samsung</option><option value='1'>Ar Midea</option>"
    "</select></div><button class='btn primary' onclick='openRemote()'>Abrir controle</button></div>"
    "<div id='remoteBox' class='remote' style='display:none'><div class='row'><b id='remoteName'>Ar</b><div id='irLed' class='ir-led'></div></div>"
    "<div class='ac-display'><div class='sub' style='color:#38505c'>Temperatura</div><div class='temp'><span id='temp'>24</span><small>°C</small></div>"
    "<div id='modeLabel'>Frio</div></div><div class='remote-grid'>"
    "<button class='btn' onclick='ac(0);changeTemp(-1)'>Temp −</button><button class='btn' onclick='ac(1);changeTemp(1)'>Temp +</button><button class='btn' onclick='ac(7)'>Power</button>"
    "<button class='btn' onclick='ac(2)'>Modo</button><button class='btn' onclick='ac(3)'>Ventilação</button><button class='btn' onclick='ac(4)'>Swing</button>"
    "<button class='btn' onclick='ac(5)'>Turbo</button><button class='btn' onclick='ac(6)'>Sleep</button></div></div>"
    "<script>function openRemote(){remoteBox.style.display='block';remoteName.textContent=device.options[device.selectedIndex].text}"
    "async function ac(c){const j=await api('/api/ir/ac?device='+device.value+'&action='+c);if(j.ok)flashIr()}"
    "function changeTemp(v){let n=Number(temp.textContent)+v;temp.textContent=Math.max(16,Math.min(30,n))}</script>";
  webServer.send(200, "text/html; charset=utf-8", webShell("Ar-condicionado", body));
}

void handleWebApiSavedDetail() {
  if (!webServer.hasArg("index")) {
    sendJson(false, "Índice ausente");
    return;
  }
  int index = webServer.arg("index").toInt();
  if (index < 0 || index >= savedNetworkCount) {
    sendJson(false, "Rede inválida");
    return;
  }
  String json = "{\"ok\":true,\"ssid\":\"" + jsonEscape(savedNetworks[index].ssid) +
                "\",\"health\":\"" + savedNetworkHealthText(savedNetworks[index]) +
                "\",\"reason\":\"" + jsonEscape(savedNetworkFailureText(savedNetworks[index].failure)) + "\"}";
  webServer.send(200, "application/json", json);
}

void handleWebApiSaveNetwork() {
  if (!webServer.hasArg("ssid")) {
    sendJson(false, "SSID obrigatório");
    return;
  }

  int index = webServer.hasArg("index") ? webServer.arg("index").toInt() : -1;
  String ssid = webServer.arg("ssid");
  String password = webServer.arg("password");
  bool testFirst = webServer.arg("test") == "1";
  ssid.trim();

  if (!ssid.length()) {
    sendJson(false, "SSID vazio");
    return;
  }

  if (index >= savedNetworkCount) {
    sendJson(false, "Rede inválida");
    return;
  }

  if (index >= 0 && !password.length()) {
    password = savedNetworks[index].password;
  }

  if (!testFirst) {
    if (index >= 0) {
      savedNetworks[index].ssid = ssid;
      savedNetworks[index].password = password;
      savedNetworks[index].health = SavedNetworkHealth::UNTESTED;
      savedNetworks[index].failure = SavedNetworkFailure::NONE;
      saveSavedNetworks();
      sendJson(true, "Alterações salvas sem teste.");
    } else {
      if (!upsertSavedNetwork(ssid, password)) {
        sendJson(false, "Limite de 10 redes atingido.");
        return;
      }
      sendJson(true, "Rede salva sem teste.");
    }
    return;
  }

  if (index >= 0) {
    wifiEditingSavedIndex = index;
    wifiConnectSource = WifiConnectSource::EDIT_VERIFY;
  } else {
    wifiEditingSavedIndex = -1;
    wifiConnectSource = WifiConnectSource::WEB_SETUP;
  }

  beginWifiConnection(ssid, password,
    index >= 0 ? WifiConnectSource::EDIT_VERIFY : WifiConnectSource::WEB_SETUP);
  sendJson(true, "Teste iniciado. Acompanhe o resultado.");
}

void configureWebRoutes() {
  webServer.on("/", HTTP_GET, handleWebRoot);
  webServer.on("/wifi", HTTP_GET, handleWebWifiPage);
  webServer.on("/infrared", HTTP_GET, handleWebIrPage);
  webServer.on("/infrared/tv", HTTP_GET, handleWebTvPage);
  webServer.on("/infrared/ac", HTTP_GET, handleWebAcPage);
  webServer.on("/api/wifi/scan", HTTP_GET, handleWebApiScan);
  webServer.on("/api/wifi/connect", HTTP_POST, handleWebApiConnect);
  webServer.on("/api/status", HTTP_GET, handleWebApiStatus);
  webServer.on("/api/webui/toggle", HTTP_POST, handleWebApiWebUiToggle);
  webServer.on("/api/ir/tv", HTTP_GET, handleWebApiTv);
  webServer.on("/api/ir/ac", HTTP_GET, handleWebApiAc);
  webServer.on("/api/wifi/saved", HTTP_GET, handleWebApiSavedNetworks);
  webServer.on("/api/wifi/saved/detail", HTTP_GET, handleWebApiSavedDetail);
  webServer.on("/api/wifi/saved/save", HTTP_POST, handleWebApiSaveNetwork);
  webServer.on("/api/wifi/saved/delete", HTTP_POST, handleWebApiDeleteSaved);
  webServer.onNotFound([]() {
    webServer.send(404, "text/plain", "Pagina nao encontrada");
  });
}

void startWebServerIfNeeded() {
  if (!webServerRunning) {
    webServer.begin();
    webServerRunning = true;
  }
}

void startSetupAccessPoint() {
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(WIFI_SETUP_SSID, WIFI_SETUP_PASSWORD)) {
    showToast("ERRO AO CRIAR AP", 1600);
    return;
  }

  webUiMode = WebUiMode::SETUP_AP;
  startWebServerIfNeeded();
  screen = Screen::WIFI_AP_INFO;
  selected = 0;
  redraw = true;
}

void stopSetupAccessPoint() {
  if (webUiMode != WebUiMode::SETUP_AP) return;

  WiFi.softAPdisconnect(true);
  if (WiFi.status() == WL_CONNECTED) WiFi.mode(WIFI_STA);
  webUiMode = WebUiMode::OFF;

  if (!lanWebUiDesired) {
    webServer.stop();
    webServerRunning = false;
  }
}

void startLanWebUi() {
  if (WiFi.status() != WL_CONNECTED) return;
  webUiMode = WebUiMode::LAN;
  startWebServerIfNeeded();
}

void stopLanWebUi() {
  if (webUiMode == WebUiMode::SETUP_AP) return;
  webUiMode = WebUiMode::OFF;
  webServer.stop();
  webServerRunning = false;
}

void syncWebUiState() {
  if (webUiMode == WebUiMode::SETUP_AP) return;

  if (lanWebUiDesired && WiFi.status() == WL_CONNECTED) {
    startLanWebUi();
  } else {
    stopLanWebUi();
  }
}

// ============================================================
// TECLADO ESTILO BRUCE - MAPEAMENTO M5STICKC PLUS 2
//
// A curto       = selecionar
// B curto       = mover para direita
// C curto       = mover para esquerda
// B longo       = mover para baixo
// C longo       = mover para cima
//
// Fora do teclado, C longo continua desligando o aparelho.
// ============================================================

const char BRUCE_KEYS[4][12][2] = {
  {{'1','!'},{'2','@'},{'3','#'},{'4','$'},{'5','%'},{'6','^'},{'7','&'},{'8','*'},{'9','('},{'0',')'},{'-','_'},{'=','+'}},
  {{'q','Q'},{'w','W'},{'e','E'},{'r','R'},{'t','T'},{'y','Y'},{'u','U'},{'i','I'},{'o','O'},{'p','P'},{'[','{'},{']','}'}},
  {{'a','A'},{'s','S'},{'d','D'},{'f','F'},{'g','G'},{'h','H'},{'j','J'},{'k','K'},{'l','L'},{';',':'},{'\'','"'},{'\\','|'}},
  {{'z','Z'},{'x','X'},{'c','C'},{'v','V'},{'b','B'},{'n','N'},{'m','M'},{',','<'},{'.','>'},{'/','?'},{'@','@'},{'_','_'}}
};

String wifiKeyboard(const String& title, const String& initial, bool masked, bool& cancelled) {
  String text = initial;
  bool caps = false;
  int x = 0;
  int y = -1;  // Linha superior: OK, CAP, DEL, SPACE, EXIT.
  bool redrawKeyboard = true;
  cancelled = false;

  while (true) {
    M5.update();
    buttonC.update();

    if (redrawKeyboard) {
      redrawKeyboard = false;
      auto& d = M5.Display;
      d.fillScreen(UI_BG);

      const char* actions[] = {"OK", "A@", "<-", "_", "EX"};
      const int actionX[] = {3, 48, 93, 138, 183};
      const int actionW = 42;

      for (int i = 0; i < 5; i++) {
        bool active = y == -1 && x == i;
        uint16_t bg = active ? UI_SELECTED : UI_PANEL;
        d.fillRoundRect(actionX[i], 2, actionW, 18, 3, bg);
        d.drawRoundRect(actionX[i], 2, actionW, 18, 3, active ? UI_SELECTED : UI_BORDER);
        d.setTextDatum(middle_center);
        d.setTextSize(1);
        d.setTextColor(active ? UI_BG : UI_TEXT, bg);
        String label = actions[i];
        if (i == 1) label = caps ? "ab" : "A@";
        d.drawString(label, actionX[i] + actionW / 2, 11);
      }

      d.setTextDatum(top_left);
      d.setTextColor(UI_MUTED, UI_BG);
      d.drawString(title.substring(0, 25), 3, 23);
      d.setTextDatum(top_right);
      d.drawString(String(text.length()) + "/63", 237, 23);

      d.drawRoundRect(3, 34, 234, 19, 3, UI_SELECTED);
      d.setTextDatum(middle_left);
      d.setTextColor(UI_TEXT, UI_BG);
      String shown;
      if (masked) { for (size_t i = 0; i < text.length(); i++) shown += '*'; }
      else shown = text;
      if (shown.length() > 36) shown = "..." + shown.substring(shown.length() - 33);
      d.drawString(shown, 7, 43);

      const int keyW = 20;
      const int keyH = 19;
      const int startY = 56;

      for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 12; col++) {
          int keyX = col * keyW;
          int keyY = startY + row * keyH;
          bool active = y == row && x == col;
          uint16_t bg = active ? UI_SELECTED : UI_BG;
          d.fillRect(keyX, keyY, keyW, keyH, bg);
          d.drawRect(keyX, keyY, keyW, keyH, UI_BORDER);
          d.setTextDatum(middle_center);
          d.setTextColor(active ? UI_BG : UI_TEXT, bg);
          d.drawString(String(BRUCE_KEYS[row][col][caps ? 1 : 0]), keyX + keyW / 2, keyY + keyH / 2);
        }
      }
    }

    // Longos têm prioridade sobre os curtos.
    if (M5.BtnB.wasHold()) {
      y++;
      if (y > 3) y = -1;
      if (y == -1 && x > 4) x = 0;
      redrawKeyboard = true;
      delay(140);
      continue;
    }

    if (buttonC.wasHeld()) {
      y--;
      if (y < -1) y = 3;
      if (y == -1 && x > 4) x = 0;
      redrawKeyboard = true;
      delay(140);
      continue;
    }

    if (M5.BtnB.wasClicked()) {
      int width = y == -1 ? 5 : 12;
      x = (x + 1) % width;
      redrawKeyboard = true;
    }

    if (buttonC.wasClicked()) {
      int width = y == -1 ? 5 : 12;
      x = (x + width - 1) % width;
      redrawKeyboard = true;
    }

    if (M5.BtnA.wasClicked()) {
      if (y == -1) {
        if (x == 0) return text;
        if (x == 1) caps = !caps;
        if (x == 2 && text.length()) text.remove(text.length() - 1);
        if (x == 3 && text.length() < 63) text += ' ';
        if (x == 4) {
          cancelled = true;
          return initial;
        }
      } else if (text.length() < 63) {
        text += BRUCE_KEYS[y][x][caps ? 1 : 0];
      }
      redrawKeyboard = true;
    }

    delay(10);
  }
}


// ============================================================
// INTERFACE
// ============================================================

void drawFooter() {
  auto& display = M5.Display;
  display.fillRect(0, 122, 240, 13, UI_BG);
  display.setTextDatum(middle_center);
  display.setTextSize(1);

  if (toast.length() && millis() < toastUntil) {
    display.setTextColor(UI_GREEN, UI_BG);
    display.drawString(toast, 120, 128);
  } else {
    toast = "";
    display.setTextColor(UI_MUTED, UI_BG);
    display.drawString("A OK   B > / SEG VOLTA   C <", 120, 128);
  }
}

void drawTitle(const String& title, const String& subtitle) {
  auto& display = M5.Display;
  display.fillRoundRect(4, 3, 232, 34, 7, UI_PANEL);
  display.setTextDatum(middle_left);
  display.setTextSize(2);
  display.setTextColor(UI_TEXT, UI_PANEL);
  display.drawString(title, 12, 16);

  if (subtitle.length()) {
    display.setTextDatum(middle_right);
    display.setTextSize(1);
    display.setTextColor(UI_YELLOW, UI_PANEL);
    display.drawString(subtitle, 228, 17);
  }
}


void drawListItem(uint8_t index, int y, const String& label, const String& detail) {
  auto& display = M5.Display;
  const bool active = selected == index;
  const uint16_t fill = active ? UI_SELECTED : UI_PANEL;
  const uint16_t foreground = active ? UI_BG : UI_TEXT;

  display.fillRoundRect(8, y, 224, 23, 5, fill);
  display.drawRoundRect(8, y, 224, 23, 5, active ? UI_TEXT : UI_BORDER);

  const int detailWidth = detail.length() ? min<int>(72, static_cast<int>(display.textWidth(detail)) + 10) : 0;
  const int labelX = 16;
  const int labelRight = 226 - detailWidth;
  const int labelWidth = labelRight - labelX;

  display.setTextDatum(middle_left);
  display.setTextSize(1);
  display.setTextColor(foreground, fill);

  display.setClipRect(labelX, y + 2, labelWidth, 19);
  int textWidth = display.textWidth(label);
  int offset = 0;

  // O item selecionado faz rolagem horizontal suave quando o nome não cabe.
  if (active && textWidth > labelWidth) {
    const int travel = textWidth - labelWidth + 18;
    const uint32_t cycle = 900 + travel * 35 + 900;
    const uint32_t phase = millis() % cycle;
    if (phase < 900) offset = 0;
    else if (phase < cycle - 900) offset = min(travel, int((phase - 900) / 35));
    else offset = travel;
  }

  display.drawString(label, labelX - offset, y + 12);
  display.clearClipRect();

  if (detail.length()) {
    display.setTextDatum(middle_right);
    display.setTextColor(active ? UI_BG : UI_MUTED, fill);
    display.drawString(detail, 224, y + 12);
  }
}


void drawGridButton(uint8_t index, int x, int y, int w, int h,
                    const String& label, const String& value) {
  auto& display = M5.Display;
  const bool active = selected == index;
  const uint16_t fill = active ? UI_SELECTED : UI_PANEL;
  const uint16_t border = active ? UI_TEXT : UI_BORDER;
  const uint16_t foreground = active ? UI_BG : UI_TEXT;

  display.fillRoundRect(x, y, w, h, 6, fill);
  display.drawRoundRect(x, y, w, h, 6, border);
  display.setTextDatum(middle_center);
  display.setTextSize(1);
  display.setTextColor(foreground, fill);

  if (value.length()) {
    display.drawString(label, x + w / 2, y + 9);
    display.setTextColor(active ? UI_BG : UI_YELLOW, fill);
    display.drawString(value, x + w / 2, y + 22);
  } else {
    display.drawString(label, x + w / 2, y + h / 2);
  }
}

void drawMain() {
  drawTitle("M5 PERSONAL", "v0.8");
  drawListItem(0, 44, "INFRAVERMELHO", "IR");
  drawListItem(1, 71, "WIFI", WiFi.status() == WL_CONNECTED ? "ON" : "OFF");
}



void drawWifiMenu() {
  drawTitle("WIFI", WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "DESCONECTADO");

  const String labels[] = {"CONECTAR", "CONECTAR WEB UI", "WEB UI REDE", "REDES SALVAS"};
  const String details[] = {
    "REDES",
    "AP",
    webUiMode == WebUiMode::LAN ? "ATIVA" : "OFF",
    String(savedNetworkCount)
  };

  const uint8_t visible = 3;
  uint8_t first = selected >= visible ? selected - visible + 1 : 0;

  for (uint8_t row = 0; row < visible; row++) {
    uint8_t item = first + row;
    if (item >= 4) break;
    drawListItem(item, 43 + row * 27, labels[item], details[item]);
  }

  // Barra de rolagem vertical.
  M5.Display.fillRoundRect(235, 43, 3, 77, 2, UI_BORDER);
  int thumbY = 43 + (selected * 57 / 3);
  M5.Display.fillRoundRect(235, thumbY, 3, 20, 2, UI_SELECTED);
}


void drawWifiScanning() {
  drawTitle("WIFI", "ESCANEANDO...");
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(UI_SELECTED, UI_BG);
  String dots;
  for (uint8_t i = 0; i < (millis() / 300) % 4; i++) dots += ".";
  M5.Display.drawString("PROCURANDO" + dots, 120, 70);
}

void drawWifiNetworks() {
  drawTitle("REDES DISPONIVEIS", String(scannedNetworkCount));
  if (!scannedNetworkCount) {
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(UI_RED, UI_BG);
    M5.Display.drawString("NENHUMA REDE", 120, 70);
    return;
  }

  uint8_t start = selected > 2 ? selected - 2 : 0;
  uint8_t row = 0;
  for (uint8_t i = start; i < scannedNetworkCount && row < 3; i++, row++) {
    String detail = String(scannedRssi[i]) + "dB";
    if (scannedSavedIndex[i] != 255) detail = "SALVA " + detail;
    drawListItem(i, 43 + row * 27, scannedSsids[i], detail);
  }
}

void drawWifiConnecting() {
  drawTitle("CONECTANDO", wifiPendingSsid);
  const char frames[] = {'|', '/', '-', '\\'};
  char frame[2] = {frames[(millis() / 180) % 4], '\0'};
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(UI_SELECTED, UI_BG);
  M5.Display.drawString(frame, 120, 70);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(UI_MUTED, UI_BG);
  M5.Display.drawString(String((millis() - wifiConnectStartedAt) / 1000) + "s", 120, 96);
}

void drawWifiResult() {
  drawTitle(wifiResultTitle, wifiResultDetail);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(wifiResultTitle == "CONECTADO" ? UI_GREEN : UI_RED, UI_BG);
  M5.Display.drawString(wifiResultTitle == "CONECTADO" ? "OK" : "ERRO", 120, 65);
  if (WiFi.status() == WL_CONNECTED) {
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(UI_TEXT, UI_BG);
    M5.Display.drawString(WiFi.localIP().toString(), 120, 92);
  }
}

void drawWifiApInfo() {
  drawTitle("CONECTAR WEB UI", "AP ATIVO");
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(UI_TEXT, UI_BG);
  M5.Display.drawString("REDE: " + String(WIFI_SETUP_SSID), 120, 49);
  M5.Display.drawString("SENHA: " + String(WIFI_SETUP_PASSWORD), 120, 66);
  M5.Display.setTextColor(UI_SELECTED, UI_BG);
  M5.Display.drawString("192.168.4.1", 120, 86);
  M5.Display.setTextColor(UI_MUTED, UI_BG);
  M5.Display.drawString("B LONGO PARA SAIR", 120, 106);
}

void drawWifiWebUiNetwork() {
  drawTitle("WEB UI REDE", WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "SEM WIFI");
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(webUiMode == WebUiMode::LAN ? UI_GREEN : UI_RED, UI_BG);
  M5.Display.drawString(webUiMode == WebUiMode::LAN ? "ATIVA" : "DESATIVADA", 120, 62);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(UI_TEXT, UI_BG);
  if (WiFi.status() == WL_CONNECTED) {
    M5.Display.drawString(WiFi.localIP().toString(), 120, 90);
  } else {
    M5.Display.drawString("CONECTE AO WIFI", 120, 90);
  }
}


void drawWifiSavedList() {
  drawTitle("REDES SALVAS", String(savedNetworkCount) + "/10");
  if (!savedNetworkCount) {
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(UI_MUTED, UI_BG);
    M5.Display.drawString("NENHUMA REDE", 120, 68);
    return;
  }

  uint8_t start = selected > 2 ? selected - 2 : 0;
  uint8_t row = 0;
  for (uint8_t i = start; i < savedNetworkCount && row < 3; i++, row++) {
    String detail;
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == savedNetworks[i].ssid) detail = "ATUAL";
    else if (savedNetworks[i].health == SavedNetworkHealth::WARNING) detail = "!";
    else if (savedNetworks[i].health == SavedNetworkHealth::VERIFIED) detail = "OK";
    drawListItem(i, 43 + row * 27, savedNetworks[i].ssid, detail);
  }

  if (savedNetworkCount > 3) {
    M5.Display.fillRoundRect(235, 43, 3, 77, 2, UI_BORDER);
    int maxIndex = savedNetworkCount - 1;
    int thumbY = 43 + (selected * 57 / max<int>(1, static_cast<int>(maxIndex)));
    M5.Display.fillRoundRect(235, thumbY, 3, 20, 2, UI_SELECTED);
  }
}


void drawWifiSavedDetail() {
  if (wifiSelectedSavedIndex < 0 || wifiSelectedSavedIndex >= savedNetworkCount) {
    screen = Screen::WIFI_SAVED_LIST;
    selected = 0;
    return;
  }

  drawTitle(savedNetworks[wifiSelectedSavedIndex].ssid, "REDE SALVA");
  drawListItem(0, 39, "CONECTAR");
  drawListItem(1, 63, "EDITAR SSID");
  drawListItem(2, 87, "EDITAR SENHA");
  drawListItem(3, 111, "EXCLUIR", "!");
}

void drawWifiDeleteConfirm() {
  drawTitle("EXCLUIR REDE?", savedNetworks[wifiSelectedSavedIndex].ssid);
  drawListItem(0, 55, "NAO");
  drawListItem(1, 84, "SIM, EXCLUIR", "!");
}


void drawIrTypes() {
  drawTitle("INFRAVERMELHO");
  drawListItem(0, 46, "TVs", String(TV_COUNT));
  drawListItem(1, 73, "AR-CONDICIONADO", String(AC_COUNT));
}

void drawTvList() {
  drawTitle("TVs");
  for (uint8_t i = 0; i < TV_COUNT; i++) {
    drawListItem(i, 42 + i * 26, televisions[i].name);
  }
}

void drawAcList() {
  drawTitle("AR-CONDICIONADO");
  for (uint8_t i = 0; i < AC_COUNT; i++) {
    drawListItem(i, 46 + i * 28, airConditioners[i].name);
  }
}

void drawTvRemote(bool navigationPage) {
  const TvDevice& tv = televisions[activeTv];
  drawTitle(tv.name, navigationPage ? "NAVEGACAO" : "CONTROLE");

  constexpr int x0 = 4;
  constexpr int y0 = 43;
  constexpr int gap = 3;
  constexpr int width = 56;
  constexpr int height = 36;

  if (!navigationPage) {
    drawGridButton(0, x0 + 0 * (width + gap), y0, width, height, "POWER");
    drawGridButton(1, x0 + 1 * (width + gap), y0, width, height, "MUDO");
    drawGridButton(2, x0 + 2 * (width + gap), y0, width, height, "VOL", "+");
    drawGridButton(3, x0 + 3 * (width + gap), y0, width, height, "VOL", "-");
    drawGridButton(4, x0 + 0 * (width + gap), y0 + height + gap, width, height, "CAN", "+");
    drawGridButton(5, x0 + 1 * (width + gap), y0 + height + gap, width, height, "CAN", "-");
    drawGridButton(6, x0 + 2 * (width + gap), y0 + height + gap, width, height, "INPUT");
    drawGridButton(7, x0 + 3 * (width + gap), y0 + height + gap, width, height, "NAV");
  } else {
    drawGridButton(0, x0 + 0 * (width + gap), y0, width, height, "CIMA");
    drawGridButton(1, x0 + 1 * (width + gap), y0, width, height, "BAIXO");
    drawGridButton(2, x0 + 2 * (width + gap), y0, width, height, "ESQ");
    drawGridButton(3, x0 + 3 * (width + gap), y0, width, height, "DIR");
    drawGridButton(4, x0 + 0 * (width + gap), y0 + height + gap, width, height, "OK");
    drawGridButton(5, x0 + 1 * (width + gap), y0 + height + gap, width, height, "VOLTAR");
    drawGridButton(6, x0 + 2 * (width + gap), y0 + height + gap, width, height, "HOME");
    drawGridButton(7, x0 + 3 * (width + gap), y0 + height + gap, width, height, "MENU");
  }
}

void drawAcRemote() {
  const AcDevice& device = airConditioners[activeAc];
  const AcState& state = device.state;
  auto& display = M5.Display;

  display.fillRoundRect(4, 3, 232, 52, 7, UI_PANEL);
  display.setTextDatum(top_left);
  display.setTextSize(1);
  display.setTextColor(UI_MUTED, UI_PANEL);
  display.drawString(device.name, 11, 8);

  display.setTextDatum(middle_left);
  display.setTextSize(3);
  display.setTextColor(UI_TEXT, UI_PANEL);
  display.drawString(String(state.temp) + "C", 12, 34);

  display.setTextDatum(middle_center);
  display.setTextSize(1);
  display.setTextColor(UI_YELLOW, UI_PANEL);
  display.drawString(acModeName(state.mode), 122, 31);
  display.setTextColor(UI_MUTED, UI_PANEL);
  display.drawString("FAN " + String(acFanName(state.fan)), 122, 44);

  display.setTextDatum(middle_right);
  display.setTextColor(state.power ? UI_GREEN : UI_RED, UI_PANEL);
  display.drawString(state.power ? "ON" : "OFF", 226, 20);
  display.setTextColor(UI_MUTED, UI_PANEL);
  display.drawString(state.swing ? "SWING" : "-", 226, 35);
  display.drawString(state.turbo ? "TURBO" : sleepName(device), 226, 47);

  constexpr int startX = 4;
  constexpr int startY = 59;
  constexpr int gap = 3;
  constexpr int buttonW = 56;
  constexpr int buttonH = 28;

  drawGridButton(0, startX + 0 * (buttonW + gap), startY, buttonW, buttonH, "TEMP", "-");
  drawGridButton(1, startX + 1 * (buttonW + gap), startY, buttonW, buttonH, "TEMP", "+");
  drawGridButton(2, startX + 2 * (buttonW + gap), startY, buttonW, buttonH, "MODO", acModeName(state.mode));
  drawGridButton(3, startX + 3 * (buttonW + gap), startY, buttonW, buttonH, "FAN", acFanName(state.fan));
  drawGridButton(4, startX + 0 * (buttonW + gap), startY + buttonH + gap, buttonW, buttonH, "SWING", state.swing ? "ON" : "OFF");
  drawGridButton(5, startX + 1 * (buttonW + gap), startY + buttonH + gap, buttonW, buttonH, "TURBO", state.turbo ? "ON" : "OFF");
  drawGridButton(6, startX + 2 * (buttonW + gap), startY + buttonH + gap, buttonW, buttonH, "SLEEP", sleepName(device));
  drawGridButton(7, startX + 3 * (buttonW + gap), startY + buttonH + gap, buttonW, buttonH, "POWER", state.power ? "OFF" : "ON");
}

void drawScreen() {
  auto& display = M5.Display;
  display.startWrite();
  display.fillScreen(UI_BG);

  switch (screen) {
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
    case Screen::IR_TYPES:  drawIrTypes(); break;
    case Screen::TV_LIST:   drawTvList(); break;
    case Screen::AC_LIST:   drawAcList(); break;
    case Screen::TV_REMOTE: drawTvRemote(false); break;
    case Screen::TV_NAV:    drawTvRemote(true); break;
    case Screen::AC_REMOTE: drawAcRemote(); break;
  }

  drawFooter();
  display.endWrite();
  redraw = false;
}

// ============================================================
// NAVEGACAO
// ============================================================

uint8_t itemCount() {
  switch (screen) {
    case Screen::MAIN:                return 2;
    case Screen::WIFI_MENU:           return 4;
    case Screen::WIFI_SCANNING:       return 1;
    case Screen::WIFI_NETWORKS:       return scannedNetworkCount ? scannedNetworkCount : 1;
    case Screen::WIFI_KEYBOARD:       return 1;
    case Screen::WIFI_CONNECTING:     return 1;
    case Screen::WIFI_RESULT:         return 1;
    case Screen::WIFI_AP_INFO:        return 1;
    case Screen::WIFI_WEBUI_NETWORK:  return 1;
    case Screen::WIFI_SAVED_LIST:     return savedNetworkCount ? savedNetworkCount : 1;
    case Screen::WIFI_SAVED_DETAIL:   return 4;
    case Screen::WIFI_DELETE_CONFIRM: return 2;
    case Screen::IR_TYPES:  return 2;
    case Screen::TV_LIST:   return TV_COUNT;
    case Screen::AC_LIST:   return AC_COUNT;
    case Screen::TV_REMOTE: return 8;
    case Screen::TV_NAV:    return 8;
    case Screen::AC_REMOTE: return AC_MENU_COUNT;
  }
  return 1;
}

void nextItem() {
  selected = (selected + 1) % itemCount();
  redraw = true;
}

void previousItem() {
  selected = (selected + itemCount() - 1) % itemCount();
  redraw = true;
}

void goBack() {
  switch (screen) {
    case Screen::MAIN:
      showToast("MENU PRINCIPAL");
      return;

    case Screen::WIFI_MENU:
      screen = Screen::MAIN;
      break;

    case Screen::WIFI_NETWORKS:
    case Screen::WIFI_RESULT:
    case Screen::WIFI_WEBUI_NETWORK:
    case Screen::WIFI_SAVED_LIST:
      screen = Screen::WIFI_MENU;
      break;

    case Screen::WIFI_SAVED_DETAIL:
    case Screen::WIFI_DELETE_CONFIRM:
      screen = Screen::WIFI_SAVED_LIST;
      break;

    case Screen::WIFI_AP_INFO:
      stopSetupAccessPoint();
      screen = Screen::WIFI_MENU;
      break;

    case Screen::WIFI_SCANNING:
    case Screen::WIFI_CONNECTING:
    case Screen::WIFI_KEYBOARD:
      return;

    case Screen::IR_TYPES:
      screen = Screen::MAIN;
      break;

    case Screen::TV_LIST:
    case Screen::AC_LIST:
      screen = Screen::IR_TYPES;
      break;

    case Screen::TV_REMOTE:
      screen = Screen::TV_LIST;
      selected = activeTv;
      redraw = true;
      return;

    case Screen::TV_NAV:
      screen = Screen::TV_REMOTE;
      break;

    case Screen::AC_REMOTE:
      screen = Screen::AC_LIST;
      selected = activeAc;
      redraw = true;
      return;
  }

  selected = 0;
  redraw = true;
}

void executeSelected() {
  switch (screen) {
    case Screen::MAIN:
      screen = selected == 0 ? Screen::IR_TYPES : Screen::WIFI_MENU;
      selected = 0;
      break;

    case Screen::WIFI_MENU:
      if (selected == 0) {
        scanNetworksNow();
      } else if (selected == 1) {
        startSetupAccessPoint();
      } else if (selected == 2) {
        screen = Screen::WIFI_WEBUI_NETWORK;
        selected = 0;
      } else {
        screen = Screen::WIFI_SAVED_LIST;
        selected = 0;
      }
      break;

    case Screen::WIFI_NETWORKS:
      if (!scannedNetworkCount) break;
      wifiChosenSsid = scannedSsids[selected];
      if (scannedSavedIndex[selected] != 255) {
        uint8_t saved = scannedSavedIndex[selected];
        beginWifiConnection(savedNetworks[saved].ssid, savedNetworks[saved].password, WifiConnectSource::SAVED_MANUAL);
      } else {
        bool cancelled = false;
        String password = wifiKeyboard("SENHA: " + wifiChosenSsid, "", true, cancelled);
        if (!cancelled) beginWifiConnection(wifiChosenSsid, password, WifiConnectSource::PHYSICAL);
        else {
          screen = Screen::WIFI_NETWORKS;
          redraw = true;
        }
      }
      break;

    case Screen::WIFI_RESULT:
      screen = Screen::WIFI_MENU;
      selected = 0;
      break;

    case Screen::WIFI_WEBUI_NETWORK:
      if (WiFi.status() != WL_CONNECTED) {
        showToast("SEM WIFI", 1200);
      } else {
        lanWebUiDesired = !lanWebUiDesired;
        saveWebUiPreference();
        syncWebUiState();
        showToast(lanWebUiDesired ? "WEB UI ATIVA" : "WEB UI OFF", 1200);
      }
      break;

    case Screen::WIFI_SAVED_LIST:
      if (savedNetworkCount) {
        wifiSelectedSavedIndex = selected;
        screen = Screen::WIFI_SAVED_DETAIL;
        selected = 0;
      }
      break;

    case Screen::WIFI_SAVED_DETAIL:
      if (wifiSelectedSavedIndex < 0 || wifiSelectedSavedIndex >= savedNetworkCount) break;
      if (selected == 0) {
        beginWifiConnection(
          savedNetworks[wifiSelectedSavedIndex].ssid,
          savedNetworks[wifiSelectedSavedIndex].password,
          WifiConnectSource::SAVED_MANUAL
        );
      } else if (selected == 1) {
        bool cancelled = false;
        String newSsid = wifiKeyboard("NOVO SSID", savedNetworks[wifiSelectedSavedIndex].ssid, false, cancelled);
        if (!cancelled && newSsid.length()) {
          wifiEditingSavedIndex = wifiSelectedSavedIndex;
          beginWifiConnection(newSsid, savedNetworks[wifiSelectedSavedIndex].password, WifiConnectSource::EDIT_VERIFY);
        }
      } else if (selected == 2) {
        bool cancelled = false;
        String newPassword = wifiKeyboard("NOVA SENHA", "", true, cancelled);
        if (!cancelled) {
          wifiEditingSavedIndex = wifiSelectedSavedIndex;
          beginWifiConnection(savedNetworks[wifiSelectedSavedIndex].ssid, newPassword, WifiConnectSource::EDIT_VERIFY);
        }
      } else {
        screen = Screen::WIFI_DELETE_CONFIRM;
        selected = 0;
      }
      break;

    case Screen::WIFI_DELETE_CONFIRM:
      if (selected == 1 && wifiSelectedSavedIndex >= 0) {
        deleteSavedNetwork(wifiSelectedSavedIndex);
        wifiSelectedSavedIndex = -1;
        screen = Screen::WIFI_SAVED_LIST;
        selected = 0;
        showToast("REDE EXCLUIDA", 1200);
      } else {
        screen = Screen::WIFI_SAVED_DETAIL;
        selected = 0;
      }
      break;

    case Screen::WIFI_AP_INFO:
    case Screen::WIFI_SCANNING:
    case Screen::WIFI_CONNECTING:
    case Screen::WIFI_KEYBOARD:
      break;

    case Screen::IR_TYPES:
      screen = selected == 0 ? Screen::TV_LIST : Screen::AC_LIST;
      selected = 0;
      break;

    case Screen::TV_LIST:
      activeTv = selected;
      screen = Screen::TV_REMOTE;
      selected = 0;
      break;

    case Screen::AC_LIST:
      activeAc = selected;
      screen = Screen::AC_REMOTE;
      selected = 0;
      break;

    case Screen::TV_REMOTE:
      if (selected == 7) {
        screen = Screen::TV_NAV;
        selected = 0;
      } else {
        static const TvCommand map[] = {
          TV_POWER, TV_MUTE, TV_VOL_UP, TV_VOL_DOWN,
          TV_CH_UP, TV_CH_DOWN, TV_INPUT
        };
        sendTvCommand(map[selected]);
      }
      break;

    case Screen::TV_NAV: {
      static const TvCommand map[] = {
        TV_UP, TV_DOWN, TV_LEFT, TV_RIGHT,
        TV_OK, TV_BACK, TV_HOME, TV_MENU
      };
      sendTvCommand(map[selected]);
      break;
    }

    case Screen::AC_REMOTE:
      executeAcAction(selected);
      break;
  }

  redraw = true;
}

// ============================================================
// SETUP / LOOP
// ============================================================

void setup() {
  Serial.begin(115200);

  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(3);
  M5.Display.setBrightness(90);
  M5.Display.setTextFont(1);
  M5.Display.setTextWrap(false);

  buttonC.begin();
  loadAcStates();
  loadSavedNetworks();
  configureWebRoutes();

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  wifiAutoScanPending = true;
  wifiLastRetryScanAt = millis();

  tvIr.begin();

  samsungAc.stateReset(true, false);
  samsungAc.begin();
  applySamsungState(airConditioners[0].state);

  mideaAc.stateReset();
  mideaAc.begin();
  applyMideaState(airConditioners[1].state);

  drawScreen();

  Serial.println();
  Serial.println("M5 PERSONAL v0.8 - IR + CHECKPOINT 1 iniciado.");
  Serial.println("Rotacao 3: emissor IR deve ficar para cima.");
}

void loop() {
  if (webServerRunning) {
    webServer.handleClient();
  }

  processWifiConnection();
  processWifiMaintenance();

  M5.update();
  buttonC.update();

  if (screen == Screen::WIFI_CONNECTING) {
    redraw = true;
    drawScreen();
    delay(10);
    return;
  }

  // B longo tem prioridade sobre B curto.
  if (M5.BtnB.wasHold()) {
    goBack();
  } else if (M5.BtnB.wasClicked()) {
    nextItem();
  }

  if (M5.BtnA.wasClicked()) {
    executeSelected();
  }

  if (buttonC.wasHeld()) {
    M5.Display.fillScreen(UI_BG);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(UI_TEXT, UI_BG);
    M5.Display.drawString("DESLIGANDO...", 120, 67);
    delay(250);
    M5.Power.powerOff();
  } else if (buttonC.wasClicked()) {
    previousItem();
  }

  static bool toastWasVisible = false;
  const bool toastVisible = toast.length() && millis() < toastUntil;

  if (toastWasVisible && !toastVisible) {
    redraw = true;
  }

  toastWasVisible = toastVisible;

  if (screen == Screen::WIFI_SCANNING ||
      screen == Screen::WIFI_CONNECTING ||
      screen == Screen::MAIN ||
      screen == Screen::WIFI_MENU ||
      screen == Screen::WIFI_NETWORKS ||
      screen == Screen::WIFI_SAVED_LIST ||
      screen == Screen::WIFI_SAVED_DETAIL ||
      screen == Screen::IR_TYPES ||
      screen == Screen::TV_LIST ||
      screen == Screen::AC_LIST) {
    redraw = true;
  }

  if (redraw) {
    drawScreen();
  }

  delay(10);
}
