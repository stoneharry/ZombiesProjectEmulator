using System;
using System.IO;
using WoW.Streaming.Utils;

namespace WoW.Streaming.IO {
    class MpqFile : IDisposable {
        private static readonly Crc32 Hasher = new Crc32();

        public MemoryStream DataStream {
            get {
                AssertReadFile();
                return mBackingStream;
            }
        }

        private IntPtr mHandle;
        private MemoryStream mBackingStream;

        public MpqFile(IntPtr handle) {
            mHandle = handle;
        }

        public uint CalculateHash() {
            return Hasher.ComputeChecksum(DataStream.ToArray());
        }

        public void Dispose() {
            if (mHandle == IntPtr.Zero) return;
            
            Stormlib.SFileCloseFile(mHandle);
            mHandle = IntPtr.Zero;
        }

        private void AssertReadFile() {
            if (mBackingStream != null) return;
            
            uint sizeHigh = 0;
            var sizeLow = Stormlib.SFileGetFileSize(mHandle, ref sizeHigh);
            var size = (long) sizeLow | sizeHigh << 32;

            var content = new byte[size];
            int numRead;

            Stormlib.SFileReadFile(mHandle, content, (int) size, out numRead);
            Stormlib.SFileCloseFile(mHandle);
            mHandle = IntPtr.Zero;
            mBackingStream = new MemoryStream(content);
        }
    }
}
