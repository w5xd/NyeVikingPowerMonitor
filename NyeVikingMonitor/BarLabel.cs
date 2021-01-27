using System;
using System.Windows.Forms;
using System.Drawing;

namespace NyeVikingMonitor
{
    class BarLabel : Label
    {
        private float barValue;
        public float Value { get { return barValue; }
            set
            {
                barValue = value;
                Invalidate();
            }
        }

        public float RightValue { get; set; } = 1.0f;
        public float LeftValue { get; set; } = 0.0f;

        protected override void OnPaint(PaintEventArgs e)
        {
            System.Drawing.Brush fb = new SolidBrush(BackColor);
            using (fb)
                e.Graphics.FillRectangle(fb, ClientRectangle);
            if (LeftValue >= RightValue)
                return;
            Brush barBrush = new SolidBrush(ForeColor);
            using (barBrush)
            {
                float width = this.Size.Width - 2;
                width *= (Value - LeftValue) / (RightValue - LeftValue);
                RectangleF valueRect = new RectangleF(1, 1, width, this.Size.Height -2);
                e.Graphics.FillRectangle(barBrush, valueRect);
                Brush textBrush = new SolidBrush(Color.White);
                Pen blackPen = new Pen(Color.Black);
                using (textBrush)
                {
                    e.Graphics.DrawRectangle(blackPen, valueRect.X, valueRect.Y, valueRect.Width, valueRect.Height);
                    e.Graphics.DrawString(Text, Font, textBrush, 1, 1 + (this.Size.Height - Font.Height)/2);
                }
                blackPen.Dispose();
            }
        }
    }
}
