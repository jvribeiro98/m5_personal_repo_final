#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include "driver/twai.h"

namespace Obd2 {

constexpr gpio_num_t CAN_TX_PIN = GPIO_NUM_32;
constexpr gpio_num_t CAN_RX_PIN = GPIO_NUM_33;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 1800;
constexpr uint8_t MAX_DTCS = 48;

enum class Status : uint8_t {
  IDLE,
  STARTING,
  READY,
  SCANNING,
  COMPLETE,
  NO_RESPONSE,
  CAN_ERROR
};

enum class DtcKind : uint8_t {
  STORED,
  PENDING,
  PERMANENT
};

struct Dtc {
  char code[6] = {0};
  DtcKind kind = DtcKind::STORED;
  uint16_t ecuId = 0;
};

struct State {
  Status status = Status::IDLE;
  bool canStarted = false;
  uint8_t count = 0;
  Dtc dtcs[MAX_DTCS];
  uint16_t ecuResponses = 0;
  String lastMessage;
};

extern State state;

bool begin();
void end();
void clearResults();
bool scanAll();
bool scanStored();
bool scanPending();
bool scanPermanent();
const char* statusText();
const char* kindText(DtcKind kind);
void drawScreen(uint8_t selectedIndex = 0);
void printResultsToSerial();

}  // namespace Obd2
