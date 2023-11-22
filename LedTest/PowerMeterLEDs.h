#pragma once
#include "Tlc59108.h"

//only one of the below
#define LEDS_ARE_RGY
///#define LEDS_ARE_RGB

class PowerMeterLeds {
public: 
    enum class FrontPanel { ALO_SENSE, ALO_LOCK, PEAK_SAMPLE, PEAK_HOLD, RANGE_LOW, RANGE_HIGH};
    PowerMeterLeds(uint8_t powerEnablePin, uint8_t Laddr = 0x41, uint8_t Raddr = 0x43);

    void SetSenseLed(bool yellow);
    void SetAloLock(bool red);
    void SetSampleLed(bool yellow);
    void SetHoldLed(bool green, bool yellow = false);
    void SetLowLed(bool green, bool yellow = false);
    void BlinkLed(FrontPanel, bool);
    void SetHighLed(bool red);

    void SetBrightness(uint8_t);
    uint8_t GetBrightness();

    bool GetAloLock();
    bool GetHighLed();

    void begin();
    void loop(unsigned long now);
    void sleep();
    void wake();

    void test();

    Tlc59108 &LeftDevice() {return m_BankLeft;}
    Tlc59108 &RightDevice() {return m_BankRight;}

protected:
    // LedMask matches the PCB layout channel numbers on the tlc59108''s
#ifdef LEDS_ARE_RGY
    enum class LedChannel {
        SENSE_YELLOW = Tlc59108::LED0, SENSE_GREEN = Tlc59108::LED1, SENSE_RED = Tlc59108::LED2, 
        LOCK_RED = Tlc59108::LED3, LOCK_YELLOW = Tlc59108::LED7, 
        SAMPLE_YELLOW = Tlc59108::LED4, SAMPLE_RED= Tlc59108::LED5, SAMPLE_GREEN= Tlc59108::LED6,
        HOLD_GREEN = Tlc59108::LED5, HOLD_YELLOW = Tlc59108::LED4,
        LOW_GREEN = Tlc59108::LED7, LOW_YELLOW = Tlc59108::LED6, LOW_RED = Tlc59108::LED3,
        HIGH_RED = Tlc59108::LED2, HIGH_YELLOW = Tlc59108::LED1, HIGH_GREEN= Tlc59108::LED0,

        ALO_SENSE = 1 << SENSE_GREEN | 1 << SENSE_YELLOW | 1 << SENSE_RED,
        ALO_LOCK = 1 << LOCK_RED | 1 << LOCK_YELLOW,
        PEAK_SAMPLE = 1 << SAMPLE_YELLOW | 1 << SAMPLE_RED | 1 << SAMPLE_GREEN,
        PEAK_HOLD = 1 << HOLD_GREEN | 1 << HOLD_YELLOW,
        RANGE_LOW = 1 << LOW_GREEN | 1 << LOW_YELLOW | 1 << LOW_RED,
        RANGE_HIGH = 1 << HIGH_RED | 1 << HIGH_YELLOW | 1 << HIGH_GREEN,
};
#endif
#ifdef LEDS_ARE_RGB
    // wire RGB LEDS with R and G matching the RGY connections
    enum class LedChannel {
        SENSE_BLUE = Tlc59108::LED0, SENSE_GREEN = Tlc59108::LED1, SENSE_RED = Tlc59108::LED2,
        LOCK_RED = Tlc59108::LED3, LOCK_BLUE = Tlc59108::LED7,
        SAMPLE_BLUE = Tlc59108::LED4, SAMPLE_RED = Tlc59108::LED5, SAMPLE_GREEN = Tlc59108::LED6,

        HOLD_GREEN = Tlc59108::LED5, HOLD_RED = Tlc59108::LED4,
        LOW_GREEN = Tlc59108::LED7, LOW_BLUE = Tlc59108::LED6, LOW_RED = Tlc59108::LED3,
        HIGH_RED = Tlc59108::LED2, HIGH_BLUE = Tlc59108::LED1, HIGH_GREEN = Tlc59108::LED0,

        ALO_SENSE = 1 << SENSE_GREEN | 1 << SENSE_BLUE | 1 << SENSE_RED,
        ALO_LOCK = 1 << LOCK_RED | 1 << LOCK_BLUE,
        PEAK_SAMPLE = 1 << SAMPLE_BLUE | 1 << SAMPLE_RED | 1 << SAMPLE_GREEN,
        PEAK_HOLD = 1 << HOLD_GREEN | 1 << HOLD_RED,
        RANGE_LOW = 1 << LOW_GREEN | 1 << LOW_BLUE | 1 << LOW_RED,
        RANGE_HIGH = 1 << HIGH_RED | 1 << HIGH_BLUE | 1 << HIGH_GREEN,
    };
#endif
    enum {NUM_CHANNELS_PER_DRIVER = 8};

    Tlc59108 m_BankLeft;
    Tlc59108 m_BankRight;
    unsigned long m_blinkTime;
    uint8_t m_BlinkMaskLeft;
    uint8_t m_BlinkMaskRight;
    bool m_BlinkState;
    uint8_t m_PowerEnablePin;
    uint8_t m_StateLeft[NUM_CHANNELS_PER_DRIVER];
    uint8_t m_StateRight[NUM_CHANNELS_PER_DRIVER];
    uint8_t m_UpdateLeftMask;
    uint8_t m_UpdateRightMask;
    uint8_t m_brightness;
};

