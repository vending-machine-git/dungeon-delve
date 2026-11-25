using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace DungeonDelve
{
    internal sealed class LaunchOptions
    {
        internal string GamePath;
        internal string ProfileRoot;
        internal string Report;
        internal bool CheckOnly;
        internal bool PrepareOnly;
        internal bool Framework;
    }

    internal static class Launcher
    {
        internal static readonly string Root = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
        internal const string PayloadFolder = "Dungeon-Delve-0.17.2";

        internal static string FindPayload()
        {
            string nested = Path.Combine(Root, PayloadFolder);
            string candidate = Directory.Exists(nested) ? nested : Root;
            if (!File.Exists(Path.Combine(candidate, "Launch.ps1")) || !File.Exists(Path.Combine(candidate, "release.json")))
                throw new IOException("Extract the complete Dungeon Delve ZIP into your Barony folder. Keep Dungeon Delve.exe beside barony.exe and the Dungeon-Delve-0.17.2 supporting folder. Launch.ps1 or release.json is missing.");
            return candidate;
        }

        // Windows command-line escaping, including trailing backslashes. No shell evaluates these arguments.
        internal static string Quote(string value)
        {
            StringBuilder result = new StringBuilder("\"");
            int slashes = 0;
            foreach (char character in value)
            {
                if (character == '\\') { slashes++; continue; }
                if (character == '"') result.Append('\\', slashes * 2 + 1);
                else result.Append('\\', slashes);
                result.Append(character);
                slashes = 0;
            }
            return result.Append('\\', slashes * 2).Append('"').ToString();
        }

        internal static object Run(LaunchOptions options)
        {
            string payloadRoot = FindPayload();
            string script = Path.Combine(payloadRoot, "Launch.ps1");
            List<string> arguments = new List<string> {
                "-NoLogo", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
                "-WindowStyle", "Hidden", "-File", script, "-Mode", options.Framework ? "Framework" : "Delve"
            };
            if (options.GamePath != null) { arguments.Add("-GamePath"); arguments.Add(options.GamePath); }
            if (options.ProfileRoot != null) { arguments.Add("-ProfileRoot"); arguments.Add(options.ProfileRoot); }
            if (options.CheckOnly) arguments.Add("-CheckOnly");
            if (options.PrepareOnly) arguments.Add("-PrepareOnly");
            string powershell = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),
                @"System32\WindowsPowerShell\v1.0\powershell.exe");
            ProcessStartInfo start = new ProcessStartInfo {
                FileName = powershell,
                Arguments = String.Join(" ", arguments.ConvertAll(Quote).ToArray()),
                WorkingDirectory = payloadRoot,
                UseShellExecute = false,
                CreateNoWindow = true,
                WindowStyle = ProcessWindowStyle.Hidden,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                StandardOutputEncoding = Encoding.UTF8,
                StandardErrorEncoding = Encoding.UTF8
            };
            using (Process process = Process.Start(start))
            {
                Task<string> output = process.StandardOutput.ReadToEndAsync();
                Task<string> error = process.StandardError.ReadToEndAsync();
                process.WaitForExit();
                Task.WaitAll(output, error);
                if (process.ExitCode != 0)
                    throw new IOException(String.IsNullOrWhiteSpace(error.Result) ? output.Result.Trim() : error.Result.Trim());
                object report = new JavaScriptSerializer().DeserializeObject(output.Result.Trim().TrimStart('\ufeff'));
                if (report == null) throw new IOException("The launcher did not return a startup result.");
                return report;
            }
        }

        internal static void WriteReport(string path, object result)
        {
            string text = new JavaScriptSerializer().Serialize(result) + Environment.NewLine;
            if (path != null) File.WriteAllText(path, text, new UTF8Encoding(false));
            else Console.Write(text);
        }

        [STAThread]
        private static int Main(string[] arguments)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            if (arguments.Length == 0)
            {
                Application.Run(new LaunchForm(new LaunchOptions()));
                return 0;
            }
            LaunchOptions options = new LaunchOptions();
            try
            {
                for (int index = 0; index < arguments.Length; index++)
                {
                    string option = arguments[index];
                    if (option == "--check") options.CheckOnly = true;
                    else if (option == "--prepare") options.PrepareOnly = true;
                    else if (option == "--framework") options.Framework = true;
                    else if (option == "--game-dir" || option == "--profile-root" || option == "--report")
                    {
                        if (++index >= arguments.Length) throw new ArgumentException("Missing value for " + option);
                        if (option == "--game-dir") options.GamePath = arguments[index];
                        else if (option == "--profile-root") options.ProfileRoot = arguments[index];
                        else options.Report = arguments[index];
                    }
                    else throw new ArgumentException("Unknown option: " + option);
                }
                if (options.CheckOnly && options.PrepareOnly) throw new ArgumentException("Choose --check or --prepare.");
                WriteReport(options.Report, Run(options));
                return 0;
            }
            catch (Exception error)
            {
                WriteReport(options.Report, new { status = "FAIL", error = error.Message });
                return 1;
            }
        }
    }

    internal sealed class LaunchForm : Form
    {
        private readonly LaunchOptions options;
        private readonly Label heading = new Label();
        private readonly TextBox status = new TextBox();
        private readonly ProgressBar progress = new ProgressBar();
        private readonly Button browse = new Button();
        private readonly Button retry = new Button();
        private bool busy;

        internal LaunchForm(LaunchOptions launchOptions)
        {
            options = launchOptions;
            Text = "Dungeon Delve";
            Icon = Icon.ExtractAssociatedIcon(Assembly.GetExecutingAssembly().Location);
            ClientSize = new Size(520, 270);
            Font = new Font("Segoe UI", 10);
            BackColor = Color.FromArgb(24, 28, 32);
            ForeColor = Color.FromArgb(236, 229, 209);
            StartPosition = FormStartPosition.CenterScreen;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            PictureBox mark = new PictureBox { Image = Icon.ToBitmap(), SizeMode = PictureBoxSizeMode.Zoom };
            mark.SetBounds(28, 25, 56, 56);
            heading.Text = "DUNGEON DELVE";
            heading.Font = new Font("Segoe UI Semibold", 19, FontStyle.Bold);
            heading.SetBounds(100, 27, 390, 42);
            status.SetBounds(30, 99, 460, 92);
            status.Multiline = true;
            status.ReadOnly = true;
            status.BorderStyle = BorderStyle.None;
            status.BackColor = BackColor;
            status.ForeColor = ForeColor;
            status.ScrollBars = ScrollBars.Vertical;
            status.TabStop = false;
            progress.Style = ProgressBarStyle.Marquee;
            progress.SetBounds(30, 212, 460, 12);
            browse.Text = "Choose Barony folder...";
            browse.SetBounds(30, 215, 215, 33);
            browse.Click += async delegate
            {
                using (FolderBrowserDialog dialog = new FolderBrowserDialog {
                    Description = "Choose the Barony game folder containing barony.exe", ShowNewFolderButton = false })
                {
                    if (options.GamePath != null && Directory.Exists(options.GamePath)) dialog.SelectedPath = options.GamePath;
                    if (dialog.ShowDialog(this) == DialogResult.OK) { options.GamePath = dialog.SelectedPath; await Start(); }
                }
            };
            retry.Text = "Try again";
            retry.SetBounds(362, 215, 128, 33);
            retry.Click += async delegate { await Start(); };
            Controls.AddRange(new Control[] { mark, heading, status, progress, browse, retry });
            Shown += async delegate { await Start(); };
            FormClosing += delegate(object sender, FormClosingEventArgs args) { if (busy) args.Cancel = true; };
        }

        private async Task Start()
        {
            busy = true;
            heading.Text = "DUNGEON DELVE";
            status.Text = "Starting your adventure...\nChecking Barony and preparing a fresh session.";
            progress.Visible = true;
            browse.Visible = retry.Visible = false;
            try
            {
                await Task.Run(() => Launcher.Run(options));
                busy = false;
                Close();
            }
            catch (Exception error)
            {
                heading.Text = "Unable to start";
                status.Text = error.Message;
                progress.Visible = false;
                browse.Visible = retry.Visible = true;
                busy = false;
            }
        }
    }
}
