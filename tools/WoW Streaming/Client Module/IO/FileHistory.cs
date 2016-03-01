using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WoW.Streaming.IO {
    class FileHistory {
        public class Entry {
            public string Name { get; set; }
            public uint? Hash { get; set; }
            public MpqArchive Archive { get; set; }
            public bool IsStreamed { get; set; }
        }

        private readonly Dictionary<int, Entry> mFileCache = new Dictionary<int, Entry>();

        public Entry GetFileEntry(string fileName) {
            var hash = fileName.ToUpperInvariant().GetHashCode();

            lock (mFileCache) {
                return mFileCache.ContainsKey(hash) ? mFileCache[hash] : null;
            }
        }

        public void AddArchive(MpqArchive archive, bool streamArchive) {
            var file = archive.OpenFile("(listfile)");
            if (file == IntPtr.Zero) {
                return;
            }

            StreamReader content;
            using (var mf = new MpqFile(file)) {
                content = new StreamReader(mf.DataStream);
            }

            var line = content.ReadLine();
            lock (mFileCache) {
                while (line != null) {
                    var curLine = line.Trim();
                    line = content.ReadLine();

                    var hash = curLine.ToUpperInvariant().GetHashCode();
                    if (mFileCache.ContainsKey(hash)) {
                        continue;
                    }

                    var entry = new Entry {
                        Name = curLine,
                        Archive = archive,
                        IsStreamed = streamArchive
                    };

                    mFileCache.Add(hash, entry);
                }
            }
        }
    }
}
