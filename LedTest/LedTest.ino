/* Copyright (c) 2023 by Wayne E. Wright
 ** W5XD
 ** Round Rock, Texas, USA
 **
 ** For terms of use, see LICENSE
 */
#include <EEPROM.h>

#define DEBUG_TLD59108
#include "PowerMeterLEDs.h"

static_assert(sizeof(uint16_t)==2,"uint16_t");
static_assert(sizeof(int16_t)==2, "int16_t");
static_assert(sizeof(uint32_t)==4, "uint32_t");
static_assert(sizeof(int32_t)==4, "int32_t");
static_assert(sizeof(uint64_t)==8, "uint64_t");

/* Nye Viking RF Power Meter
 * Runs like the original analog circuit board, but on Arduino.

 * This code was tested with the RFM-003 (as opposed to the RFM-005).
 */

typedef uint16_t AcquiredVolts_t; // maxes at ADC max (1024) * 50 -- using 5VDC reference
typedef uint32_t DisplayPower_t; // This is 128 times Watts (i.e. value 128 is 1 watt

namespace {
    // pin assignments
    const int couplerPowerDetectPinIn = 2;
    const int initiateCalibratePinIn = 3;
    const int ALOtripSwitchPinIn = 4;
    const int PowerForwReflSwitchPinIn = 5;
    const int SwrMeterPinOut = 10;
    const int RfMeterPinOut = 11;
    const int PanelLampsPinOut = 9;
    const int Tlc59108PowerEnablePinOut = 8;

    const int ReversePwrAnalogHighPinIn = A1;
    const int ForwardPwrAnalogHighPinIn = A2;
    const int ReversePwrAnalogLowPinIn = A7;
    const int ForwardPwrAnalogLowPinIn = A0;
    const int HoldTimePotAnalogPinIn = A3;
    const int AverageSwitchPinIn = 7;
    const int PeakSwitchPinIn = 6;

    const int MAXED_ADC = 1022;
    const int MAXED_HOLD_POT = 710; // lm324 can't go full scale
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
    const unsigned ADC_DISCARD = 50; // 5.6 msec

    const long TimerLoopIntervalMicroSec = 1500; // 670KHz. analog input filter is 750Hz.
    const unsigned MeterUpdateIntervalMsec = 125; // 8Hz
    const unsigned CommUpdateIntervalMsec = 100;
    const unsigned long FrontPanelLampsOnMsec = 90000; // stay on 90 seconds per RFM documentation
    const unsigned long HoldPwrLampsOnMsec = 500;
    const unsigned HoldHighLedMsec = 400;
    const unsigned long LockoutLengthMsec = 5000;

    bool doAdcTest;
    bool printAdcTest;
    bool colorRotation;
    PowerMeterLeds leds(Tlc59108PowerEnablePinOut);
}

void setup() {
        pinMode(couplerPowerDetectPinIn, INPUT);
        pinMode(ALOtripSwitchPinIn, INPUT_PULLUP);
        pinMode(PowerForwReflSwitchPinIn, INPUT_PULLUP);
        pinMode(initiateCalibratePinIn, INPUT_PULLUP);

        digitalWrite(PanelLampsPinOut, LOW);
        pinMode(PanelLampsPinOut, OUTPUT);

        pinMode(PeakSwitchPinIn, INPUT_PULLUP);
        pinMode(AverageSwitchPinIn, INPUT_PULLUP);

        pinMode(ReversePwrAnalogHighPinIn, INPUT);
        pinMode(ForwardPwrAnalogHighPinIn,INPUT);
        pinMode(ReversePwrAnalogLowPinIn, INPUT);
        pinMode(ForwardPwrAnalogLowPinIn, INPUT);
        pinMode(HoldTimePotAnalogPinIn,INPUT);

        analogReference(DEFAULT); // go to 5.0V reference

        digitalWrite(PanelLampsPinOut,  HIGH); // turn on front panel lights on boot

        Wire.begin();

        Serial.begin(38400);
        Serial.println("W5XD LedTest 1.0");

        leds.begin();
        leds.SetLowLed(false,true);
        leds.BlinkLed(PowerMeterLeds::FrontPanel::RANGE_LOW, true);
}

static void AdcTest();
static void ColorRotation();

static bool testPot;

void loop()
{
    unsigned long now = millis();
    leds.loop(now);
    static unsigned long lastUpdate;
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
            testPot = false;
            buf[numInBuf] = 0;
            if (strcmp(buf, "ADC") == 0)
            {
                doAdcTest = !doAdcTest;
                printAdcTest = true;
            }
            else if (strcmp(buf, "LED") == 0)
                leds.test();
            else if (strcmp(buf, "DUMP") == 0)
            {
                leds.LeftDevice().dump();
                leds.RightDevice().dump();
            }
            else if (strcmp(buf, "COLOR") == 0)
            {
                colorRotation = !colorRotation;
            }
            else if (strcmp(buf, "RELAY") == 0)
            {
                leds.SetAloLock(!leds.GetAloLock());
            }
            else if (strcmp(buf, "POT") == 0)
                testPot = true;
            else if (strncmp(buf, "M1=", 3) == 0)
            {
                int v = atoi(buf+3);
                Serial.print("M1=");
                Serial.println(v);
                analogWrite(RfMeterPinOut, v);
            }
            else if (strncmp(buf, "M2=", 3) == 0)
            {
                int v = atoi(buf+3);
                Serial.print("M2=");
                Serial.println(v);
                analogWrite(SwrMeterPinOut, v);
            }
            else if (strncmp(buf, "BRI=", 4) == 0)
            {
                uint8_t v = atoi(buf + 4);
                Serial.print("BRI=");
                Serial.println(v);
                leds.SetBrightness(v);
            }
            else if (strcmp(buf, "SP3T") == 0)
            {
                bool peak = digitalRead(PeakSwitchPinIn) == LOW;
                bool avg = digitalRead(AverageSwitchPinIn) == LOW;
                uint8_t status = (peak ? 1 : 0) + (avg ? 2 : 0);
                switch (status)
                {
                    case 0:
                        Serial.println("PEAK HOLD");
                        break;
                    case 1:
                        Serial.println("PEAK");
                        break;
                    case 2:
                        Serial.println("AVERAGE");
                        break;
                    default:
                        Serial.print("Oops. Status = ");
                        Serial.println(status);
                        break;
                }
            }
            else if (strcmp(buf, "CAL") == 0)
            {
                Serial.print("Cali switch :");
                Serial.println(digitalRead(initiateCalibratePinIn) == LOW ? "pressed" : "off");
            }
            else if (strncmp(buf, "IREF=", 5) == 0)
            {
                int v = atoi(buf+5);
                leds.LeftDevice().SetCurrent(v);
                leds.RightDevice().SetCurrent(v);
            }
            else if (strcmp(buf, "LERR") == 0)
            {
                Serial.print("LED error Left=0x");
                Serial.println((int)leds.LeftDevice().GetErrors(), HEX);
                Serial.print("LED error Right=0x");
                Serial.println((int)leds.RightDevice().GetErrors(), HEX);
            }
            else if (strcmp(buf, "BACK") == 0)
            {
                Serial.println(digitalRead(ALOtripSwitchPinIn) == LOW ? "ALO: REV" : "ALO: SWR");
                Serial.println(digitalRead(PowerForwReflSwitchPinIn) == LOW ? "Power: Reflected" : "Power: Forward");
            }
            else if (strcmp(buf, "LAMPS") == 0)
            {
                digitalWrite(PanelLampsPinOut, digitalRead(PanelLampsPinOut) == LOW ? HIGH : LOW);
            }
            numInBuf = 0;
            Serial.println("W5XD LedTest>");
        }
        else numInBuf += 1;
        if (numInBuf >= sizeof(buf) - 1)
            numInBuf = 0;
    }
    
    if (doAdcTest)
        AdcTest();

    if (colorRotation)
        ColorRotation(now);

    if (testPot)
    {
        static unsigned long lastTested;
        if (now - lastTested > 1000)
        {
            lastTested = now;
            auto pot = analogRead(HoldTimePotAnalogPinIn);
            Serial.print("Pot is ");
            Serial.println((int)pot);
        }
    }
}

static void AdcTest()
{
    auto fLow = analogRead(ForwardPwrAnalogLowPinIn);
    auto fHigh = analogRead(ForwardPwrAnalogHighPinIn);
    auto rLow = analogRead(ReversePwrAnalogLowPinIn);
    auto rHigh = analogRead(ReversePwrAnalogHighPinIn);

    auto pDet = digitalRead(couplerPowerDetectPinIn);

    auto analogOut = fHigh >> 2;

    static bool Lastdetect;

    if (Lastdetect != (pDet != 0))
    {
        Lastdetect = (pDet != 0);
        Serial.print("Detect= ");
        Serial.println(pDet);
        leds.SetSampleLed(Lastdetect);
    }

    if (printAdcTest)
    {
        Serial.print("ADC TEst: fLow=");
        Serial.print(fLow);
        Serial.print(" fHigh=");
        Serial.print(fHigh);
        Serial.print(" rLow=");
        Serial.print(rLow);
        Serial.print(" rHigh=");
        Serial.print(rHigh);
        Serial.print(" fDet=");
        Serial.println(pDet == HIGH ? " high" : " low");
        Serial.print("Analog = ");
        Serial.println(analogOut);
        printAdcTest = false;
    }

}

static void ColorRotation(unsigned long n)
{
    static unsigned long prev;
    if (n - prev < 1000)
        return;
    prev = n;

    static const Tlc59108::LED diodes[3] = {
        Tlc59108::LED::LED0,
        Tlc59108::LED::LED1,
        Tlc59108::LED::LED2
    };
    auto ld = leds.LeftDevice();

    static uint8_t bright[3];

    uint8_t val = 0;
    uint8_t which;
    for (which = 0; which < 3; which++)
    {
        if (bright[which] < 64)
        {
            val = (bright[which] += 16);
            break;
        }
        else
        {
            bright[which] = 0;
            ld.UpdatePWM(diodes[which], 0);
            which += 1;
            if (which > 2)
                which = 0;
        }
    }


    ld.UpdatePWM(diodes[which], val);

    Serial.print("Values: ");
    for (uint8_t i = 0; i < 3; i++)
    {
        if (i != 0)
            Serial.print(", ");
        Serial.print((int)ld.ReadPwm(diodes[i]));
    }
    Serial.println();
    
}