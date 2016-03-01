using System.IO;
using YamlDotNet.Serialization;
using YamlDotNet.Serialization.NamingConventions;

namespace WoW.Streaming.Utils {
    class Configuration {
        public static dynamic Instance { get; private set; }

        static Configuration() {
            using (var input = File.OpenRead("Streaming.yaml")) {
                Instance = new Deserializer(namingConvention: new CamelCaseNamingConvention()).Deserialize(new StreamReader(input));
            }
        }
    }
}
