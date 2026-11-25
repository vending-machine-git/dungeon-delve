using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Win32;

namespace DungeonDelve
{
    internal static class Installer
    {
        internal static readonly JavaScriptSerializer Json = new JavaScriptSerializer();
        internal static readonly Dictionary<string, object> Config = ReadResourceJson("installer.json");
        internal static readonly Dictionary<string, object> Release = (Dictionary<string, object>)Config["release"];
        internal static string Version { get { return (string)Release["version"]; } }
        internal static string Folder { get { return "Dungeon-Delve-" + Version; } }
        internal static string LauncherName { get { return ((Dictionary<string, object>)Config["files"]).ContainsKey("Dungeon Delve.exe") ? "Dungeon Delve.exe" : "Play-Dungeon-Delve.cmd"; } }
        internal static bool RootLauncher { get { return Config.ContainsKey("root_launcher"); } }
        internal static string LauncherDirectory(string destination) { return RootLauncher ? Path.GetDirectoryName(destination) : destination; }

        internal static void CheckRootLauncher(string game)
        {
            if (!RootLauncher) return;
            string target = Path.Combine(game, "Dungeon Delve.exe");
            RejectLinks(game, target);
            if (Directory.Exists(target)) throw new IOException("A folder already occupies " + target + ". Nothing was overwritten.");
            if (File.Exists(target))
            {
                string hash = FileHash(target);
                if (hash != (string)Config["root_launcher"] && hash != "19a370f6d6f17c7da739ddb843f621fe363c1ca2aa3cf3667f0d0f179e2d3bcc")
                    throw new IOException("A different Dungeon Delve.exe already exists beside barony.exe. Rename that file before installing. Nothing was overwritten.");
            }
        }

        internal static void InstallRootLauncher(string game)
        {
            if (!RootLauncher) return;
            CheckRootLauncher(game);
            string target = Path.Combine(game, "Dungeon Delve.exe");
            if (File.Exists(target) && FileHash(target) == (string)Config["root_launcher"]) return;
            string temporary = Path.Combine(game, ".dd-launcher-" + Guid.NewGuid().ToString("N"));
            string backup = temporary + ".bak";
            try
            {
                using (Stream payload = Assembly.GetExecutingAssembly().GetManifestResourceStream("payload.zip"))
                {
                    if (Hash(payload) != (string)Config["archive_sha256"]) throw new IOException("Installer payload checksum failed. Download a fresh installer.");
                    payload.Position = 0;
                    using (ZipArchive archive = new ZipArchive(payload, ZipArchiveMode.Read))
                    {
                        ZipArchiveEntry entry = archive.GetEntry("Dungeon Delve.exe");
                        if (entry == null) throw new IOException("The game-root launcher is missing from the installer.");
                        using (Stream input = entry.Open())
                        using (FileStream output = new FileStream(temporary, FileMode.CreateNew)) input.CopyTo(output);
                    }
                }
                if (FileHash(temporary) != (string)Config["root_launcher"]) throw new IOException("Extracted launcher checksum failed.");
                CheckRootLauncher(game);
                if (File.Exists(target)) File.Replace(temporary, target, backup);
                else File.Move(temporary, target);
            }
            finally
            {
                if (File.Exists(temporary)) File.Delete(temporary);
                if (File.Exists(backup)) File.Delete(backup);
            }
        }

        internal static Dictionary<string, object> ReadResourceJson(string name)
        {
            using (Stream stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(name))
            using (StreamReader reader = new StreamReader(stream, Encoding.UTF8))
                return Json.Deserialize<Dictionary<string, object>>(reader.ReadToEnd());
        }

        internal static string Hash(Stream stream)
        {
            using (SHA256 algorithm = SHA256.Create())
                return BitConverter.ToString(algorithm.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
        }

        internal static string FileHash(string path)
        {
            using (FileStream stream = File.OpenRead(path)) return Hash(stream);
        }

        internal static string Normalize(string path)
        {
            return Path.GetFullPath(path.Trim().Trim('"')).TrimEnd(Path.DirectorySeparatorChar);
        }

        internal static List<string> Detect(IEnumerable<string> explicitRoots)
        {
            HashSet<string> roots = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            if (explicitRoots != null)
            {
                foreach (string root in explicitRoots) roots.Add(Normalize(root));
            }
            else
            {
                foreach (RegistryHive hive in new[] { RegistryHive.CurrentUser, RegistryHive.LocalMachine })
                foreach (RegistryView view in new[] { RegistryView.Registry32, RegistryView.Registry64 })
                {
                    try
                    {
                        using (RegistryKey baseKey = RegistryKey.OpenBaseKey(hive, view))
                        using (RegistryKey key = baseKey.OpenSubKey(@"Software\Valve\Steam"))
                        {
                            if (key == null) continue;
                            object value = key.GetValue("SteamPath") ?? key.GetValue("InstallPath");
                            if (value is string) roots.Add(Normalize((string)value));
                        }
                    }
                    catch (System.Security.SecurityException) { }
                    catch (UnauthorizedAccessException) { }
                }
                foreach (string variable in new[] { "ProgramFiles(x86)", "ProgramFiles" })
                {
                    string directory = Environment.GetEnvironmentVariable(variable);
                    if (!String.IsNullOrEmpty(directory)) roots.Add(Path.Combine(directory, "Steam"));
                }
            }
            HashSet<string> libraries = new HashSet<string>(roots, StringComparer.OrdinalIgnoreCase);
            foreach (string root in roots)
            {
                string text = ReadOptional(Path.Combine(root, "steamapps", "libraryfolders.vdf"));
                foreach (Match match in Regex.Matches(text, "\"(?:path|[0-9]+)\"\\s+\"([^\"]+)\""))
                {
                    string value = match.Groups[1].Value.Replace(@"\\", @"\");
                    if (Path.IsPathRooted(value)) libraries.Add(Normalize(value));
                }
            }
            HashSet<string> games = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (string library in libraries)
            {
                string common = Path.Combine(library, "steamapps", "common");
                AddGame(games, Path.Combine(common, "Barony"));
                string manifest = ReadOptional(Path.Combine(library, "steamapps", "appmanifest_371970.acf"));
                Match match = Regex.Match(manifest, "\"installdir\"\\s+\"([^\"]+)\"", RegexOptions.IgnoreCase);
                if (match.Success)
                {
                    string directory = match.Groups[1].Value;
                    if (directory != "." && directory != ".." && directory.IndexOfAny(new[] { '\\', '/', ':' }) < 0)
                        AddGame(games, Path.Combine(common, directory));
                }
            }
            if (explicitRoots == null)
            {
                string adjacent = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
                AddGame(games, adjacent);
                AddGame(games, Path.GetDirectoryName(adjacent));
            }
            return games.OrderBy(path => path, StringComparer.OrdinalIgnoreCase).ToList();
        }

        internal static string ReadOptional(string path)
        {
            try { return File.Exists(path) ? File.ReadAllText(path) : ""; }
            catch (IOException) { return ""; }
            catch (UnauthorizedAccessException) { return ""; }
        }

        internal static void AddGame(HashSet<string> games, string path)
        {
            if (!String.IsNullOrEmpty(path) && File.Exists(Path.Combine(path, "barony.exe")))
                games.Add(Normalize(path));
        }

        internal static void ValidateGame(string game)
        {
            string executable = Path.Combine(game, "barony.exe");
            if (!File.Exists(executable)) throw new IOException("Select the Barony folder containing barony.exe, not its parent or a mod folder.");
            if (FileHash(executable) != (string)Release["game_sha256"])
                throw new IOException("This is not the supported Steam Windows Barony 5.0.2 build. Verify game files in Steam or obtain a compatible mod release. No retail files were changed.");
            foreach (Dictionary<string, object> dependency in (System.Collections.IEnumerable)Release["game_dlls"])
            {
                string path = Path.Combine(game, (string)dependency["name"]);
                if (!File.Exists(path) || FileHash(path) != (string)dependency["sha256"])
                    throw new IOException("Missing or incompatible " + dependency["name"] + ". Verify Barony files in Steam. No retail files were changed.");
            }
        }

        internal static string SafePath(string root, string relative)
        {
            if (String.IsNullOrEmpty(relative) || relative.IndexOf(':') >= 0 || relative.IndexOf('\\') >= 0 || Path.IsPathRooted(relative))
                throw new IOException("Unsafe path in installer payload.");
            string[] parts = relative.Split('/');
            if (parts.Any(part => part.Length == 0 || part == "." || part == ".." || part.EndsWith(".") || part.EndsWith(" ")))
                throw new IOException("Unsafe path in installer payload.");
            string result = Path.GetFullPath(Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar)));
            if (!result.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                throw new IOException("Payload path escapes the installation folder.");
            return result;
        }

        internal static void RejectLinks(string root, string path)
        {
            string current = path;
            while (current.Length >= root.Length)
            {
                if ((File.Exists(current) || Directory.Exists(current)) && (File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0)
                    throw new IOException("Installation contains a link or junction. Choose a clean versioned folder.");
                if (String.Equals(current, root, StringComparison.OrdinalIgnoreCase)) break;
                current = Path.GetDirectoryName(current);
            }
        }

        internal static bool MatchesInstall(string destination)
        {
            if (!Directory.Exists(destination)) return false;
            Dictionary<string, object> files = (Dictionary<string, object>)Config["files"];
            Stack<string> pending = new Stack<string>();
            pending.Push(destination);
            while (pending.Count > 0)
            {
                foreach (string entry in Directory.GetFileSystemEntries(pending.Pop()))
                {
                    RejectLinks(destination, entry);
                    if (Directory.Exists(entry)) pending.Push(entry);
                    else if (!files.ContainsKey(entry.Substring(destination.Length + 1).Replace('\\', '/'))) return false;
                }
            }
            foreach (KeyValuePair<string, object> item in files)
            {
                string path = SafePath(destination, item.Key);
                RejectLinks(destination, path);
                if (!File.Exists(path) || FileHash(path) != (string)item.Value) return false;
            }
            return true;
        }

        internal static string Install(string game, bool shortcut)
        {
            game = Normalize(game);
            ValidateGame(game);
            CheckRootLauncher(game);
            string destination = Path.Combine(game, Folder);
            bool existed = Directory.Exists(destination);
            RejectLinks(destination, destination);
            if (File.Exists(destination)) throw new IOException("A file already occupies " + destination);
            if (Directory.Exists(destination))
            {
                if (!MatchesInstall(destination))
                    throw new IOException("The versioned mod folder already exists but is incomplete or modified. Nothing was overwritten. Close the mod and rename that folder before installing again.");
            }
            else
            {
                string temporary = Path.Combine(game, ".dd-" + Guid.NewGuid().ToString("N").Substring(0, 16));
                if (Directory.Exists(temporary) || File.Exists(temporary)) throw new IOException("Temporary installation path is occupied. Please try again.");
                bool created = false;
                try
                {
                    using (Stream payload = Assembly.GetExecutingAssembly().GetManifestResourceStream("payload.zip"))
                    {
                        if (Hash(payload) != (string)Config["archive_sha256"])
                            throw new IOException("Installer payload checksum failed. Download a fresh installer.");
                        Directory.CreateDirectory(temporary);
                        created = true;
                        payload.Position = 0;
                        using (ZipArchive archive = new ZipArchive(payload, ZipArchiveMode.Read))
                        {
                            Dictionary<string, object> files = (Dictionary<string, object>)Config["files"];
                            HashSet<string> seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                            foreach (ZipArchiveEntry entry in archive.Entries)
                            {
                                if (RootLauncher && entry.FullName == "Dungeon Delve.exe") continue;
                                string prefix = Folder + "/";
                                if (!entry.FullName.StartsWith(prefix, StringComparison.Ordinal)) throw new IOException("Unexpected payload folder.");
                                string relative = entry.FullName.Substring(prefix.Length);
                                string target = SafePath(temporary, relative);
                                if (!files.ContainsKey(relative) || !seen.Add(relative)) throw new IOException("Unexpected or duplicated payload file.");
                                Directory.CreateDirectory(Path.GetDirectoryName(target));
                                using (Stream input = entry.Open())
                                using (FileStream output = new FileStream(target, FileMode.CreateNew)) input.CopyTo(output);
                                if (FileHash(target) != (string)files[relative]) throw new IOException("Extracted file checksum failed: " + relative);
                            }
                            if (seen.Count != files.Count) throw new IOException("Incomplete installer payload.");
                        }
                    }
                    Directory.Move(temporary, destination);
                }
                catch (UnauthorizedAccessException)
                {
                    throw new IOException("Cannot write to this Barony folder. Use the portable ZIP in a writable folder, or adjust this folder's permissions. Do not disable antivirus.");
                }
                catch (PathTooLongException)
                {
                    throw new IOException("The installation path is too long for Windows .NET Framework. Use the portable ZIP in a shorter writable path.");
                }
                finally
                {
                    if (created && Directory.Exists(temporary)) Directory.Delete(temporary, true);
                }
            }
            try { InstallRootLauncher(game); }
            catch
            {
                if (!existed && MatchesInstall(destination)) Directory.Delete(destination, true);
                throw;
            }
            if (shortcut) CreateShortcut(destination, Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory));
            return destination;
        }

        internal static void CreateShortcut(string destination, string desktop)
        {
            string path = Path.Combine(desktop, "Dungeon Delve " + Version + ".lnk");
            if (File.Exists(path)) return;
            object shell = null;
            object link = null;
            try
            {
                Type shellType = Type.GetTypeFromProgID("WScript.Shell", true);
                shell = Activator.CreateInstance(shellType);
                link = shellType.InvokeMember("CreateShortcut", BindingFlags.InvokeMethod, null, shell, new object[] { path });
                Type linkType = link.GetType();
                string target = Path.Combine(LauncherDirectory(destination), LauncherName);
                linkType.InvokeMember("TargetPath", BindingFlags.SetProperty, null, link, new object[] { target });
                if (LauncherName.EndsWith(".exe", StringComparison.OrdinalIgnoreCase))
                    linkType.InvokeMember("IconLocation", BindingFlags.SetProperty, null, link, new object[] { target + ",0" });
                linkType.InvokeMember("WorkingDirectory", BindingFlags.SetProperty, null, link, new object[] { LauncherDirectory(destination) });
                linkType.InvokeMember("Description", BindingFlags.SetProperty, null, link, new object[] { "Dungeon Delve " + Version });
                linkType.InvokeMember("Save", BindingFlags.InvokeMethod, null, link, null);
            }
            catch (Exception error)
            {
                throw new IOException("The mod was installed successfully, but the desktop shortcut could not be created. Run " + LauncherName + " inside " + LauncherDirectory(destination) + ". " + error.Message);
            }
            finally
            {
                if (link != null) System.Runtime.InteropServices.Marshal.FinalReleaseComObject(link);
                if (shell != null) System.Runtime.InteropServices.Marshal.FinalReleaseComObject(shell);
            }
        }

        [STAThread]
        private static int Main(string[] arguments)
        {
            if (arguments.Length == 0)
            {
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(new InstallForm());
                return 0;
            }
            string report = null;
            try
            {
                string mode = null;
                string game = null;
                List<string> roots = new List<string>();
                for (int index = 0; index < arguments.Length; index++)
                {
                    string option = arguments[index];
                    if (option == "--detect" || option == "--check" || option == "--install")
                    {
                        if (mode != null) throw new ArgumentException("Choose one installer operation.");
                        mode = option;
                    }
                    else if (option == "--report") report = arguments[++index];
                    else if (option == "--game-dir") game = arguments[++index];
                    else if (option == "--steam-root") roots.Add(arguments[++index]);
                    else throw new ArgumentException("Unknown option: " + option);
                }
                object result;
                if (mode == "--detect") result = new { status = "PASS", games = Detect(roots.Count == 0 ? null : roots) };
                else if (mode == "--check" || mode == "--install")
                {
                    if (String.IsNullOrWhiteSpace(game)) throw new ArgumentException("--game-dir is required.");
                    game = Normalize(game);
                    ValidateGame(game);
                    result = new { status = "PASS", game = game, destination = mode == "--install" ? Install(game, false) : Path.Combine(game, Folder) };
                }
                else throw new ArgumentException("Use --detect, --check or --install with --report and optionally --game-dir / --steam-root. Command-line installs do not create shortcuts.");
                WriteReport(report, result);
                return 0;
            }
            catch (Exception error)
            {
                WriteReport(report, new { status = "FAIL", error = error.Message });
                return 1;
            }
        }

        internal static void WriteReport(string path, object result)
        {
            string text = Json.Serialize(result);
            if (path != null) File.WriteAllText(path, text + Environment.NewLine, new UTF8Encoding(false));
            else Console.WriteLine(text);
        }
    }

    internal sealed class InstallForm : Form
    {
        private readonly ComboBox folder = new ComboBox();
        private readonly Button browse = new Button();
        private readonly Button scan = new Button();
        private readonly Button install = new Button();
        private readonly CheckBox shortcut = new CheckBox();
        private readonly Label status = new Label();
        private readonly ProgressBar progress = new ProgressBar();
        private bool busy;

        internal InstallForm()
        {
            Text = "Dungeon Delve " + Installer.Version + " Setup";
            Icon = Icon.ExtractAssociatedIcon(Assembly.GetExecutingAssembly().Location);
            ClientSize = new Size(620, 385);
            Font = new Font("Segoe UI", 10);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            Label title = new Label { Text = "Install Dungeon Delve", Font = new Font("Segoe UI", 19, FontStyle.Bold), AutoSize = true, Location = new Point(24, 20) };
            Label description = new Label { Text = "Includes vending_machine framework, mod content and runtime.\nRequires your own Steam Windows copy of Barony.", AutoSize = true, Location = new Point(26, 67) };
            Label prompt = new Label { Text = "Barony installation folder (contains barony.exe)", AutoSize = true, Location = new Point(26, 125) };
            folder.SetBounds(26, 152, 465, 30);
            folder.DropDownStyle = ComboBoxStyle.DropDown;
            browse.Text = "Browse...";
            browse.SetBounds(503, 151, 91, 31);
            browse.Click += delegate
            {
                using (FolderBrowserDialog dialog = new FolderBrowserDialog { Description = "Select the Barony folder containing barony.exe", ShowNewFolderButton = false })
                {
                    if (Directory.Exists(folder.Text)) dialog.SelectedPath = folder.Text;
                    if (dialog.ShowDialog(this) == DialogResult.OK) folder.Text = dialog.SelectedPath;
                }
            };
            scan.Text = "Scan again";
            scan.SetBounds(26, 193, 112, 31);
            scan.Click += async delegate { await Scan(); };
            shortcut.Text = "Create a desktop shortcut";
            shortcut.Checked = true;
            shortcut.SetBounds(157, 196, 315, 28);
            Label safety = new Label { Text = Installer.RootLauncher ? "Adds Dungeon Delve.exe beside barony.exe.\nSupporting files stay in " + Installer.Folder + "." : "Installs into " + Installer.Folder + " inside the selected folder.\nRetail game files, saves and settings are never replaced.", AutoSize = true, Location = new Point(26, 239) };
            status.SetBounds(26, 292, 568, 38);
            progress.SetBounds(26, 344, 408, 14);
            progress.Visible = false;
            progress.Style = ProgressBarStyle.Marquee;
            install.Text = "Install";
            install.SetBounds(460, 335, 134, 35);
            install.Click += async delegate { await Install(); };
            Controls.AddRange(new Control[] { title, description, prompt, folder, browse, scan, shortcut, safety, status, progress, install });
            Shown += async delegate { await Scan(); };
            FormClosing += delegate(object sender, FormClosingEventArgs args) { if (busy) args.Cancel = true; };
        }

        private void Busy(bool value)
        {
            busy = value;
            folder.Enabled = browse.Enabled = scan.Enabled = shortcut.Enabled = install.Enabled = !value;
            progress.Visible = value;
        }

        private async Task Scan()
        {
            Busy(true);
            status.Text = "Looking in Steam libraries...";
            try
            {
                List<string> games = await Task.Run(() => Installer.Detect(null));
                string previous = folder.Text;
                folder.Items.Clear();
                foreach (string game in games) folder.Items.Add(game);
                if (String.IsNullOrWhiteSpace(previous) && games.Count > 0) folder.SelectedIndex = 0;
                else folder.Text = previous;
                status.Text = games.Count == 0 ? "No installation found. Browse to your Barony folder manually." : games.Count + " installation(s) found. Choose a folder, then click Install.";
            }
            catch (Exception error) { status.Text = "Automatic scan failed; use Browse. " + error.Message; }
            finally { Busy(false); }
        }

        private async Task Install()
        {
            string game = folder.Text;
            if (String.IsNullOrWhiteSpace(game)) { MessageBox.Show(this, "Choose your Barony installation first."); return; }
            bool makeShortcut = shortcut.Checked;
            Busy(true);
            status.Text = "Verifying Barony and installing the complete mod...";
            try
            {
                string destination = await Task.Run(() => Installer.Install(game, makeShortcut));
                status.Text = "Installed successfully. Use the desktop shortcut or " + Installer.LauncherName + ".";
                MessageBox.Show(this, "Dungeon Delve is installed.\n\n" + Path.Combine(Installer.LauncherDirectory(destination), Installer.LauncherName) + "\n\nDouble-click this launcher to play. Keep Steam running.\nNormal Steam Play still starts vanilla Barony.", "Installation complete", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception error)
            {
                status.Text = "Installation did not complete. See the message for details.";
                MessageBox.Show(this, error.Message, "Dungeon Delve Setup", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
            finally { Busy(false); }
        }
    }
}
