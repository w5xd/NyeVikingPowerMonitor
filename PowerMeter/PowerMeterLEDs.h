#pragma once
#include "Tlc59108.h"

//only one of the below
//#define LEDS_ARE_RGY
#define LEDS_ARE_RGB
/* WARNING ABOUT RGB 5mm T1-3/4 diodes: THEY DO NOT ALL HAVE THE SAME PINOUT
** I have at least two configurations, for pins 1 through 4:
** Code below is for RABG   (pin 2 is anode)
** also have:        RAGB   (note that the G and B leads are reversed)
** And I managed to install some of each into the same device */
//#define ROLL_YOUR_OWN_LED_ASSIGNMENTS

class PowerMeterLeds {
public: 
    enum class FrontPanel : uint8_t { ALO_SENSE, LED_FIRST=ALO_SENSE, ALO_LOCK, PEAK_SAMPLE, PEAK_HOLD, RANGE_LOW, RANGE_HIGH, LED_LAST=RANGE_HIGH} ;
    PowerMeterLeds(uint8_t powerEnablePin, uint8_t Laddr = 0x41, uint8_t Raddr = 0x43);

    void SetSenseLed(bool amber);
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
    void setAll(bool);

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

        SENSE_UNUSED = SENSE_YELLOW,
};
#endif
#ifdef LEDS_ARE_RGB
#ifdef ROLL_YOUR_OWN_LED_ASSIGNMENTS
#include "RollYourOwnLeds.h"
#else
    // wire RGB LEDS with R and G matching the RGY connections
    enum class LedChannel {
        // these assignments for LED pinouts that are RAGB on pins 1 through 4
        SENSE_BLUE = Tlc59108::LED1, SENSE_GREEN = Tlc59108::LED0, SENSE_RED = Tlc59108::LED2,
        LOCK_RED = Tlc59108::LED3, LOCK_BLUE = Tlc59108::LED7,
        SAMPLE_BLUE = Tlc59108::LED4, SAMPLE_RED = Tlc59108::LED5, SAMPLE_GREEN = Tlc59108::LED6,

        HOLD_GREEN = Tlc59108::LED5, HOLD_BLUE = Tlc59108::LED4,
        LOW_GREEN = Tlc59108::LED7, LOW_BLUE = Tlc59108::LED6, LOW_RED = Tlc59108::LED3,
        HIGH_RED = Tlc59108::LED2, HIGH_BLUE = Tlc59108::LED1, HIGH_GREEN = Tlc59108::LED0,

        ALO_SENSE = 1 << SENSE_GREEN | 1 << SENSE_BLUE | 1 << SENSE_RED,
        ALO_LOCK = 1 << LOCK_RED | 1 << LOCK_BLUE,
        PEAK_SAMPLE = 1 << SAMPLE_BLUE | 1 << SAMPLE_RED | 1 << SAMPLE_GREEN,
        PEAK_HOLD = 1 << HOLD_GREEN | 1 << HOLD_BLUE,
        RANGE_LOW = 1 << LOW_GREEN | 1 << LOW_BLUE | 1 << LOW_RED,
        RANGE_HIGH = 1 << HIGH_RED | 1 << HIGH_BLUE | 1 << HIGH_GREEN,

        SENSE_UNUSED = SENSE_BLUE,
    };
#endif
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

