using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Text;
using WoW.Streaming.Utils;

namespace WoW.Streaming.IO {
    class FileRequest : FileHandle {
        private static readonly Uri BaseRequestUri;
        private static readonly string AuthorizationHeader;

        private readonly MpqFile mBaseFile;
        private readonly string mFileName;
        private readonly FileHistory.Entry mEntry;

        public bool Exists { get; private set; }

        static FileRequest() {
            var config = Configuration.Instance;
            var host = config["server"]["host"];
            var port = config["server"]["port"];
            BaseRequestUri = new Uri("http://" + host + ":" + port);
            try {
                var userName = config["server"]["authentication"]["user"];
                var password = config["server"]["authentication"]["password"];
                var base64 = Convert.ToBase64String(Encoding.UTF8.GetBytes(userName + ":" + password));
                AuthorizationHeader = "Basic " + base64;
            } catch {
                AuthorizationHeader = "";
            }
        }

        public FileRequest(MpqFile file, string fileName, FileHistory.Entry entry) {
            mBaseFile = file;
            mFileName = fileName;
            mEntry = entry;
        }

        public void Process() {
            var builder = new UriBuilder(BaseRequestUri) { Path = "/files/" + mFileName.Replace('\\', '/') };
            var request = WebRequest.CreateHttp(builder.Uri);
            if (mBaseFile != null) {
                if (mEntry.Hash.HasValue == false) {
                    mEntry.Hash = mBaseFile.CalculateHash();
                }

                request.Headers.Add("X-Local-Version", "" + mEntry.Hash.Value);
            }

            if (!string.IsNullOrEmpty(AuthorizationHeader)) {
                request.Headers.Add("Authorization", AuthorizationHeader);
            }

            request.Method = "GET";
            HttpWebResponse response;
            try {
                response = request.GetResponse() as HttpWebResponse;
            } catch (WebException ex) {
                if (ex.Response == null) {
                    throw;
                }  

                response = ex.Response as HttpWebResponse;
            }

            if (response == null) {
                Exists = false;
                return;
            }

            switch (response.StatusCode) {
                case HttpStatusCode.OK:
                    OnNewFileReceived(response);
                    break;

                case HttpStatusCode.NotModified:
                    if (mBaseFile != null) {
                        Exists = true;
                        inputStream = mBaseFile.DataStream;
                    } else {
                        Exists = false;
                    }
                    break;

                default:
                    Exists = false;
                    break;
            }
        }

        private void OnNewFileReceived(WebResponse response) {
            try {
                Exists = true;
                using (var strm = response.GetResponseStream()) {
                    if (strm == null) {
                        Exists = false;
                        return;
                    }

                    var memStream = new MemoryStream();
                    inputStream = memStream;
                    strm.CopyTo(inputStream);
                    FileSystem.Instance.AppendStreamedFile(memStream.ToArray(), mFileName);
                }
            } catch (IOException) {
                Exists = false;
            }
        }
    }
}
