using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WoW.Streaming
{
    public class Main
    {
        public static int main(String arg) {
            AppDomain.CurrentDomain.UnhandledException += (sender, args) => {
                Console.WriteLine(args.ExceptionObject);
            };

            try {
                IO.FileSystem.Instance.Initialize();
                IO.StormHooks.Instance.Initialize();
            }
            catch (Exception ex) {
                Console.WriteLine(ex);
            }

            return 0;
        }
    }
}
