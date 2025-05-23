﻿
namespace NyeVikingMonitor
{
    partial class Main
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Main));
            this.serialPort1 = new System.IO.Ports.SerialPort(this.components);
            this.comboBoxSerial = new System.Windows.Forms.ComboBox();
            this.timer1 = new System.Windows.Forms.Timer(this.components);
            this.labelW = new System.Windows.Forms.Label();
            this.labelSwrMax = new System.Windows.Forms.Label();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.radioButtonPeak = new System.Windows.Forms.RadioButton();
            this.radioButtonAverage = new System.Windows.Forms.RadioButton();
            this.labelPr = new NyeVikingMonitor.BarLabel();
            this.labelPf = new NyeVikingMonitor.BarLabel();
            this.labelSWR = new NyeVikingMonitor.BarLabel();
            this.groupBox1.SuspendLayout();
            this.SuspendLayout();
            // 
            // serialPort1
            // 
            this.serialPort1.BaudRate = 38400;
            this.serialPort1.DataReceived += new System.IO.Ports.SerialDataReceivedEventHandler(this.serialPort1_DataReceived);
            // 
            // comboBoxSerial
            // 
            this.comboBoxSerial.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboBoxSerial.FormattingEnabled = true;
            this.comboBoxSerial.Location = new System.Drawing.Point(216, 117);
            this.comboBoxSerial.Name = "comboBoxSerial";
            this.comboBoxSerial.Size = new System.Drawing.Size(121, 21);
            this.comboBoxSerial.TabIndex = 0;
            this.comboBoxSerial.DropDown += new System.EventHandler(this.comboBoxSerial_DropDown);
            this.comboBoxSerial.SelectedIndexChanged += new System.EventHandler(this.comboBoxSerial_SelectedIndexChanged);
            // 
            // timer1
            // 
            this.timer1.Interval = 500;
            this.timer1.Tick += new System.EventHandler(this.timer1_Tick);
            // 
            // labelW
            // 
            this.labelW.AutoSize = true;
            this.labelW.Location = new System.Drawing.Point(311, 24);
            this.labelW.Name = "labelW";
            this.labelW.Size = new System.Drawing.Size(19, 13);
            this.labelW.TabIndex = 4;
            this.labelW.Text = "30";
            // 
            // labelSwrMax
            // 
            this.labelSwrMax.AutoSize = true;
            this.labelSwrMax.Location = new System.Drawing.Point(314, 89);
            this.labelSwrMax.Name = "labelSwrMax";
            this.labelSwrMax.Size = new System.Drawing.Size(22, 13);
            this.labelSwrMax.TabIndex = 5;
            this.labelSwrMax.Text = "4.0";
            // 
            // groupBox1
            // 
            this.groupBox1.Controls.Add(this.radioButtonPeak);
            this.groupBox1.Controls.Add(this.radioButtonAverage);
            this.groupBox1.Location = new System.Drawing.Point(-6, 106);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(178, 39);
            this.groupBox1.TabIndex = 6;
            this.groupBox1.TabStop = false;
            // 
            // radioButtonPeak
            // 
            this.radioButtonPeak.AutoSize = true;
            this.radioButtonPeak.Location = new System.Drawing.Point(101, 14);
            this.radioButtonPeak.Name = "radioButtonPeak";
            this.radioButtonPeak.Size = new System.Drawing.Size(50, 17);
            this.radioButtonPeak.TabIndex = 1;
            this.radioButtonPeak.Text = "Peak";
            this.radioButtonPeak.UseVisualStyleBackColor = true;
            this.radioButtonPeak.CheckedChanged += new System.EventHandler(this.radioButtonAverage_CheckedChanged);
            // 
            // radioButtonAverage
            // 
            this.radioButtonAverage.AutoSize = true;
            this.radioButtonAverage.Checked = true;
            this.radioButtonAverage.Location = new System.Drawing.Point(23, 13);
            this.radioButtonAverage.Name = "radioButtonAverage";
            this.radioButtonAverage.Size = new System.Drawing.Size(65, 17);
            this.radioButtonAverage.TabIndex = 0;
            this.radioButtonAverage.TabStop = true;
            this.radioButtonAverage.Text = "Average";
            this.radioButtonAverage.UseVisualStyleBackColor = true;
            this.radioButtonAverage.CheckedChanged += new System.EventHandler(this.radioButtonAverage_CheckedChanged);
            // 
            // labelPr
            // 
            this.labelPr.ForeColor = System.Drawing.Color.Red;
            this.labelPr.LeftValue = 0F;
            this.labelPr.Location = new System.Drawing.Point(12, 43);
            this.labelPr.Name = "labelPr";
            this.labelPr.RightValue = 1F;
            this.labelPr.Size = new System.Drawing.Size(293, 22);
            this.labelPr.TabIndex = 3;
            this.labelPr.Text = "Reflected";
            this.labelPr.Value = 0F;
            // 
            // labelPf
            // 
            this.labelPf.ForeColor = System.Drawing.Color.Green;
            this.labelPf.LeftValue = 0F;
            this.labelPf.Location = new System.Drawing.Point(12, 5);
            this.labelPf.Name = "labelPf";
            this.labelPf.RightValue = 1F;
            this.labelPf.Size = new System.Drawing.Size(293, 22);
            this.labelPf.TabIndex = 2;
            this.labelPf.Text = "Forward";
            this.labelPf.Value = 0F;
            // 
            // labelSWR
            // 
            this.labelSWR.LeftValue = 0F;
            this.labelSWR.Location = new System.Drawing.Point(12, 81);
            this.labelSWR.Name = "labelSWR";
            this.labelSWR.RightValue = 1F;
            this.labelSWR.Size = new System.Drawing.Size(293, 22);
            this.labelSWR.TabIndex = 1;
            this.labelSWR.Text = "labelSWR";
            this.labelSWR.Value = 0F;
            // 
            // Main
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(349, 141);
            this.Controls.Add(this.groupBox1);
            this.Controls.Add(this.labelSwrMax);
            this.Controls.Add(this.labelW);
            this.Controls.Add(this.labelPr);
            this.Controls.Add(this.labelPf);
            this.Controls.Add(this.labelSWR);
            this.Controls.Add(this.comboBoxSerial);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "Main";
            this.Text = "Power Monitor";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.Main_FormClosed);
            this.Load += new System.EventHandler(this.Main_Load);
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.IO.Ports.SerialPort serialPort1;
        private System.Windows.Forms.ComboBox comboBoxSerial;
        private System.Windows.Forms.Timer timer1;
        private BarLabel labelSWR;
        private BarLabel labelPf;
        private BarLabel labelPr;
        private System.Windows.Forms.Label labelW;
        private System.Windows.Forms.Label labelSwrMax;
        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.RadioButton radioButtonAverage;
        private System.Windows.Forms.RadioButton radioButtonPeak;
    }
}

