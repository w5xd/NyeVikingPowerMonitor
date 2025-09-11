# 3D model for work-alike console
The 3D model was done in <a href='http://freecad.org'>FreeCAD</a>. Open console.FCStd to review or modify the model.

If you're going to undertake to modify this model, you will save yourself a lot of waiting for the model
to recompute by temporarily supressing the Pad objects that print the text in both the front panel and in the back panel.
Together, those two take about 45 seconds of recompute time for every single modification I make anywhere in the model.
The Pad objects that print the text are one of the last features in the model. You'll find them.

Most of the modeling in this project is conventional, but with one exception. The pipes that route the optic fiber
threads from the front panel to the LEDs span two different Parts: the sub panel and the front panel. I struggled
in FreeCAD to find a way to share a modeled 3D path for a light pipe across two Parts. Eventually, I gave up sharing. Instead, the
light pipes are modeled in their own part, <i>light pipe design</i>, which is separate from either printable part.
The <i>light pipe design</i> volume spans both parts that will eventually contain the light pipe such that a boolean
intersection and union of the <i>light pipe design</i> with each of the other parts results in a 3D printable part
that can later be attached for a seamless light pipe. FreeCAD shows some bugs in this respect. I found it necessary
to do the intersection operation first, followed by the union. Computationally, either order should give the same
result, but FreeCAD simply gave a fatal error when I first tried the other order.

The file NyeVikingConsoleSTLs.FCMacro contains a FreeCAD macro that invokes the appropriate FreeCAD
primitives to rotate and export STL for the four parts for 3D printing. FreeCAD won't invoke the macro
from the directory it is published here in this repository. You have to bring up FreeCADs Macro menu,
figure out what folder it requires macros to be in, and relocate the FCMacro file there.

<h3>History</h3>
The FreeCAD design published here was not the first. Look back in the git history for details. Earlier designs were
done in Solid Edge, and with a PCB that had SMT LEDs along the edge of the board. I gave up on the SMT LEDs 
because it was just too easy to snap them off when handling the board during assembly. I decided to use
FreeCAD instead of Solid Edge hoping the the open source tool would work. FreeCAD definitely has its bugs,
but it eventually worked as published here.