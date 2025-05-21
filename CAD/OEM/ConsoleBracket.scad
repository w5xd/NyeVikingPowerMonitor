
// not metal, so thicker
BracketPrintThicknessThou = 135; 
// The plastic thickness limits pivot angle w.r.t. OEM metal brack. Make taller to compensate
BoostHeightToIncreasePivotAngleThou = 100; 

// The OEM bracket has two pivot holes. 3D print only the one you select
PivotHolePosition = "bottom"; // [bottom, top]

module __none__() {}

// measured...
Hole1ZoffsetThou = 1500; // from top of bracket
Hole2ZoffsetThou = 438; // ditto
BracketHeightThou = 3625; 
BracketWidthThou = 8000;    // measured
BracketDepthThou = 3313;    // measured
PivotThreadRecessDiameterThou = 457; // measured
PivotThreadRecessDepthThou = 90; // measured
ConsoleWidthThou = 7563; // measured
BracketTopRoundRadiusThou = 250;    // measured
HoleDiameterThou = 200; // OEM console is threaded #8-32
HeadDiameterThou = 380; // ditto
HeadThicknessThou = 135; // ditto

BracketCompensatedHeightThou = BracketHeightThou + BoostHeightToIncreasePivotAngleThou;

PivotHoleZThou = PivotHolePosition == "bottom" ? (BracketCompensatedHeightThou - Hole1ZoffsetThou) : (BracketCompensatedHeightThou - Hole2ZoffsetThou);

mmPerThou = .0254;

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
                translate([BracketPrintThicknessThou, 0, BracketPrintThicknessThou])
                    cube([BracketWidthThou - 2 * BracketPrintThicknessThou, BracketDepthThou, BracketCompensatedHeightThou]);
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
                #cylinder(h=HeadThicknessThou, d = HeadDiameterThou);
        }  
    }
}
