namespace WoW.Streaming.Utils {
    internal static class Murmur {
        private const uint C1 = 0xCC9E2D51;
        private const uint C2 = 0x1B873593;
        private const int R1 = 15;
        private const int R2 = 13;
        private const uint M = 5;
        private const uint N = 0xE6546B64;

        public static unsafe uint Hash(byte[] data, uint seed) {
            var hash = seed;
            var nblocks = data.Length / 4;

            fixed (byte* ptr = data) {
                var blocks = (uint*) ptr;
                for (var i = 0; i < nblocks; ++i) {
                    var k = blocks[i];
                    k *= C1;
                    k = (k << R1) | (k >> (32 - R1));
                    k *= C2;

                    hash ^= k;
                    hash = ((hash << R2) | (hash >> (32 - R2))) * M + N;
                }

                var tail = ptr + nblocks * 4;
                uint k1 = 0;
                switch (data.Length & 3) {
                    case 3:
                        k1 ^= (uint)(tail[2] << 16);
                        goto case 2;

                    case 2:
                        k1 ^= (uint) (tail[1] << 8);
                        goto case 1;

                    case 1:
                        k1 ^= tail[0];

                        k1 *= C1;
                        k1 = (k1 << R1) | (k1 >> (32 - R1));
                        k1 *= C2;
                        hash ^= k1;
                        break;
                }

                hash ^= (uint) data.Length;
                hash ^= (hash >> 16);
                hash *= 0x85EBCA6B;
                hash ^= hash >> 13;
                hash *= 0xC2B2AE35;
                hash ^= hash >> 16;

                return hash;
            }
        }
    }
}
