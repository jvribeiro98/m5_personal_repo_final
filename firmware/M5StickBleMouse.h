#pragma once

#include <Arduino.h>
#include <atomic>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEHIDDevice.h>
#include <NimBLECharacteristic.h>

constexpr uint8_t MOUSE_REPORT_ID = 0x02;

constexpr uint8_t MOUSE_BUTTON_LEFT   = 0x01;
constexpr uint8_t MOUSE_BUTTON_RIGHT  = 0x02;
constexpr uint8_t MOUSE_BUTTON_MIDDLE = 0x04;

static const uint8_t _m5StickMouseReportMap[] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xA1, 0x01,                    // COLLECTION (Application)
    0x85, MOUSE_REPORT_ID,         //   REPORT_ID (2)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xA1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x03,                    //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x05,                    //     REPORT_SIZE (5)
    0x81, 0x03,                    //     INPUT (Const,Var,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x09, 0x38,                    //     USAGE (Wheel)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7F,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)
    0xC0,                          //   END_COLLECTION
    0xC0                           // END_COLLECTION
};

class M5StickBleMouse : public NimBLEServerCallbacks, public NimBLECharacteristicCallbacks {
public:
    M5StickBleMouse()
        : _connected(false),
          _subscribed(false),
          _started(false),
          _buttons(0),
          _deviceName("M5Stick Mouse"),
          _server(nullptr),
          _hid(nullptr),
          _inputReport(nullptr) {}

    bool begin(const char* deviceName = "M5Stick Mouse") {
        if (_started) return true;
        _deviceName = deviceName ? deviceName : "M5Stick Mouse";

        // 1. Inicializa a pilha NimBLE
        NimBLEDevice::init(_deviceName);

        // 2. Seguranca robusta com LE Secure Connections (exigido por Android 11+ e Windows 10/11)
        NimBLEDevice::setSecurityAuth(/*bonding=*/true, /*mitm=*/false, /*sc=*/true);
        NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
        NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
        NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

        // 3. Cria o Servidor BLE
        _server = NimBLEDevice::createServer();
        _server->setCallbacks(this);

        // 4. Cria o dispositivo HID
        _hid = new NimBLEHIDDevice(_server);
        _inputReport = _hid->getInputReport(MOUSE_REPORT_ID);
        if (_inputReport) {
            _inputReport->setCallbacks(this);
        }

        _hid->setManufacturer("M5Stack");
        _hid->setPnp(0x02, 0x0e5a, 0x0202, 0x0100);
        _hid->setHidInfo(0x00, 0x01);
        _hid->setBatteryLevel(100);
        _hid->setReportMap(const_cast<uint8_t*>(_m5StickMouseReportMap), sizeof(_m5StickMouseReportMap));

        // 5. Inicia servicos do servidor GATT
        _server->start();

        // 6. Propaganda (Advertising) otimizada:
        // Pacote Primario: Cabe perfeitamente nos 31 bytes (Flags, Appearance, HID Service)
        NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
        adv->setAppearance(HID_MOUSE); // 0x03C2
        adv->addServiceUUID(_hid->getHidService()->getUUID());

        // Scan Response: Nome amigavel completo para Windows e Android encontrarem imediatamente
        NimBLEAdvertisementData scanData;
        scanData.setName(_deviceName);
        scanData.addServiceUUID(_hid->getBatteryService()->getUUID());
        adv->setScanResponseData(scanData);
        adv->enableScanResponse(true);

        adv->start();
        _started = true;
        Serial.printf("[BLE] Mouse inicializado como '%s' e anunciando...\n", _deviceName.c_str());
        return true;
    }

    bool isConnected() const {
        return _connected;
    }

    bool isSubscribed() const {
        return _subscribed;
    }

    void move(int8_t x, int8_t y, int8_t wheel = 0) {
        if (!_connected || !_subscribed || !_inputReport) return;
        uint8_t report[4];
        report[0] = _buttons;
        report[1] = static_cast<uint8_t>(x);
        report[2] = static_cast<uint8_t>(y);
        report[3] = static_cast<uint8_t>(wheel);
        _inputReport->setValue(report, sizeof(report));
        _inputReport->notify();
    }

    void press(uint8_t button) {
        _buttons |= button;
        move(0, 0, 0);
    }

    void release(uint8_t button) {
        _buttons &= ~button;
        move(0, 0, 0);
    }

    void click(uint8_t button) {
        press(button);
        delay(20);
        release(button);
    }

    void releaseAll() {
        _buttons = 0;
        move(0, 0, 0);
    }

    // Callbacks do Servidor BLE
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        _connected = true;
        _subscribed = false;
        _buttons = 0;
        pServer->updateConnParams(connInfo.getConnHandle(), 8, 12, 0, 200);
        Serial.printf("[BLE] Host conectado: %s\n", connInfo.getAddress().toString().c_str());
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        _connected = false;
        _subscribed = false;
        _buttons = 0;
        Serial.printf("[BLE] Host desconectado (motivo=%d). Reiniciando propaganda...\n", reason);
        NimBLEDevice::startAdvertising();
    }

    // Callbacks da Caracteristica (CCCD subscription)
    void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override {
        _subscribed = (subValue & 1) != 0;
        Serial.printf("[BLE] Subscricao de relatorios do Mouse alterada: %d\n", subValue);
    }

private:
    std::atomic<bool> _connected;
    std::atomic<bool> _subscribed;
    bool _started;
    std::atomic<uint8_t> _buttons;
    std::string _deviceName;
    NimBLEServer* _server;
    NimBLEHIDDevice* _hid;
    NimBLECharacteristic* _inputReport;
};
