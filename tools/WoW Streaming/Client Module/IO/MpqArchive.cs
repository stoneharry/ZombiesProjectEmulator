using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace WoW.Streaming.IO {
    internal class MpqArchive {
        private readonly IntPtr mHandle;

        public string FileName { get; private set; }

        public bool CanAppendFiles {
            get {
                var maxFileBytes = new byte[4];
                var curFileBytes = new byte[4];

                int sizeRequired;

                Stormlib.SFileGetFileInfo(mHandle, Stormlib.SFileInfoClass.SFileMpqMaxFileCount, maxFileBytes, 4,
                    out sizeRequired);
                Stormlib.SFileGetFileInfo(mHandle, Stormlib.SFileInfoClass.SFileMpqNumberOfFiles, curFileBytes, 4,
                    out sizeRequired);

                return BitConverter.ToUInt32(curFileBytes, 0) < BitConverter.ToUInt32(maxFileBytes, 0);
            }
        }

        public MpqArchive(string fileName) {
            if (!Stormlib.SFileOpenArchive(fileName, 0, 0, out mHandle)) {
                throw new InvalidOperationException("Unable to open MPQ " + fileName);
            }

            FileName = fileName;
        }

        private MpqArchive(IntPtr handle, string fileName) {
            mHandle = handle;
            FileName = fileName;
        }

        public IntPtr OpenFile(string fileName) {
            IntPtr handle;
            return !Stormlib.SFileOpenFileEx(mHandle, fileName, 0, out handle) ? IntPtr.Zero : handle;
        }

        public void AddFile(string fileName, byte[] content) {
            var handle = IntPtr.Zero;
            Stormlib.SFileCreateFile(mHandle, fileName, DateTime.Now.ToFileTime(), content.Length, 0,
                Stormlib.MPQFileFlags.Compress | Stormlib.MPQFileFlags.ReplaceExisting, ref handle);

            if (Marshal.GetLastWin32Error() != 0 || handle == IntPtr.Zero) {
                throw new Win32Exception(Marshal.GetLastWin32Error());
            }

            Stormlib.SFileWriteFile(handle, content, content.Length, Stormlib.MPQCompression.ZLib);
            Stormlib.SFileCloseFile(handle);
            Stormlib.SFileFlushArchive(mHandle);
        }

        public static MpqArchive CreateArchive(string filePath, int maxFiles) {
            var createMpqInfo = new Stormlib.SFILE_CREATE_MPQ {
                cbSize = Marshal.SizeOf<Stormlib.SFILE_CREATE_MPQ>(),
                cbUserData = 0,
                dwAttrFlags = 0,
                dwFileFlags = 0x80000000,
                dwFileFlags2 = 0,
                dwFileFlags3 = 0,
                dwMaxFileCount = maxFiles,
                dwMpqVersion = 1,
                dwRawChunkSize = 0,
                dwSectorSize = 4096,
                dwStreamFlags = 0,
                pvUserData = IntPtr.Zero
            };

            IntPtr handle;
            if (!Stormlib.SFileCreateArchive2(filePath, ref createMpqInfo, out handle)) {
                throw new InvalidOperationException("Unable to create new archive: " + filePath + ". Error code: " + Marshal.GetLastWin32Error());
            }

            return new MpqArchive(handle, filePath);
        }
    }
}
