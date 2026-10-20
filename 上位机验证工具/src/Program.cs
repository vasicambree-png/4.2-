using System;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;

namespace Ch584ScreenVerifier
{
    internal static class Program
    {
        [STAThread]
        private static int Main(string[] args)
        {
            if (args.Length > 0 && args[0] == "--export-defaults")
            {
                string path = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "当前固件_屏幕验证预置.xml");
                if (File.Exists(path)) return 2;
                Protocol.SaveXml(path, Protocol.Defaults());
                return 0;
            }
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            bool smoke = args.Length > 0 && args[0] == "--smoke-test";
            try
            {
                using (VerifierForm form = new VerifierForm(smoke))
                {
                    if (smoke)
                    {
                        form.RunSmokeTest();
                        return 0;
                    }
                    Application.Run(form);
                }
                return 0;
            }
            catch (Exception ex)
            {
                if (smoke)
                {
                    File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,
                        "验证结果", "界面自测错误.txt"), ex.ToString(), Encoding.UTF8);
                }
                else MessageBox.Show(ex.Message, "验证助手启动失败", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return 1;
            }
        }
    }

    internal sealed class RxChunk
    {
        public bool Debug;
        public int Generation;
        public byte[] Bytes;
        public string Error;
    }

    // Used only by offline smoke tests to exercise a failed persistent journal.
    internal sealed class JournalFailureStream : MemoryStream
    {
        public bool FailWrites;
        public override void Write(byte[] buffer, int offset, int count)
        {
            if (FailWrites) throw new IOException("模拟日志磁盘写入失败");
            base.Write(buffer, offset, count);
        }
    }

    public sealed class VerifierForm : Form
    {
        private readonly string baseDir = AppDomain.CurrentDomain.BaseDirectory;
        private readonly bool smoke;
        private readonly Color ink = Color.FromArgb(24, 43, 68);
        private readonly Color blue = Color.FromArgb(30, 91, 170);
        private readonly Color muted = Color.FromArgb(84, 98, 117);
        private readonly ComboBox testPorts = new ComboBox(), debugPorts = new ComboBox();
        private readonly NumericUpDown testBaud = Number(115200, 1200, 2000000);
        private readonly NumericUpDown debugBaud = Number(115200, 1200, 2000000);
        private readonly NumericUpDown interval = Number(200, 50, 2000);
        private readonly Button testConnect = new Button(), debugConnect = new Button();
        private readonly Label footer = new Label(), selectionInfo = new Label(), runStatus = new Label();
        private readonly Label bootStatus = new Label(), rxStatus = new Label(), fileStatus = new Label();
        private readonly DataGridView pagesGrid = Grid(), valuesGrid = Grid(), channelsGrid = Grid();
        private readonly TextBox preview = new TextBox(), raw = new TextBox();
        private readonly RichTextBox trafficLog = LogBox(), debugLog = LogBox();
        private readonly TabControl tabs = new TabControl();
        private readonly ConcurrentQueue<RxChunk> rxQueue = new ConcurrentQueue<RxChunk>();
        private readonly FrameDecoder decoder = new FrameDecoder();
        private readonly Stopwatch clock = Stopwatch.StartNew();
        private readonly System.Windows.Forms.Timer timer = new System.Windows.Forms.Timer();
        private readonly StringBuilder debugLine = new StringBuilder();
        private readonly Decoder textDecoder = new UTF8Encoding(false, false).GetDecoder();
        private readonly List<Button> sendButtons = new List<Button>();
        private List<PageSpec> pages;
        private List<PageSpec> cyclePages;
        private PageSpec activePage;
        private SerialPort testPort, debugPort;
        private int testGeneration, debugGeneration, queuedCount, droppedCount, lastDropped;
        private int cycleIndex, cycleCount, lastBoot;
        private long nextSend, nextPage, resumeAt, readDeadline;
        private long txCount, rxFrameCount;
        private bool editing, disposed, readPending, journalFaulted;
        private string mode = "停止", journalPath;
        private StreamWriter journal;

        public VerifierForm(bool smokeTest)
        {
            smoke = smokeTest;
            Text = "CH584 屏幕验证助手  |  页面发送 · 启动日志 · 协议检查";
            Font = new Font("Microsoft YaHei UI", 9F);
            ForeColor = ink;
            BackColor = Color.FromArgb(242, 246, 251);
            StartPosition = FormStartPosition.CenterScreen;
            Size = new Size(1380, 930);
            MinimumSize = new Size(1120, 780);
            AutoScaleMode = AutoScaleMode.Dpi;
            pages = Protocol.Defaults();
            BuildUi();
            RefreshPageRows();
            RefreshPorts();
            if (!smoke) OpenJournal();
            timer.Interval = 20;
            timer.Tick += Tick;
            if (!smoke) timer.Start();
            FormClosing += delegate { Shutdown(); };
            UpdateStatus();
        }

        private static NumericUpDown Number(decimal value, decimal min, decimal max)
        { return new NumericUpDown { Minimum = min, Maximum = max, Value = value, Width = 96 }; }

        private static DataGridView Grid()
        {
            return new DataGridView {
                Dock = DockStyle.Fill, BackgroundColor = Color.White, BorderStyle = BorderStyle.None,
                AllowUserToAddRows = false, AllowUserToDeleteRows = false, AllowUserToResizeRows = false,
                RowHeadersVisible = false, SelectionMode = DataGridViewSelectionMode.FullRowSelect,
                MultiSelect = false, AutoSizeRowsMode = DataGridViewAutoSizeRowsMode.None,
                ColumnHeadersHeight = 34, RowTemplate = { Height = 29 },
                EnableHeadersVisualStyles = false, GridColor = Color.FromArgb(225, 232, 242)
            };
        }

        private static RichTextBox LogBox()
        {
            return new RichTextBox { Dock = DockStyle.Fill, ReadOnly = true, BackColor = Color.White,
                BorderStyle = BorderStyle.None, Font = new Font("Consolas", 9F), WordWrap = false };
        }

        private void BuildUi()
        {
            TableLayoutPanel layout = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 5,
                Padding = new Padding(16, 10, 16, 8), BackColor = BackColor };
            layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 60));
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 104));
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 92));
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 34));
            Controls.Add(layout);

            TableLayoutPanel heading = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2 };
            heading.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 55));
            heading.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 45));
            heading.Controls.Add(new Label { Text = "CH584 / 屏幕验证助手", AutoSize = true,
                Font = new Font(Font.FontFamily, 19F, FontStyle.Bold), Padding = new Padding(0, 4, 0, 0) }, 0, 0);
            heading.Controls.Add(new Label { Text = "先持续发送主页，再对照启动日志定位空白\n当前协议：115200 · AA EE · 大端参数 · 累加校验",
                Dock = DockStyle.Fill, ForeColor = muted, TextAlign = ContentAlignment.MiddleRight }, 1, 0);
            layout.Controls.Add(heading, 0, 0);

            TableLayoutPanel portsLayout = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2 };
            portsLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
            portsLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
            portsLayout.Controls.Add(PortPanel(false), 0, 0);
            portsLayout.Controls.Add(PortPanel(true), 1, 0);
            layout.Controls.Add(portsLayout, 0, 1);

            TableLayoutPanel actions = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 2 };
            actions.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));
            actions.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));
            FlowLayoutPanel buttons = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false };
            AddButton(buttons, "持续发送主页", delegate { StartHome(); }, true);
            AddButton(buttons, "初始化后显示主页", delegate { RecoverHome(); }, true);
            AddButton(buttons, "发送选中页一次", delegate { SendSelectedOnce(); }, true);
            AddButton(buttons, "持续发送选中页", delegate { StartSelected(); }, true);
            AddButton(buttons, "开始页面轮播", delegate { StartCycle(); }, true);
            AddButton(buttons, "停止发送", delegate { StopRun("手动停止发送"); }, false);
            AddButton(buttons, "读取通道 0x03", delegate { RequestRead(); }, true);
            actions.Controls.Add(buttons, 0, 0);
            FlowLayoutPanel timing = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false };
            timing.Controls.Add(Caption("重复发送间隔 ms"));
            timing.Controls.Add(interval);
            interval.ValueChanged += delegate { nextSend = clock.ElapsedMilliseconds + (long)interval.Value; };
            timing.Controls.Add(new Label { Text = "建议 200 ms；页面停留时间在下方编辑。停止发送超过 5 秒仅停止刷新，屏幕可能保留旧画面。",
                AutoSize = true, ForeColor = muted, Padding = new Padding(12, 6, 0, 0) });
            actions.Controls.Add(timing, 0, 1);
            layout.Controls.Add(actions, 0, 2);

            tabs.Dock = DockStyle.Fill;
            tabs.Padding = new Point(20, 7);
            tabs.TabPages.Add(BuildPagesTab());
            tabs.TabPages.Add(BuildLogsTab());
            tabs.TabPages.Add(BuildChannelsTab());
            layout.Controls.Add(tabs, 0, 3);
            footer.Dock = DockStyle.Fill;
            footer.TextAlign = ContentAlignment.MiddleLeft;
            footer.ForeColor = muted;
            layout.Controls.Add(footer, 0, 4);
        }

        private GroupBox PortPanel(bool debug)
        {
            GroupBox group = new GroupBox { Text = debug ? "B / 启动日志口（可选，只接收 UART1）" : "A / 页面测试口（UART3）",
                Dock = DockStyle.Fill, Padding = new Padding(10), BackColor = Color.White };
            TableLayoutPanel inner = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 2 };
            inner.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
            inner.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            FlowLayoutPanel row = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false };
            ComboBox ports = debug ? debugPorts : testPorts;
            NumericUpDown baud = debug ? debugBaud : testBaud;
            Button connect = debug ? debugConnect : testConnect;
            ports.Width = 85; ports.DropDownStyle = ComboBoxStyle.DropDownList;
            row.Controls.Add(ports); row.Controls.Add(Caption("波特率")); row.Controls.Add(baud);
            connect.Text = "连接"; connect.Width = 72; connect.Height = 28;
            connect.Click += delegate { Guard(delegate { TogglePort(debug); }); };
            row.Controls.Add(connect);
            AddButton(row, "刷新", delegate { RefreshPorts(); }, false, 60);
            inner.Controls.Add(row, 0, 0);
            inner.Controls.Add(new Label { Text = debug ? "适配器 RX ← PA9；GND 共地。两个口须使用不同 COM。" :
                "适配器 TX → PA4，RX ← PA5；GND 共地。替代主控时断开 STM32 TX。",
                Dock = DockStyle.Fill, ForeColor = muted, TextAlign = ContentAlignment.MiddleLeft }, 0, 1);
            group.Controls.Add(inner);
            return group;
        }

        private TabPage BuildPagesTab()
        {
            TabPage tab = new TabPage("页面与参数") { BackColor = Color.White, Padding = new Padding(10) };
            SplitContainer split = new SplitContainer { Dock = DockStyle.Fill, SplitterWidth = 7, FixedPanel = FixedPanel.None };
            // Set distance only after the control receives its final client width.
            Shown += delegate { if (split.Width > 800) split.SplitterDistance = (int)(split.Width * .59); };
            TableLayoutPanel left = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 4 };
            left.RowStyles.Add(new RowStyle(SizeType.Absolute, 36));
            left.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            left.RowStyles.Add(new RowStyle(SizeType.Absolute, 43));
            left.RowStyles.Add(new RowStyle(SizeType.Absolute, 72));
            left.Controls.Add(new Label { Text = "当前固件页面  /  单击选中后编辑右侧参数", AutoSize = true,
                Font = new Font(Font, FontStyle.Bold), Padding = new Padding(0, 6, 0, 0) }, 0, 0);
            pagesGrid.ColumnHeadersDefaultCellStyle.BackColor = Color.FromArgb(231, 238, 248);
            pagesGrid.Columns.Add(new DataGridViewCheckBoxColumn { Name = "enabled", HeaderText = "轮播", Width = 47 });
            pagesGrid.Columns.Add("name", "页面名称"); pagesGrid.Columns[1].AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill;
            string[] names = { "Type", "rank", "menu", "focus", "停留ms" };
            foreach (string name in names) { int id = pagesGrid.Columns.Add(name, name); pagesGrid.Columns[id].Width = name == "停留ms" ? 70 : 46; }
            pagesGrid.SelectionChanged += delegate { if (!editing) ShowSelection(); };
            pagesGrid.CellEndEdit += delegate { if (!editing) PreviewEdits(); };
            pagesGrid.DataError += delegate(object s, DataGridViewDataErrorEventArgs e) { e.ThrowException = false; };
            left.Controls.Add(pagesGrid, 0, 1);
            FlowLayoutPanel files = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false };
            AddButton(files, "导入 XML", LoadProfile, false);
            AddButton(files, "导出 XML", SaveProfile, false);
            AddButton(files, "恢复当前预置", RestoreDefaults, false);
            left.Controls.Add(files, 0, 2);
            runStatus.Dock = DockStyle.Fill; runStatus.ForeColor = blue;
            runStatus.Padding = new Padding(5, 5, 5, 0);
            runStatus.BackColor = Color.FromArgb(240, 246, 255);
            left.Controls.Add(runStatus, 0, 3);
            split.Panel1.Controls.Add(left);

            TableLayoutPanel right = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 4, Padding = new Padding(9, 0, 0, 0) };
            right.RowStyles.Add(new RowStyle(SizeType.Absolute, 66));
            right.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            right.RowStyles.Add(new RowStyle(SizeType.Absolute, 28));
            right.RowStyles.Add(new RowStyle(SizeType.Absolute, 124));
            selectionInfo.Dock = DockStyle.Fill;
            selectionInfo.ForeColor = muted;
            right.Controls.Add(selectionInfo, 0, 0);
            valuesGrid.Columns.Add("label", "参数含义"); valuesGrid.Columns[0].ReadOnly = true;
            valuesGrid.Columns[0].AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill;
            valuesGrid.Columns.Add("value", "十进制值"); valuesGrid.Columns[1].Width = 105;
            valuesGrid.CellEndEdit += delegate { if (!editing) PreviewEdits(); };
            right.Controls.Add(valuesGrid, 0, 1);
            right.Controls.Add(new Label { Text = "发送预览（仅预览，不代表屏端已接收）", AutoSize = true, Padding = new Padding(0, 5, 0, 0) }, 0, 2);
            preview.Dock = DockStyle.Fill; preview.Multiline = true; preview.ReadOnly = true;
            preview.Font = new Font("Consolas", 9.5F); preview.BackColor = Color.FromArgb(246, 248, 251);
            preview.ScrollBars = ScrollBars.Vertical;
            right.Controls.Add(preview, 0, 3);
            split.Panel2.Controls.Add(right);
            tab.Controls.Add(split);
            return tab;
        }

        private TabPage BuildLogsTab()
        {
            TabPage tab = new TabPage("收发与启动日志") { BackColor = Color.White, Padding = new Padding(10) };
            TableLayoutPanel layout = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 4 };
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 38));
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 34));
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 105));
            FlowLayoutPanel bar = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false };
            AddButton(bar, "打开日志目录", delegate { Directory.CreateDirectory(Path.Combine(baseDir, "logs"));
                Process.Start("explorer.exe", Path.Combine(baseDir, "logs")); }, false);
            AddButton(bar, "清空显示", delegate { trafficLog.Clear(); debugLog.Clear(); }, false);
            fileStatus.AutoSize = true; fileStatus.ForeColor = muted; fileStatus.Padding = new Padding(6, 6, 0, 0);
            bar.Controls.Add(fileStatus); layout.Controls.Add(bar, 0, 0);
            SplitContainer split = new SplitContainer { Dock = DockStyle.Fill, Orientation = Orientation.Horizontal, SplitterWidth = 6 };
            Shown += delegate { if (split.Height > 150) split.SplitterDistance = (int)(split.Height * .49); };
            split.Panel1.Controls.Add(LogPanel("A / 原始收发与完整协议帧", trafficLog));
            split.Panel2.Controls.Add(LogPanel("B / UART1 启动与调试文本", debugLog));
            layout.Controls.Add(split, 0, 1);
            bootStatus.Dock = DockStyle.Fill; bootStatus.ForeColor = blue;
            layout.Controls.Add(bootStatus, 0, 2);
            TableLayoutPanel rawLayout = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, Padding = new Padding(0, 3, 0, 0) };
            rawLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            rawLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 172));
            raw.Dock = DockStyle.Fill; raw.Multiline = true; raw.Font = new Font("Consolas", 9F);
            raw.ScrollBars = ScrollBars.Vertical;
            raw.Text = Protocol.Hex(Protocol.Frame(Protocol.Home()));
            rawLayout.Controls.Add(raw, 0, 0);
            FlowLayoutPanel rawButtons = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false };
            AddButton(rawButtons, "发送 HEX 一次", SendRaw, true, 155);
            rawButtons.Controls.Add(new Label { Text = "发送前校验长度与 SUM\n仅支持页面 01 / 读取 03\n禁止解绑、存储、恢复出厂", AutoSize = true, ForeColor = muted });
            rawLayout.Controls.Add(rawButtons, 1, 0); layout.Controls.Add(rawLayout, 0, 3);
            tab.Controls.Add(layout); return tab;
        }

        private Control LogPanel(string title, RichTextBox box)
        {
            TableLayoutPanel panel = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 2 };
            panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 26)); panel.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            panel.Controls.Add(new Label { Text = title, Dock = DockStyle.Fill, ForeColor = muted }, 0, 0);
            panel.Controls.Add(box, 0, 1); return panel;
        }

        private TabPage BuildChannelsTab()
        {
            TabPage tab = new TabPage("通道数据 03 / 06") { BackColor = Color.White, Padding = new Padding(10) };
            TableLayoutPanel layout = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 2 };
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 57)); layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            rxStatus.Dock = DockStyle.Fill; rxStatus.ForeColor = muted;
            layout.Controls.Add(rxStatus, 0, 0);
            channelsGrid.ReadOnly = true; channelsGrid.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
            foreach (string title in new[] { "通道", "类型", "原始16位值", "按当前UI取整", "电压原始字节" }) channelsGrid.Columns.Add(title, title);
            for (int i = 0; i < 20; i++) channelsGrid.Rows.Add(i + 1, "尚未接收", "—", "—", "—");
            rxStatus.Text = "尚未接收通道数据。连接 A 口后单次读取0x03；固件可能等待30秒，或主动上报0x06。";
            layout.Controls.Add(channelsGrid, 0, 1); tab.Controls.Add(layout); return tab;
        }

        private static Label Caption(string text)
        { return new Label { Text = text, AutoSize = true, Padding = new Padding(3, 6, 3, 0) }; }

        private Button AddButton(Control parent, string text, Action action, bool send, int width = 128)
        {
            Button button = new Button { Text = text, Width = width, Height = 31, FlatStyle = FlatStyle.Flat,
                BackColor = send ? Color.FromArgb(234, 242, 254) : Color.White, ForeColor = send ? blue : ink };
            button.FlatAppearance.BorderColor = Color.FromArgb(196, 211, 231);
            button.Click += delegate { Guard(action); };
            parent.Controls.Add(button); if (send) sendButtons.Add(button); return button;
        }

        private void Guard(Action action)
        {
            try { action(); }
            catch (Exception ex)
            {
                StopRun("操作失败：" + ex.Message);
                Append(trafficLog, "[ERROR] " + ex.Message, Color.Firebrick);
                Record("ERROR", ex.Message, "");
                MessageBox.Show(this, ex.Message, "操作未完成", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
            UpdateStatus();
        }

        private void RefreshPorts()
        {
            string[] found = SerialPort.GetPortNames().OrderBy(p => p.Length).ThenBy(p => p).ToArray();
            FillPorts(testPorts, found, testPort != null);
            FillPorts(debugPorts, found, debugPort != null);
            if (debugPorts.Items.Count > 1 && debugPorts.SelectedItem == testPorts.SelectedItem) debugPorts.SelectedIndex = 1;
        }

        private static void FillPorts(ComboBox combo, string[] names, bool open)
        {
            if (open) return;
            string previous = combo.Text; combo.Items.Clear(); combo.Items.AddRange(names);
            if (names.Contains(previous)) combo.SelectedItem = previous;
            else if (names.Length > 0) combo.SelectedIndex = 0;
        }

        private void TogglePort(bool debug)
        {
            SerialPort existing = debug ? debugPort : testPort;
            if (existing != null) { ClosePort(debug); return; }
            ComboBox combo = debug ? debugPorts : testPorts;
            string name = combo.Text;
            if (String.IsNullOrWhiteSpace(name)) throw new InvalidOperationException("未发现串口。请连接 USB 转串口适配器后刷新。");
            SerialPort other = debug ? testPort : debugPort;
            if (other != null && String.Equals(name, other.PortName, StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("测试口和日志口不能使用同一个 COM；单适配器时只连接测试口。");
            if (smoke) throw new InvalidOperationException("界面自测禁止打开实际串口。");
            int generation = debug ? ++debugGeneration : ++testGeneration;
            SerialPort port = new SerialPort(name, (int)(debug ? debugBaud.Value : testBaud.Value), Parity.None, 8, StopBits.One) {
                Handshake = Handshake.None, DtrEnable = false, RtsEnable = false, ReadTimeout = 150, WriteTimeout = 200,
                ReadBufferSize = 16384, ReceivedBytesThreshold = 1 };
            port.DataReceived += delegate { Receive(port, debug, generation); };
            try { port.Open(); }
            catch { port.Dispose(); throw; }
            if (debug) { debugPort = port; lastBoot = 0; debugLine.Clear(); textDecoder.Reset(); }
            else { testPort = port; decoder.Reset(); readPending = false; ResetChannels(name + "：尚未接收本连接数据"); }
            Record("CONNECT", (debug ? "UART1日志 " : "UART3测试 ") + name + " " + port.BaudRate + " 8N1", "");
            Append(debug ? debugLog : trafficLog, "已连接 " + name + " / " + port.BaudRate + " 8N1", blue);
            UpdateStatus();
        }

        private void ClosePort(bool debug)
        {
            SerialPort port = debug ? debugPort : testPort;
            if (port == null) return;
            string portName = port.PortName;
            if (debug) { debugPort = null; debugGeneration++; debugLine.Clear(); textDecoder.Reset(); lastBoot = 0; }
            else { testPort = null; testGeneration++; StopRun("测试串口已断开"); readPending = false; decoder.Reset(); ResetChannels("A口已断开，上一连接数据已清除"); }
            try { port.Close(); } finally { port.Dispose(); }
            Record("DISCONNECT", (debug ? "UART1 " : "UART3 ") + portName, "");
            UpdateStatus();
        }

        private void Receive(SerialPort port, bool debug, int generation)
        {
            try
            {
                int budget = 65536;
                while (budget > 0 && port.IsOpen)
                {
                    int count = Math.Min(Math.Min(port.BytesToRead, 4096), budget);
                    if (count <= 0) break;
                    byte[] bytes = new byte[count]; int read = port.Read(bytes, 0, count);
                    if (read <= 0) break;
                    if (read != bytes.Length) Array.Resize(ref bytes, read);
                    Enqueue(new RxChunk { Debug = debug, Generation = generation, Bytes = bytes });
                    budget -= read;
                }
            }
            catch (Exception ex)
            { Enqueue(new RxChunk { Debug = debug, Generation = generation, Error = ex.Message }); }
        }

        private void Enqueue(RxChunk chunk)
        {
            if (disposed) return;
            if (Interlocked.Increment(ref queuedCount) > 256)
            { Interlocked.Decrement(ref queuedCount); Interlocked.Increment(ref droppedCount); return; }
            rxQueue.Enqueue(chunk);
        }

        private void PumpReceive()
        {
            RxChunk item;
            for (int n = 0; n < 80 && rxQueue.TryDequeue(out item); n++)
            {
                Interlocked.Decrement(ref queuedCount);
                if (item.Generation != (item.Debug ? debugGeneration : testGeneration)) continue;
                if (item.Error != null)
                {
                    Append(trafficLog, "[RX_ERROR] " + item.Error, Color.Firebrick);
                    Record("RX_ERROR", (item.Debug ? "UART1 " : "UART3 ") + item.Error, "");
                    ClosePort(item.Debug); continue;
                }
                string hex = Protocol.Hex(item.Bytes);
                Record(item.Debug ? "UART1_RX_RAW" : "UART3_RX_RAW", item.Bytes.Length + " bytes", hex);
                if (item.Debug) ProcessDebug(item.Bytes);
                else
                {
                    Append(trafficLog, "RX RAW  " + hex, Color.FromArgb(66, 116, 86));
                    int rejected = decoder.Rejected;
                    foreach (byte[] frame in decoder.Feed(item.Bytes))
                    {
                        rxFrameCount++;
                        string detail = Protocol.Describe(frame);
                        Append(trafficLog, "RX FRAME  " + detail + "  |  " + Protocol.Hex(frame), Color.DarkGreen);
                        Record("RX_FRAME", detail, Protocol.Hex(frame));
                        if ((frame[4] == 3 || frame[4] == 6) && frame.Length == 87)
                        {
                            ShowChannels(frame);
                            if (frame[4] == 3) readPending = false;
                        }
                    }
                    if (decoder.Rejected != rejected)
                        Append(trafficLog, "丢弃无效帧候选；累计 " + decoder.Rejected + " 个（可在原始日志核对）", Color.DarkOrange);
                }
            }
            int drops = Volatile.Read(ref droppedCount);
            if (drops != lastDropped)
            {
                lastDropped = drops;
                Record("RX_QUEUE_DROP", "接收队列过载，累计丢弃 " + drops + " 块；此段日志不完整", "");
                Append(trafficLog, "接收队列过载，日志有缺失；累计丢弃 " + drops + " 块", Color.Firebrick);
            }
        }

        private void ProcessDebug(byte[] bytes)
        {
            char[] chars = new char[Encoding.UTF8.GetMaxCharCount(bytes.Length)];
            int count = textDecoder.GetChars(bytes, 0, bytes.Length, chars, 0, false);
            string text = new string(chars, 0, count);
            AppendText(debugLog, text);
            foreach (char c in text)
            {
                if (c == '\r') continue;
                if (c == '\n')
                {
                    string line = debugLine.ToString(); debugLine.Clear();
                    Record("UART1_LINE", line, "");
                    Match match = Regex.Match(line, @"\[BOOT\]\s*(\d+)");
                    if (match.Success) Int32.TryParse(match.Groups[1].Value, out lastBoot);
                    if (line.IndexOf("error", StringComparison.OrdinalIgnoreCase) >= 0)
                        Append(trafficLog, "启动口报告：" + line, Color.Firebrick);
                }
                else if (debugLine.Length < 4096) debugLine.Append(c);
                else { Record("UART1_PARTIAL", debugLine.ToString(), ""); debugLine.Clear(); debugLine.Append(c); }
            }
        }

        private void ShowChannels(byte[] frame)
        {
            string[] typeNames = { "无", "锚杆", "激光", "位移2", "位移4", "位移6", "位移8", "裂缝", "倾角", "应力", "液位", "微震", "地音", "测试" };
            for (int i = 0; i < 20; i++)
            {
                int at = 5 + i * 4, type = frame[at], rawValue = (frame[at + 1] << 8) | frame[at + 2];
                int display = type == 8 ? (short)rawValue : rawValue;
                if (type == 1 || (type >= 3 && type <= 6) || type == 8 || type == 9) display /= 10;
                channelsGrid.Rows[i].SetValues(i + 1, type < typeNames.Length ? typeNames[type] : "未知 " + type,
                    rawValue, type == 0 ? "—" : display.ToString(), frame[at + 3]);
            }
            rxStatus.Text = "最近有效 " + (frame[4] == 3 ? "0x03 应答" : "0x06 主动上报") + "：" + DateTime.Now.ToString("HH:mm:ss.fff") +
                "  / 20 通道\n原始字节按当前源码解码；类型 0 无效。上位机不向 BLE 通道表注入模拟数据。";
        }

        private void ResetChannels(string reason)
        {
            for (int i = 0; i < channelsGrid.Rows.Count; i++)
                channelsGrid.Rows[i].SetValues(i + 1, "尚未接收", "—", "—", "—");
            rxStatus.Text = reason + "\n只显示当前连接的完整有效应答；历史数据仍保存在日志文件。";
        }

        private void EnsureConnected()
        {
            if (journalFaulted) throw new IOException("日志已失效，发送已锁定。请解决磁盘或目录权限问题后重新启动程序。");
            if (testPort == null || !testPort.IsOpen) throw new InvalidOperationException("请先连接 A / 页面测试口。");
        }

        private void SendFrame(byte[] frame, string detail)
        {
            EnsureConnected();
            SubmitFrame(frame, detail, delegate(byte[] bytes) { testPort.Write(bytes, 0, bytes.Length); });
        }

        private void SubmitFrame(byte[] frame, string detail, Action<byte[]> write)
        {
            if (journalFaulted) throw new IOException("日志已失效，不能继续发送。");
            string error = Protocol.ValidateTransmit(frame);
            if (error != null) throw new ArgumentException(error);
            string hex = Protocol.Hex(frame);
            if (!Record("TX_ATTEMPT", detail, hex))
                throw new IOException("发送前日志写入失败：本帧没有提交串口，后续发送已锁定。");
            try { write(frame); }
            catch (Exception ex) { Record("TX_ERROR", detail + " / " + ex.Message, hex); throw; }
            txCount++;
            Append(trafficLog, "TX  " + detail + "  |  " + hex, blue);
            if (!Record("TX_OK", detail + " / 已提交串口写入，屏端解析尚未确认", hex))
                throw new IOException("本帧已提交串口，但结果日志保存失败；自动发送已停止，后续发送已锁定。");
        }

        private void SendPage(PageSpec page)
        { SendFrame(Protocol.Frame(page), page.Name + " rank=" + page.Rank + " menu=" + page.Menu + " focus=" + page.Focus); }

        private void ClearMessage()
        {
            SendPage(new PageSpec { Name = "清除界面提示", Sensor = 0, Rank = 6, Menu = 0, Focus = 0, Delay = 1500,
                Labels = new[] { "提示码：0清除" }, Values = new ushort[] { 0 } });
        }

        private void StartHome()
        {
            EnsureConnected(); StopRun("开始主页诊断"); ClearMessage();
            StartKeep(Protocol.Home(), "持续主页");
        }

        private void RecoverHome()
        {
            EnsureConnected(); StopRun("初始化后恢复主页"); SendPage(Protocol.Reinitialize());
            mode = "等待初始化"; resumeAt = clock.ElapsedMilliseconds + 1300;
            Record("RUN_START", "仅发送一次初始化请求，1300ms后清提示并持续发送主页", "");
            UpdateStatus();
        }

        private void StartSelected()
        {
            CommitEdits(); PageSpec selected = Selected();
            if (selected.Rank == 5) throw new ArgumentException("初始化请求不适合持续发送，请使用“初始化后显示主页”。");
            EnsureConnected(); StopRun("开始持续发送选中页"); StartKeep(Protocol.Clone(selected), "持续选中页");
        }

        private void StartKeep(PageSpec page, string title)
        {
            string error = Protocol.Validate(page); if (error != null) throw new ArgumentException(error);
            activePage = Protocol.Clone(page); mode = title; SendPage(activePage);
            nextSend = clock.ElapsedMilliseconds + (long)interval.Value;
            Record("RUN_START", title + " / " + activePage.Name, ""); UpdateStatus();
        }

        private void SendSelectedOnce()
        { CommitEdits(); PageSpec selected = Selected(); EnsureConnected(); StopRun("发送选中页一次"); SendPage(selected); }

        private void StartCycle()
        {
            CommitEdits();
            List<PageSpec> enabled = pages.Where(p => p.Enabled).Select(p => Protocol.Clone(p)).ToList();
            if (enabled.Count == 0) throw new ArgumentException("至少勾选一个轮播页面。");
            foreach (PageSpec page in enabled)
            {
                string error = Protocol.Validate(page);
                if (error != null) throw new ArgumentException(page.Name + "：" + error);
                if (page.Rank == 5) throw new ArgumentException("轮播中不能反复初始化屏幕；请取消勾选初始化页。");
                if (page.Rank == 6 && page.Values[0] >= 8)
                    throw new ArgumentException("常驻关机提示不适合轮播；请单次发送，并用持续主页清除。");
            }
            EnsureConnected(); StopRun("开始页面轮播"); ClearMessage();
            cyclePages = enabled; cycleIndex = 0; cycleCount = 1; activePage = cyclePages[0]; mode = "页面轮播";
            SendPage(activePage); nextPage = clock.ElapsedMilliseconds + activePage.Delay;
            nextSend = clock.ElapsedMilliseconds + (long)interval.Value;
            Record("RUN_START", "轮播 " + enabled.Count + " 页；运行使用启动时参数快照", ""); UpdateStatus();
        }

        private void StopRun(string detail)
        {
            if (mode != "停止") Record("RUN_STOP", detail, "");
            mode = "停止"; activePage = null; cyclePages = null; UpdateStatus();
        }

        private void SendRaw()
        {
            byte[] frame = Protocol.ParseHex(raw.Text);
            string error = Protocol.ValidateTransmit(frame); if (error != null) throw new ArgumentException(error);
            EnsureConnected(); StopRun("发送自定义 HEX");
            if (frame[4] == 3) { RequestRead(); return; }
            SendFrame(frame, "自定义页面 HEX");
        }

        private void RequestRead()
        {
            EnsureConnected();
            if (readPending) throw new InvalidOperationException("上一条0x03仍在等待应答，固件可能等待一轮数据或30秒超时。请勿重复请求。");
            SendFrame(Protocol.ReadRequest(), "读取20通道 0x03");
            readPending = true; readDeadline = clock.ElapsedMilliseconds + 35000;
            rxStatus.Text = "等待 0x03 应答：固件可能等待传感器数据收齐，最长约30秒后回复。";
        }

        private void Tick(object sender, EventArgs e)
        {
            try
            {
                PumpReceive(); long now = clock.ElapsedMilliseconds;
                if (readPending && now >= readDeadline)
                {
                    readPending = false;
                    rxStatus.Text = "35秒内未收到有效0x03应答；检查启动、接线和收帧日志。";
                    Record("READ_TIMEOUT", "35秒内没有完整0x03应答", "");
                }
                if (mode == "等待初始化" && now >= resumeAt) { ClearMessage(); StartKeep(Protocol.Home(), "持续主页"); }
                else if (mode == "页面轮播" && now >= nextPage)
                {
                    cycleIndex++; if (cycleIndex >= cyclePages.Count) { cycleIndex = 0; cycleCount++; }
                    activePage = cyclePages[cycleIndex]; SendPage(activePage);
                    nextPage = clock.ElapsedMilliseconds + activePage.Delay;
                    nextSend = clock.ElapsedMilliseconds + (long)interval.Value;
                }
                else if (activePage != null && now >= nextSend)
                { SendPage(activePage); nextSend = clock.ElapsedMilliseconds + (long)interval.Value; }
                UpdateStatus();
            }
            catch (Exception ex)
            {
                StopRun("运行停止：" + ex.Message);
                Append(trafficLog, "[ERROR] 自动发送停止：" + ex.Message, Color.Firebrick);
                Record("RUN_ERROR", ex.Message, "");
            }
        }

        private PageSpec Selected()
        {
            if (pagesGrid.CurrentRow == null) throw new InvalidOperationException("请先选择一个页面。");
            return (PageSpec)pagesGrid.CurrentRow.Tag;
        }

        private void RefreshPageRows()
        {
            editing = true;
            try
            {
                pagesGrid.Rows.Clear();
                foreach (PageSpec p in pages)
                {
                    int index = pagesGrid.Rows.Add(p.Enabled, p.Name, p.Sensor, p.Rank, p.Menu, p.Focus, p.Delay);
                    pagesGrid.Rows[index].Tag = p;
                    if (Protocol.Validate(p) != null) pagesGrid.Rows[index].DefaultCellStyle.ForeColor = Color.Firebrick;
                }
                if (pagesGrid.Rows.Count > 0) pagesGrid.CurrentCell = pagesGrid.Rows[0].Cells[1];
            }
            finally { editing = false; }
            ShowSelection();
        }

        private void ShowSelection()
        {
            if (pagesGrid.CurrentRow == null) return;
            editing = true;
            try
            {
                PageSpec p = Selected(); valuesGrid.Rows.Clear();
                ushort[] vals = p.Values ?? new ushort[0];
                for (int i = 0; i < vals.Length; i++) valuesGrid.Rows.Add(p.Labels != null && i < p.Labels.Length ? p.Labels[i] : "参数 " + (i + 1), vals[i]);
                UpdatePreview(p);
            }
            finally { editing = false; }
        }

        private void CommitEdits()
        {
            pagesGrid.EndEdit(); valuesGrid.EndEdit();
            foreach (DataGridViewRow row in pagesGrid.Rows)
            {
                PageSpec p = (PageSpec)row.Tag;
                p.Enabled = row.Cells[0].Value != null && Convert.ToBoolean(row.Cells[0].Value);
                p.Name = Convert.ToString(row.Cells[1].Value);
                p.Sensor = ReadByte(row.Cells[2].Value, "Type");
                p.Rank = ReadByte(row.Cells[3].Value, "rank");
                p.Menu = ReadByte(row.Cells[4].Value, "menu");
                p.Focus = ReadByte(row.Cells[5].Value, "focus");
                int delay;
                if (!Int32.TryParse(Convert.ToString(row.Cells[6].Value), out delay) || delay < 50 || delay > 60000)
                    throw new ArgumentException(p.Name + "：页面停留时间必须在50～60000 ms内。");
                p.Delay = delay;
            }
            if (pagesGrid.CurrentRow != null)
            {
                ushort[] vals = new ushort[valuesGrid.Rows.Count];
                for (int i = 0; i < vals.Length; i++)
                    if (!UInt16.TryParse(Convert.ToString(valuesGrid.Rows[i].Cells[1].Value), out vals[i]))
                        throw new ArgumentException("第" + (i + 1) + "个参数须为0～65535的十进制整数。");
                Selected().Values = vals;
            }
        }

        private static byte ReadByte(object value, string name)
        {
            byte parsed;
            if (!Byte.TryParse(Convert.ToString(value), out parsed)) throw new ArgumentException(name + "须为0～255的十进制整数。");
            return parsed;
        }

        private void PreviewEdits()
        {
            try { CommitEdits(); UpdatePreview(Selected()); }
            catch (Exception ex) { preview.ForeColor = Color.Firebrick; preview.Text = "参数未通过检查：" + ex.Message; }
        }

        private void UpdatePreview(PageSpec p)
        {
            string error = Protocol.Validate(p);
            selectionInfo.Text = p.Name + "\nType=" + p.Sensor + "  rank=" + p.Rank + "  menu=" + p.Menu + "  focus=" + p.Focus +
                "；参数 " + (p.Values == null ? 0 : p.Values.Length) + " 项";
            preview.ForeColor = error == null ? ink : Color.Firebrick;
            preview.Text = error == null ? Protocol.Hex(Protocol.Frame(p)) + "\r\n\r\n" + Protocol.Describe(Protocol.Frame(p)) : "不能发送：" + error;
            if (pagesGrid.CurrentRow != null) pagesGrid.CurrentRow.DefaultCellStyle.ForeColor = error == null ? ink : Color.Firebrick;
        }

        private void LoadProfile()
        {
            using (OpenFileDialog dialog = new OpenFileDialog { Filter = "页面配置 XML|*.xml", InitialDirectory = baseDir })
            {
                if (dialog.ShowDialog(this) != DialogResult.OK) return;
                List<PageSpec> loaded = Protocol.LoadXml(dialog.FileName);
                StopRun("导入新页面配置"); pages = loaded; RefreshPageRows();
                int invalid = pages.Count(p => Protocol.Validate(p) != null);
                Record("XML_LOAD", dialog.FileName + " / " + pages.Count + " 页 / " + invalid + " 页不兼容", "");
                if (invalid > 0) MessageBox.Show(this, "已导入配置，但有" + invalid + "个页面与当前固件不兼容，已标红。\n原初锚力4参数配置不能用于当前信息汇总页。请恢复当前预置进行排查。",
                    "配置兼容性", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
        }

        private void SaveProfile()
        {
            CommitEdits();
            using (SaveFileDialog dialog = new SaveFileDialog { Filter = "页面配置 XML|*.xml", InitialDirectory = baseDir,
                FileName = "当前固件_自定义页面.xml", OverwritePrompt = true })
            { if (dialog.ShowDialog(this) == DialogResult.OK) { Protocol.SaveXml(dialog.FileName, pages); Record("XML_SAVE", dialog.FileName, ""); } }
        }

        private void RestoreDefaults()
        {
            if (MessageBox.Show(this, "恢复当前固件预置会替换窗口内尚未导出的参数。", "恢复预置", MessageBoxButtons.OKCancel,
                MessageBoxIcon.Question) != DialogResult.OK) return;
            StopRun("恢复预置"); pages = Protocol.Defaults(); RefreshPageRows();
        }

        private void UpdateStatus()
        {
            bool open = testPort != null && testPort.IsOpen;
            testConnect.Text = open ? "断开" : "连接";
            debugConnect.Text = debugPort != null && debugPort.IsOpen ? "断开" : "连接";
            testPorts.Enabled = !open; testBaud.Enabled = !open;
            debugPorts.Enabled = debugPort == null; debugBaud.Enabled = debugPort == null;
            foreach (Button button in sendButtons) button.Enabled = open && !journalFaulted;
            runStatus.Text = "运行：" + mode + (activePage == null ? "" : "  / " + activePage.Name) +
                (mode == "页面轮播" ? "  第" + cycleCount + "轮，第" + (cycleIndex + 1) + "/" + cyclePages.Count + "页" : "") +
                "\n轮播使用启动时参数快照；编辑后重新开始生效。通道数据由BLE维护。";
            footer.Text = "串口写入 " + txCount + " 帧   ·   有效接收 " + rxFrameCount + " 帧   ·   " +
                (open ? "A: " + testPort.PortName : "A: 未连接") + "   ·   " +
                (debugPort == null ? "B: 未连接" : "B: " + debugPort.PortName) + "   ·   TX_OK仅代表串口写入，画面仍需实际观察";
            if (lastBoot >= 10) bootStatus.Text = "启动日志已到 [BOOT] 10：初始化流程已返回。仍需核对页面帧是否进入屏端解析。";
            else if (lastBoot == 3) bootStatus.Text = "最近日志为 [BOOT] 3：若长期没有 [BOOT] 4，重点检查屏幕初始化 / SPI DMA阻塞。";
            else bootStatus.Text = lastBoot > 0 ? "最近启动阶段：[BOOT] " + lastBoot + "（仅根据已收到的日志）" :
                "尚未收到 [BOOT] 日志；先连接 B 口，再重新启动板卡。日志口无需发送数据。";
        }

        private void OpenJournal()
        {
            string dir = Path.Combine(baseDir, "logs"); Directory.CreateDirectory(dir);
            journalPath = Path.Combine(dir, DateTime.Now.ToString("yyyyMMdd_HHmmss_fff") + "_" + Guid.NewGuid().ToString("N").Substring(0, 6) + ".tsv");
            journal = new StreamWriter(new FileStream(journalPath, FileMode.CreateNew, FileAccess.Write, FileShare.Read), new UTF8Encoding(true));
            journal.AutoFlush = true; journal.WriteLine("time\tevent\tdetails\thex");
            fileStatus.Text = "自动记录：" + Path.GetFileName(journalPath);
            Record("SESSION_START", "日志追加保存，清空显示不会删除文件；没有自动连接或发送", "");
        }

        private bool Record(string kind, string detail, string hex)
        {
            if (journal == null) return !journalFaulted;
            try { journal.WriteLine(DateTimeOffset.Now.ToString("o") + "\t" + Clean(kind) + "\t" + Clean(detail) + "\t" + Clean(hex)); return true; }
            catch (IOException ex) { DisableJournal(ex.Message); return false; }
            catch (UnauthorizedAccessException ex) { DisableJournal(ex.Message); return false; }
        }

        private void DisableJournal(string error)
        {
            journalFaulted = true;
            StreamWriter failed = journal; journal = null;
            try { failed.Dispose(); } catch { }
            mode = "停止"; activePage = null; cyclePages = null;
            fileStatus.Text = "日志写入失败；自动发送已停止"; fileStatus.ForeColor = Color.Firebrick;
            Append(trafficLog, "日志写入失败：" + error, Color.Firebrick);
        }

        private static string Clean(string text)
        { return (text ?? "").Replace("\t", " ").Replace("\r", " ").Replace("\n", " "); }

        private static void Append(RichTextBox box, string line, Color color)
        {
            if (box.TextLength > 120000) box.Text = box.Text.Substring(box.TextLength - 70000);
            box.SelectionStart = box.TextLength; box.SelectionLength = 0; box.SelectionColor = color;
            box.AppendText(DateTime.Now.ToString("HH:mm:ss.fff") + "  " + line + Environment.NewLine);
            box.SelectionStart = box.TextLength; box.ScrollToCaret();
        }

        private static void AppendText(RichTextBox box, string text)
        {
            if (box.TextLength > 120000) box.Text = box.Text.Substring(box.TextLength - 70000);
            box.AppendText(text); box.SelectionStart = box.TextLength; box.ScrollToCaret();
        }

        private void Shutdown()
        {
            if (disposed) return;
            timer.Stop();
            try { ClosePort(false); } catch { }
            try { ClosePort(true); } catch { }
            Record("SESSION_END", "正常退出", "");
            disposed = true; timer.Dispose();
            if (journal != null) { try { journal.Dispose(); } catch { } journal = null; }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing) Shutdown();
            base.Dispose(disposing);
        }

        public void RunSmokeTest()
        {
            if (!smoke) throw new InvalidOperationException("Only for smoke test.");
            string output = Path.Combine(baseDir, "验证结果"); Directory.CreateDirectory(output);
            StartPosition = FormStartPosition.Manual;
            Location = new Point(-32000, -32000);
            ShowInTaskbar = false;
            Opacity = 0;
            Show(); Application.DoEvents();
            if (testPort != null || debugPort != null || journal != null) throw new Exception("Smoke test must not connect serial ports or record a session.");
            if (pagesGrid.Rows.Count < 8 || valuesGrid.Rows.Count < 10 || String.IsNullOrWhiteSpace(preview.Text))
                throw new Exception("Missing default pages, home parameters or frame preview.");
            List<string> checks = new List<string>();
            checks.Add("GUI offline test: " + DateTimeOffset.Now.ToString("o"));
            checks.Add("PASS: form initialized; no serial port opened; no automatic hardware transmission.");
            checks.Add("PASS: " + pagesGrid.Rows.Count + " default pages; home has " + valuesGrid.Rows.Count + " parameters.");
            foreach (TabPage tab in tabs.TabPages)
            {
                tabs.SelectedTab = tab; Application.DoEvents();
                using (Bitmap bitmap = new Bitmap(Width, Height))
                { DrawToBitmap(bitmap, new Rectangle(0, 0, Width, Height)); bitmap.Save(Path.Combine(output, tab.Text.Replace('/', '_') + ".png")); }
                checks.Add("PASS: tab rendered: " + tab.Text);
            }
            if (sendButtons.Any(button => button.Enabled)) throw new Exception("Send controls must be disabled before connection.");
            checks.Add("PASS: all send controls disabled before connecting.");
            RunOfflineUiChecks(checks);
            File.WriteAllLines(Path.Combine(output, "界面自测.txt"), checks.ToArray(), Encoding.UTF8);
            Hide();
        }

        private void RunOfflineUiChecks(List<string> checks)
        {
            tabs.SelectedIndex = 0;
            pagesGrid.CurrentCell = pagesGrid.Rows[0].Cells[1]; ShowSelection();
            PageSpec snapshot = Protocol.Clone(Selected());
            valuesGrid.Rows[0].Cells[1].Value = 20;
            CommitEdits();
            if (Selected().Values[0] != 20 || snapshot.Values[0] != 10)
                throw new Exception("Parameter edits must commit without changing a running snapshot.");
            valuesGrid.Rows[0].Cells[1].Value = 10; CommitEdits();
            pagesGrid.CurrentCell = pagesGrid.Rows[1].Cells[1]; ShowSelection();
            if (valuesGrid.Rows.Count != 4) throw new Exception("Selection did not load menu parameters.");
            valuesGrid.Rows[0].Cells[1].Value = 70000;
            bool rejected = false;
            try { CommitEdits(); } catch (ArgumentException) { rejected = true; }
            if (!rejected) throw new Exception("Out-of-range u16 edit was not rejected.");
            valuesGrid.Rows[0].Cells[1].Value = 100; CommitEdits();
            checks.Add("PASS: page selection, parameter editing, u16 rejection and immutable run snapshot.");

            ProcessDebug(Encoding.UTF8.GetBytes("[BOOT] 3  u8g2Init\r\n")); UpdateStatus();
            if (lastBoot != 3 || !bootStatus.Text.Contains("DMA")) throw new Exception("BOOT3 classification failed.");
            byte[] debugBytes = Encoding.UTF8.GetBytes("[BOOT] 4  fifo_init\r\n[BOOT] 10 Main_Circulation\r\n离线模拟日志，不是板上实测。\r\n");
            ProcessDebug(debugBytes.Take(debugBytes.Length - 5).ToArray());
            ProcessDebug(debugBytes.Skip(debugBytes.Length - 5).ToArray()); UpdateStatus();
            if (lastBoot != 10 || !bootStatus.Text.Contains("10")) throw new Exception("BOOT10 classification failed.");
            checks.Add("PASS: BOOT3/BOOT10 classification and UTF8 fragmented debug receive.");

            byte[] response = new byte[87]; response[0] = 0xAA; response[1] = 0xEE;
            response[3] = 81; response[4] = 3; response[5] = 8;
            response[6] = 0xFF; response[7] = 0xF1; response[8] = 37;
            int sum = 0; for (int i = 4; i < 85; i++) sum += response[i];
            response[85] = (byte)(sum & 255); response[86] = 0x0A;
            Enqueue(new RxChunk { Debug = false, Generation = testGeneration, Bytes = response });
            PumpReceive();
            if (Convert.ToString(channelsGrid.Rows[0].Cells[3].Value) != "-1" || rxFrameCount != 1)
                throw new Exception("Receive dispatch or signed angle display failed.");
            checks.Add("PASS: queued receive, complete response and signed angle channel display (-15/10=-1).");
            ResetChannels("离线模拟换连接");
            if (Convert.ToString(channelsGrid.Rows[0].Cells[2].Value) != "—") throw new Exception("Old channel values survived a connection reset.");
            checks.Add("PASS: channel view reset removes values from the previous connection.");

            JournalFailureStream stream = new JournalFailureStream();
            journal = new StreamWriter(stream, new UTF8Encoding(false)); journal.AutoFlush = true;
            stream.FailWrites = true;
            int writes = 0;
            bool attemptFailed = false;
            try { SubmitFrame(Protocol.Frame(Protocol.Home()), "离线失败注入", delegate { writes++; }); }
            catch (IOException) { attemptFailed = true; }
            if (!attemptFailed || writes != 0 || !journalFaulted) throw new Exception("Journal failure allowed an unrecorded write.");
            try { SubmitFrame(Protocol.Frame(Protocol.Home()), "再次发送", delegate { writes++; }); }
            catch (IOException) { }
            if (writes != 0) throw new Exception("Further write was allowed after journal failure.");
            checks.Add("PASS: failed TX_ATTEMPT prevents transport write and locks further sends.");

            journalFaulted = false;
            stream = new JournalFailureStream(); journal = new StreamWriter(stream, new UTF8Encoding(false)); journal.AutoFlush = true;
            bool resultFailed = false;
            try { SubmitFrame(Protocol.Frame(Protocol.Home()), "离线结果失败注入", delegate { writes++; stream.FailWrites = true; }); }
            catch (IOException ex) { resultFailed = ex.Message.Contains("已提交串口"); }
            if (!resultFailed || writes != 1 || !journalFaulted || txCount != 1)
                throw new Exception("Post-write journal failure lost the distinction between submitted and blocked writes.");
            checks.Add("PASS: failed TX_OK reports the already-submitted write and stops later sends.");
            journalFaulted = false; txCount = 0;
            tabs.SelectedIndex = 0;
            pagesGrid.CurrentCell = pagesGrid.Rows[0].Cells[1]; ShowSelection(); UpdateStatus();
        }
    }
}
