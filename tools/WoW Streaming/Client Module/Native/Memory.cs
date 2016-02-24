using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace WoW.Streaming.Native
{
    public static class Memory
    {
        private static Process CurrentProcess { get; set; }

        public static IntPtr BaseAddress {get; private set;}

        static Memory() {
            CurrentProcess = Process.GetCurrentProcess();
            BaseAddress = CurrentProcess.MainModule.BaseAddress;
        }

        public static unsafe void Read<T>(IntPtr address, T[] data) where T : struct
        {
            if (data == null || data.Length == 0)
                return;

            var size = SizeCache<T>.Size * data.Length;
            var ptr = SizeCache<T>.GetUnsafePtr(ref data[0]);
            var numRead = 0;
            if (ReadProcessMemoryPtr(CurrentProcess.Handle, address, ptr, size, ref numRead) == false || numRead != size)
                throw new InvalidOperationException("Unable to read array of type " + typeof (T) + " with length " +
                                                    data.Length + " at 0x" + address.ToString("X8"));

        }

        public static unsafe T Read<T>(IntPtr address) where T : struct
        {
            var ret = new T();
            var ptr = SizeCache<T>.GetUnsafePtr(ref ret);
            var numRead = 0;
            if (ReadProcessMemoryPtr(CurrentProcess.Handle, address, ptr, SizeCache<T>.Size, ref numRead) == false ||
                numRead != SizeCache<T>.Size)
                throw new InvalidOperationException("Unable to read a value of type '" + typeof (T) + "' at 0x" +
                                                    address.ToString("X8"));

            return ret;
        }

        public static string ReadString(IntPtr address)
        {
            var bytes = new List<byte>();
            var curByte = Read<byte>(address);
            while(curByte != 0)
            {
                bytes.Add(curByte);
                address += 1;
                curByte = Read<byte>(address);
            }

            return Encoding.UTF8.GetString(bytes.ToArray());
        }

        public static byte[] ReadBytes(IntPtr address, int numBytes)
        {
            if (numBytes <= 0)
                return new byte[0];

            var ret = new byte[numBytes];
            var numRead = 0;
            if (ReadProcessMemory(CurrentProcess.Handle, address, ret, numBytes, ref numRead) == false ||
                numRead != numBytes)
                throw new InvalidOperationException("Unable to read " + numBytes + " bytes at 0x" +
                                                    address.ToString("X8"));

            return ret;
        }

        public static T ReadRelative<T>(IntPtr address) where T : struct
        {
            return Read<T>(BaseAddress + address.ToInt32());
        }

        public static void ReadRelative<T>(IntPtr address, T[] data) where T : struct
        {
            Read<T>(BaseAddress + address.ToInt32(), data);
        }

        public static byte[] ReadBytesRelative(IntPtr address, int numBytes)
        {
            return ReadBytes(BaseAddress + address.ToInt32(), numBytes);
        }

        public unsafe static void Write<T>(IntPtr address, T value) where T : struct
        {
            var size = SizeCache<T>.Size;
            var ptr = SizeCache<T>.GetUnsafePtr(ref value);
            var numWritten = 0;
            if (WriteProcessMemoryPtr(CurrentProcess.Handle, address, ptr, size, ref numWritten) == false ||
                numWritten != size)
                throw new InvalidOperationException("Unable to write value of type '" + typeof (T) + "' to 0x" +
                                                    address.ToString("X8"));
        }

        public unsafe static void Write<T>(IntPtr address, T[] value) where T : struct
        {
            if (value == null || value.Length == 0)
                return;

            var size = SizeCache<T>.Size * value.Length;
            var ptr = SizeCache<T>.GetUnsafePtr(ref value[0]);
            var numWritten = 0;
            if (WriteProcessMemoryPtr(CurrentProcess.Handle, address, ptr, size, ref numWritten) == false ||
                numWritten != size)
                throw new InvalidOperationException("Unable to write array of type '" + typeof (T) + "' with length " +
                                                    value.Length + " to 0x" + address.ToString("X8"));
        }

        public static void Write(IntPtr address, byte[] value)
        {
            if (value == null || value.Length == 0)
                return;

            var size = value.Length;
            var numWritten = 0;
            if (WriteProcessMemory(CurrentProcess.Handle, address, value, size, ref numWritten) == false ||
                numWritten != size)
                throw new InvalidOperationException("Unable to write byte array with length " +
                                                    value.Length + " to 0x" + address.ToString("X8"));
        }

        public static void WriteRelative<T>(IntPtr address, T value) where T : struct
        {
            Write(BaseAddress + address.ToInt32(), value);
        }

        public static void WriteRelative<T>(IntPtr address, T[] value) where T : struct
        {
            Write(BaseAddress + address.ToInt32(), value);
        }

        public static void WriteRelative(IntPtr address, byte[] value)
        {
            Write(BaseAddress + address.ToInt32(), value);
        }

        [DllImport("Kernel32.dll")]
        private static extern bool ReadProcessMemory(IntPtr handle, IntPtr address, byte[] buffer, int size,
            ref int numRead);

        [DllImport("Kernel32.dll", EntryPoint = "ReadProcessMemory")]
        private static unsafe extern bool ReadProcessMemoryPtr(IntPtr handle, IntPtr address, void* buffer, int size,
            ref int numRead);

        [DllImport("Kernel32.dll")]
        private static extern bool WriteProcessMemory(IntPtr handle, IntPtr address, byte[] buffer, int size,
            ref int numWritten);

        [DllImport("Kernel32.dll", EntryPoint = "WriteProcessMemory")]
        private unsafe static extern bool WriteProcessMemoryPtr(IntPtr handle, IntPtr address, void* buffer, int size,
            ref int numWritten);
    }
}
