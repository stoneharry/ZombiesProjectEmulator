using System;
using System.Diagnostics;

namespace WoW.Streaming.Utils {
    public class Crc32 {
        private readonly uint[] mTable;

        public uint ComputeChecksum(byte[] bytes) {
            var crc = 0xffffffff;
            for (var i = 0; i < bytes.Length; ++i) {
                var index = (byte) ((crc & 0xff) ^ bytes[i]);
                crc = (crc >> 8) ^ mTable[index];
            }

            return ~crc;
        }

        public byte[] ComputeChecksumBytes(byte[] bytes) {
            return BitConverter.GetBytes(ComputeChecksum(bytes));
        }

        public Crc32() {
            const uint poly = 0xedb88320;
            mTable = new uint[256];
            for (uint i = 0; i < mTable.Length; ++i) {
                var temp = i;
                for (var j = 8; j > 0; --j) {
                    if ((temp & 1) == 1) {
                        temp = (temp >> 1) ^ poly;
                    } else {
                        temp >>= 1;
                    }
                }
                mTable[i] = temp;
            }
        }
    }
}
