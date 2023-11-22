using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Drawing.Imaging;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;

/*
** This program draws meter scales onto a bitmap that can then be printed and used behind an analog
** meter movement to show RF Power and SWR.
*/

namespace MeterFaces
{
    public partial class MeterFacesForm : Form
    {
        public MeterFacesForm()
        {
            InitializeComponent();
        }
        private System.Drawing.Bitmap myBitmap; // Our Bitmap declaration

        private void Form1_Paint(object sender, PaintEventArgs e)
        {
            Graphics graphicsObj = e.Graphics;
            graphicsObj.DrawImage(myBitmap, 0, 0, myBitmap.Width, myBitmap.Height);
            graphicsObj.Dispose();
        }

        private static ImageCodecInfo GetEncoderInfo(string mimeType)
        {
            foreach (ImageCodecInfo codec in ImageCodecInfo.GetImageEncoders())
                if (codec.MimeType == mimeType)
                    return codec;

            return null;
        }

        private static float Bitmap_DPI_X_and_Y = 1200; // on screen is NOT to scale, but the jpg save prints "actual size" at this resolution.
        private static float SCANNING_DPI = 600;
        private static int ScanPixelsToBitmapPixels(int scanPixels)
        {
            return (int)(0.5 + scanPixels * Bitmap_DPI_X_and_Y / SCANNING_DPI);
        }

        static int axisX = ScanPixelsToBitmapPixels(762);
        static int axisY = ScanPixelsToBitmapPixels(970);
        static int meterRadius = ScanPixelsToBitmapPixels(750);

        static int meterLimitMinX = ScanPixelsToBitmapPixels(160);
        static int meterLimitMaxX = ScanPixelsToBitmapPixels(1364);
        static int meterLimitsY = ScanPixelsToBitmapPixels(370);

        static double pi = Math.Atan(1) *  4;

        bool MeterTickPositionFromValue(double fraction, int tickLen, out Point p)
        {
            double radius = meterRadius - tickLen;
            double angle = (-fraction - 0.5) * pi * 0.5;
            double deltaX = Math.Cos(angle) * radius;
            double deltaY = Math.Sin(angle) * radius;
            double meterCircleOriginX = axisX - radius;
            double meterCircleOriginY = axisY - radius;
            p = new Point((int)(axisX - deltaX), (int)(axisY + deltaY));
            return true;
        }

        private void MeterFaceTicks(int nTics)
        {
            System.Drawing.Graphics graphicsObj = Graphics.FromImage(myBitmap);
            Pen myPen = new Pen(System.Drawing.Color.Black, 6);
            for (int i = 0; i <= nTics; i += 1)
            {
                float frac = i;
                frac /= nTics;
                Point p; Point p2;
                MeterTickPositionFromValue(frac, 0, out p);
                MeterTickPositionFromValue(frac, ScanPixelsToBitmapPixels(25), out p2);
                graphicsObj.DrawLine(myPen, p, p2);
            }
        }

        private static int TickFontSize = ScanPixelsToBitmapPixels(65);
        private static int LabelFontSize = ScanPixelsToBitmapPixels(85);
        private int LabelOutsideRadius = ScanPixelsToBitmapPixels(100);
        private int TickTextPixelWidth = ScanPixelsToBitmapPixels(400);
        private int TickTextPixelHeight = ScanPixelsToBitmapPixels(200);


        private void MeterFaceSquare(double[] tickPositions, double[] labels)
        {
            double minSqrt = Math.Sqrt(tickPositions.First());
            double maxSqrt = Math.Sqrt(tickPositions.Last());
            double ratio = 1 / maxSqrt;
            System.Drawing.Graphics graphicsObj = Graphics.FromImage(myBitmap);
            Pen myPen = new Pen(System.Drawing.Color.Black, ScanPixelsToBitmapPixels(6));
            foreach (var v in tickPositions) {
                Point p; Point p2;
                double frac = ratio * (Math.Sqrt(v) - minSqrt);
                MeterTickPositionFromValue(frac, 0, out p);
                MeterTickPositionFromValue(frac, ScanPixelsToBitmapPixels(25), out p2);
                graphicsObj.DrawLine(myPen, p, p2);

            }

            TextFormatFlags flags = TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter;
            System.Drawing.Font drawFont = new System.Drawing.Font("Arial", TickFontSize);
            foreach (var v in labels)
            {
                Point p; Point p2;
                double frac = ratio * (Math.Sqrt(v) - minSqrt);
                MeterTickPositionFromValue(frac, 0, out p);
                MeterTickPositionFromValue(frac, ScanPixelsToBitmapPixels(50), out p2);
                graphicsObj.DrawLine(myPen, p, p2);

                MeterTickPositionFromValue(frac, -LabelOutsideRadius, out p);
                int TickTextPixelWidth = ScanPixelsToBitmapPixels(400);
                int TickTextPixelHeight = ScanPixelsToBitmapPixels(200);
                var TextRect = new Rectangle(p.X - TickTextPixelWidth / 2, p.Y - TickTextPixelHeight / 2, TickTextPixelWidth, TickTextPixelHeight);
                TextRenderer.DrawText(graphicsObj, String.Format("{0}", v), drawFont, TextRect, Color.Black, flags);

            }
            drawFont.Dispose();

            drawFont = new System.Drawing.Font("Arial", LabelFontSize);
            var TextRect2 = new Rectangle(axisX - TickTextPixelWidth / 2, meterLimitsY - TickTextPixelHeight / 2, TickTextPixelWidth, TickTextPixelHeight);
            TextRenderer.DrawText(graphicsObj, "Power", drawFont, TextRect2, Color.Black, flags);
        }

        private void MeterFaceSWR(double[] majorTicks, double max)
        {
            double minSWRlog = Math.Log(majorTicks.First());
            double maxSWRlog = Math.Log(max);
            double scale = maxSWRlog - minSWRlog;
            double inverse = 1 / scale;
            System.Drawing.Graphics graphicsObj = Graphics.FromImage(myBitmap);
            Pen myPen = new Pen(System.Drawing.Color.Black, ScanPixelsToBitmapPixels(6));
            System.Drawing.Font drawFont = new System.Drawing.Font("Arial", TickFontSize);
            TextFormatFlags flags = TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter;
            foreach (var v in majorTicks)
            {
                Point p; Point p2;
                double frac = (Math.Log(v) - minSWRlog) * inverse;
                MeterTickPositionFromValue(frac, 0, out p);
                MeterTickPositionFromValue(frac, ScanPixelsToBitmapPixels(20), out p2);
                graphicsObj.DrawLine(myPen, p, p2);

                MeterTickPositionFromValue(frac, -LabelOutsideRadius, out p);
                var TextRect = new Rectangle(p.X - TickTextPixelWidth / 2, p.Y - TickTextPixelHeight / 2, TickTextPixelWidth, TickTextPixelHeight);
                TextRenderer.DrawText(graphicsObj, String.Format("{0}", v), drawFont, TextRect, Color.Black, flags);

            }
            drawFont.Dispose();

            drawFont = new System.Drawing.Font("Arial", LabelFontSize);
            var TextRect2 = new Rectangle(axisX - TickTextPixelWidth / 2, meterLimitsY - TickTextPixelHeight / 2, TickTextPixelWidth, TickTextPixelHeight);
            TextRenderer.DrawText(graphicsObj, "SWR", drawFont, TextRect2, Color.Black, flags);
        }


        private void MeterFaceAll()
        {
            /* From a scan of the BAOMAIN meter, 0-1mA meter face
             * at 600 DPI, the meter axis is at 762, 970. 
             * Radius is 850 pixels
             * fastener holes are at 408, 807 pixels and 1110, 807 pixels
            */
            myBitmap = new Bitmap(
                ScanPixelsToBitmapPixels(1600),
                ScanPixelsToBitmapPixels(1000), 
                System.Drawing.Imaging.PixelFormat.Format24bppRgb);
            myBitmap.SetResolution(Bitmap_DPI_X_and_Y, Bitmap_DPI_X_and_Y);
            System.Drawing.Graphics graphicsObj = Graphics.FromImage(myBitmap);
            var backgroundfill = new System.Drawing.SolidBrush(System.Drawing.Color.White);
            graphicsObj.FillRectangle(backgroundfill, 0, 0, myBitmap.Width, myBitmap.Height);
            Pen myPen = new Pen(System.Drawing.Color.Black, ScanPixelsToBitmapPixels(8));
            Rectangle rectangleObj = new Rectangle(axisX - meterRadius, axisY - meterRadius, meterRadius * 2, meterRadius * 2);
            // meter movement is 90 degrees total, from -45 to +45 from vertical.
            graphicsObj.DrawArc(myPen, rectangleObj, -135, 90);

            // Line between fastener holes
            myPen = new Pen(System.Drawing.Color.Red, ScanPixelsToBitmapPixels(5));
            myPen.DashStyle = System.Drawing.Drawing2D.DashStyle.Solid;
            myPen.Width = 6;
            graphicsObj.DrawLine(myPen, ScanPixelsToBitmapPixels(411), ScanPixelsToBitmapPixels(807), ScanPixelsToBitmapPixels(1113), ScanPixelsToBitmapPixels(807));
            graphicsObj.DrawLine(myPen, ScanPixelsToBitmapPixels(411), ScanPixelsToBitmapPixels(757), ScanPixelsToBitmapPixels(411), ScanPixelsToBitmapPixels(857));
            graphicsObj.DrawLine(myPen, ScanPixelsToBitmapPixels(1113), ScanPixelsToBitmapPixels(757), ScanPixelsToBitmapPixels(1113), ScanPixelsToBitmapPixels(857));

            //center horizontal cutoff
            myPen.Color = System.Drawing.Color.Black;
            graphicsObj.DrawLine(myPen, ScanPixelsToBitmapPixels(581), ScanPixelsToBitmapPixels(708), ScanPixelsToBitmapPixels(937), ScanPixelsToBitmapPixels(708));

            // verify endpoints
            //myPen.Width = 3;
            //graphicsObj.DrawLine(myPen, 160, 370, 762, 970);
            //graphicsObj.DrawLine(myPen, 1364, 370, 762, 970);

        }

        private void Form1_Load(object sender, EventArgs e)
        {
            MeterFaceAll();
        }

        private void exitToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Close();
        }
 
        private void saveToolStripMenuItem_Click_1(object sender, EventArgs e)
        {
                SaveFileDialog dlg = new SaveFileDialog();
                dlg.Filter = "Bitmap File (*.bmp)|*.bmp|JPEG File (*.jpg)|*.jpg";
                dlg.Title = "Save As File";
                if (dlg.ShowDialog() == DialogResult.OK)
                {
                    ImageCodecInfo myImageCodecInfo = null;
                    string ext = System.IO.Path.GetExtension(dlg.FileName);
                    switch (ext)
                    {
                        case ".jpg":
                            myImageCodecInfo = GetEncoderInfo("image/jpeg");
                            break;
                        case ".bmp":
                            myImageCodecInfo = GetEncoderInfo("image/bmp");
                            break;
                        default:
                            return;
                    }
                    Encoder myEncoder = Encoder.Quality;
                    EncoderParameter myEncoderParameter = new EncoderParameter(myEncoder, 100L);
                    EncoderParameters myEncoderParameters = new EncoderParameters(1);
                    myEncoderParameters.Param[0] = myEncoderParameter;
                    myBitmap.Save(dlg.FileName, myImageCodecInfo, myEncoderParameters);
                }
        }

         private void toolStripMenuSWR_Click(object sender, EventArgs e)
        {
            double[] ticks = { 1,  1.5, 2, 3, 4, 6, 10};
            MeterFaceSWR(ticks, 11);
            Invalidate();
        }

        private void toolStripMenuReset_Click(object sender, EventArgs e)
        {
            MeterFaceAll();
            Invalidate();
        }

        private void toolStripMenuTicks_Click(object sender, EventArgs e)
        {
            MeterFaceTicks(50);
            Invalidate();
        }

        private void toolStripMenuSquare_Click(object sender, EventArgs e)
        {
            double[] ticks = {0, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90,  100,   150, 200, 250, 300 };
            double[] labels = { 0, 5, 20, 50, 100, 200, 300 };
            MeterFaceSquare(ticks, labels);
            Invalidate();
        }
    }
}
