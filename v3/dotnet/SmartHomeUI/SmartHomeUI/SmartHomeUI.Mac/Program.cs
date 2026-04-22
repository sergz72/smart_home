using System;
using Eto.Forms;
using SmartHomeService;

namespace SmartHomeUI.Mac
{
    class Program
    {
        [STAThread]
        public static void Main(string[] args)
        {
            var service = ISmartHomeService.Create(args[0]);
            new Application(Eto.Platforms.Mac64).Run(new MainForm(service));
        }
    }
}