using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml;
using System.Xml.Serialization;

namespace Ch584ScreenVerifier
{
    // Field names and array element names intentionally match the original ArrayOfPageSpec XML.
    public class PageSpec
    {
        public string Name;
        public bool Enabled = true;
        public byte Sensor = 1, Rank = 2, Menu, Focus;
        public int Delay = 1500;
        public string[] Labels;
        public ushort[] Values;
    }

    public static class Protocol
    {
        internal const int MaximumFrameLength = 512;
        private static readonly int[] MenuCounts = { 4, 8, 16, 23, 8, 5, 10, 2 };
        private static readonly string[] MenuNames =
            { "地址分区", "组网测试", "设备绑定", "安装调试", "上传设置", "其他设置", "信息汇总", "返回主页" };
        private static readonly string[] SensorNames =
            { "无数据", "锚杆", "激光", "位移2", "位移4", "位移6", "位移8", "裂缝", "倾角", "应力", "液位", "微震", "地音", "测试" };
        private static readonly string[] MessageNames =
            { "清除提示", "参数保存成功", "参数保存失败", "绑定保存成功", "绑定保存失败", "无绑定可保存", "绑定已清除", "绑定清除失败", "长按K1+K3开机", "正在关机中" };

        public static string Validate(PageSpec page)
        {
            string error = ValidateShape(page);
            if (error != null) return error;
            if (page.Sensor > 13) return "传感器类型应在 0～13 内。";
            if (page.Focus > 7) return "三级焦点应在 0～7 内。";
            if (page.Rank < 1 || page.Rank > 6) return "页面等级应在 1～6 内。";
            int count = page.Values.Length;
            if (page.Rank == 1)
            {
                // Firmware reads ten fixed words although its own early check only requires five.
                if (count < 10 || count > 30)
                    return "当前首页必须包含 10 个固定参数，可再追加最多 20 个参数（总数 10～30）。旧 9 参数首页不兼容。";
            }
            else if (page.Rank == 2 || page.Rank == 3)
            {
                if (page.Menu >= MenuCounts.Length)
                    return "当前固件仅接收二级菜单 0～7；返回主页菜单确认后由主控发送等级 1 的首页帧。";
                if (page.Menu == 6 && count == 4)
                    return "此配置是旧 4 参数初锚力页；当前菜单 6 是信息汇总，要求原 8 参数加 chu_num2/re_flag 共 10 参数。不能直接发送，也不会自动改写旧页面语义。";
                if (count != MenuCounts[page.Menu])
                    return "当前菜单 " + page.Menu + "（" + MenuNames[page.Menu] + "）需要 " + MenuCounts[page.Menu] +
                        " 个参数，末尾两项必须为 chu_num2（子页页码）和 re_flag（子页选择）；旧参数块不能直接发送。";
            }
            else if (page.Rank == 6)
            {
                if (count != 1 || page.Values[0] > 9)
                    return "等级 6 的提示帧必须且只能含 1 个 msg 参数，取值 0～9；8/9 是常驻关机提示，需发送 msg=0 清除。";
            }
            else if (count != 0)
            {
                return "等级 4/5 不携带参数；等级 5 请求显示重新初始化，会清空屏幕，之后应发送首页。";
            }
            return null;
        }

        private static string ValidateShape(PageSpec page)
        {
            if (page == null) return "页面对象为空。";
            if (page.Values == null) return "页面缺少 Values 参数数组。";
            if (page.Labels == null) return "页面缺少 Labels 参数标签数组。";
            if (page.Labels.Length != page.Values.Length) return "Labels 和 Values 的数量必须一致。";
            if (page.Values.Length > 30) return "参数数目不能超过当前固件的 30 个上限。";
            if (page.Delay < 50 || page.Delay > 60000) return "停留时间应在 50～60000 ms 内。";
            return null;
        }

        public static byte[] Frame(PageSpec page)
        {
            string error = Validate(page);
            if (error != null) throw new ArgumentException(error, "page");
            byte[] frame = new byte[12 + 2 * page.Values.Length];
            int payloadLength = frame.Length - 6;
            frame[0] = 0xAA; frame[1] = 0xEE;
            frame[2] = (byte)(payloadLength >> 8); frame[3] = (byte)payloadLength;
            frame[4] = 0x01; frame[5] = page.Sensor; frame[6] = page.Rank;
            frame[7] = page.Menu; frame[8] = page.Focus; frame[9] = (byte)page.Values.Length;
            for (int i = 0; i < page.Values.Length; i++)
            {
                frame[10 + i * 2] = (byte)(page.Values[i] >> 8);
                frame[11 + i * 2] = (byte)page.Values[i];
            }
            frame[frame.Length - 2] = Sum(frame, 4, payloadLength);
            frame[frame.Length - 1] = 0x0A;
            return frame;
        }

        public static byte[] ReadRequest()
        {
            return new byte[] { 0xAA, 0xEE, 0x00, 0x01, 0x03, 0x03, 0x0A };
        }

        public static string Hex(byte[] data)
        {
            return data == null ? "" : BitConverter.ToString(data).Replace('-', ' ');
        }

        public static byte[] ParseHex(string input)
        {
            if (String.IsNullOrWhiteSpace(input)) throw new ArgumentException("请输入十六进制字节。", "input");
            if (input.Length > 8192) throw new FormatException("十六进制输入过长；一帧最多 512 字节。");
            string compact = Regex.Replace(input, "0[xX]", "");
            compact = Regex.Replace(compact, @"[\s,;:\-]+", "");
            if (compact.Length == 0 || (compact.Length & 1) != 0 || !Regex.IsMatch(compact, @"\A[0-9a-fA-F]+\z"))
                throw new FormatException("每个字节必须有两位十六进制数字；可使用空格、逗号、冒号、分号或连字符分隔。");
            if (compact.Length / 2 > MaximumFrameLength) throw new FormatException("一帧最多 512 字节。");
            byte[] data = new byte[compact.Length / 2];
            for (int i = 0; i < data.Length; i++)
                data[i] = Byte.Parse(compact.Substring(i * 2, 2), NumberStyles.HexNumber, CultureInfo.InvariantCulture);
            return data;
        }

        internal static byte Sum(byte[] data, int start, int length)
        {
            int sum = 0;
            for (int i = start; i < start + length; i++) sum += data[i];
            return (byte)(sum & 0xFF);
        }

        internal static string CheckFrame(byte[] frame)
        {
            if (frame == null || frame.Length < 7) return "完整帧至少为 7 字节。";
            if (frame.Length > MaximumFrameLength) return "整帧长度超过 512 字节。";
            if (frame[0] != 0xAA || frame[1] != 0xEE) return "帧头应为 AA EE。";
            int length = (frame[2] << 8) | frame[3];
            if (length < 1 || length + 6 != frame.Length) return "LEN 应等于从 CMD 到 DATA 的字节数，整帧长度应为 LEN+6。";
            if (frame[frame.Length - 1] != 0x0A) return "帧尾应为单字节 0A。";
            if (Sum(frame, 4, length) != frame[frame.Length - 2]) return "SUM 错误，应从 CMD 累加到 DATA 末尾并取低 8 位。";
            return null;
        }

        public static string ValidateTransmit(byte[] frame)
        {
            string error = CheckFrame(frame);
            if (error != null) return error;
            if (frame[4] == 0x03)
                return frame.Length == 7 ? null : "0x03 只允许无参数的只读请求：AA EE 00 01 03 03 0A；不能发送应答载荷。";
            if (frame[4] != 0x01)
                return "辅助工具只允许发送 0x01 页面帧或 0x03 只读请求；禁止发送 0x02/0x04/0x05/0x06/0x07 等其他命令。";
            if (frame.Length < 12) return "0x01 页面帧至少为 12 字节。";
            int count = frame[9];
            if (count > 30 || frame.Length != 12 + count * 2) return "0x01 的 param_cnt、LEN 和实际参数字节数不一致。";
            PageSpec page = new PageSpec();
            page.Name = "手工页面帧"; page.Sensor = frame[5]; page.Rank = frame[6];
            page.Menu = frame[7]; page.Focus = frame[8];
            page.Labels = new string[count]; page.Values = new ushort[count];
            for (int i = 0; i < count; i++)
            {
                page.Labels[i] = "参数" + (i + 1);
                page.Values[i] = (ushort)((frame[10 + i * 2] << 8) | frame[11 + i * 2]);
            }
            return Validate(page);
        }

        public static string Describe(byte[] frame)
        {
            string error = CheckFrame(frame);
            if (error != null) return "无效帧：" + error;
            int length = (frame[2] << 8) | frame[3];
            byte command = frame[4];
            StringBuilder text = new StringBuilder();
            if ((command == 0x03 || command == 0x06) && length == 81)
            {
                text.Append(command == 0x03 ? "0x03 读取应答" : "0x06 采集主动上报");
                text.Append("：20 通道，LEN=81（原始数据大端；voltage/10 V 按当前UI显示，不代表物理实测）");
                for (int i = 0; i < 20; i++)
                {
                    int offset = 5 + i * 4;
                    byte type = frame[offset];
                    int raw = (frame[offset + 1] << 8) | frame[offset + 2];
                    byte voltage = frame[offset + 3];
                    string name = type < SensorNames.Length ? SensorNames[type] : "未知类型";
                    text.AppendLine();
                    text.Append("通道").Append(i + 1).Append("：type=").Append(type).Append("（").Append(name).Append("）");
                    text.Append("，data=").Append(raw);
                    if (type == 8) text.Append("（有符号倾角原值 ").Append(unchecked((short)raw)).Append("）");
                    text.Append("，voltage=").Append(voltage).Append("（");
                    text.Append((voltage / 10.0).ToString("0.0", CultureInfo.InvariantCulture)).Append(" V）");
                }
                return text.ToString();
            }
            if (command == 0x03 && length == 1)
                return "0x03 只读请求；当前固件等待本轮收齐或 30 秒超时后应答，发送成功不代表已收到应答。";
            if (command == 0x01)
            {
                if (frame.Length < 12 || frame.Length != 12 + frame[9] * 2)
                    return "0x01 页面帧：固定字段/参数数量不完整。";
                string name;
                byte rank = frame[6], menu = frame[7];
                if (rank == 1) name = "首页";
                else if (rank == 4) name = "旧等级4重启保存提示（仅显示，仅手动）";
                else if (rank == 5) name = "显示重新初始化（需后续首页帧）";
                else if (rank == 6)
                {
                    int message = frame[9] > 0 ? (frame[10] << 8) | frame[11] : -1;
                    name = message >= 0 && message < MessageNames.Length ? MessageNames[message] : "未知提示";
                }
                else name = menu < MenuNames.Length ? MenuNames[menu] : "未知菜单";
                return "0x01 " + name + "：type=" + frame[5] + "，rank=" + rank + "，menu=" + menu +
                    "，focus=" + frame[8] + "，参数=" + frame[9] + "；页面帧没有专门的串口 ACK。";
            }
            if (command == 0x03 || command == 0x06)
                return "0x" + command.ToString("X2", CultureInfo.InvariantCulture) + " 载荷长度异常：当前 20 通道应答/上报要求 LEN=81，实际 LEN=" + length + "。";
            return "收到命令 0x" + command.ToString("X2", CultureInfo.InvariantCulture) + "，LEN=" + length + "，SUM 正确。";
        }

        public static PageSpec Home()
        {
            PageSpec page = NewPage("首页（模拟主控状态，本地 BLE 通道数据来自固件）", 1, 0,
                "版本（10=V1.0）|电池电压（百分之一V）|主机号|发送主机号（0=中继）|分站号|状态（1开机，0关机）|LoRa信号值|首页页码 chu_num1|子页页码 chu_num2|子页选择 re_flag",
                new ushort[] { 10, 370, 1, 0, 1, 1, 0, 0, 0, 0 });
            page.Sensor = 0;
            return page;
        }

        public static PageSpec Reinitialize()
        {
            PageSpec page = NewPage("显示重新初始化（会清空，随后发送首页）", 5, 0, "", new ushort[0]);
            page.Sensor = 0; page.Enabled = false;
            return page;
        }

        public static PageSpec Clone(PageSpec page)
        {
            if (page == null) throw new ArgumentNullException("page");
            return new PageSpec
            {
                Name = page.Name, Enabled = page.Enabled, Sensor = page.Sensor,
                Rank = page.Rank, Menu = page.Menu, Focus = page.Focus, Delay = page.Delay,
                Labels = page.Labels == null ? null : (string[])page.Labels.Clone(),
                Values = page.Values == null ? null : (ushort[])page.Values.Clone()
            };
        }

        private static PageSpec NewPage(string name, byte rank, byte menu, string labels, ushort[] values)
        {
            return new PageSpec
            {
                Name = name, Rank = rank, Menu = menu,
                Labels = labels.Length == 0 ? new string[0] : labels.Split('|'), Values = values
            };
        }

        private static PageSpec MenuPage(string name, byte rank, byte menu, byte focus, ushort reFlag)
        {
            string labels;
            ushort[] original;
            switch (menu)
            {
                case 0:
                    labels = "主机号（模拟）|分站号（模拟）";
                    original = new ushort[] { 100, 2 }; break;
                case 1:
                    labels = "次数序号（模拟）|本机地址（模拟）|测试地址（模拟）|测试次数（模拟）|成功比率%（模拟）|页面（0组网选择，1组网结果）";
                    original = new ushort[] { 1, 100, 118, 10, 90, 0 }; break;
                case 2:
                    labels = "遗留标定组1数值1|遗留标定组1数值2|遗留标定组2数值1|遗留标定组2数值2|遗留标定组3数值1|遗留标定组3数值2|遗留密码第1位|遗留密码第2位|遗留密码第3位|遗留密码第4位|遗留密码页标志|遗留标定组1标志|遗留标定组2标志|遗留标定组3标志";
                    original = new ushort[] { 100, 1000, 200, 2000, 300, 3000, 1, 2, 3, 4, 0, 1, 1, 1 }; break;
                case 3:
                    string[] realTime = new string[21];
                    original = new ushort[21];
                    for (int i = 0; i < 20; i++)
                    {
                        realTime[i] = "遗留实时参数" + (i + 1) + "（本页实际显示本地 BLE 数据）";
                        original[i] = (ushort)((i + 1) * 100);
                    }
                    realTime[20] = "遗留卡尔曼选项"; original[20] = 3;
                    labels = String.Join("|", realTime); break;
                case 4:
                    labels = "原地址1（模拟）|原地址2（模拟）|原地址3（模拟）|新地址1（模拟）|新地址2（模拟）|新地址3（模拟）";
                    original = new ushort[] { 100, 101, 102, 110, 111, 112 }; break;
                case 5:
                    labels = "通信功率（模拟）|亮屏秒数（0常亮）|通信状态（1开机，0关机）";
                    original = new ushort[] { 20, 0, 1 }; break;
                case 6:
                    labels = "遗留长度值1|遗留长度值2|遗留长度值3|遗留AD值1|遗留AD值2|遗留AD值3|遗留旧长度|遗留新长度";
                    original = new ushort[] { 1234, 5678, 9012, 1000, 2000, 3000, 1234, 5678 }; break;
                case 7:
                    labels = "";
                    original = new ushort[0]; break;
                default: throw new ArgumentOutOfRangeException("menu");
            }
            ushort[] values = new ushort[original.Length + 2];
            Array.Copy(original, values, original.Length);
            values[values.Length - 2] = 1;
            values[values.Length - 1] = reFlag;
            PageSpec page = NewPage(name, rank, menu, (labels.Length == 0 ? "" : labels + "|") + "子页页码 chu_num2（1/2）|子页选择 re_flag（0菜单，1解绑列表，2绑定扫描，3信号，4电压，5名称，6恢复确认）", values);
            page.Focus = focus;
            return page;
        }

        public static List<PageSpec> Defaults()
        {
            List<PageSpec> pages = new List<PageSpec>();
            pages.Add(Home());
            for (byte menu = 0; menu < MenuNames.Length; menu++)
                pages.Add(MenuPage(MenuNames[menu] + " · 二级菜单", 2, menu, 0, 0));
            pages.Add(MenuPage("返回主页 · 三级确认页（主控确认后下发首页）", 3, 7, 0, 0));
            PageSpec results = MenuPage("组网结果（模拟统计）", 3, 1, 0, 0);
            results.Values[5] = 1; pages.Add(results);
            pages.Add(MenuPage("地址分区 · 主机号焦点", 3, 0, 1, 0));
            pages.Add(MenuPage("设备绑定 · 绑定项焦点（不进入扫描）", 3, 2, 2, 0));
            pages.Add(MenuPage("设备绑定 · 已绑定列表（本地名称/MAC，只展示）", 3, 2, 0, 1));
            pages.Add(MenuPage("安装调试 · 设备信号（本地20通道）", 3, 3, 0, 3));
            pages.Add(MenuPage("安装调试 · 设备电压（本地20通道）", 3, 3, 1, 4));
            pages.Add(MenuPage("信息汇总 · 设备名称（本地20通道）", 3, 6, 0, 5));
            pages.Add(MenuPage("信息汇总 · 名称警报焦点（本地计数）", 3, 6, 1, 0));
            pages.Add(MenuPage("信息汇总 · 电压警报焦点（本地计数）", 3, 6, 2, 0));
            pages.Add(MenuPage("上传设置 · 新地址焦点（模拟）", 3, 4, 1, 0));
            pages.Add(MenuPage("其他设置 · 开关状态焦点（模拟）", 3, 5, 5, 0));
            PageSpec scan = MenuPage("绑定扫描子页（改变扫描模式，仅手动）", 3, 2, 0, 2);
            scan.Enabled = false; pages.Add(scan);
            PageSpec factory = MenuPage("恢复出厂确认页（仅显示，不执行，仅手动）", 3, 6, 1, 6);
            factory.Enabled = false; pages.Add(factory);
            for (ushort message = 0; message <= 9; message++)
            {
                PageSpec prompt = NewPage("提示 · " + MessageNames[message] + "（仅显示，仅手动）", 6, 0,
                    "提示码 msg（0清除；1～7短时；8/9常驻）", new ushort[] { message });
                prompt.Sensor = 0; prompt.Enabled = false; pages.Add(prompt);
            }
            pages.Add(Reinitialize());
            PageSpec legacy = NewPage("旧等级4重启保存提示（仅显示，仅手动）", 4, 0, "", new ushort[0]);
            legacy.Enabled = false; pages.Add(legacy);
            return pages;
        }

        private static void CheckXmlPages(List<PageSpec> pages)
        {
            if (pages == null || pages.Count < 1 || pages.Count > 100)
                throw new ArgumentException("XML 配置应包含 1～100 个页面。");
            for (int i = 0; i < pages.Count; i++)
            {
                PageSpec page = pages[i];
                // Labels have no protocol meaning. Supplying names for omitted labels is safe;
                // Values and page codes are preserved exactly, including incompatible old pages.
                if (page != null && page.Values != null && (page.Labels == null || page.Labels.Length == 0))
                {
                    page.Labels = new string[page.Values.Length];
                    for (int j = 0; j < page.Labels.Length; j++) page.Labels[j] = "参数" + (j + 1);
                }
                string error = ValidateShape(page);
                if (error != null) throw new ArgumentException("XML 第 " + (i + 1) + " 页：" + error);
            }
        }

        public static List<PageSpec> LoadXml(string path)
        {
            XmlReaderSettings settings = new XmlReaderSettings();
            settings.DtdProcessing = DtdProcessing.Prohibit;
            settings.XmlResolver = null;
            settings.MaxCharactersInDocument = 1024 * 1024;
            XmlDocument document = new XmlDocument();
            document.XmlResolver = null;
            try
            {
                using (XmlReader reader = XmlReader.Create(path, settings)) document.Load(reader);
            }
            catch (XmlException ex)
            {
                throw new ArgumentException("XML 结构无效或含被禁止的 DTD：" + ex.Message, "path", ex);
            }
            CheckXmlStructure(document);
            List<PageSpec> pages;
            using (XmlReader reader = new XmlNodeReader(document))
                pages = (List<PageSpec>)new XmlSerializer(typeof(List<PageSpec>)).Deserialize(reader);
            CheckXmlPages(pages);
            return pages;
        }

        private static void CheckXmlStructure(XmlDocument document)
        {
            XmlElement root = document.DocumentElement;
            if (root == null || root.Name != "ArrayOfPageSpec" || root.NamespaceURI.Length != 0)
                throw new ArgumentException("XML 根节点应为 ArrayOfPageSpec。");
            HashSet<string> allowed = new HashSet<string>(StringComparer.Ordinal);
            foreach (string name in new string[] { "Name", "Enabled", "Sensor", "Rank", "Menu", "Focus", "Delay", "Labels", "Values" })
                allowed.Add(name);
            int count = 0;
            foreach (XmlNode page in root.ChildNodes)
            {
                if (page.NodeType == XmlNodeType.Comment || page.NodeType == XmlNodeType.Whitespace || page.NodeType == XmlNodeType.SignificantWhitespace) continue;
                if (page.NodeType != XmlNodeType.Element || page.Name != "PageSpec" || page.NamespaceURI.Length != 0)
                    throw new ArgumentException("ArrayOfPageSpec 内只允许 PageSpec 页面节点。");
                count++;
                HashSet<string> found = new HashSet<string>(StringComparer.Ordinal);
                foreach (XmlNode field in page.ChildNodes)
                {
                    if (field.NodeType == XmlNodeType.Comment || field.NodeType == XmlNodeType.Whitespace || field.NodeType == XmlNodeType.SignificantWhitespace) continue;
                    if (field.NodeType != XmlNodeType.Element || field.NamespaceURI.Length != 0 || !allowed.Contains(field.Name))
                        throw new ArgumentException("XML 第 " + count + " 页包含未知字段：" + field.Name + "。");
                    if (!found.Add(field.Name))
                        throw new ArgumentException("XML 第 " + count + " 页字段重复：" + field.Name + "。");
                    if (field.Name == "Labels" || field.Name == "Values")
                    {
                        string expected = field.Name == "Labels" ? "string" : "unsignedShort";
                        foreach (XmlNode item in field.ChildNodes)
                        {
                            if (item.NodeType == XmlNodeType.Comment || item.NodeType == XmlNodeType.Whitespace || item.NodeType == XmlNodeType.SignificantWhitespace) continue;
                            if (item.NodeType != XmlNodeType.Element || item.Name != expected || item.NamespaceURI.Length != 0)
                                throw new ArgumentException("XML 第 " + count + " 页的 " + field.Name + " 只允许 " + expected + " 元素。");
                            foreach (XmlNode value in item.ChildNodes)
                                if (value.NodeType == XmlNodeType.Element)
                                    throw new ArgumentException("XML 数组元素不能嵌套子元素。");
                        }
                    }
                    else
                    {
                        foreach (XmlNode value in field.ChildNodes)
                            if (value.NodeType == XmlNodeType.Element) throw new ArgumentException("XML 标量字段不能嵌套子元素。");
                    }
                }
                if (!found.Contains("Values")) throw new ArgumentException("XML 第 " + count + " 页缺少 Values 参数数组。");
            }
        }

        public static void SaveXml(string path, List<PageSpec> pages)
        {
            CheckXmlPages(pages);
            XmlWriterSettings settings = new XmlWriterSettings();
            settings.Encoding = new UTF8Encoding(false); settings.Indent = true;
            using (XmlWriter writer = XmlWriter.Create(path, settings))
                new XmlSerializer(typeof(List<PageSpec>)).Serialize(writer, pages);
        }
    }

    public sealed class FrameDecoder
    {
        private readonly List<byte> pending = new List<byte>(Protocol.MaximumFrameLength);
        private readonly object gate = new object();
        public int Rejected;

        public void Reset()
        {
            lock (gate) { pending.Clear(); Rejected = 0; }
        }

        private void RejectFirst()
        {
            pending.RemoveAt(0);
            if (Rejected < Int32.MaxValue) Rejected++;
        }

        public List<byte[]> Feed(byte[] input)
        {
            if (input == null) throw new ArgumentNullException("input");
            List<byte[]> frames = new List<byte[]>();
            lock (gate)
            {
                foreach (byte value in input)
                {
                    pending.Add(value);
                    while (pending.Count > 0)
                    {
                        if (pending[0] != 0xAA) { RejectFirst(); continue; }
                        if (pending.Count < 2) break; // Retain a split AA EE header.
                        if (pending[1] != 0xEE) { RejectFirst(); continue; }
                        if (pending.Count < 4) break;
                        int length = (pending[2] << 8) | pending[3];
                        if (length < 1 || length > Protocol.MaximumFrameLength - 6)
                        { RejectFirst(); continue; }
                        int total = length + 6;
                        if (pending.Count < total) break; // AA EE inside data must remain payload.
                        byte[] candidate = pending.GetRange(0, total).ToArray();
                        if (Protocol.CheckFrame(candidate) != null) { RejectFirst(); continue; }
                        frames.Add(candidate);
                        pending.RemoveRange(0, total);
                    }
                }
            }
            return frames;
        }
    }
}
