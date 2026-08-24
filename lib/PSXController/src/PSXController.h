#pragma once
#include <Arduino.h>

struct PSXControllerState {
    bool connected=false;
    bool analogMode=false;
    bool pressureMode=false;
    uint8_t mode=0;
    uint16_t buttons=0;
    uint8_t leftX=128,leftY=128,rightX=128,rightY=128;
    uint8_t pressureUp=0,pressureRight=0,pressureDown=0,pressureLeft=0;
    uint8_t pressureTriangle=0,pressureCircle=0,pressureCross=0,pressureSquare=0;
    uint8_t pressureL1=0,pressureR1=0,pressureL2=0,pressureR2=0;
};

enum class PSXButton:uint8_t { Select=0,L3,R3,Start,Up,Right,Down,Left,L2,R2,L1,R1,Triangle,Circle,Cross,Square };
enum class PSXControllerMode:uint8_t { Unknown=0x00,Digital=0x41,Analog=0x73,Pressure=0x79 };

struct PSXControllerPins { int8_t data=-1,command=-1,attention=-1,clock=-1; };
struct PSXControllerConfig {
    bool requestAnalog=true;
    bool requestPressure=true;
    bool lockAnalogMode=true;
    uint16_t clockDelayUs=4;
    uint16_t retryIntervalMs=1000;
};

class PSXController {
public:
    PSXController()=default;
    explicit PSXController(const PSXControllerPins& pins);
    bool begin();
    bool begin(const PSXControllerPins& pins);
    bool begin(const PSXControllerPins& pins,const PSXControllerConfig& config);
    void end();
    bool update();
    const PSXControllerState& state() const;
    PSXControllerState read();
    bool connected() const;
    bool analogMode() const;
    bool pressureMode() const;
    PSXControllerMode mode() const;
    bool pressed(PSXButton button) const;
    uint8_t pressure(PSXButton button) const;
    bool configure();
    bool setAnalogMode(bool enabled=true,bool lock=true);
    bool enablePressureMode(bool enabled=true);
    const PSXControllerPins& pins() const;
    const PSXControllerConfig& config() const;
private:
    static constexpr uint8_t MODE_DIGITAL=0x41,MODE_ANALOG=0x73,MODE_PRESSURE=0x79;
    PSXControllerPins _pins{};
    PSXControllerConfig _config{};
    PSXControllerState _state{};
    bool _started=false;
    uint32_t _lastConfigAttempt=0;
    void resetState();
    uint8_t transfer(uint8_t value);
    void beginTransaction();
    void endTransaction();
    bool command(const uint8_t* tx,uint8_t* rx,size_t length);
    bool poll();
    bool enterConfigMode();
    bool exitConfigMode();
    bool isValidMode(uint8_t mode) const;
    void markDisconnected();
};
