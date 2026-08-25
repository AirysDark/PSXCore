#include "PSXController.h"

PSXController::PSXController(const PSXControllerPins& pins):_pins(pins) {}
bool PSXController::begin(const PSXControllerPins& pins){ _pins=pins; return begin(); }
bool PSXController::begin(const PSXControllerPins& pins,const PSXControllerConfig& config){ _pins=pins; _config=config; return begin(); }

bool PSXController::begin(){
    if(_pins.data<0||_pins.command<0||_pins.attention<0||_pins.clock<0) return false;
    pinMode(_pins.data,INPUT_PULLUP); pinMode(_pins.command,OUTPUT); pinMode(_pins.attention,OUTPUT); pinMode(_pins.clock,OUTPUT);
    if(_pins.analogButton>=0){
        pinMode(_pins.analogButton,_config.analogButtonActiveLow?INPUT_PULLUP:INPUT);
        _analogButtonRaw=_config.analogButtonActiveLow?!digitalRead(_pins.analogButton):digitalRead(_pins.analogButton);
        _analogButtonPressed=_analogButtonRaw;
    }
    digitalWrite(_pins.command,HIGH); digitalWrite(_pins.attention,HIGH); digitalWrite(_pins.clock,HIGH);
    resetState(); _started=true; delay(50);
    if(!poll()) return false;
    _lastAnalogMode=_state.analogMode;
    if(_config.requestAnalog||_config.requestPressure) configure();
    return poll();
}

void PSXController::end(){ _started=false; resetState(); _systemEvent=PSXSystemButtonEvent::None; _analogButtonPressed=false; _analogLongPressSent=false; }

bool PSXController::update(){
    if(!_started) return false;
    updateAnalogButton();
    const bool ok=poll();
    updateAnalogModeEvent();
    const bool needsConfig=_state.connected&&((_config.requestAnalog&&!_state.analogMode)||(_config.requestPressure&&!_state.pressureMode));
    const uint32_t now=millis();
    if((!ok||needsConfig)&&(now-_lastConfigAttempt>=_config.retryIntervalMs)){
        _lastConfigAttempt=now;
        configure();
        const bool repoll=poll();
        updateAnalogModeEvent();
        return repoll;
    }
    return ok;
}

const PSXControllerState& PSXController::state() const { return _state; }
PSXControllerState PSXController::read(){ update(); return _state; }
bool PSXController::connected() const { return _state.connected; }
bool PSXController::analogMode() const { return _state.analogMode; }
bool PSXController::pressureMode() const { return _state.pressureMode; }
PSXControllerMode PSXController::mode() const { return static_cast<PSXControllerMode>(_state.mode); }
bool PSXController::pressed(PSXButton button) const { return (_state.buttons&(1u<<static_cast<uint8_t>(button)))!=0; }
bool PSXController::analogButtonPressed() const { return _analogButtonPressed; }
PSXSystemButtonEvent PSXController::systemEvent(){ const auto event=_systemEvent; _systemEvent=PSXSystemButtonEvent::None; return event; }
PSXSystemButtonEvent PSXController::peekSystemEvent() const { return _systemEvent; }
void PSXController::clearSystemEvent(){ _systemEvent=PSXSystemButtonEvent::None; }

uint8_t PSXController::pressure(PSXButton button) const {
    switch(button){
        case PSXButton::Up:return _state.pressureUp; case PSXButton::Right:return _state.pressureRight; case PSXButton::Down:return _state.pressureDown; case PSXButton::Left:return _state.pressureLeft;
        case PSXButton::Triangle:return _state.pressureTriangle; case PSXButton::Circle:return _state.pressureCircle; case PSXButton::Cross:return _state.pressureCross; case PSXButton::Square:return _state.pressureSquare;
        case PSXButton::L1:return _state.pressureL1; case PSXButton::R1:return _state.pressureR1; case PSXButton::L2:return _state.pressureL2; case PSXButton::R2:return _state.pressureR2;
        default:return 0;
    }
}

bool PSXController::configure(){
    if(!_started) return false;
    _lastConfigAttempt=millis();
    if(!enterConfigMode()) return false;
    bool ok=true;
    if(_config.requestAnalog) ok=setAnalogMode(true,_config.lockAnalogMode)&&ok;
    if(ok&&_config.requestPressure) ok=enablePressureMode(true)&&ok;
    const bool exited=exitConfigMode();
    delay(10);
    return ok&&exited;
}

bool PSXController::setAnalogMode(bool enabled,bool lock){
    const uint8_t tx[]={0x01,0x44,0x00,static_cast<uint8_t>(enabled?0x01:0x00),static_cast<uint8_t>(lock?0x03:0x00),0x00,0x00,0x00,0x00};
    uint8_t rx[sizeof(tx)]{};
    return command(tx,rx,sizeof(tx))&&rx[1]!=0xFF&&rx[1]!=0x00;
}

bool PSXController::enablePressureMode(bool enabled){
    const uint8_t value=enabled?0xFF:0x00;
    const uint8_t tx[]={0x01,0x4F,0x00,value,value,static_cast<uint8_t>(enabled?0x03:0x00),0x00,0x00,0x00};
    uint8_t rx[sizeof(tx)]{};
    return command(tx,rx,sizeof(tx))&&rx[1]!=0xFF&&rx[1]!=0x00;
}

const PSXControllerPins& PSXController::pins() const { return _pins; }
const PSXControllerConfig& PSXController::config() const { return _config; }
void PSXController::resetState(){ _state=PSXControllerState{}; }

void PSXController::setSystemEvent(PSXSystemButtonEvent event){
    // Preserve the first unread event so a short press cannot overwrite a wake event.
    if(_systemEvent==PSXSystemButtonEvent::None) _systemEvent=event;
}

void PSXController::updateAnalogButton(){
    if(_pins.analogButton<0) return;
    const uint32_t now=millis();
    const bool raw=_config.analogButtonActiveLow?!digitalRead(_pins.analogButton):digitalRead(_pins.analogButton);
    if(raw!=_analogButtonRaw){ _analogButtonRaw=raw; _analogButtonChangedAt=now; }
    if((now-_analogButtonChangedAt)>=_config.analogButtonDebounceMs && _analogButtonPressed!=_analogButtonRaw){
        _analogButtonPressed=_analogButtonRaw;
        if(_analogButtonPressed){
            _analogButtonPressedAt=now;
            _analogLongPressSent=false;
            setSystemEvent(PSXSystemButtonEvent::AnalogPressed);
        }else{
            setSystemEvent(PSXSystemButtonEvent::AnalogReleased);
            if(!_analogLongPressSent) setSystemEvent(PSXSystemButtonEvent::AnalogShortPress);
        }
    }
    if(_analogButtonPressed&&!_analogLongPressSent&&(now-_analogButtonPressedAt)>=_config.analogButtonLongPressMs){
        _analogLongPressSent=true;
        setSystemEvent(PSXSystemButtonEvent::AnalogLongPress);
    }
}

void PSXController::updateAnalogModeEvent(){
    if(_state.analogMode==_lastAnalogMode) return;
    _lastAnalogMode=_state.analogMode;
    setSystemEvent(_state.analogMode?PSXSystemButtonEvent::AnalogModeEnabled:PSXSystemButtonEvent::AnalogModeDisabled);
}

uint8_t PSXController::transfer(uint8_t value){
    uint8_t result=0;
    for(uint8_t bit=0;bit<8;++bit){
        digitalWrite(_pins.command,(value&(1u<<bit))?HIGH:LOW); delayMicroseconds(1);
        digitalWrite(_pins.clock,LOW); delayMicroseconds(_config.clockDelayUs);
        if(digitalRead(_pins.data)) result|=(1u<<bit);
        digitalWrite(_pins.clock,HIGH); delayMicroseconds(_config.clockDelayUs);
    }
    return result;
}

void PSXController::beginTransaction(){ digitalWrite(_pins.attention,LOW); delayMicroseconds(10); }
void PSXController::endTransaction(){ digitalWrite(_pins.command,HIGH); digitalWrite(_pins.clock,HIGH); digitalWrite(_pins.attention,HIGH); delayMicroseconds(10); }

bool PSXController::command(const uint8_t* tx,uint8_t* rx,size_t length){
    if(!_started||!tx||length<3) return false;
    beginTransaction();
    for(size_t i=0;i<length;++i){ const uint8_t value=transfer(tx[i]); if(rx) rx[i]=value; }
    endTransaction();
    return true;
}

bool PSXController::enterConfigMode(){
    const uint8_t tx[]={0x01,0x43,0x00,0x01,0x00,0x00,0x00,0x00,0x00}; uint8_t rx[sizeof(tx)]{};
    return command(tx,rx,sizeof(tx))&&rx[1]!=0xFF&&rx[1]!=0x00;
}

bool PSXController::exitConfigMode(){
    const uint8_t tx[]={0x01,0x43,0x00,0x00,0x5A,0x5A,0x5A,0x5A,0x5A}; uint8_t rx[sizeof(tx)]{};
    return command(tx,rx,sizeof(tx))&&rx[1]!=0xFF&&rx[1]!=0x00;
}

bool PSXController::isValidMode(uint8_t mode) const { return mode==MODE_DIGITAL||mode==MODE_ANALOG||mode==MODE_PRESSURE; }
void PSXController::markDisconnected(){ resetState(); }

bool PSXController::poll(){
    const uint8_t request[]={0x01,0x42,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    uint8_t response[sizeof(request)]{};
    if(!command(request,response,sizeof(request))){ markDisconnected(); return false; }
    const uint8_t controllerMode=response[1];
    if(!isValidMode(controllerMode)||response[2]!=0x5A){ markDisconnected(); return false; }

    _state.connected=true; _state.mode=controllerMode;
    _state.analogMode=(controllerMode==MODE_ANALOG||controllerMode==MODE_PRESSURE);
    _state.pressureMode=(controllerMode==MODE_PRESSURE);
    _state.buttons=static_cast<uint16_t>(~(static_cast<uint16_t>(response[3])|(static_cast<uint16_t>(response[4])<<8)));

    if(_state.analogMode){ _state.rightX=response[5]; _state.rightY=response[6]; _state.leftX=response[7]; _state.leftY=response[8]; }
    else { _state.rightX=_state.rightY=_state.leftX=_state.leftY=128; }

    _state.pressureUp=_state.pressureRight=_state.pressureDown=_state.pressureLeft=0;
    _state.pressureL2=_state.pressureR2=_state.pressureL1=_state.pressureR1=0;
    _state.pressureTriangle=_state.pressureCircle=_state.pressureCross=_state.pressureSquare=0;
    if(_state.pressureMode){
        _state.pressureUp=response[9]; _state.pressureRight=response[10]; _state.pressureDown=response[11]; _state.pressureLeft=response[12];
        _state.pressureL2=response[13]; _state.pressureR2=response[14]; _state.pressureL1=response[15]; _state.pressureR1=response[16];
        _state.pressureTriangle=response[17]; _state.pressureCircle=response[18]; _state.pressureCross=response[19]; _state.pressureSquare=response[20];
    }
    return true;
}
