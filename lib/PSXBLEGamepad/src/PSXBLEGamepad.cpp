#include "PSXBLEGamepad.h"
#include "PSXHIDDescriptor.h"

static const NimBLEUUID HID_SERVICE("1812");
static const NimBLEUUID BATTERY_SERVICE("180F");
static const NimBLEUUID REPORT_MAP("2A4B");
static const NimBLEUUID HID_INFO("2A4A");
static const NimBLEUUID CONTROL_POINT("2A4C");
static const NimBLEUUID PROTOCOL_MODE("2A4E");
static const NimBLEUUID INPUT_REPORT("2A4D");
static const NimBLEUUID BATTERY_LEVEL("2A19");

bool PSXBLEGamepad::begin(const char* deviceName) {
    if (_started) {
        return true;
    }

    NimBLEDevice::init(deviceName);

    _server = NimBLEDevice::createServer();
    if (_server == nullptr) {
        return false;
    }

    _server->setCallbacks(this);

    NimBLEService* hid = _server->createService(HID_SERVICE);
    if (hid == nullptr) {
        return false;
    }

    NimBLECharacteristic* reportMap = hid->createCharacteristic(
        REPORT_MAP, NIMBLE_PROPERTY::READ
    );
    reportMap->setValue(PSX_HID_REPORT_DESCRIPTOR, PSX_HID_REPORT_DESCRIPTOR_SIZE);

    NimBLECharacteristic* hidInfo = hid->createCharacteristic(
        HID_INFO, NIMBLE_PROPERTY::READ
    );
    const uint8_t info[] = {0x11, 0x01, 0x00, 0x02};
    hidInfo->setValue(info, sizeof(info));

    hid->createCharacteristic(
        CONTROL_POINT, NIMBLE_PROPERTY::WRITE_NR
    );

    NimBLECharacteristic* protocolMode = hid->createCharacteristic(
        PROTOCOL_MODE, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR
    );
    const uint8_t reportProtocol = 0x01;
    protocolMode->setValue(&reportProtocol, sizeof(reportProtocol));

    _input = hid->createCharacteristic(
        INPUT_REPORT,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    _input->setValue(reinterpret_cast<uint8_t*>(&_report), sizeof(_report));

    hid->start();

    NimBLEService* batteryService = _server->createService(BATTERY_SERVICE);
    if (batteryService != nullptr) {
        _battery = batteryService->createCharacteristic(
            BATTERY_LEVEL,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );
        const uint8_t initialBattery = 100;
        _battery->setValue(&initialBattery, sizeof(initialBattery));
        batteryService->start();
    }

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(HID_SERVICE);
    advertising->addServiceUUID(BATTERY_SERVICE);
    advertising->setAppearance(0x03C4);
    advertising->start();

    _started = true;
    _connected = false;
    return true;
}

void PSXBLEGamepad::end() {
    if (!_started) {
        return;
    }

    _connected = false;
    _started = false;
    _input = nullptr;
    _battery = nullptr;
    _server = nullptr;
    NimBLEDevice::deinit(true);
}

void PSXBLEGamepad::update(const PSXInputState& input) {
    _report.buttons = input.connected ? input.buttons : 0;
    _report.hat = input.connected ? input.hat : 8;
    _report.lx = input.connected ? input.leftX : 0;
    _report.ly = input.connected ? input.leftY : 0;
    _report.rx = input.connected ? input.rightX : 0;
    _report.ry = input.connected ? input.rightY : 0;
    _report.l2 = input.connected ? input.l2 : 0;
    _report.r2 = input.connected ? input.r2 : 0;
}

bool PSXBLEGamepad::send(const PSXInputState& input) {
    update(input);
    return send();
}

bool PSXBLEGamepad::send() {
    if (!_started || !_connected || _input == nullptr) {
        return false;
    }

    _input->setValue(reinterpret_cast<uint8_t*>(&_report), sizeof(_report));
    return _input->notify();
}

bool PSXBLEGamepad::connected() const {
    return _connected;
}

bool PSXBLEGamepad::started() const {
    return _started;
}

void PSXBLEGamepad::setBatteryLevel(uint8_t percent) {
    if (_battery == nullptr) {
        return;
    }

    if (percent > 100) {
        percent = 100;
    }

    _battery->setValue(&percent, sizeof(percent));
    if (_connected) {
        _battery->notify();
    }
}

void PSXBLEGamepad::onConnect(NimBLEServer*, NimBLEConnInfo&) {
    _connected = true;
}

void PSXBLEGamepad::onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) {
    _connected = false;

    if (_started) {
        NimBLEDevice::getAdvertising()->start();
    }
}
