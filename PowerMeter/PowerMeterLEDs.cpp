#include "PowerMeterLEDs.h"

static const int AloLockPinOut = 13;
        
PowerMeterLeds::PowerMeterLeds(uint8_t pwrPin, uint8_t Laddr, uint8_t Raddr)
    : m_BankLeft(Laddr), m_BankRight(Raddr)
    , m_PowerEnablePin(pwrPin), m_blinkTime(0), m_BlinkMaskLeft(0), m_BlinkMaskRight(0)
    , m_UpdateLeftMask(~0), m_UpdateRightMask(~0), m_brightness(0x1F)
    , m_BlinkState(true)
{
    memset(m_StateLeft, 0, sizeof(m_StateLeft));
    memset(m_StateRight, 0, sizeof(m_StateRight));
}

void PowerMeterLeds::begin()
{
    pinMode(AloLockPinOut, OUTPUT);
    digitalWrite(AloLockPinOut, LOW);
    pinMode(m_PowerEnablePin, OUTPUT);
    digitalWrite(m_PowerEnablePin, HIGH);
    m_BankLeft.begin();
    m_BankRight.begin();
    m_UpdateLeftMask = ~0;
    m_UpdateRightMask = ~0;
}

static const int BLINK_MSEC = 100;

void PowerMeterLeds::loop(unsigned long now)
{
    bool blinkChanged = false;
    if (now > (m_blinkTime + BLINK_MSEC))
    {
        m_BlinkState = !m_BlinkState;
        blinkChanged = true;
        m_blinkTime = now;
    }

    uint8_t bright[8];
    uint8_t mask = 1;
    if ((m_UpdateLeftMask != 0) || (m_UpdateRightMask != 0))
    {
        m_BlinkState = true;
        m_blinkTime = now;
    }
    else
    for (uint8_t i = 0; i < 8; i += 1, mask <<= 1)
    {
        if ((m_BlinkMaskLeft & mask) && m_StateLeft[i] != 0)
            m_UpdateLeftMask |= mask;
        if ((m_BlinkMaskRight & mask) && m_StateRight[i] != 0)
            m_UpdateRightMask |= mask;
    }

    if ((m_UpdateLeftMask != 0) || (blinkChanged && m_BlinkMaskLeft != 0))
    {
        mask = 1;
        for (uint8_t i = 0; i < 8; i += 1, mask <<= 1)
        {
            if ((m_BlinkMaskLeft & mask) && !m_BlinkState)
                bright[i] = 0;
            else
                bright[i] = m_StateLeft[i];
        }
        m_BankLeft.UpdatePWM(bright);
        m_UpdateLeftMask = 0;
    }
    if ((m_UpdateRightMask != 0) || (blinkChanged && m_BlinkMaskRight != 0))
    {
        mask = 1;
        for (uint8_t i = 0; i < 8; i += 1, mask <<= 1)
        {
            if ((m_BlinkMaskRight & mask) && !m_BlinkState)
                bright[i] = 0;
            else
                bright[i] = m_StateRight[i];
        }
        m_BankRight.UpdatePWM(bright);
        m_UpdateRightMask = 0;
    }
}

void PowerMeterLeds::BlinkLed(FrontPanel led, bool blink)
{
    uint8_t maskL(0), maskR(0);
    switch (led)
    {
        case FrontPanel::ALO_SENSE:
            maskL |= static_cast<uint8_t>(LedChannel::ALO_SENSE);
            break;
        case FrontPanel::ALO_LOCK:
            maskL |= static_cast<uint8_t>(LedChannel::ALO_LOCK);
            break;
        case FrontPanel::PEAK_SAMPLE:
            maskL |= static_cast<uint8_t>(LedChannel::PEAK_SAMPLE);
            break;

        case FrontPanel::PEAK_HOLD:
            maskR |= static_cast<uint8_t>(LedChannel::PEAK_HOLD);
            break;
        case FrontPanel::RANGE_LOW:
            maskR |= static_cast<uint8_t>(LedChannel::RANGE_LOW);
            break;
        case FrontPanel::RANGE_HIGH:
            maskR |= static_cast<uint8_t>(LedChannel::RANGE_HIGH);
            break;

    }
    if (blink)
    {
        m_BlinkMaskLeft |= maskL;
        m_BlinkMaskRight |= maskR;
    }
    else
    {
        m_BlinkMaskLeft &= ~maskL;
        m_BlinkMaskRight &= ~maskR;
    }
}

void PowerMeterLeds::SetSenseLed(bool yellow)
{ 
    auto b1 = m_StateLeft[static_cast<uint8_t>(LedChannel::SENSE_GREEN)];
    auto b2 = m_StateLeft[static_cast<uint8_t>(LedChannel::SENSE_RED)];
    if (yellow)
    {
        m_StateLeft[static_cast<uint8_t>(LedChannel::SENSE_GREEN)] = m_brightness >> 2;
        m_StateLeft[static_cast<uint8_t>(LedChannel::SENSE_RED)] = m_brightness >> 1;
    }
    else
        m_StateLeft[static_cast<uint8_t>(LedChannel::SENSE_GREEN)] = m_StateLeft[static_cast<uint8_t>(LedChannel::SENSE_RED)] = 0;

    if ( b1 != m_StateLeft[static_cast<uint8_t>(LedChannel::SENSE_GREEN)] ||
            b2 != m_StateLeft[static_cast<uint8_t>(LedChannel::SENSE_RED)])
        m_UpdateLeftMask |= (1 << static_cast<uint8_t>(LedChannel::SENSE_GREEN)) | (1 << static_cast<uint8_t>(LedChannel::SENSE_RED));
}

void PowerMeterLeds::SetAloLock(bool red)
{
    auto b1 = m_StateLeft[static_cast<uint8_t>(LedChannel::LOCK_RED)];
    m_StateLeft[static_cast<uint8_t>(LedChannel::LOCK_RED)] = red ? m_brightness : 0;
    digitalWrite(AloLockPinOut, red ? HIGH : LOW);
    if (b1 != m_StateLeft[static_cast<uint8_t>(LedChannel::LOCK_RED)])
        m_UpdateLeftMask |= 1 << static_cast<uint8_t>(LedChannel::LOCK_RED);
}

void PowerMeterLeds::SetSampleLed(bool yellow)
{
#ifdef LEDS_ARE_RGY
    auto b1 = m_StateLeft[static_cast<uint8_t>(LedChannel::SAMPLE_YELLOW)];
    m_StateLeft[static_cast<uint8_t>(LedChannel::SAMPLE_YELLOW)] = yellow ? m_brightness : 0;
    if (b1 != m_StateLeft[static_cast<uint8_t>(LedChannel::SAMPLE_YELLOW)])
        m_UpdateLeftMask |= 1 << static_cast<uint8_t>(LedChannel::SAMPLE_YELLOW);
#endif
#ifdef LEDS_ARE_RGB
    auto b1 = m_StateLeft[static_cast<uint8_t>(LedChannel::SAMPLE_RED)];
    auto b2 = m_StateLeft[static_cast<uint8_t>(LedChannel::SAMPLE_GREEN)];
    m_StateLeft[static_cast<uint8_t>(LedChannel::SAMPLE_RED)] = m_StateLeft[static_cast<uint8_t>(LedChannel::SAMPLE_GREEN)] = yellow ? (m_brightness >> 1) : 0;
    if (b1 != m_StateLeft[static_cast<uint8_t>(LedChannel::SAMPLE_RED)] ||
        b2 != m_StateLeft[static_cast<uint8_t>(LedChannel::SAMPLE_GREEN)])
        m_UpdateLeftMask |= (1 << static_cast<uint8_t>(LedChannel::SAMPLE_RED)) | (1 << static_cast<uint8_t>(LedChannel::SAMPLE_GREEN));
#endif
}

void PowerMeterLeds::SetHoldLed(bool green, bool yellow)
{
#ifdef LEDS_ARE_RGY
    auto b1 = m_StateRight[static_cast<uint8_t>(LedChannel::HOLD_GREEN)];
    auto b2 = m_StateRight[static_cast<uint8_t>(LedChannel::HOLD_YELLOW)];
    m_StateRight[static_cast<uint8_t>(LedChannel::HOLD_GREEN)] = green ? m_brightness : 0;
    m_StateRight[static_cast<uint8_t>(LedChannel::HOLD_YELLOW)] = yellow ? m_brightness  : 0;
    if (b1 != m_StateRight[static_cast<uint8_t>(LedChannel::HOLD_GREEN)] ||
        b2 != m_StateRight[static_cast<uint8_t>(LedChannel::HOLD_YELLOW)])
        m_UpdateRightMask |= (1 << static_cast<uint8_t>(LedChannel::HOLD_GREEN)) | (1 << static_cast<uint8_t>(LedChannel::HOLD_YELLOW));
#endif
#ifdef LEDS_ARE_RGB
    auto b1 = m_StateRight[static_cast<uint8_t>(LedChannel::HOLD_GREEN)];
    auto b2 = m_StateRight[static_cast<uint8_t>(LedChannel::HOLD_RED)];
    m_StateRight[static_cast<uint8_t>(LedChannel::HOLD_GREEN)] = green ? m_brightness : (yellow ? m_brightness >> 1 : 0);
    m_StateRight[static_cast<uint8_t>(LedChannel::HOLD_RED)] = yellow ? m_brightness>>1 : 0;
    if (b1 != m_StateRight[static_cast<uint8_t>(LedChannel::HOLD_GREEN)] ||
        b2 != m_StateRight[static_cast<uint8_t>(LedChannel::HOLD_RED)])
        m_UpdateRightMask |= (1 << static_cast<uint8_t>(LedChannel::HOLD_GREEN)) | (1 << static_cast<uint8_t>(LedChannel::HOLD_RED));
#endif
}

void PowerMeterLeds::SetLowLed(bool green, bool yellow)
{
#ifdef LEDS_ARE_RGY
    auto b1 = m_StateRight[static_cast<uint8_t>(LedChannel::LOW_GREEN)];
    auto b2 = m_StateRight[static_cast<uint8_t>(LedChannel::LOW_YELLOW)];
    m_StateRight[static_cast<uint8_t>(LedChannel::LOW_GREEN)] = green ? m_brightness : 0;
    m_StateRight[static_cast<uint8_t>(LedChannel::LOW_YELLOW)] = yellow ? m_brightness : 0;
    if (b1 != m_StateRight[static_cast<uint8_t>(LedChannel::LOW_GREEN)] ||
        b2 != m_StateRight[static_cast<uint8_t>(LedChannel::LOW_YELLOW)])
        m_UpdateRightMask |= (1 << static_cast<uint8_t>(LedChannel::LOW_GREEN)) | (1 << static_cast<uint8_t>(LedChannel::LOW_YELLOW));
#endif
#ifdef LEDS_ARE_RGB
    auto b1 = m_StateRight[static_cast<uint8_t>(LedChannel::LOW_GREEN)];
    auto b2 = m_StateRight[static_cast<uint8_t>(LedChannel::LOW_BLUE)];
    m_StateRight[static_cast<uint8_t>(LedChannel::LOW_GREEN)] = green ? m_brightness : 0;
    m_StateRight[static_cast<uint8_t>(LedChannel::LOW_BLUE)] = yellow ? m_brightness : 0;
    if (b1 != m_StateRight[static_cast<uint8_t>(LedChannel::LOW_GREEN)] ||
        b2 != m_StateRight[static_cast<uint8_t>(LedChannel::LOW_BLUE)])
        m_UpdateRightMask |= (1 << static_cast<uint8_t>(LedChannel::LOW_BLUE)) | (1 << static_cast<uint8_t>(LedChannel::LOW_GREEN));
#endif
}

void PowerMeterLeds::SetHighLed(bool red)
{
    auto b = m_StateRight[static_cast<uint8_t>(LedChannel::HIGH_RED)];
    m_StateRight[static_cast<uint8_t>(LedChannel::HIGH_RED)] = red ? m_brightness : 0;
    if (b != m_StateRight[static_cast<uint8_t>(LedChannel::HIGH_RED)])
        m_UpdateRightMask |= 1 << static_cast<uint8_t>(LedChannel::HIGH_RED);
}
bool PowerMeterLeds::GetHighLed()
{
    return m_StateRight[static_cast<uint8_t>(LedChannel::HIGH_RED)] != 0;
}
bool PowerMeterLeds::GetAloLock()
{
    return 0 != m_StateLeft[static_cast<uint8_t>(LedChannel::LOCK_RED)];
}

void PowerMeterLeds::sleep()
{
    m_BankLeft.end();
    m_BankRight.end();
    digitalWrite(m_PowerEnablePin, LOW);
    m_UpdateLeftMask = ~0;
    m_UpdateRightMask = ~0;
}

void PowerMeterLeds::wake()
{
    digitalWrite(m_PowerEnablePin, HIGH);
    m_BankLeft.begin();
    m_BankRight.begin();
}

void PowerMeterLeds::SetBrightness(uint8_t b)
{
    m_brightness = b;
}

uint8_t PowerMeterLeds::GetBrightness()
{   return m_brightness; }

void PowerMeterLeds::test()
{
    uint8_t bright[8];
    {
        for (int i = 0; i < 8; i++)
            bright[i] = 0;
        m_BankLeft.UpdatePWM(bright);
        m_BankRight.UpdatePWM(bright);
        delay(1000);
        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
                    bright[j] = (j==i) ? m_brightness : 0;
            m_BankLeft.UpdatePWM(bright);
            delay(1000);
        }
        for (int i = 0; i < 8; i++)
            bright[i] = 0;
        m_BankRight.UpdatePWM(bright);
        m_BankLeft.UpdatePWM(bright);
        delay(1000);
        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
                bright[j] = (j == i) ? m_brightness : 0;
            m_BankRight.UpdatePWM(bright);
            delay(1000);
        }
    }
    for (int i = 0; i < 8; i++)
        m_StateLeft[i] = m_StateRight[i] = bright[i] = 0;
    m_BankRight.UpdatePWM(bright);
    m_BankLeft.UpdatePWM(bright);

}

void PowerMeterLeds::setAll(bool turnOn)
{
    uint8_t bright[8];
    for (int j = 0; j < 8; j++)
        bright[j] = turnOn ? m_brightness : 0;
    m_BankLeft.UpdatePWM(bright);
    m_BankRight.UpdatePWM(bright);
}