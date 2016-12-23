# Nye Viking Power Monitor
Brain transplant for Nye Viking Power Monitor RFM-003

My old RFM-003 quit working. The single circuit board in it is an analog computer that converts 
the two voltages from a directional coupler (forward and reflected) to an SWR meter reading and 
an RF Power meter reading. After switching out the obvious parts, I gave up trying to fix it and
instead bought an Arduino UNO single-board computer and its mating Proto Shield circuit board.
See http://arduino.cc. I built a replacement for the Power Monitor's original circuit board. 
This repo documents the hardware and software used.

Don't know about the Nye Viking Power Monitor? Here is a demonstration: http://www.youtube.com/watch?v=muCM9BKhpKA

# Files
NyeVikingBrain1.png is the circuit diagram of the new interface.
<br/>NyeVikingBrain2.png is the layout of the circuit onto the Proto Shield prototyping circuit board.
<br/>PowerMeter.cpp is the source code.

# Construction
The original instrument has a single circuit board. This project consists of three circuit boards
bolted together. The final assembly is still much smaller than the original. 
The first two are of commercial manufacture:

1 Arduino UNO single-board computer<br/>
2 Arduino PROTO Shield board.<br/>
3 Generic prototyping circuit board.<br/>
<br/>
<p>Board (3) is drilled with mounting hole pattern to match the original RFM-003 board. (In my case,
I only covered the front two mounting posts.) It has the 6 LEDs (SENSE, LOCK, SAMPLE, HOLD, LOW, 
and HIGH) mounted such that they fit through the original RFM front panel holes. This board also has the 220 ohm 
series resistors for the LEDs.</p>
<p>I removed the original board by snipping each wire at its end opposite the circuit board. That is
the old board ends up with lots of flying single-ended wires attached. (EXCEPTION: the Ni-Cd battery 
pack wires I snipped at the circuit board.) I did not scavange any parts from
it and instead bought new LEDs, a relay, etc. The original board, I suppose, could still be repaired and
resinstalled. The new board assembly fits in the position of the old board. I used 22 gauge solid 
hookup wire to connect all the front panel and back panel meters, potentiometer, switches, etc. </p>
<p>
The PROTO shield has all the interfacing parts except the new relay. The UNO is used unmodified. 
This pair connects to each
other with several headers. I ran a pair of 4-40 screws through all three boards and fastened them 
together with nylon 4-40 nuts. I mounted the new relay with double-stick tape to the back panel.
</p>

# Power
I disconnected the old Ni-Cd battery back and require external 12VDC in the new design. A subsequent project
could restore battery power but likely would require a physical modification to the UNO board to remove
or disable its 5VDC linear power regulator. Note that the accuracy of the 5V supply is used for the ADC
converts in this design. I also replaced the front panel incandecent lamps with LED equivalents.

 #Calibration
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
 
 ##Added low-low power feature
 While the code (nearly) duplicates the original behavior of the analog board, there is
 one additional feature. When it detects power levels below 1/10 of full scale, it 
 multiplies the value by 10 and flashes the LOW LED.
 
# RFM-005 support?
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
 in the code could be increased to accomplish that gain change in software, the 3.5V limit
 of the LM324 would prevent the reading of powers above about 3000W.
 
 A better solution would be to leave the code alone, and instead change the
 1M/100K voltage dividers in the input circuit. (There are two--one for forward power
 and the other for reflected.) The 1:11 specified for the RFM-003 should be changed
 by a factor of  SQRT(5000 / 3000), which is 1.29 * 11, which is 14:1. That is,
 the 1M in series with L1/L2 should be increased to 1.3M, leaving the 100K resistor
 unchanged. This change in the voltage divider would reduce the 5000W voltage at the
 LM324 to 3.5V or so, and would reduce the overall system gain such that the meter
 readings should be close enough that the EEPROM calibration included below should
 be able to get the final accuracy to within 5% or so.
 
 The values in the PwmToPwr array would, for the RFM-005, no longer correspond
 to the labeled values on the meter, but the overall system still "works." That is,
 a 300W signal from the 5000W coupler gives a full scale 300W reading on the RFM-003.
 With an RFM-005, and with the 1M resistor (above) changed to 1.3M, a 500W signal
 would be required to deflect the meter full scale, which happens to be labeled 500W.
  
