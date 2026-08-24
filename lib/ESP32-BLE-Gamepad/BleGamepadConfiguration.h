#ifndef ESP32_BLE_GAMEPAD_CONFIG_H
#define ESP32_BLE_GAMEPAD_CONFIG_H

#define POSSIBLESPECIALBUTTONS 8
#define POSSIBLEAXES 8
#define POSSIBLESIMULATIONCONTROLS 5

#include <Arduino.h>

#define CONTROLLER_TYPE_JOYSTICK 0x04
#define CONTROLLER_TYPE_GAMEPAD 0x05
#define CONTROLLER_TYPE_MULTI_AXIS 0x08

#define BUTTON_1 0x1
#define BUTTON_2 0x2
#define BUTTON_3 0x3
#define BUTTON_4 0x4
#define BUTTON_5 0x5
#define BUTTON_6 0x6
#define BUTTON_7 0x7
#define BUTTON_8 0x8
#define BUTTON_9 0x9
#define BUTTON_10 0xa
#define BUTTON_11 0xb
#define BUTTON_12 0xc
#define BUTTON_13 0xd
#define BUTTON_14 0xe
#define BUTTON_15 0xf
#define BUTTON_16 0x10

#define DPAD_CENTERED 0
#define DPAD_UP 1
#define DPAD_UP_RIGHT 2
#define DPAD_RIGHT 3
#define DPAD_DOWN_RIGHT 4
#define DPAD_DOWN 5
#define DPAD_DOWN_LEFT 6
#define DPAD_LEFT 7
#define DPAD_UP_LEFT 8

#define HAT_CENTERED 0
#define HAT_UP 1
#define HAT_UP_RIGHT 2
#define HAT_RIGHT 3
#define HAT_DOWN_RIGHT 4
#define HAT_DOWN 5
#define HAT_DOWN_LEFT 6
#define HAT_LEFT 7
#define HAT_UP_LEFT 8

#define X_AXIS 0
#define Y_AXIS 1
#define Z_AXIS 2
#define RX_AXIS 3
#define RY_AXIS 4
#define RZ_AXIS 5
#define SLIDER1 6
#define SLIDER2 7

#define RUDDER 0
#define THROTTLE 1
#define ACCELERATOR 2
#define BRAKE 3
#define STEERING 4

#define START_BUTTON 0
#define SELECT_BUTTON 1
#define MENU_BUTTON 2
#define HOME_BUTTON 3
#define BACK_BUTTON 4
#define VOLUME_INC_BUTTON 5
#define VOLUME_DEC_BUTTON 6
#define VOLUME_MUTE_BUTTON 7

#define POWER_STATE_UNKNOWN 0
#define POWER_STATE_NOT_SUPPORTED 1
#define POWER_STATE_NOT_PRESENT 2
#define POWER_STATE_NOT_DISCHARGING 2
#define POWER_STATE_NOT_CHARGING 2
#define POWER_STATE_GOOD 2
#define POWER_STATE_PRESENT 3
#define POWER_STATE_DISCHARGING 3
#define POWER_STATE_CHARGING 3
#define POWER_STATE_CRITICAL 3

class BleGamepadConfiguration
{
private:
    uint8_t _controllerType;
    bool _autoReport;
    uint8_t _hidReportId;
    uint16_t _buttonCount;
    uint8_t _hatSwitchCount;
    bool _whichSpecialButtons[POSSIBLESPECIALBUTTONS];
    bool _whichAxes[POSSIBLEAXES];
    bool _whichSimulationControls[POSSIBLESIMULATIONCONTROLS];
    bool _includeGyroscope;
    bool _includeAccelerometer;
    uint16_t _vid;
    uint16_t _pid;
    uint16_t _guidVersion;
    int16_t _axesMin;
    int16_t _axesMax;
    int16_t _simulationMin;
    int16_t _simulationMax;
    int16_t _motionMin;
    int16_t _motionMax;
    const char* _modelNumber;
    const char* _softwareRevision;
    const char* _serialNumber;
    const char* _firmwareRevision;
    const char* _hardwareRevision;
    bool _enableOutputReport;
    bool _enableNordicUARTService;
    uint16_t _outputReportLength;
    int8_t _transmitPowerLevel;

public:
    BleGamepadConfiguration();

    bool getAutoReport();
    uint8_t getControllerType();
    uint8_t getHidReportId();
    uint16_t getButtonCount();
    uint8_t getTotalSpecialButtonCount();
    uint8_t getDesktopSpecialButtonCount();
    uint8_t getConsumerSpecialButtonCount();
    uint8_t getHatSwitchCount();
    uint8_t getAxisCount();
    uint8_t getSimulationCount();
    bool getIncludeStart();
    bool getIncludeSelect();
    bool getIncludeMenu();
    bool getIncludeHome();
    bool getIncludeBack();
    bool getIncludeVolumeInc();
    bool getIncludeVolumeDec();
    bool getIncludeVolumeMute();
    const bool* getWhichSpecialButtons() const;
    bool getIncludeXAxis(); bool getIncludeYAxis(); bool getIncludeZAxis();
    bool getIncludeRxAxis(); bool getIncludeRyAxis(); bool getIncludeRzAxis();
    bool getIncludeSlider1(); bool getIncludeSlider2();
    const bool* getWhichAxes() const;
    bool getIncludeRudder(); bool getIncludeThrottle(); bool getIncludeAccelerator();
    bool getIncludeBrake(); bool getIncludeSteering();
    const bool* getWhichSimulationControls() const;
    bool getIncludeAccelerometer(); bool getIncludeGyroscope();
    uint16_t getVid(); uint16_t getPid(); uint16_t getGuidVersion();
    int16_t getAxesMin(); int16_t getAxesMax();
    int16_t getSimulationMin(); int16_t getSimulationMax();
    int16_t getMotionMin(); int16_t getMotionMax();
    const char* getModelNumber();
    const char* getSoftwareRevision();
    const char* getSerialNumber();
    const char* getFirmwareRevision();
    const char* getHardwareRevision();
    bool getEnableOutputReport();
    bool getEnableNordicUARTService();
    uint16_t getOutputReportLength();
    int8_t getTXPowerLevel();

    void setControllerType(uint8_t controllerType); void setAutoReport(bool value);
    void setHidReportId(uint8_t value); void setButtonCount(uint16_t value); void setHatSwitchCount(uint8_t value);
    void setIncludeStart(bool value); void setIncludeSelect(bool value); void setIncludeMenu(bool value); void setIncludeHome(bool value); void setIncludeBack(bool value);
    void setIncludeVolumeInc(bool value); void setIncludeVolumeDec(bool value); void setIncludeVolumeMute(bool value);
    void setWhichSpecialButtons(bool start, bool select, bool menu, bool home, bool back, bool volumeInc, bool volumeDec, bool volumeMute);
    void setIncludeXAxis(bool value); void setIncludeYAxis(bool value); void setIncludeZAxis(bool value); void setIncludeRxAxis(bool value); void setIncludeRyAxis(bool value); void setIncludeRzAxis(bool value); void setIncludeSlider1(bool value); void setIncludeSlider2(bool value);
    void setWhichAxes(bool xAxis, bool yAxis, bool zAxis, bool rxAxis, bool ryAxis, bool rzAxis, bool slider1, bool slider2);
    void setIncludeRudder(bool value); void setIncludeThrottle(bool value); void setIncludeAccelerator(bool value); void setIncludeBrake(bool value); void setIncludeSteering(bool value);
    void setWhichSimulationControls(bool rudder, bool throttle, bool accelerator, bool brake, bool steering);
    void setIncludeGyroscope(bool value); void setIncludeAccelerometer(bool value);
    void setVid(uint16_t value); void setPid(uint16_t value); void setGuidVersion(uint16_t value);
    void setAxesMin(int16_t value); void setAxesMax(int16_t value); void setSimulationMin(int16_t value); void setSimulationMax(int16_t value); void setMotionMin(int16_t value); void setMotionMax(int16_t value);
    void setModelNumber(const char* value); void setSoftwareRevision(const char* value); void setSerialNumber(const char* value); void setFirmwareRevision(const char* value); void setHardwareRevision(const char* value);
    void setEnableOutputReport(bool value); void setEnableNordicUARTService(bool value); void setOutputReportLength(uint16_t value); void setTXPowerLevel(int8_t value);
};

#endif
