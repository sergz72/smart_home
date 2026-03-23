using System;
using Eto.Forms;
using SmartHomeService;

namespace SmartHomeUI.Gtk
{
    class Program
    {
        [STAThread]
        public static void Main(string[] args)
        {
            var service = new RedisSmartHomeService(args[0]);
            new Application(Eto.Platforms.Gtk).Run(new MainForm(service));
        }
    }
}