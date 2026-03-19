using System;
using Eto.Forms;

namespace SmartHomeUI.Wpf
{
    class Program
    {
        [STAThread]
        public static void Main(string[] args)
        {
            var service = new SmartHomeService(args[0]);
            new Application(Eto.Platforms.Wpf).Run(new MainForm(service));
        }
    }
}