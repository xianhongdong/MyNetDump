using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace MyNetDump
{
    [StructLayout(LayoutKind.Sequential)]
    public struct NetPackage
    {
        [MarshalAs(UnmanagedType.ByValTStr,SizeConst =16)]
        public string dstip;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 16)]
        public string srcip;
    }
    public class Test
    {
        public Test() { }
        ~Test() { }
    }
}
