using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using WoW.Streaming.Native;
using System.IO;
using WoW.Streaming.Utils;

// ReSharper disable InconsistentNaming

namespace WoW.Streaming.IO
{
    class FileHandle
    {
        protected Stream inputStream;

        internal FileHandle(Stream file)
        {
            inputStream = file;
        }

        internal FileHandle() {
            inputStream = null;
        }

        public long Size { get { return inputStream.Length; } }
        public long Position { get { return inputStream.Position; } set { inputStream.Position = value; } }
        public Stream FD { get { return inputStream; } }
    }

    unsafe class StormHooks
    {
        public static StormHooks Instance { get; private set; }

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate int SFileOpenArchive([MarshalAs(UnmanagedType.LPStr)] string a1, int a2, int a3, ref uint a4);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate int SFileGetFileSize(IntPtr fileHandle, IntPtr fileSizeHigh);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate bool SFileReadFile(IntPtr fileHandle, IntPtr buffer, int toRead, IntPtr numRead, int a5, int a6);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate bool SFileCloseFile(IntPtr fileHandle);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate IntPtr SFileOpenFile(int a2, [MarshalAs(UnmanagedType.LPStr)] string fileName, int a4, ref IntPtr handle);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate bool SFileUnk1(int a1, [MarshalAs(UnmanagedType.LPStr)] string someName, int a3, int a4, int a5);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate bool SFileCloseArchive(int archive);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate int SFileSetFilePointer(IntPtr hFile, int low, int high, int seekDir);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate int EnumFilesInArchive(IntPtr archive, IntPtr callback, int a3);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        delegate int FrameXMLSignatureCheck([MarshalAs(UnmanagedType.LPStr)] string a1, int a2, int a3, int a4);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        delegate int FrameXMLUnkFunction(IntPtr a1, IntPtr a2);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate bool FrameXMLUnkFunction2(int a1, int a2);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate bool StormReadFileDLG(int a1, [MarshalAs(UnmanagedType.LPStr)] string fileName, ref IntPtr outBuffer, ref int numRead,
            int a5, int a6, int a7);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate bool StormFreeBufferDLG(IntPtr buffer);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        delegate void CMap__SafeLoadDLG(IntPtr _this);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        delegate void CMap__LoadDLG([MarshalAs(UnmanagedType.LPStr)] string continent, int mapId);

        SFileOpenArchive newFunction;
        SFileGetFileSize SFileGetFileSizeNew;
        SFileReadFile SFileReadFileNew;
        SFileCloseFile SFileCloseFileNew;
        SFileOpenFile SFileOpenFileNew;
        SFileUnk1 SFileUnk1New;
        SFileCloseArchive SFileCloseArchiveNew;
        SFileSetFilePointer SFileSetFilePointerNew;
        EnumFilesInArchive EnumFilesInArchiveNew;
        FrameXMLSignatureCheck FrameXMLSignatureCheckNew;
        FrameXMLUnkFunction FrameXMLUnkFunctionNew;
        FrameXMLUnkFunction2 FrameXMLUnkFunction2New;

        Hook<SFileOpenArchive> SFileOpenArchiveOrig;
        Hook<SFileGetFileSize> SFileGetFileSizeOrig;
        Hook<SFileReadFile> SFileReadFileOrig;
        Hook<SFileCloseFile> SFileCloseFileOrig;
        Hook<SFileOpenFile> SFileOpenFileOrig;
        Hook<SFileUnk1> SFileUnk1Orig;
        Hook<SFileCloseArchive> SFileCloseArchiveOrig;
        Hook<SFileSetFilePointer> SFileSetFilePointerOrig;
        Hook<EnumFilesInArchive> EnumFilesInArchiveOrig;
        Hook<FrameXMLSignatureCheck> FrameXMLSignatureCheckOrig;
        Hook<FrameXMLUnkFunction> FrameXMLUnkFunctionOrig;
        Hook<FrameXMLUnkFunction2> FrameXMLUnkFunction2Orig;

        IntPtr archiveHandle;

        static StormHooks()
        {
            Instance = new StormHooks();
        }

        public void Initialize()
        {
            archiveHandle = Marshal.AllocCoTaskMem(4);
            Marshal.WriteInt32(archiveHandle, 1);

            Console.WriteLine("Archive handle is: " + archiveHandle);

            newFunction = SFileOpenArchive_Hook;
            SFileOpenArchiveOrig = new Hook<SFileOpenArchive>(new IntPtr(0x421950), false, newFunction);
            SFileOpenArchiveOrig.Apply();

            SFileGetFileSizeNew = SFileGetFileSize_Hook;
            SFileGetFileSizeOrig = new Hook<SFileGetFileSize>(new IntPtr(0x4218C0), false, SFileGetFileSizeNew);
            SFileGetFileSizeOrig.Apply();

            SFileReadFileNew = SFileReadFile_Hook;
            SFileReadFileOrig = new Hook<SFileReadFile>(new IntPtr(0x422530), false, SFileReadFileNew);
            SFileReadFileOrig.Apply();

            SFileCloseFileNew = SFileCloseFile_Hook;
            SFileCloseFileOrig = new Hook<SFileCloseFile>(new IntPtr(0x422910), false, SFileCloseFileNew);
            SFileCloseFileOrig.Apply();

            SFileOpenFileNew = SFileOpenFile_Hook;
            SFileOpenFileOrig = new Hook<SFileOpenFile>(new IntPtr(0x424B50), false, SFileOpenFileNew);
            SFileOpenFileOrig.Apply();

            SFileUnk1New = SFileUnk1_Hook;
            SFileUnk1Orig = new Hook<SFileUnk1>(new IntPtr(0x421A10), false, SFileUnk1New);
            SFileUnk1Orig.Apply();

            SFileCloseArchiveNew = SFileCloseArchive_Hook;
            SFileCloseArchiveOrig = new Hook<SFileCloseArchive>(new IntPtr(0x421720), false, SFileCloseArchiveNew);
            SFileCloseArchiveOrig.Apply();

            SFileSetFilePointerNew = SFileSetFilePointer_Hook;
            SFileSetFilePointerOrig = new Hook<SFileSetFilePointer>(new IntPtr(0x421BB0), false, SFileSetFilePointerNew);
            SFileSetFilePointerOrig.Apply();

            EnumFilesInArchiveNew = EnumFilesInArchive_Hook;
            EnumFilesInArchiveOrig = new Hook<EnumFilesInArchive>(new IntPtr(0x424FA0), false, EnumFilesInArchiveNew);
            EnumFilesInArchiveOrig.Apply();

            FrameXMLSignatureCheckNew = FrameXMLSignatureCheck_Hook;
            FrameXMLSignatureCheckOrig = new Hook<FrameXMLSignatureCheck>(new IntPtr(0x8165E0), false, FrameXMLSignatureCheckNew);
            FrameXMLSignatureCheckOrig.Apply();

            FrameXMLUnkFunctionNew = FrameXMLUnkFunction_Hook;
            FrameXMLUnkFunctionOrig = new Hook<FrameXMLUnkFunction>(new IntPtr(0x779AE0), false, FrameXMLUnkFunctionNew);
            FrameXMLUnkFunctionOrig.Apply();

            FrameXMLUnkFunction2New = FrameXMLUnkFunction2_Hook;
            FrameXMLUnkFunction2Orig = new Hook<FrameXMLUnkFunction2>(new IntPtr(0x424B10), false, FrameXMLUnkFunction2New);
            FrameXMLUnkFunction2Orig.Apply();

            Marshal.GetDelegateForFunctionPointer<StormReadFileDLG>(new IntPtr(0x424E80));
            Marshal.GetDelegateForFunctionPointer<StormFreeBufferDLG>(new IntPtr(0x421CA0));
            Marshal.GetDelegateForFunctionPointer<CMap__SafeLoadDLG>(new IntPtr(0x7D9A20));
            Marshal.GetDelegateForFunctionPointer<CMap__LoadDLG>(new IntPtr(0x7BFCE0));
        }

        int FrameXMLUnkFunction_Hook(IntPtr a1, IntPtr a2)
        {
            byte[] data = new byte[16];
            Marshal.Copy(data, 0, a1, 16);
            return 0;
        }

        bool FrameXMLUnkFunction2_Hook(int a1, int a2)
        {
            return true;
        }

        int FrameXMLSignatureCheck_Hook(string a1, int a2, int a3, int a4)
        {
            byte[] data = new byte[16];
            Marshal.Copy(data, 0, new IntPtr(a4), 16);
            return 3;
        }

        int EnumFilesInArchive_Hook(IntPtr hArchive, IntPtr callback, int a3)
        {
            return (int)EnumFilesInArchiveOrig.CallOriginal(archiveHandle, callback, a3);
        }

        int SFileSetFilePointer_Hook(IntPtr hFile, int low, int high, int seekDir)
        {
            if(hFile == IntPtr.Zero)
                return 0;

            var handle = (FileHandle) Marshal.GetObjectForIUnknown(hFile);

            var value = low | (high << 32);
            switch (seekDir) {
                case 1:
                    handle.Position += value;
                    break;
                case 2:
                    handle.Position -= value;
                    break;
                default:
                    handle.Position = value;
                    break;
            }

            return (int) handle.Position;
        }

        bool SFileCloseArchive_Hook(int a1)
        {
            return true;
        }

        bool SFileUnk1_Hook(int a1, string name, int a3, int a4, int a5)
        {
            return true;
        }

        // ReSharper disable once RedundantAssignment
        IntPtr SFileOpenFile_Hook(int a2, string fileName, int a4, ref IntPtr handle) {
            var stopwatch = Stopwatch.StartNew();
            //Console.WriteLine("Fetching: " + fileName);

            FileHandle localHandle;
            var path = Path.Combine(Directory.GetCurrentDirectory(), fileName);
            if(!File.Exists(path))
            {
                var request = FileSystem.Instance.GetFile(fileName);
                if (request == null) {
                    handle = IntPtr.Zero;
                    return IntPtr.Zero;
                }

                request.FD.Position = 0;
                localHandle = request;
            }
            else
            {
                localHandle = new FileHandle(File.OpenRead(path));
            }

            handle = Marshal.GetIUnknownForObject(localHandle);
            stopwatch.Stop();
            if (stopwatch.ElapsedMilliseconds > 10) {
                Console.WriteLine(fileName + ": " + stopwatch.ElapsedMilliseconds);
            }
            return handle;
        }

        bool SFileReadFile_Hook(IntPtr fileHandle, IntPtr buffer, int toRead, IntPtr numRead, int a5, int a6) {
            var pNumRead = (int*) numRead.ToPointer();
            
            if (a5 != 0)
                return false;

            if (toRead == 0)
            {
                if (numRead != IntPtr.Zero) {
                    *pNumRead = 0;
                }
                return true;
            }

            FileHandle file = (FileHandle)Marshal.GetObjectForIUnknown(fileHandle);

            var available = file.Size - file.Position;
            if (available < toRead)
                toRead = (int)available;

            if (available <= 0)
                return false;

            if (numRead != IntPtr.Zero)
                *pNumRead = toRead;

            var data = new byte[toRead];
            var totalRead = 0;
            while (totalRead < toRead)
            {
                var chunkSize = file.FD.Read(data, totalRead, toRead - totalRead);
                totalRead += chunkSize;
            }

            fixed (byte* ptr = data) {
                UnsafeNativeMethods.CopyMemory(buffer.ToPointer(), ptr, toRead);
            }

            return true;
        }

        // ReSharper disable once RedundantAssignment
        int SFileOpenArchive_Hook(string archive, int a2, int a3, ref uint a4)
        {
            a4 = (uint) archiveHandle.ToInt64();
            return archiveHandle.ToInt32();
        }

        int SFileGetFileSize_Hook(IntPtr hFile, IntPtr fileSizeHigh)
        {
            var file = (FileHandle)Marshal.GetObjectForIUnknown(hFile);

            var size = file.Size;
            var low = (int)(size & 0xFFFFFFFF);
            var high = (int)(size >> 32);
            if (fileSizeHigh != IntPtr.Zero)
                Marshal.WriteInt32(fileSizeHigh, high);

            return low;
        }

        bool SFileCloseFile_Hook(IntPtr hFile)
        {
            if (hFile == IntPtr.Zero)
                return true;

            Marshal.Release(hFile);

            return true;
        }
    }
}
