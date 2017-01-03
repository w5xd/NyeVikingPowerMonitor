# Battery Power Branch
As of this writing, this branch in the repository is <b>not tested</b>.
It appears that the function of the 4 cell Ni-Cd battery in the original unit
can be (mostly) restored. But the UNO is probably not going to be the right
CPU board to use for battery power. This branch may eventually be updated to confirm
construction and test details. For now, its a work in progress.

# Nye Viking Power Monitor
Brain transplant for Nye Viking Power Monitor RFM-003

My old RFM-003 quit working. The single circuit board in it is an analog computer that converts 
the two voltages from a directional coupler (forward and reflected) to an SWR reading and 
an RF Power reading. After switching out the obvious parts, I gave up trying to fix it and
instead bought an Arduino UNO single-board computer and its mating Proto Shield circuit board.
See http://arduino.cc. I built a replacement for the Power Monitor's original circuit board. 
This git repo documents the hardware and software used.

Don't know about the Nye Viking Power Monitor? Here is a demonstration
videoed by N8RWS:<br/> http://www.youtube.com/watch?v=muCM9BKhpKA

<h2>Files</h2>
NyeVikingBrain1.png is the circuit diagram of the new interface.
<br/>NyeVikingBrain2.png is the layout of the circuit onto the Proto Shield prototyping circuit board.
<br/>PowerMeter.cpp is the source code.

<h2>Construction</h2>
The original instrument has a single circuit board. This project consists of three circuit boards
held together by a couple of 4-40 machine screws. 
The first two are of commercial manufacture:
<ol> <li>1 Arduino UNO single-board computer.
<li>Arduino PROTO Shield board.
<li>Generic prototyping circuit board.</ol>
The final assembly is still much smaller than the original. 
<p>The generic prototyping board (3) is drilled with mounting hole pattern to match the original RFM-003 board. (In my case,
I only covered the front two mounting posts.) It has the 6 LEDs (SENSE, LOCK, SAMPLE, HOLD, LOW, 
and HIGH) mounted such that they fit through the original RFM front panel holes. This board also has the 220 ohm 
series resistors for the LEDs.</p>
<p>I removed the original board by snipping each wire at its end opposite the circuit board. 
The old board ends up with lots of flying single-ended wires attached. (EXCEPTIONS: the Ni-Cd battery 
pack wires, and the L1/L2 wires from the coupler I snipped at the circuit board.) 
I did not scavange any parts from
it and instead bought new LEDs, a relay, etc. The original board, I suppose, could still be repaired and
resinstalled. The new board assembly fits in the position of the old board. I used 22 gauge solid 
hookup wire to connect all the front panel and back panel meters, potentiometer, switches, etc. </p>
<p>
The PROTO shield has all the interfacing parts except the new relay. The UNO is used unmodified. 
This pair connects to each
other with several headers. I ran a pair of 4-40 screws through all three boards and fastened them 
together with nylon 4-40 nuts. I mounted the new relay with double-stick tape to the back panel.
</p>
<p>Disassembly hint.</p><p>The box splits into a clam shell by removing the four screws from the right hand side,
the four screws from the left hand side, and the outer-most four screws from the back panel. Do NOT remove the
front panel screws nor the bottom panel screws.</p>
<h2>Power</h2>
<p>A prospective builder may want to know that, while the 12VDC connector at the back
of the RFM-003 matches the voltage (about 12V), polarity (positive on the inner pin) and
outer diameter, (5.5mm) of the Arduino, the diameters of the inner pins do NOT match.</p>
<p>
The original battery power design was that the batteries stay permanently connected
and external 12VDC, when applied, trickle charges the NiCd's. I have two reasons to
change that design:</p>
<ol>
<li>Trickle charging will raise the battery voltage above 6VDC, but that number is
the absolute maximum input voltage specified for the LTC3525 part I chose to make
regulated 5VDC from the NiCds.
<li>Continuous charging of NiCd's is not recommended by their manufacturers. They
say to bring them to full charge and then disconnect them.
</ol>
For these reasons the design published here adds an internal DPDT switch that
selects the power source between external 12VDC or the battery pack. The battery
cells must be removed to be charged in an external charger. Going to 
battery power, then, requires a lot more steps than the original design.
<ul><li>Remove the batteries and charge them.
<li>Reinstall the batteries and switch the DPDT switch to battery power.
<li>Run on battery power
<li>Open the box and switch the DPDT switch to 12V external to go back
to external power.
</ul>
<p>There is one
major caveat: the new switch <b>does not prevent both</b> the USB 5V and the battery 5V
from being simulateously applied. Therefore, do <b>not</b> connect the USB cable and
switch to battery power simultaneously. The smoke will likely be released from
the power supply parts, rendering them useless. If you just build from this plan, you will 
only program your Arduino once, it should not be too big a burden to remember
that the one and only time they connect the USB to program it, remove the
battieres!</p>
<p>The battery is converted to 5VDC using an LTC3525 step-up converter. This 
device is limited to 6VDC input, while taking lower-than-5VDC
at its input. The original pack of four AA NiCd cells would nominally be at a
safe
4.8V. Alkaline AA cells <i>cannot</i> be used here in a battery of four, but three of
them in series (or even in parallel!) would work fine with the LTC3525. 
The LTC3525 will drain them all the
way down to below 1V. Here is a commercially available board that has
the LTC3525 along with the other (tiny) parts needed to make
a 5VDC step-up: 
<br><a href='http://moderndevice.com/product/jeelabs-aa-power-board/'>JeeLabs AA Power Board</a>.
It has space for a single AA battery, and this power meter will run on that
single AA cell for a while. Or wire in the original 4 by AA NiCd cells.</p>
<p>For battery operation, a substitution can improve
current draw from the original LM324 op-amp. Substitute an LMC6044 CMOS op-amp, and I
measure a drop in overall current consumption down by about 1mA from before:
from 52 mA down to 51mA when measured in the 12VDC line to the external DC input
and while the software is not in power down mode. The LMC6044 also features
rail-to-rail swing on its output, which opens the possibility for
circuit changes to increase the resolution on the forward and reflected
power digitizing.</p>
<p>Measuring current drain with the UNO makes me believe that long term battery
operation with it is probably not viable. Activation of the power shut down code in
this git branch of
PowerMeter.cpp drops current drain for the assembly (measured with the original LM324 
in place) only from about 45mA down to about 33mA. Divide that 33mA idle
current into the 700mA-hr capacity
of my NiCd's and I only get one day at idle. Actual use to measure power or
SWR will shorten that. </p>
<p>Where is the 33mA going? There is almost
certainly a posting on an arduino forum somewhere with the answer. I
haven't run across it, though. And I am not willing to go
after my UNO with a soldering iron to confirm any of this. For my own tastes, in the 
absence of an external on/off switch, I would want at least a month of
idling available for the battery. That means another order of magnitude
of consumption must be found and removed. Any further experiments to
reduce idling current will need to be done on a different Arduino board.</p>
 <h2>Calibration</h2>
 <p>The code supports four settings in EEPROM. These (roughly) correspond to 
 potentiometers on the original analog board. The EEPROM settings are:
 <br/>ALO SWR lock-out threshold
 <br/>ALO PWR lock-out threshold
 <br/>Foward voltage calibration correction (+/-10%) range
 <br/>Reflected voltage calibration correction (+/-10%) range
 </p><p>
 Setting the EEPROM values is accomplished using a magic switch switch sequence 
 followed by turning the front panel HOLD pot. See the code for full instructions.
 </p>
 
 <h2>Added low-low power feature</h2>
 While the code (nearly) duplicates the original behavior of the analog board, there is
 one additional feature. When it detects power levels below 1/10 of full scale, it 
 multiplies the value by 10 and flashes the LOW LED.
 
 <h2>Hardware changes from the no-battery branch</h2>
 The differences in the circuit design for this battery enabled design,
 compared to the original, external power only design, are:
 <ol>
 <li> A battery module is added. 
  <li>A new switch is added to the back panel. Use the existing .25" hole that
 allowed access to the old ALO pot. Use a momentary SPST NC switch. 
 Wire that new switch to pull D3 down.
 <li>An internally accessible DPDT switch is added to prevent simultaneous
 operation of the external 12VDC and internal battery back power supplies.
 <li>The ALO TRIP SWR/REV function that used to be on D3 is now wired to A0.
 <li>Substitute an LMC6044 CMOS op-amp for the LM324. Its output swings rail-to-rail and 
 it requires much reduced power supply current.
 <li>The input network becomes 1:7.6@500Hz for both forward and reflected. 
 This has a little less noise, and a little better resolution--taking advantage
 of the rail-to-rail output voltage swing of the LMC6044.
 <li>The ALO lockout relay with a 3VDC coil is an unnecessary battery drain
 compared to an equivalent part with a 5VDC coil.
 The only reason mine is 3V is because Fry's did not have a 5V part in stock,
 and because I don't really care much about battery operation.
 <li>Of course, if you modify your hardware per this battery-power branch, you must also
 upload the program as compiled from this branch.
 </ol> 
<h2>RFM-005 support? </h2>
 The difference between the two meters, according to the schematic in their (common) manual,
 is that the former has a full scale power meter reading of 300 vs 500 in the latter.
 
 Without an RFM-005 to test with, the following is speculation.
 
 Their circuit diagrams are published to be identical, therefore the only published
 distinction between it and the RFM-003 is the meter marking--no components are
 marked different on their circuit diagrams. There are several unlabeled potentiometers
 that presumably are adjusted differently between the two. P3 sets the overall scale
 factor for power readings. Use of this code with the RFM-005 assumes that P3
 was adjusted differently at the factory for the RFM-003 vs RFM-005.
 
 There are also two different couplers, but the documentation indicates they are
 interchangeable. I tested only with the "K" model with 5000W limit  as opposed to
 the "C" model with its 500W limit (C-1.8-30K versus C-1.8-30C.) Again speculation,
 but the code below should work with either coupler without change. The full
 scale readings would simply be a factor of ten lower for the lower power coupler.
 
 To use this Arduino circuit and code with the RFM-005, more speculation.
 
 Since the couplers are documented as "interchangeable" I assume the voltages
 coupler connector are, for some power in watts, the same voltage in volts
 regardless of which meter full scale you have. That is, 26V is 1500W (as I measured)
 regardless which meter you happen to have. That would mean that support of the
 RFM-005 needs only an overall gain factor change. While the NominalCouplerResistance
 in the code could be increased to accomplish that gain change in software, a 5000W
 signal will overflow the ADC for the forward power such that the 3000W limit
 of the RFM-003 would also apply to the RFM-005.
 
 A better solution would be to leave the code alone, and instead change the
 1M/150K voltage dividers in the input circuit. (There are two--one for forward power
 and the other for reflected.) The 1:7.6 ration specified for the RFM-003 should be changed
 by a factor of  SQRT(5000 / 3000), which is 1.29 * 7.6, which is (about) 10:1. That is,
 the 1M in series with L1/L2 should be increased to 1.3M, leaving the 150K resistor
 unchanged. This change in the voltage divider would reduce the 5000W voltage at pin 12
 of the op-amp to 4.8V or so, and would reduce the overall system gain such that the meter
 readings should be close enough that the EEPROM calibration included below should
 be able to get the final accuracy to within 5% or so.
 
 The values in the PwmToPwr array would, for the RFM-005, no longer correspond
 to the labeled values on the meter, but the overall system still "works." That is,
 a 300W signal from the 5000W coupler gives a full scale 300W reading on the RFM-003.
 With an RFM-005, and with the 1M resistor (above) changed to 1.3M, a 500W signal
 would be required to deflect the meter full scale, which happens to be labeled 500W.
 
 The Forward/Reflected calibration step centers around the 100W meter reading on the RFM-003.
 On the RFM-005, it would center around
 <br/>(SQRT(100) * SQRT(500) / SQRT (300)) ** 2 = 167W
  
