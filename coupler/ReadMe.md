# Coupler Details
A build-it-yourself coupler schematic is here: <a href="../PCB/schematics.pdf"><img alt='page1' src='../PCB/schematics-1.png'/></a>
 A two layer PCB documented in the PCB folder makes it easy to construct.
The hole pattern for the 1411B aluminum box sides are:
<ul>
<li><a href='Front-template.pdf'>Front-template.pdf</a>
<li><a href='back-template.pdf'>back-template.pdf</a>
<li><a href='bottom-template.pdf'>bottom-template.pdf</a>
</ul>
<a href='../PCB/CouplerPcbMap.pdf'>Map of the coupler PCB</a>
<p>A 0.63 inch (or 16mm) metal punch is recommended for the SO-239 holes, as well as the 4-pin connector hole </p>
<p>The T1 side of the coupler:</p>
<p align='center'><img src='T1-side.jpg' alt='T1-side.jpg' /></p>
<p>The T2 side of the coupler:</p>
<p align='center'><img src='T2-side.jpg' alt='T2-side.jpg' /></p>
The PCB holes for the 40-turn sides of T1 and T2 are such that routing their wires to the
closest holes reverses the Forward and Reflected voltages at the output. Get the right voltages
as marked Forward and Reflected by either
<ul>
<li>winding the two transformers with opposite pitch of each other
<li>...or...
<li>routing the 40-turn side of ONE of T1 or T2 to their more distant holes on the PCB
</ul>
<p>The tape holding the coil windings in place is fiberglass. There is another layer
of white tape holding the RG-213 braid in place.</p>
<p>One end <i>only</i> of the RG-213 braids is grounded.</p>
<p>T2 is held in place mechanically by the leads to its 1-turn winding. Those leads should be 14 gauge.</p>
<p>There is limited clearance inside the aluminum box. The <code>CliffCon-Spacer.stp</code> is a
3D printed spacer that makes more room inside the box to route the wires.</p>
<h2>Theory of Operation</h2>
<p>The easiest model to understand is to model T1 as an ideal current
transformer, and T2 as an ideal voltage transformer. That makes them
a current dependent current source, and a voltage dependent voltage
source, respectively. The
result looks like this:</p>
<p align='center'><img src='coupler-electrical-model.png' alt='coupler-electrical-model'/></p>
<p>Z<sub>0</sub> is 50 ohms resistive in this design. I<sub>G</sub> is the current
into the generator, and V<sub>L</sub> is the voltage across the Load.</p>
<p>The polarity of the two transformers drawn in the PCB circuit board above
is would have this T1 current arrow in the opposite direction, but
the note says to reverse the polarity of either T1 or T2, so we'll chose
to reverse T1 for this model.</p>
<p>Inspecting the model, the voltage equation and the current equation are:
<br/>&nbsp;&nbsp;V<sub>F</sub> = V<sub>L</sub>&nbsp;/&nbsp;N + V<sub>R</sub>
<br/>&nbsp;&nbsp;-I<sub>G</sub>&nbsp;/&nbsp;N&nbsp;=&nbsp;V<sub>F</sub>&nbsp;/&nbsp;Z<sub>0</sub>+&nbsp;V<sub>R</sub>&nbsp;/&nbsp;Z<sub>0</sub>
</p>
<p>Solve for the two dependent voltages:
<br/>&nbsp;&nbsp;V<sub>F</sub>&nbsp;=&nbsp;1/&nbsp;2N&nbsp;(V<sub>L</sub>&nbsp;+&nbsp;I<sub>G</sub>&nbsp;Z<sub>0</sub>)
<br/>&nbsp;&nbsp;V<sub>R</sub>&nbsp;=&nbsp;1/&nbsp;2N&nbsp;(V<sub>L</sub>&nbsp;-&nbsp;I<sub>G</sub>&nbsp;Z<sub>0</sub>)
</p>

<p>Because the SWR is calculated from the detected values of the two voltages, its not possible
to ignore the time domain any further. The SWR equation, in prose, is "the sum of the detected voltages
divided by their difference." In the console, the SWR computation is:
<br/>SWR = ( | V<sub>F</sub> | + | V<sub>R</sub> | ) &divide; ( | V<sub>F</sub> | - | V<sub>R</sub> | )
</p>

<p>To help verify this behavior is what we want in an SWR meter, consider 
if I<sub>G</sub> happens to be matched and thus equal to  V<sub>L</sub>&nbsp;/&nbsp;Z<sub>0</sub> , then
we can see that the forward voltage is 1/N of the load voltage
and the reflected voltage is zero, and the SWR equation results in 1:1. This analysis is not frequency nor time dependent,
but its easy to see that to achieve zero V<sub>R</sub> at the diode detector,
that I<sub>G</sub> must not only have the matching amplitude, but also must
 be in phase with V<sub>L</sub>. That is to say, the
only way to get zero detected reflected voltage is for the load to be Z<sub>0</sub> resistive.</p>
<p>The short and open infinite SWR cases are also correctly covered. If the load is shorted, then V<sub>L</sub> is zero, or
if the load is open, then I<sub>G</sub> is zero. In both of those cases, the other term is non-zero which results in V<sub>F</sub> and V<sub>R</sub> 
having equal magnitudes to each other. When their amplitudes match, the SWR division blows up, which
is correct. (Their phases might not match each other, but the diode detectors
don't detect phase differences between V<sub>F</sub> and V<sub>R</sub>, while they are sensitive to
phase differences between V<sub>L</sub> and I<sub>G</sub>.)</p>
<p>To look at the more general case of arbitrary and possibly reactive load impedance, assume that T2's primary current is 
 neglible, and introduce the load impedance as Z<sub>L</sub> , we can substitute:
<br/>V<sub>L</sub>&nbsp;/&nbsp;Z<sub>L</sub> =&nbsp;I<sub>G</sub></p>
<p>Rearrange:
<br/>&nbsp;&nbsp;V<sub>F</sub>&nbsp;=&nbsp;V<sub>L</sub>&nbsp;/&nbsp;2N&nbsp;(1&nbsp;+&nbsp;Z<sub>0</sub>/Z<sub>L</sub>)
<br/>&nbsp;&nbsp;V<sub>R</sub>&nbsp;=&nbsp;V<sub>L</sub>&nbsp;/&nbsp;2N&nbsp;(1&nbsp;-&nbsp;Z<sub>0</sub>/Z<sub>L</sub>)
</p><p>Substitute for Z<sub>L</sub> :
<br/>SWR = ( |&nbsp1&nbsp;+&nbsp;Z<sub>0</sub>/Z<sub>L</sub>&nbsp| + |&nbsp1&nbsp;-&nbsp;Z<sub>0</sub>/Z<sub>L</sub>&nbsp| ) &divide; ( |&nbsp1&nbsp;+&nbsp;Z<sub>0</sub>/Z<sub>L</sub>&nbsp| - |&nbsp1&nbsp;-&nbsp;Z<sub>0</sub>/Z<sub>L</sub>&nbsp| )
</p>
<p align='center'><img src='coupler-swr.png' alt='coupler-swr.png'/></p>
<p>Consider the case Z<sub>L</sub> is purely reactive, &theta; is 90 degrees, and R+ and R- are equal length and lie
on the Y axis. The denominator of the SWR equation is zero, and the SWR infinite.</p>
<p>If Z<sub>L</sub> is purely resistive, &theta; is 0 degrees, and R+ and R- both lie along the X axis. Define Z
to be the normalized impedance (where Z = 1 means Z<sub>0</sub>=Z<sub>L</sub>), then, for Z > 1, R+ is of length 1 + Z and R- is of length Z - 1, 
and the SWR works out to be Z. Similarly, if Z < 1, the SWR works out to be 1/Z. In prose, for a resistive load impedance, the SWR is the ratio of the load and the characteristic impedance, taking the larger in the numerator.</p>
<p>The diode/capacitor detectors
produce the peak voltage, and have a driving impedance of 50 ohms
from the point of view of the ADC's in the coupler.</p>
<p>The code in the sketch implements the above SWR equation after first implementing
a correction for the non-ideal diode.</p>
