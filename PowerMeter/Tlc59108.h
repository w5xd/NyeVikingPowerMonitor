#pragma once
// For TLC59108 LED driver IC 
#include <Arduino.h>
#include <Wire.h>
class Tlc59108 {
public:
    enum LED { LED0, LED1, LED2, LED3, LED4, LED5, LED6, LED7 };
    Tlc59108(uint8_t addr) : I2CADDR(addr)
    {    }

    void begin()
    {      
        Wire.beginTransmission(I2CADDR);
            Wire.write(static_cast<uint8_t>(AUTO_INCREMENT) | static_cast<uint8_t>(Addr::MODE1));
            Wire.write(AUTO_INCREMENT); // MODE1
            Wire.write(0); // MODE2
            for (uint8_t i = 0; i < 8; i++)
                Wire.write(0); // brightness for 8 diodes
            Wire.write(0); // GRPPWM group PWM is off
            Wire.write(0); // GRPFREQ is ignored when corresponding bit zero in GRPPWM
            static const uint8_t individual = 2 | (2 << 2) | (2 << 4) | (2 << 6);
            Wire.write(individual); // LEDOUT0. LEDs 0-3 are individually controlled
            Wire.write(individual); // LEDOUT1. LEDs 4-7 are individually controlled
        Wire.endTransmission();
    }

    void end()
    {}

    void dump()
    {
        Serial.print(F("Tlc at 0x"));
        Serial.println((int)I2CADDR, HEX);
        Wire.beginTransmission(I2CADDR);
        Wire.write(static_cast<uint8_t>(AUTO_INCREMENT) | static_cast<uint8_t>(Addr::MODE1));
        Wire.endTransmission();
        Wire.requestFrom(I2CADDR, static_cast<uint8_t>(Addr::REG_MAX));
        for (uint8_t i = 0; i < static_cast<uint8_t>(Addr::REG_MAX); i++)
        {
            uint8_t b = Wire.read();
            Serial.print("i = 0x");
            Serial.print((int)i, HEX);
            Serial.print(" reg = 0x");
            Serial.println((int) b, HEX);
        }

    }

    void UpdatePWM(uint8_t v[8])
    {
        Wire.beginTransmission(I2CADDR);
        Wire.write(static_cast<uint8_t>(Addr::PWM0) | static_cast<uint8_t>(AUTO_INCREMENT_BRIGHTNESS));
        for (uint8_t i = 0; i < 8; i++)
            Wire.write(v[i]);
        Wire.endTransmission();
    }

    void UpdatePWM(enum LED which, uint8_t value)
    {
        Wire.beginTransmission(I2CADDR);
        Wire.write(static_cast<uint8_t>(which) + static_cast<uint8_t>(Addr::PWM0));
        Wire.write(value);
        Wire.endTransmission();
    }

    uint8_t ReadPwm(enum LED which)
    {
        Wire.beginTransmission(I2CADDR);
        Wire.write(static_cast<uint8_t>(which) + static_cast<uint8_t>(Addr::PWM0));
        Wire.endTransmission();
        Wire.requestFrom(I2CADDR, static_cast<uint8_t>(1));
        return Wire.read();
    }

    void SetCurrent(uint8_t val)
    {
        Wire.beginTransmission(I2CADDR);
        Wire.write(static_cast<uint8_t>(Addr::IREF));
        Wire.write(val);
        Wire.endTransmission();
    }

    uint8_t GetErrors()
    {
        Wire.beginTransmission(I2CADDR);
        Wire.write(static_cast<uint8_t>(Addr::EFLAG));
        Wire.endTransmission();
        Wire.requestFrom(I2CADDR, static_cast<uint8_t>(1));
        return Wire.read();
    }

protected:
    enum {
        NO_AUTO_INCREMENT = 0, AUTO_INCREMENT = 0b10000000,
        AUTO_INCREMENT_BRIGHTNESS = 0b10100000,
        AUTO_INCREMENT_GLOBAL_ONLY = 0b11000000, AUTO_INCREMENT_IND_GLO_ONLY = 0b11100000,
        OSCILLATOR_OFF = 0x10
    };

    enum class Addr {
        MODE1, MODE2, PWM0, PWM1, PWM2, PWM3, PWM4, PWM5, PWM6, PWM7, GRPPWM,
        GRPFREQ,
        LEDOUT0,
        LEDOUT1,
        IREF = 0x12,
        EFLAG = 0x13,
        REG_MAX
    };

    const uint8_t I2CADDR;

};

