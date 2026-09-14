#include <M5Unified.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Samsung.h>
#include <ir_Midea.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <time.h>
#include <sys/time.h>
#include "M5StickBleMouse.h"
// Voice AI Module enabled

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
constexpr uint16_t UI_BG         = 0x0000; // True Black OLED
constexpr uint16_t UI_PANEL      = 0x1084; // Obsidian Slate
constexpr uint16_t UI_PANEL_ALT  = 0x18C6; // Elevated Card
constexpr uint16_t UI_BORDER     = 0x2126; // Subtle Border
constexpr uint16_t UI_SELECTED   = 0x067F; // Electric Cyan
constexpr uint16_t UI_TEXT       = 0xFFFF; // Pure White
constexpr uint16_t UI_MUTED      = 0x8CD1; // Metallic Silver
constexpr uint16_t UI_GREEN      = 0x27E8; // Cyber Mint
constexpr uint16_t UI_RED        = 0xF9C7; // Coral Red
constexpr uint16_t UI_YELLOW     = 0xFDE0; // Cyber Gold
constexpr uint16_t UI_ORANGE     = 0xFD20; // Neon Orange
constexpr uint16_t UI_PURPLE     = 0xB29F; // Magic Purple
constexpr uint16_t UI_CYAN       = 0x05BF; // Bright Cyan

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
  AC_REMOTE,
  TEAM_MENU,
  TEAM_CATTLE_LIMIT,
  TEAM_CATTLE_COUNTER,
  TEAM_CATTLE_RESET_CONFIRM,
  TEAM_TRAIN_COUNT,
  TEAM_TRAIN_SELECT_HORSE,
  TEAM_TRAIN_ACTIVE,
  TEAM_TRAIN_END_CONFIRM,
  TEAM_TRAIN_SUMMARY,
  TEAM_TRAIN_HISTORY,
  TEAM_TRAIN_HISTORY_DETAIL,
  SETTINGS_MENU,
  SETTINGS_BRIGHTNESS,
  SETTINGS_CLOCK,
  SETTINGS_SLEEP,
  MOUSE,
  VOICE_AI
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

bool wifiScanRunning = false;
bool wifiScanForMenu = false;
bool wifiScanForAuto = false;
bool wifiScanHasResults = false;
bool wifiAutoCandidatesReady = false;
bool wifiKeyboardActive = false;
bool setupExitPending = false;
uint32_t setupExitAt = 0;
uint8_t wifiAutoCandidateCursor = 0;
uint32_t wifiScanStartedAt = 0;

bool pendingWebSaveWithoutTest = false;
bool pendingWebEdit = false;
int8_t pendingWebEditIndex = -1;
String pendingWebOriginalSsid;
String pendingWebOriginalPassword;

uint32_t wifiConnectStartedAt = 0;
uint32_t wifiLastRetryScanAt = 0;

bool webUiSyncPending=false;
uint32_t webUiSyncAt=0;
bool webServerStopPending=false;
uint32_t webServerStopAt=0;

IRsend tvIr(IR_PIN);
IRSamsungAc samsungAc(IR_PIN);
IRMideaAC mideaAc(IR_PIN);

Screen screen = Screen::MAIN;
uint8_t selected = 0;
uint8_t activeTv = 0;
uint8_t activeAc = 0;
bool redraw = true;
bool forceFullRedraw = true;

String toast;
uint32_t toastUntil = 0;
ButtonCState buttonC;

constexpr uint32_t TEAM_A_HOLD_MS = 3000;
uint8_t cattleMaxNumber = 9;
uint16_t cattleDrawnMask = 0;
uint8_t cattleSelectedNumber = 0;
bool cattleSessionInitialized = false;
uint32_t teamAPressedAt = 0;
bool teamAHoldTriggered = false;

constexpr uint8_t MAX_TRAIN_HORSES = 5;
constexpr uint8_t MAX_TRAIN_HISTORY = 2;
const char* TRAIN_HORSE_NAMES[MAX_TRAIN_HORSES] = {"Linda", "Purity", "Cavalo 1", "Cavalo 2", "Cavalo 3"};

struct TrainingSession {
  bool active = false;
  uint8_t horseCount = 0;
  uint8_t horseIds[MAX_TRAIN_HORSES] = {0, 1, 2, 3, 4};
  uint16_t passes[MAX_TRAIN_HORSES] = {0, 0, 0, 0, 0};
  uint8_t currentHorse = 0;
};

struct TrainingRecord {
  bool valid = false;
  char date[11] = {0};
  uint8_t horseCount = 0;
  uint8_t horseIds[MAX_TRAIN_HORSES] = {0, 1, 2, 3, 4};
  uint16_t passes[MAX_TRAIN_HORSES] = {0, 0, 0, 0, 0};
};

TrainingSession trainingSession;
TrainingRecord trainingHistory[MAX_TRAIN_HISTORY];
uint8_t trainingSetupCount = 1;
uint8_t trainingSetupSlot = 0;
uint8_t trainingSetupCandidate = 0;
uint8_t trainingSetupHorseIds[MAX_TRAIN_HORSES] = {0, 1, 2, 3, 4};
uint8_t trainingSummaryHorse = 0;
uint8_t trainingHistoryRecord = 0;
uint8_t trainingHistoryHorse = 0;


enum class DisplayIdleState : uint8_t { ACTIVE, CLOCK, OFF };
DisplayIdleState displayIdleState = DisplayIdleState::ACTIVE;
constexpr uint32_t SCREEN_CLOCK_AFTER_MS = 3UL * 60UL * 1000UL;
constexpr uint32_t SCREEN_OFF_AFTER_CLOCK_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t WEATHER_REFRESH_MS = 40UL * 60UL * 1000UL;
constexpr uint8_t SLEEP_BRIGHTNESS = 28;
const uint8_t BRIGHTNESS_LEVELS[] = {51, 102, 153, 204, 255};
uint8_t brightnessIndex = 2;
uint32_t lastUserActivityAt = 0;
uint32_t clockScreenStartedAt = 0;
uint32_t lastClockRedrawAt = 0;
uint32_t lastWeatherAttemptAt = 0;
String lastWeatherSsid;
String weatherCity;
String weatherTimezone;
float weatherLatitude = 0.0f;
float weatherLongitude = 0.0f;
float weatherTemperature = NAN;
time_t weatherUpdatedAt = 0;
bool weatherLocationValid = false;
bool weatherUpdatePending = false;

bool isMenuScreen(Screen value);
void drawWifiIcon(int x, int y, bool connected);
void drawBatteryGauge(int x, int y, int battery, bool charging);
void drawStatusBar();
void updateStatusBarClock();
void drawClockScreen(bool fullClear = true);
void processDisplayIdle(bool anyButtonPressed);
void restoreDisplayFromIdle();
void loadSystemSettings();
void saveSystemSettings();
void loadWeatherCache();
void saveWeatherCache();
void initializeClockFromRtc();
bool clockIsValid();
String clockTimeText();
String clockDateText();
void processWeatherAndClock();
bool updateLocationAndWeather();
bool syncClockFromInternet(int32_t utcOffsetSeconds);
String jsonStringValue(const String& json, const String& key);
double jsonNumberValue(const String& json, const String& key, double fallback);
int32_t timezoneOffsetSeconds(const String& offset);
void drawSettingsMenu();
void drawSettingsBrightness();
void drawSettingsClock();
void drawSettingsSleep();
void drawMouseScreen();
void updateMouseSearchingDots();
void updateMouseCrosshair(float vx, float vy, uint16_t ballColor);
void startBleMouse();

enum class VoiceState : uint8_t {
  IDLE,
  LISTENING,
  THINKING,
  RESULT
};

VoiceState voiceState = VoiceState::IDLE;
String voiceActiveAgent = "IA Geral";
String voiceTranscription = "";
String voiceResultTitle = "";
String voiceResultBody = "";
int voiceScrollLine = 0;
uint8_t voiceWavePhase = 0;
uint32_t voiceAnimTimer = 0;

void drawVoiceAiScreen();
void processVoiceAiScreen();
void initVoiceAiScreen();
void playWandChime();
void playFailSound();
void drawWrappedText(int x, int y, int maxW, int maxLines, int startLine, const String& text, uint16_t color);

M5StickBleMouse bleMouse;
bool bleMouseStarted = false;

bool mouseCalibrated = false;
uint16_t mouseCalibSamples = 0;
float mouseSumGx = 0.0f;
float mouseSumGy = 0.0f;
float mouseSumGz = 0.0f;
float mouseBiasGx = 0.0f;
float mouseBiasGy = 0.0f;
float mouseBiasGz = 0.0f;
float mouseSmoothDx = 0.0f;
float mouseSmoothDy = 0.0f;
float mouseRemainderX = 0, mouseRemainderY = 0;
uint32_t mouseSampleAt = 0, mouseUiAt = 0, mouseQuietUntil = 0;
bool mouseButtonsArmed = false;
int mouseDotX = 67, mouseDotY = 111;

void resetMouseCalibration() {
  bleMouse.releaseAll();
  mouseCalibrated = false;
  mouseCalibSamples = 0;
  mouseSumGx = mouseSumGy = mouseSumGz = 0;
  mouseSmoothDx = mouseSmoothDy = 0;
  mouseRemainderX = mouseRemainderY = 0;
  mouseSampleAt = millis();
  mouseButtonsArmed = false;
  redraw = true;
}


bool isMouseConnected() {
  return bleMouse.isConnected() && bleMouse.isSubscribed();
}

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
bool beginWifiConnection(const String& ssid, const String& password, WifiConnectSource source);
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
void handleWebTeamPage();
void handleWebCattlePage();
void handleWebApiCattleState();
void handleWebApiCattleSelect();
void handleWebApiCattleMark();
void handleWebApiCattleReset();
void handleWebApiCattleLimit();
void handleWebTrainingPage();
void handleWebSystemPage();
void handleWebApiSystemState();
void handleWebApiBrightness();
void handleWebApiClockSync();
void handleWebApiTrainingState();
void handleWebApiTrainingStart();
void handleWebApiTrainingAdd();
void handleWebApiTrainingNext();
void handleWebApiTrainingPrevious();
void handleWebApiTrainingRemove();
void handleWebApiTrainingEnd();
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
void drawTeamMenu();
void drawCattleLimit();
void drawCattleCounter();
void drawCattleResetConfirm();
void loadCattleSession();
void saveCattleSession();
void resetCattleRound();
bool isCattleDrawn(uint8_t number);
uint8_t cattleRemainingCount();
void normalizeCattleSelection();
void markSelectedCattle();
void loadTrainingData();
void saveTrainingData();
void saveTrainingSession();
void startTraining(uint8_t count, const uint8_t* horseIds);
void addTrainingPass();
void removeTrainingPass();
void finishTraining();
String currentDateText();
void drawTrainingCount();
void drawTrainingSelectHorse();
void drawTrainingActive();
void drawTrainingEndConfirm();
void drawTrainingSummary();
void drawTrainingHistory();
void drawTrainingHistoryDetail();
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
// TEAM PENNING - BOIS SORTEADOS
// ============================================================

bool isCattleDrawn(uint8_t number) {
  return (cattleDrawnMask & (1U << number)) != 0;
}

uint8_t cattleRemainingCount() {
  uint8_t count = 0;
  for (uint8_t i = 0; i <= cattleMaxNumber; i++) {
    if (!isCattleDrawn(i)) count++;
  }
  return count;
}

void saveCattleSession() {
  prefs.begin("team-cattle", false);
  prefs.putBool("ready", cattleSessionInitialized);
  prefs.putUChar("max", cattleMaxNumber);
  prefs.putUShort("mask", cattleDrawnMask);
  prefs.putUChar("sel", cattleSelectedNumber);
  prefs.end();
}

void loadCattleSession() {
  prefs.begin("team-cattle", true);
  cattleSessionInitialized = prefs.getBool("ready", false);
  cattleMaxNumber = min<uint8_t>(9, prefs.getUChar("max", 9));
  cattleDrawnMask = prefs.getUShort("mask", 0) & ((1U << (cattleMaxNumber + 1)) - 1U);
  cattleSelectedNumber = min<uint8_t>(cattleMaxNumber, prefs.getUChar("sel", 0));
  prefs.end();
  normalizeCattleSelection();
}

void normalizeCattleSelection() {
  if (!cattleSessionInitialized) return;
  if (!isCattleDrawn(cattleSelectedNumber)) return;
  for (uint8_t step = 1; step <= cattleMaxNumber + 1; step++) {
    uint8_t candidate = (cattleSelectedNumber + step) % (cattleMaxNumber + 1);
    if (!isCattleDrawn(candidate)) {
      cattleSelectedNumber = candidate;
      return;
    }
  }
}

void resetCattleRound() {
  cattleDrawnMask = 0;
  cattleSelectedNumber = 0;
  cattleSessionInitialized = true;
  saveCattleSession();
  redraw = true;
}

void markSelectedCattle() {
  const uint8_t remaining = cattleRemainingCount();
  if (remaining == 1) {
    resetCattleRound();
    return;
  }

  cattleDrawnMask |= (1U << cattleSelectedNumber);
  normalizeCattleSelection();
  saveCattleSession();
  redraw = true;
}


// ============================================================
// TEAM PENNING - TREINO
// ============================================================

String currentDateText() {
  time_t now = time(nullptr);
  if (now < 1704067200) return "Sem data";
  struct tm info;
  localtime_r(&now, &info);
  char text[11];
  strftime(text, sizeof(text), "%d/%m/%y", &info);
  return String(text);
}

void saveTrainingData() {
  prefs.begin("team-train", false);
  prefs.putBytes("session", &trainingSession, sizeof(trainingSession));
  prefs.putBytes("history", trainingHistory, sizeof(trainingHistory));
  prefs.end();
}

void saveTrainingSession() { saveTrainingData(); }

void loadTrainingData() {
  prefs.begin("team-train", true);
  if (prefs.getBytesLength("session") == sizeof(trainingSession))
    prefs.getBytes("session", &trainingSession, sizeof(trainingSession));
  if (prefs.getBytesLength("history") == sizeof(trainingHistory))
    prefs.getBytes("history", trainingHistory, sizeof(trainingHistory));
  prefs.end();

  if (trainingSession.horseCount > MAX_TRAIN_HORSES) trainingSession = TrainingSession();
  if (trainingSession.currentHorse >= trainingSession.horseCount) trainingSession.currentHorse = 0;
  for (uint8_t r = 0; r < MAX_TRAIN_HISTORY; r++) {
    if (trainingHistory[r].horseCount > MAX_TRAIN_HORSES) trainingHistory[r] = TrainingRecord();
  }
}

void startTraining(uint8_t count, const uint8_t* horseIds) {
  trainingSession = TrainingSession();
  trainingSession.active = true;
  trainingSession.horseCount = constrain(count, 1, MAX_TRAIN_HORSES);
  for (uint8_t i = 0; i < trainingSession.horseCount; i++) trainingSession.horseIds[i] = horseIds[i];
  saveTrainingData();
  trainingSummaryHorse = 0;
  redraw = true;
}

void addTrainingPass() {
  if (!trainingSession.active || !trainingSession.horseCount) return;
  if (trainingSession.passes[trainingSession.currentHorse] < 65535)
    trainingSession.passes[trainingSession.currentHorse]++;
  saveTrainingSession();
  redraw = true;
}

void removeTrainingPass() {
  if (!trainingSession.active || !trainingSession.horseCount) return;
  uint16_t& value = trainingSession.passes[trainingSession.currentHorse];
  if (value > 0) value--;
  saveTrainingSession();
  redraw = true;
}

void finishTraining() {
  if (!trainingSession.active) return;
  trainingHistory[1] = trainingHistory[0];
  trainingHistory[0] = TrainingRecord();
  trainingHistory[0].valid = true;
  String date = currentDateText();
  date.toCharArray(trainingHistory[0].date, sizeof(trainingHistory[0].date));
  trainingHistory[0].horseCount = trainingSession.horseCount;
  for (uint8_t i = 0; i < trainingSession.horseCount; i++) {
    trainingHistory[0].horseIds[i] = trainingSession.horseIds[i];
    trainingHistory[0].passes[i] = trainingSession.passes[i];
  }
  trainingSession = TrainingSession();
  trainingSummaryHorse = 0;
  saveTrainingData();
  redraw = true;
}

bool trainingHorseAlreadyChosen(uint8_t horseId, uint8_t beforeSlot) {
  for (uint8_t i = 0; i < beforeSlot; i++) if (trainingSetupHorseIds[i] == horseId) return true;
  return false;
}

void normalizeTrainingCandidate(int direction) {
  for (uint8_t attempt = 0; attempt < MAX_TRAIN_HORSES; attempt++) {
    trainingSetupCandidate = (trainingSetupCandidate + MAX_TRAIN_HORSES + direction) % MAX_TRAIN_HORSES;
    if (!trainingHorseAlreadyChosen(trainingSetupCandidate, trainingSetupSlot)) return;
  }
}

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

    samsungAc.sendExtended();
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

  // Assegura que a rede pré-configurada do usuário (Ribeiro) esteja sempre cadastrada
  if (findSavedNetwork("Ribeiro") < 0) {
    if (savedNetworkCount < MAX_SAVED_NETWORKS) {
      savedNetworks[savedNetworkCount].ssid = "Ribeiro";
      savedNetworks[savedNetworkCount].password = "Jv22019198@";
      savedNetworks[savedNetworkCount].health = SavedNetworkHealth::UNTESTED;
      savedNetworks[savedNetworkCount].failure = SavedNetworkFailure::NONE;
      savedNetworks[savedNetworkCount].lastRssi = -50;
      savedNetworkCount++;
    } else {
      savedNetworks[0].ssid = "Ribeiro";
      savedNetworks[0].password = "Jv22019198@";
      savedNetworks[0].health = SavedNetworkHealth::UNTESTED;
      savedNetworks[0].failure = SavedNetworkFailure::NONE;
      savedNetworks[0].lastRssi = -50;
    }
    saveSavedNetworks();
  }

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
  // Índices mudam quando a lista é compactada; não mantenha uma confirmação antiga.
  if (screen == Screen::WIFI_SAVED_DETAIL || screen == Screen::WIFI_DELETE_CONFIRM) {
    wifiSelectedSavedIndex = -1;
    screen = Screen::WIFI_SAVED_LIST;
    selected = 0;
    redraw = true;
  }

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
    WiFi.disconnect(false, false);
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
  if (!requestWifiScan(true, false)) showToast("WIFI OCUPADO", 1200);
}

bool beginWifiConnection(const String& ssid, const String& password, WifiConnectSource source) {
  if (wifiConnecting || wifiConnectPending || wifiScanRunning) {
    showToast("CONEXAO EM ANDAMENTO", 1200);
    return false;
  }
  if (!ssid.length() || ssid.length() > 32 || password.length() > 63) return false;
  if (source == WifiConnectSource::EDIT_VERIFY) {
    const int8_t duplicate = findSavedNetwork(ssid);
    if (duplicate >= 0 && duplicate != wifiEditingSavedIndex) {
      wifiEditingSavedIndex = -1;
      showToast("SSID JA SALVO", 1200);
      return false;
    }
  }
  wifiPendingSsid = ssid;
  wifiPendingPassword = password;
  wifiConnectSource = source;
  wifiConnectPending = true;
  return true;
}


void processWifiConnection() {
  if (wifiConnectPending && !wifiConnecting) {
    wifiConnectPending = false;
    wifiConnecting = true;
    wifiConnectStartedAt = millis();

    const bool background =
        wifiConnectSource == WifiConnectSource::AUTO_BOOT ||
        wifiConnectSource == WifiConnectSource::AUTO_RECONNECT;

    // Limpa somente a associação STA anterior. Não derruba o AP de setup.
    WiFi.disconnect(false, false);
    if (webUiMode == WebUiMode::SETUP_AP) WiFi.mode(WIFI_AP_STA);
    else WiFi.mode(WIFI_STA);
    delay(20);

    WiFi.begin(wifiPendingSsid.c_str(), wifiPendingPassword.c_str());
    if (!background && !wifiKeyboardActive) {
      screen = Screen::WIFI_CONNECTING;
      redraw = true;
    }
  }

  if (!wifiConnecting) return;

  const bool background =
      wifiConnectSource == WifiConnectSource::AUTO_BOOT ||
      wifiConnectSource == WifiConnectSource::AUTO_RECONNECT;

  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == wifiPendingSsid) {
    wifiConnecting = false;
    wifiAutoCandidatesReady = false;
    wifiAutoCandidateCursor = 0;
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

    // Sincroniza NTP imediatamente com fuso horário de Brasília (UTC-3)
    syncClockFromInternet(-10800);
    lastWeatherAttemptAt = 0;

    wifiResultTitle = "CONECTADO";
    wifiResultDetail = wifiPendingSsid;

    if (wifiConnectSource == WifiConnectSource::WEB_SETUP) {
      lanWebUiDesired = true;
      saveWebUiPreference();
    }

    // Ao sair do AP, encerra o listener antigo e só sobe a Web UI
    // na interface STA depois que a pilha de rede estabilizar.
    if (webUiMode == WebUiMode::SETUP_AP) {
      // Dá tempo ao navegador para ler o IP antes de perder o AP.
      setupExitPending = true;
      setupExitAt = millis() + 10000;
    } else {
      webUiSyncPending = true;
      webUiSyncAt = millis() + 300;
    }

    if (!savedOk) showToast("LIMITE DE 10 REDES", 1800);

    if (background) {
      showToast("WIFI CONECTADO", 900);
    } else if (!wifiKeyboardActive) {
      screen = Screen::WIFI_RESULT;
    }
    redraw = true;
    return;
  }

  const uint32_t timeoutMs = background ? 8000 : WIFI_CONNECT_TIMEOUT_MS;
  if (millis() - wifiConnectStartedAt >= timeoutMs) {
    wifiConnecting = false;

    const wl_status_t finalStatus = WiFi.status();
    bool networkWasVisible = false;
    for (uint8_t i = 0; i < scannedNetworkCount; ++i) {
      if (scannedSsids[i] == wifiPendingSsid) networkWasVisible = true;
    }

    SavedNetworkFailure failure = SavedNetworkFailure::UNKNOWN;
    // WL_CONNECT_FAILED não distingue senha incorreta de todas as outras falhas.
    if (finalStatus == WL_NO_SSID_AVAIL) failure = SavedNetworkFailure::NONE;
    else if (finalStatus == WL_IDLE_STATUS) failure = SavedNetworkFailure::CONNECTION_TIMEOUT;

    WiFi.disconnect(false, false);

    if (wifiConnectSource != WifiConnectSource::EDIT_VERIFY && networkWasVisible && failure != SavedNetworkFailure::NONE) {
      markSavedNetworkFailure(wifiPendingSsid, failure);
    }

    wifiEditingSavedIndex = -1;
    pendingWebEdit = false;
    pendingWebEditIndex = -1;

    if (background) {
      wifiLastRetryScanAt = millis();
      wifiAutoScanPending = true;
    } else {
      wifiResultTitle = "FALHA";
      wifiResultDetail = failure == SavedNetworkFailure::NONE
          ? "REDE INDISPONIVEL" : "VERIFIQUE A REDE";
      if (!wifiKeyboardActive) screen = Screen::WIFI_RESULT;
    }
    redraw = true;
  }
}


void tryAutoConnectStrongest() {
  if (!savedNetworkCount || wifiConnecting || wifiConnectPending ||
      wifiScanRunning || wifiKeyboardActive || webUiMode == WebUiMode::SETUP_AP ||
      WiFi.status() == WL_CONNECTED) return;
  if (!wifiAutoCandidatesReady) {
    requestWifiScan(false, true);
    return;
  }
  while (wifiAutoCandidateCursor < scannedNetworkCount) {
    const int8_t index = findSavedNetwork(scannedSsids[wifiAutoCandidateCursor++]);
    if (index < 0) continue;
    beginWifiConnection(savedNetworks[index].ssid, savedNetworks[index].password,
                        WifiConnectSource::AUTO_RECONNECT);
    return;
  }
  wifiAutoCandidatesReady = false;
  wifiLastRetryScanAt = millis();
}

void processWifiMaintenance() {
  const uint32_t now = millis();
  processWifiScan();
  if (setupExitPending && static_cast<int32_t>(now - setupExitAt) >= 0) {
    setupExitPending = false;
    stopSetupAccessPoint();
  }

  if (webServerStopPending && static_cast<int32_t>(now - webServerStopAt) >= 0) {
    webServerStopPending = false;
    stopLanWebUi();
  }

  if (webUiSyncPending && static_cast<int32_t>(now - webUiSyncAt) >= 0) {
    webUiSyncPending = false;
    syncWebUiState();
  }

  if (wifiAutoScanPending && !wifiConnecting && !wifiConnectPending) {
    wifiAutoScanPending = false;
    tryAutoConnectStrongest();
    wifiLastRetryScanAt = now;
  }

  if (WiFi.status() != WL_CONNECTED && !wifiConnecting && !wifiConnectPending &&
      now - wifiLastRetryScanAt >= WIFI_RETRY_SCAN_MS) {
    wifiLastRetryScanAt = now;
    tryAutoConnectStrongest();
  }

  // syncWebUiState é idempotente após o fix, mas durante um desligamento
  // adiado não pode encerrar o socket antes da resposta HTTP sair.
  if (!webServerStopPending && !webUiSyncPending) {
    syncWebUiState();
  }
}

String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  value.replace("\"", "&quot;");
  return value;
}

String jsonEscape(String value) {
  String result;
  const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    if (c == '"' || c == '\\') { result += '\\'; result += char(c); }
    else if (c == '\n') result += "\\n";
    else if (c == '\r') result += "\\r";
    else if (c == '\t') result += "\\t";
    else if (c < 0x20) { result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
    else result += char(c);
  }
  return result;
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
:root{--bg:#080f14;--panel:#101b22;--panel2:#14232b;--line:#29414a;--accent:#58e4bb;--accent2:#58e4bb;--muted:#a3b7be}
body{background:var(--bg);line-height:1.45}.topbar{background:rgba(8,15,20,.95)}
.card{background:var(--panel);box-shadow:none}.icon{background:rgba(88,228,187,.12);color:var(--accent)}
.btn{font:inherit;font-weight:650;min-height:44px;background:#1a2c35}.btn.primary{background:var(--accent);color:#071b14}
.remote{background:var(--panel);border-color:var(--line);box-shadow:none}.dpad button{background:#25483e}.dpad .ok{background:var(--accent);color:#071b14}
.tile:hover{border-color:var(--accent)}:focus-visible{outline:3px solid var(--accent);outline-offset:3px}input,select{font:inherit}
@media(max-width:520px){.grid{grid-template-columns:repeat(2,minmax(0,1fr))}.tile{min-height:112px}.topbar{grid-template-columns:76px 1fr 76px}}
@media(max-width:340px){.grid{grid-template-columns:1fr}.row{flex-wrap:wrap}}
@media(prefers-reduced-motion:reduce){*{transition:none!important}}
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
      "let scanBusy=false;async function scan(){if(scanBusy)return;scanBusy=true;setupStatus.textContent='Escaneando...';let j=await api('/api/wifi/scan?start=1');while(j.scanning){await new Promise(r=>setTimeout(r,350));j=await api('/api/wifi/scan')}scanBusy=false;if(j.ok===false){setupStatus.textContent=j.message||'Falha no scan';return}nets.innerHTML='';"
      "(j.networks||[]).forEach(n=>{const b=document.createElement('button');b.className='btn';b.textContent=n.ssid+'  '+n.rssi+' dBm'+(n.saved?' • salva':'');b.onclick=()=>choose(n);nets.appendChild(b)});setupStatus.textContent=(j.networks||[]).length+' redes encontradas'}"
      "function choose(n){const p=prompt('Senha de '+n.ssid,n.saved?'(senha salva)':'');if(p!==null)connect(n.ssid,p,n.saved)}"
      "async function connect(s,p,reuse){setupStatus.textContent='Conectando...';const b='ssid='+encodeURIComponent(s)+'&password='+encodeURIComponent(p)+'&reuse='+(reuse?'1':'0');"
      "const j=await api('/api/wifi/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b});if(j.ok)poll()}"
      "let pollTimer;function poll(){clearInterval(pollTimer);let failures=0;pollTimer=setInterval(async()=>{const j=await api('/api/status');if(j.ok===false){if(++failures>=5){clearInterval(pollTimer);setupStatus.textContent='Conexão com o M5 perdida. Consulte o IP na tela do aparelho.'}return}failures=0;setupStatus.textContent=j.message||'';if(!j.connecting){clearInterval(pollTimer);if(j.connected)setupStatus.textContent='Conectado. Entre na mesma rede e abra http://'+j.ip}},900)}"
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
    "<a class='card tile' href='/team-penning'><div class='icon'>"
    "<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'><path d='M5 7h14M5 12h14M5 17h14'/><circle cx='8' cy='7' r='2'/><circle cx='14' cy='12' r='2'/><circle cx='18' cy='17' r='2'/></svg>"
    "</div><div><h2>Team Penning</h2><div class='sub'>Bois sorteados e treinos</div></div></a>"
    "<a class='card tile' href='/sistema'><div class='icon'>"
    "<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'><circle cx='12' cy='12' r='3'/><path d='M19.4 15a1.7 1.7 0 0 0 .3 1.9l.1.1-2.8 2.8-.1-.1a1.7 1.7 0 0 0-1.9-.3 1.7 1.7 0 0 0-1 1.6v.2h-4V21a1.7 1.7 0 0 0-1-1.6 1.7 1.7 0 0 0-1.9.3l-.1.1L4.2 17l.1-.1a1.7 1.7 0 0 0 .3-1.9A1.7 1.7 0 0 0 3 14H2.8v-4H3a1.7 1.7 0 0 0 1.6-1 1.7 1.7 0 0 0-.3-1.9L4.2 7 7 4.2l.1.1A1.7 1.7 0 0 0 9 4.6 1.7 1.7 0 0 0 10 3V2.8h4V3a1.7 1.7 0 0 0 1 1.6 1.7 1.7 0 0 0 1.9-.3l.1-.1L19.8 7l-.1.1a1.7 1.7 0 0 0-.3 1.9 1.7 1.7 0 0 0 1.6 1h.2v4H21a1.7 1.7 0 0 0-1.6 1Z'/></svg>"
    "</div><div><h2>Sistema</h2><div class='sub'>Bateria, relógio, clima e tela</div></div></a>"
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
    "let saveTimer;function pollSave(){clearInterval(saveTimer);let failures=0;saveTimer=setInterval(async()=>{const j=await api('/api/status');if(j.ok===false){if(++failures>=5){clearInterval(saveTimer);saveStatus.textContent='A rede pode ter mudado. Consulte o IP na tela do M5.'}return}failures=0;saveStatus.textContent=j.message||'';if(!j.connecting){clearInterval(saveTimer);if(j.connected)setTimeout(()=>location.reload(),500)}},800)}"
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
  if (!wifiScanRunning && (webServer.arg("start") == "1" || !wifiScanHasResults)) {
    if (!requestWifiScan(false, false)) {
      sendJson(false, "WIFI ocupado. Tente após a conexão ou o teclado terminar.");
      return;
    }
  }
  String json = String("{\"scanning\":") + (wifiScanRunning ? "true" : "false") + ",\"networks\":[";
  bool first = true;
  for (uint8_t i = 0; !wifiScanRunning && i < scannedNetworkCount; ++i) {
    const String& ssid = scannedSsids[i];
    if (!first) json += ",";
    first = false;

    int8_t saved = findSavedNetwork(ssid);
    json += "{\"ssid\":\"" + jsonEscape(ssid) + "\",\"rssi\":" +
            String(scannedRssi[i]) + ",\"saved\":" + (saved >= 0 ? "true" : "false") + "}";
  }

  json += "]}";
  webServer.send(200, "application/json", json);
}

void handleWebApiConnect() {
  if (wifiConnecting || wifiConnectPending || wifiScanRunning || wifiKeyboardActive) {
    sendJson(false, "Uma conexão já está em andamento.");
    return;
  }
  if (!webServer.hasArg("ssid")) {
    sendJson(false, "SSID ausente");
    return;
  }

  String ssid = webServer.arg("ssid");
  String password = webServer.arg("password");
  bool reuse = webServer.arg("reuse") == "1";

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

  if (!beginWifiConnection(ssid, password, WifiConnectSource::WEB_SETUP)) {
    sendJson(false, "SSID ou senha inválidos.");
    return;
  }
  sendJson(true, "Conexao iniciada. Acompanhe na tela do M5.");
}

void handleWebApiStatus() {
  bool connected = WiFi.status() == WL_CONNECTED;
  String message;

  if (wifiConnecting || wifiConnectPending) message = "Conectando em " + wifiPendingSsid;
  else if (connected) message = "Conectado em " + WiFi.SSID();
  else message = wifiResultTitle == "FALHA" ? "Falha ao conectar. Dados anteriores preservados." : "Desconectado";

  String json = "{\"ok\":true,\"connected\":";
  json += connected ? "true" : "false";
  json += ",\"connecting\":";
  json += (wifiConnecting || wifiConnectPending) ? "true" : "false";
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
  const bool enabling = lanWebUiDesired;

  // Primeiro responde ao navegador; só depois altera o listener.
  sendJson(true, enabling ? "Web UI ativada" : "Web UI desativada");

  if (enabling) {
    webUiSyncPending = true;
    webUiSyncAt = millis() + 50;
  } else {
    webServerStopPending = true;
    webServerStopAt = millis() + 300;
  }
}

void handleWebApiTv() {
  if (!webServer.hasArg("device") || !webServer.hasArg("cmd")) {
    sendJson(false, "Parametros ausentes");
    return;
  }

  int device, command;
  if (!parseIndexArg("device", TV_COUNT, device) ||
      !parseIndexArg("cmd", TV_COMMAND_COUNT, command)) {
    sendJson(false, "Comando invalido");
    return;
  }

  if (televisions[device].code[command] == 0) {
    sendJson(false, "Codigo IR pendente");
    return;
  }

  const uint8_t physicalTv = activeTv;
  activeTv = device;
  sendTvCommand(static_cast<TvCommand>(command));
  activeTv = physicalTv;
  sendJson(true, String(televisions[device].name) + " - " +
                 tvCommandName(static_cast<TvCommand>(command)));
}

void handleWebApiAc() {
  if (!webServer.hasArg("device") || !webServer.hasArg("action")) {
    sendJson(false, "Parametros ausentes");
    return;
  }

  int device, action;
  if (!parseIndexArg("device", AC_COUNT, device) ||
      !parseIndexArg("action", AC_MENU_COUNT, action)) {
    sendJson(false, "Comando invalido");
    return;
  }

  const uint8_t physicalAc = activeAc;
  activeAc = device;
  executeAcAction(action);
  activeAc = physicalAc;
  sendJson(true, String(airConditioners[device].name) + " atualizado", acStateJson(device));
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
  if (wifiConnecting || wifiConnectPending || wifiScanRunning || wifiKeyboardActive) {
    sendJson(false, "Aguarde a conexão terminar antes de excluir redes.");
    return;
  }
  if (!webServer.hasArg("index")) {
    sendJson(false, "Indice ausente");
    return;
  }

  int index;
  if (!parseIndexArg("index", savedNetworkCount, index)) {
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
    "let tvBusy=false;async function sendTv(c){if(tvBusy)return;tvBusy=true;try{const j=await api('/api/ir/tv?device='+device.value+'&cmd='+c,{method:'POST'});if(j.ok)flashIr()}finally{tvBusy=false}}</script>";
  webServer.send(200, "text/html; charset=utf-8", webShell("TVs", body));
}

void handleWebAcPage() {
  String body =
    "<div class='card stack'><div class='field'><label>Dispositivo</label><select id='device'>"
    "<option value='0'>Ar Samsung</option><option value='1'>Ar Midea</option>"
    "</select></div><button class='btn primary' onclick='openRemote()'>Abrir controle</button></div>"
    "<div id='remoteBox' class='remote' style='display:none'><div class='row'><b id='remoteName'>Ar</b><div id='irLed' class='ir-led'></div></div>"
    "<div class='ac-display'><div class='sub' style='color:#38505c'>Estado do controle</div><div class='temp'><span id='temp'>--</span><small>°C</small></div>"
    "<div id='modeLabel'>Carregando...</div><div id='stateLabel'></div></div><div class='remote-grid'>"
    "<button class='btn' onclick='ac(0)'>Temp −</button><button class='btn' onclick='ac(1)'>Temp +</button><button class='btn' onclick='ac(7)'>Power</button>"
    "<button class='btn' onclick='ac(2)'>Modo</button><button class='btn' onclick='ac(3)'>Ventilação</button><button class='btn' onclick='ac(4)'>Swing</button>"
    "<button class='btn' onclick='ac(5)'>Turbo</button><button class='btn' onclick='ac(6)'>Sleep</button></div></div>"
    "<script>let acBusy=false;"
    "function renderState(j){if(!j.ok||String(j.device)!==device.value)return;temp.textContent=j.temp;modeLabel.textContent=j.mode;stateLabel.textContent=(j.power?'Ligado':'Desligado')+' • Fan '+j.fan+' • Swing '+(j.swing?'ON':'OFF')+' • Turbo '+(j.turbo?'ON':'OFF')+' • Sleep '+j.sleep}"
    "async function refreshState(){if(acBusy)return;acBusy=true;try{renderState(await api('/api/ir/ac/state?device='+device.value,{cache:'no-store'}))}finally{acBusy=false}}"
    "async function openRemote(){remoteBox.style.display='block';remoteName.textContent=device.options[device.selectedIndex].text;await refreshState()}"
    "async function ac(c){if(acBusy)return;acBusy=true;try{const j=await api('/api/ir/ac?device='+device.value+'&action='+c,{method:'POST'});if(j.ok){renderState(j);flashIr()}}finally{acBusy=false}}"
    "device.onchange=()=>{temp.textContent='--';modeLabel.textContent='Carregando...';stateLabel.textContent='';if(remoteBox.style.display==='block')openRemote()};"
    "setInterval(()=>{if(remoteBox.style.display==='block')refreshState()},1500);</script>";
  webServer.send(200, "text/html; charset=utf-8", webShell("Ar-condicionado", body));
}

void handleWebApiSavedDetail() {
  if (!webServer.hasArg("index")) {
    sendJson(false, "Índice ausente");
    return;
  }
  int index;
  if (!parseIndexArg("index", savedNetworkCount, index)) {
    sendJson(false, "Rede inválida");
    return;
  }
  String json = "{\"ok\":true,\"ssid\":\"" + jsonEscape(savedNetworks[index].ssid) +
                "\",\"health\":\"" + savedNetworkHealthText(savedNetworks[index]) +
                "\",\"reason\":\"" + jsonEscape(savedNetworkFailureText(savedNetworks[index].failure)) + "\"}";
  webServer.send(200, "application/json", json);
}

void handleWebApiSaveNetwork() {
  if (wifiConnecting || wifiConnectPending || wifiScanRunning || wifiKeyboardActive) {
    sendJson(false, "Aguarde a conexão terminar antes de editar redes.");
    return;
  }
  if (!webServer.hasArg("ssid")) {
    sendJson(false, "SSID obrigatório");
    return;
  }

  int index = -1;
  if (webServer.hasArg("index") && webServer.arg("index") != "-1" &&
      !parseIndexArg("index", savedNetworkCount, index)) {
    sendJson(false, "Rede inválida.");
    return;
  }
  String ssid = webServer.arg("ssid");
  String password = webServer.arg("password");
  bool testFirst = webServer.arg("test") == "1";

  if (!ssid.length() || ssid.length() > 32 || password.length() > 63) {
    sendJson(false, "SSID deve ter 1–32 bytes; senha até 63 bytes.");
    return;
  }

  const int8_t duplicate = findSavedNetwork(ssid);
  if ((index >= 0 && duplicate >= 0 && duplicate != index) ||
      (index < 0 && duplicate < 0 && savedNetworkCount >= MAX_SAVED_NETWORKS)) {
    sendJson(false, "Rede duplicada ou limite de 10 redes atingido.");
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


void handleWebTeamPage() {
  String body = "<div class='grid'><a class='card tile' href='/team-penning/bois'><div class='icon'>"
                "<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'><circle cx='12' cy='12' r='8'/><path d='M9 9h6v6H9z'/></svg>"
                "</div><div><h2>Bois sorteados</h2><div class='sub'>Acompanhar números que saíram e os que faltam</div></div></a>"
                "<a class='card tile' href='/team-penning/treino'><div class='icon'>"
                "<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'><path d='M5 19V9m7 10V5m7 14v-7'/><path d='M3 19h18'/></svg>"
                "</div><div><h2>Treino</h2><div class='sub'>Contador de passadas por cavalo</div></div></a></div>";
  webServer.send(200, "text/html; charset=utf-8", webShell("Team Penning", body));
}

void handleWebCattlePage() {
  String body = R"HTML(
<style>
.cattle-wrap{max-width:430px;margin:0 auto;text-align:center}.cattle-title{font-size:18px;font-weight:850;letter-spacing:.08em;margin:8px 0 20px}.cattle-number{font-size:112px;line-height:1;font-weight:900;margin:8px 0 20px}.remaining{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin:18px 0}.remaining span{font-size:25px;font-weight:800;padding:8px 0}.remaining .active{color:var(--yellow)}.cattle-controls{display:grid;grid-template-columns:1fr 1.3fr 1fr;gap:9px}.last-mode{background:#098c52!important}.limit-row{display:grid;grid-template-columns:1fr auto;gap:8px;margin-top:14px}
</style>
<div class='card cattle-wrap' id='cattleCard'><div class='cattle-title'>BOIS SORTEADOS</div><div id='bigNumber' class='cattle-number'>0</div><div class='sub'>FALTAM</div><div id='remaining' class='remaining'></div><div class='cattle-controls'><button class='btn' onclick='move(-1)'>Anterior</button><button class='btn primary' onclick='mark()'>Marcar</button><button class='btn' onclick='move(1)'>Próximo</button></div><div class='limit-row'><select id='limit'></select><button class='btn small' onclick='changeLimit()'>Alterar boiada</button></div><button class='btn danger' style='width:100%;margin-top:10px' onclick='resetRound()'>Zerar boiada</button></div>
<script>
let state={};for(let i=0;i<=9;i++){const o=document.createElement('option');o.value=i;o.textContent='Boiada de 0 até '+i;limit.appendChild(o)}
async function refresh(){state=await api('/api/team/cattle/state');bigNumber.textContent=state.selected;limit.value=state.max;remaining.innerHTML='';(state.remaining||[]).forEach(n=>{const e=document.createElement('span');e.textContent=n;if(n===state.selected)e.className='active';remaining.appendChild(e)});cattleCard.classList.toggle('last-mode',state.remainingCount===1);}
async function move(d){await api('/api/team/cattle/select?dir='+d);refresh()}
async function mark(){await api('/api/team/cattle/mark',{method:'POST'});refresh()}
async function resetRound(){if(confirm('Zerar boiada?')){await api('/api/team/cattle/reset',{method:'POST'});refresh()}}
async function changeLimit(){if(confirm('Alterar o limite e iniciar uma nova boiada?')){await api('/api/team/cattle/limit?max='+limit.value,{method:'POST'});refresh()}}
refresh();setInterval(refresh,1500)
</script>)HTML";
  webServer.send(200, "text/html; charset=utf-8", webShell("Bois sorteados", body));
}

void handleWebApiCattleState() {
  String json = "{\"ok\":true,\"max\":" + String(cattleMaxNumber) + ",\"selected\":" + String(cattleSelectedNumber) + ",\"remainingCount\":" + String(cattleRemainingCount()) + ",\"remaining\":[";
  bool first = true;
  for (uint8_t i = 0; i <= cattleMaxNumber; i++) if (!isCattleDrawn(i)) { if (!first) json += ","; json += String(i); first = false; }
  json += "]}";
  webServer.send(200, "application/json", json);
}

void handleWebApiCattleSelect() {
  int dir = webServer.hasArg("dir") ? webServer.arg("dir").toInt() : 1;
  if (dir < 0) {
    do { cattleSelectedNumber = (cattleSelectedNumber + cattleMaxNumber) % (cattleMaxNumber + 1); } while (isCattleDrawn(cattleSelectedNumber));
  } else {
    do { cattleSelectedNumber = (cattleSelectedNumber + 1) % (cattleMaxNumber + 1); } while (isCattleDrawn(cattleSelectedNumber));
  }
  saveCattleSession();
  redraw = true;
  sendJson(true, "Seleção atualizada");
}

void handleWebApiCattleMark() { markSelectedCattle(); sendJson(true, "Boiada atualizada"); }
void handleWebApiCattleReset() { resetCattleRound(); sendJson(true, "Boiada zerada"); }
void handleWebApiCattleLimit() {
  if (!webServer.hasArg("max")) { sendJson(false, "Limite ausente"); return; }
  int value = webServer.arg("max").toInt();
  if (value < 0 || value > 9) { sendJson(false, "Limite inválido"); return; }
  cattleMaxNumber = value; resetCattleRound(); sendJson(true, "Nova boiada iniciada");
}


void handleWebTrainingPage() {
  String body = R"HTML(
<style>
.train-wrap{max-width:520px;margin:0 auto}.horse-main{text-align:center}.horse-name{font-size:30px;font-weight:900}.pass-number{font-size:96px;line-height:1;font-weight:950;margin:12px 0}.train-controls{display:grid;grid-template-columns:repeat(3,1fr);gap:9px}.setup-grid{display:grid;gap:10px}.history-card{margin-top:12px}.history-horse{padding:10px 0;border-top:1px solid var(--line)}
</style>
<div class='train-wrap'><div id='setup' class='card' style='display:none'><h2>Novo treino</h2><div class='field'><label>Quantos cavalos?</label><select id='trainCount' onchange='renderHorseSelects()'></select></div><div id='horseSelects' class='setup-grid'></div><button class='btn primary' style='width:100%;margin-top:12px' onclick='startTrainingWeb()'>Iniciar treino</button></div>
<div id='active' class='card horse-main' style='display:none'><div class='sub'>TREINO EM ANDAMENTO</div><div id='horseName' class='horse-name'></div><div class='sub'>PASSADAS</div><div id='passNumber' class='pass-number'>0</div><div class='train-controls'><button class='btn' onclick='prevHorse()'>Anterior</button><button class='btn primary' onclick='addPass()'>+ Passada</button><button class='btn' onclick='nextHorse()'>Próximo</button></div><button class='btn danger' style='width:100%;margin-top:9px' onclick='removePass()'>Remover uma passada</button><button class='btn' style='width:100%;margin-top:9px' onclick='endTrainingWeb()'>Encerrar treino</button></div>
<div id='history'></div></div>
<script>
const horseNames=['Linda','Purity','Cavalo 1','Cavalo 2','Cavalo 3'];
for(let i=1;i<=5;i++){let o=document.createElement('option');o.value=i;o.textContent=i;trainCount.appendChild(o)}
function renderHorseSelects(){horseSelects.innerHTML='';for(let i=0;i<+trainCount.value;i++){let s=document.createElement('select');s.id='h'+i;horseNames.forEach((n,id)=>{let o=document.createElement('option');o.value=id;o.textContent=n;s.appendChild(o)});s.value=i;horseSelects.appendChild(s)}}
async function refreshTrain(){let s=await api('/api/team/training/state');setup.style.display=s.active?'none':'block';active.style.display=s.active?'block':'none';if(s.active){horseName.textContent=s.current.name;passNumber.textContent=s.current.passes}history.innerHTML='';(s.history||[]).forEach((r,idx)=>{let c=document.createElement('div');c.className='card history-card';c.innerHTML='<h2>Treino '+(idx+1)+'</h2><div class="sub">'+r.date+'</div>'+r.horses.map(h=>'<div class="history-horse"><b>'+h.name+'</b><div>'+h.passes+' passadas</div></div>').join('');history.appendChild(c)})}
async function startTrainingWeb(){let count=+trainCount.value,ids=[];for(let i=0;i<count;i++)ids.push(+document.getElementById('h'+i).value);if(new Set(ids).size!==ids.length){toastMessage('Escolha cavalos diferentes');return}let body='count='+count+ids.map((v,i)=>'&h'+i+'='+v).join('');await api('/api/team/training/start',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});refreshTrain()}
async function addPass(){await api('/api/team/training/add',{method:'POST'});refreshTrain()}
async function removePass(){await api('/api/team/training/remove',{method:'POST'});refreshTrain()}
async function nextHorse(){await api('/api/team/training/next',{method:'POST'});refreshTrain()}
async function prevHorse(){await api('/api/team/training/previous',{method:'POST'});refreshTrain()}
async function endTrainingWeb(){if(confirm('Encerrar treino?')){await api('/api/team/training/end',{method:'POST'});refreshTrain()}}
renderHorseSelects();refreshTrain();setInterval(refreshTrain,1500)
</script>)HTML";
  webServer.send(200, "text/html; charset=utf-8", webShell("Treino", body));
}

void handleWebApiTrainingState() {
  String json = "{\"ok\":true,\"active\":" + String(trainingSession.active ? "true" : "false");
  if (trainingSession.active && trainingSession.horseCount) {
    uint8_t i = trainingSession.currentHorse;
    json += ",\"current\":{\"index\":" + String(i) + ",\"name\":\"" + jsonEscape(TRAIN_HORSE_NAMES[trainingSession.horseIds[i]]) + "\",\"passes\":" + String(trainingSession.passes[i]) + "}";
  }
  json += ",\"history\":[";
  bool firstRecord = true;
  for (uint8_t r = 0; r < MAX_TRAIN_HISTORY; r++) {
    if (!trainingHistory[r].valid) continue;
    if (!firstRecord) json += ",";
    firstRecord = false;
    json += "{\"date\":\"" + jsonEscape(String(trainingHistory[r].date)) + "\",\"horses\":[";
    for (uint8_t i = 0; i < trainingHistory[r].horseCount; i++) {
      if (i) json += ",";
      json += "{\"name\":\"" + jsonEscape(TRAIN_HORSE_NAMES[trainingHistory[r].horseIds[i]]) + "\",\"passes\":" + String(trainingHistory[r].passes[i]) + "}";
    }
    json += "]}";
  }
  json += "]}";
  webServer.send(200, "application/json", json);
}

void handleWebApiTrainingStart() {
  int count = webServer.hasArg("count") ? webServer.arg("count").toInt() : 0;
  if (count < 1 || count > MAX_TRAIN_HORSES) { sendJson(false, "Quantidade inválida"); return; }
  uint8_t ids[MAX_TRAIN_HORSES];
  uint8_t used = 0;
  for (int i = 0; i < count; i++) {
    String key = "h" + String(i);
    if (!webServer.hasArg(key)) { sendJson(false, "Cavalo ausente"); return; }
    int id = webServer.arg(key).toInt();
    if (id < 0 || id >= MAX_TRAIN_HORSES || (used & (1U << id))) { sendJson(false, "Escolha cavalos diferentes"); return; }
    used |= (1U << id); ids[i] = id;
  }
  startTraining(count, ids);
  screen = Screen::TEAM_TRAIN_ACTIVE;
  sendJson(true, "Treino iniciado");
}
void handleWebApiTrainingAdd() { if (!trainingSession.active) { sendJson(false,"Nenhum treino ativo"); return; } addTrainingPass(); sendJson(true,"Passada adicionada"); }
void handleWebApiTrainingRemove() { if (!trainingSession.active) { sendJson(false,"Nenhum treino ativo"); return; } removeTrainingPass(); sendJson(true,"Passada removida"); }
void handleWebApiTrainingNext() { if (!trainingSession.active) { sendJson(false,"Nenhum treino ativo"); return; } trainingSession.currentHorse=(trainingSession.currentHorse+1)%trainingSession.horseCount; saveTrainingSession(); redraw=true; sendJson(true,"Próximo cavalo"); }
void handleWebApiTrainingPrevious() { if (!trainingSession.active) { sendJson(false,"Nenhum treino ativo"); return; } trainingSession.currentHorse=(trainingSession.currentHorse+trainingSession.horseCount-1)%trainingSession.horseCount; saveTrainingSession(); redraw=true; sendJson(true,"Cavalo anterior"); }
void handleWebApiTrainingEnd() { if (!trainingSession.active) { sendJson(false,"Nenhum treino ativo"); return; } finishTraining(); screen=Screen::TEAM_TRAIN_SUMMARY; sendJson(true,"Treino salvo"); }


void handleWebSystemPage() {
  String body = F(R"HTML(
<section class="card"><h2>Sistema</h2>
<div class="status"><span>Wi-Fi</span><strong>)HTML");
  body += WiFi.status() == WL_CONNECTED ? htmlEscape(WiFi.SSID()) : "Desconectado";
  body += F(R"HTML(</strong></div><div class="status"><span>Web UI</span><strong>)HTML");
  body += webUiMode == WebUiMode::LAN ? "Rede" : (webUiMode == WebUiMode::SETUP_AP ? "AP" : "Desativada");
  body += F(R"HTML(</strong></div><div class="status"><span>Bateria</span><strong>)HTML");
  body += String(constrain(M5.Power.getBatteryLevel(),0,100)) + "%";
  body += F(R"HTML(</strong></div><div class="status"><span>Hora</span><strong>)HTML");
  body += clockTimeText();
  body += F(R"HTML(</strong></div><div class="status"><span>Local</span><strong>)HTML");
  body += htmlEscape(weatherCity.length() ? weatherCity : "Não obtido");
  body += F(R"HTML(</strong></div><div class="status"><span>Temperatura</span><strong>)HTML");
  body += isnan(weatherTemperature) ? "-- °C" : String(weatherTemperature,1) + " °C";
  body += F(R"HTML(</strong></div></section>
<section class="card"><h2>Brilho</h2><div class="actions">
<button onclick="brightness(0)">20%</button><button onclick="brightness(1)">40%</button>
<button onclick="brightness(2)">60%</button><button onclick="brightness(3)">80%</button>
<button onclick="brightness(4)">100%</button></div></section>
<section class="card"><h2>Horário e clima</h2><button onclick="syncClock()">Atualizar agora</button>
<p class="muted">Atualização automática a cada 40 minutos com internet.</p></section>
<script>
async function brightness(v){const r=await api('/api/system/brightness',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'level='+v});if(r.ok)location.reload()}
async function syncClock(){const r=await api('/api/system/clock-sync',{method:'POST'});if(r.ok)location.reload()}
</script>)HTML");
  webServer.send(200, "text/html; charset=utf-8", webShell("Sistema", body));
}

void handleWebApiSystemState() {
  String extra = "\"wifi\":" + String(WiFi.status()==WL_CONNECTED?"true":"false") +
                 ",\"webMode\":\"" + String(webUiMode==WebUiMode::LAN?"lan":(webUiMode==WebUiMode::SETUP_AP?"ap":"off")) + "\"" +
                 ",\"battery\":" + String(constrain(M5.Power.getBatteryLevel(),0,100)) +
                 ",\"time\":\"" + jsonEscape(clockTimeText()) + "\"" +
                 ",\"date\":\"" + jsonEscape(clockDateText()) + "\"" +
                 ",\"city\":\"" + jsonEscape(weatherCity) + "\"" +
                 ",\"temperature\":" + (isnan(weatherTemperature)?String("null"):String(weatherTemperature,1)) +
                 ",\"brightness\":" + String(brightnessIndex);
  sendJson(true, "OK", extra);
}

void handleWebApiBrightness() {
  if (!webServer.hasArg("level")) { sendJson(false,"Nível ausente"); return; }
  int level=webServer.arg("level").toInt();
  if (level<0 || level>4) { sendJson(false,"Nível inválido"); return; }
  brightnessIndex=level; saveSystemSettings();
  if (displayIdleState==DisplayIdleState::ACTIVE) M5.Display.setBrightness(BRIGHTNESS_LEVELS[brightnessIndex]);
  redraw=true; sendJson(true,"Brilho salvo");
}

void handleWebApiClockSync() {
  if (WiFi.status()!=WL_CONNECTED) { sendJson(false,"Sem internet"); return; }
  lastWeatherAttemptAt=millis();
  if (updateLocationAndWeather()) sendJson(true,"Horário e clima atualizados");
  else sendJson(false,"Falha ao atualizar");
}

void configureWebRoutes() {
  webServer.on("/", HTTP_GET, handleWebRoot);
  webServer.on("/wifi", HTTP_GET, handleWebWifiPage);
  webServer.on("/infrared", HTTP_GET, handleWebIrPage);
  webServer.on("/infrared/tv", HTTP_GET, handleWebTvPage);
  webServer.on("/infrared/ac", HTTP_GET, handleWebAcPage);
  webServer.on("/team-penning", HTTP_GET, handleWebTeamPage);
  webServer.on("/team-penning/bois", HTTP_GET, handleWebCattlePage);
  webServer.on("/team-penning/treino", HTTP_GET, handleWebTrainingPage);
  webServer.on("/sistema", HTTP_GET, handleWebSystemPage);
  webServer.on("/api/system/state", HTTP_GET, handleWebApiSystemState);
  webServer.on("/api/system/brightness", HTTP_POST, handleWebApiBrightness);
  webServer.on("/api/system/clock-sync", HTTP_POST, handleWebApiClockSync);
  webServer.on("/api/team/cattle/state", HTTP_GET, handleWebApiCattleState);
  webServer.on("/api/team/cattle/select", HTTP_GET, handleWebApiCattleSelect);
  webServer.on("/api/team/cattle/mark", HTTP_POST, handleWebApiCattleMark);
  webServer.on("/api/team/cattle/reset", HTTP_POST, handleWebApiCattleReset);
  webServer.on("/api/team/cattle/limit", HTTP_POST, handleWebApiCattleLimit);
  webServer.on("/api/team/training/state", HTTP_GET, handleWebApiTrainingState);
  webServer.on("/api/team/training/start", HTTP_POST, handleWebApiTrainingStart);
  webServer.on("/api/team/training/add", HTTP_POST, handleWebApiTrainingAdd);
  webServer.on("/api/team/training/remove", HTTP_POST, handleWebApiTrainingRemove);
  webServer.on("/api/team/training/next", HTTP_POST, handleWebApiTrainingNext);
  webServer.on("/api/team/training/previous", HTTP_POST, handleWebApiTrainingPrevious);
  webServer.on("/api/team/training/end", HTTP_POST, handleWebApiTrainingEnd);
  webServer.on("/api/wifi/scan", HTTP_GET, handleWebApiScan);
  webServer.on("/api/wifi/connect", HTTP_POST, handleWebApiConnect);
  webServer.on("/api/status", HTTP_GET, handleWebApiStatus);
  webServer.on("/api/webui/toggle", HTTP_POST, handleWebApiWebUiToggle);
  webServer.on("/api/ir/tv", HTTP_POST, handleWebApiTv);
  webServer.on("/api/ir/ac", HTTP_POST, handleWebApiAc);
  webServer.on("/api/ir/ac/state", HTTP_GET, handleWebApiAcState);
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
  if (wifiConnecting || wifiConnectPending || wifiScanRunning) {
    showToast("AGUARDE O WIFI", 1200);
    return;
  }
  setupExitPending = false;
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

  // O servidor que atendia o AP é encerrado antes da troca de interface.
  if (webServerRunning) {
    webServer.stop();
    webServerRunning = false;
  }

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  webUiMode = WebUiMode::OFF;
  setupExitPending = false;
  webServerStopPending = false;

  if (lanWebUiDesired && WiFi.status() == WL_CONNECTED) {
    webUiSyncPending = true;
    webUiSyncAt = millis() + 300;
  }
}

void startLanWebUi() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (webUiMode == WebUiMode::LAN && webServerRunning) return;

  webUiMode = WebUiMode::LAN;
  startWebServerIfNeeded();
}

void stopLanWebUi() {
  if (webUiMode == WebUiMode::SETUP_AP) return;

  if (webServerRunning) {
    webServer.stop();
    webServerRunning = false;
  }
  webUiMode = WebUiMode::OFF;
}

void syncWebUiState() {
  if (webUiMode == WebUiMode::SETUP_AP) return;

  const bool shouldRun = lanWebUiDesired && WiFi.status() == WL_CONNECTED;
  if (shouldRun) {
    if (webUiMode != WebUiMode::LAN || !webServerRunning) {
      startLanWebUi();
    }
  } else if (webUiMode == WebUiMode::LAN || webServerRunning) {
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
  wifiKeyboardActive = true;
  auto finish = [&](const String& value) {
    wifiKeyboardActive = false;
    redraw = true;
    return value;
  };

  while (true) {
  // O teclado é modal, mas não deixa a Web UI morrer enquanto está aberto.
  if (webServerRunning) webServer.handleClient();
  processWifiConnection();
  processWifiMaintenance();
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
        if (x == 0) return finish(text);
        if (x == 1) caps = !caps;
        if (x == 2 && text.length()) text.remove(text.length() - 1);
        if (x == 3 && text.length() < 63) text += ' ';
        if (x == 4) {
          cancelled = true;
          return finish(initial);
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
// SISTEMA: STATUS, RELOGIO, CLIMA E DESCANSO DE TELA
// ============================================================

bool isMenuScreen(Screen value) {
  switch (value) {
    case Screen::MAIN:
    case Screen::WIFI_MENU:
    case Screen::WIFI_NETWORKS:
    case Screen::WIFI_SAVED_LIST:
    case Screen::WIFI_SAVED_DETAIL:
    case Screen::IR_TYPES:
    case Screen::TV_LIST:
    case Screen::AC_LIST:
    case Screen::TEAM_MENU:
    case Screen::TEAM_TRAIN_HISTORY:
    case Screen::SETTINGS_MENU:
      return true;
    default:
      return false;
  }
}

void loadSystemSettings() {
  prefs.begin("system", true);
  brightnessIndex = min<uint8_t>(4, prefs.getUChar("bright", 2));
  prefs.end();
  M5.Display.setBrightness(BRIGHTNESS_LEVELS[brightnessIndex]);
}

void saveSystemSettings() {
  prefs.begin("system", false);
  prefs.putUChar("bright", brightnessIndex);
  prefs.end();
}

void loadWeatherCache() {
  prefs.begin("weather", true);
  weatherCity = prefs.getString("city", "");
  weatherTimezone = prefs.getString("tz", "");
  weatherLatitude = prefs.getFloat("lat", 0.0f);
  weatherLongitude = prefs.getFloat("lon", 0.0f);
  weatherTemperature = prefs.getFloat("temp", NAN);
  weatherUpdatedAt = (time_t)prefs.getLong64("updated", 0);
  weatherLocationValid = prefs.getBool("valid", false);
  prefs.end();
}

void saveWeatherCache() {
  prefs.begin("weather", false);
  prefs.putString("city", weatherCity);
  prefs.putString("tz", weatherTimezone);
  prefs.putFloat("lat", weatherLatitude);
  prefs.putFloat("lon", weatherLongitude);
  prefs.putFloat("temp", weatherTemperature);
  prefs.putLong64("updated", (int64_t)weatherUpdatedAt);
  prefs.putBool("valid", weatherLocationValid);
  prefs.end();
}

void initializeClockFromRtc() {
  m5::rtc_datetime_t dt;
  if (!M5.Rtc.getDateTime(&dt)) return;
  tm value = dt.get_tm();
  if (value.tm_year + 1900 < 2024) return;
  time_t epoch = mktime(&value);
  if (epoch <= 0) return;
  timeval tv = {epoch, 0};
  settimeofday(&tv, nullptr);
}

bool clockIsValid() {
  time_t now = time(nullptr);
  tm value;
  localtime_r(&now, &value);
  return value.tm_year + 1900 >= 2024;
}

String clockTimeText() {
  if (!clockIsValid()) return "--:--";
  time_t now = time(nullptr); tm value; localtime_r(&now, &value);
  char out[6]; strftime(out, sizeof(out), "%H:%M", &value); return String(out);
}

String clockDateText() {
  if (!clockIsValid()) return "DATA NAO AJUSTADA";
  time_t now = time(nullptr); tm value; localtime_r(&now, &value);
  char out[11]; strftime(out, sizeof(out), "%d/%m/%Y", &value); return String(out);
}

String jsonStringValue(const String& json, const String& key) {
  String token = "\"" + key + "\"";
  int p = json.indexOf(token); if (p < 0) return "";
  p = json.indexOf(':', p + token.length()); if (p < 0) return "";
  p = json.indexOf('"', p + 1); if (p < 0) return "";
  int end = p + 1;
  while (end < (int)json.length()) {
    if (json[end] == '"' && json[end - 1] != '\\') break;
    end++;
  }
  return end < (int)json.length() ? json.substring(p + 1, end) : "";
}

double jsonNumberValue(const String& json, const String& key, double fallback) {
  String token = "\"" + key + "\"";
  int p = json.indexOf(token); if (p < 0) return fallback;
  p = json.indexOf(':', p + token.length()); if (p < 0) return fallback;
  p++;
  while (p < (int)json.length() && (json[p] == ' ' || json[p] == '\n' || json[p] == '\r' || json[p] == '\t')) p++;
  int end = p;
  while (end < (int)json.length() && (isDigit(json[end]) || json[end]=='-' || json[end]=='+' || json[end]=='.')) end++;
  if (end == p) return fallback;
  return json.substring(p, end).toDouble();
}

int32_t timezoneOffsetSeconds(const String& offset) {
  if (offset.length() < 6) return 0;
  int sign = offset[0] == '-' ? -1 : 1;
  int hours = offset.substring(1, 3).toInt();
  int minutes = offset.substring(4, 6).toInt();
  return sign * (hours * 3600 + minutes * 60);
}

bool syncClockFromInternet(int32_t utcOffsetSeconds) {
  // Configura servidores NTP confiáveis (incluindo ntp.br para máxima precisão no Brasil)
  configTime(utcOffsetSeconds, 0, "a.st1.ntp.br", "pool.ntp.org", "time.google.com");
  tm value;
  if (!getLocalTime(&value, 1500) || value.tm_year + 1900 < 2024) return false;
  M5.Rtc.setDateTime(m5::rtc_datetime_t(value));
  return true;
}

bool updateLocationAndWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  Serial.println("[CLIMA] Buscando localizacao via ipwho.is...");
  HTTPClient locationHttp;
  locationHttp.setConnectTimeout(3500);
  locationHttp.setTimeout(3500);
  if (!locationHttp.begin("http://ipwho.is/?fields=success,city,latitude,longitude,timezone")) return false;
  int locationCode = locationHttp.GET();
  if (locationCode != HTTP_CODE_OK) {
    Serial.printf("[CLIMA] Falha HTTP ipwho.is: %d\n", locationCode);
    locationHttp.end();
    return false;
  }
  String locationJson = locationHttp.getString();
  locationHttp.end();
  if (locationJson.indexOf("\"success\":false") >= 0) {
    Serial.println("[CLIMA] ipwho.is retornou success=false");
    return false;
  }

  String city = jsonStringValue(locationJson, "city");
  double latitude = jsonNumberValue(locationJson, "latitude", 999.0);
  double longitude = jsonNumberValue(locationJson, "longitude", 999.0);
  String timezone = jsonStringValue(locationJson, "id");
  String utcOffset = jsonStringValue(locationJson, "utc");

  Serial.printf("[CLIMA] Cidade: %s, Lat: %.4f, Lon: %.4f\n", city.c_str(), latitude, longitude);

  if (!city.length() || latitude > 90 || longitude > 180) {
    Serial.println("[CLIMA] Coordenadas invalidas");
    return false;
  }

  HTTPClient weatherHttp;
  weatherHttp.setConnectTimeout(3500);
  weatherHttp.setTimeout(3500);
  String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 5) +
               "&longitude=" + String(longitude, 5) +
               "&current=temperature_2m&temperature_unit=celsius&timezone=auto&forecast_days=1";
  if (!weatherHttp.begin(url)) return false;
  int weatherCode = weatherHttp.GET();
  if (weatherCode != HTTP_CODE_OK) {
    Serial.printf("[CLIMA] Falha HTTP open-meteo: %d\n", weatherCode);
    weatherHttp.end();
    return false;
  }
  String weatherJson = weatherHttp.getString();
  weatherHttp.end();
  int currentPos = weatherJson.indexOf("\"current\":");
  String currentSection = (currentPos >= 0) ? weatherJson.substring(currentPos) : weatherJson;
  double temperature = jsonNumberValue(currentSection, "temperature_2m", 999.0);
  Serial.printf("[CLIMA] Temp: %.1f C\n", temperature);
  if (temperature < -80 || temperature > 70) return false;

  weatherCity = city;
  weatherLatitude = latitude;
  weatherLongitude = longitude;
  weatherTimezone = timezone;
  weatherTemperature = temperature;

  // Sincroniza o relógio usando o fuso horário retornado
  int32_t offsetSec = -10800;
  if (utcOffset.length()) {
    offsetSec = timezoneOffsetSeconds(utcOffset);
  } else {
    offsetSec = (int32_t)jsonNumberValue(weatherJson, "utc_offset_seconds", -10800);
  }
  syncClockFromInternet(offsetSec);

  weatherUpdatedAt = time(nullptr);
  weatherLocationValid = true;
  lastWeatherSsid = WiFi.SSID();
  saveWeatherCache();
  redraw = true;
  return true;
}

void processWeatherAndClock() {
  if (WiFi.status() != WL_CONNECTED) return;

  // Se o relógio do sistema ainda não estiver ajustado, verifica se o NTP já respondeu em segundo plano
  if (!clockIsValid()) {
    tm value;
    if (getLocalTime(&value, 50) && value.tm_year + 1900 >= 2024) {
      M5.Rtc.setDateTime(m5::rtc_datetime_t(value));
      redraw = true;
    }
  }

  const uint32_t now = millis();
  const bool newNetwork = (lastWeatherSsid != WiFi.SSID());
  // Se for rede nova ou se já passou o tempo de refresh (40 min), ou se a última tentativa falhou há mais de 45s
  if (newNetwork || now - lastWeatherAttemptAt >= WEATHER_REFRESH_MS) {
    lastWeatherAttemptAt = now;
    if (!updateLocationAndWeather()) {
      // Em caso de falha, marca que tentou para não entrar em loop infinito travando a CPU
      lastWeatherSsid = WiFi.SSID();
      // Agenda próxima tentativa após 45 segundos em vez de tentar a cada 10ms
      lastWeatherAttemptAt = now - WEATHER_REFRESH_MS + 45000UL;
    }
  }
}

void drawWifiIcon(int x, int y, bool connected) {
  auto& d = M5.Display;
  uint16_t col = connected ? UI_GREEN : UI_MUTED;

  // Ponto base
  d.fillRect(x + 5, y + 9, 2, 2, col);

  // Arco 1 (interno)
  d.drawPixel(x + 2, y + 7, col);
  d.drawPixel(x + 3, y + 6, col);
  d.drawFastHLine(x + 4, y + 5, 4, col);
  d.drawPixel(x + 8, y + 6, col);
  d.drawPixel(x + 9, y + 7, col);

  // Arco 2 (externo)
  d.drawPixel(x + 0, y + 4, col);
  d.drawPixel(x + 1, y + 3, col);
  d.drawPixel(x + 2, y + 2, col);
  d.drawFastHLine(x + 3, y + 1, 6, col);
  d.drawPixel(x + 9, y + 2, col);
  d.drawPixel(x + 10, y + 3, col);
  d.drawPixel(x + 11, y + 4, col);

  // Se desconectado, barra diagonal vermelha marcando corte/desconexao
  if (!connected) {
    d.drawLine(x + 0, y + 11, x + 11, y + 0, UI_RED);
    d.drawLine(x + 1, y + 11, x + 12, y + 0, UI_RED);
  }
}

void drawBatteryGauge(int x, int y, int battery, bool charging) {
  auto& d = M5.Display;
  uint16_t bColor = UI_GREEN;
  if (charging) bColor = UI_CYAN;
  else if (battery <= 20) bColor = UI_RED;
  else if (battery <= 45) bColor = UI_YELLOW;

  // Carcaca metalica da bateria com terminal
  d.drawRoundRect(x, y, 20, 10, 2, UI_BORDER);
  d.fillRect(x + 20, y + 2, 2, 6, UI_BORDER);

  // Barra proporcional de nivel (0 a 100% -> 0 a 16px)
  int fillW = map(constrain(battery, 0, 100), 0, 100, 0, 16);
  if (fillW > 0) {
    d.fillRect(x + 2, y + 2, fillW, 6, bColor);
  }

  // Mostrador numerico de porcentagem
  d.setTextDatum(middle_left);
  d.setTextSize(1);
  d.setTextColor(bColor, UI_BG);
  String batStr = charging ? (String(battery) + "%+") : (String(battery) + "%");
  d.drawString(batStr, x + 25, y + 5);
}

void drawStatusBar() {
  auto& d = M5.Display;

  // 1. Wi-Fi icone grafico (centrado e com status conectado / desconectado)
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  drawWifiIcon(104, 7, wifiConnected);

  // 2. Bateria com mostrador (icone grafico preenchido + porcentagem numerica)
  int battery = constrain(M5.Power.getBatteryLevel(), 0, 100);
  bool charging = M5.Power.isCharging();
  drawBatteryGauge(126, 8, battery, charging);

  // 3. Relogio digital de alto contraste
  d.setTextDatum(middle_right);
  d.setTextSize(1);
  d.setTextColor(clockIsValid() ? UI_YELLOW : UI_MUTED, UI_BG);
  d.drawString(clockTimeText(), 234, 13);
}

void updateStatusBarClock() {
  if (!isMenuScreen(screen)) return;
  auto& d = M5.Display;
  d.startWrite();
  d.fillRect(190, 2, 48, 23, UI_BG);
  d.setTextDatum(middle_right);
  d.setTextSize(1);
  d.setTextColor(clockIsValid() ? UI_YELLOW : UI_MUTED, UI_BG);
  d.drawString(clockTimeText(), 234, 13);
  d.endWrite();
}

void drawClockScreen(bool fullClear) {
  auto& d = M5.Display;
  d.setRotation(3);
  d.startWrite();
  if (fullClear) {
    d.fillScreen(UI_BG);
  }

  // Hora em tamanho grande
  if (!fullClear) {
    d.fillRect(20, 8, 200, 42, UI_BG);
  }
  d.setTextDatum(middle_center);
  d.setTextColor(UI_TEXT, UI_BG);
  d.setTextSize(4);
  d.drawString(clockTimeText(), 120, 30);

  // Data
  if (!fullClear) {
    d.fillRect(20, 48, 200, 14, UI_BG);
  }
  d.setTextSize(1);
  d.setTextColor(UI_MUTED, UI_BG);
  d.drawString(clockDateText(), 120, 53);

  if (fullClear) {
    // Linha divisoria sutil (desenhada apenas uma vez na inicialização da tela)
    d.drawFastHLine(20, 64, 200, UI_BORDER);
  }

  // Status Conexao e Bateria
  if (!fullClear) {
    d.fillRect(20, 68, 200, 30, UI_BG);
  }
  String connection;
  if (WiFi.status() == WL_CONNECTED) connection += "WIFI: " + WiFi.SSID();
  else if (webUiMode == WebUiMode::SETUP_AP) connection += "MODO AP ATIVO";
  else connection += "WIFI DESCONECTADO";
  d.setTextDatum(middle_center);
  d.setTextSize(1);
  d.setTextColor(WiFi.status() == WL_CONNECTED ? 0x05BF : UI_MUTED, UI_BG);
  d.drawString(connection, 120, 76);

  int battery = constrain(M5.Power.getBatteryLevel(), 0, 100);
  String batInfo = "BATERIA: " + String(battery) + "%" + (M5.Power.isCharging() ? " (CARREGANDO)" : "");
  d.setTextColor(UI_GREEN, UI_BG);
  d.drawString(batInfo, 120, 90);

  // Clima e Cidade
  if (!fullClear) {
    d.fillRect(20, 98, 200, 36, UI_BG);
  }
  d.setTextColor(UI_TEXT, UI_BG);
  String place = weatherCity.length() ? weatherCity : "CIDADE NAO OBTIDA";
  d.drawString(place.substring(0, 26), 120, 106);

  String temperature = isnan(weatherTemperature) ? "-- C" : String(weatherTemperature, 1) + " C";
  d.setTextSize(2);
  d.setTextColor(UI_YELLOW, UI_BG);
  d.drawString(temperature, 120, 122);

  d.endWrite();
  lastClockRedrawAt = millis();
}

void restoreDisplayFromIdle() {
  displayIdleState = DisplayIdleState::ACTIVE;
  M5.Display.setBrightness(BRIGHTNESS_LEVELS[brightnessIndex]);
  lastUserActivityAt = millis();
  redraw = true;
}

void processDisplayIdle(bool anyButtonPressed) {
  const uint32_t now = millis();
  if (displayIdleState == DisplayIdleState::ACTIVE) {
    if (anyButtonPressed) lastUserActivityAt = now;
    if (now - lastUserActivityAt >= SCREEN_CLOCK_AFTER_MS) {
      displayIdleState = DisplayIdleState::CLOCK;
      clockScreenStartedAt = now;
      M5.Display.setBrightness(SLEEP_BRIGHTNESS);
      drawClockScreen(true);
    }
    return;
  }

  if (anyButtonPressed) {
    if (displayIdleState == DisplayIdleState::OFF) {
      displayIdleState = DisplayIdleState::CLOCK;
      clockScreenStartedAt = now;
      M5.Display.setBrightness(SLEEP_BRIGHTNESS);
      drawClockScreen(true);
    } else {
      restoreDisplayFromIdle();
    }
    return;
  }

  if (displayIdleState == DisplayIdleState::CLOCK) {
    static int lastClockIdleMin = -1;
    time_t tnow = time(nullptr);
    tm tval;
    localtime_r(&tnow, &tval);
    if (tval.tm_min != lastClockIdleMin) {
      lastClockIdleMin = tval.tm_min;
      drawClockScreen(false); // Atualizacao suave sem apagar o display!
    }
    if (now - clockScreenStartedAt >= SCREEN_OFF_AFTER_CLOCK_MS) {
      displayIdleState = DisplayIdleState::OFF;
      M5.Display.setBrightness(0);
    }
  }
}

void drawSettingsMenu() {
  drawTitle("CONFIGURACOES");
  drawListItem(0, 40, "BRILHO", String((brightnessIndex + 1) * 20) + "%");
  drawListItem(1, 67, "HORARIO & CLIMA", clockTimeText());
  drawListItem(2, 94, "DESCANSO DE TELA", "3 + 10 min");
}

void drawSettingsBrightness() {
  drawTitle("BRILHO", String((brightnessIndex + 1) * 20) + "%");
  M5.Display.fillRect(30, 42, 180, 58, UI_BG);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(UI_TEXT, UI_BG);
  M5.Display.setTextSize(5);
  M5.Display.drawString(String((brightnessIndex + 1) * 20) + "%", 120, 76);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(UI_MUTED, UI_BG);
  M5.Display.drawString("B/C ALTERA  A SALVA", 120, 108);
}

void drawSettingsClock() {
  drawTitle("HORARIO & CLIMA", clockIsValid() ? "AJUSTADO" : "SEM DATA");
  M5.Display.fillRect(20, 36, 200, 44, UI_BG);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(UI_TEXT, UI_BG);
  M5.Display.drawString(clockTimeText(), 120, 58);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(UI_MUTED, UI_BG);
  String info = clockDateText();
  if (weatherCity.length() && !isnan(weatherTemperature)) {
    info += "  " + weatherCity + " " + String(weatherTemperature, 1) + " C";
  }
  M5.Display.fillRect(10, 76, 220, 16, UI_BG);
  M5.Display.drawString(info, 120, 84);
  M5.Display.setTextColor(UI_GREEN, UI_BG);
  M5.Display.drawString("A SINCRONIZA PELA INTERNET", 120, 108);
}

void drawSettingsSleep() {
  drawTitle("DESCANSO DE TELA");
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(UI_TEXT, UI_BG);
  M5.Display.drawString("3 MIN", 120, 55);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(UI_MUTED, UI_BG);
  M5.Display.drawString("RELOGIO COM BRILHO BAIXO", 120, 77);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(UI_TEXT, UI_BG);
  M5.Display.drawString("+ 10 MIN", 120, 98);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(UI_MUTED, UI_BG);
  M5.Display.drawString("TELA APAGADA / SISTEMA ATIVO", 120, 116);
}

// ============================================================
// INTERFACE
// ============================================================

void drawFooter() {
  auto& display = M5.Display;
  display.fillRect(0, 120, 240, 15, UI_BG);
  display.setTextSize(1);

  if (toast.length() && millis() < toastUntil) {
    display.fillRoundRect(8, 119, 224, 15, 3, UI_PANEL_ALT);
    display.setTextDatum(middle_center);
    display.setTextColor(UI_GREEN, UI_PANEL_ALT);
    display.drawString(toast, 120, 126);
  } else if (screen == Screen::MOUSE) {
    display.setTextDatum(middle_left);
    display.setTextColor(UI_MUTED, UI_BG);
    display.drawString("A:Esq  B:Dir  C:Sair", 8, 127);

    display.setTextDatum(middle_right);
    display.setTextColor(UI_CYAN, UI_BG);
    display.drawString("AIR MOUSE", 232, 127);
  } else {
    toast = "";
    // Pilulas modernas estilo console gamer
    display.fillRoundRect(8, 121, 14, 11, 2, UI_ORANGE);
    display.setTextColor(UI_BG, UI_ORANGE);
    display.setTextDatum(middle_center);
    display.drawString("A", 15, 126);
    display.setTextColor(UI_TEXT, UI_BG);
    display.setTextDatum(middle_left);
    display.drawString("OK", 25, 126);

    display.fillRoundRect(64, 121, 14, 11, 2, UI_CYAN);
    display.setTextColor(UI_BG, UI_CYAN);
    display.setTextDatum(middle_center);
    display.drawString("B", 71, 126);
    display.setTextColor(UI_TEXT, UI_BG);
    display.setTextDatum(middle_left);
    display.drawString("Descer", 81, 126);

    display.fillRoundRect(140, 121, 14, 11, 2, UI_MUTED);
    display.setTextColor(UI_BG, UI_MUTED);
    display.setTextDatum(middle_center);
    display.drawString("C", 147, 126);
    display.setTextColor(UI_TEXT, UI_BG);
    display.setTextDatum(middle_left);
    display.drawString("Voltar", 157, 126);
  }
}

void drawTitle(const String& title, const String& subtitle) {
  auto& display = M5.Display;
  display.fillRect(0, 0, 240, 27, UI_BG);
  display.fillRoundRect(4, 4, 3, 18, 1, UI_SELECTED);

  display.setTextDatum(middle_left);
  if (title.length() > 8) {
    display.setTextSize(1);
    display.setTextColor(UI_TEXT, UI_BG);
    display.drawString(title, 12, 13);
  } else {
    display.setTextSize(2);
    display.setTextColor(UI_TEXT, UI_BG);
    display.drawString(title, 12, 13);
  }

  display.drawFastHLine(0, 26, 240, UI_BORDER);

  if (isMenuScreen(screen)) {
    drawStatusBar();
  } else if (subtitle.length()) {
    display.setTextDatum(middle_right);
    display.setTextSize(1);
    display.setTextColor(UI_YELLOW, UI_BG);
    display.drawString(subtitle, 234, 13);
  }
}

void drawListItem(uint8_t index, int y, const String& label, const String& detail) {
  auto& display = M5.Display;
  const bool active = selected == index;
  const uint16_t fill = active ? UI_PANEL_ALT : UI_PANEL;
  const uint16_t border = active ? UI_SELECTED : UI_BORDER;

  display.fillRoundRect(6, y, 228, 22, 4, fill);
  display.drawRoundRect(6, y, 228, 22, 4, border);

  if (active) {
    display.fillRoundRect(8, y + 4, 3, 14, 1, UI_SELECTED);
  }

  const int detailWidth = detail.length() ? min<int>(72, static_cast<int>(display.textWidth(detail)) + 10) : 0;
  const int labelX = active ? 16 : 12;
  const int labelRight = 228 - detailWidth;
  const int labelWidth = max(20, labelRight - labelX);

  display.setTextDatum(middle_left);
  display.setTextSize(1);
  display.setTextColor(active ? UI_TEXT : UI_MUTED, fill);

  display.setClipRect(labelX, y + 1, labelWidth, 20);
  int textWidth = display.textWidth(label);
  int offset = 0;

  if (active && textWidth > labelWidth) {
    const int travel = textWidth - labelWidth + 18;
    const uint32_t cycle = 900 + travel * 35 + 900;
    const uint32_t phase = millis() % cycle;
    if (phase < 900) offset = 0;
    else if (phase < cycle - 900) offset = min(travel, int((phase - 900) / 35));
    else offset = travel;
  }

  display.drawString(label, labelX - offset, y + 11);
  display.clearClipRect();

  if (detail.length()) {
    display.setTextDatum(middle_right);
    display.setTextColor(active ? UI_YELLOW : UI_MUTED, fill);
    display.drawString(detail, 226, y + 11);
  }
}

void drawGridButton(uint8_t index, int x, int y, int w, int h,
                    const String& label, const String& value) {
  auto& display = M5.Display;
  const bool active = selected == index;
  const uint16_t fill = active ? UI_PANEL_ALT : UI_PANEL;
  const uint16_t border = active ? UI_SELECTED : UI_BORDER;
  const uint16_t foreground = active ? UI_TEXT : UI_MUTED;

  display.fillRoundRect(x, y, w, h, 4, fill);
  display.drawRoundRect(x, y, w, h, 4, border);
  display.setTextDatum(middle_center);
  display.setTextSize(1);
  display.setTextColor(foreground, fill);

  if (value.length()) {
    display.drawString(label, x + w / 2, y + 9);
    display.setTextColor(active ? UI_YELLOW : UI_MUTED, fill);
    display.drawString(value, x + w / 2, y + 22);
  } else {
    display.drawString(label, x + w / 2, y + h / 2);
  }
}

void drawMain() {
  drawTitle("M5 PERSONAL");
  const char* labels[] = {"Controle IR", "Wi-Fi Hub", "Air Mouse", "Agente IA", "Team Penning", "Ajustes"};
  const char* notes[]  = {"TV e ar-condicionado", "Redes e controle web", "Apontador Bluetooth", "Comando de voz no PC", "Contagem e treinos", "Tela, relogio, repouso"};
  const char* badges[] = {"IR", "WF", "MS", "IA", "TP", "CF"};
  const uint16_t badgeColors[] = {UI_ORANGE, UI_CYAN, UI_GREEN, UI_PURPLE, UI_YELLOW, UI_MUTED};

  auto& d = M5.Display;
  d.fillRect(0, 28, 240, 92, UI_BG);

  const int totalItems = 6;
  int first = selected > 1 ? (selected >= 5 ? 3 : selected - 1) : 0;

  for (int row = 0; row < 3; ++row) {
    const int item = first + row;
    if (item >= totalItems) break;
    const int y = 29 + row * 29;
    const bool active = selected == item;
    const uint16_t cardBg = active ? UI_PANEL_ALT : UI_PANEL;
    const uint16_t border = active ? UI_SELECTED : UI_BORDER;

    d.fillRoundRect(6, y, 218, 26, 4, cardBg);
    d.drawRoundRect(6, y, 218, 26, 4, border);

    if (active) {
      d.fillRoundRect(8, y + 4, 3, 18, 1, UI_SELECTED);
    }

    // Badge de categoria
    d.fillRoundRect(16, y + 4, 22, 18, 3, active ? badgeColors[item] : UI_PANEL);
    d.drawRoundRect(16, y + 4, 22, 18, 3, badgeColors[item]);
    d.setTextDatum(middle_center);
    d.setTextSize(1);
    d.setTextColor(active ? UI_BG : badgeColors[item], active ? badgeColors[item] : UI_PANEL);
    d.drawString(badges[item], 27, y + 13);

    // Titulo
    d.setTextDatum(middle_left);
    d.setTextSize(1);
    d.setTextColor(active ? UI_TEXT : UI_MUTED, cardBg);
    d.drawString(labels[item], 43, y + 8);

    // Subtitulo / Nota
    d.setTextColor(active ? UI_YELLOW : UI_MUTED, cardBg);
    d.drawString(notes[item], 43, y + 19);

    // Indicador direito
    if (active) {
      d.setTextDatum(middle_right);
      d.setTextColor(UI_SELECTED, cardBg);
      d.drawString(">", 216, y + 13);
    }
  }

  // Barra de rolagem lateral moderna
  d.drawFastVLine(232, 31, 84, UI_BORDER);
  int thumbY = 31 + (selected * 70) / (totalItems - 1);
  d.fillRoundRect(230, thumbY, 5, 14, 2, UI_SELECTED);
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
    drawListItem(item, 40 + row * 26, labels[item], details[item]);
  }
}


void drawWifiScanning() {
  drawTitle("WIFI", "ESCANEANDO...");
  M5.Display.fillRect(20, 55, 200, 30, UI_BG);
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
  M5.Display.fillRect(20, 50, 200, 60, UI_BG);
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
    drawListItem(i, 40 + row * 26, savedNetworks[i].ssid, detail);
  }
}


void drawWifiSavedDetail() {
  if (wifiSelectedSavedIndex < 0 || wifiSelectedSavedIndex >= savedNetworkCount) {
    screen = Screen::WIFI_SAVED_LIST;
    selected = 0;
    return;
  }

  drawTitle(savedNetworks[wifiSelectedSavedIndex].ssid, "REDE SALVA");
  const char* labels[] = {"CONECTAR", "EDITAR SSID", "EDITAR SENHA", "EXCLUIR"};
  const uint8_t first = selected >= 3 ? selected - 2 : 0;
  for (uint8_t row = 0; row < 3 && first + row < 4; ++row) {
    const uint8_t item = first + row;
    drawListItem(item, 43 + row * 27, labels[item], item == 3 ? "!" : "");
  }
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
  display.drawRoundRect(4, 3, 232, 52, 7, UI_BORDER);
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


void drawTeamMenu() {
  drawTitle("TEAM PENNING");
  drawListItem(0, 40, "BOIS SORTEADOS");
  drawListItem(1, 67, trainingSession.active ? "TREINO - CONTINUAR" : "TREINO");
  drawListItem(2, 94, "TREINOS SALVOS");
}

void drawCattleLimit() {
  auto& display = M5.Display;
  display.setTextDatum(top_center);
  display.setTextSize(1);
  display.setTextColor(UI_MUTED, UI_BG);
  display.drawString("BOIADA DE 0 ATE", 67, 18);
  display.setTextDatum(middle_center);
  display.setTextSize(7);
  display.setTextColor(UI_TEXT, UI_BG);
  display.drawString(String(cattleMaxNumber), 67, 112);
  display.setTextSize(1);
  display.setTextColor(UI_MUTED, UI_BG);
  display.drawString("A CONFIRMA", 67, 214);
}

void drawCattleCounter() {
  auto& display = M5.Display;
  const uint8_t remaining = cattleRemainingCount();
  if (remaining == 1) {
    display.fillScreen(UI_GREEN);
    display.setTextDatum(top_center);
    display.setTextSize(1);
    display.setTextColor(UI_TEXT, UI_GREEN);
    display.drawString("ULTIMO BOI", 67, 22);
    display.setTextDatum(middle_center);
    display.setTextSize(8);
    display.drawString(String(cattleSelectedNumber), 67, 122);
    display.setTextSize(1);
    display.drawString("A REINICIA A BOIADA", 67, 214);
    return;
  }

  display.setTextDatum(top_center);
  display.setTextSize(1);
  display.setTextColor(UI_MUTED, UI_BG);
  display.drawString("BOIS SORTEADOS", 67, 12);
  display.setTextDatum(middle_center);
  display.setTextSize(8);
  display.setTextColor(UI_TEXT, UI_BG);
  display.drawString(String(cattleSelectedNumber), 67, 88);

  display.drawFastHLine(10, 145, 115, UI_BORDER);
  display.setTextDatum(top_center);
  display.setTextSize(1);
  display.setTextColor(UI_MUTED, UI_BG);
  display.drawString("FALTAM", 67, 154);

  int index = 0;
  for (uint8_t i = 0; i <= cattleMaxNumber; i++) {
    if (isCattleDrawn(i)) continue;
    int col = index % 5;
    int row = index / 5;
    int x = 15 + col * 26;
    int y = 180 + row * 27;
    display.setTextDatum(middle_center);
    display.setTextSize(2);
    display.setTextColor(i == cattleSelectedNumber ? UI_YELLOW : UI_TEXT, UI_BG);
    display.drawString(String(i), x, y);
    index++;
  }
}

void drawCattleResetConfirm() {
  auto& display = M5.Display;
  display.setTextDatum(middle_center);
  display.setTextSize(2);
  display.setTextColor(UI_TEXT, UI_BG);
  display.drawString("ZERAR", 67, 78);
  display.drawString("BOIADA?", 67, 108);
  display.setTextSize(1);
  display.setTextColor(UI_GREEN, UI_BG);
  display.drawString("A CONFIRMA", 67, 164);
  display.setTextColor(UI_MUTED, UI_BG);
  display.drawString("B VOLTA", 67, 190);
}


void drawTrainingCount() {
  drawTitle("TREINO", "QUANTOS CAVALOS?");
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(6);
  M5.Display.setTextColor(UI_TEXT, UI_BG);
  M5.Display.drawString(String(trainingSetupCount), 120, 82);
}

void drawTrainingSelectHorse() {
  drawTitle("TREINO", "ESCOLHA " + String(trainingSetupSlot + 1) + "/" + String(trainingSetupCount));
  M5.Display.fillRoundRect(20, 54, 200, 46, 8, UI_PANEL);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(UI_TEXT, UI_PANEL);
  M5.Display.drawString(TRAIN_HORSE_NAMES[trainingSetupCandidate], 120, 77);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(UI_MUTED, UI_BG);
  M5.Display.drawString("A CONFIRMA", 120, 112);
}

void drawTrainingActive() {
  uint8_t i = trainingSession.currentHorse;
  drawTitle("TREINO", String(i + 1) + "/" + String(trainingSession.horseCount));
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(UI_TEXT, UI_BG);
  M5.Display.drawString(TRAIN_HORSE_NAMES[trainingSession.horseIds[i]], 120, 38);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(UI_MUTED, UI_BG);
  M5.Display.drawString("PASSADAS", 120, 66);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(6);
  M5.Display.setTextColor(UI_SELECTED, UI_BG);
  M5.Display.drawString(String(trainingSession.passes[i]), 120, 94);
}

void drawTrainingEndConfirm() {
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(UI_TEXT, UI_BG);
  M5.Display.drawString("ENCERRAR TREINO?", 120, 52);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(UI_GREEN, UI_BG);
  M5.Display.drawString("A CONFIRMA", 120, 86);
  M5.Display.setTextColor(UI_MUTED, UI_BG);
  M5.Display.drawString("B VOLTA", 120, 106);
}

void drawTrainingSummary() {
  const TrainingRecord& r = trainingHistory[0];
  if (!r.valid || !r.horseCount) { drawTitle("TREINO SALVO"); return; }
  uint8_t i = min<uint8_t>(trainingSummaryHorse, r.horseCount - 1);
  drawTitle("TREINO SALVO", String(r.date));
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(UI_TEXT, UI_BG);
  M5.Display.drawString(TRAIN_HORSE_NAMES[r.horseIds[i]], 120, 44);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(UI_SELECTED, UI_BG);
  M5.Display.drawString(String(r.passes[i]) + " passadas", 120, 76);
}

void drawTrainingHistory() {
  drawTitle("TREINOS SALVOS");
  for (uint8_t i = 0; i < MAX_TRAIN_HISTORY; i++) {
    String label = trainingHistory[i].valid ? "TREINO " + String(i + 1) : "VAZIO";
    String detail = trainingHistory[i].valid ? String(trainingHistory[i].date) : "Sem registro";
    drawListItem(i, 52 + i * 34, label, detail);
  }
}

void drawTrainingHistoryDetail() {
  const TrainingRecord& r = trainingHistory[trainingHistoryRecord];
  if (!r.valid || !r.horseCount) { drawTitle("SEM REGISTRO"); return; }
  uint8_t i = min<uint8_t>(trainingHistoryHorse, r.horseCount - 1);
  drawTitle("TREINO " + String(trainingHistoryRecord + 1), String(r.date));
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(UI_TEXT, UI_BG);
  M5.Display.drawString(TRAIN_HORSE_NAMES[r.horseIds[i]], 120, 44);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(UI_SELECTED, UI_BG);
  M5.Display.drawString(String(r.passes[i]) + " passadas", 120, 76);
}

void startBleMouse() {
  if (!bleMouseStarted) {
    if (!M5.Imu.isEnabled()) {
      M5.Imu.begin();
      if (!M5.Imu.isEnabled()) {
        M5.Imu.begin(&M5.In_I2C, M5.getBoard());
      }
    }
    bleMouse.begin("M5Stick Mouse");
    bleMouseStarted = true;
    Serial.printf("[BLE] Air Mouse BLE inicializado. IMU enabled=%d, type=%d\n", M5.Imu.isEnabled(), (int)M5.Imu.getType());
  }
}

void updateMouseSearchingDots() {
  auto& d = M5.Display;
  d.fillRoundRect(8, 35, 119, 22, 6, UI_PANEL);
  d.setTextSize(1);
  d.setTextDatum(middle_center);
  d.setTextColor(UI_SELECTED, UI_PANEL);
  const char* state = !M5.Imu.isEnabled() ? "ERRO NO SENSOR" :
    !isMouseConnected() ? (bleMouse.isConnected() ? "CONECTANDO" : "PAREAR BLUETOOTH") :
    !mouseCalibrated ? "MANTENHA PARADO" : "PRONTO";
  d.drawString(state, 67, 46);
}

void updateMouseCrosshair(float vx, float vy, uint16_t ballColor) {
  auto& d = M5.Display;
  d.startWrite();
  d.fillCircle(mouseDotX, mouseDotY, 5, UI_BG);
  d.drawCircle(67, 111, 29, UI_BORDER);
  d.drawFastHLine(32, 111, 71, UI_BORDER);
  d.drawFastVLine(67, 76, 71, UI_BORDER);
  mouseDotX = constrain(67 + (int)(vx * 0.45f), 44, 90);
  mouseDotY = constrain(111 + (int)(vy * 0.45f), 88, 134);
  d.fillCircle(mouseDotX, mouseDotY, 4, ballColor);
  d.endWrite();
}

void drawMouseScreen() {
  auto& d = M5.Display;
  d.setTextSize(1);
  d.setTextDatum(middle_left);
  d.setTextColor(UI_MUTED, UI_BG);
  d.drawString("M5 / CONTROLE", 9, 10);
  d.setTextSize(2);
  d.setTextColor(UI_TEXT, UI_BG);
  d.drawString("Air Mouse", 9, 25);
  updateMouseSearchingDots();
  d.fillRect(4, 60, 127, 15, UI_BG);
  d.setTextSize(1);
  d.setTextDatum(middle_center);
  d.setTextColor(UI_MUTED, UI_BG);
  d.drawString(!isMouseConnected() ? "M5Stick Mouse" :
    !mouseCalibrated ? "Apoie por 1 segundo" : "Mova para apontar", 67, 66);
  d.fillRect(28, 76, 79, 72, UI_BG);
  mouseDotX = 67; mouseDotY = 111;
  updateMouseCrosshair(0, 0, UI_SELECTED);
  d.fillRoundRect(8, 155, 119, 53, 7, UI_PANEL);
  d.setTextDatum(middle_left);
  d.setTextColor(UI_SELECTED, UI_PANEL);
  d.drawString("A", 16, 168);
  d.drawString("B", 16, 192);
  d.setTextColor(UI_TEXT, UI_PANEL);
  d.drawString("Esquerdo", 32, 165);
  d.drawString("Direito", 32, 192);
  d.setTextColor(UI_MUTED, UI_PANEL);
  d.drawString("Segure: arraste", 32, 177);
  d.setTextDatum(middle_center);
  d.setTextColor(UI_MUTED, UI_BG);
  d.drawString("C: voltar ao menu", 67, 219);
  d.drawString("Segure C: calibrar", 67, 231);
}

void playWandChime() {
  if (M5.Speaker.isEnabled()) {
    M5.Speaker.tone(1318, 50);
    delay(40);
    M5.Speaker.tone(1568, 50);
    delay(40);
    M5.Speaker.tone(2093, 80);
  }
}

void playFailSound() {
  if (M5.Speaker.isEnabled()) {
    M5.Speaker.tone(420, 120);
  }
}

void initVoiceAiScreen() {
  voiceState = VoiceState::IDLE;
  voiceScrollLine = 0;
  voiceTranscription = "";
  voiceResultTitle = "";
  voiceResultBody = "";
  Serial.println("VOICE_READY");
  redraw = true;
}

void drawWrappedText(int x, int y, int maxW, int maxLines, int startLine, const String& text, uint16_t color) {
  auto& d = M5.Display;
  d.setTextSize(1);
  d.setTextColor(color, UI_BG);
  d.setTextDatum(top_left);

  int curX = x;
  int curY = y;
  int lineIdx = 0;
  int drawnLines = 0;
  String word = "";

  for (size_t i = 0; i <= text.length(); ++i) {
    char c = (i < text.length()) ? text[i] : ' ';
    if (c == ' ' || c == '\n' || i == text.length()) {
      if (word.length() > 0) {
        int wWidth = d.textWidth(word);
        if (curX + wWidth > x + maxW && curX > x) {
          curX = x;
          lineIdx++;
          if (lineIdx >= startLine && drawnLines < maxLines) {
            curY += 12;
            drawnLines++;
          }
        }
        if (lineIdx >= startLine && drawnLines < maxLines) {
          d.drawString(word, curX, curY);
        }
        curX += wWidth + d.textWidth(" ");
        word = "";
      }
      if (c == '\n') {
        curX = x;
        lineIdx++;
        if (lineIdx >= startLine && drawnLines < maxLines) {
          curY += 12;
          drawnLines++;
        }
      }
    } else {
      word += c;
    }
  }
}

void drawVoiceAiScreen() {
  auto& d = M5.Display;
  d.fillRect(0, 0, 135, 240, UI_BG);

  // Topo: Header Holografico
  d.fillRoundRect(6, 4, 123, 24, 4, UI_PANEL);
  d.drawRoundRect(6, 4, 123, 24, 4, UI_BORDER);
  d.fillRoundRect(9, 7, 18, 18, 3, UI_PURPLE);
  d.setTextDatum(middle_center);
  d.setTextSize(1);
  d.setTextColor(UI_BG, UI_PURPLE);
  d.drawString("IA", 18, 16);
  d.setTextDatum(middle_left);
  d.setTextColor(UI_TEXT, UI_PANEL);
  d.drawString("AGENTE IA", 31, 16);

  // Wi-Fi e Bateria compactos no topo
  drawWifiIcon(86, 10, WiFi.status() == WL_CONNECTED);
  int batLevel = constrain(M5.Power.getBatteryLevel(), 0, 100);
  d.drawRoundRect(104, 10, 18, 9, 2, UI_BORDER);
  d.fillRect(122, 12, 2, 5, UI_BORDER);
  int bFill = map(batLevel, 0, 100, 0, 14);
  if (bFill > 0) d.fillRect(106, 12, bFill, 5, batLevel > 20 ? UI_GREEN : UI_RED);

  // Pílula do Agente Ativo
  d.fillRoundRect(6, 31, 123, 19, 3, UI_PANEL_ALT);
  d.drawRoundRect(6, 31, 123, 19, 3, UI_BORDER);
  d.setTextDatum(middle_left);
  d.setTextSize(1);
  d.setTextColor(UI_MUTED, UI_PANEL_ALT);
  d.drawString("Alvo:", 12, 41);
  d.setTextColor(UI_CYAN, UI_PANEL_ALT);
  d.drawString(voiceActiveAgent, 44, 41);

  // Card Principal (x: 6..129, y: 53..168, h: 115)
  d.fillRoundRect(6, 53, 123, 116, 5, UI_PANEL);
  d.drawRoundRect(6, 53, 123, 116, 5, UI_BORDER);

  if (voiceState == VoiceState::IDLE) {
    // Ícone de Microfone Estilizado
    d.drawRoundRect(60, 65, 15, 22, 7, UI_CYAN);
    d.fillRect(63, 68, 9, 16, UI_CYAN);
    d.drawFastHLine(56, 88, 23, UI_BORDER);
    d.drawFastVLine(67, 88, 6, UI_BORDER);
    d.drawFastHLine(61, 94, 13, UI_BORDER);

    d.setTextDatum(middle_center);
    d.setTextSize(1);
    d.setTextColor(UI_YELLOW, UI_PANEL);
    d.drawString("SEGURE [ A ] E FALE", 67, 106);

    // Exemplos de comandos
    d.setTextDatum(middle_left);
    d.setTextColor(UI_MUTED, UI_PANEL);
    d.drawString("Diga por voz:", 14, 122);
    d.setTextColor(UI_CYAN, UI_PANEL);
    d.drawString("• 'abrir vscode'", 14, 134);
    d.drawString("• 'abrir chrome'", 14, 145);
    d.setTextColor(UI_GREEN, UI_PANEL);
    d.drawString("• 'pergunta livre...'", 14, 157);

  } else if (voiceState == VoiceState::LISTENING) {
    d.setTextDatum(middle_center);
    d.setTextSize(1);
    d.setTextColor(UI_YELLOW, UI_PANEL);
    d.drawString("OUVINDO VOZ...", 67, 68);

    // Onda Sonora Animada (VU meter com 7 barras)
    constexpr int barXs[] = {25, 38, 51, 64, 77, 90, 103};
    constexpr int baseH[] = {10, 22, 38, 50, 36, 20, 12};
    for (int b = 0; b < 7; ++b) {
      int h = baseH[b] + (int)(sinf((voiceWavePhase + b * 45) * 0.08f) * 12.0f);
      h = constrain(h, 4, 52);
      int by = 118 - h / 2;
      uint16_t bCol = (b == 3) ? UI_YELLOW : ((b % 2 == 0) ? UI_CYAN : UI_GREEN);
      d.fillRoundRect(barXs[b], by, 7, h, 2, bCol);
    }

    d.setTextDatum(middle_center);
    d.setTextColor(UI_MUTED, UI_PANEL);
    d.drawString("Fale seu comando", 67, 154);

  } else if (voiceState == VoiceState::THINKING) {
    d.setTextDatum(middle_center);
    d.setTextSize(1);
    d.setTextColor(UI_CYAN, UI_PANEL);
    d.drawString("PROCESSANDO IA...", 67, 72);

    // Barra de progresso animada
    int pW = ((millis() / 30) % 80) + 10;
    d.drawRoundRect(27, 90, 82, 8, 2, UI_BORDER);
    d.fillRect(29, 92, pW, 4, UI_CYAN);

    d.setTextColor(UI_MUTED, UI_PANEL);
    d.drawString("Aguardando resposta", 67, 114);
    if (voiceTranscription.length() > 0) {
      d.setTextColor(UI_YELLOW, UI_PANEL);
      d.drawString("\"" + voiceTranscription.substring(0, 16) + "...\"", 67, 138);
    }

  } else if (voiceState == VoiceState::RESULT) {
    // Exibe o comando ouvido
    d.setTextDatum(top_left);
    d.setTextSize(1);
    d.setTextColor(UI_YELLOW, UI_PANEL);
    String qLine = "> " + voiceTranscription;
    if (qLine.length() > 18) qLine = qLine.substring(0, 16) + "..";
    d.drawString(qLine, 12, 58);

    // Título do resultado (ex: AGENTE ATIVADO / RESPOSTA IA)
    d.setTextColor(UI_CYAN, UI_PANEL);
    d.drawString(voiceResultTitle, 12, 70);
    d.drawFastHLine(12, 80, 111, UI_BORDER);

    // Corpo da resposta com quebra automática de linha
    drawWrappedText(12, 84, 111, 6, voiceScrollLine, voiceResultBody, UI_TEXT);

    // Indicador de rolagem se houver texto
    d.setTextDatum(bottom_right);
    d.setTextColor(UI_MUTED, UI_PANEL);
    d.drawString("[B] Rolar", 124, 166);
  }

  // Painel de Comandos e Ajuda (y: 173..218)
  d.fillRoundRect(6, 173, 123, 44, 4, UI_PANEL);
  d.drawRoundRect(6, 173, 123, 44, 4, UI_BORDER);
  d.setTextDatum(middle_left);
  d.setTextSize(1);

  d.fillRoundRect(12, 178, 12, 11, 2, UI_ORANGE);
  d.setTextColor(UI_BG, UI_ORANGE);
  d.setTextDatum(middle_center);
  d.drawString("A", 18, 183);
  d.setTextDatum(middle_left);
  d.setTextColor(UI_TEXT, UI_PANEL);
  d.drawString("Segure p/ Falar", 30, 183);

  d.fillRoundRect(12, 194, 12, 11, 2, UI_CYAN);
  d.setTextColor(UI_BG, UI_CYAN);
  d.setTextDatum(middle_center);
  d.drawString("B", 18, 199);
  d.setTextDatum(middle_left);
  d.setTextColor(UI_MUTED, UI_PANEL);
  d.drawString("Rolar / Nova fala", 30, 199);

  // Rodapé
  d.setTextDatum(middle_center);
  d.setTextColor(UI_MUTED, UI_BG);
  d.drawString("C: voltar ao menu", 67, 229);
}

void processVoiceAiScreen() {
  lastUserActivityAt = millis();

  if (buttonC.wasClicked() || buttonC.wasHeld()) {
    goBack();
    return;
  }

  // Botão B: se estiver em RESULT, rola o texto ou limpa
  if (M5.BtnB.wasClicked()) {
    if (voiceState == VoiceState::RESULT) {
      voiceScrollLine += 2;
      if (voiceScrollLine > 20) voiceScrollLine = 0;
      redraw = true;
    } else {
      voiceState = VoiceState::IDLE;
      redraw = true;
    }
  }

  // Pressionou Botão A: Inicia gravação Push-to-Talk
  if (M5.BtnA.wasPressed()) {
    voiceState = VoiceState::LISTENING;
    voiceTranscription = "";
    voiceResultTitle = "";
    voiceResultBody = "";
    voiceScrollLine = 0;
    voiceWavePhase = 0;
    Serial.println("VOICE_START");
    if (M5.Speaker.isEnabled()) M5.Speaker.tone(1200, 40);
    redraw = true;
  }

  // Segurando Botão A: anima as ondas de áudio na tela
  if (M5.BtnA.isPressed()) {
    voiceWavePhase = (voiceWavePhase + 1) % 360;
    if (millis() - voiceAnimTimer > 40) {
      voiceAnimTimer = millis();
      redraw = true;
    }
  }

  // Soltou Botão A: Envia parada e entra no estado de processamento
  if (M5.BtnA.wasReleased()) {
    voiceState = VoiceState::THINKING;
    Serial.println("VOICE_STOP");
    if (M5.Speaker.isEnabled()) M5.Speaker.tone(1600, 40);
    redraw = true;
  }

  // Lê respostas seriais enviadas pela ponte no PC
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    // Parse simples e seguro de JSON recebido
    if (line.startsWith("{") && line.endsWith("}")) {
      int typeIdx = line.indexOf("\"type\":\"");
      if (typeIdx != -1) {
        int typeEnd = line.indexOf("\"", typeIdx + 8);
        String msgType = line.substring(typeIdx + 8, typeEnd);

        if (msgType == "READY") {
          voiceActiveAgent = "IA Geral";
          redraw = true;
        } else if (msgType == "TRANS") {
          int tIdx = line.indexOf("\"text\":\"");
          if (tIdx != -1) {
            int tEnd = line.indexOf("\"", tIdx + 8);
            voiceTranscription = line.substring(tIdx + 8, tEnd);
            redraw = true;
          }
        } else if (msgType == "STATUS") {
          // Status temporário
          redraw = true;
        } else if (msgType == "RESULT") {
          // Extrai agent, title, body
          int aIdx = line.indexOf("\"agent\":\"");
          if (aIdx != -1) {
            int aEnd = line.indexOf("\"", aIdx + 9);
            voiceActiveAgent = line.substring(aIdx + 9, aEnd);
          }
          int titIdx = line.indexOf("\"title\":\"");
          if (titIdx != -1) {
            int titEnd = line.indexOf("\"", titIdx + 9);
            voiceResultTitle = line.substring(titIdx + 9, titEnd);
          }
          int bIdx = line.indexOf("\"body\":\"");
          if (bIdx != -1) {
            int bEnd = line.lastIndexOf("\"");
            if (bEnd > bIdx + 8) {
              voiceResultBody = line.substring(bIdx + 8, bEnd);
            }
          }
          voiceState = VoiceState::RESULT;
          voiceScrollLine = 0;
          playWandChime();
          redraw = true;
        }
      }
    }
  }
}

void drawScreen() {
  auto& display = M5.Display;
  const bool portrait = screen == Screen::TEAM_CATTLE_LIMIT ||
                        screen == Screen::TEAM_CATTLE_COUNTER ||
                        screen == Screen::TEAM_CATTLE_RESET_CONFIRM ||
                        screen == Screen::MOUSE ||
                        screen == Screen::VOICE_AI;
  display.setRotation(portrait ? 0 : 3);
  display.startWrite();

  static Screen lastRenderedScreen = (Screen)255;
  static bool lastRenderedPortrait = false;

  if (forceFullRedraw || screen != lastRenderedScreen || portrait != lastRenderedPortrait) {
    display.fillScreen(UI_BG);
    forceFullRedraw = false;
    lastRenderedScreen = screen;
    lastRenderedPortrait = portrait;
  }

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
    case Screen::TEAM_MENU: drawTeamMenu(); break;
    case Screen::TEAM_CATTLE_LIMIT: drawCattleLimit(); break;
    case Screen::TEAM_CATTLE_COUNTER: drawCattleCounter(); break;
    case Screen::TEAM_CATTLE_RESET_CONFIRM: drawCattleResetConfirm(); break;
    case Screen::TEAM_TRAIN_COUNT: drawTrainingCount(); break;
    case Screen::TEAM_TRAIN_SELECT_HORSE: drawTrainingSelectHorse(); break;
    case Screen::TEAM_TRAIN_ACTIVE: drawTrainingActive(); break;
    case Screen::TEAM_TRAIN_END_CONFIRM: drawTrainingEndConfirm(); break;
    case Screen::TEAM_TRAIN_SUMMARY: drawTrainingSummary(); break;
    case Screen::TEAM_TRAIN_HISTORY: drawTrainingHistory(); break;
    case Screen::TEAM_TRAIN_HISTORY_DETAIL: drawTrainingHistoryDetail(); break;
    case Screen::SETTINGS_MENU: drawSettingsMenu(); break;
    case Screen::SETTINGS_BRIGHTNESS: drawSettingsBrightness(); break;
    case Screen::SETTINGS_CLOCK: drawSettingsClock(); break;
    case Screen::SETTINGS_SLEEP: drawSettingsSleep(); break;
    case Screen::MOUSE:          drawMouseScreen(); break;
    case Screen::VOICE_AI:     drawVoiceAiScreen(); break;
  }

  if (!portrait) drawFooter();
  display.endWrite();
  redraw = false;
}

// ============================================================
// NAVEGACAO
// ============================================================

uint8_t itemCount() {
  switch (screen) {
    case Screen::MAIN:                return 6;
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
    case Screen::TEAM_MENU: return 3;
    case Screen::TEAM_CATTLE_LIMIT: return cattleMaxNumber + 1;
    case Screen::TEAM_CATTLE_COUNTER: return cattleMaxNumber + 1;
    case Screen::TEAM_CATTLE_RESET_CONFIRM: return 1;
    case Screen::TEAM_TRAIN_COUNT: return MAX_TRAIN_HORSES;
    case Screen::TEAM_TRAIN_SELECT_HORSE: return MAX_TRAIN_HORSES;
    case Screen::TEAM_TRAIN_ACTIVE: return trainingSession.horseCount ? trainingSession.horseCount : 1;
    case Screen::TEAM_TRAIN_END_CONFIRM: return 1;
    case Screen::TEAM_TRAIN_SUMMARY: return trainingHistory[0].horseCount ? trainingHistory[0].horseCount : 1;
    case Screen::TEAM_TRAIN_HISTORY: return MAX_TRAIN_HISTORY;
    case Screen::TEAM_TRAIN_HISTORY_DETAIL: return trainingHistory[trainingHistoryRecord].horseCount ? trainingHistory[trainingHistoryRecord].horseCount : 1;
    case Screen::SETTINGS_MENU: return 3;
    case Screen::SETTINGS_BRIGHTNESS: return 5;
    case Screen::SETTINGS_CLOCK: return 1;
    case Screen::SETTINGS_SLEEP: return 1;
    case Screen::MOUSE: return 1;
    case Screen::VOICE_AI: return 1;
  }
  return 1;
}

void nextItem() {
  if (screen == Screen::SETTINGS_BRIGHTNESS) {
    brightnessIndex = (brightnessIndex + 1) % 5;
    M5.Display.setBrightness(BRIGHTNESS_LEVELS[brightnessIndex]);
  } else if (screen == Screen::TEAM_CATTLE_LIMIT) {
    cattleMaxNumber = (cattleMaxNumber + 1) % 10;
  } else if (screen == Screen::TEAM_CATTLE_COUNTER) {
    do { cattleSelectedNumber = (cattleSelectedNumber + 1) % (cattleMaxNumber + 1); } while (isCattleDrawn(cattleSelectedNumber));
    saveCattleSession();
  } else if (screen == Screen::TEAM_TRAIN_COUNT) {
    trainingSetupCount = trainingSetupCount % MAX_TRAIN_HORSES + 1;
  } else if (screen == Screen::TEAM_TRAIN_SELECT_HORSE) {
    normalizeTrainingCandidate(1);
  } else if (screen == Screen::TEAM_TRAIN_ACTIVE) {
    trainingSession.currentHorse = (trainingSession.currentHorse + 1) % trainingSession.horseCount; saveTrainingSession();
  } else if (screen == Screen::TEAM_TRAIN_SUMMARY) {
    trainingSummaryHorse = (trainingSummaryHorse + 1) % max<uint8_t>(1, trainingHistory[0].horseCount);
  } else if (screen == Screen::TEAM_TRAIN_HISTORY_DETAIL) {
    trainingHistoryHorse = (trainingHistoryHorse + 1) % max<uint8_t>(1, trainingHistory[trainingHistoryRecord].horseCount);
  } else {
    selected = (selected + 1) % itemCount();
  }
  redraw = true;
}

void previousItem() {
  if (screen == Screen::SETTINGS_BRIGHTNESS) {
    brightnessIndex = (brightnessIndex + 4) % 5;
    M5.Display.setBrightness(BRIGHTNESS_LEVELS[brightnessIndex]);
  } else if (screen == Screen::TEAM_CATTLE_LIMIT) {
    cattleMaxNumber = (cattleMaxNumber + 9) % 10;
  } else if (screen == Screen::TEAM_CATTLE_COUNTER) {
    do { cattleSelectedNumber = (cattleSelectedNumber + cattleMaxNumber) % (cattleMaxNumber + 1); } while (isCattleDrawn(cattleSelectedNumber));
    saveCattleSession();
  } else if (screen == Screen::TEAM_TRAIN_COUNT) {
    trainingSetupCount = trainingSetupCount == 1 ? MAX_TRAIN_HORSES : trainingSetupCount - 1;
  } else if (screen == Screen::TEAM_TRAIN_SELECT_HORSE) {
    normalizeTrainingCandidate(-1);
  } else if (screen == Screen::TEAM_TRAIN_ACTIVE) {
    trainingSession.currentHorse = (trainingSession.currentHorse + trainingSession.horseCount - 1) % trainingSession.horseCount; saveTrainingSession();
  } else if (screen == Screen::TEAM_TRAIN_SUMMARY) {
    trainingSummaryHorse = (trainingSummaryHorse + max<uint8_t>(1, trainingHistory[0].horseCount) - 1) % max<uint8_t>(1, trainingHistory[0].horseCount);
  } else if (screen == Screen::TEAM_TRAIN_HISTORY_DETAIL) {
    uint8_t count=max<uint8_t>(1,trainingHistory[trainingHistoryRecord].horseCount); trainingHistoryHorse=(trainingHistoryHorse+count-1)%count;
  } else {
    selected = (selected + itemCount() - 1) % itemCount();
  }
  redraw = true;
}

void goBack() {
  switch (screen) {
    case Screen::MAIN:
      showToast("MENU PRINCIPAL");
      return;

    case Screen::MOUSE:
      if (isMouseConnected()) {
        bleMouse.releaseAll();
      }
      screen = Screen::MAIN;
      selected = 2;
      redraw = true;
      return;

    case Screen::VOICE_AI:
      screen = Screen::MAIN;
      selected = 3;
      redraw = true;
      return;

    case Screen::WIFI_MENU:
    case Screen::SETTINGS_MENU:
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
    case Screen::TEAM_MENU:
      screen = Screen::MAIN;
      break;

    case Screen::TEAM_CATTLE_LIMIT:
    case Screen::TEAM_CATTLE_COUNTER:
      screen = Screen::TEAM_MENU;
      break;

    case Screen::TEAM_CATTLE_RESET_CONFIRM:
      screen = Screen::TEAM_CATTLE_COUNTER;
      break;

    case Screen::TEAM_TRAIN_COUNT:
    case Screen::TEAM_TRAIN_SUMMARY:
    case Screen::TEAM_TRAIN_HISTORY:
      screen = Screen::TEAM_MENU;
      break;

    case Screen::TEAM_TRAIN_SELECT_HORSE:
      if (trainingSetupSlot > 0) { trainingSetupSlot--; trainingSetupCandidate = trainingSetupHorseIds[trainingSetupSlot]; }
      else screen = Screen::TEAM_TRAIN_COUNT;
      break;

    case Screen::TEAM_TRAIN_ACTIVE:
      screen = Screen::TEAM_MENU;
      break;

    case Screen::TEAM_TRAIN_END_CONFIRM:
      screen = Screen::TEAM_TRAIN_ACTIVE;
      break;

    case Screen::SETTINGS_BRIGHTNESS:
    case Screen::SETTINGS_CLOCK:
    case Screen::SETTINGS_SLEEP:
      screen = Screen::SETTINGS_MENU;
      break;

    case Screen::TEAM_TRAIN_HISTORY_DETAIL:
      screen = Screen::TEAM_TRAIN_HISTORY;
      selected = trainingHistoryRecord;
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
      if (selected == 0) screen = Screen::IR_TYPES;
      else if (selected == 1) screen = Screen::WIFI_MENU;
      else if (selected == 2) {
        screen = Screen::MOUSE;
        startBleMouse();
        resetMouseCalibration();
        mouseCalibSamples = 0;
        mouseSumGx = 0.0f;
        mouseSumGy = 0.0f;
        mouseSumGz = 0.0f;
        mouseSmoothDx = 0.0f;
        mouseSmoothDy = 0.0f;
      }
      else if (selected == 3) {
        screen = Screen::VOICE_AI;
        initVoiceAiScreen();
      }
      else if (selected == 4) screen = Screen::TEAM_MENU;
      else screen = Screen::SETTINGS_MENU;
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

    case Screen::TEAM_MENU:
      if (selected == 0) screen = cattleSessionInitialized ? Screen::TEAM_CATTLE_COUNTER : Screen::TEAM_CATTLE_LIMIT;
      else if (selected == 1) {
        if (trainingSession.active) screen = Screen::TEAM_TRAIN_ACTIVE;
        else { trainingSetupCount = 1; screen = Screen::TEAM_TRAIN_COUNT; }
      } else screen = Screen::TEAM_TRAIN_HISTORY;
      selected = 0;
      break;

    case Screen::TEAM_CATTLE_LIMIT:
      cattleDrawnMask = 0;
      cattleSelectedNumber = 0;
      cattleSessionInitialized = true;
      saveCattleSession();
      screen = Screen::TEAM_CATTLE_COUNTER;
      break;

    case Screen::TEAM_CATTLE_COUNTER:
      markSelectedCattle();
      break;

    case Screen::TEAM_CATTLE_RESET_CONFIRM:
      resetCattleRound();
      screen = Screen::TEAM_CATTLE_COUNTER;
      break;

    case Screen::TEAM_TRAIN_COUNT:
      trainingSetupSlot = 0; trainingSetupCandidate = 0;
      while (trainingHorseAlreadyChosen(trainingSetupCandidate, trainingSetupSlot)) normalizeTrainingCandidate(1);
      screen = Screen::TEAM_TRAIN_SELECT_HORSE;
      break;

    case Screen::TEAM_TRAIN_SELECT_HORSE:
      trainingSetupHorseIds[trainingSetupSlot] = trainingSetupCandidate;
      trainingSetupSlot++;
      if (trainingSetupSlot >= trainingSetupCount) { startTraining(trainingSetupCount, trainingSetupHorseIds); screen = Screen::TEAM_TRAIN_ACTIVE; }
      else { trainingSetupCandidate = 0; if (trainingHorseAlreadyChosen(trainingSetupCandidate, trainingSetupSlot)) normalizeTrainingCandidate(1); }
      break;

    case Screen::TEAM_TRAIN_ACTIVE:
      addTrainingPass();
      break;

    case Screen::TEAM_TRAIN_END_CONFIRM:
      finishTraining(); screen = Screen::TEAM_TRAIN_SUMMARY;
      break;

    case Screen::TEAM_TRAIN_SUMMARY:
      screen = Screen::TEAM_MENU; selected = 0;
      break;

    case Screen::TEAM_TRAIN_HISTORY:
      if (trainingHistory[selected].valid) { trainingHistoryRecord=selected; trainingHistoryHorse=0; screen=Screen::TEAM_TRAIN_HISTORY_DETAIL; }
      break;

    case Screen::TEAM_TRAIN_HISTORY_DETAIL:
      screen = Screen::TEAM_TRAIN_HISTORY; selected = trainingHistoryRecord;
      break;


    case Screen::SETTINGS_MENU:
      if (selected == 0) screen = Screen::SETTINGS_BRIGHTNESS;
      else if (selected == 1) screen = Screen::SETTINGS_CLOCK;
      else screen = Screen::SETTINGS_SLEEP;
      selected = 0;
      break;

    case Screen::SETTINGS_BRIGHTNESS:
      saveSystemSettings();
      screen = Screen::SETTINGS_MENU;
      selected = 0;
      showToast("BRILHO SALVO", 1000);
      break;

    case Screen::SETTINGS_CLOCK:
      if (WiFi.status() == WL_CONNECTED) {
        showToast("SINCRONIZANDO...", 1000);
        syncClockFromInternet(-10800);
        lastWeatherAttemptAt = 0;
        if (updateLocationAndWeather()) showToast("HORARIO ATUALIZADO", 1200);
        else if (clockIsValid()) showToast("NTP SINCRONIZADO", 1200);
        else showToast("FALHA NA INTERNET", 1400);
      } else showToast("SEM INTERNET", 1200);
      break;

    case Screen::SETTINGS_SLEEP:
      screen = Screen::SETTINGS_MENU;
      selected = 0;
      break;

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
  M5.Imu.begin();
  if (!M5.Imu.isEnabled()) {
    M5.Imu.begin(&M5.In_I2C, M5.getBoard());
  }
  Serial.printf("[BOOT] IMU inicializado: enabled=%d, type=%d\n", M5.Imu.isEnabled(), (int)M5.Imu.getType());
  M5.Display.setRotation(3);
  M5.Display.setBrightness(153);
  M5.Display.setTextFont(1);
  M5.Display.setTextWrap(false);

  buttonC.begin();
  loadSystemSettings();
  loadWeatherCache();
  initializeClockFromRtc();
  lastUserActivityAt = millis();
  loadAcStates();
  loadSavedNetworks();
  loadCattleSession();
  loadTrainingData();
  configureWebRoutes();

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.setAutoReconnect(true);
  wifiAutoScanPending = false;
  wifiLastRetryScanAt = millis();

  tvIr.begin();

  samsungAc.stateReset(true, false);
  samsungAc.begin();
  applySamsungState(airConditioners[0].state);

  mideaAc.stateReset();
  mideaAc.begin();
  applyMideaState(airConditioners[1].state);

  drawScreen();

  // Inicia conexão automática em segundo plano à rede pré-configurada Ribeiro
  beginWifiConnection("Ribeiro", "Jv22019198@", WifiConnectSource::AUTO_BOOT);

  Serial.println();
  Serial.println("M5 PERSONAL v0.9.6 - AIR MOUSE & UI iniciado.");
  Serial.println("Rotacao 3: emissor IR deve ficar para cima.");
}

void loop() {
  if (screen != Screen::MOUSE && screen != Screen::VOICE_AI) {
    if (webServerRunning) webServer.handleClient();
    processWifiConnection();
    processWifiMaintenance();
  }

  M5.update();
  buttonC.update();
  if (screen != Screen::MOUSE && screen != Screen::VOICE_AI) processWeatherAndClock();

  static bool previousCPressed = false;
  const bool cPressedNow = buttonC.stablePressed;
  const bool anyButtonPressed = M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || (cPressedNow && !previousCPressed);
  previousCPressed = cPressedNow;
  const DisplayIdleState idleBeforeInput = displayIdleState;
  if (screen == Screen::MOUSE || screen == Screen::VOICE_AI) lastUserActivityAt = millis();
  processDisplayIdle(anyButtonPressed);
  if (idleBeforeInput != DisplayIdleState::ACTIVE || displayIdleState != DisplayIdleState::ACTIVE) {
    delay(10);
    return;
  }

  if (screen == Screen::VOICE_AI) {
    processVoiceAiScreen();
    if (redraw) drawScreen();
    delay(1);
    return;
  }

  if (screen == Screen::MOUSE) {
    const uint32_t now = millis();
    // No network requests or sleep transitions on the latency-sensitive path.
    lastUserActivityAt = now;
    if (buttonC.wasHeld()) resetMouseCalibration();
    else if (buttonC.wasClicked()) { goBack(); return; }

    static bool previousReady = false;
    const bool ready = isMouseConnected();
    if (ready != previousReady) {
      previousReady = ready;
      resetMouseCalibration();
    }
    // Opening the screen or reconnecting with a button held must not click.
    if (ready && mouseCalibrated && !M5.BtnA.isPressed() && !M5.BtnB.isPressed()) mouseButtonsArmed = true;
    if (mouseButtonsArmed && ready && mouseCalibrated) {
      if (M5.BtnA.wasPressed()) bleMouse.press(MOUSE_BUTTON_LEFT);
      if (M5.BtnA.wasReleased()) bleMouse.release(MOUSE_BUTTON_LEFT);
      if (M5.BtnB.wasPressed()) bleMouse.press(MOUSE_BUTTON_RIGHT);
      if (M5.BtnB.wasReleased()) bleMouse.release(MOUSE_BUTTON_RIGHT);
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnA.wasReleased() || M5.BtnB.wasReleased()) {
        mouseQuietUntil = now + 65;
        mouseSmoothDx = mouseSmoothDy = mouseRemainderX = mouseRemainderY = 0;
      }
    }

    if (now - mouseSampleAt >= 10 && M5.Imu.isEnabled()) {
      const uint32_t elapsed = now - mouseSampleAt;
      mouseSampleAt = now;
      float gx = 0, gy = 0, gz = 0, ax = 0, ay = 0, az = 0;
      // getGyro refreshes cached sensor data when older than 256 microseconds.
      if (M5.Imu.getGyro(&gx, &gy, &gz) && M5.Imu.getAccel(&ax, &ay, &az) &&
          isfinite(gx) && isfinite(gy) && isfinite(gz)) {
        if (!mouseCalibrated) {
          const float gravity = ax*ax + ay*ay + az*az;
          bool stable = gravity > 0.85f && gravity < 1.15f &&
            fabsf(gx) < 5 && fabsf(gy) < 5 && fabsf(gz) < 5;
          if (mouseCalibSamples > 0) stable = stable &&
            fabsf(gx - mouseSumGx/mouseCalibSamples) < 0.8f &&
            fabsf(gy - mouseSumGy/mouseCalibSamples) < 0.8f &&
            fabsf(gz - mouseSumGz/mouseCalibSamples) < 0.8f;
          if (!stable || M5.BtnA.isPressed() || M5.BtnB.isPressed() || buttonC.stablePressed) {
            mouseCalibSamples = 0; mouseSumGx = mouseSumGy = mouseSumGz = 0;
          } else {
            mouseSumGx += gx; mouseSumGy += gy; mouseSumGz += gz;
            if (++mouseCalibSamples >= 100) {
              mouseBiasGx = mouseSumGx / mouseCalibSamples;
              mouseBiasGy = mouseSumGy / mouseCalibSamples;
              mouseBiasGz = mouseSumGz / mouseCalibSamples;
              mouseCalibrated = true;
              redraw = true;
              Serial.println("[AIR-MOUSE] Repouso calibrado; pronto.");
            }
          }
        } else {
          // Fixed axes avoid the old accelerometer-dependent steering changes.
          const float rx = -(gz - mouseBiasGz), ry = -(gx - mouseBiasGx);
          const float dt = min(elapsed, (uint32_t)30) / 1000.0f;
          const float alpha = 1.0f - expf(-dt / 0.018f);
          const float ex = fabsf(rx) <= 0.7f ? 0 : copysignf(fabsf(rx)-0.7f, rx);
          const float ey = fabsf(ry) <= 0.7f ? 0 : copysignf(fabsf(ry)-0.7f, ry);
          mouseSmoothDx += alpha * (ex - mouseSmoothDx);
          mouseSmoothDy += alpha * (ey - mouseSmoothDy);
          if (ex == 0) { mouseSmoothDx = 0; mouseRemainderX = 0; }
          if (ey == 0) { mouseSmoothDy = 0; mouseRemainderY = 0; }
          if (ready && (int32_t)(now - mouseQuietUntil) >= 0 && !buttonC.stablePressed) {
            mouseRemainderX += mouseSmoothDx * 18.0f * dt;
            mouseRemainderY += mouseSmoothDy * 18.0f * dt;
            const int dx = constrain((int)mouseRemainderX, -127, 127);
            const int dy = constrain((int)mouseRemainderY, -127, 127);
            mouseRemainderX -= dx; mouseRemainderY -= dy;
            // Drop saturation overflow; do not replay a large movement later.
            mouseRemainderX = constrain(mouseRemainderX, -0.99f, 0.99f);
            mouseRemainderY = constrain(mouseRemainderY, -0.99f, 0.99f);
            if (dx || dy) bleMouse.move(dx, dy);
          } else mouseRemainderX = mouseRemainderY = 0;
          if (now - mouseUiAt >= 50) {
            mouseUiAt = now;
            updateMouseCrosshair(rx, ry, M5.BtnA.isPressed() || M5.BtnB.isPressed() ? UI_YELLOW : UI_SELECTED);
          }
        }
      }
    }
    if (redraw) drawScreen();
    delay(1);
    return;
  }

  const bool cattleScreen = screen == Screen::TEAM_CATTLE_COUNTER || screen == Screen::TEAM_CATTLE_RESET_CONFIRM;
  const bool trainingActiveScreen = screen == Screen::TEAM_TRAIN_ACTIVE;

  // B longo mantém o padrão global de voltar. Na confirmação, B curto cancela.
  if (M5.BtnB.wasHold()) {
    goBack();
  } else if (M5.BtnB.wasClicked()) {
    if (screen == Screen::TEAM_CATTLE_RESET_CONFIRM || screen == Screen::TEAM_TRAIN_END_CONFIRM) goBack();
    else nextItem();
  }

  if (cattleScreen || trainingActiveScreen) {
    if (M5.BtnA.wasPressed()) { teamAPressedAt = millis(); teamAHoldTriggered = false; }
    if (M5.BtnA.isPressed() && !teamAHoldTriggered && millis() - teamAPressedAt >= TEAM_A_HOLD_MS) {
      teamAHoldTriggered = true;
      if (screen == Screen::TEAM_CATTLE_COUNTER) screen = Screen::TEAM_CATTLE_RESET_CONFIRM;
      else if (screen == Screen::TEAM_TRAIN_ACTIVE) screen = Screen::TEAM_TRAIN_END_CONFIRM;
      redraw = true;
    }
    if (M5.BtnA.wasReleased() && !teamAHoldTriggered) executeSelected();
  } else if (M5.BtnA.wasClicked()) executeSelected();

  if (buttonC.wasHeld() && screen == Screen::TEAM_TRAIN_ACTIVE) {
    removeTrainingPass();
  } else if (buttonC.wasHeld()) {
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

  // Animação temporizada a cada 250ms (elimina o flicker e as listras do LCD)
  static uint32_t lastAnimTick = 0;
  if (screen == Screen::WIFI_SCANNING || screen == Screen::WIFI_CONNECTING) {
    if (millis() - lastAnimTick >= 250) {
      lastAnimTick = millis();
      redraw = true;
    }
  }

  // Atualização suave do relógio no cabeçalho dos menus e na tela de configuração
  static int lastClockMin = -1;
  if (clockIsValid()) {
    time_t tnow = time(nullptr);
    tm tval;
    localtime_r(&tnow, &tval);
    if (tval.tm_min != lastClockMin) {
      lastClockMin = tval.tm_min;
      if (isMenuScreen(screen)) {
        updateStatusBarClock(); // Atualiza apenas o cantinho do relógio sem piscar a tela
      } else if (screen == Screen::SETTINGS_CLOCK) {
        redraw = true; // Na tela de ajuste do relógio, atualiza quando o minuto mudar
      }
    }
  }

  if (redraw) {
    drawScreen();
  }

  delay(10);
}


bool requestWifiScan(bool forMenu, bool forAuto) {
  if (wifiConnecting || wifiConnectPending || wifiScanRunning || wifiKeyboardActive) return false;
  WiFi.mode(webUiMode == WebUiMode::SETUP_AP ? WIFI_AP_STA : WIFI_STA);
  WiFi.scanDelete();
  const int result = WiFi.scanNetworks(true, true);
  if (result == WIFI_SCAN_FAILED) return false;
  wifiScanRunning = true;
  wifiScanForMenu = forMenu;
  wifiScanForAuto = forAuto;
  wifiScanStartedAt = millis();
  if (forMenu) { screen = Screen::WIFI_SCANNING; selected = 0; redraw = true; }
  return true;
}

void processWifiScan() {
  if (!wifiScanRunning) return;
  const int found = WiFi.scanComplete();
  if (found == WIFI_SCAN_RUNNING) return;
  wifiScanRunning = false;
  wifiScanHasResults = true;
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

  if (wifiScanForMenu && screen == Screen::WIFI_SCANNING) {
    screen = Screen::WIFI_NETWORKS;
    selected = 0;
  }
  if (screen == Screen::WIFI_NETWORKS && selected >= scannedNetworkCount) selected = 0;
  if (wifiScanForAuto) {
    wifiAutoCandidatesReady = true;
    wifiAutoCandidateCursor = 0;
    tryAutoConnectStrongest();
  }
  wifiScanForMenu = false;
  wifiScanForAuto = false;
  redraw = true;
}

bool parseIndexArg(const char* name, int count, int& result) {
  if (!webServer.hasArg(name)) return false;
  const String raw = webServer.arg(name);
  if (!raw.length() || raw.length() > 3) return false;
  int number = 0;
  for (size_t i = 0; i < raw.length(); ++i) {
    if (raw[i] < '0' || raw[i] > '9') return false;
    number = number * 10 + raw[i] - '0';
  }
  if (number >= count) return false;
  result = number;
  return true;
}

String acStateJson(uint8_t device) {
  const AcDevice& ac = airConditioners[device];
  const AcState& state = ac.state;
  return "\"device\":" + String(device) + ",\"temp\":" + String(state.temp) +
    ",\"mode\":\"" + acModeName(state.mode) + "\",\"fan\":\"" + acFanName(state.fan) +
    "\",\"power\":" + (state.power ? "true" : "false") +
    ",\"swing\":" + (state.swing ? "true" : "false") +
    ",\"turbo\":" + (state.turbo ? "true" : "false") +
    ",\"sleep\":\"" + sleepName(ac) + "\"";
}

void handleWebApiAcState() {
  int device;
  if (!parseIndexArg("device", AC_COUNT, device)) {
    sendJson(false, "Dispositivo inválido.");
    return;
  }
  webServer.sendHeader("Cache-Control", "no-store");
  sendJson(true, "Estado do controle", acStateJson(device));
}
