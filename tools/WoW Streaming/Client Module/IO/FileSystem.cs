using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace WoW.Streaming.IO {
    class FileSystem {
        public static FileSystem Instance { get; private set; }

        static FileSystem() {
            Instance = new FileSystem();
        }

        private readonly List<MpqArchive> mArchives = new List<MpqArchive>();
        private readonly List<MpqArchive> mStreamArchives = new List<MpqArchive>();

        private readonly Dictionary<string, Task<FileRequest>> mPendingRequests = new Dictionary<string, Task<FileRequest>>();
        private readonly FileHistory mFileHistory = new FileHistory();

        public void Initialize() {
            var baseDir = Directory.GetCurrentDirectory();

            var streamDir = Path.Combine(baseDir, "Data", "streaming");
            Directory.CreateDirectory(streamDir);

            var baseFiles = Directory.GetFiles(Path.Combine(baseDir, "Data"), "*.mpq");
            var streamFiles = Directory.GetFiles(streamDir, "*.mpq");
            var localeFiles = new string[0];

            var locale = GetLocalePrefix();
            if (locale != null) {
                localeFiles = Directory.GetFiles(Path.Combine(baseDir, "Data", locale), "*.mpq");
            }

            SortRegularFiles(baseFiles);
            SortLocaleFiles(localeFiles);

            var baseArchives = GetValidArchives(baseFiles).ToList();
            var localeArchives = GetValidArchives(localeFiles).ToList();
            var streamArchives = GetValidArchives(streamFiles).ToList();

            foreach(var archive in streamArchives) mFileHistory.AddArchive(archive, true);
            foreach (var archive in localeArchives) mFileHistory.AddArchive(archive, false);
            foreach(var archive in baseArchives) mFileHistory.AddArchive(archive, false);

            mArchives.AddRange(baseArchives);
            mArchives.AddRange(localeArchives);
            mStreamArchives.AddRange(streamArchives);

            mArchives.Reverse();
            mStreamArchives.Reverse();
        }

        public FileRequest GetFile(string fileName) {
            fileName = fileName.ToLowerInvariant();
            Task<FileRequest> pendingRequest;
            lock (mPendingRequests) {
                if (mPendingRequests.ContainsKey(fileName)) {
                    pendingRequest = mPendingRequests[fileName];
                } else {
                    pendingRequest = Task.Factory.StartNew(() => RequestFile(fileName));
                    mPendingRequests.Add(fileName, pendingRequest);
                }
            }

            var result = pendingRequest.Result;
            return result;
        }

        public void AppendStreamedFile(byte[] content, string fileName) {
            lock (mStreamArchives) {
                var availableArchive = mStreamArchives.FirstOrDefault(a => a.CanAppendFiles);
                if (availableArchive != null) {
                    availableArchive.AddFile(fileName, content);
                } else {
                    var index = GetNextStreamingIndex();
                    var archiveName = string.Format(".\\Data\\streaming\\stream_{0:D3}.mpq", index);
                    var archive = MpqArchive.CreateArchive(archiveName, 1024);
                    mStreamArchives.Insert(0, archive);
                }
            }
        }

        private int GetNextStreamingIndex() {
            var pattern = new Regex("stream_(\\d){3}\\.mpq$", RegexOptions.IgnoreCase);
            var maxIndex = 0;
            foreach (var match in mStreamArchives.Select(archive => archive.FileName).Select(fileName => pattern.Match(fileName)).Where(match => match.Success)) {
                int index;
                if (!int.TryParse(match.Groups[1].Value, out index)) {
                    continue;
                }

                if (index >= maxIndex) {
                    maxIndex = index + 1;
                }
            }

            return maxIndex;
        }

        private FileRequest RequestFile(string fileName) {
            MpqFile file = null;

            try {
                var historyEntry = mFileHistory.GetFileEntry(fileName);
                if (historyEntry != null) {
                    var fileEntry = historyEntry.Archive.OpenFile(fileName);
                    if (fileEntry != IntPtr.Zero) {
                        file = new MpqFile(fileEntry);
                    }
                }

                if (file == null) {
                    lock (mStreamArchives) {
                        foreach (var archive in mStreamArchives) {
                            var handle = archive.OpenFile(fileName);
                            if (handle == IntPtr.Zero) {
                                continue;
                            }

                            file = new MpqFile(handle);
                            break;
                        }
                    }

                    if (file == null) {
                        foreach (var archive in mArchives) {
                            lock (archive) {
                                var handle = archive.OpenFile(fileName);
                                if (handle == IntPtr.Zero) {
                                    continue;
                                }

                                file = new MpqFile(handle);
                                break;
                            }
                        }
                    }
                }

                var ret = new FileRequest(file, fileName, historyEntry);
                ret.Process();

                lock (mPendingRequests) {
                    mPendingRequests.Remove(fileName);
                }

                return ret.Exists == false ? null : ret;
            } catch (Exception) {
                return null;
            } finally {
                if (file != null) {
                    file.Dispose();
                }  
            }
        }

        private static IEnumerable<MpqArchive> GetValidArchives(IEnumerable<string> files) {
            return files.Select(v => {
                try { return new MpqArchive(v); }
                catch { return null; }
            }).Where(v => v != null);
        } 

        private static void SortLocaleFiles(string[] files) {
            var patchMatch = new Regex("^patch-(locale|speech)-([0-9a-z]+)\\.mpq$", RegexOptions.IgnoreCase);
            var specialPatchMatch = new Regex("^(expansion|lichking)-(locale|speech)-patch-[a-z]{4}\\.mpq$");

            Array.Sort(files, (a, b) => {
                var patch1 = patchMatch.Match(a);
                var patch2 = patchMatch.Match(b);
                var spatch1 = specialPatchMatch.Match(a);
                var spatch2 = specialPatchMatch.Match(b);

                if (patch1.Success && !patch2.Success) {
                    return -1;
                }

                if (patch2.Success && !patch1.Success) {
                    return 1;
                }

                if (!patch1.Success && !patch2.Success) {
                    if (spatch1.Success && !spatch2.Success) {
                        return -1;
                    }

                    if (spatch2.Success && !spatch2.Success) {
                        return 1;
                    }

                    if (!spatch1.Success && !spatch2.Success) {
                        return -string.Compare(a, b, StringComparison.OrdinalIgnoreCase);
                    }

                    var prefix1 = spatch1.Groups[1].Value;
                    var prefix2 = spatch2.Groups[1].Value;

                    return string.Compare(prefix1, prefix2, StringComparison.OrdinalIgnoreCase);
                }

                var suffix1 = patch1.Groups[2].Value;
                var suffix2 = patch2.Groups[2].Value;

                return -string.Compare(suffix1, suffix2, StringComparison.OrdinalIgnoreCase);
            });
        }

        private static void SortRegularFiles(string[] files) {
            var patchMatch = new Regex("^patch-([0-9a-z]+)\\.mpq$", RegexOptions.IgnoreCase);

            Array.Sort(files, (a, b) => {
                var patch1 = patchMatch.Match(a);
                var patch2 = patchMatch.Match(b);

                if (!patch1.Success && patch2.Success) {
                    return -1;
                }

                if (!patch2.Success && patch1.Success) {
                    return 1;
                }

                if (!patch2.Success && !patch1.Success) {
                    return -string.Compare(a, b, StringComparison.OrdinalIgnoreCase);
                }

                var patchName1 = patch1.Groups[1].Value;
                var patchName2 = patch2.Groups[1].Value;

                return -string.Compare(patchName1, patchName2, StringComparison.OrdinalIgnoreCase);
            });
        }

        private static string GetLocalePrefix() {
            var basePath = Directory.GetCurrentDirectory();
            var configPath = Path.Combine(basePath, "WTF", "Config.wtf");
            if (File.Exists(configPath)) {
                var lines = File.ReadAllLines(configPath);
                foreach (var line in lines) {
                    var parts = line.Trim().Split(' ');
                    if (parts.Length != 3 || line.StartsWith("#")) {
                        continue;
                    }

                    var key = parts[1].ToLowerInvariant();
                    if (key != "locale") {
                        continue;
                    }

                    var localeName = parts[2];
                    localeName = localeName.Substring(1, localeName.Length - 2);
                    if (Directory.Exists(Path.Combine(basePath, "Data", localeName))) {
                        return localeName;
                    }
                }
            }

            var locales = new[] {
                "deDE",
                "enUS",
                "frFR",
                "enGB",
                "itIT",
                "koKR",
                "zhCN",
                "zhTW",
                "ruRU",
                "esES",
                "esMX",
                "ptBR"
            };

            return locales.FirstOrDefault(locale => Directory.Exists(Path.Combine(basePath, "Data", locale)));
        }
    }
}
