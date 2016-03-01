using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace WoW.Streaming.IO {
    static class Stormlib {
        [StructLayout(LayoutKind.Sequential)]
        public struct SFILE_CREATE_MPQ {
            public int cbSize;
            public int dwMpqVersion;
            public IntPtr pvUserData;
            public int cbUserData;
            public int dwStreamFlags;
            public uint dwFileFlags;
            public int dwFileFlags2;
            public int dwFileFlags3;
            public int dwAttrFlags;
            public int dwSectorSize;
            public int dwRawChunkSize;
            public int dwMaxFileCount;
        }

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern bool SFileOpenArchive(string szMpqName, int dwPriority, int flags, out IntPtr handle);

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall, SetLastError = true)]
        public static extern bool SFileCreateArchive2(string mpqName, ref SFILE_CREATE_MPQ createInfo, out IntPtr handle);

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern bool SFileFlushArchive(IntPtr handle);

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern bool SFileCloseArchive(IntPtr handle);

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern bool SFileSignArchive(IntPtr handle, int signatureType = 0);

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern bool SFileCompactArchive(IntPtr handle, IntPtr szListFile, bool reserved = false);

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern bool SFileOpenFileEx(IntPtr handle, string fileName, int searchScope, out IntPtr hFile);

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern uint SFileGetFileSize(IntPtr hFile, ref uint fileSizeHigh);

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern int SFileSetFilePointer(IntPtr hFile, int filePos, ref int filePosHigh, int moveMethod);

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern bool SFileReadFile(IntPtr hFile, byte[] buffer, int toRead, out int numRead,
            int lpOverlapped = 0);

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern bool SFileCloseFile(IntPtr hFile);

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern bool SFileGetFileInfo(IntPtr handle, SFileInfoClass infoClass, byte[] fileInfo, int sizeBuffer,
            out int sizeRequired);

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall, SetLastError = true)]
        public static extern bool SFileCreateFile(IntPtr handle, string archiveName, long fileTime, int fileSize,
            int locale, MPQFileFlags flags, ref IntPtr hFile);

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern bool SFileFinishFile(IntPtr hFile);

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern bool SFileWriteFile(IntPtr hFile, byte[] buffer, int size, MPQCompression compression);

        [DllImport("Stormlib.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern bool SFileRemoveFile(IntPtr handle, string fileName, int searchScope = 0);

        [Flags]
        public enum MPQFileFlags : uint {
            Implode = 0x00000100,
            Compress = 0x00000200,
            Encrypted = 0x00010000,
            FixKey = 0x00020000,
            DeleteMarker = 0x02000000,
            SectorCrc = 0x04000000,
            ReplaceExisting = 0x80000000
        }

        [Flags]
        public enum MPQCompression : uint {
            Huffmann = 0x01,
            ZLib = 0x02,
            PKWare = 0x08,
            BZip2 = 0x10,
            Sparse = 0x20,
            ADPCM_Mono = 0x40,
            ADPCM_Stereo = 0x80,
            LZMA = 0x12
        }

        public enum SFileInfoClass : uint {
            // Info classes for archives
            SFileMpqFileName, // Name of the archive file (TCHAR [])
            SFileMpqStreamBitmap, // Array of bits, each bit means availability of one block (BYTE [])
            SFileMpqUserDataOffset, // Offset of the user data header (ULONGLONG)
            SFileMpqUserDataHeader, // Raw (unfixed) user data header (TMPQUserData)
            SFileMpqUserData, // MPQ USer data, without the header (BYTE [])
            SFileMpqHeaderOffset, // Offset of the MPQ header (ULONGLONG)
            SFileMpqHeaderSize, // Fixed size of the MPQ header
            SFileMpqHeader, // Raw (unfixed) archive header (TMPQHeader)
            SFileMpqHetTableOffset, // Offset of the HET table, relative to MPQ header (ULONGLONG)
            SFileMpqHetTableSize, // Compressed size of the HET table (ULONGLONG)
            SFileMpqHetHeader, // HET table header (TMPQHetHeader)
            SFileMpqHetTable, // HET table as pointer. Must be freed using SFileFreeFileInfo
            SFileMpqBetTableOffset, // Offset of the BET table, relative to MPQ header (ULONGLONG)
            SFileMpqBetTableSize, // Compressed size of the BET table (ULONGLONG)
            SFileMpqBetHeader, // BET table header, followed by the flags (TMPQBetHeader + DWORD[])
            SFileMpqBetTable, // BET table as pointer. Must be freed using SFileFreeFileInfo
            SFileMpqHashTableOffset, // Hash table offset, relative to MPQ header (ULONGLONG)
            SFileMpqHashTableSize64, // Compressed size of the hash table (ULONGLONG)
            SFileMpqHashTableSize, // Size of the hash table, in entries (DWORD)
            SFileMpqHashTable, // Raw (unfixed) hash table (TMPQBlock [])
            SFileMpqBlockTableOffset, // Block table offset, relative to MPQ header (ULONGLONG)
            SFileMpqBlockTableSize64, // Compressed size of the block table (ULONGLONG)
            SFileMpqBlockTableSize, // Size of the block table, in entries (DWORD)
            SFileMpqBlockTable, // Raw (unfixed) block table (TMPQBlock [])
            SFileMpqHiBlockTableOffset, // Hi-block table offset, relative to MPQ header (ULONGLONG)
            SFileMpqHiBlockTableSize64, // Compressed size of the hi-block table (ULONGLONG)
            SFileMpqHiBlockTable, // The hi-block table (USHORT [])
            SFileMpqSignatures, // Signatures present in the MPQ (DWORD)
            SFileMpqStrongSignatureOffset,
            // Byte offset of the strong signature, relative to begin of the file (ULONGLONG)
            SFileMpqStrongSignatureSize, // Size of the strong signature (DWORD)
            SFileMpqStrongSignature, // The strong signature (BYTE [])
            SFileMpqArchiveSize64, // Archive size from the header (ULONGLONG)
            SFileMpqArchiveSize, // Archive size from the header (DWORD)
            SFileMpqMaxFileCount, // Max number of files in the archive (DWORD)
            SFileMpqFileTableSize, // Number of entries in the file table (DWORD)
            SFileMpqSectorSize, // Sector size (DWORD)
            SFileMpqNumberOfFiles, // Number of files (DWORD)
            SFileMpqRawChunkSize, // Size of the raw data chunk for MD5
            SFileMpqStreamFlags, // Stream flags (DWORD)
            SFileMpqIsReadOnly, // Nonzero if the MPQ is read only (DWORD)

            // Info classes for files
            SFileInfoPatchChain, // Chain of patches where the file is (TCHAR [])
            SFileInfoFileEntry, // The file entry for the file (TFileEntry)
            SFileInfoHashEntry, // Hash table entry for the file (TMPQHash)
            SFileInfoHashIndex, // Index of the hash table entry (DWORD)
            SFileInfoNameHash1, // The first name hash in the hash table (DWORD)
            SFileInfoNameHash2, // The second name hash in the hash table (DWORD)
            SFileInfoNameHash3, // 64-bit file name hash for the HET/BET tables (ULONGLONG)
            SFileInfoLocale, // File locale (DWORD)
            SFileInfoFileIndex, // Block index (DWORD)
            SFileInfoByteOffset, // File position in the archive (ULONGLONG)
            SFileInfoFileTime, // File time (ULONGLONG)
            SFileInfoFileSize, // Size of the file (DWORD)
            SFileInfoCompressedSize, // Compressed file size (DWORD)
            SFileInfoFlags, // File flags from (DWORD)
            SFileInfoEncryptionKey, // File encryption key
            SFileInfoEncryptionKeyRaw, // Unfixed value of the file key
        }
    }
}
