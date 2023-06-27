using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO.Ports;

namespace NyeVikingMonitor
{
    public partial class Main : Form
    {
        public Main()
        {
            InitializeComponent();
        }

        private void Main_Load(object sender, EventArgs e)
        {
            LoadSerialCombo();
            labelSWR.Text = "";
            labelPr.Text = "";
            labelPr.LeftValue = 0;
            labelPr.RightValue = 100.0f;

            labelPf.Text = "";
            labelPf.LeftValue = 0;
            labelPf.RightValue = 100.0f;

            labelSWR.LeftValue = 1.0f;
            labelSWR.RightValue = 4.0f;
        }

        private class PortDescriptor
        {
            public PortDescriptor(string name)
            {
                this.name = name;
                if (name.StartsWith("COM"))
                    idx = Int32.Parse(name.Substring(3));
            }
            public string name;
            public int idx;
            public override string ToString() { return name; }
        }

        private void LoadSerialCombo()
        {
            List<PortDescriptor> ports = new List<PortDescriptor>();
            PortDescriptor selected = null;
            foreach (string s in SerialPort.GetPortNames())
            {
                PortDescriptor next = new PortDescriptor(s);
                ports.Add(next);
                if (Properties.Settings.Default.PortIdx == next.idx)
                    selected = next;
            }
            ports.Sort((PortDescriptor s1, PortDescriptor s2) => {
                return s1.idx - s2.idx;
            });
            comboBoxSerial.Items.Clear();
            foreach (var s in ports)
                comboBoxSerial.Items.Add(s);
            comboBoxSerial.SelectedItem = selected;
        }

        private void comboBoxSerial_DropDown(object sender, EventArgs e)
        {
            LoadSerialCombo();
        }

        private bool SetPort(PortDescriptor pd)
        {
            serialPort1.PortName = pd.name;
            serialPort1.Open();
            var ret = serialPort1.IsOpen;
            if (ret)
            {
                HaveReceivedAnything = false;
                timer1.Interval = TIMER_INIT_MSEC;
                timer1.Enabled = true;
                serialPort1.WriteLine(radioButtonAverage.Checked ? "P ON" : "P PEAK");
            }
            return ret;
        }

        private void comboBoxSerial_SelectedIndexChanged(object sender, EventArgs e)
        {
            var sel = comboBoxSerial.SelectedItem as PortDescriptor;
            if (null != sel)
            {
                if (serialPort1.IsOpen)
                    serialPort1.Close();
                timer1.Enabled = false;
                if (SetPort(sel))
                    Properties.Settings.Default.PortIdx = sel.idx;
            }
        }
        const int NYE_VIKING_TIMER_MSEC = 5000;
        const int TIMER_INIT_MSEC = 500;
        bool HaveReceivedAnything;
        int Vf; int Vr; int Pf; int Pr;
        string serialBuffer;
        private void SerialReceived(string s)
        {
            if (!HaveReceivedAnything)
            {
                HaveReceivedAnything = true;
                timer1.Interval = NYE_VIKING_TIMER_MSEC;
            }
            foreach (char c in s)
            {
                if (c == '\r' || c == '\n')
                {
                    string[] items = serialBuffer.Split();
                    foreach (string item in items)
                    {
                        if (item.StartsWith("Vf:"))
                            Vf = Int32.Parse(item.Substring(3));
                        else if (item.StartsWith("Vr:"))
                            Vr = Int32.Parse(item.Substring(3));
                        else if (item.StartsWith("Pf:"))
                            Pf = Int32.Parse(item.Substring(3));
                        else if (item.StartsWith("Pr:"))
                            Pr = Int32.Parse(item.Substring(3));

                    }
                    serialBuffer = "";
                    if ((Vf != 0) && (Vr != 0))
                    {
                        if (Vr >= Vf)
                        {
                            labelSWR.Text = "infinite";
                            labelSWR.Value = labelSWR.RightValue;
                            labelSWR.ForeColor = Color.Red;
                        }
                        else if (Vr == 0)
                        {
                            labelSWR.Text = "1.0";
                            labelSWR.Value = 1.0f;
                            labelSWR.ForeColor = Color.Green;
                        }
                        else
                        {
                            double num = (Vf + Vr);
                            double denom = (Vf - Vr);
                            double SWR = num / denom;
                            labelSWR.Value = (float)SWR;
                            labelSWR.Text = String.Format("{0:0.00}", SWR);
                            if (SWR < 2.0f)
                                labelSWR.ForeColor = Color.Green;
                            else if (SWR < labelSWR.RightValue)
                                labelSWR.ForeColor = Color.Yellow;
                            else
                                labelSWR.ForeColor = Color.Red;
                        }
                    }

                    float powerF = Pf / 128.0f;
                    float powerR = Pr / 128.0f;

                    ToLabel(labelPf, powerF);
                    ToLabel(labelPr, powerR);

                    if (powerF <= 30.0f)
                        labelPf.RightValue = 30.0f;
                    else if (powerF <= 300.0f)
                        labelPf.RightValue = 300.0f;
                    else
                        labelPf.RightValue = 3000.0f;
                    labelPr.RightValue = labelPf.RightValue;
                    labelW.Text = String.Format("{0} W", labelPf.RightValue);
                }
                else
                    serialBuffer += c;
            }
        }
        private void ToLabel(BarLabel l, float v)
        {
            if (v < 100)
                l.Text = String.Format("{0:0.0}", v);
            else
                l.Text = String.Format("{0:0}", v);
            l.Value = v;
        }
        private void serialPort1_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            var sp = (SerialPort)sender;
            BeginInvoke(new Action<string>(SerialReceived), sp.ReadExisting());
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            if (serialPort1.IsOpen)
                serialPort1.WriteLine(radioButtonAverage.Checked ? "P ON" : "P PEAK");
        }

        private void Main_FormClosed(object sender, FormClosedEventArgs e)
        {
            timer1.Enabled = false;
            if (serialPort1.IsOpen)
                serialPort1.Dispose();
            Properties.Settings.Default.Save();
        }

        private void radioButtonAverage_CheckedChanged(object sender, EventArgs e)
        {
            timer1_Tick(null, null);
        }
    }
}
