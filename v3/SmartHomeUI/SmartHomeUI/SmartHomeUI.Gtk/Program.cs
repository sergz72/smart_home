using System;
using Eto.Forms;

namespace SmartHomeUI.Gtk
{
    class Program
    {
        [STAThread]
        public static void Main(string[] args)
        {
            var service = new SmartHomeService(args[0]);
            new Application(Eto.Platforms.Gtk).Run(new MainForm(service));
        }
    }
}