using System;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

internal static class LauncherUiSmoke
{
    private delegate bool WindowCallback(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] private static extern bool EnumThreadWindows(uint thread, WindowCallback callback, IntPtr parameter);
    [DllImport("kernel32.dll")] private static extern uint GetCurrentThreadId();
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetClassName(IntPtr window, StringBuilder text, int count);
    [DllImport("user32.dll")] private static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr first, IntPtr second);

    private static object Field(object value, string name)
    {
        return value.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(value);
    }

    [STAThread]
    private static int Main(string[] arguments)
    {
        Application.EnableVisualStyles();
        Assembly assembly = Assembly.LoadFile(Path.GetFullPath(arguments[0]));
        Type optionsType = assembly.GetType("DungeonDelve.LaunchOptions");
        object options = Activator.CreateInstance(optionsType, true);
        Action<string, object> set = (name, value) => optionsType.GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(options, value);
        set("GamePath", Path.Combine(arguments[3], "absent game"));
        set("ProfileRoot", Path.Combine(arguments[3], "gui sessions"));
        set("PrepareOnly", true);
        Form form = (Form)Activator.CreateInstance(assembly.GetType("DungeonDelve.LaunchForm"), BindingFlags.Instance | BindingFlags.NonPublic, null, new[] { options }, null);
        form.Opacity = 0;
        form.ShowInTaskbar = false;
        Button browse = (Button)Field(form, "browse");
        Button retry = (Button)Field(form, "retry");
        TextBox status = (TextBox)Field(form, "status");
        int stage = 0;
        int ticks = 0;
        string error = null;
        bool starting = false;
        bool closed = false;
        Timer timer = new Timer { Interval = 100 };
        form.FormClosed += delegate { closed = true; };
        form.Shown += delegate
        {
            starting = status.Text.StartsWith("Starting your adventure") && form.Icon != null;
            using (Bitmap bitmap = new Bitmap(form.Width, form.Height))
            {
                form.DrawToBitmap(bitmap, new Rectangle(0, 0, bitmap.Width, bitmap.Height));
                bitmap.Save(Path.Combine(arguments[3], "launcher-startup.png"));
            }
            timer.Start();
        };
        timer.Tick += delegate
        {
            try
            {
                if (++ticks > 600) throw new Exception("Launcher UI timed out at stage " + stage);
                if (stage == 0 && !(bool)Field(form, "busy") && browse.Visible && retry.Visible)
                {
                    if (!status.Text.Contains("Choose the Barony game folder")) throw new Exception("Missing readable failure: " + status.Text);
                    using (Bitmap bitmap = new Bitmap(form.Width, form.Height))
                    {
                        form.DrawToBitmap(bitmap, new Rectangle(0, 0, bitmap.Width, bitmap.Height));
                        bitmap.Save(Path.Combine(arguments[3], "launcher-error.png"));
                    }
                    stage = 1;
                    browse.PerformClick();
                }
                else if (stage == 1)
                {
                    EnumThreadWindows(GetCurrentThreadId(), delegate(IntPtr window, IntPtr parameter)
                    {
                        StringBuilder className = new StringBuilder(256);
                        GetClassName(window, className, className.Capacity);
                        if (className.ToString() == "#32770") { stage = 2; SendMessage(window, 0x0010, IntPtr.Zero, IntPtr.Zero); return false; }
                        return true;
                    }, IntPtr.Zero);
                }
                else if (stage == 2)
                {
                    set("GamePath", arguments[1]);
                    stage = 3;
                    retry.PerformClick();
                }
            }
            catch (Exception failure) { error = failure.ToString(); timer.Stop(); Application.Exit(); }
        };
        Application.Run(form);
        timer.Stop();
        bool passed = starting && closed && stage == 3 && error == null && Directory.Exists(Path.Combine(arguments[3], "gui sessions"));
        File.WriteAllText(arguments[2], passed ? "PASS" : "FAIL " + stage + " " + error);
        return passed ? 0 : 1;
    }
}
