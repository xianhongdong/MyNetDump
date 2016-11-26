using MyNetDump;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace MyNetDump
{
    public partial class Form1 : Form
    {
        public delegate void CallBack(NetPackage package);
        [DllImport("CoreMonitor.dll")]
        public static extern bool Start();
        [DllImport("CoreMonitor.dll")]
        public static extern bool Stop();
        [DllImport("CoreMonitor.dll",CallingConvention = CallingConvention.Cdecl)]
        public static extern bool Init(CallBack handler);

        CallBack callback = null;
        int index = 0;
        public Form1()
        {
            InitializeComponent();
        }

        private void buttonStart_Click(object sender, EventArgs e)
        {
            callback = new CallBack(OnReceivePackage);
            Init(callback);
            var ret = Start();
            
        }
        private void OnReceivePackage(NetPackage package)
        {
            Invoke(new MethodInvoker(delegate () {
                var rowIndex = this.dataGridView1.Rows.Add(new object[] { index++, package.dstip, package.srcip });
                dataGridView1.FirstDisplayedScrollingRowIndex = rowIndex;
            }));
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            dataGridView1.Columns.Add("index", "index");
            dataGridView1.Columns.Add("dstip", "dstip");
            dataGridView1.Columns.Add("srcip", "srcip");
        }

        private void buttonStop_Click(object sender, EventArgs e)
        {
            Stop();
        }
    }
}
