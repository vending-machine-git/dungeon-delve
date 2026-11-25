using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Web.Script.Serialization;
using System.Windows.Forms;

internal static class InstallerUiSmoke
{
    private delegate bool WindowCallback(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")]
    private static extern bool EnumThreadWindows(uint thread, WindowCallback callback, IntPtr parameter);
    [DllImport("user32.dll")]
    private static extern bool EnumChildWindows(IntPtr parent, WindowCallback callback, IntPtr parameter);
    [DllImport("kernel32.dll")]
    private static extern uint GetCurrentThreadId();
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(IntPtr window, StringBuilder text, int count);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(IntPtr window, StringBuilder text, int count);
    [DllImport("user32.dll")]
    private static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr first, IntPtr second);

    private static object Field(Form form, string name)
    {
        return form.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(form);
    }

    [STAThread]
    private static int Main(string[] arguments)
    {
        Application.EnableVisualStyles();
        Assembly assembly = Assembly.LoadFile(Path.GetFullPath(arguments[0]));
        Form form = (Form)Activator.CreateInstance(assembly.GetType("DungeonDelve.InstallForm"), true);
        form.Opacity = 0;
        form.ShowInTaskbar = false;
        ComboBox folder = (ComboBox)Field(form, "folder");
        Button install = (Button)Field(form, "install");
        Button browse = (Button)Field(form, "browse");
        CheckBox shortcut = (CheckBox)Field(form, "shortcut");
        Label status = (Label)Field(form, "status");
        int stage = 0;
        int ticks = 0;
        string error = null;
        bool browseDialog = false;
        bool successDialog = false;
        bool defaults = shortcut.Checked && folder.DropDownStyle == ComboBoxStyle.DropDown;
        Timer timer = new Timer { Interval = 100 };
        timer.Tick += delegate
        {
            try
            {
                if (++ticks > 600) throw new Exception("UI test timed out at stage " + stage);
                if (stage == 0 && install.Enabled && folder.Items.Count > 0)
                {
                    folder.Text = arguments[1];
                    stage = 1;
                    browse.PerformClick();
                }
                else if (stage == 1 || stage == 3)
                {
                    EnumThreadWindows(GetCurrentThreadId(), delegate(IntPtr window, IntPtr parameter)
                    {
                        StringBuilder className = new StringBuilder(256);
                        GetClassName(window, className, className.Capacity);
                        if (className.ToString() != "#32770") return true;
                        StringBuilder caption = new StringBuilder(256);
                        GetWindowText(window, caption, caption.Capacity);
                        if (stage == 1)
                        {
                            browseDialog = true;
                            stage = 2;
                        }
                        else
                        {
                            successDialog = caption.ToString() == "Installation complete";
                            if (!successDialog)
                            {
                                StringBuilder details = new StringBuilder();
                                EnumChildWindows(window, delegate(IntPtr child, IntPtr unused)
                                {
                                    StringBuilder text = new StringBuilder(2048);
                                    GetWindowText(child, text, text.Capacity);
                                    details.AppendLine(text.ToString());
                                    return true;
                                }, IntPtr.Zero);
                                error = "Installation dialog was not successful: " + caption + "\n" + details;
                            }
                            stage = 4;
                        }
                        SendMessage(window, 0x0010, IntPtr.Zero, IntPtr.Zero);
                        return false;
                    }, IntPtr.Zero);
                }
                else if (stage == 2)
                {
                    if (folder.Text != arguments[1]) throw new Exception("Cancelling Browse changed the manual path.");
                    shortcut.Checked = false;
                    stage = 3;
                    install.PerformClick();
                }
                else if (stage == 4 && install.Enabled)
                {
                    if (!status.Text.StartsWith("Installed successfully")) throw new Exception(error ?? status.Text);
                    timer.Stop();
                    form.Close();
                }
            }
            catch (Exception failure)
            {
                error = failure.ToString();
                timer.Stop();
                Application.Exit();
            }
        };
        form.Shown += delegate { timer.Start(); };
        Application.Run(form);
        string shortcutPath = Path.Combine(arguments[2], "Dungeon Delve 0.20.4.lnk");
        try
        {
            Type installer = assembly.GetType("DungeonDelve.Installer");
            MethodInfo create = installer.GetMethod("CreateShortcut", BindingFlags.Static | BindingFlags.NonPublic);
            string destination = Path.Combine(arguments[1], "Dungeon-Delve-0.20.4");
            create.Invoke(null, new object[] { destination, arguments[2] });
            byte[] original = File.ReadAllBytes(shortcutPath);
            create.Invoke(null, new object[] { destination, arguments[2] });
            if (!Convert.ToBase64String(original).Equals(Convert.ToBase64String(File.ReadAllBytes(shortcutPath))))
                throw new Exception("Existing shortcut was overwritten.");
        }
        catch (Exception failure) { error = failure.ToString(); }
        bool passed = error == null && defaults && browseDialog && successDialog && stage == 4 && File.Exists(shortcutPath);
        Dictionary<string, object> result = new Dictionary<string, object> {
            { "status", passed ? "PASS" : "FAIL" }, { "automatic_scan_populated", stage > 0 },
            { "manual_path", arguments[1] }, { "browse_dialog_cancelled", browseDialog },
            { "installation_success_dialog", successDialog }, { "default_controls", defaults },
            { "isolated_shortcut", shortcutPath }, { "error", error }
        };
        File.WriteAllText(arguments[3], new JavaScriptSerializer().Serialize(result));
        return passed ? 0 : 1;
    }
}
