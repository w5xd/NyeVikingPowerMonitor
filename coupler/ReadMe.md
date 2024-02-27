# Coupler Details
The hole pattern for the 1411B aluminum box sides are:
<ul>
<li><a href='Front-template.pdf'>Front-template.pdf</a>
<li><a href='back-template.pdf'>back-template.pdf</a>
<li><a href='bottom-template.pdf'>bottom-template.pdf</a>
</ul>
<p align='center'><img src='T1-side.jpg' alt='T1-side.jpg' /></p>
<p align='center'><img src='T2-side.jpg' alt='T2-side.jpg' /></p>
The PCB holes for the 40-turn sides of T1 and T2 are such that routing their wires to the
closest holes reverses the Forward and Reflected voltages at the output. This may be remedied by either
<ul>
<li>wind the two transformers with opposite pitch of each other
<li>...or...
<li>route the 40-turn side of ONE of T1 or T2 to their more distant holes on the PCB
</ul>

There is limited clearance inside the aluminum box. The <code>CliffCon-Spacer.stp</code> is a
3D printed spacer that makes more room inside the box to route the wires.