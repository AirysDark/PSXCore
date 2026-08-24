#include "PSXBLEGamepad.h"
#include "PSXHIDDescriptor.h"

static const NimBLEUUID HID_SERVICE("1812");
static const NimBLEUUID REPORT_MAP("2A4B");
static const NimBLEUUID HID_INFO("2A4A");
static const NimBLEUUID CONTROL_POINT("2A4C");
static const NimBLEUUID PROTOCOL_MODE("2A4E");
static const NimBLEUUID INPUT_REPORT("2A4D");

bool PSXBLEGamepad::begin(const char* deviceName) {
    NimBLEDevice::init(deviceName);
    _server=NimBLEDevice::createServer();
    _server->setCallbacks(this);
    NimBLEService* hid=_server->createService(HID_SERVICE);
    NimBLECharacteristic* map=hid->createCharacteristic(REPORT_MAP,NIMBLE_PROPERTY::READ);
    map->setValue(PSX_HID_REPORT_DESCRIPTOR,PSX_HID_REPORT_DESCRIPTOR_SIZE);
    NimBLECharacteristic* info=hid->createCharacteristic(HID_INFO,NIMBLE_PROPERTY::READ);
    const uint8_t hidInfo[]={0x11,0x01,0x00,0x02}; info->setValue(hidInfo,sizeof(hidInfo));
    hid->createCharacteristic(CONTROL_POINT,NIMBLE_PROPERTY::WRITE_NR);
    NimBLECharacteristic* protocol=hid->createCharacteristic(PROTOCOL_MODE,NIMBLE_PROPERTY::READ|NIMBLE_PROPERTY::WRITE_NR);
    protocol->setValue((uint8_t)0x01);
    _input=hid->createCharacteristic(INPUT_REPORT,NIMBLE_PROPERTY::READ|NIMBLE_PROPERTY::NOTIFY);
    hid->start();
    NimBLEAdvertising* advertising=NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(HID_SERVICE);
    advertising->setAppearance(0x03C4);
    advertising->start();
    return true;
}
void PSXBLEGamepad::update(const PSXInputState& input){_report.buttons=input.buttons;_report.hat=input.hat;_report.lx=input.leftX;_report.ly=input.leftY;_report.rx=input.rightX;_report.ry=input.rightY;_report.l2=input.l2;_report.r2=input.r2;}
bool PSXBLEGamepad::send(){if(!_connected||!_input)return false;_input->setValue(reinterpret_cast<uint8_t*>(&_report),sizeof(_report));return _input->notify();}
bool PSXBLEGamepad::connected() const{return _connected;}
void PSXBLEGamepad::onConnect(NimBLEServer*,NimBLEConnInfo&){_connected=true;}
void PSXBLEGamepad::onDisconnect(NimBLEServer*,NimBLEConnInfo&,int){_connected=false;NimBLEDevice::getAdvertising()->start();}
