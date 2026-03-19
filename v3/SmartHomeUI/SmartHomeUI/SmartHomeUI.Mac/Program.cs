using System;
using Eto.Forms;

namespace SmartHomeUI.Mac
{
    class Program
    {
        [STAThread]
        public static void Main(string[] args)
        {
            var service = new SmartHomeService(args[0]);
            new Application(Eto.Platforms.Mac64).Run(new MainForm(service));
        }
    }
}