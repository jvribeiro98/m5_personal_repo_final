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
#include "ui_core.h"
// Voice AI Module enabled

// ============================================================
// M5 PERSONAL - MODULO IR v0.3 CORRIGIDO
// Hardware: M5StickC Plus2
// IR interno: GPIO 19
// Orientacao: rotation 3 (emissor IR fisicamente para cima)
//
// Botoes (regra unica, valida em todas as telas):
//   A curto  -> OK / acao principal
//   B curto  -> proximo / descer / +
//   C curto  -> anterior / subir / -   (em tela sem lista: voltar)
//   B longo  -> voltar
//   C longo  -> voltar  (no menu principal: desligar)
// Excecoes explicitas, sempre indicadas no rodape da propria tela:
//   Teclado   -> B/C longos movem entre linhas
//   Air Mouse -> A/B sao os botoes do mouse, C longo recalibra
//   Voz       -> B curto em resposta = descer, C curto = subir
// A e B usam o mesmo limiar de "segurar" que C (BTN_HOLD_MS).
// ============================================================

constexpr uint8_t IR_PIN = 19;
constexpr uint8_t BTN_C_PIN = 35;
constexpr uint32_t BTN_C_DEBOUNCE_MS = 35;
constexpr uint32_t BTN_HOLD_MS = 1200;          // segurar = 1,2 s em A, B e C
constexpr uint32_t KEYBOARD_HOLD_MS = 450;      // dentro do teclado o "segurar" e mais curto
constexpr uint32_t BTN_C_HOLD_MS = BTN_HOLD_MS; // compatibilidade

// Cores: a paleta inteira vive em ui_core.h (namespace ui).

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
  uint32_t holdMs = BTN_HOLD_MS;

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

inline bool isToastActive() {
  return ui::toastActive();
}
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

// --- Relogio / Watchface ---
uint8_t currentWatchfaceStyle = 0; // 0: AURORA, 1: MINIMAL, 2: PAINEL
bool clockReturnToSettings = false;
String clockDayOfWeekText();

bool isMenuScreen(Screen value);
bool screenUsesPreviousItem();
void updateBatteryState(bool forceNow = false);
int getBatteryLevelCached();
bool isBatteryChargingCached();
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

enum class VoiceInputMode : uint8_t {
  ALEXA = 0, // Modo Alexa: Mãos-livres contínuo com Wake-Word ("Ei M5")
  PTT = 1    // Push-To-Talk: Clique [A] para falar, Clique [A] para enviar
};

VoiceState voiceState = VoiceState::IDLE;
VoiceInputMode voiceInputMode = VoiceInputMode::ALEXA;
String voiceActiveAgent = "AGY";
String voiceTranscription = "";
String voiceResultTitle = "";
String voiceResultBody = "";
int voiceScrollLine = 0;
uint8_t voiceWavePhase = 0;
uint32_t voiceAnimTimer = 0;
bool voiceBridgeConnected = true;

// Variáveis do modo Alexa / VOX (Detecção de Fala e Cooldown de Silêncio Ultrarrápido)
bool voxSpeechDetected = false;
uint32_t voxSilenceStart = 0;
static constexpr uint32_t VOX_SILENCE_COOLDOWN_MS = 600; // 600ms de silêncio contínuo (resposta instantânea)

// Áudio: 16000Hz, 16-bit mono. 30 segundos no PSRAM (960.000 bytes)
static constexpr size_t VOICE_SAMPLE_RATE = 16000;
static constexpr size_t VOICE_MAX_SECS = 30;
static constexpr size_t VOICE_BUFFER_BYTES = VOICE_SAMPLE_RATE * VOICE_MAX_SECS * sizeof(int16_t);
static int16_t* voiceAudioBuffer = nullptr;
static size_t voiceAudioBufferCapacityBytes = 0;
static size_t voiceRecordedSamples = 0;
static String pcBridgeIp = "192.168.0.2";
static int pcBridgePort = 5000;
static bool voiceMicRecordingActive = false;
static uint32_t lastVoiceResultAt = 0;

// Canvas de Double-Buffering (Padrão Bruce / CatHack para 60 FPS sem flicker)
static M5Canvas uiCanvas(&M5.Display);
static bool uiCanvasReady = false;

void ensureUiCanvas();

inline M5Canvas& getGfx() {
  ensureUiCanvas();
  return uiCanvas;
}
#include "VoiceHttpTransport.h"
VoiceHttpTransport voiceTransport;
uint32_t voiceGeneration = 0;
void processVoiceTransport();

void drawVoiceAiScreen();
void processVoiceAiScreen();
void initVoiceAiScreen();
void startVoiceRecording();
void stopVoiceRecordingAndSend();
void stopVoiceMic();
void cancelVoiceRecording();
void exitVoiceScreen();
void parseVoiceAiResponse(const String& line);
void executeLocalIrCommand(const String& cmd);
int countWrappedTextLines(int maxW, const String& text);
void drawWrappedTextCanvas(M5Canvas& c, int x, int y, int maxW, int maxLines, int startLine, const String& text, uint16_t color);
void playWandChime();
void playFailSound();
void drawWrappedText(int x, int y, int maxW, int maxLines, int startLine, const String& text, uint16_t color);

M5StickBleMouse bleMouse;
bool bleMouseStarted = false;

#include "MouseCalibration.h"
MouseCalibration mouseCalibration;

bool mouseCalibrated = false;
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
  mouseCalibration.reset();
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
void showBootIntro();
void drawScreen();
uint8_t itemCount();
void nextItem();
void previousItem();
void goBack();
void executeSelected();
bool requestWifiScan(bool forMenu, bool forAuto);
void processWifiScan();
bool parseIndexArg(const char* name, int count, int& result);
String acStateJson(uint8_t device);
void handleWebApiAcState();

#include "ui_screens.h"

// ============================================================
// BOTAO C
// ============================================================

void ButtonCState::begin() {
  pinMode(BTN_C_PIN, INPUT);
  rawPressed = digitalRead(BTN_C_PIN) == LOW;
  stablePressed = rawPressed;
  changedAt = millis();
  pressedAt = millis();
  holdSent = stablePressed;
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

  if (stablePressed && !holdSent && now - pressedAt >= holdMs) {
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

  if (trainingSession.horseCount > MAX_TRAIN_HORSES || (trainingSession.active && trainingSession.horseCount == 0)) {
    trainingSession = TrainingSession();
  }
  if (trainingSession.horseCount > 0 && trainingSession.currentHorse >= trainingSession.horseCount) {
    trainingSession.currentHorse = 0;
  }
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

bool normalizeTrainingCandidate(int direction) {
  for (uint8_t attempt = 0; attempt < MAX_TRAIN_HORSES; attempt++) {
    trainingSetupCandidate = (trainingSetupCandidate + MAX_TRAIN_HORSES + direction) % MAX_TRAIN_HORSES;
    if (!trainingHorseAlreadyChosen(trainingSetupCandidate, trainingSetupSlot)) return true;
  }
  return false;
}

// ============================================================
// TV
// ============================================================

void showToast(const String& message, uint16_t duration) {
  ui::showToast(message, duration, ui::GREEN);
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

  if (!prefs.begin("m5-ir", false)) return;
  const AcState& state = airConditioners[index].state;
  prefs.putBool(prefKey(index, "p").c_str(), state.power);
  prefs.putUChar(prefKey(index, "t").c_str(), state.temp);
  prefs.putUChar(prefKey(index, "m").c_str(), static_cast<uint8_t>(state.mode));
  prefs.putUChar(prefKey(index, "f").c_str(), static_cast<uint8_t>(state.fan));
  prefs.putBool(prefKey(index, "s").c_str(), state.swing);
  prefs.putBool(prefKey(index, "u").c_str(), state.turbo);
  prefs.putUShort(prefKey(index, "z").c_str(), state.sleepMinutes);
  prefs.end();
}

void loadAcStates() {
  if (prefs.begin("m5-ir", true)) {
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
    prefs.end();
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

  // Assegura que a rede padrao esteja cadastrada se a lista estiver vazia (Bug 14)
  if (savedNetworkCount == 0 && findSavedNetwork("Ribeiro") < 0) {
    savedNetworks[0].ssid = "Ribeiro";
    savedNetworks[0].password = "Jv22019198@";
    savedNetworks[0].health = SavedNetworkHealth::UNTESTED;
    savedNetworks[0].failure = SavedNetworkFailure::NONE;
    savedNetworks[0].lastRssi = -50;
    savedNetworkCount = 1;
    saveSavedNetworks();
  }

  prefs.begin("wifi-cfg", true);
  lanWebUiDesired = prefs.getBool("webui", false);
  prefs.end();
}

void saveSavedNetworks() {
  if (!prefs.begin("wifi-nets", false)) return;
  prefs.putUChar("count", savedNetworkCount);

  for (uint8_t i = 0; i < savedNetworkCount; i++) {
    prefs.putString(wifiPrefKey(i, "s").c_str(), savedNetworks[i].ssid);
    prefs.putString(wifiPrefKey(i, "p").c_str(), savedNetworks[i].password);
    prefs.putUChar(wifiPrefKey(i, "h").c_str(), static_cast<uint8_t>(savedNetworks[i].health));
    prefs.putUChar(wifiPrefKey(i, "f").c_str(), static_cast<uint8_t>(savedNetworks[i].failure));
    prefs.putInt(wifiPrefKey(i, "r").c_str(), savedNetworks[i].lastRssi);
  }
  for (uint8_t i = savedNetworkCount; i < MAX_SAVED_NETWORKS; i++) {
    prefs.remove(wifiPrefKey(i, "s").c_str());
    prefs.remove(wifiPrefKey(i, "p").c_str());
    prefs.remove(wifiPrefKey(i, "h").c_str());
    prefs.remove(wifiPrefKey(i, "f").c_str());
    prefs.remove(wifiPrefKey(i, "r").c_str());
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

  if (selected >= savedNetworkCount && savedNetworkCount > 0) {
    selected = savedNetworkCount - 1;
  } else if (savedNetworkCount == 0) {
    selected = 0;
  }

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
  value.replace("'", "&#39;");
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
  int device;
  if (!parseIndexArg("device", AC_COUNT, device)) {
    sendJson(false, "Dispositivo invalido");
    return;
  }

  const bool hasAction = webServer.hasArg("action");
  const bool hasPower = webServer.hasArg("power");
  const bool hasTemp = webServer.hasArg("temp");

  if (!hasAction && !hasPower && !hasTemp) {
    sendJson(false, "Acao obrigatoria");
    return;
  }

  int action = -1;
  if (hasAction) {
    if (!parseIndexArg("action", AC_MENU_COUNT, action)) {
      sendJson(false, "Acao invalida");
      return;
    }
  }

  if (hasPower && action == 7) {
    sendJson(false, "Nao combine power com toggle");
    return;
  }

  if (hasTemp && (action == 0 || action == 1)) {
    sendJson(false, "Nao combine temp com alteracao relativa");
    return;
  }

  bool powerOn = false;
  if (hasPower) {
    String p = webServer.arg("power");
    if (p == "on") {
      powerOn = true;
    } else if (p == "off") {
      powerOn = false;
    } else {
      sendJson(false, "Power deve ser 'on' ou 'off'");
      return;
    }
  }

  int targetTemp = -1;
  if (hasTemp) {
    String t = webServer.arg("temp");
    t.trim();
    if (t.length() == 0) {
      sendJson(false, "Temperatura invalida");
      return;
    }
    for (size_t i = 0; i < t.length(); i++) {
      if (!isdigit(t[i])) {
        sendJson(false, "Temperatura deve ser inteiro");
        return;
      }
    }
    long val = t.toInt();
    if (val < 16 || val > 30) {
      sendJson(false, "Temperatura fora da faixa 16-30");
      return;
    }
    targetTemp = (int)val;
  }

  const uint8_t physicalAc = activeAc;
  activeAc = device;
  AcState& state = airConditioners[device].state;

  if (hasPower) {
    if (state.power != powerOn) {
      toggleAcPower();
    }
  }

  if (targetTemp >= 16 && targetTemp <= 30) {
    if (state.temp != (uint8_t)targetTemp) {
      if (hasPower && state.power != powerOn) delay(120);
      state.temp = (uint8_t)targetTemp;
      state.power = true;
      sendAcState("TEMP");
    }
  }

  if (hasAction && action != -1) {
    delay(120);
    executeAcAction(action);
  }

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
  if (cattleRemainingCount() > 0) {
    for (uint8_t step = 1; step <= cattleMaxNumber + 1; step++) {
      uint8_t candidate = (dir < 0)
        ? (cattleSelectedNumber + cattleMaxNumber + 1 - (step % (cattleMaxNumber + 1))) % (cattleMaxNumber + 1)
        : (cattleSelectedNumber + step) % (cattleMaxNumber + 1);
      if (!isCattleDrawn(candidate)) {
        cattleSelectedNumber = candidate;
        break;
      }
    }
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
void handleWebApiTrainingNext() { if (!trainingSession.active || !trainingSession.horseCount) { sendJson(false,"Nenhum treino ativo"); return; } trainingSession.currentHorse=(trainingSession.currentHorse+1)%trainingSession.horseCount; saveTrainingSession(); redraw=true; sendJson(true,"Próximo cavalo"); }
void handleWebApiTrainingPrevious() { if (!trainingSession.active || !trainingSession.horseCount) { sendJson(false,"Nenhum treino ativo"); return; } trainingSession.currentHorse=(trainingSession.currentHorse+trainingSession.horseCount-1)%trainingSession.horseCount; saveTrainingSession(); redraw=true; sendJson(true,"Cavalo anterior"); }
void handleWebApiTrainingEnd() { if (!trainingSession.active) { sendJson(false,"Nenhum treino ativo"); return; } finishTraining(); screen=Screen::TEAM_TRAIN_SUMMARY; sendJson(true,"Treino salvo"); }


void handleWebSystemPage() {
  String body = F(R"HTML(
<section class="card"><h2>Sistema</h2>
<div class="status"><span>Wi-Fi</span><strong>)HTML");
  body += WiFi.status() == WL_CONNECTED ? htmlEscape(WiFi.SSID()) : "Desconectado";
  body += F(R"HTML(</strong></div><div class="status"><span>Web UI</span><strong>)HTML");
  body += webUiMode == WebUiMode::LAN ? "Rede" : (webUiMode == WebUiMode::SETUP_AP ? "AP" : "Desativada");
  body += F(R"HTML(</strong></div><div class="status"><span>Bateria</span><strong>)HTML");
  body += String(getBatteryLevelCached()) + "%";
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
                 ",\"battery\":" + String(getBatteryLevelCached()) +
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
  triggerWeatherUpdateAsync();
  sendJson(true,"Atualização de horário e clima iniciada");
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
// B longo       = mover para baixo   (limiar KEYBOARD_HOLD_MS, igual para B e C)
// C longo       = mover para cima
//
// Sair do teclado e sempre pela tecla "EX"; B/C longos nao voltam aqui.
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
  // No teclado, "segurar" e mais curto e simetrico entre B (baixo) e C (cima).
  M5.BtnB.setHoldThresh(KEYBOARD_HOLD_MS);
  buttonC.holdMs = KEYBOARD_HOLD_MS;
  auto finish = [&](const String& value) {
    M5.BtnB.setHoldThresh(BTN_HOLD_MS);
    buttonC.holdMs = BTN_HOLD_MS;
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
  updateBatteryState(false);

    static uint32_t caretBlinkAt = 0;
    if (millis() - caretBlinkAt >= 500) { caretBlinkAt = millis(); redrawKeyboard = true; }
    if (redrawKeyboard) {
      redrawKeyboard = false;
      drawKeyboardFrame(title, text, masked, caps, x, y, BRUCE_KEYS, ui::BLUE);
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

// Telas em que C curto faz "anterior/subir" (inverso de B curto).
// Nas demais, C curto volta.
bool screenUsesPreviousItem() {
  switch (screen) {
    case Screen::SETTINGS_CLOCK:   // B troca o estilo; C volta (rodape "[C] Voltar").
    case Screen::WIFI_RESULT:
    case Screen::WIFI_AP_INFO:
    case Screen::WIFI_WEBUI_NETWORK:
    case Screen::WIFI_SCANNING:
    case Screen::WIFI_CONNECTING:
    case Screen::WIFI_KEYBOARD:
    case Screen::SETTINGS_SLEEP:
    case Screen::TEAM_CATTLE_RESET_CONFIRM:
    case Screen::TEAM_TRAIN_END_CONFIRM:
    case Screen::MOUSE:
    case Screen::VOICE_AI:
      return false;
    default:
      return itemCount() > 1;
  }
}

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
  currentWatchfaceStyle = prefs.getUChar("wf_style", 0) % 3;
  uiState.soundEnabled = prefs.getBool("snd", true);
  prefs.end();
  M5.Display.setBrightness(BRIGHTNESS_LEVELS[brightnessIndex]);
}

void saveSystemSettings() {
  prefs.begin("system", false);
  prefs.putUChar("bright", brightnessIndex);
  prefs.putBool("snd", uiState.soundEnabled);
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

static TaskHandle_t weatherTaskHandle = nullptr;
static volatile bool weatherTaskRunning = false;

static void weatherTaskEntry(void* param) {
  weatherTaskRunning = true;
  updateLocationAndWeather();
  weatherTaskRunning = false;
  weatherTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

void triggerWeatherUpdateAsync() {
  if (weatherTaskRunning || WiFi.status() != WL_CONNECTED) return;
  if (xTaskCreatePinnedToCore(weatherTaskEntry, "weather-task", 4096, nullptr, 1, &weatherTaskHandle, 0) != pdPASS) {
    weatherTaskRunning = false;
    weatherTaskHandle = nullptr;
  }
}

void processWeatherAndClock() {
  if (WiFi.status() != WL_CONNECTED) return;

  // Rate-limit NTP check to once every 3 seconds instead of every 10ms frame (Bug 04)
  static uint32_t lastNtpCheckAt = 0;
  const uint32_t now = millis();
  if (!clockIsValid() && (now - lastNtpCheckAt >= 3000UL)) {
    lastNtpCheckAt = now;
    tm value;
    if (getLocalTime(&value, 20) && value.tm_year + 1900 >= 2024) {
      M5.Rtc.setDateTime(m5::rtc_datetime_t(value));
      redraw = true;
    }
  }

  const bool newNetwork = (lastWeatherSsid != WiFi.SSID());
  if (newNetwork || now - lastWeatherAttemptAt >= WEATHER_REFRESH_MS) {
    lastWeatherAttemptAt = now;
    lastWeatherSsid = WiFi.SSID();
    triggerWeatherUpdateAsync();
  }
}


// ============================================================
// GERENCIADOR DE BATERIA ESTAVEL (FILTRO IIR, OVERSAMPLING E HISTERESE)
// Elimina 100% da oscilacao do ADC durante navegacao e scroll no M5StickC Plus 2
// ============================================================
static int s_cachedBatteryLevel = -1;
static bool s_cachedBatteryCharging = false;
static uint32_t s_lastBatterySampleTime = 0;
static float s_filteredBatteryLevel = -1.0f;
static constexpr uint32_t BATTERY_SAMPLE_INTERVAL_MS = 3000;

void updateBatteryState(bool forceNow) {
  uint32_t now = millis();
  if (!forceNow && s_cachedBatteryLevel >= 0 && (now - s_lastBatterySampleTime < BATTERY_SAMPLE_INTERVAL_MS)) {
    return;
  }
  s_lastBatterySampleTime = now;

  bool charging = M5.Power.isCharging();

  // Media de 4 leituras rapidas para cancelar ripple e ruido de chaveamento do ADC
  int32_t sum = 0;
  for (int i = 0; i < 4; ++i) {
    sum += M5.Power.getBatteryLevel();
    delayMicroseconds(400);
  }
  float rawSample = constrain((float)sum / 4.0f, 0.0f, 100.0f);

  if (s_filteredBatteryLevel < 0.0f || charging != s_cachedBatteryCharging) {
    // Inicializacao no boot ou transicao de plug/unplug do cabo USB
    s_cachedBatteryCharging = charging;
    s_filteredBatteryLevel = rawSample;
    s_cachedBatteryLevel = constrain((int)roundf(rawSample), 0, 100);
    return;
  }

  // Filtro passa-baixa IIR: absorve quedas de tensao transitorias de carga da CPU/display
  s_filteredBatteryLevel = (s_filteredBatteryLevel * 0.85f) + (rawSample * 0.15f);
  int candidate = constrain((int)roundf(s_filteredBatteryLevel), 0, 100);

  if (charging) {
    // Carregando: nivel sobe progressivamente
    if (candidate > s_cachedBatteryLevel) {
      s_cachedBatteryLevel = candidate;
    }
  } else {
    // Descarregando: NUNCA salta para cima por ruido de tecla ou scroll!
    if (candidate < s_cachedBatteryLevel) {
      s_cachedBatteryLevel = candidate;
    } else if (candidate > s_cachedBatteryLevel + 6) {
      // Reajuste caso a tensao estabilize bem acima (ex: desconexao de carga)
      s_cachedBatteryLevel = candidate;
    }
  }
}

int getBatteryLevelCached() {
  updateBatteryState(false);
  return (s_cachedBatteryLevel >= 0) ? s_cachedBatteryLevel : 100;
}

bool isBatteryChargingCached() {
  updateBatteryState(false);
  return s_cachedBatteryCharging;
}



void updateStatusBarClock() {
  if (!isMenuScreen(screen)) return;
  redraw = true;
}


String clockDayOfWeekText() {
  if (!clockIsValid()) return "---";
  time_t now = time(nullptr);
  tm value;
  localtime_r(&now, &value);
  const char* days[] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"};
  if (value.tm_wday >= 0 && value.tm_wday <= 6) return days[value.tm_wday];
  return "---";
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
    static int lastClockIdleSec = -1;
    if (tval.tm_sec != lastClockIdleSec) {
      lastClockIdleSec = tval.tm_sec;
      drawClockScreen(false); // Atualizacao a cada segundo no screensaver!
    }
    if (now - clockScreenStartedAt >= SCREEN_OFF_AFTER_CLOCK_MS) {
      displayIdleState = DisplayIdleState::OFF;
      M5.Display.setBrightness(0);
    }
  }
}





// ============================================================
// INTERFACE
// ============================================================





































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

void ensureUiCanvas() {
  if (!uiCanvasReady) {
    uiCanvas.setColorDepth(16);
    if (psramFound()) {
      uiCanvas.setPsram(true);
    }
    uiCanvas.createSprite(240, 135);
    uiCanvas.setRotation(0);
    uiCanvasReady = true;
  }
}

void executeLocalIrCommand(const String& cmd) {
  Serial.printf("[VOICE IR] Disparando comando IR fisico no GPIO 19: %s\n", cmd.c_str());
  if (cmd == "AC_POWER") {
    activeAc = 0; // Ar Samsung
    executeAcAction(7); // Toggle Power
  } else if (cmd == "AC_TEMP_UP") {
    activeAc = 0;
    executeAcAction(1); // Temp +
  } else if (cmd == "AC_TEMP_DOWN") {
    activeAc = 0;
    executeAcAction(0); // Temp -
  } else if (cmd == "TV_POWER") {
    activeTv = 0; // TV Samsung
    sendTvCommand(TV_POWER);
  } else if (cmd == "TV_VOL_UP") {
    activeTv = 0;
    sendTvCommand(TV_VOL_UP);
  } else if (cmd == "TV_VOL_DOWN") {
    activeTv = 0;
    sendTvCommand(TV_VOL_DOWN);
  } else if (cmd == "TV_MUTE") {
    activeTv = 0;
    sendTvCommand(TV_MUTE);
  }
}

void initVoiceAiScreen() {
  ensureUiCanvas();
  voiceState = VoiceState::IDLE;
  voiceScrollLine = 0;
  voiceTranscription = "";
  voiceResultTitle = "";
  voiceResultBody = "";
  voxSpeechDetected = false;
  voxSilenceStart = 0;

  Preferences prefs;
  if (prefs.begin("m5p_voice", true)) {
    voiceInputMode = (VoiceInputMode)prefs.getUChar("v_mode", (uint8_t)VoiceInputMode::ALEXA);
    prefs.end();
  }

  if (!voiceAudioBuffer) {
    if (psramFound()) {
      voiceAudioBuffer = (int16_t*)ps_malloc(VOICE_BUFFER_BYTES);
      if (voiceAudioBuffer) voiceAudioBufferCapacityBytes = VOICE_BUFFER_BYTES;
    }
    if (!voiceAudioBuffer) {
      size_t fb = VOICE_SAMPLE_RATE * 8 * sizeof(int16_t);
      voiceAudioBuffer = (int16_t*)heap_caps_malloc(fb, MALLOC_CAP_8BIT);
      if (voiceAudioBuffer) voiceAudioBufferCapacityBytes = fb;
    }
    if (!voiceAudioBuffer) {
      size_t fb = VOICE_SAMPLE_RATE * 4 * sizeof(int16_t);
      voiceAudioBuffer = (int16_t*)malloc(fb);
      if (voiceAudioBuffer) voiceAudioBufferCapacityBytes = fb;
    }
  }
  voiceRecordedSamples = 0;
  voiceMicRecordingActive = false;
  auto mic_cfg = M5.Mic.config();
  mic_cfg.magnification = 48;
  M5.Mic.config(mic_cfg);

  Serial.printf("[VOICE] Tela IA iniciada. Modo: %s | Buffer: %p (PSRAM=%d)\n",
                voiceInputMode == VoiceInputMode::ALEXA ? "ALEXA (MAOS-LIVRES)" : "PTT",
                voiceAudioBuffer, psramFound());
  Serial.println("VOICE_READY");
  redraw = true;

  if (voiceInputMode == VoiceInputMode::ALEXA) {
    startVoiceRecording();
  }
}

String extractJsonField(const String& json, const String& key) {
  String searchKey = "\"" + key + "\"";
  int kIdx = json.indexOf(searchKey);
  if (kIdx == -1) return "";
  int colonIdx = json.indexOf(":", kIdx + searchKey.length());
  if (colonIdx == -1) return "";

  int valStart = colonIdx + 1;
  const int len = json.length();
  while (valStart < len && (json[valStart] == ' ' || json[valStart] == '\t' || json[valStart] == '\r' || json[valStart] == '\n')) {
    valStart++;
  }
  if (valStart >= len) return "";

  if (json[valStart] == '\"') {
    valStart++;
    String out;
    out.reserve(64);
    for (int i = valStart; i < len; ++i) {
      char c = json[i];
      if (c == '\\' && i + 1 < len) {
        char next = json[++i];
        if (next == '\"') out += '\"';
        else if (next == '\\') out += '\\';
        else if (next == 'n') out += '\n';
        else if (next == 'r') out += '\r';
        else if (next == 't') out += '\t';
        else { out += '\\'; out += next; }
      } else if (c == '\"') {
        return out;
      } else {
        out += c;
      }
    }
    return out;
  } else {
    int valEnd = valStart;
    while (valEnd < len && json[valEnd] != ',' && json[valEnd] != '}' && json[valEnd] != '\r' && json[valEnd] != '\n') {
      valEnd++;
    }
    String out = json.substring(valStart, valEnd);
    out.trim();
    return out;
  }
}

String extractJsonBody(const String& json) {
  return extractJsonField(json, "body");
}

void parseVoiceAiResponse(const String& rawLine) {
  String line = rawLine;
  line.trim();
  if (line.startsWith("{") && line.endsWith("}")) {
    if (line.indexOf("\"type\":\"IGNORE\"") != -1) {
      Serial.println("[VOICE] Audio descartado pelo PC (sem 'Ei M5'). Continuando escuta...");
      voxSpeechDetected = false;
      voxSilenceStart = 0;
      voiceRecordedSamples = 0;
      redraw = true;
      return;
    }

    // Se houver comando infravermelho de hardware, executa direto no GPIO 19!
    String irCmd = extractJsonField(line, "ir");
    if (irCmd.length() > 0) {
      executeLocalIrCommand(irCmd);
    }

    String t = extractJsonField(line, "title");
    if (t.length() > 0) voiceResultTitle = t;

    String a = extractJsonField(line, "agent");
    if (a.length() > 0) voiceActiveAgent = a;

    String tx = extractJsonField(line, "text");
    if (tx.length() > 0) voiceTranscription = tx;

    String b = extractJsonBody(line);
    if (b.length() > 0) voiceResultBody = b;

    voiceState = VoiceState::RESULT;
    voiceScrollLine = 0;
    voiceBridgeConnected = true;
    lastVoiceResultAt = millis();
    voxSpeechDetected = false;
    voxSilenceStart = 0;
    voiceRecordedSamples = 0;
    playWandChime();
    redraw = true;
    Serial.printf("[VOICE PARSED] Titulo: %s | Agente: %s | Texto: %s | IR: %s\n",
                  voiceResultTitle.c_str(), voiceActiveAgent.c_str(), voiceTranscription.c_str(), irCmd.c_str());
  }
}

static uint32_t voiceRecStartTime = 0;

void startVoiceRecording() {
  if (voiceTransport.busy()) {
    Serial.println("[VOICE] Bloqueado: transporte HTTP anterior ainda ocupado.");
    return;
  }
  voiceState = VoiceState::LISTENING;
  voiceRecordedSamples = 0;
  voiceScrollLine = 0;
  voiceWavePhase = 0;
  voxSpeechDetected = false;
  voxSilenceStart = 0;
  voiceRecStartTime = millis();

  if (!voiceMicRecordingActive) {
    if (M5.Speaker.isEnabled()) M5.Speaker.tone(1200, 30);
    delay(40);
    M5.Speaker.end();
    M5.Mic.begin();
    voiceMicRecordingActive = true;
  }

  Serial.printf("[VOICE] Escuta ativa (Modo: %s)...\n",
                voiceInputMode == VoiceInputMode::ALEXA ? "ALEXA (MAOS-LIVRES)" : "PTT");
  redraw = true;
}

// Desliga o microfone e devolve o alto-falante (I2S compartilhado).
void stopVoiceMic() {
  if (voiceMicRecordingActive) {
    voiceMicRecordingActive = false;
    M5.Mic.end();
    M5.Speaker.begin();
  }
}

// Descarta o trecho gravado sem enviar e volta para IDLE.
void cancelVoiceRecording() {
  stopVoiceMic();
  voxSpeechDetected = false;
  voxSilenceStart = 0;
  voiceRecordedSamples = 0;
  voiceState = VoiceState::IDLE;
  if (M5.Speaker.isEnabled()) M5.Speaker.tone(600, 60);
  Serial.println("[VOICE] Gravacao cancelada pelo usuario.");
  redraw = true;
}

// Sai da tela do agente: mic desligado, estado limpo, volta ao menu.
void exitVoiceScreen() {
  stopVoiceMic();
  voxSpeechDetected = false;
  voiceScrollLine = 0;
  voiceState = VoiceState::IDLE;
  goBack();
}

void stopVoiceRecordingAndSend() {
  voiceMicRecordingActive = false;
  M5.Mic.end();
  M5.Speaker.begin();
  if (M5.Speaker.isEnabled()) M5.Speaker.tone(1600, 40);

  // Se não estiver em RESULT, mostra tela de THINKING.
  // Se já estiver em RESULT, apenas atualiza o indicador no topo para que a resposta anterior não suma!
  if (voiceState != VoiceState::RESULT) {
    voiceState = VoiceState::THINKING;
  }
  redraw = true;
  drawScreen();

  uint32_t recDurationMs = millis() - voiceRecStartTime;
  Serial.printf("[VOICE] Trecho captado: %u amostras (%ums)\n", (unsigned int)voiceRecordedSamples, recDurationMs);

  if (voiceRecordedSamples < 1200 && recDurationMs < 350) {
    voxSpeechDetected = false;
    voxSilenceStart = 0;
    voiceRecordedSamples = 0;
    redraw = true;
    return;
  }

  Serial.println("VOICE_STOP");

  bool sentOk = false;
  if (WiFi.status() == WL_CONNECTED && voiceAudioBuffer && voiceRecordedSamples > 0 && pcBridgeIp.length() > 0) {
    String url = "http://" + pcBridgeIp + ":" + String(pcBridgePort) + "/audio";
    String mode = (voiceInputMode == VoiceInputMode::ALEXA ? "ALEXA" : "PTT");
    voiceGeneration++;
    if (voiceTransport.start(url, mode, (const uint8_t*)voiceAudioBuffer, voiceRecordedSamples * sizeof(int16_t), voiceGeneration)) {
      Serial.println("[VOICE] Enviando audio em background (FreeRTOS)...");
      sentOk = true;
    }
  }

  if (!sentOk && voiceAudioBuffer && voiceRecordedSamples > 0) {
    Serial.printf("VOICE_AUDIO %u %s\n",
                  (unsigned int)(voiceRecordedSamples * sizeof(int16_t)),
                  voiceInputMode == VoiceInputMode::ALEXA ? "ALEXA" : "PTT");
    Serial.write((const uint8_t*)voiceAudioBuffer, voiceRecordedSamples * sizeof(int16_t));
    Serial.println();
  }

  M5.update();
  lastVoiceResultAt = millis();
  voxSpeechDetected = false;
  voxSilenceStart = 0;
  voiceRecordedSamples = 0;
  redraw = true;
}

void processVoiceTransport() {
  VoiceHttpTransport::Reply reply;
  if (voiceTransport.poll(reply)) {
    if (reply.status == 200) {
      Serial.printf("[VOICE] Resposta HTTP recebida: %s\n", reply.body.c_str());
      parseVoiceAiResponse(reply.body);
    } else {
      Serial.printf("[VOICE] Falha HTTP background: %d\n", reply.status);
      voiceBridgeConnected = false;
      if (voiceState == VoiceState::THINKING) {
        voiceState = VoiceState::IDLE;
        redraw = true;
      }
    }
  }
}




void processVoiceAiScreen() {
  lastUserActivityAt = millis();
  processVoiceTransport();

  // A resposta permanece na tela ate o usuario agir (sem auto-dismiss).

  // ============================================================
  // MAPA DE BOTOES DO AGENTE IA (igual nos modos ALEXA e PTT)
  //   A curto          : falar  <->  enviar (toggle; um clique = um evento)
  //   B curto          : RESULT -> descer | LISTENING -> cancelar | IDLE -> alternar modo
  //   C curto          : RESULT com rolagem -> subir | senao -> voltar
  //   B longo / C longo: voltar ao menu
  // ============================================================

  if (buttonC.wasHeld() || M5.BtnB.wasHold()) {
    exitVoiceScreen();
    return;
  }

  if (buttonC.wasClicked()) {
    if (voiceState == VoiceState::RESULT && voiceScrollLine > 0) {
      voiceScrollLine = max(0, voiceScrollLine - 2);
      redraw = true;
    } else {
      exitVoiceScreen();
    }
    return;
  }

  if (M5.BtnB.wasClicked()) {
    if (voiceState == VoiceState::RESULT) {
      int totalL = countWrappedTextLines(224, voiceResultBody);
      if (voiceScrollLine + 3 < totalL) voiceScrollLine += 2;
      redraw = true;
    } else if (voiceState == VoiceState::LISTENING) {
      cancelVoiceRecording();
    } else if (voiceState == VoiceState::IDLE) {
      voiceInputMode = (voiceInputMode == VoiceInputMode::ALEXA) ? VoiceInputMode::PTT : VoiceInputMode::ALEXA;
      Preferences prefs;
      if (prefs.begin("m5p_voice", false)) {
        prefs.putUChar("v_mode", (uint8_t)voiceInputMode);
        prefs.end();
      }
      playWandChime();
      if (voiceInputMode == VoiceInputMode::ALEXA) {
        startVoiceRecording();
      } else {
        stopVoiceMic();
        redraw = true;
      }
    }
    // THINKING: aguardando o PC; B nao faz nada.
  }

  // A: um clique inicia, o proximo clique envia. Vale para PTT e ALEXA
  // (no ALEXA o clique forca o envio sem esperar o silencio).
  if (M5.BtnA.wasClicked()) {
    if (voiceState == VoiceState::IDLE || voiceState == VoiceState::RESULT) {
      startVoiceRecording();
    } else if (voiceState == VoiceState::LISTENING && millis() - voiceRecStartTime >= 400) {
      stopVoiceRecordingAndSend();
    }
  }

  // ============================================================
  // ESCUTA EM SEGUNDO PLANO (ATIVO EM LISTENING OU EM RESULT NO MODO ALEXA)
  // ============================================================
  const bool shouldListen = !voiceTransport.busy() &&
                            ((voiceState == VoiceState::LISTENING) ||
                            (voiceState == VoiceState::RESULT && voiceInputMode == VoiceInputMode::ALEXA));

  if (shouldListen) {
    if (!voiceMicRecordingActive) {
      M5.Mic.begin();
      voiceMicRecordingActive = true;
    }

    constexpr size_t CHUNK = 512;
    const size_t maxSamples = voiceAudioBufferCapacityBytes > 0
        ? (voiceAudioBufferCapacityBytes / sizeof(int16_t))
        : (VOICE_BUFFER_BYTES / sizeof(int16_t));
    if (voiceAudioBuffer && (voiceRecordedSamples + CHUNK <= maxSamples)) {
      if (M5.Mic.record(&voiceAudioBuffer[voiceRecordedSamples], CHUNK, VOICE_SAMPLE_RATE)) {
        while (M5.Mic.isRecording()) delay(1);

        int32_t chunkMax = 0;
        int64_t sumSq = 0;
        for (size_t i = 0; i < CHUNK; ++i) {
          int16_t sample = voiceAudioBuffer[voiceRecordedSamples + i];
          int32_t absS = abs(sample);
          if (absS > chunkMax) chunkMax = absS;
          sumSq += (int64_t)sample * sample;
        }
        int32_t rms = (int32_t)sqrt(sumSq / CHUNK);
        uiState.voiceLevel = uiState.voiceLevel * 0.55f + 0.45f * ui::clamp01(rms / 2600.0f);

        voiceRecordedSamples += CHUNK;

        if (voiceInputMode == VoiceInputMode::ALEXA) {
          if (!voxSpeechDetected) {
            // Pré-buffer circular de 1024 amostras (~64ms) para nunca cortar o "Ei"
            if (voiceRecordedSamples >= 1024) {
              memmove(&voiceAudioBuffer[0], &voiceAudioBuffer[CHUNK], CHUNK * sizeof(int16_t));
              voiceRecordedSamples = CHUNK;
            }
            if (rms > 650 || chunkMax > 1600) {
              voxSpeechDetected = true;
              voxSilenceStart = 0;
              voiceRecStartTime = millis();
              redraw = true;
            }
          } else {
            // Fala detectada
            if (rms > 500 || chunkMax > 1300) {
              voxSilenceStart = 0;
            } else {
              if (voxSilenceStart == 0) {
                voxSilenceStart = millis();
              } else if (millis() - voxSilenceStart >= VOX_SILENCE_COOLDOWN_MS) {
                Serial.println("[VOX ALEXA] Silencio de 2.0s atingido. Enviando audio!");
                stopVoiceRecordingAndSend();
                return;
              }
            }
          }
        }
      }
    }

    voiceWavePhase = (voiceWavePhase + 1) % 360;
    if (millis() - voiceAnimTimer > 40) {
      voiceAnimTimer = millis();
      redraw = true;
    }

    if (voxSpeechDetected && (millis() - voiceRecStartTime >= (VOICE_MAX_SECS * 1000))) {
      stopVoiceRecordingAndSend();
    }
  }

  // Leitura de mensagens seriais do PC
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    if (line.startsWith("{") && line.endsWith("}")) {
      if (line.indexOf("\"type\":\"CONFIG\"") != -1) {
        int ipIdx = line.indexOf("\"ip\":\"");
        if (ipIdx != -1) {
          int ipEnd = line.indexOf("\"", ipIdx + 6);
          pcBridgeIp = line.substring(ipIdx + 6, ipEnd);
        }
        int pIdx = line.indexOf("\"port\":");
        if (pIdx != -1) {
          pcBridgePort = line.substring(pIdx + 7).toInt();
        }
        Serial.printf("[VOICE] PC Bridge configurado: %s:%d\n", pcBridgeIp.c_str(), pcBridgePort);
      } else if (line.indexOf("\"type\":\"READY\"") != -1) {
        voiceActiveAgent = "AGY";
        voiceBridgeConnected = true;
        redraw = true;
      } else if (line.indexOf("\"type\":\"TRANS\"") != -1) {
        int tIdx = line.indexOf("\"text\":\"");
        if (tIdx != -1) {
          int tEnd = line.indexOf("\"", tIdx + 8);
          voiceTranscription = line.substring(tIdx + 8, tEnd);
          redraw = true;
        }
      } else if (line.indexOf("\"type\":\"RESULT\"") != -1 || line.indexOf("\"type\":\"IGNORE\"") != -1) {
        parseVoiceAiResponse(line);
      }
    }
  }
}


// ============================================================
// NAVEGACAO
// ============================================================

uint8_t itemCount() {
  switch (screen) {
    case Screen::MAIN:                return 7;
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
    case Screen::SETTINGS_MENU: return 4;
    case Screen::SETTINGS_BRIGHTNESS: return 5;
    case Screen::SETTINGS_CLOCK: return 1;
    case Screen::SETTINGS_SLEEP: return 1;
    case Screen::MOUSE: return 1;
    case Screen::VOICE_AI: return 1;
  }
  return 1;
}

void nextItem() {
  if (screen == Screen::SETTINGS_CLOCK) {
    currentWatchfaceStyle = (currentWatchfaceStyle + 1) % 3;
    prefs.begin("system", false);
    prefs.putUChar("wf_style", currentWatchfaceStyle);
    prefs.end();
    if (currentWatchfaceStyle == 0) showToast("Estilo: Aurora", 900);
    else if (currentWatchfaceStyle == 1) showToast("Estilo: Minimal", 900);
    else showToast("Estilo: Painel", 900);
    forceFullRedraw = true;
    redraw = true;
    return;
  } else if (screen == Screen::SETTINGS_BRIGHTNESS) {
    brightnessIndex = (brightnessIndex + 1) % 5;
    M5.Display.setBrightness(BRIGHTNESS_LEVELS[brightnessIndex]);
  } else if (screen == Screen::TEAM_CATTLE_LIMIT) {
    cattleMaxNumber = (cattleMaxNumber + 1) % 10;
  } else if (screen == Screen::TEAM_CATTLE_COUNTER) {
    if (cattleRemainingCount() > 0) {
      for (uint8_t step = 1; step <= cattleMaxNumber + 1; step++) {
        uint8_t candidate = (cattleSelectedNumber + step) % (cattleMaxNumber + 1);
        if (!isCattleDrawn(candidate)) {
          cattleSelectedNumber = candidate;
          break;
        }
      }
    }
    saveCattleSession();
  } else if (screen == Screen::TEAM_TRAIN_COUNT) {
    trainingSetupCount = trainingSetupCount % MAX_TRAIN_HORSES + 1;
  } else if (screen == Screen::TEAM_TRAIN_SELECT_HORSE) {
    normalizeTrainingCandidate(1);
  } else if (screen == Screen::TEAM_TRAIN_ACTIVE) {
    const uint8_t hCount = max<uint8_t>(1, trainingSession.horseCount);
    trainingSession.currentHorse = (trainingSession.currentHorse + 1) % hCount; saveTrainingSession();
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
  if (screen == Screen::SETTINGS_CLOCK) {
    currentWatchfaceStyle = (currentWatchfaceStyle + 2) % 3;
    prefs.begin("system", false);
    prefs.putUChar("wf_style", currentWatchfaceStyle);
    prefs.end();
    if (currentWatchfaceStyle == 0) showToast("Estilo: Aurora", 900);
    else if (currentWatchfaceStyle == 1) showToast("Estilo: Minimal", 900);
    else showToast("Estilo: Painel", 900);
    forceFullRedraw = true;
    redraw = true;
    return;
  } else if (screen == Screen::SETTINGS_BRIGHTNESS) {
    brightnessIndex = (brightnessIndex + 4) % 5;
    M5.Display.setBrightness(BRIGHTNESS_LEVELS[brightnessIndex]);
  } else if (screen == Screen::TEAM_CATTLE_LIMIT) {
    cattleMaxNumber = (cattleMaxNumber + 9) % 10;
  } else if (screen == Screen::TEAM_CATTLE_COUNTER) {
    if (cattleRemainingCount() > 0) {
      for (uint8_t step = 1; step <= cattleMaxNumber + 1; step++) {
        uint8_t candidate = (cattleSelectedNumber + cattleMaxNumber + 1 - (step % (cattleMaxNumber + 1))) % (cattleMaxNumber + 1);
        if (!isCattleDrawn(candidate)) {
          cattleSelectedNumber = candidate;
          break;
        }
      }
    }
    saveCattleSession();
  } else if (screen == Screen::TEAM_TRAIN_COUNT) {
    trainingSetupCount = trainingSetupCount == 1 ? MAX_TRAIN_HORSES : trainingSetupCount - 1;
  } else if (screen == Screen::TEAM_TRAIN_SELECT_HORSE) {
    normalizeTrainingCandidate(-1);
  } else if (screen == Screen::TEAM_TRAIN_ACTIVE) {
    const uint8_t hCount = max<uint8_t>(1, trainingSession.horseCount);
    trainingSession.currentHorse = (trainingSession.currentHorse + hCount - 1) % hCount; saveTrainingSession();
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
      selected = (uint8_t)HomeApp::MOUSE;
      redraw = true;
      return;

    case Screen::VOICE_AI:
      screen = Screen::MAIN;
      selected = (uint8_t)HomeApp::AI;
      redraw = true;
      return;

    case Screen::SETTINGS_CLOCK:
      if (clockReturnToSettings) {
        screen = Screen::SETTINGS_MENU;
        selected = 1;
      } else {
        screen = Screen::MAIN;
        selected = (uint8_t)HomeApp::CLOCK;
      }
      forceFullRedraw = true;
      redraw = true;
      return;

    case Screen::WIFI_MENU:
      screen = Screen::MAIN;
      selected = (uint8_t)HomeApp::WIFI;
      redraw = true;
      return;

    case Screen::SETTINGS_MENU:
      screen = Screen::MAIN;
      selected = (uint8_t)HomeApp::SETTINGS;
      redraw = true;
      return;

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
      selected = (uint8_t)HomeApp::REMOTE;
      redraw = true;
      return;

    case Screen::TEAM_MENU:
      screen = Screen::MAIN;
      selected = (uint8_t)HomeApp::TEAM;
      redraw = true;
      return;

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
      switch ((HomeApp)(selected % HOME_APP_COUNT)) {
        case HomeApp::AI:
          screen = Screen::VOICE_AI;
          initVoiceAiScreen();
          break;
        case HomeApp::REMOTE: screen = Screen::IR_TYPES; break;
        case HomeApp::TEAM:   screen = Screen::TEAM_MENU; break;
        case HomeApp::MOUSE:
          screen = Screen::MOUSE;
          startBleMouse();
          resetMouseCalibration();
          mouseSmoothDx = 0.0f;
          mouseSmoothDy = 0.0f;
          break;
        case HomeApp::WIFI:   screen = Screen::WIFI_MENU; break;
        case HomeApp::CLOCK:
          clockReturnToSettings = false;
          screen = Screen::SETTINGS_CLOCK;
          forceFullRedraw = true;
          break;
        case HomeApp::SETTINGS: screen = Screen::SETTINGS_MENU; break;
      }
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
        uiState.grid.press(selected);
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
      uiState.grid.press(selected);
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
      if (trainingHorseAlreadyChosen(trainingSetupCandidate, trainingSetupSlot)) normalizeTrainingCandidate(1);
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
      else if (selected == 1) { clockReturnToSettings = true; screen = Screen::SETTINGS_CLOCK; forceFullRedraw = true; }
      else if (selected == 2) screen = Screen::SETTINGS_SLEEP;
      else {
        // alterna sons de toque sem sair do menu
        uiState.soundEnabled = !uiState.soundEnabled;
        saveSystemSettings();
        uiConfirm();
        showToast(uiState.soundEnabled ? "Sons ligados" : "Sons desligados", 900);
        break;
      }
      selected = 0;
      break;

    case Screen::SETTINGS_BRIGHTNESS:
      saveSystemSettings();
      screen = Screen::SETTINGS_MENU;
      selected = 0;
      showToast("Brilho salvo", 1000);
      break;

    case Screen::SETTINGS_CLOCK:
      if (WiFi.status() == WL_CONNECTED) {
        if (M5.Speaker.isEnabled()) {
          M5.Speaker.tone(1500, 50);
          delay(60);
          M5.Speaker.tone(2000, 70);
        }
        showToast("SINCRONIZANDO NTP...", 1000);
        syncClockFromInternet(-10800);
        lastWeatherAttemptAt = 0;
        if (updateLocationAndWeather()) {
          if (M5.Speaker.isEnabled()) M5.Speaker.tone(2400, 80);
          showToast("HORARIO E CLIMA OK", 1200);
        } else if (clockIsValid()) {
          if (M5.Speaker.isEnabled()) M5.Speaker.tone(2200, 80);
          showToast("NTP SINCRONIZADO", 1200);
        } else {
          showToast("FALHA NA INTERNET", 1400);
        }
      } else {
        if (M5.Speaker.isEnabled()) M5.Speaker.tone(400, 120);
        showToast("SEM WIFI (CONECTE NO HUB)", 1200);
      }
      forceFullRedraw = true;
      redraw = true;
      break;

    case Screen::SETTINGS_SLEEP:
      screen = Screen::SETTINGS_MENU;
      selected = 0;
      break;

    case Screen::AC_REMOTE:
      uiState.grid.press(selected);
      executeAcAction(selected);
      break;
  }

  redraw = true;
}

// ============================================================
// SETUP / LOOP
// ============================================================


// ============================================================
// BOOT INTRO & HARDWARE DIAGNOSTICS (CYBER OS)
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
  updateBatteryState(true);
  M5.Display.setRotation(3);
  M5.Display.setBrightness(153);
  M5.Display.setTextWrap(false);

  // Mesmo limiar de "segurar" nos tres botoes (o padrao da M5Unified e 500 ms).
  M5.BtnA.setHoldThresh(BTN_HOLD_MS);
  M5.BtnB.setHoldThresh(BTN_HOLD_MS);
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

  showBootIntro();
  drawScreen();

  // Inicia conexão automática em segundo plano à rede pré-configurada Ribeiro
  beginWifiConnection("Ribeiro", "Jv22019198@", WifiConnectSource::AUTO_BOOT);

  Serial.println();
  Serial.println("M5 PERSONAL v0.9.6 - AIR MOUSE & UI iniciado.");
  Serial.println("Rotacao 3: emissor IR deve ficar para cima.");
}

static uint32_t uiLastFrameAt = 0;

void loop() {
  if (webServerRunning) webServer.handleClient();
  processWifiConnection();
  processWifiMaintenance();

  M5.update();
  buttonC.update();
  processVoiceTransport();
  updateBatteryState(false);
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
    if (redraw || (ui::framePending() && millis() - uiLastFrameAt >= 33)) { drawScreen(); uiLastFrameAt = millis(); }
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
    if (ready && !M5.BtnA.isPressed() && !M5.BtnB.isPressed()) mouseButtonsArmed = true;
    if (mouseButtonsArmed && ready) {
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
          mouseCalibration.add(gx, gy, gz, gravity);
          if (now - mouseUiAt >= 50) {
            mouseUiAt = now;
            drawMouseCalibrationBar((100 * mouseCalibration.count) / MouseCalibration::target);
          }
          if (mouseCalibration.ready) {
            mouseBiasGx = mouseCalibration.mean[0];
            mouseBiasGy = mouseCalibration.mean[1];
            mouseBiasGz = mouseCalibration.mean[2];
            mouseCalibrated = true;
            redraw = true;
            Serial.println("[AIR-MOUSE] Repouso calibrado; pronto.");
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
            updateMouseCrosshair(rx, ry, M5.BtnA.isPressed() || M5.BtnB.isPressed() ? ui::YELLOW : ui::CYAN);
          }
        }
      }
    }
    // Sem quadros continuos aqui (latencia do mouse); so a transicao de entrada anima.
    if (redraw || uiState.transition.active) drawScreen();
    delay(1);
    return;
  }

  // Team Penning: BOIS SORTEADOS
  if (screen == Screen::TEAM_CATTLE_COUNTER) {
    if (M5.BtnA.wasPressed()) {
      teamAPressedAt = millis();
      teamAHoldTriggered = false;
    }
    if (M5.BtnA.isPressed() && !teamAHoldTriggered && millis() - teamAPressedAt >= 900) {
      teamAHoldTriggered = true;
      screen = Screen::TEAM_CATTLE_RESET_CONFIRM;
      if (M5.Speaker.isEnabled()) M5.Speaker.tone(1400, 40);
      redraw = true;
    }
    if (M5.BtnA.wasReleased() && !teamAHoldTriggered) {
      markSelectedCattle();
      uiState.bounce.trigger();
      if (M5.Speaker.isEnabled()) M5.Speaker.tone(1800, 30);
    }
    if (M5.BtnB.wasClicked()) {
      nextItem();
      if (M5.Speaker.isEnabled()) M5.Speaker.tone(1200, 20);
    }
    if (buttonC.wasClicked()) {
      previousItem();
      if (M5.Speaker.isEnabled()) M5.Speaker.tone(1200, 20);
    }
    if (M5.BtnB.wasHold() || buttonC.wasHeld()) {
      screen = Screen::TEAM_MENU;
      selected = 0;
      redraw = true;
    }
    if (redraw || (ui::framePending() && millis() - uiLastFrameAt >= 33)) { drawScreen(); uiLastFrameAt = millis(); }
    delay(1);
    return;
  }

  // Team Penning: TREINO ATIVO
  if (screen == Screen::TEAM_TRAIN_ACTIVE) {
    if (M5.BtnA.wasPressed()) {
      teamAPressedAt = millis();
      teamAHoldTriggered = false;
    }
    if (M5.BtnA.isPressed() && !teamAHoldTriggered && millis() - teamAPressedAt >= 900) {
      teamAHoldTriggered = true;
      screen = Screen::TEAM_TRAIN_END_CONFIRM;
      if (M5.Speaker.isEnabled()) M5.Speaker.tone(1400, 40);
      redraw = true;
    }
    if (M5.BtnA.wasReleased() && !teamAHoldTriggered) {
      addTrainingPass();
      uiState.bounce.trigger();
      if (M5.Speaker.isEnabled()) M5.Speaker.tone(1800, 30);
    }
    if (M5.BtnB.wasClicked()) {
      nextItem();
      if (M5.Speaker.isEnabled()) M5.Speaker.tone(1200, 20);
    }
    if (buttonC.wasClicked()) {
      removeTrainingPass();
      if (M5.Speaker.isEnabled()) M5.Speaker.tone(800, 30);
    }
    if (M5.BtnB.wasHold() || buttonC.wasHeld()) {
      screen = Screen::TEAM_MENU;
      selected = 1;
      redraw = true;
    }
    if (redraw || (ui::framePending() && millis() - uiLastFrameAt >= 33)) { drawScreen(); uiLastFrameAt = millis(); }
    delay(1);
    return;
  }

  // Team Penning: CONFIRMACOES (Reset da boiada ou Fim do Treino)
  if (screen == Screen::TEAM_CATTLE_RESET_CONFIRM || screen == Screen::TEAM_TRAIN_END_CONFIRM) {
    if (M5.BtnA.wasClicked()) {
      uiConfirm();
      executeSelected();
    } else if (M5.BtnB.wasClicked() || buttonC.wasClicked() || M5.BtnB.wasHold() || buttonC.wasHeld()) {
      uiClick();
      goBack();
    }
    if (redraw || (ui::framePending() && millis() - uiLastFrameAt >= 33)) { drawScreen(); uiLastFrameAt = millis(); }
    delay(1);
    return;
  }

  // Regra unica de navegacao (ver cabecalho do arquivo).
  if (M5.BtnB.wasHold()) {
    uiClick();
    goBack();
  } else if (M5.BtnB.wasClicked()) {
    uiClick();
    nextItem();
  }

  if (M5.BtnA.wasClicked()) {
    uiConfirm();
    executeSelected();
  }

  if (buttonC.wasHeld()) {
    if (screen == Screen::MAIN) {
      showPowerOff();
      M5.Power.powerOff();
    } else {
      uiClick();
      goBack();
    }
  } else if (buttonC.wasClicked()) {
    uiClick();
    // Em listas e ajustes, C curto e o inverso de B curto; onde nao ha nada para
    // "subir", vira voltar.
    if (screenUsesPreviousItem()) {
      previousItem();
    } else {
      goBack();
    }
  }

  static bool toastWasVisible = false;
  const bool toastVisible = isToastActive();

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
  static int lastClockSec = -1;
  if (clockIsValid()) {
    time_t tnow = time(nullptr);
    tm tval;
    localtime_r(&tnow, &tval);

    if (screen == Screen::SETTINGS_CLOCK) {
      if (tval.tm_sec != lastClockSec) {
        lastClockSec = tval.tm_sec;
        redraw = true; // Atualizacao segundo a segundo para o Watchface!
      }
    }

    if (tval.tm_min != lastClockMin) {
      lastClockMin = tval.tm_min;
      if (isMenuScreen(screen)) {
        updateStatusBarClock();
      }
    }
  }

  // Quadros animados (~30 fps) enquanto houver animacao; imediato quando ha mudanca.
  if (redraw || (ui::framePending() && millis() - uiLastFrameAt >= 33)) {
    drawScreen();
    uiLastFrameAt = millis();
  }

  delay(redraw || ui::framePending() ? 2 : 10);
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
  if (found == WIFI_SCAN_RUNNING) {
    if (millis() - wifiScanStartedAt >= 10000UL) {
      Serial.println("[WIFI] Scan timeout apos 10s - abortando scan");
      WiFi.scanDelete();
      wifiScanRunning = false;
      wifiScanHasResults = true;
      if (wifiScanForMenu && screen == Screen::WIFI_SCANNING) {
        showToast("TIMEOUT NO SCAN", 1200);
        screen = Screen::WIFI_MENU;
        redraw = true;
      }
    }
    return;
  }
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
