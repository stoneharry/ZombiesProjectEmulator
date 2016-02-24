using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace WoW.Streaming.Utils {
    static unsafe class UnsafeNativeMethods {

        [DllImport("Kernel32.dll")]
        public static extern void CopyMemory(void* dest, void* source, int numBytes);
    }
}
