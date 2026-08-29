#include "obd2_scanner.h"

namespace Obd2 {

State state;

static bool sendFrame(uint32_t id, const uint8_t* data, uint8_t len) {
  twai_message_t msg = {};
  msg.identifier = id;
  msg.extd = 0;
  msg.rtr = 0;
  msg.data_length_code = len;
  memcpy(msg.data, data, len);
  return twai_transmit(&msg, pdMS_TO_TICKS(100)) == ESP_OK;
}

static void decodeDtc(uint8_t a, uint8_t b, char out[6]) {
  static const char family[4] = {'P', 'C', 'B', 'U'};
  out[0] = family[(a >> 6) & 0x03];
  out[1] = '0' + ((a >> 4) & 0x03);
  const uint8_t n2 = a & 0x0F;
  const uint8_t n3 = (b >> 4) & 0x0F;
  const uint8_t n4 = b & 0x0F;
  const char hex[] = "0123456789ABCDEF";
  out[2] = hex[n2];
  out[3] = hex[n3];
  out[4] = hex[n4];
  out[5] = '\0';
}

static bool alreadyPresent(const char* code, DtcKind kind, uint16_t ecuId) {
  for (uint8_t i = 0; i < state.count; ++i) {
    if (state.dtcs[i].kind == kind && state.dtcs[i].ecuId == ecuId && strcmp(state.dtcs[i].code, code) == 0) {
      return true;
    }
  }
  return false;
}

static void addDtc(uint8_t a, uint8_t b, DtcKind kind, uint16_t ecuId) {
  if (a == 0 && b == 0) return;
  if (state.count >= MAX_DTCS) return;

  char code[6];
  decodeDtc(a, b, code);
  if (alreadyPresent(code, kind, ecuId)) return;

  Dtc& d = state.dtcs[state.count++];
  strncpy(d.code, code, sizeof(d.code));
  d.kind = kind;
  d.ecuId = ecuId;
}

static bool receiveMessage(twai_message_t& msg, uint32_t timeoutMs) {
  return twai_receive(&msg, pdMS_TO_TICKS(timeoutMs)) == ESP_OK;
}

static bool requestMode(uint8_t mode, DtcKind kind) {
  uint8_t req[8] = {0x02, mode, 0x00, 0, 0, 0, 0, 0};

  while (true) {
    twai_message_t dump;
    if (twai_receive(&dump, 0) != ESP_OK) break;
  }

  if (!sendFrame(0x7DF, req, 8)) {
    state.lastMessage = "Falha ao transmitir CAN";
    state.status = Status::CAN_ERROR;
    return false;
  }

  const uint32_t started = millis();
  bool gotAny = false;

  struct IsoTpRx {
    bool active = false;
    uint16_t total = 0;
    uint16_t used = 0;
    uint8_t nextSeq = 1;
    uint8_t payload[128] = {0};
  } rx[8];

  auto parsePayload = [&](uint16_t ecuId, const uint8_t* payload, uint16_t len) {
    if (len < 1 || payload[0] != static_cast<uint8_t>(mode + 0x40)) return;
    gotAny = true;
    state.ecuResponses |= (1U << (ecuId - 0x7E8));

    for (uint16_t i = 1; i + 1 < len; i += 2) {
      addDtc(payload[i], payload[i + 1], kind, ecuId);
    }
  };

  while (millis() - started < RESPONSE_TIMEOUT_MS) {
    twai_message_t msg;
    if (!receiveMessage(msg, 80)) continue;
    if (msg.extd || msg.rtr) continue;
    if (msg.identifier < 0x7E8 || msg.identifier > 0x7EF) continue;
    if (msg.data_length_code < 2) continue;

    const uint8_t idx = static_cast<uint8_t>(msg.identifier - 0x7E8);
    const uint8_t pciType = (msg.data[0] >> 4) & 0x0F;

    if (pciType == 0x0) {
      const uint8_t len = msg.data[0] & 0x0F;
      if (len <= 7 && len + 1 <= msg.data_length_code) {
        parsePayload(msg.identifier, &msg.data[1], len);
      }
      continue;
    }

    if (pciType == 0x1 && msg.data_length_code == 8) {
      IsoTpRx& s = rx[idx];
      s.active = true;
      s.total = static_cast<uint16_t>(((msg.data[0] & 0x0F) << 8) | msg.data[1]);
      s.used = min<uint16_t>(6, s.total);
      memcpy(s.payload, &msg.data[2], s.used);
      s.nextSeq = 1;

      uint8_t fc[8] = {0x30, 0x00, 0x00, 0, 0, 0, 0, 0};
      sendFrame(0x7E0 + idx, fc, 8);
      continue;
    }

    if (pciType == 0x2 && msg.data_length_code >= 2) {
      IsoTpRx& s = rx[idx];
      if (!s.active) continue;
      const uint8_t seq = msg.data[0] & 0x0F;
      if (seq != (s.nextSeq & 0x0F)) {
        s.active = false;
        continue;
      }
      s.nextSeq++;

      const uint16_t remaining = s.total > s.used ? s.total - s.used : 0;
      const uint8_t available = msg.data_length_code - 1;
      const uint8_t take = min<uint16_t>(remaining, available);
      if (s.used + take <= sizeof(s.payload)) {
        memcpy(&s.payload[s.used], &msg.data[1], take);
        s.used += take;
      } else {
        s.active = false;
        continue;
      }

      if (s.used >= s.total) {
        parsePayload(msg.identifier, s.payload, s.total);
        s.active = false;
      }
    }
  }

  return gotAny;
}

bool begin() {
  if (state.canStarted) return true;

  state.status = Status::STARTING;
  state.lastMessage = "Iniciando CAN 500 kbps";

  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  esp_err_t err = twai_driver_install(&g, &t, &f);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    state.status = Status::CAN_ERROR;
    state.lastMessage = "Falha instalando TWAI";
    return false;
  }

  err = twai_start();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    twai_driver_uninstall();
    state.status = Status::CAN_ERROR;
    state.lastMessage = "Falha iniciando TWAI";
    return false;
  }

  state.canStarted = true;
  state.status = Status::READY;
  state.lastMessage = "CAN pronto";
  return true;
}

void end() {
  if (!state.canStarted) return;
  twai_stop();
  twai_driver_uninstall();
  state.canStarted = false;
  state.status = Status::IDLE;
  state.lastMessage = "CAN desligado";
}

void clearResults() {
  state.count = 0;
  state.ecuResponses = 0;
  memset(state.dtcs, 0, sizeof(state.dtcs));
}

bool scanStored() {
  return requestMode(0x03, DtcKind::STORED);
}

bool scanPending() {
  return requestMode(0x07, DtcKind::PENDING);
}

bool scanPermanent() {
  return requestMode(0x0A, DtcKind::PERMANENT);
}

bool scanAll() {
  if (!begin()) return false;

  clearResults();
  state.status = Status::SCANNING;
  state.lastMessage = "Lendo DTCs...";

  const bool a = scanStored();
  const bool b = scanPending();
  const bool c = scanPermanent();

  if (a || b || c) {
    state.status = Status::COMPLETE;
    state.lastMessage = state.count ? String(state.count) + " DTC(s)" : "ECU respondeu, sem DTC";
    printResultsToSerial();
    return true;
  }

  state.status = Status::NO_RESPONSE;
  state.lastMessage = "Sem resposta OBD2";
  Serial.println("[OBD2] Nenhuma ECU respondeu em 0x7E8-0x7EF");
  return false;
}

const char* statusText() {
  switch (state.status) {
    case Status::IDLE: return "DESLIGADO";
    case Status::STARTING: return "INICIANDO";
    case Status::READY: return "PRONTO";
    case Status::SCANNING: return "LENDO";
    case Status::COMPLETE: return "CONCLUIDO";
    case Status::NO_RESPONSE: return "SEM RESPOSTA";
    case Status::CAN_ERROR: return "ERRO CAN";
  }
  return "?";
}

const char* kindText(DtcKind kind) {
  switch (kind) {
    case DtcKind::STORED: return "SALVO";
    case DtcKind::PENDING: return "PEND";
    case DtcKind::PERMANENT: return "PERM";
  }
  return "?";
}

void printResultsToSerial() {
  Serial.println();
  Serial.println("===== OBD2 DTC SCAN =====");
  Serial.printf("Status: %s\n", statusText());
  Serial.printf("ECUs bitmap: 0x%04X\n", state.ecuResponses);
  Serial.printf("DTCs: %u\n", state.count);
  for (uint8_t i = 0; i < state.count; ++i) {
    Serial.printf("%02u  ECU 0x%03X  %-5s  %s\n", i + 1, state.dtcs[i].ecuId,
                  kindText(state.dtcs[i].kind), state.dtcs[i].code);
  }
  Serial.println("=========================");
}

void drawScreen(uint8_t selectedIndex) {
  auto& d = M5.Display;
  d.startWrite();
  d.fillScreen(TFT_BLACK);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setTextSize(1);
  d.setCursor(8, 8);
  d.println("OBD2 / STRADA");
  d.setTextColor(state.status == Status::COMPLETE ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
  d.setCursor(8, 25);
  d.printf("CAN: %s", statusText());
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setCursor(8, 42);
  d.print(state.lastMessage);

  if (state.count) {
    const uint8_t visible = 5;
    uint8_t start = selectedIndex;
    if (start >= state.count) start = state.count - 1;
    if (start >= visible) start -= (visible - 1); else start = 0;

    for (uint8_t row = 0; row < visible && start + row < state.count; ++row) {
      const uint8_t i = start + row;
      const int y = 62 + row * 22;
      d.setTextColor(i == selectedIndex ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
      d.setCursor(8, y);
      d.printf("%c %s %-4s", i == selectedIndex ? '>' : ' ', state.dtcs[i].code,
               kindText(state.dtcs[i].kind));
      d.setTextColor(TFT_DARKGREY, TFT_BLACK);
      d.setCursor(170, y);
      d.printf("%03X", state.dtcs[i].ecuId);
    }
  } else {
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(8, 70);
    d.println("A = LER ERROS");
    d.setCursor(8, 88);
    d.println("B = VOLTAR");
  }

  d.setTextColor(TFT_DARKGREY, TFT_BLACK);
  d.setCursor(8, d.height() - 14);
  d.print("SOMENTE LEITURA - SEM RESET");
  d.endWrite();
}

}  // namespace Obd2
