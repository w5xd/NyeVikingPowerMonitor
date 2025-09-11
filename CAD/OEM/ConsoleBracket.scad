
// not metal, so thicker
BracketPrintThicknessThou = 135; 
// The plastic thickness limits pivot angle w.r.t. OEM metal brack. Make taller to compensate
BoostHeightToIncreasePivotAngleThou = 100; 

// The OEM bracket has two pivot holes. 3D print only the one you select
PivotHolePosition = "bottom"; // [bottom, top]

/* [Console Measurements] */
BracketHeightThou = 3625; 
BracketWidthThou = 8000;    // measured
BracketDepthThou = 3313;    // measured
ConsoleWidthThou = 7563; // measured
// from top of bracket
Hole1ZoffsetThou = 1500; 
// from top of bracket
Hole2ZoffsetThou = 438; 
// OEM console is threaded M5
HoleDiameterThou = 200; 
// OEM is M5
HeadDiameterThou = 380; 
// OEM is M5
HeadThicknessThou = 135; 
PivotThreadRecessDepthThou = 90; // measured

/* [Units for display and STL] */
mmPerThou = .0254; // [0.0254:mm, 1:thou]

module __none__() {}

// measured...
PivotThreadRecessDiameterThou = 457; // measured
BracketTopRoundRadiusThou = 250;    // measured

BracketCompensatedHeightThou = BracketHeightThou + BoostHeightToIncreasePivotAngleThou;

PivotHoleZThou = PivotHolePosition == "bottom" ? (BracketCompensatedHeightThou - Hole1ZoffsetThou) : (BracketCompensatedHeightThou - Hole2ZoffsetThou);



echo ("Console screw head clearance = ", 0.5*(BracketWidthThou - ConsoleWidthThou) - BracketPrintThicknessThou, " thou");

module mount()
{
    difference()
    {
        diam = PivotThreadRecessDiameterThou * 0.9;
        peg = PivotThreadRecessDepthThou + 0.5*(BracketWidthThou - ConsoleWidthThou);
        cylinder(h=peg, d=diam);
        translate([0,0,BracketPrintThicknessThou])
        translate([diam/2,-diam/2])
        rotate([0,-30])
        cube([peg, diam, diam]);
    }
}

scale(mmPerThou)
{
    PivotYPos = BracketDepthThou/2;
    difference()
    {
        union()
        {
            difference()
            {
                profile1=[[0,0], [0,BracketDepthThou], [BracketCompensatedHeightThou, BracketDepthThou], [BracketCompensatedHeightThou,0]];
                profile2=[[0,0], [0,BracketDepthThou], [BracketTopRoundRadiusThou, BracketDepthThou], [BracketTopRoundRadiusThou,0]];
                translate([BracketWidthThou,0,0])
                rotate([0,-90])
                linear_extrude(height=BracketWidthThou)
                {
                    offset(BracketTopRoundRadiusThou)
                    offset(-BracketTopRoundRadiusThou)
                        polygon(profile1);
                    polygon(profile2);
                } 
                translate([BracketPrintThicknessThou, -1, BracketPrintThicknessThou])
                    cube([BracketWidthThou - 2 * BracketPrintThicknessThou, BracketDepthThou+2, BracketCompensatedHeightThou]);
            }

            translate([0, PivotYPos, PivotHoleZThou])
                rotate([0,90])
                    mount();
            translate([BracketWidthThou, PivotYPos, PivotHoleZThou])
                rotate([0,90, 180])
                    mount();

        }

        translate([0, PivotYPos, PivotHoleZThou])
        {
            translate([BracketWidthThou,0])
            rotate([0,-90])
            {
                cylinder(h=BracketWidthThou, d = HoleDiameterThou);
                cylinder(h=HeadThicknessThou, d = HeadDiameterThou);
            }
            rotate([0,90])
                cylinder(h=HeadThicknessThou, d = HeadDiameterThou);
        }  
    }
}
