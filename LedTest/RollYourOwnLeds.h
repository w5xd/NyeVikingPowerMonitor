    // wire RGB LEDS with R and G matching the RGY connections
    enum class LedChannel {
        // these assignments for LED pinouts that are RAGB on pins 1 through 4 for 
        // SENSE,LOCK,SAMPLE
        SENSE_BLUE = Tlc59108::LED1, SENSE_GREEN = Tlc59108::LED0, SENSE_RED = Tlc59108::LED2,
        LOCK_RED = Tlc59108::LED3, LOCK_BLUE = Tlc59108::LED7,
        SAMPLE_BLUE = Tlc59108::LED4, SAMPLE_RED = Tlc59108::LED5, SAMPLE_GREEN = Tlc59108::LED6,

        // for HIGH, LOW, HOLD, the assignment is RABG
        HOLD_GREEN = Tlc59108::LED4, HOLD_BLUE = Tlc59108::LED5,
        LOW_GREEN = Tlc59108::LED6, LOW_BLUE = Tlc59108::LED7, LOW_RED = Tlc59108::LED3,
        HIGH_RED = Tlc59108::LED2, HIGH_BLUE = Tlc59108::LED0, HIGH_GREEN = Tlc59108::LED1,

        ALO_SENSE = 1 << SENSE_GREEN | 1 << SENSE_BLUE | 1 << SENSE_RED,
        ALO_LOCK = 1 << LOCK_RED | 1 << LOCK_BLUE,
        PEAK_SAMPLE = 1 << SAMPLE_BLUE | 1 << SAMPLE_RED | 1 << SAMPLE_GREEN,
        PEAK_HOLD = 1 << HOLD_GREEN | 1 << HOLD_BLUE,
        RANGE_LOW = 1 << LOW_GREEN | 1 << LOW_BLUE | 1 << LOW_RED,
        RANGE_HIGH = 1 << HIGH_RED | 1 << HIGH_BLUE | 1 << HIGH_GREEN,

        SENSE_UNUSED = SENSE_BLUE
    };
