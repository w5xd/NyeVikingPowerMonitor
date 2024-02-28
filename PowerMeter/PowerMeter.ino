/* Copyright (c) 2023 by Wayne E. Wright
 ** W5XD
 ** Round Rock, Texas, USA
 **
 ** For terms of use, see LICENSE
 */
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <EEPROM.h>

#include "PowerMeterLEDs.h"

static_assert(sizeof(uint16_t)==2,"uint16_t");
static_assert(sizeof(int16_t)==2, "int16_t");
static_assert(sizeof(uint32_t)==4, "uint32_t");
static_assert(sizeof(int32_t)==4, "int32_t");
static_assert(sizeof(uint64_t)==8, "uint64_t");

/* Nye Viking RF Power Meter
 * Behaves like the original analog circuit board, but on Arduino. This code was tested with the RFM-003, the 3000W model
 * This sketch runs on an Arduino Pro Mini installed in either of two hardware configurations:
 * a) On the PCB (also documented in this repository) installed in an RFM-003 in place of the OEM analog board
 * ...OR...
 * b) On the PCB installed in an enclosure designed to mimic the OEM enclosure. 
 * 
 * Compile time #define's distinguish the arithmetic for these hardware options:
 * a) the OEM coupler 
        versus a home built coupler documented in this repo 
        (arithmetic differs from ADC to Power and SWR)
 * b) The front panel meters in the OEM console 
        versus custom meter faces (documented here) installed in a 0-1mA meter movement
        (lookup tables for PWM values to SWR and Power differ)
 * c) RGB LEDs for the front panel 
        versus RGY.
        (which LED gets turned on when differs)
*  d) Watchdog timer support. This is an extra reliability feature that progamming the Arduino with an ISP
 */

// Use one of the following two. 
// Affects the arithmetic calculating power/SWR from the ADCs
#define OEM_COUPLER // The OEM coupler, which, in turn requires a 15/115 input voltage divider at R12/R13/R16/R17
//#define W5XD_COUPLER // The coupler in ths report, wich requires a 100/320 voltage divider

// Use one of the following two
#define OEM_METER_SCALES  // the meters are series resistor with PWM=250 is full scale, as painted by Nye Viking
//#define CUSTOM_METER_SCALES // the meters are series resistor with PWM=250 is full scale, with meter face backings printed per this repo

/* The above two compile directives switch between look up table entries. Those look up tables are part of
** the elimination of any need floating point at run time, and any need for trig or log functions.
** We do end up with some 32 bit and 64 bit long integer arithmetic, including divides, but floating point
** is avoided by using the two fixed point integer typedef's below, AcquiredVolts_t and DisplayPower_t. */

// There is the LEDS_ARE_RGB compile time option in PowerMeterLEDs.h

//#define SUPPORT_WDT /* READ THE PARAGRAPH BELOW*/

/* do NOT enable the SUPPORT_WDT symbol when using the Pro Mini with its default boot loader.
** If you do not know what "default boot loader" means, then also do NOT enable SUPPORT_WDT
** The issue is that the standard boot loader does not support watchdog timer (WDT) reset.
** What does this mean?
** The WDT support is a backup for the case something goes horribly wrong in the sketch code below.
** It "resets" the arduino. But if the default boot loader is what is in FLASH on the Arduino
** Pro Mini (and it WILL BE unless you have used a programmer to change it) then the WDT makes
** any such problem worse. You will not get control of the Power Meter again without removing all power.
** That includes removing the battery backup! You'll have to open the enclosure and pull one of the
** AA cells. The author used a programmer ("Arduino as ISP") to replace the bootloader with the
** optiboot loader. That requires changes to boards.txt in the Arduino IDE and, of course,
** access to an Arduino as ISP programmer. */

typedef uint16_t AcquiredVolts_t; // maxes at ADC max (1024) * VOLTS_LOW_MULTIPLIER 
typedef uint32_t DisplayPower_t; // In units of  1/128  Watt (e.g. value 128 is 1 watt )
namespace {
    // pin assignments on the PCB
    const int PIN_TXD = 1; // directly control TX pin in sleep mode
    const int PIN_RXD = 0; // ditto
    const int couplerPowerDetectPinIn = 2;
    const int initiateCalibratePinIn = 3;
    const int ALOtripSwitchPinIn = 4;
    const int PowerForwReflSwitchPinIn = 5;
    const int SwrMeterPinOut = 10;
    const int RfMeterPinOut = 11;
    const int PanelLampsPinOut = 9;
    const int Tlc59108PowerEnablePinOut = 8;

    const int ReversePwrAnalogUndividedPinIn = A1;
    const int ForwardPwrAnalogUndividedPinIn = A2;
    const int ReversePwrAnalogLowPinIn = A7;
    const int ForwardPwrAnalogLowPinIn = A0;
    const int HoldTimePotAnalogPinIn = A3;
    const int SP3TinPinIn1 = 7;
    const int SP3TinPinIn2 = 6;

    const int MAXED_ADC = 1022; // this is the highest number we'll believe
    const int SWR_SCALE_PWR = 7;
    const uint16_t SWR_SCALE = 1 << SWR_SCALE_PWR;  // 128
    const long INFINITE_SWR = 100;

    // This is the "display" unit for power: watt times 128
    const int PWR_SCALE_PWR = 7;
    const uint16_t PWR_SCALE = 1 << PWR_SCALE_PWR;  // 128

    const int PWM_MAX_PWR = 8;
    const int PWM_MAX_PLUS1 = 1 << PWM_MAX_PWR;

    const unsigned PWR_BREAKTOHIGH_POINT = 250u * PWR_SCALE;
    const unsigned PWR_BREAKTOLOWLOW_POINT = 20u * PWR_SCALE;

    // RC input is 100K-33nF = RC=3.3msec or 48KHz
    const long TimerLoopIntervalMicroSec = 1500; // sample frequency is 1/1500 usec = 660Hz sampling
    const unsigned MeterUpdateIntervalMsec = 125; // 8Hz
    const unsigned CommUpdateIntervalMsec = 100; // COM port message throttle
    const unsigned long HoldPwrLampsOnMsec = 500; 
    const unsigned HoldHighLedMsec = 400;
    const unsigned long LockoutLengthMsec = 5000;

    const unsigned long FrontPanelLampsOnMsec = 90000; // stay on 90 seconds per RFM documentation
    // DEBUG const unsigned long FrontPanelLampsOnMsec = 10000; 

    const unsigned long SERIAL_BAUD = 38400;

    /* multiply the LOW and UNDIVIDED ADC readings by these two in order to put them in AcquiredVolts_t
    ** The compile time computation here enables run time calculations to scale properly in 32 bit integers (or 64, when coded that way)
    ** and without floating point arithmetic.
    ** 
    ** MULTIPLIER's are chosen so a) the results are in same units and b) an ADC count of 1023 (its max) times the multiplier fits in uint16_t
    ** NominalCouplerResistance is such that dividing into (AcquiredVolts_t * AcquiredVolts_t) gives DisplayPower_t
    ** The  couplers have Schottkey barrier diodes. */
    const double SchottkeyBarrierVolts = .2; 
#ifdef OEM_COUPLER
    // The RFM-003 (3000W) works with these parameters
    const double COUPLER_NOMINAL_WATTS = 1500; // as measured...
    const double COUPLER_NOMINAL_VOLTS = 26; // ...ditto
    const uint16_t VOLTS_LOW_MULTIPLIER = 23;
    const uint16_t VOLTS_UNDIVIDED_MULTIPLER = 3;
#endif
#ifdef W5XD_COUPLER
    /* Inspired by the 2008 Radio Amateur's Handbook, chapter 19, High Power Coupler.
    ** The parameters below should work for that coupler as well as the one documented in this repo.
    ** The differences between the two are (a) the transformers are wound on red powdered iron cores,
    ** but three of them together, and the next diameter bigger. 
    ** b) mechanically, this one is built into a smaller box.
    ** The 40:1 turns ratio is maintained, which is the primary contributer to these parameters */
    const double COUPLER_NOMINAL_WATTS = 100; // as measured...
    const double COUPLER_NOMINAL_VOLTS = 2.2; // as measured PLUS schotkey assumed loss
    const uint16_t VOLTS_LOW_MULTIPLIER = 16; // The "low" is 220K / (100K + 220K) voltage divider = 5/16
    const uint16_t VOLTS_UNDIVIDED_MULTIPLER = 5;
#endif
    const double ADC_BASE_VOLTS = 5;
    const int ADC_RESOLUTION = 1023;
    const int WATTS_TO_DISPLAY_T = 128;
    const double A_T_to_VOLTS = ADC_BASE_VOLTS / ( ADC_RESOLUTION * VOLTS_UNDIVIDED_MULTIPLER * COUPLER_NOMINAL_VOLTS);
    const uint32_t NominalCouplerResistance32 = static_cast<uint32_t>(0.5 + 1.0 / (A_T_to_VOLTS * A_T_to_VOLTS * COUPLER_NOMINAL_WATTS * WATTS_TO_DISPLAY_T));
    static_assert(NominalCouplerResistance32 > 1 && NominalCouplerResistance32 < 64000, "NominalCouplerResistance32");
    const uint16_t NominalCouplerResistance = static_cast<uint16_t>(NominalCouplerResistance32);
    const uint32_t NominalCouplerResistanceMultiplier = 0x20000u; // power of two such that divide is optimized as a shift
    const uint32_t NominalCouplerResistanceRecip32 =
        NominalCouplerResistanceMultiplier / NominalCouplerResistance;
    static_assert(NominalCouplerResistanceRecip32 < 0x10000, "NominalCouplerResistanceRecip32");
    const uint16_t NominalCouplerResistanceRecip = static_cast<uint16_t>(NominalCouplerResistanceRecip32);

    const AcquiredVolts_t SchottkeyBarrier =
        static_cast<AcquiredVolts_t>(VOLTS_UNDIVIDED_MULTIPLER * SchottkeyBarrierVolts * ADC_RESOLUTION / 5.0); // multiply into AcquiredVolts_t

    PowerMeterLeds leds(Tlc59108PowerEnablePinOut);
    int8_t AverageSwitchPinIn = SP3TinPinIn1;
    int8_t PeakSwitchPinIn = SP3TinPinIn2;
    uint16_t PowerMinToDisplay = 10; // in DisplayPower_t units, 1/128 W
    unsigned AdcMinNonzero = 4;

    enum EEPROM_ASSIGNMENTS {
        EEPROM_SWR_LOCK, EEPROM_PWR_LOCK, EEPROM_FWD_CALIBRATION, EEPROM_REFL_CALIBRATION, EEPROM_POT_MAX,
        EEPROM_POT_REVERSE = EEPROM_POT_MAX + 2,
        EEPROM_IREF, EEPROM_BRIGHTNESS = EEPROM_IREF+2,
        EEPROM_SP3T_REVERSE, EEPROM_MINPWR,
        EEPROM_ADCMIN = EEPROM_MINPWR + 2,
        EEPROM_USED = EEPROM_ADCMIN + 2
    };
    uint8_t SwrToMeter(uint16_t swrCoded);
    void PwrToMeter(uint16_t toDisplay); // units of PWR_SCALE
}

namespace movingAverage{
        void clear();
}

namespace calibrate {
        void SetCalibrationConstantsFromEEPROM();
        void doCalibrateSetup();
        /*
         * HOW TO CALIBRATE
         *
         * There are two different setups that are written to EEPROM.
         * A) Foward power and Relected power are separately acquired and multiplied by their
         * respective calibration settings from EEPROM [2] and [3]
         * B) ALO has two settings--and SWR lockout value and a reflected power lockout value.
         *
         * Both calibrations are accessed by setting the meter into calibration mode.
         * To set power calibrations (A), start with the front switch in PEAK&HOLD
         * To set ALO settings (B), start with the front switch in AVERAGE.
         *
         * On the back panel, momentarily press the added pushbutton in the ALO SENSE hole.
         *
         * ********* power calibration ***********
         *
         * The (A) calibration mode initially moves the RF Power meter to
         * the current power calibration setting for FORWARD or REFLECTED (depending
         * on the back panel POWER switch.) The 100W value is nominal. Changing
         * the calibration to the 50W setting reduces the meter output by roughly 20%,
         * while the 150W setting increases output by roughly 20%.
         * Attempts to set a calibration outside that range are ignored.
         *
         * To cancel the calibration mode, wait for it to time out (and the LOCK LED to
         * extinguish.) The EEPROM settings are not updated.
         *
         * To change the power calibration, move the front panel switch to AVERAGE.
         * The meter responds by displaying the RF Power meter according to the HOLD
         * pot. Move the pot to adjust the setting, but nothing is saved in EEPROM yet.
         * Change the back panel POWER switch between FORWARD/REFLECTED and, in
         * each switch position, move the HOLD pot to change the corresponding
         * calibration. When you have them like you want, change the front panel
         * switch to PEAK and the settings are saved to EEPROM and the calibration mode
         * is cancelled.
         *
         * Summary:
         * AVERAGE: the HOLD pot adjusts the value
         * PEAK&HOLD: the current EEPROM setting is displayed
         * PEAK: write the current settings to EEPROM and end calibration procedure.
         *
         *
         * ********* ALO calibration *****************
         *
         * The (B) calibration mode turns on the LOCK LED (like A above) and also
         * the SENSE LED during the duration of the calibration procedure.
         * The SWR lockout value is displayed on the SWR meter, and the Reflected power
         * lockout value is displayed on the RF Power meter.
         *
         * Change the back panel ALO switch between SWR/REV to display (on PEAK&HOLD)
         * or change (on AVERAGE) using the HOLD pot.
         * Cancel by waiting for the timeout.
         * Commit the changes by moving the front switch to PEAK.
         *
         * Summary:
         * AVERAGE: the HOLD pot adjusts the value
         * PEAK&HOLD: the current EEPROM setting is displayed
         * PEAK: write the current settings to EEPROM and end calibration procedure.
         */
}

namespace Alo {
        void CheckAloPwr();
        void CheckAloSwr(uint8_t);
        void doAloSetup();
}

namespace sleep {
    void SleepNow();

    void pullUpPins(bool on)
    {
        if (on)
        {
            pinMode(ALOtripSwitchPinIn, INPUT_PULLUP);
            pinMode(PowerForwReflSwitchPinIn, INPUT_PULLUP);
            pinMode(SP3TinPinIn2, INPUT_PULLUP);
            pinMode(SP3TinPinIn1, INPUT_PULLUP);
        }
        else
        {   // Pins that are being pulled down now, switch to INPUT mode
            if (digitalRead(ALOtripSwitchPinIn) == LOW)
                pinMode(ALOtripSwitchPinIn, INPUT);
            if (digitalRead(PowerForwReflSwitchPinIn) == LOW)
                pinMode(PowerForwReflSwitchPinIn, INPUT);
            if (digitalRead(SP3TinPinIn2) == LOW)
                pinMode(SP3TinPinIn2, INPUT);
            if (digitalRead(SP3TinPinIn1) == LOW)
                pinMode(SP3TinPinIn1, INPUT);
            /* if any of the above pins is touched by the human while in sleep mode,
            ** then current consumption will go up until the CPU is waked up, which only
            ** happens via external interrupts on D2 (RF detected) or D3 (back panel CALI button) */
        }
    }
}

namespace {
    void sample();
    uint8_t DisplaySwr();
    bool FrontPanelLamps();
    enum SetupMode_t { METER_NORMAL, ALO_SETUP, CALIBRATE_SETUP };

    // Voltages are in acquisition units.
    // Maximum possible is VOLTS_LOW_MULTIPLIER * ADC max, which is 
    // (less than) 2**6 times 2**10, so it fits in 16 bits, unsigned
    //
    DisplayPower_t getPeakPwr();
    DisplayPower_t getPeakHoldPwr();
    DisplayPower_t getAveragePwr();
    void DisplayPwr(DisplayPower_t);

    unsigned long previousMicrosec;
    unsigned long SwrUpdateTime;
    unsigned long CommUpdateTime;
    unsigned long coupler7dot5LastHeardMillis = 0;
    unsigned HoldTimePotMsec;
    unsigned long HoldPeakRecordedAtMillis;
    bool BackPanelPwrSwitchFwd;
    bool BackPanelAloSwitchSwr;
    unsigned long LockoutStartedAtMillis;

    enum SetupMode_t MeterMode(METER_NORMAL);

    // Alo/calibrate Setup Mode detect
    const unsigned AloSetupModeTimesOut = 30000;
    unsigned long EnteredAloSetupModeTime;

    AcquiredVolts_t fwdCalibration = 0x8000; // this fixed point 1.0 multiplier
    AcquiredVolts_t revCalibration = 0x8000; // ditto

    uint32_t calibrateScaleFwd(uint32_t v)
    {
        v *= revCalibration;
        v /= 0x8000u;
        return v;
    }

    AcquiredVolts_t calibrateFwd(AcquiredVolts_t v)
    {  return (AcquiredVolts_t)calibrateScaleFwd(v);   }

    DisplayPower_t calibrateFwdPower(DisplayPower_t w)
    {
        uint64_t t = w;
        t *= fwdCalibration;
        t *= fwdCalibration;
        t /= 0x8000u;
        t /= 0x8000u;
        return (DisplayPower_t)t;
    }

    uint32_t calibrateScaleRev(uint32_t v)
    {
        v *= revCalibration;
        v /= 0x8000u;
        return v;
    }

    AcquiredVolts_t calibrateRev(AcquiredVolts_t v)
    {  return (AcquiredVolts_t)calibrateScaleRev(v);  }

    DisplayPower_t calibrateRevPower(DisplayPower_t w)
    {
        uint64_t t = w;
        t *= revCalibration;
        t *= revCalibration;
        t /= 0x8000u;
        t /= 0x8000u;
        return (DisplayPower_t)t;
    }

    uint16_t readHoldPot();
    void AdcTest();
}

namespace Comm {
    enum OutputToSerial_t { NO_OUTPUT_TO_SERIAL, AVG_OUTPUT_TO_SERIAL, PEAK_OUTPUT_TO_SERIAL };
        OutputToSerial_t OutputToSerial;
        unsigned long OutputStartedMsec;
        bool forever = false;
        void CommUpdateForwardAndReverse();
        const unsigned long OUTPUT_TIMEOUT_MSEC = 10000;
}

static void get_mcusr();

void setup() {
#ifdef SUPPORT_WDT
    MCUSR = 0;
    wdt_disable();
#endif
    pinMode(couplerPowerDetectPinIn, INPUT);
    pinMode(PanelLampsPinOut, OUTPUT);

    pinMode(initiateCalibratePinIn, INPUT_PULLUP); // unchanged during sleep

    pinMode(ReversePwrAnalogUndividedPinIn, INPUT);
    pinMode(ForwardPwrAnalogUndividedPinIn, INPUT);
    pinMode(ReversePwrAnalogLowPinIn, INPUT);
    pinMode(ForwardPwrAnalogLowPinIn, INPUT);
    pinMode(HoldTimePotAnalogPinIn, INPUT);

    sleep::pullUpPins(true);

    analogReference(DEFAULT); // stay on default reference, which is Vcc, which is 5V
    /* The PCB has two power supplies, battery and linear. This code assumes their Vcc result is 
    ** close enough that no calibration shift is required between them.
    ** Measuremments with the PCB documented here show them within a few percent of each other. */

    movingAverage::clear();

    calibrate::SetCalibrationConstantsFromEEPROM();
    digitalWrite(PanelLampsPinOut, HIGH); // turn on front panel lights on boot

    Wire.begin();

    leds.begin();

    auto iref = EEPROM.read((int)EEPROM_IREF);
    leds.LeftDevice().SetCurrent(iref);
    leds.RightDevice().SetCurrent(iref);

    if (EEPROM.read((int)EEPROM_SP3T_REVERSE) != 0)
    {
        AverageSwitchPinIn = SP3TinPinIn1;
        PeakSwitchPinIn = SP3TinPinIn2;
    }
    else
    {
        AverageSwitchPinIn = SP3TinPinIn2;
        PeakSwitchPinIn = SP3TinPinIn1;
    }


    // present some info about our calibration and version number
    Serial.begin(SERIAL_BAUD);
    Serial.println(F("W5XD PowerMeter 2.2"));
    Serial.print(F("Forward CAL = 0x"));
    Serial.println(fwdCalibration, HEX);
    Serial.print(F("Reflected CAL = 0x"));
    Serial.println(revCalibration, HEX);
    Serial.print(F("Pot reverse = "));
    Serial.println(EEPROM.read((int)EEPROM_POT_REVERSE) != 0 ? "0" : "1");
    Serial.print(F("Pot max="));
    uint16_t pmax;
    EEPROM.get((int)EEPROM_POT_MAX, pmax);
    Serial.println(pmax);
    Serial.print(F("LED IREF=0x"));
    Serial.println((int)EEPROM.read((int)EEPROM_IREF), HEX);

    uint8_t bright = EEPROM.read((int)EEPROM_BRIGHTNESS);
    if (bright != 0)
        leds.SetBrightness(bright);

    Serial.print(F("LED brightness = "));
    Serial.println((int)leds.GetBrightness());

    Serial.print(F("Coupler resistance cal: "));
    Serial.println(NominalCouplerResistance);
    Serial.print(F("SP3TUPDOWN = "));
    Serial.println(EEPROM.read((int)EEPROM_SP3T_REVERSE) != 0 ? "0" : "1");
    uint16_t pmin;
    EEPROM.get((int)EEPROM_MINPWR, pmin);
    if (pmin != 0xFFFF)
        PowerMinToDisplay = pmin;
    Serial.print(F("PMIN="));
    Serial.println(PowerMinToDisplay);
    EEPROM.get((int)EEPROM_ADCMIN, pmin);
    if (pmin != 0xFFFF)
        AdcMinNonzero = pmin;
    Serial.print(F("ADCMIN="));
    Serial.println(AdcMinNonzero);

#ifdef SUPPORT_WDT
    wdt_enable(WDTO_1S);
    /* FYI: any call to delay() for more than 1S will trigger the watchdog 
    ** unless wdt_disable() is called first. And, of course, wdt_enable */
#endif
}

namespace cmd {
    enum COMMAND_ENUM { P_ON, P_OFF, P_PEAK, P_FOREVER, POTREVERSE, POTMAX, SP3TUPDOWN, PMIN, POT, IREF,LED, METERS, ADCX, BRI, DUMP, RSCALI, ADCMIN, NUM_COMMANDS};
    const int MAX_COMMAND_LEN = 12;
    const char c0[] PROGMEM = "P ON";
    const char c1[] PROGMEM = "P OFF";
    const char c2[] PROGMEM = "P PEAK";
    const char c3[] PROGMEM = "P FOREVER";
    const char c4[] PROGMEM = "POTREVERSE=";
    const char c5[] PROGMEM = "POTMAX=";
    const char c6[] PROGMEM = "SP3TUPDOWN=";
    const char c7[] PROGMEM = "PMIN=";
    const char c8[] PROGMEM = "POT";
    const char c9[] PROGMEM = "IREF=";
    const char c10[] PROGMEM = "LED";
    const char c11[] PROGMEM = "METERS";
    const char c12[] PROGMEM = "ADC";
    const char c13[] PROGMEM = "BRI=";
    const char c14[] PROGMEM = "DUMP";
    const char c15[] PROGMEM = "RSCALI";
    const char c16[] PROGMEM = "ADCMIN=";
    const char *const tbl[NUM_COMMANDS] PROGMEM = {c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15, c16};
    static_assert(NUM_COMMANDS == 17, "command table mismatch");

    int strncmp(const char *b, COMMAND_ENUM e, uint8_t len)
    {
        char cmd[MAX_COMMAND_LEN];
        strcpy_P(cmd, (char*)pgm_read_ptr(&(tbl[static_cast<int>(e)])));
        return ::strncmp(b, cmd, len);
    }
    int strcmp(const char *b, COMMAND_ENUM e)
    {
        return strncmp(b, e, 1 + ::strlen(b));
    }
}

void loop()
{
#ifdef SUPPORT_WDT
    wdt_reset();
#endif
    previousMicrosec = micros();
    unsigned long now = millis();
    leds.loop(now);
    // throttle to one loop every TimerLoopIntervalMicroSec
    while (Serial.available() > 0)
    {
        static unsigned char numInBuf = 0;
        static char buf[16];
        auto inChar = Serial.read();
        if (islower(inChar))
            inChar = toupper(inChar);
        buf[numInBuf] = inChar;
        if (inChar == '\r' || inChar == '\n')
        {
            buf[numInBuf] = 0;
            if (cmd::strcmp(buf, cmd::P_ON) == 0)
            {   // send serial port AVG power output 
                Comm::OutputToSerial = Comm::AVG_OUTPUT_TO_SERIAL;
                Comm::OutputStartedMsec = now;
            }
            else if (cmd::strcmp(buf, cmd::P_OFF) == 0) // don't send serial port power
                Comm::OutputToSerial = Comm::NO_OUTPUT_TO_SERIAL;
            else if (cmd::strcmp(buf, cmd::P_PEAK) == 0)
            {   // send serial port PEAK power output
                Comm::OutputToSerial = Comm::PEAK_OUTPUT_TO_SERIAL;
                Comm::OutputStartedMsec = now;
            }
            else if (cmd::strcmp(buf, cmd::P_FOREVER) == 0)
            {   // normal operation is that serial port output must be requested every 10 seconds. Keep it on forever
                Comm::forever = !Comm::forever;
                Serial.print(F("P Forever ="));
                Serial.println(Comm::forever ? "1" : "0");
            }
            else if (cmd::strncmp(buf, cmd::POTREVERSE, 11) == 0)
            {   /* If the hold pot is wired backwards, fix in software*/
                EEPROM.write((int)EEPROM_POT_REVERSE, static_cast<uint8_t>(buf[11] == '1' ? 0 : 1));
            }
            else if (cmd::strncmp(buf, cmd::POTMAX, 7) == 0)
            {   /* If the hold pot won't go full scale, EEPROM setting of 0-1023 to specify*/
                uint16_t pmax = atoi(buf+7);
                EEPROM.put((int)EEPROM_POT_MAX, pmax);
            }
            else if (cmd::strncmp(buf, cmd::SP3TUPDOWN, 11) == 0)
            {   /* The front panel SP3T switch can be installed upside down. Fix in software here*/
                bool reverse = buf[11] == '1';
                EEPROM.write((int)EEPROM_SP3T_REVERSE, static_cast<uint8_t>(reverse ? 0 : 1));
                if (reverse)
                {
                    AverageSwitchPinIn = SP3TinPinIn2;
                    PeakSwitchPinIn = SP3TinPinIn1;
                }
                else
                {
                    AverageSwitchPinIn = SP3TinPinIn1;
                    PeakSwitchPinIn = SP3TinPinIn2;
                }
            }
            else if (cmd::strncmp(buf, cmd::PMIN, 5) == 0)
            {
                /* Units of 1/128W, set the minium power to keep the display turned on */
                PowerMinToDisplay = atoi(buf+5);
                EEPROM.put((int)EEPROM_MINPWR, PowerMinToDisplay);
            }
            else if (cmd::strcmp(buf, cmd::POT) == 0)
            {   /* diagnostic hold pot readout*/
                auto potRaw = analogRead(HoldTimePotAnalogPinIn);
                auto potRead = readHoldPot();
                Serial.print(F("Pot raw= "));
                Serial.print(potRaw);
                Serial.print(F(" read= "));
                Serial.println(potRead);
            }
            else if (cmd::strncmp(buf, cmd::IREF, 5) == 0)
            {   /* Set the LED driver chip IREF. (reference current.) 0 is dimmest, 255 is brightest */
                uint8_t iref = atoi(buf + 5);
                EEPROM.put((int)EEPROM_IREF, iref);
                leds.LeftDevice().SetCurrent(iref);
                leds.RightDevice().SetCurrent(iref);
            }
            else if (cmd::strcmp(buf, cmd::LED) == 0)
            {
                /* Turn each of the LEDS on/off*/
                Serial.println("LED test");
#ifdef SUPPORT_WDT
                wdt_disable();
#endif
                leds.test();
#ifdef SUPPORT_WDT
                wdt_enable(WDTO_1S);
#endif
                Serial.println("LED test end");
            }
            else if (cmd::strcmp(buf, cmd::METERS) == 0)
            {   /* Verify the meter labeling. Move meter movements to 
                ** each tick on their printed scales. */
                static const int NUM_TESTS = 6;
                static const uint16_t PowersToDisplay[NUM_TESTS] = 
                {
                    PWR_SCALE * 5,
                    PWR_SCALE * 20,
                    PWR_SCALE * 50,
                    PWR_SCALE * 100,
                    PWR_SCALE * 200,
                    PWR_SCALE * 300
                };
                static const uint16_t SwrsToDisplay[NUM_TESTS] = 
                {
                    static_cast<uint16_t>(1.5f * SWR_SCALE),
                    static_cast<uint16_t>(2.f * SWR_SCALE),
                    static_cast<uint16_t>(3.f * SWR_SCALE),
                    static_cast<uint16_t>(4.f * SWR_SCALE),
                    static_cast<uint16_t>(6.f * SWR_SCALE),
                    static_cast<uint16_t>(10.f * SWR_SCALE),
                };
#ifdef SUPPORT_WDT
                wdt_disable();
#endif
                for (uint8_t i = 0; i < NUM_TESTS; i += 1)
                {
                    for (uint8_t j = 0; j < 10; j++)
                    {
                        PwrToMeter(PowersToDisplay[i]);
                        SwrToMeter(SwrsToDisplay[i]);
                        delay(10);
                    }
                    delay(1000);
                }
#ifdef SUPPORT_WDT
                wdt_enable(WDTO_1S);
#endif
            }
            else if (cmd::strcmp(buf, cmd::ADCX) == 0)
                AdcTest();
            else if (cmd::strncmp(buf, cmd::ADCMIN, 7) == 0)
            {   /* The ADC might not go all the way to zero when there is no signal. Set the lowest nonzero value. 0-255*/
                AdcMinNonzero = atoi(buf + 7);
                EEPROM.put((int)EEPROM_ADCMIN, AdcMinNonzero);
            }
            else if (cmd::strncmp(buf, cmd::BRI, 4) == 0)
            {   /* 0-255 sets the duty cycle on the LEDS. 255 brightest*/
                uint8_t v = atoi(buf + 4);
                Serial.print(F("BRI="));
                Serial.println(v);
                if (v != 0)
                {
                    leds.SetBrightness(v);
                    EEPROM.write((int)EEPROM_BRIGHTNESS, v);
                }
            }
            else if (cmd::strcmp(buf, cmd::DUMP) == 0)
            {
                leds.LeftDevice().dump();
                leds.RightDevice().dump();
            }
            else if (cmd::strcmp(buf, cmd::RSCALI) == 0)
            {   /* reset FWD/REFL calibration values to default*/
                EEPROM.write((int)EEPROM_FWD_CALIBRATION, 0xff);
                EEPROM.write((int)EEPROM_REFL_CALIBRATION, 0xff);
            }
#ifdef SUPPORT_WDT
            // force the watch dog timer to trigger
            else if (strcmp(buf, "WDTT") == 0)
                while(true); // test watchdog timer
	    /* the correct result is that the sketch reboots and you see the 
            ** the printouts from the setup() routine above after 1 second */
#endif
            numInBuf = 0;
        }
        else numInBuf += 1;
        if (numInBuf >= sizeof(buf) - 1)
            numInBuf = 0;
    }
    sample(); // read FWD/REFL ADCs

    BackPanelPwrSwitchFwd = digitalRead(PowerForwReflSwitchPinIn) == HIGH;
    BackPanelAloSwitchSwr = digitalRead(ALOtripSwitchPinIn) == HIGH;

    // CHECK FOR ENTER CALIBRATE MODE
    if (MeterMode == METER_NORMAL &&
        digitalRead(PeakSwitchPinIn) != LOW &&
        digitalRead(initiateCalibratePinIn) == LOW)
    {
        MeterMode = (digitalRead(AverageSwitchPinIn) == LOW) ?
            ALO_SETUP : CALIBRATE_SETUP;
        digitalWrite(PanelLampsPinOut, HIGH);
        coupler7dot5LastHeardMillis = now;
        EnteredAloSetupModeTime = now;
    }

    // dispatch per MeterMode
    if (MeterMode == ALO_SETUP)
    {
        Alo::doAloSetup();
        if (MeterMode != ALO_SETUP)
        {
            leds.SetSenseLed(false);
            leds.SetAloLock(false);
        }
        return;
    }
    else if (MeterMode == CALIBRATE_SETUP)
    {
        calibrate::doCalibrateSetup();
        if (MeterMode != CALIBRATE_SETUP)
            leds.SetAloLock(false);
        return;
    }

    if (digitalRead(couplerPowerDetectPinIn) == LOW)
    {   // RF detect activated
        coupler7dot5LastHeardMillis = now;
        digitalWrite(PanelLampsPinOut, HIGH);
    }

    if (leds.GetAloLock() &&
        now - LockoutStartedAtMillis > LockoutLengthMsec)
    {
        leds.SetAloLock(false);
    }

    if (!Comm::forever && (now - Comm::OutputStartedMsec > Comm::OUTPUT_TIMEOUT_MSEC))
        Comm::OutputToSerial = Comm::NO_OUTPUT_TO_SERIAL;

    if (now - CommUpdateTime >= CommUpdateIntervalMsec)
    {
        CommUpdateTime = now;
        Comm::CommUpdateForwardAndReverse();
    }

    // Update the displays less frequently than loop() can excute
    if (now - SwrUpdateTime >= MeterUpdateIntervalMsec)
    {
        SwrUpdateTime = now;
        uint8_t swr = DisplaySwr();
        DisplayPower_t pwr;
        if (digitalRead(PeakSwitchPinIn) == LOW)
            pwr = getPeakPwr();
        else if (digitalRead(AverageSwitchPinIn) == LOW)
            pwr = getAveragePwr();
        else
            pwr = getPeakHoldPwr();
        DisplayPwr(pwr);
        if (BackPanelAloSwitchSwr)
            Alo::CheckAloSwr(swr);
        else
            Alo::CheckAloPwr();
        if (!FrontPanelLamps())
        {
            leds.sleep();
            sleep::SleepNow();
            leds.wake();
        }
    }

    // 
    unsigned long nowusec = micros();
    long diff = nowusec - previousMicrosec;
    diff -= TimerLoopIntervalMicroSec;
    if ((diff < 0) && (diff >= -TimerLoopIntervalMicroSec))
        delayMicroseconds((unsigned int)-diff);
}

namespace movingAverage {
    // A dot-length at 13wpm is 92msec
    // run moving average over power of 2 samples for cheap divide
    // 256 sample moving average
    // * 1.5 msec sample interval = 384msec history length
    const int PWR_TO_AVERAGE = 8; // takes up 512 bytes out of 2048 total on UNO
    const int NUM_TO_AVERAGE = 1 << PWR_TO_AVERAGE;
    int curIndex;

    // The "acquisition" units for power are what we get from the ADC, times
    // what we used to acquire it (VOLTS_LOW_MULTIPLIER or VOLTS_UNDIVIDED_MULTIPLIER)
    // The ADC is 10 bits, so multiplying by those still keeps us within 16 bit unsigned

    AcquiredVolts_t fwdHistory[NUM_TO_AVERAGE]; // units are ADC converter units
    AcquiredVolts_t revHistory[NUM_TO_AVERAGE];
    uint64_t fwdTotal;
    uint64_t revTotal;

    class AvgSinceLastCheck
    {
    public:
        AvgSinceLastCheck(int start = 0) : lastIndex(start) {}

        // expect to be called at meter update frequency: 8Hz
        // BEWARE. f/r are calibrated TO EACH OTHER ONLY
        void getCalibratedSums(uint32_t& f, uint32_t& r)
        {
            f = 0; r = 0;
            // backwards from newest sample
            // stop at sample we used last time called
            unsigned count = 0;
            for (int i = curIndex; i != lastIndex; )
            {
                f += fwdHistory[i];
                r += revHistory[i];
                count += 1;
                if (--i < 0)
                    i = NUM_TO_AVERAGE - 1;
            }
            // optimize the divide by count to nearby power of two
            // Only the ratio of f and r will be used to compute SWR
            for (;;)
            {
                count >>= 1;
                if (count == 0)
                    break;
                f >>= 1;
                r >>= 1;
            }
            f = calibrateScaleFwd(f);
            r = calibrateScaleRev(r);
            lastIndex = curIndex;
        }

    private:
        int lastIndex;
    };

    void clear()
    {
        for (unsigned i = 0; i < NUM_TO_AVERAGE; i++)
        {
            fwdHistory[i] = 0;
            revHistory[i] = 0;
        }
        fwdTotal = 0;
        revTotal = 0;
    }

    void apply(AcquiredVolts_t f, AcquiredVolts_t r)
    {
        fwdTotal -= (long)fwdHistory[curIndex] * fwdHistory[curIndex];
        revTotal -= (long)revHistory[curIndex] * revHistory[curIndex];
        fwdTotal += (long)f * f;
        revTotal += (long)r * r;
        fwdHistory[curIndex] = f;
        revHistory[curIndex++] = r;
        if (curIndex >= NUM_TO_AVERAGE)
            curIndex = 0;
    }

    // UNCALIBRATED
    DisplayPower_t fwdPwr()
    {
        uint64_t f = fwdTotal;
        f += 1 << (PWR_TO_AVERAGE - 1);
        f >>= PWR_TO_AVERAGE;
        return (DisplayPower_t)f;
    }

    // UNCALIBRATED
    DisplayPower_t revPwr()
    {
        uint64_t r = revTotal;
        r += 1 << (PWR_TO_AVERAGE - 1);
        r >>= PWR_TO_AVERAGE;
        return (DisplayPower_t)r;
    }

    // UNCALIBRATED
    void getPeaks(AcquiredVolts_t& f, AcquiredVolts_t& r)
    {
        f = r = 0;
        for (int i = 0; i < NUM_TO_AVERAGE; i++)
        {
            if (fwdHistory[i] > f)
                f = fwdHistory[i];
            if (revHistory[i] > r)
                r = revHistory[i];
        }
    }
}

namespace SwrMeter {
        const int PWR_ENTRIES = 8;
        const int NUM_PWM = 1 << PWR_ENTRIES;
        // value in table is SWR * SWR_SCALE
        // index into table is the PWM value to put on output pin to make meter read that SWR
#ifdef OEM_METER_SCALES
        const uint16_t PwmToSwr[NUM_PWM] PROGMEM = // PROGMEM puts the array in program memory
        {
            // this table was constructing by observing the meter position on the OEM meter
            // for the PWM values from 0 to 255
                //  (f+r)/(f-r)*128  -> SWR. Index into table, 0-255 is PWM output
                128, // 1
                129, // 1.00685
                130, // 1.01379
                131, // 1.02083
                132, // 1.02797
                133, // 1.03521
                133, // 1.04255...two consecutive identical entries is useless....
                134, // 1.05
                135, // 1.05714
                136, // 1.06438
                137, // 1.07172
                138, // 1.07917
                139, // 1.08671
                140, // 1.09437
                141, // 1.10213
                142, // 1.11
                143, // 1.11829
                144, // 1.12671
                145, // 1.13525
                146, // 1.14393
                148, // 1.15274
                149, // 1.16169
                150, // 1.17077
                151, // 1.18
                152, // 1.18832
                153, // 1.19675
                154, // 1.20531
                155, // 1.21399
                157, // 1.2228
                158, // 1.23173
                159, // 1.2408
                160, // 1.25
                161, // 1.26058
                163, // 1.27135
                164, // 1.2823
                166, // 1.29344
                167, // 1.30477
                168, // 1.31631
                170, // 1.32805
                172, // 1.34
                173, // 1.35173
                175, // 1.36367
                176, // 1.37583
                178, // 1.3882
                179, // 1.4008
                181, // 1.41363
                183, // 1.42669
                184, // 1.44
                186, // 1.45289
                188, // 1.46601
                189, // 1.47937
                191, // 1.49298
                193, // 1.50684
                195, // 1.52095
                197, // 1.53534
                198, // 1.55
                200, // 1.56071
                201, // 1.57156
                203, // 1.58257
                204, // 1.59373
                205, // 1.60505
                207, // 1.61653
                208, // 1.62818
                210, // 1.64
                212, // 1.65299
                213, // 1.66618
                215, // 1.67959
                217, // 1.69322
                219, // 1.70706
                220, // 1.72114
                222, // 1.73545
                224, // 1.75
                226, // 1.76526
                228, // 1.78078
                230, // 1.79659
                232, // 1.81267
                234, // 1.82905
                236, // 1.84572
                238, // 1.8627
                241, // 1.88
                242, // 1.89421
                244, // 1.90863
                246, // 1.92327
                248, // 1.93814
                250, // 1.95325
                252, // 1.96859
                254, // 1.98417
                256, // 2
                259, // 2.02086
                261, // 2.04215
                264, // 2.06391
                267, // 2.08612
                270, // 2.10883
                273, // 2.13203
                276, // 2.15575
                279, // 2.18
                281, // 2.19657
                283, // 2.21339
                286, // 2.23047
                288, // 2.24782
                290, // 2.26544
                292, // 2.28334
                295, // 2.30152
                297, // 2.32
                300, // 2.34645
                304, // 2.37352
                307, // 2.40122
                311, // 2.42957
                315, // 2.4586
                319, // 2.48833
                322, // 2.51879
                326, // 2.55
                329, // 2.57339
                332, // 2.59722
                336, // 2.6215
                339, // 2.64623
                342, // 2.67143
                345, // 2.69712
                349, // 2.7233
                352, // 2.75
                356, // 2.77895
                359, // 2.80851
                363, // 2.83871
                367, // 2.86957
                371, // 2.9011
                375, // 2.93333
                380, // 2.96629
                384, // 3
                387, // 3.02474
                390, // 3.04988
                394, // 3.07545
                397, // 3.10145
                400, // 3.12789
                404, // 3.15479
                407, // 3.18216
                411, // 3.21
                417, // 3.25407
                422, // 3.29936
                428, // 3.34593
                434, // 3.39383
                441, // 3.44313
                447, // 3.49388
                454, // 3.54614
                461, // 3.6
                467, // 3.64557
                473, // 3.69231
                479, // 3.74026
                485, // 3.78947
                492, // 3.84
                498, // 3.89189
                505, // 3.94521
                512, // 4
                518, // 4.04598
                524, // 4.09302
                530, // 4.14118
                536, // 4.19048
                543, // 4.24096
                549, // 4.29268
                556, // 4.34568
                563, // 4.4
                572, // 4.467
                581, // 4.53608
                590, // 4.60733
                599, // 4.68085
                609, // 4.75676
                619, // 4.83517
                629, // 4.9162
                640, // 5
                649, // 5.06787
                658, // 5.13761
                667, // 5.2093
                676, // 5.28302
                686, // 5.35885
                696, // 5.43689
                706, // 5.51724
                717, // 5.6
                728, // 5.68889
                740, // 5.78065
                752, // 5.87541
                765, // 5.97333
                778, // 6.07458
                791, // 6.17931
                805, // 6.28772
                819, // 6.4
                834, // 6.51952
                850, // 6.6436
                867, // 6.77249
                884, // 6.90648
                902, // 7.04587
                920, // 7.19101
                940, // 7.34226
                960, // 7.5
                977, // 7.63158
                994, // 7.76786
                1012, // 7.90909
                1031, // 8.05556
                1051, // 8.20755
                1071, // 8.36538
                1092, // 8.52941
                1114, // 8.7
                1138, // 8.89051
                1163, // 9.08955
                1190, // 9.29771
                1218, // 9.51563
                1247, // 9.744
                1278, // 9.98361
                1310, // 10.2353
                1344, // 10.5
                1377, // 10.7586
                1412, // 11.0303
                1448, // 11.3161
                1487, // 11.617
                1528, // 11.9344
                1571, // 12.2697
                1616, // 12.6243
                1664, // 13
                1732, // 13.5342
                1807, // 14.1143
                1888, // 14.7463
                1976, // 15.4375
                2073, // 16.1967
                2180, // 17.0345
                2299, // 17.9636
                2432, // 19
                2542, // 19.8559
                2661, // 20.7925
                2793, // 21.8218
                2939, // 22.9583
                3100, // 24.2198
                3280, // 25.6279
                3483, // 27.2099
                3712, // 29
                3918, // 30.6069
                4147, // 32.4022
                4406, // 34.4214
                4699, // 36.7089
                5033, // 39.322
                5419, // 42.3358
                5869, // 45.8498
                6400, // 50
                6827, // 53.3333
                7314, // 57.1429
                7877, // 61.5385
                8533, // 66.6667
                9309, // 72.7273
                10240, // 80
                11378, // 88.8889
                12800, // 100 <-- last entry to use--series resistor selected for full scale on meter
                14629, // 114.286
                17067, // 133.333
                20480, // 160
                25600, // 200
                34133, // 266.667
                51200, // 400
                51201,
                51202,
                // this table is not complete. Just don't call here for higher than 100SWR
        };
#endif
#ifdef CUSTOM_METER_SCALES
        const uint16_t PwmToSwr[NUM_PWM] PROGMEM = // PROGMEM puts the array in program memory
        {
            // SWR is (f+r)/(f-r) times 128 (so no floating point is involved.)
            // In MeterFacesForm.cs, an SWR analog meter face is drawn with these parameters:
            //             double[] ticks = { 1,  1.5, 2, 3, 4, 6, 10};
            //              MeterFaceSWR(ticks, 11);
            // The tick value of 1 is PWM of zero and the tick value of SWR = 11.0 is PWM at max (250)
            // The meter scale is drawn such that logarithm(1.0) is zero, and logarithm(11.0) is max.
            //A         B                               C                       D
            //0	        1	                            =+B1*128	            =LN(C1)
            //255	    21	                            =+B2*128	            =LN(C2)		
            //0	        =+$D$1+A3/250*($D$2-$D$1)	    =EXP(B3)																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																													
            //=+A3+1	=+$D$1+A4/250*($D$2-$D$1)	    =EXP(B4)																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																													
            //
128	, //	1
129	, //	1.0096377277
130	, //	1.0193683412
132	, //	1.0291927358
133	, //	1.0391118151
134	, //	1.0491264919
136	, //	1.0592376874
137	, //	1.0694463318
138	, //	1.0797533644
140	, //	1.0901597333
141	, //	1.100666396
142	, //	1.111274319
144	, //	1.1219844784
145	, //	1.1327978593
146	, //	1.1437154566
148	, //	1.1547382748
149	, //	1.1658673279
151	, //	1.1771036397
152	, //	1.1884482441
154	, //	1.1999021847
155	, //	1.2114665153
157	, //	1.2231422997
158	, //	1.2349306121
160	, //	1.2468325371
161	, //	1.2588491697
163	, //	1.2709816152
164	, //	1.28323099
166	, //	1.2955984209
167	, //	1.3080850457
169	, //	1.3206920132
171	, //	1.3334204832
172	, //	1.3462716268
174	, //	1.3592466262
176	, //	1.3723466751
177	, //	1.3855729787
179	, //	1.3989267538
181	, //	1.4124092289
183	, //	1.4260216445
184	, //	1.4397652529
186	, //	1.4536413184
188	, //	1.4676511176
190	, //	1.4817959395
191	, //	1.4960770853
193	, //	1.5104958689
195	, //	1.5250536168
197	, //	1.5397516683
199	, //	1.5545913757
201	, //	1.5695741041
203	, //	1.584701232
205	, //	1.599974151
207	, //	1.6153942662
209	, //	1.6309629963
211	, //	1.6466817736
213	, //	1.6625520442
215	, //	1.6785752681
217	, //	1.6947529195
219	, //	1.7110864867
221	, //	1.7275774724
223	, //	1.7442273937
225	, //	1.7610377824
228	, //	1.7780101851
230	, //	1.7951461631
232	, //	1.8124472931
234	, //	1.8299151666
236	, //	1.8475513908
239	, //	1.865357588
241	, //	1.8833353966
243	, //	1.9014864704
246	, //	1.9198124792
248	, //	1.9383151092
250	, //	1.9569960625
253	, //	1.9758570577
255	, //	1.99489983
258	, //	2.0141261314
260	, //	2.0335377307
263	, //	2.0531364137
265	, //	2.0729239834
268	, //	2.0929022604
270	, //	2.1130730825
273	, //	2.1334383055
276	, //	2.153999803
278	, //	2.1747594667
281	, //	2.1957192063
284	, //	2.2168809502
286	, //	2.2382466452
289	, //	2.2598182569
292	, //	2.28159777
295	, //	2.3035871881
298	, //	2.3257885342
301	, //	2.3482038508
303	, //	2.3708352002
306	, //	2.3936846643
309	, //	2.4167543454
312	, //	2.4400463657
315	, //	2.4635628683
318	, //	2.4873060164
321	, //	2.5112779946
325	, //	2.5354810081
328	, //	2.5599172838
331	, //	2.5845890695
334	, //	2.6094986353
337	, //	2.6346482726
340	, //	2.6600402953
344	, //	2.6856770394
347	, //	2.7115608635
350	, //	2.7376941488
354	, //	2.7640792996
357	, //	2.7907187433
361	, //	2.8176149308
364	, //	2.8447703363
368	, //	2.8721874582
371	, //	2.8998688189
375	, //	2.9278169651
378	, //	2.9560344678
382	, //	2.9845239232
386	, //	3.0132879521
389	, //	3.042329201
393	, //	3.0716503415
397	, //	3.1012540711
401	, //	3.1311431135
405	, //	3.1613202183
409	, //	3.1917881618
412	, //	3.2225497471
416	, //	3.2536078041
420	, //	3.2849651903
425	, //	3.3166247904
429	, //	3.3485895171
433	, //	3.3808623111
437	, //	3.4134461415
441	, //	3.446344006
445	, //	3.4795589312
450	, //	3.5130939728
454	, //	3.546952216
458	, //	3.5811367757
463	, //	3.6156507969
467	, //	3.6504974549
472	, //	3.6856799554
476	, //	3.7212015353
481	, //	3.7570654625
486	, //	3.7932750365
490	, //	3.8298335885
495	, //	3.8667444819
500	, //	3.9040111124
505	, //	3.9416369085
509	, //	3.9796253318
514	, //	4.0179798772
519	, //	4.0567040733
524	, //	4.0958014826
529	, //	4.1352757021
534	, //	4.1751303634
540	, //	4.2153691331
545	, //	4.2559957131
550	, //	4.297013841
555	, //	4.3384272904
561	, //	4.3802398714
566	, //	4.4224554306
572	, //	4.465077852
577	, //	4.5081110566
583	, //	4.5515590035
588	, //	4.5954256899
594	, //	4.6397151515
600	, //	4.6844314629
605	, //	4.7295787379
611	, //	4.77516113
617	, //	4.8211828328
623	, //	4.8676480803
629	, //	4.9145611471
635	, //	4.9619263494
641	, //	5.0097480445
647	, //	5.0580306322
654	, //	5.1067785542
660	, //	5.1559962955
666	, //	5.205688384
673	, //	5.2558593912
679	, //	5.306513933
686	, //	5.3576566695
692	, //	5.4092923057
699	, //	5.4614255922
706	, //	5.514061325
713	, //	5.5672043467
719	, //	5.6208595464
726	, //	5.6750318603
733	, //	5.7297262722
740	, //	5.784947814
748	, //	5.8407015659
755	, //	5.8969926574
762	, //	5.953826267
769	, //	6.0112076235
777	, //	6.0691420059
784	, //	6.1276347441
792	, //	6.1866912193
800	, //	6.2463168649
807	, //	6.3065171661
815	, //	6.3672976614
823	, //	6.4286639427
831	, //	6.4906216554
839	, //	6.5531764997
847	, //	6.6163342305
855	, //	6.6801006584
863	, //	6.7444816497
872	, //	6.8094831275
880	, //	6.8751110719
888	, //	6.9413715205
897	, //	7.0082705692
906	, //	7.0758143728
914	, //	7.1440091452
923	, //	7.2128611602
932	, //	7.2823767522
941	, //	7.3525623165
950	, //	7.4234243102
959	, //	7.4949692525
969	, //	7.5672037255
978	, //	7.6401343747
987	, //	7.7137679096
997	, //	7.7881111044
1006	, //	7.8631707988
1016	, //	7.938953898
1026	, //	8.0154673741
1036	, //	8.0927182663
1046	, //	8.1707136815
1056	, //	8.2494607953
1066	, //	8.3289668523
1076	, //	8.4092391671
1087	, //	8.4902851246
1097	, //	8.5721121809
1108	, //	8.6547278642
1118	, //	8.7381397749
1129	, //	8.8223555869
1140	, //	8.9073830479
1151	, //	8.9932299805
1162	, //	9.0799042824
1173	, //	9.1674139277
1185	, //	9.2557669671
1196	, //	9.344971529
1208	, //	9.4350358202
1219	, //	9.5259681266
1231	, //	9.6177768137
1243	, //	9.710470328
1255	, //	9.8040571971
1267	, //	9.898546031
1279	, //	9.9939455225
1292	, //	10.0902644484
1304	, //	10.1875116698
1317	, //	10.2856961335
1329	, //	10.3848268723
1342	, //	10.4849130062
1355	, //	10.585963743
1368	, //	10.6879883793
1381	, //	10.7909963012
1395	, //	10.8949969855
1408	, //	11
1422	, //	11.106015005
1435	, //	11.2130517537
1449	, //	11.3211200935
1463	, //	11.4302299666
1477	, //	11.5403914108
        };
#endif
}

namespace {
    class MeterFilter {
    public:
        MeterFilter() : value(0) {}
        int apply(int s)
        {
            int ret = s + value;
            ret /= 2;
            // move half way from last requested position to new position.
            // except...
            if (ret == value) // sample didn't change filter value? use s
                ret = s;
            value = ret;
            return ret;
        }
    private:
        int value;
    };

    AcquiredVolts_t fwdHires;
    AcquiredVolts_t revHires;

    void sample()
    {
        /* SWR should be calculated with coincident FWD and REV measurements.
         * But we have to digitize them serially. There will always be at least
         * 100uSec of clock skew between the two measurements. FWD will (almost)
         * always be the larger, so read it first, and in the HIGH sensitivity.*/

        // start with the Undivided ADC input
        fwdHires = analogRead(ForwardPwrAnalogUndividedPinIn); // 100 usec
        if (fwdHires <= AdcMinNonzero)
            fwdHires = 0;
        if (fwdHires >= MAXED_ADC)
        {   // undivided voltage at ADC is above 5V, so use the divided ones
            fwdHires = analogRead(ForwardPwrAnalogLowPinIn); // 100 usec
            if (fwdHires <= AdcMinNonzero)
                fwdHires = 0;
            revHires = analogRead(ReversePwrAnalogLowPinIn); // 100 usec
            if (revHires <= AdcMinNonzero)
                revHires = 0;
            fwdHires *= VOLTS_LOW_MULTIPLIER;
            revHires *= VOLTS_LOW_MULTIPLIER;
        }
        else
        {
            revHires = analogRead(ReversePwrAnalogUndividedPinIn); // 100 usec
            if (revHires <= AdcMinNonzero)
                revHires = 0;
            fwdHires *= VOLTS_UNDIVIDED_MULTIPLER;
            revHires *= VOLTS_UNDIVIDED_MULTIPLER;
        }

        // the coupler has schottkey barrier diodes, which limit to about 380mV
        if (fwdHires > 0)
            fwdHires += SchottkeyBarrier;
        if (revHires > 0)
            revHires += SchottkeyBarrier;

        movingAverage::apply(fwdHires, revHires);
    }

    template <int PWRENTRIES>
    class MeterInvert {
    public:
        class FLASH
        {
            /* FLASH
             * binary search the table who's contents are in program memory
             */
        public:
            FLASH(const uint16_t* pgm) : addr((uint16_t)pgm) {}
            FLASH(const FLASH& other) : addr(other.addr) {}
            uint16_t operator[] (int i) const
            {
                return pgm_read_word_near(addr + i * sizeof(uint16_t));
            }
        private:
            uint16_t addr;
        };
        static unsigned map(unsigned i) { return i << (PWM_MAX_PWR - PWRENTRIES); }
        static unsigned TableLookup(uint16_t value, FLASH table)
        {
            // binary search the table
            if (value <= table[0])
                return 0;
            static const int LAST_ENTRY = (1 << PWRENTRIES) - 1;
            if (value >= table[LAST_ENTRY])
                return LAST_ENTRY;
            unsigned inc = 1 << (PWRENTRIES - 2);
            unsigned i;
            for (i = 1 << (PWRENTRIES - 1); /*start from middle*/
                inc != 0;
                inc >>= 1)
            {
                bool higher = value > table[i];
                if (!higher)
                {
                    if (value == table[i])
                        return i;
                    i -= inc;
                }
                else
                    i += inc;
            }
            // one more compare once inc == 0
            if (value < table[i])
                i -= 1;
            return i;
        }
    };

    uint8_t SwrToMeter(uint16_t swrCoded)
    {
        uint16_t v = MeterInvert<SwrMeter::PWR_ENTRIES>::TableLookup(swrCoded, SwrMeter::PwmToSwr);
        v = MeterInvert<SwrMeter::PWR_ENTRIES>::map(v);
        static MeterFilter meterFilter; // don't jerk the meter around too quickly
        analogWrite(SwrMeterPinOut, meterFilter.apply(v));
        return (uint8_t)v;
    }

    uint8_t DisplaySwr()
    {
        static movingAverage::AvgSinceLastCheck average;
        unsigned long displayValue = 1;
        uint32_t f;
        uint32_t r;
        average.getCalibratedSums(f, r);
        if (f)
        {
            if (r < f)
            {   // SWR = (f + r) / (f - r) -- all in volts (not power!)
                uint32_t fplusr = f;
                fplusr += r;
                fplusr <<= SWR_SCALE_PWR; // convert to display units
                uint32_t fminusr = f;
                fminusr -= r;
                if (fminusr != 0)
                {
                    displayValue = fplusr / fminusr;
                    if (displayValue > INFINITE_SWR << SWR_SCALE_PWR)
                        displayValue = INFINITE_SWR << SWR_SCALE_PWR;
                }
                else
                    displayValue = INFINITE_SWR << SWR_SCALE_PWR;
            }
            else
                displayValue = INFINITE_SWR << SWR_SCALE_PWR;
        }
        return SwrToMeter(static_cast<uint16_t>(displayValue));
    }

    bool FrontPanelLamps()
    {
        if (digitalRead(PanelLampsPinOut) == HIGH)
        {
            unsigned long timeOn = millis() - coupler7dot5LastHeardMillis;
            if (timeOn > FrontPanelLampsOnMsec)
                digitalWrite(PanelLampsPinOut, LOW);
            return true;
        }
        else
            return false;
    }

    DisplayPower_t SquareToWatts(DisplayPower_t s)
    {
        uint64_t ret = s;
        ret *= NominalCouplerResistanceRecip; // really is a multiply
        ret /= NominalCouplerResistanceMultiplier;  // implemented as logical shift
        return ret;
    }

    DisplayPower_t VoltsToWatts(AcquiredVolts_t v)
    {
        DisplayPower_t w = v;
        w *= w;
        return SquareToWatts(w);
    }

    DisplayPower_t getPeakPwr()
    {
        AcquiredVolts_t f;
        AcquiredVolts_t r;
        movingAverage::getPeaks(f, r);
        DisplayPower_t ret = VoltsToWatts(BackPanelPwrSwitchFwd ? calibrateFwd(f) : calibrateRev(r));
        static DisplayPower_t prev;
        bool sample = ret != 0;
        leds.SetSampleLed(ret != 0);
        leds.BlinkLed(PowerMeterLeds::FrontPanel::PEAK_SAMPLE, true);
        leds.SetHoldLed(false);
        prev = ret;
        HoldTimePotMsec = 0;
        return ret;
    }

    DisplayPower_t getPeakHoldPwr()
    {
        // calculate the peak
        AcquiredVolts_t f;
        AcquiredVolts_t r;
        movingAverage::getPeaks(f, r);
        DisplayPower_t ret = VoltsToWatts(BackPanelPwrSwitchFwd ? calibrateFwd(f) : calibrateRev(r));

        static DisplayPower_t peakHold;
        unsigned long now = millis();
        if ((ret > peakHold) || (now - HoldPeakRecordedAtMillis > HoldTimePotMsec))
        {
            HoldPeakRecordedAtMillis = now;
            leds.SetSampleLed(ret > 0);
            leds.SetHoldLed(ret > 0);
            peakHold = ret;
        }
        else
        {
            leds.SetSampleLed(false);
            ret = peakHold;
        }
        leds.BlinkLed(PowerMeterLeds::FrontPanel::PEAK_SAMPLE, false);

        // Read the hold pot
        uint16_t holdPot = readHoldPot();
        // range is 0 to  1024
        HoldTimePotMsec = holdPot * 25;
        return ret;
    }

    DisplayPower_t getAveragePwr()
    {
        leds.SetHoldLed(false);
        leds.SetSampleLed(false);
        HoldTimePotMsec = 0;
        return SquareToWatts(BackPanelPwrSwitchFwd ?
            calibrateFwdPower(movingAverage::fwdPwr()) :
            calibrateRevPower(movingAverage::revPwr()));
    }

    namespace PwrMeter {
        /*
         *  PWM values are from 0 to 255 on the Arduino/Atmega128, which correspond to zero and
         *  full output, which is somewhat MORE than full scale on the meters (so that we can
         *  access the full meter movement.)
         *  We only NUM_PWM values out of 256:
         *      search this table for the nearest value to display
         *      use the index in this table to write analogWrite, but multiply by
         *      128  first.
         *
         *      Entries in this table are Power * PWR_SCALE
         */
        const int PWR_ENTRIES = 8;
        const int NUM_PWM = 1 << PWR_ENTRIES;
        const int LOWEST_VALID_CALIBRATION = 100;
        const int HIGHEST_VALID_CALIBRATION = 207;
#ifdef OEM_METER_SCALES
        // this table was constructing by observing the meter position on the OEM meter
        // for the PWM values from 0 to 255
        // These numbers were chosen based on the RFM-003. The RFM-005 will require
        // a different table.
        const uint16_t PwmToPwr[NUM_PWM] PROGMEM =
        {
                0, // 0
                3, // 0.0204082
                10, // 0.0816327
                24, // 0.183674
                42, // 0.326531
                65, // 0.510204
                94, // 0.734694
                128, // 1
                142, // 1.10623
                156, // 1.21783
                171, // 1.33479
                187, // 1.45711
                203, // 1.58479
                220, // 1.71783
                238, // 1.85623
                256, // 2
                283, // 2.21247
                312, // 2.43566
                342, // 2.66958
                373, // 2.91421
                406, // 3.16958
                440, // 3.43566
                475, // 3.71247
                512, // 4
                554, // 4.32939
                598, // 4.67181
                643, // 5.02727
                691, // 5.39575
                739, // 5.77727
                790, // 6.17181
                842, // 6.57939
                896, // 7
                940, // 7.34582
                986, // 7.69997
                1032, // 8.06247
                1079, // 8.4333
                1128, // 8.81247
                1178, // 9.19998
                1228, // 9.59582
                1280, // 10
                1353, // 10.5698
                1428, // 11.1553
                1505, // 11.7566
                1584, // 12.3737
                1665, // 13.0066
                1748, // 13.6553
                1833, // 14.3198
                1920, // 15
                1966, // 15.3601
                2013, // 15.7244
                2060, // 16.093
                2108, // 16.4658
                2156, // 16.843
                2205, // 17.2244
                2254, // 17.6101
                2304, // 18
                2380, // 18.5915
                2457, // 19.1926
                2535, // 19.8033
                2614, // 20.4235
                2695, // 21.0533
                2777, // 21.6926
                2860, // 22.3415
                2944, // 23
                3021, // 23.5981
                3098, // 24.2039
                3177, // 24.8174
                3256, // 25.4386
                3337, // 26.0674
                3418, // 26.7039
                3501, // 27.3481
                3584, // 28
                3676, // 28.7182
                3769, // 29.4454
                3863, // 30.1818
                3959, // 30.9272
                4055, // 31.6818
                4153, // 32.4454
                4252, // 33.2182
                4352, // 34
                4445, // 34.7233
                4538, // 35.4543
                4633, // 36.1929
                4728, // 36.9391
                4825, // 37.6929
                4922, // 38.4543
                5021, // 39.2234
                5120, // 40 <-- valid Calibration setup lowest
                5213, // 40.7271
                5307, // 41.4607
                5402, // 42.2009
                5497, // 42.9476
                5594, // 43.7009
                5691, // 44.4607
                5789, // 45.2271
                5888, // 46
                6026, // 47.0811
                6166, // 48.1747
                6308, // 49.2808
                6451, // 50.3996
                6596, // 51.5308
                6742, // 52.6747
                6890, // 53.8311
                7040, // 55
                7149, // 55.8521
                7259, // 56.7107
                7370, // 57.5759
                7481, // 58.4476
                7594, // 59.3259
                7707, // 60.2107
                7821, // 61.1021
                7936, // 62
                8091, // 63.2091
                8247, // 64.4299
                8405, // 65.6624
                8564, // 66.9066
                8725, // 68.1624
                8887, // 69.4299
                9051, // 70.7091
                9216, // 72
                9341, // 72.977
                9467, // 73.9605
                9594, // 74.9506
                9721, // 75.9473
                9850, // 76.9506
                9979, // 77.9605
                10109, // 78.977
                10240, // 80
                10396, // 81.2178
                10553, // 82.4448
                10711, // 83.681
                10871, // 84.9264
                11031, // 86.181
                11193, // 87.4448
                11356, // 88.7178
                11520, // 90
                11692, // 91.3403
                11864, // 92.6905
                12038, // 94.0507
                12214, // 95.4207
                12390, // 96.8007
                12568, // 98.1906
                12748, // 99.5903
                12928, // 101
                13115, // 102.463
                13304, // 103.937
                13494, // 105.421
                13685, // 106.916
                13878, // 108.421
                14072, // 109.937
                14267, // 111.463
                14464, // 113
                14652, // 114.467
                14841, // 115.943
                15031, // 117.429
                15222, // 118.924
                15415, // 120.429
                15609, // 121.943
                15804, // 123.467
                16000, // 125
                16219, // 126.709
                16439, // 128.43
                16661, // 130.163
                16884, // 131.907
                17109, // 133.663
                17335, // 135.43
                17563, // 137.209
                17792, // 139
                17981, // 140.473
                18170, // 141.953
                18361, // 143.442
                18552, // 144.938
                18745, // 146.442
                18938, // 147.953
                19133, // 149.473
                19328, // 151
                19548, // 152.716
                19769, // 154.442
                19991, // 156.177
                20214, // 157.922
                20439, // 159.677
                20665, // 161.442
                20892, // 163.216
                21120, // 165
                21355, // 166.839 <- valid calibration setup highest
                21592, // 168.689
                21830, // 170.549
                22070, // 172.418
                22310, // 174.299
                22552, // 176.189
                22795, // 178.089
                23040, // 180
                23276, // 181.842
                23513, // 183.694
                23751, // 185.555
                23990, // 187.425
                24231, // 189.305
                24473, // 191.194
                24716, // 193.092
                24960, // 195
                25196, // 196.845
                25433, // 198.698
                25672, // 200.56 -- this one is the HIGH lockout threshold
                25911, // 202.431
                26152, // 204.31
                26393, // 206.198
                26636, // 208.095
                26880, // 210
                27116, // 211.847
                27354, // 213.701
                27592, // 215.564
                27832, // 217.435
                28072, // 219.314
                28314, // 221.202
                28556, // 223.097
                28800, // 225
                29037, // 226.849
                29274, // 228.705
                29513, // 230.568
                29752, // 232.44
                29993, // 234.318
                30234, // 236.205
                30477, // 238.099
                30720, // 240
                31034, // 242.456
                31350, // 244.925
                31668, // 247.406
                31987, // 249.9
                32308, // 252.406
                32630, // 254.925
                32954, // 257.456
                33280, // 260
                33486, // 261.608
                33692, // 263.22
                33899, // 264.838
                34107, // 266.46
                34315, // 268.088
                34524, // 269.72
                34734, // 271.358
                34944, // 273
                35290, // 275.703
                35638, // 278.42
                35987, // 281.15
                36338, // 283.893
                36691, // 286.65
                37046, // 289.42
                37402, // 292.203
                37760, // 295
                38120, // 297.81
                38481, // 300.633  Series resistor is selected for meter full scale @ 249
                38844, // 303.47
                39209, // 306.32
                39575, // 309.183
                39944, // 312.059
                40314, // 314.949
                40685, // 317.852
         };
#endif
#ifdef CUSTOM_METER_SCALES
         const uint16_t PwmToPwr[NUM_PWM] PROGMEM =
         {
            // generated by a spreadsheet where
            // 
            // A         B           C                   D
            // 0	   =+A1/250	    =POWER(B1;2)*300	=C1*128
            // =+A1+1	=+A2/250	=POWER(B2;2)*300	=C2*128
            // Generate 256 rows
            // 
            // The 250 is the PWM full scale
            0	,
            1	,
            2	,
            6	,
            10	,
            15	,
            22	,
            30	,
            39	,
            50	,
            61	,
            74	,
            88	,
            104	,
            120	,
            138	,
            157	,
            178	,
            199	,
            222	,
            246	,
            271	,
            297	,
            325	,
            354	,
            384	,
            415	,
            448	,
            482	,
            517	,
            553	,
            590	,
            629	,
            669	,
            710	,
            753	,
            796	,
            841	,
            887	,
            935	,
            983	,
            1033	,
            1084	,
            1136	,
            1189	,
            1244	,
            1300	,
            1357	,
            1416	,
            1475	,
            1536	,
            1598	,
            1661	,
            1726	,
            1792	,
            1859	,
            1927	,
            1996	,
            2067	,
            2139	,
            2212	,
            2286	,
            2362	,
            2439	,
            2517	,
            2596	,
            2676	,
            2758	,
            2841	,
            2925	,
            3011	,
            3097	,
            3185	,
            3274	,
            3364	,
            3456	,
            3549	,
            3643	,
            3738	,
            3834	,
            3932	,
            4031	,
            4131	,
            4233	,
            4335	,
            4439	,
            4544	,
            4650	,
            4758	,
            4867	,
            4977	,
            5088	,
            5200	,
            5314	,
            5429	,
            5545	,
            5662	,
            5781	,
            5901	,
            6022	,
            6144	,
            6267	,
            6392	,
            6518	,
            6645	,
            6774	,
            6903	,
            7034	,
            7166	,
            7300	,
            7434	,
            7570	,
            7707	,
            7845	,
            7985	,
            8125	,
            8267	,
            8411	,
            8555	,
            8701	,
            8847	,
            8995	,
            9145	,
            9295	,
            9447	,
            9600	,
            9754	,
            9910	,
            10066	,
            10224	,
            10383	,
            10544	,
            10705	,
            10868	,
            11032	,
            11197	,
            11364	,
            11532	,
            11701	,
            11871	,
            12042	,
            12215	,
            12389	,
            12564	,
            12740	,
            12918	,
            13097	,
            13277	,
            13458	,
            13640	,
            13824	,
            14009	,
            14195	,
            14382	,
            14571	,
            14761	,
            14952	,
            15144	,
            15338	,
            15533	,
            15729	,
            15926	,
            16124	,
            16324	,
            16525	,
            16727	,
            16930	,
            17135	,
            17341	,
            17548	,
            17756	,
            17966	,
            18176	,
            18388	,
            18602	,
            18816	,
            19032	,
            19249	,
            19467	,
            19686	,
            19907	,
            20128	,
            20351	,
            20576	,
            20801	,
            21028	,
            21256	,
            21485	,
            21715	,
            21947	,
            22180	,
            22414	,
            22649	,
            22886	,
            23124	,
            23363	,
            23603	,
            23844	,
            24087	,
            24331	,
            24576	,
            24822	,
            25070	,
            25319	,
            25569	,
            25820	,
            26073	,
            26326	,
            26581	,
            26838	,
            27095	,
            27354	,
            27614	,
            27875	,
            28137	,
            28401	,
            28665	,
            28931	,
            29199	,
            29467	,
            29737	,
            30008	,
            30280	,
            30553	,
            30828	,
            31104	,
            31381	,
            31659	,
            31939	,
            32220	,
            32502	,
            32785	,
            33069	,
            33355	,
            33642	,
            33930	,
            34220	,
            34510	,
            34802	,
            35095	,
            35389	,
            35685	,
            35982	,
            36280	,
            36579	,
            36879	,
            37181	,
            37484	,
            37788	,
            38093	,
            38400	,
            38708	,
            39017	,
            39327	,
            39639	,
            39951	,

        };
#endif
    }

    void PwrToMeter(uint16_t toDisplay)
    {
        uint16_t m = MeterInvert<PwrMeter::PWR_ENTRIES>::TableLookup(toDisplay, PwrMeter::PwmToPwr);
        auto v = MeterInvert<PwrMeter::PWR_ENTRIES>::map(m);
        static MeterFilter meterFilter;
        analogWrite(RfMeterPinOut, meterFilter.apply(v));
    }

    void DisplayPwr(DisplayPower_t v)
    {
        static unsigned long lastHighTime;
        static unsigned long lastNormalTime;
        static unsigned long lastOnTime;
        uint16_t toDisplay = v;

        unsigned long now = millis();
        if (v < PowerMinToDisplay)
        {
            unsigned long timeOn = now - lastOnTime;
            if ((timeOn > HoldPwrLampsOnMsec) && (timeOn > HoldTimePotMsec))
            {
                leds.SetLowLed(false);
                leds.BlinkLed(PowerMeterLeds::FrontPanel::RANGE_LOW, false);
                leds.SetHighLed(false);
            }
            toDisplay = 0;
        }
        else
        {
            lastOnTime = now;
            if (v > PWR_BREAKTOHIGH_POINT)
            {
                lastHighTime = now;
                toDisplay = v / 10;
                leds.SetLowLed(false);
                leds.BlinkLed(PowerMeterLeds::FrontPanel::RANGE_LOW, false);
                leds.SetHighLed(true);
            }
            else if (leds.GetHighLed())
            {
                if (now - lastHighTime > HoldHighLedMsec)
                {
                    lastNormalTime = now;
                    leds.SetLowLed(true);
                    leds.BlinkLed(PowerMeterLeds::FrontPanel::RANGE_LOW, false);
                    leds.SetHighLed(false);
                }
                else
                    toDisplay = v / 10;
            }
            else if (v <= PWR_BREAKTOLOWLOW_POINT)
            {
                if (now - lastNormalTime > HoldHighLedMsec)
                {
                    /*
                     * Feature not in original meter
                     * For very low powers, switch the meter scale to 10X and
                     * Flash the LOW LED
                     */
                    leds.SetHighLed(false);
                    toDisplay *= 10;
                    // blink the low power LED
                    leds.SetLowLed(false, true);
                    leds.BlinkLed(PowerMeterLeds::FrontPanel::RANGE_LOW, true);
                }
            }
            else
            {
                lastNormalTime = now;
                leds.SetLowLed(true);
                leds.BlinkLed(PowerMeterLeds::FrontPanel::RANGE_LOW, false);
                leds.SetHighLed(false);
            }
        }
        PwrToMeter(toDisplay);
    }
}

namespace Alo {
        void Lockout(DisplayPower_t p)
        {
                static const uint32_t LockoutThreshold =    25672; // 200W
                // only turn on SENSE light until foward power exceeds 200W
                if (p >= LockoutThreshold )
                {
                        LockoutStartedAtMillis = millis();
                        leds.SetAloLock(true);
                }
        }

        void CheckAloSwr(uint8_t swr)
        {
                uint8_t aloLimit = EEPROM.read((int)EEPROM_SWR_LOCK);
                if (swr >= aloLimit)
                {
                        leds.SetSenseLed(true);
                        AcquiredVolts_t f;
                        AcquiredVolts_t r;
                        movingAverage::getPeaks(f,r);
                        Lockout(VoltsToWatts(calibrateFwd(f)));
                }
                else
                    leds.SetSenseLed(false);
        }

        void CheckAloPwr()
        {
                AcquiredVolts_t f;
                AcquiredVolts_t r;
                movingAverage::getPeaks(f,r);
                DisplayPower_t ref = VoltsToWatts(calibrateRev(r));
                uint8_t v = MeterInvert<PwrMeter::PWR_ENTRIES>::TableLookup(ref, PwrMeter::PwmToPwr);
                if (v >= EEPROM.read((int)EEPROM_PWR_LOCK))
                {
                        leds.SetSenseLed(true);
                        Lockout(VoltsToWatts(calibrateFwd(f)));
                }
                else
                    leds.SetSenseLed(false);
        }

        // meter is in calibrate mode,
        // ALO lock out settings from HOLD pot
        void doAloSetup()
        {
                static uint8_t swr;
                static uint8_t pwr;
                if (digitalRead(PeakSwitchPinIn) == LOW)
                {   // at top of function, but happens last...
                        MeterMode = METER_NORMAL;
                        EEPROM.update((int)EEPROM_SWR_LOCK, swr);
                        EEPROM.update((int)EEPROM_PWR_LOCK, pwr);
                        return;
                }

                unsigned long now = millis();
                if (now - EnteredAloSetupModeTime > AloSetupModeTimesOut)
                {
                        MeterMode = METER_NORMAL;
                        return;
                }

                leds.SetSenseLed(true);
                leds.SetAloLock(true);

                if (digitalRead(AverageSwitchPinIn) == HIGH)
                {   // read EEPROM settings
                        analogWrite(SwrMeterPinOut, EEPROM.read((int)EEPROM_SWR_LOCK));
                        analogWrite(RfMeterPinOut, EEPROM.read((int)EEPROM_PWR_LOCK));
                        leds.SetLowLed(true);
                        leds.BlinkLed(PowerMeterLeds::FrontPanel::RANGE_LOW, false);
                        leds.SetHighLed(false);
                        return;
                }


                int holdPot = readHoldPot();
                holdPot /= 2;
                if (holdPot > 255)
                        holdPot = 255;

                static bool lastAdjust;
                bool adjust = lastAdjust;
                if (BackPanelAloSwitchSwr)
                {
                        analogWrite(SwrMeterPinOut, holdPot);
                        swr = holdPot;
                        leds.SetLowLed(false);
                        leds.SetHighLed(false);
                        lastAdjust = false;
                }
                else
                {
                        analogWrite(RfMeterPinOut, holdPot);
                        pwr = holdPot;
                        leds.SetLowLed(true);
                        leds.BlinkLed(PowerMeterLeds::FrontPanel::RANGE_LOW, false);
                        leds.SetHighLed(false);
                        lastAdjust = true;
                }
                if (lastAdjust != adjust)
                        EnteredAloSetupModeTime = now;
        }
}

namespace calibrate {

    int16_t EpromByteToCaliOffset(uint8_t v)
    {
        if ((v >= PwrMeter::LOWEST_VALID_CALIBRATION) &&
            (v <= PwrMeter::HIGHEST_VALID_CALIBRATION))
        {
            // centered integer
            int32_t c = v;
            c -= (PwrMeter::HIGHEST_VALID_CALIBRATION + PwrMeter::LOWEST_VALID_CALIBRATION) / 2;
            // c ranges about +/- 50 here
            return c * 80; // or about +/- 4000, which is roughly +/- 5% adjustment range
        }
        return 0;
    }

    void SetCalibrationConstantsFromEEPROM()
    {
        fwdCalibration = 0x8000; // this fixed point 1.0 multiplier
        revCalibration = 0x8000; // ditto

        uint8_t fCal = EEPROM.read((int)EEPROM_FWD_CALIBRATION);
        fwdCalibration += EpromByteToCaliOffset(fCal);
        uint8_t reflectedCal = EEPROM.read((int)EEPROM_REFL_CALIBRATION);
        revCalibration += EpromByteToCaliOffset(reflectedCal);
    }

    // meter is in calibrate mode, power FOR/REFL settings adjust
    // from HOLD pot.
    void doCalibrateSetup()
    {
        using namespace PwrMeter;
        static uint8_t forwardCal;
        static uint8_t reflectedCal;
        static bool previousBackPanelPwrSwitchFwd;

        if (digitalRead(PeakSwitchPinIn) == LOW)
        {
            MeterMode = METER_NORMAL;
            if ((forwardCal >= LOWEST_VALID_CALIBRATION) &&
                (forwardCal <= HIGHEST_VALID_CALIBRATION))
                EEPROM.update((int)EEPROM_FWD_CALIBRATION, forwardCal);
            if ((reflectedCal >= LOWEST_VALID_CALIBRATION) &&
                (reflectedCal <= HIGHEST_VALID_CALIBRATION))
                EEPROM.update((int)EEPROM_REFL_CALIBRATION, reflectedCal);
            SetCalibrationConstantsFromEEPROM();
            return;
        }

        unsigned long now = millis();
        if (now - EnteredAloSetupModeTime > AloSetupModeTimesOut)
        {
            MeterMode = METER_NORMAL;
            return;
        }

        leds.SetAloLock(true);

        analogWrite(SwrMeterPinOut, 0);

        if (digitalRead(AverageSwitchPinIn) == HIGH)
        {   // read EEPROM settings
            analogWrite(RfMeterPinOut, EEPROM.read(
                BackPanelPwrSwitchFwd ? EEPROM_FWD_CALIBRATION : EEPROM_REFL_CALIBRATION));
            return;
        }
        else
        {
            int holdPot = readHoldPot();
            holdPot /= 2;
            if (holdPot > 255)
                holdPot = 255;
            analogWrite(RfMeterPinOut, holdPot);
            if (BackPanelPwrSwitchFwd)
                forwardCal = holdPot;
            else
                reflectedCal = holdPot;
        }
        if (BackPanelPwrSwitchFwd != previousBackPanelPwrSwitchFwd)
            EnteredAloSetupModeTime = now;
        previousBackPanelPwrSwitchFwd = BackPanelPwrSwitchFwd;

    }
}

namespace sleep {
    void Coupler7dot5Interrupt()
    {
        detachInterrupt(digitalPinToInterrupt(couplerPowerDetectPinIn));
    }

    void CaliInterrupt()
    {
        detachInterrupt(digitalPinToInterrupt(initiateCalibratePinIn));
    }

    void SleepNow()
    {
#ifdef SUPPORT_WDT
        wdt_disable();
#endif
        pullUpPins(false);
        digitalWrite(RfMeterPinOut, LOW);
        digitalWrite(SwrMeterPinOut, LOW);
        Wire.end();
        Serial.flush();
        Serial.end();
        static_assert(PIN_WIRE_SCL == A5, "PRO Mini SCL pin");
        pinMode(PIN_WIRE_SCL, INPUT);
        static_assert(PIN_WIRE_SDA == A4, "PRO Mini SDA pin");
        pinMode(PIN_WIRE_SDA, INPUT);
        pinMode(PIN_TXD, INPUT);
        pinMode(PIN_RXD, INPUT);
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        cli();
        ADCSRA &= ~(1 << ADEN); // ADC off
        attachInterrupt(digitalPinToInterrupt(couplerPowerDetectPinIn), Coupler7dot5Interrupt, LOW);
        attachInterrupt(digitalPinToInterrupt(initiateCalibratePinIn), CaliInterrupt, LOW);
        power_all_disable(); // this appears to be redundant. no power reduction results
        sleep_enable();
        sleep_bod_disable();
        sei();
        sleep_cpu();
        // measured current consumption when here went down to below 100uA (below 0.1 mV across 
        // 1 ohm in series with two AAA cells feeding the battery in my test set up.)
        // With these caveats regarding the FT232H break out:
        // a) the FT232H breakout need not be installed, in which case the result is below 100 uA.
        // b) if the FT232H is in place, but DTR is asserted from an application on the other side of the PCB,
        // also below 0.1mV.
        // c) But if the FT232H is in place, but DTR is not asserted, the corresponding pull up on the PCB
        // is drawing roughly 10 uA, which results in a detectable 0.4 mV in my test setup.
        power_all_enable();
        sleep_disable();
        sei();
        Serial.begin(SERIAL_BAUD);
        Wire.begin();
        pullUpPins(true);
        ADCSRA |= (1 << ADEN); // ADC back on
#ifdef SUPPORT_WDT
        wdt_enable(WDTO_1S);
#endif
    }
}

namespace Comm {
        void CommUpdateForwardAndReverse()
        {
            if (OutputToSerial == NO_OUTPUT_TO_SERIAL)
                return;
            static bool printedZero = false;
            static movingAverage::AvgSinceLastCheck average;
            uint32_t fV;
            uint32_t rV;
            average.getCalibratedSums(fV, rV);
            DisplayPower_t fd(0);
            DisplayPower_t rd(0);
            if (Comm::OutputToSerial == Comm::PEAK_OUTPUT_TO_SERIAL)
            {
                AcquiredVolts_t f; AcquiredVolts_t r;
                movingAverage::getPeaks(f, r);
                fd = VoltsToWatts(calibrateFwd(f));
                rd = VoltsToWatts(calibrateRev(r));
            }
            else if (Comm::OutputToSerial == Comm::AVG_OUTPUT_TO_SERIAL)
            {
                fd = SquareToWatts(calibrateFwdPower(movingAverage::fwdPwr()));
                rd = SquareToWatts(calibrateRevPower(movingAverage::revPwr()));
            }
            if ((fd > 0) || (rd > 0) || !printedZero)
            {
                // Voltages are always averaged
                Serial.print(F("Vf:")); Serial.print(fV); 
                Serial.print(F(" Vr:")); Serial.print(rV);

                //DisplayPower_t is calibrated in units of 1/128 Watt
                Serial.print(F(" Pf:")); Serial.print(fd); 
                Serial.print(F(" Pr:")); Serial.print(rd);
                if (leds.GetAloLock())
                    Serial.print(F(" L"));
                Serial.println();
            }
		    printedZero = (fd == 0) && (rd == 0);
        }
}

namespace {
    uint16_t readHoldPot()
    {
        auto r = (uint16_t)analogRead(HoldTimePotAnalogPinIn);
        if (EEPROM.read((int)EEPROM_POT_REVERSE) != 0)
            return r;
        uint16_t pmax;
        EEPROM.get((int)EEPROM_POT_MAX, pmax);
        if (pmax == 0xffff)
            pmax = 1023;
        return pmax - r;
    }   

    void AdcTest()
    {
        auto fLow = analogRead(ForwardPwrAnalogLowPinIn);
        auto fHigh = analogRead(ForwardPwrAnalogUndividedPinIn);
        auto rLow = analogRead(ReversePwrAnalogLowPinIn);
        auto rHigh = analogRead(ReversePwrAnalogUndividedPinIn);

        auto pDet = digitalRead(couplerPowerDetectPinIn);

        Serial.print(F("ADC TEst: fLow="));
        Serial.print(fLow);
        Serial.print(F(" fHigh="));
        Serial.print(fHigh);
        Serial.print(F(" rLow="));
        Serial.print(rLow);
        Serial.print(F(" rHigh="));
        Serial.print(rHigh);
        Serial.print(F(" fDet="));
        Serial.println(pDet == HIGH ? " high" : " low");

        auto fRaw = movingAverage::fwdPwr();
        auto caliF = calibrateFwdPower(fRaw);
        auto watts = SquareToWatts(caliF);
        Serial.print(F("fRaw="));
        Serial.print(fRaw);
        Serial.print(F("VV, caliF="));
        Serial.print(caliF);
        Serial.print(F("VV, W="));
        Serial.println(watts);
    }
}

