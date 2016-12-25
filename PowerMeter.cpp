// Do not remove the include below
#include "Arduino.h"
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <EEPROM.h>

//#define DEBUG_SERIAL

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
								// 3000W is 384,000 -- which requires 32 bits to describe

// pin assignments
const int coupler7dot5VPinIn =  2;
const int ALOtripSwitchPinIn = 3;
const int PowerForwReflSwitchPinIn = 4;
const int SwrMeterPinOut = 5;
const int RfMeterPinOut = 6;
const int SenseLedPinOut = 7;
const int AloLockPinOut = 13;
const int SampleLedPinOut = 9;
const int HoldLedPinOut = 10;
const int LowLedPinOut = 11;
const int HighLedPinOut = 12;
const int PanelLampsPinOut = 8;

const int ReversePwrAnalogPinIn = A1;
const int ForwardPwrAnalogPinIn = A2;
const int HoldTimePotAnalogPinIn = A3;
const int AverageSwitchPinIn = A4;
const int PeakSwitchPinIn = A5;

const int MAXED_ADC = 1022;
const int MAXED_HOLD_POT = 710; // lm324 can't go full scale
const int SWR_SCALE_PWR = 7;
const uint16_t SWR_SCALE = 1 << SWR_SCALE_PWR; 	// 128
const long INFINITE_SWR = 100;

// This is the "display" unit for power: watt times 128
const int PWR_SCALE_PWR = 7;
const uint16_t PWR_SCALE = 1 << PWR_SCALE_PWR;	// 128

const int PWM_MAX_PWR = 8;
const int PWM_MAX_PLUS1 = 1 << PWM_MAX_PWR;

const unsigned PWR_BREAKTOHIGH_POINT = 250u * PWR_SCALE;
const unsigned PWR_BREAKTOLOWLOW_POINT = 20u * PWR_SCALE;
const unsigned ADC_DISCARD = 50; // 5.6 msec

const long TimerLoopIntervalMicroSec = 1500; // 670KHz. analog input filter is 750Hz.
const unsigned MeterUpdateIntervalMsec =	125; // 8Hz
const unsigned long FrontPanelLampsOnMsec = 90000; // stay on 90 seconds per RFM documentation
const unsigned long HoldPwrLampsOnMsec = 500;
const unsigned HoldHighLedMsec = 400;
const unsigned long LockoutLengthMsec = 5000;

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
	 * On the back panel, simultaneously switch BOTH the ALO and POWER switches
	 * three times within one second. The meter responds by setting the LOCK LED.
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
	 * Power accuracy depends on the accuracy of the 5VDC supply and the 1.1V internal
	 * ADC reference. If they are not in the ration 50/11, then low power readouts
	 * are not calibrated consistently with higher.
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

void setup() {
	// Digital pins (except PWM output pins)
    pinMode(coupler7dot5VPinIn, INPUT_PULLUP);
    pinMode(ALOtripSwitchPinIn, INPUT_PULLUP);
    pinMode(PowerForwReflSwitchPinIn, INPUT_PULLUP);
    pinMode(SenseLedPinOut, OUTPUT);
    pinMode(AloLockPinOut, OUTPUT);
    pinMode(SampleLedPinOut, OUTPUT);
    pinMode(HoldLedPinOut, OUTPUT);
    pinMode(LowLedPinOut, OUTPUT);
    pinMode(HighLedPinOut, OUTPUT);
    pinMode(PanelLampsPinOut, OUTPUT);

    pinMode(PeakSwitchPinIn, INPUT_PULLUP);
    pinMode(AverageSwitchPinIn, INPUT_PULLUP);

    pinMode(ReversePwrAnalogPinIn, INPUT);
    pinMode(ForwardPwrAnalogPinIn,INPUT);
    pinMode(HoldTimePotAnalogPinIn,INPUT);

    analogReference(DEFAULT); // go to 5.0V reference
    movingAverage::clear();

    calibrate::SetCalibrationConstantsFromEEPROM();

#if defined(DEBUG_SERIAL)
    Serial.begin(9600);
#endif
}

#if defined(DEBUG_SERIAL)
static bool printDebug; // flag that gets set once when a character comes in on the Serial
#endif

namespace Alo {
	void CheckAloPwr();
	void CheckAloSwr(uint8_t);
	void doAloSetup();
}

namespace {
	void sample();
	uint8_t DisplaySwr();
	void FrontPanelLamps();
	enum SetupMode_t {METER_NORMAL, ALO_SETUP, CALIBRATE_SETUP};
	enum EEPROM_ASSIGNMENTS {EEPROM_SWR_LOCK, EEPROM_PWR_LOCK, EEPROM_FWD_CALIBRATION, EEPROM_REFL_CALIBRATION};

	// Voltages are in acquisition units.
	// Max is 50 * ADC max, which is 2**10.
	// (Actually, on the 5V reference, the lm324 limits the
	// data to about 3.7V, so our max is not 2**10 = 1024, but about 710 or so
	//
	DisplayPower_t getPeakPwr();
	DisplayPower_t getPeakHoldPwr();
	DisplayPower_t getAveragePwr();
	void DisplayPwr(DisplayPower_t);

	const unsigned NominalCouplerResistance = 3353u; // my own meter gives zero calibration correction with this
	const uint32_t NominalCouplerResistanceMultiplier = 0x20000u;
	const unsigned NominalCouplerResistanceRecip =
			NominalCouplerResistanceMultiplier / NominalCouplerResistance;

	unsigned long previousMicrosec;
	unsigned long SwrUpdateTime;
	unsigned long coupler7dot5LastHeardMillis = 0;
	unsigned HoldTimePot;
	unsigned long HoldPeakRecordedAtMillis;
	bool BackPanelPwrSwitchFwd;
	bool BackPanelAloSwitchSwr;
	unsigned long LockoutStartedAtMillis;

	enum SetupMode_t MeterMode(METER_NORMAL);

	// Alo/calibrate Setup Mode detect
	const unsigned AloEntryDeadlineMsec = 1000;
	const unsigned AloSetupModeTimesOut = 30000;
	unsigned AloSwitchChangeCount;
	unsigned PwrSwitchChangeCount;
	unsigned long CaliSwitchChangeTime;
	unsigned long EnteredAloSetupModeTime;
	bool BackPanelAloSwitchSwrPrev;
	bool BackPanelPwrSwitchFwdPrev;

	AcquiredVolts_t fwdCalibration = 0x8000; // this fixed point 1.0 multiplier
	AcquiredVolts_t revCalibration = 0x8000; // ditto

	uint32_t calibrateScaleFwd(uint32_t v)
	{
		v *= revCalibration;
		v /= 0x8000u;
		return v;
	}

	AcquiredVolts_t calibrateFwd(AcquiredVolts_t v)
	{
		return (AcquiredVolts_t) calibrateScaleFwd(v);
	}

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
	{
		return (AcquiredVolts_t) calibrateScaleRev(v);
	}

	DisplayPower_t calibrateRevPower(DisplayPower_t w)
	{
		uint64_t t = w;
		t *= revCalibration;
		t *= revCalibration;
		t /= 0x8000u;
		t /= 0x8000u;
		return (DisplayPower_t)t;
	}
}

void loop()
{
  // throttle to one loop every TimerLoopIntervalMicroSec
  previousMicrosec = micros();

  sample(); // read FWD/REFL ADCs

  BackPanelPwrSwitchFwd = digitalRead(PowerForwReflSwitchPinIn) == HIGH;
  BackPanelAloSwitchSwr = digitalRead(ALOtripSwitchPinIn) == HIGH;

  unsigned long now = millis();

  // CHECK ENTER CALIBRATE MODE
  if (MeterMode == METER_NORMAL && digitalRead(PeakSwitchPinIn) != LOW)
  {
	  if (BackPanelPwrSwitchFwd != BackPanelPwrSwitchFwdPrev)
		  PwrSwitchChangeCount += 1;
	  if (BackPanelAloSwitchSwrPrev != BackPanelAloSwitchSwr)
		  AloSwitchChangeCount += 1;
	  if (AloSwitchChangeCount + PwrSwitchChangeCount == 1)
		  CaliSwitchChangeTime = now;
	  if (now - CaliSwitchChangeTime < AloEntryDeadlineMsec)
	  {
		  if ((PwrSwitchChangeCount >= 3) && (AloSwitchChangeCount >= 3))
		  {
			  MeterMode = (digitalRead(AverageSwitchPinIn) == LOW) ?
					  ALO_SETUP : CALIBRATE_SETUP;
			  EnteredAloSetupModeTime = now;
			  AloSwitchChangeCount = 0;
			  PwrSwitchChangeCount = 0;
		  }
	  }
	  else
	  {
		  AloSwitchChangeCount = 0;
		  PwrSwitchChangeCount = 0;
	  }

	  BackPanelPwrSwitchFwdPrev = BackPanelPwrSwitchFwd;
	  BackPanelAloSwitchSwrPrev = BackPanelAloSwitchSwr;
  }

  if ((digitalRead(coupler7dot5VPinIn) == LOW) ||
		  (MeterMode != METER_NORMAL))
  {
	  coupler7dot5LastHeardMillis = now;
	  digitalWrite(PanelLampsPinOut,	HIGH);
  }

  // dispatch per MeterMode
  if (MeterMode == ALO_SETUP)
  {
	  Alo::doAloSetup();
	  if (MeterMode != ALO_SETUP)
	  {
			digitalWrite(SenseLedPinOut, LOW);
			digitalWrite(AloLockPinOut, LOW);
	  }
	  return;
  }
  else if (MeterMode == CALIBRATE_SETUP)
  {
	  calibrate::doCalibrateSetup();
	  if (MeterMode != CALIBRATE_SETUP)
			digitalWrite(AloLockPinOut, LOW);
	  return;
  }

  if (digitalRead(AloLockPinOut) == HIGH &&
		  now - LockoutStartedAtMillis > LockoutLengthMsec)
  {
	 digitalWrite(AloLockPinOut, LOW);
  }

  // Update the displays less frequently than loop() can excute
  if (now - SwrUpdateTime >= MeterUpdateIntervalMsec)
  {
#if defined(DEBUG_SERIAL)
	  if (Serial.available() > 0)
	  {
		  Serial.read();
		  printDebug = true;
	  }
#endif
	  SwrUpdateTime = now;
	  FrontPanelLamps();
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
  }

  unsigned long nowusec = micros();
  long diff = nowusec - previousMicrosec;
  diff -= TimerLoopIntervalMicroSec;
  if ((diff < 0) && (diff >= -TimerLoopIntervalMicroSec))
	  delayMicroseconds((unsigned int)-diff);
#if defined(DEBUG_SERIAL)
  printDebug = false;
#endif
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
	// ten times the voltage reference that we used to acquire it (50 or 11)
	// The ADC is 10 bits, so multiplying by 50 still keeps us within 16 bit unsigned

	// NOTE. The Arduino compiler development environment did NOT generate any errors
	// when these arrays were only a "little" too big to fit in 2KB SRAM.
	// That just makes the Arduino crash mysteriously when you run this sketch...

	AcquiredVolts_t fwdHistory[NUM_TO_AVERAGE]; // units are ADC converter units
	AcquiredVolts_t revHistory[NUM_TO_AVERAGE];
	uint64_t fwdTotal;
	uint64_t revTotal;

	class AvgSinceLastCheck
	{
	public:
		AvgSinceLastCheck() : lastIndex(0){}

		// expect to be called at meter update frequency: 8Hz
		void getCalibratedSums(uint32_t &f, uint32_t &r)
		{
			f = 0; r = 0;
			// backwards from newest sample
			// stop at sample we used last time called
			unsigned count = 0;
			for (int i = curIndex; i != lastIndex;  )
			{
				f += fwdHistory[i];
				r += revHistory[i];
				count += 1;
				if (--i < 0)
					i = NUM_TO_AVERAGE - 1;
			}
			// optimize the divide by count to nearby power of two
			// Only the ratio of f and r will be used later
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
	void getPeaks(AcquiredVolts_t &f, AcquiredVolts_t &r)
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
	const uint16_t PwmToSwr[NUM_PWM] PROGMEM = // PROGMEM puts the array in program memory
	{
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
			12800, // 100 <-- last entry to use--full scale on meter
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
	const unsigned HoldADCDefaultMsec = 1000; // stay in high power mode for a full second
	const unsigned INTERNAL_MIN = 4; // 1.1V reference gives a couple of counts for "zero" power

	// The 3000W coupler has Schottkey barrier diodes
	const unsigned SchottkeyBarrier = 34 * 11; // 400 mV through 100K/1M divider is 36mV...
	// ... would give count of 34  out of 1024 on 1.1V reference
	// ... scale by 11 reference
	// Therefore these are our "acquisition" units for power

	AcquiredVolts_t fwdHires; // ADC converter units: 50 * 5V ref and 11 * 1.1V ref
	AcquiredVolts_t revHires;

	enum {ADC_DEFAULT, ADC_INTERNAL} AdcMode(ADC_DEFAULT);
	unsigned long AdcModeSwitchedTime;

uint16_t setAnalogReferenceDefault()
{
	analogReference(DEFAULT); // go to 5 V reference
	uint16_t ret;
	for (unsigned i = 0; i < ADC_DISCARD; i++)
		ret = analogRead(HoldTimePotAnalogPinIn); // 100 usec. user is advised to ignore
	AdcMode = ADC_DEFAULT;
	return ret;
}

void setAnalogReferenceInternal()
{
	analogReference(INTERNAL); // go to 1.1 V reference
	for (unsigned i = 0; i < ADC_DISCARD; i++)
		analogRead(HoldTimePotAnalogPinIn); // 100 usec. user is advised to ignore
	AdcMode = ADC_INTERNAL;
}

void sample()
{
	if ((AdcMode == ADC_DEFAULT) && millis() - AdcModeSwitchedTime > HoldADCDefaultMsec)
		setAnalogReferenceInternal();// switching references empirically determined takes more than 5msec

	/* SWR should be calculated with coincident FWD and REV measurements.
	 * But we have to digitize them serially. There will always be at least
	 * 100uSec of clock skew between the two measurements. FWD will (almost)
	 * always be the larger, so read it first, and in the low range.
	 * The analog filters on the channels are low pass, single pole at 500Hz
	*/

	if (AdcMode == ADC_INTERNAL)
	{
		fwdHires = analogRead(ForwardPwrAnalogPinIn); // 100 usec
		if (fwdHires <= INTERNAL_MIN)
			fwdHires = 0;
		revHires = analogRead(ReversePwrAnalogPinIn); // 100 usec
		if (revHires <= INTERNAL_MIN)
			revHires = 0;

		bool SwitchAdcMode(false);
		if (fwdHires >= MAXED_ADC)
			SwitchAdcMode = true;
		else
			fwdHires *= 11;	// unit convert from 1.1V ref

		if (revHires >= MAXED_ADC)
			SwitchAdcMode = true;
		else
			revHires *= 11;

		if (SwitchAdcMode)
		{
			// This switchover happens at 1.1V on the ADC, which is 12V from the coupler.
			// 12V corresponds to about 400W.
			// Once either FOR or REFL causes this switch, the other is read at the
			// same resolution until the power level is reduced.
			setAnalogReferenceDefault();
			AdcModeSwitchedTime = millis();
		}
	}

	if (AdcMode == ADC_DEFAULT)
	{
		fwdHires = analogRead(ForwardPwrAnalogPinIn); // 100usec
		revHires = analogRead(ReversePwrAnalogPinIn); // 100usec
		// keep extending the DEFAULT time as we read values above 1.1V
		const unsigned V11inDefault = -10l + (1100l * 1024l) / 5000l;
		if ((fwdHires > V11inDefault) ||
				(revHires > V11inDefault))
			AdcModeSwitchedTime = millis();
		fwdHires *= 50; // unit convert from 5V ref
		revHires *= 50;
	}

	// ranges up to 50 * 1024 = 5.0V or
	//              11 * 1024 = 1.1V

	// the coupler has schottkey barrier diodes, which limit to about 400mV
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
		FLASH(const uint16_t *pgm) : addr((uint16_t)pgm){}
		FLASH( const FLASH &other) : addr(other.addr){}
		uint16_t operator[] (int i) const
		{	return pgm_read_word_near(addr + i * sizeof(uint16_t));		}
	private:
		uint16_t addr;
	};
	static unsigned map(unsigned i)	{ return i << (PWM_MAX_PWR - PWRENTRIES); }
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

uint8_t DisplaySwr()
{
	static movingAverage::AvgSinceLastCheck average;
	unsigned long displayValue = 1;
	uint32_t f;
	uint32_t r;
	average.getCalibratedSums(f,r);
	if (f)
	{
		if (r < f)
		{
			// SWR = (f + r) / (f - r) -- all in volts (not power!)
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
	uint16_t swrCoded = (uint16_t)displayValue;
	uint16_t v = MeterInvert<SwrMeter::PWR_ENTRIES>::TableLookup(swrCoded, SwrMeter::PwmToSwr);
	v = MeterInvert<SwrMeter::PWR_ENTRIES>::map(v);
	static MeterFilter meterFilter;	// don't jerk the meter around too quickly
	analogWrite(SwrMeterPinOut, meterFilter.apply(v));
	return (uint8_t)v;
}

void FrontPanelLamps()
{
	if (digitalRead(PanelLampsPinOut) == HIGH)
	{
		unsigned long timeOn = millis() - coupler7dot5LastHeardMillis;
		if ( timeOn > FrontPanelLampsOnMsec)
			digitalWrite(PanelLampsPinOut,	LOW);
	}
}

DisplayPower_t SquareToWatts(DisplayPower_t s)
{
	uint64_t ret = s;
	ret *= NominalCouplerResistanceRecip; // really is a multiply
	ret /= NominalCouplerResistanceMultiplier;	// implemented as logical shift
	return ret;
}

DisplayPower_t VoltsToWatts(AcquiredVolts_t v)
{
	// 3000 W is about 700 ADC count at 5V, which is 35,000
	DisplayPower_t w = v;
	w *= w;
	return SquareToWatts(w);
}

DisplayPower_t getPeakPwr()
{
	AcquiredVolts_t f;
	AcquiredVolts_t r;
	movingAverage::getPeaks(f,r);
	DisplayPower_t ret = VoltsToWatts( BackPanelPwrSwitchFwd ? calibrateFwd(f) : calibrateRev(r));
	static DisplayPower_t prev;
	digitalWrite(SampleLedPinOut, ret != 0 && prev != ret && digitalRead(SampleLedPinOut)==LOW
				? HIGH : LOW);
	digitalWrite(HoldLedPinOut, LOW);
	prev = ret;
	HoldTimePot = 0;
	return ret;
}

DisplayPower_t getPeakHoldPwr()
{
	// calculate the peak
	AcquiredVolts_t f;
	AcquiredVolts_t r;
	movingAverage::getPeaks(f,r);
	DisplayPower_t ret = VoltsToWatts( BackPanelPwrSwitchFwd ? calibrateFwd(f) : calibrateRev(r));

	static DisplayPower_t peakHold;
	unsigned long now = millis();
	if ((ret > peakHold) || (now - HoldPeakRecordedAtMillis > HoldTimePot))
	{
		HoldPeakRecordedAtMillis = now;
		digitalWrite(SampleLedPinOut,  ret > 0 ? HIGH : LOW);
		digitalWrite(HoldLedPinOut, ret > 0 ? HIGH : LOW);
		peakHold = ret;
	}
	else
	{
		digitalWrite(SampleLedPinOut,  LOW);
		ret = peakHold;
	}

	// Read the hold pot
	uint16_t holdPot = (uint16_t)analogRead(HoldTimePotAnalogPinIn);
	if ((holdPot >= MAXED_ADC) && AdcMode == ADC_INTERNAL)
		holdPot = setAnalogReferenceDefault();
	holdPot *= AdcMode == ADC_DEFAULT ? 50 : 11;
	// range is 0 to 50 * 1024 = 51000
	// lets make 51000 map to 25 seconds, which is 25000 msec
	HoldTimePot = holdPot / 2;
	return ret;
}

DisplayPower_t getAveragePwr()
{
	digitalWrite(HoldLedPinOut, LOW);
	digitalWrite(SampleLedPinOut,  LOW);
	HoldTimePot = 0;
	return SquareToWatts(BackPanelPwrSwitchFwd ?
			  	  	  	  	  	  calibrateFwdPower(movingAverage::fwdPwr()) :
								  calibrateRevPower(movingAverage::revPwr()));
}

namespace PwrMeter {
/*
 * 	PWM values are from 0 to 255, which correspond to zero and
 * 	full output, which is MORE than full scale.
 * 	We only have NUM_PWM values out of 256:
 * 		search this table for the nearest value to display
 * 		use the index in this table to write analogWrite, but multiply by
 * 		128  first.
 *
 * 		Entries in this table are Power * PWR_SCALE
 */
    const int PWR_ENTRIES = 8;
	const int NUM_PWM = 1 << PWR_ENTRIES;
	const int LOWEST_VALID_CALIBRATION = 100;
	const int HIGHEST_VALID_CALIBRATION = 207;
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
			38481, // 300.633
			38844, // 303.47
			39209, // 306.32
			39575, // 309.183
			39944, // 312.059
			40314, // 314.949
			40685, // 317.852
	};
}

void DisplayPwr(DisplayPower_t v)
{
	static unsigned long lastHighTime;
	static unsigned long lastNormalTime;
	static unsigned long lastOnTime;
	uint16_t toDisplay = v;

	unsigned long now = millis();
	if (v == 0)
	{
		unsigned long timeOn = now - lastOnTime;
		if ((timeOn > HoldPwrLampsOnMsec) && (timeOn > HoldTimePot))
		{
			digitalWrite(LowLedPinOut, LOW);
			digitalWrite(HighLedPinOut, LOW);
		}
	}
	else
	{
		lastOnTime = now;
		if (v > PWR_BREAKTOHIGH_POINT)
		{
			lastHighTime = now;
			toDisplay = v / 10;
			digitalWrite(LowLedPinOut, LOW);
			digitalWrite(HighLedPinOut, HIGH);
		}
		else if (digitalRead(HighLedPinOut) == HIGH)
		{
			if (now - lastHighTime > HoldHighLedMsec)
			{
				lastNormalTime = now;
				digitalWrite(LowLedPinOut, HIGH);
				digitalWrite(HighLedPinOut, LOW);
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
				digitalWrite(HighLedPinOut, LOW);
				toDisplay *= 10;
				// blink the low power LED
				digitalWrite(LowLedPinOut,
						digitalRead(LowLedPinOut) == HIGH ? LOW : HIGH);
			}
		}
		else
		{
			lastNormalTime = now;
			digitalWrite(LowLedPinOut, HIGH);
			digitalWrite(HighLedPinOut, LOW);
		}
	}
	uint16_t m = MeterInvert<PwrMeter::PWR_ENTRIES>::TableLookup(toDisplay, PwrMeter::PwmToPwr);
	v = MeterInvert<PwrMeter::PWR_ENTRIES>::map(m);
	static MeterFilter meterFilter;
	analogWrite(RfMeterPinOut, meterFilter.apply(v));
}
}

namespace Alo {

void Lockout(DisplayPower_t p)
{
	static const uint32_t LockoutThreshold =	25672; // 200W
	// only turn on SENSE light until foward power exceeds 200W
	if (p >= LockoutThreshold )
	{
		LockoutStartedAtMillis = millis();
		digitalWrite(AloLockPinOut, HIGH);
	}
}

void CheckAloSwr(uint8_t swr)
{
	uint8_t aloLimit = EEPROM.read((int)EEPROM_SWR_LOCK);
	if (swr >= aloLimit)
	{
		digitalWrite(SenseLedPinOut, HIGH);
		AcquiredVolts_t f;
		AcquiredVolts_t r;
		movingAverage::getPeaks(f,r);
		Lockout(VoltsToWatts(calibrateFwd(f)));
	}
	else
		digitalWrite(SenseLedPinOut, LOW);
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
		digitalWrite(SenseLedPinOut, HIGH);
		Lockout(VoltsToWatts(calibrateFwd(f)));
	}
	else
		digitalWrite(SenseLedPinOut, LOW);
}

// meter is in calibrate mode,
// ALO lock out settings from HOLD pot
void doAloSetup()
{
	static uint8_t swr;
	static uint8_t pwr;
	if (digitalRead(PeakSwitchPinIn) == LOW)
	{	// at top of function, but happens last...
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

	digitalWrite(SenseLedPinOut, HIGH);
	digitalWrite(AloLockPinOut, HIGH);

	if (digitalRead(AverageSwitchPinIn) == HIGH)
	{	// read EEPROM settings
		analogWrite(SwrMeterPinOut, EEPROM.read((int)EEPROM_SWR_LOCK));
		analogWrite(RfMeterPinOut, EEPROM.read((int)EEPROM_PWR_LOCK));
		digitalWrite(LowLedPinOut, HIGH);
		digitalWrite(HighLedPinOut, LOW);
		return;
	}

	if (AdcMode != ADC_DEFAULT)
		setAnalogReferenceDefault();

	int holdPot = analogRead(HoldTimePotAnalogPinIn);
	holdPot /= 2;
	if (holdPot > 255)
		holdPot = 255;

	static bool lastAdjust;
	bool adjust = lastAdjust;
	if (BackPanelAloSwitchSwr)
	{
		analogWrite(SwrMeterPinOut, holdPot);
		swr = holdPot;
		digitalWrite(LowLedPinOut, LOW);
		digitalWrite(HighLedPinOut, LOW);
		lastAdjust = false;
	}
	else
	{
		analogWrite(RfMeterPinOut, holdPot);
		pwr = holdPot;
		digitalWrite(LowLedPinOut, HIGH);
		digitalWrite(HighLedPinOut, LOW);
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
		return c * 80;
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

	digitalWrite(AloLockPinOut, HIGH);

	analogWrite(SwrMeterPinOut, 0);

	if (digitalRead(AverageSwitchPinIn) == HIGH)
	{	// read EEPROM settings
		analogWrite(RfMeterPinOut, EEPROM.read(
				BackPanelPwrSwitchFwd ? EEPROM_FWD_CALIBRATION : EEPROM_REFL_CALIBRATION));
		return;
	}
	else
	{
		if (AdcMode != ADC_DEFAULT)
			setAnalogReferenceDefault();

		int holdPot = analogRead(HoldTimePotAnalogPinIn);
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
