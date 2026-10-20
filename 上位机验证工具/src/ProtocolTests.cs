using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace Ch584ScreenVerifier
{
    // Independent wire-format and compatibility checks. No SerialPort is used.
    public static class ProtocolTests
    {
        private static int passed;
        private static int failed;
        private static readonly byte[] KnownHome = new byte[] {
            0xAA,0xEE,0x00,0x1A,0x01,0x00,0x01,0x00,0x00,0x0A,
            0x00,0x0A,0x01,0x72,0x00,0x01,0x00,0x00,0x00,0x01,
            0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x8C,0x0A
        };

        public static int Main(string[] args)
        {
            Console.OutputEncoding = new UTF8Encoding(false);
            Console.WriteLine("CH584 protocol tests: " + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
            Console.WriteLine("Scope: software only; no serial connection, flashing, or hardware action.");
            Run("Known homepage: 32 bytes / LEN=26 / SUM=8C", delegate {
                var p = Protocol.Home();
                Assert(p.Rank == 1 && p.Sensor == 0, "home fields");
                Assert(p.Values.SequenceEqual(new ushort[] { 10,370,1,0,1,1,0,0,0,0 }), "home values");
                Equal(KnownHome, Protocol.Frame(p), "known wire vector");
                Assert(Protocol.ValidateTransmit(KnownHome) == null, "home transmit allowed");
            });
            Run("Big-endian unsigned parameters and modulo-256 checksum", delegate {
                var p = Protocol.Clone(Protocol.Home());
                p.Values[0] = 0x1234; p.Values[1] = 0xFFFF;
                var f = Protocol.Frame(p);
                Assert(f[10] == 0x12 && f[11] == 0x34 && f[12] == 0xFF && f[13] == 0xFF, "u16 endian");
                Assert(f[f.Length - 2] == Sum(f, 4, f.Length - 2), "SUM only covers CMD through payload");
            });
            Run("HEX formatting and mixed whitespace roundtrip", delegate {
                Equal(KnownHome, Protocol.ParseHex(Protocol.Hex(KnownHome)), "hex roundtrip");
                Equal(new byte[] {0xAA,0xEE,0x0A}, Protocol.ParseHex(" aa\tEE\r\n0a "), "whitespace");
            });
            Run("Invalid and empty HEX are rejected", delegate {
                Throws(delegate { Protocol.ParseHex("AA GG 0A"); }, "nonhex");
                Throws(delegate { Protocol.ParseHex("A EE"); }, "odd nibble");
                Throws(delegate { Protocol.ParseHex(""); }, "empty");
            });
            Run("Homepage requires all ten fixed parameters", delegate {
                var p = Protocol.Clone(Protocol.Home());
                p.Values = new ushort[9]; p.Labels = Labels(9);
                Invalid(p);
            });
            Run("Current menu payloads include two page metadata values", delegate {
                int[] counts = {4,8,16,23,8,5,10,2};
                for (byte menu = 0; menu < counts.Length; menu++) {
                    for (byte rank = 2; rank <= 3; rank++) {
                        var p = Page(rank, menu, counts[menu]);
                        Assert(Protocol.Validate(p) == null, "menu=" + menu + " rank=" + rank + " complete");
                        Assert(Protocol.ValidateTransmit(Protocol.Frame(p)) == null, "current page transmit");
                        p.Values = new ushort[counts[menu] - 1]; p.Labels = Labels(p.Values.Length);
                        Invalid(p);
                    }
                }
            });
            Run("Legacy initial-anchor page: menu6/four parameters rejected", delegate {
                Invalid(Page(2, 6, 4)); Invalid(Page(3, 6, 4));
            });
            Run("Current menu6 uses eight page parameters plus two metadata values", delegate {
                var p = Page(3, 6, 10);
                p.Values = new ushort[] {100,200,300,1000,2000,3000,10,20,1,0};
                Assert(Protocol.Validate(p) == null, "current menu6");
                Assert(Protocol.Frame(p).Length == 32, "current menu6 frame size");
            });
            Run("Return-home menu7: two metadata values and exact 16-byte page frames", delegate {
                byte[] expected = new byte[] {0xAA,0xEE,0,0x0A,1,1,3,7,0,2,0,1,0,0,0x0F,0x0A};
                for (byte rank = 2; rank <= 3; rank++) {
                    var p = Page(rank, 7, 2); p.Values[0] = 1;
                    byte[] vector = Copy(expected); vector[6] = rank; vector[14] = (byte)(rank + 12);
                    byte[] frame = Protocol.Frame(p);
                    Equal(vector, frame, "menu7 rank=" + rank);
                    Assert(Protocol.ValidateTransmit(frame) == null, "menu7 transmit allowed");
                    Assert(Protocol.Describe(frame).Contains("返回主页"), "menu7 frame named");
                }
            });
            Run("Return-home menu7 rejects missing/extra metadata and malformed raw frames", delegate {
                for (byte rank = 2; rank <= 3; rank++) {
                    foreach (int count in new int[] {0,1,3}) {
                        Invalid(Page(rank, 7, count));
                        byte[] payload = new byte[5 + count * 2];
                        payload[0] = 1; payload[1] = rank; payload[2] = 7; payload[4] = (byte)count;
                        Assert(Protocol.ValidateTransmit(Wire(1, payload)) != null, "menu7 raw metadata count=" + count);
                    }
                    var p = Page(rank, 7, 2); p.Values = new ushort[] {0x1234, 5};
                    byte[] frame = Protocol.Frame(p);
                    Assert(frame[10] == 0x12 && frame[11] == 0x34 && frame[13] == 5, "metadata preserved");
                    Assert(Protocol.ValidateTransmit(frame.Take(frame.Length - 1).ToArray()) != null, "menu7 truncation rejected");
                    frame[frame.Length - 2] ^= 1;
                    Assert(Protocol.ValidateTransmit(frame) != null, "menu7 checksum damage rejected");
                }
            });
            Run("Return-home defaults cover both menu focus and third-level confirmation", delegate {
                var pages = Protocol.Defaults().Where(x => x.Menu == 7).ToList();
                Assert(pages.Count == 2 && pages.Any(x => x.Rank == 2) && pages.Any(x => x.Rank == 3), "both return-home defaults");
                foreach (var p in pages) {
                    Assert(p.Enabled && p.Focus == 0, "ordinary return-home default");
                    Assert(p.Values.SequenceEqual(new ushort[] {1,0}), "only page metadata defaults");
                    Assert(p.Labels.Length == 2 && p.Labels[0].Contains("chu_num2") && p.Labels[1].Contains("re_flag"), "two metadata labels");
                    Assert(Protocol.Validate(p) == null, "valid return-home default");
                }
            });
            Run("Rank6 is a one-parameter transient message", delegate {
                var p = Page(6, 0, 1); p.Values[0] = 1;
                Assert(Protocol.Validate(p) == null, "message valid");
                var frame = Protocol.Frame(p);
                Assert(frame[6] == 6 && frame[9] == 1 && frame.Length == 14, "message frame");
                Assert(Protocol.ValidateTransmit(frame) == null, "message transmit");
                Invalid(Page(6, 0, 0));
            });
            Run("Rank6 message bounds and persistent prompt clear code",delegate {
                foreach(ushort code in new ushort[] {0,8,9}) {
                    var p=Page(6,0,1); p.Values[0]=code;
                    Assert(Protocol.Validate(p)==null,"clear/persistent message " + code);
                }
                var bad=Page(6,0,1); bad.Values[0]=10; Invalid(bad);
            });
            Run("Rank5 screen reinitialize is separate from ordinary pages", delegate {
                var p = Protocol.Reinitialize();
                Assert(p.Rank == 5, "reinitialize rank");
                Assert(Protocol.Validate(p) == null, "reinitialize valid");
                Assert(Protocol.ValidateTransmit(Protocol.Frame(p)) == null, "reinitialize transmit");
                Assert(Protocol.Defaults().Where(x => x.Enabled).All(x => x.Rank != 5 && x.Rank != 6), "no repeated reset/message in default cycle");
            });
            Run("Read request exactly AA EE 00 01 03 03 0A", delegate {
                var expected = new byte[] {0xAA,0xEE,0,1,3,3,0x0A};
                Equal(expected, Protocol.ReadRequest(), "read-only command");
                Assert(Protocol.ValidateTransmit(expected) == null, "03 allowed");
                Assert(Protocol.ValidateTransmit(Wire(3, new byte[] {0})) != null, "03 request must have no payload");
            });
            Run("Only CMD01 and CMD03 may be transmitted", delegate {
                foreach (byte cmd in new byte[] {0,2,4,5,6,7,0xFF}) {
                    Assert(Protocol.ValidateTransmit(Wire(cmd, new byte[0])) != null, "command blocked: " + cmd);
                }
                byte[] sensorReply = Wire(3, new byte[80]);
                Assert(Protocol.ValidateTransmit(sensorReply) != null, "03 response cannot be sent as request");
            });
            Run("Transmit validation rejects SUM, tail, length and parameter-count damage", delegate {
                byte[] sum = Copy(KnownHome); sum[sum.Length - 2] ^= 1;
                byte[] tail = Copy(KnownHome); tail[tail.Length - 1] = 0;
                byte[] len = Copy(KnownHome); len[3]--;
                byte[] count = Copy(KnownHome); count[9] = 9; count[count.Length - 2] = Sum(count,4,count.Length - 2);
                foreach (byte[] f in new byte[][] {sum,tail,len,count}) Assert(Protocol.ValidateTransmit(f) != null, "bad TX frame");
                Assert(Protocol.ValidateTransmit(new byte[0]) != null, "empty TX");
                Assert(Protocol.ValidateTransmit(new byte[] {0xAA,0xEE}) != null, "short TX");
                Assert(Protocol.ValidateTransmit(Concat(KnownHome,KnownHome)) != null, "raw input must be a single frame");
            });
            Run("Page bounds: invalid rank/menu, too many parameters, bad delay", delegate {
                Invalid(Page(0,0,10)); Invalid(Page(7,0,10)); Invalid(Page(2,8,2)); Invalid(Page(3,8,2));
                for (byte rank = 2; rank <= 3; rank++) {
                    byte[] frame = Protocol.Frame(Page(rank,7,2)); frame[7] = 8;
                    frame[frame.Length - 2] = Sum(frame,4,frame.Length - 2);
                    Assert(Protocol.ValidateTransmit(frame) != null, "raw menu8 rejected");
                }
                Invalid(Page(1,0,31));
                var p = Protocol.Home(); p.Delay = 49; Invalid(p);
                p.Delay = 60001; Invalid(p);
                p.Delay = 50; Assert(Protocol.Validate(p) == null, "50ms accepted");
                p.Delay = 60000; Assert(Protocol.Validate(p) == null, "60000ms accepted");
            });
            Run("Defaults valid and cloning does not share mutable values or labels", delegate {
                var pages = Protocol.Defaults(); Assert(pages.Count > 0, "defaults present");
                foreach (var p in pages) { Assert(Protocol.Validate(p) == null, p.Name); Protocol.Frame(p); }
                var original = Protocol.Home(); var clone = Protocol.Clone(original);
                clone.Values[0] ^= 1; clone.Labels[0] = "changed";
                Assert(original.Values[0] == 10 && original.Labels[0] != "changed", "deep clone");
            });

            Run("Decoder: every possible half-packet split", delegate {
                for (int split = 1; split < KnownHome.Length; split++) {
                    var d = new FrameDecoder();
                    Assert(d.Feed(KnownHome.Take(split).ToArray()).Count == 0, "premature split=" + split);
                    var frames = d.Feed(KnownHome.Skip(split).ToArray());
                    Assert(frames.Count == 1, "split result=" + split); Equal(KnownHome,frames[0],"split data");
                }
            });
            Run("Decoder: one byte at a time", delegate {
                var d = new FrameDecoder(); var frames = new List<byte[]>();
                foreach (byte b in KnownHome) frames.AddRange(d.Feed(new byte[] {b}));
                Assert(frames.Count == 1, "bytewise count"); Equal(KnownHome,frames[0],"bytewise data");
            });
            Run("Decoder: three concatenated frames", delegate {
                byte[] report = Wire(6, new byte[80]);
                var d = new FrameDecoder(); var frames = d.Feed(Concat(KnownHome,Protocol.ReadRequest(),report));
                Assert(frames.Count == 3, "concatenated count");
                Equal(KnownHome,frames[0],"first"); Equal(report,frames[2],"last");
            });
            Run("Decoder: noise and overlapping AA header recover", delegate {
                var d = new FrameDecoder(); var frames = d.Feed(Concat(new byte[] {0,0x7E,0xEE,0xAA},KnownHome));
                Assert(frames.Count == 1, "noise recovered"); Equal(KnownHome,frames[0],"noise data");
            });
            Run("Decoder: bad SUM and tail cannot hide following valid frame", delegate {
                foreach (int offset in new int[] {KnownHome.Length - 2,KnownHome.Length - 1}) {
                    var bad = Copy(KnownHome); bad[offset] ^= 1;
                    var d = new FrameDecoder(); var frames = d.Feed(Concat(bad,KnownHome));
                    Assert(frames.Count == 1 && d.Rejected > 0, "damaged frame dropped"); Equal(KnownHome,frames[0],"recovered frame");
                }
            });
            Run("Decoder: impossible LEN 0/FFFF/513 rejects and resynchronizes", delegate {
                foreach (byte[] prefix in new byte[][] {
                    new byte[] {0xAA,0xEE,0,0},new byte[] {0xAA,0xEE,0xFF,0xFF},new byte[] {0xAA,0xEE,2,1}
                }) {
                    var d = new FrameDecoder(); var frames = d.Feed(Concat(prefix,KnownHome));
                    Assert(frames.Count == 1 && d.Rejected > 0, "impossible length"); Equal(KnownHome,frames[0],"length recovery");
                }
            });
            Run("Decoder: 03 response and 06 report parsed without transmit permission", delegate {
                byte[] payload = new byte[80]; payload[0]=8; payload[1]=0xFF; payload[2]=0x9C; payload[3]=33;
                foreach (byte cmd in new byte[] {3,6}) {
                    var f = Wire(cmd,payload); var d = new FrameDecoder(); var result=d.Feed(f);
                    Assert(result.Count == 1 && f.Length == 87, "sensor frame accepted");
                    string desc = Protocol.Describe(f);
                    Assert(desc.Contains("通道1") && desc.Contains("65436") && desc.Contains("-100") && desc.Contains("3.3 V"), "channel1 endian/signed/voltage interpretation");
                    Assert(desc.Contains("通道20"), "all twenty channels described");
                    Assert(Protocol.ValidateTransmit(f) != null, "received sensor frame cannot be TX");
                }
            });
            Run("Decoder: Reset removes a previous partial frame", delegate {
                var d = new FrameDecoder(); d.Feed(KnownHome.Take(10).ToArray()); d.Reset();
                var frames=d.Feed(KnownHome); Assert(frames.Count==1,"reset"); Equal(KnownHome,frames[0],"reset data");
            });
            Run("Decoder: AA EE bytes inside valid payload do not start another frame",delegate {
                byte[] payload=new byte[80]; payload[5]=0xAA; payload[6]=0xEE;
                var expected=Wire(6,payload); var d=new FrameDecoder();
                var frames=d.Feed(expected); Assert(frames.Count==1,"embedded header count"); Equal(expected,frames[0],"embedded header payload");
            });
            Run("Decoder: repeated noise followed by fragmented valid packet", delegate {
                var d=new FrameDecoder();
                for(int i=0;i<64;i++) Assert(d.Feed(new byte[1024]).Count==0,"noise");
                Assert(d.Feed(KnownHome.Take(4).ToArray()).Count==0,"header only");
                Assert(d.Feed(KnownHome.Skip(4).ToArray()).Count==1,"long noise recovery");
            });

            string resultsDir = Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"验证结果");
            Directory.CreateDirectory(resultsDir);
            string fixture = Path.Combine(resultsDir,"协议自测临时_" + Guid.NewGuid().ToString("N") + ".xml");
            try {
                Run("XML: current format save/load preserves Unicode and values",delegate {
                    var p=Protocol.Home(); p.Name="中文主页 <测试> & 值";
                    Protocol.SaveXml(fixture,new List<PageSpec> {p});
                    var loaded=Protocol.LoadXml(fixture);
                    Assert(loaded.Count==1 && loaded[0].Name==p.Name,"XML page/name");
                    Equal(Protocol.Frame(p),Protocol.Frame(loaded[0]),"XML frame roundtrip");
                    Assert(loaded[0].Labels.SequenceEqual(p.Labels),"XML labels");
                });
                Run("XML: return-home pages preserve metadata and remain transmit-compatible",delegate {
                    var pages = Protocol.Defaults().Where(x => x.Menu == 7).Select(Protocol.Clone).ToList();
                    pages[1].Values = new ushort[] {2,5};
                    Protocol.SaveXml(fixture,pages);
                    var loaded = Protocol.LoadXml(fixture);
                    Assert(loaded.Count == 2,"return-home XML page count");
                    for (int i = 0; i < loaded.Count; i++) {
                        Assert(loaded[i].Menu == 7 && loaded[i].Rank == pages[i].Rank,"return-home XML codes");
                        Equal(Protocol.Frame(pages[i]),Protocol.Frame(loaded[i]),"return-home XML wire roundtrip");
                        Assert(Protocol.ValidateTransmit(Protocol.Frame(loaded[i])) == null,"return-home XML transmit allowed");
                    }
                });
                Run("XML: legacy initial-anchor profile imports but cannot transmit",delegate {
                    string reference = args.Length>0 ? args[0] : @"D:\y\tools\页面轮播\初锚力版\初锚力页_重置返回切换_50ms.xml";
                    Assert(File.Exists(reference),"reference XML exists");
                    var loaded=Protocol.LoadXml(reference); Assert(loaded.Count==4,"legacy page count");
                    foreach(var p in loaded) {
                        Assert(p.Menu==6 && p.Values.Length==4 && p.Delay==50,"legacy fields kept");
                        Invalid(p);
                    }
                });
                Run("XML: empty, malformed, empty-list and wrong root rejected",delegate {
                    foreach(string xml in new string[] {"","<ArrayOfPageSpec>","<ArrayOfPageSpec />","<WrongRoot />"}) {
                        File.WriteAllText(fixture,xml,new UTF8Encoding(true));
                        Throws(delegate {Protocol.LoadXml(fixture);},"bad XML");
                    }
                });
                Run("XML: duplicate Labels and mismatched label counts rejected",delegate {
                    foreach(string labels in new string[] {
                        "<Labels><string>one</string></Labels><Labels><string>two</string></Labels>",
                        "<Labels><string>one</string></Labels>"
                    }) {
                        File.WriteAllText(fixture,XmlPage(labels,"<Values><unsignedShort>1</unsignedShort><unsignedShort>2</unsignedShort></Values>"),new UTF8Encoding(true));
                        Throws(delegate {Protocol.LoadXml(fixture);},"ambiguous/missing labels");
                    }
                });
                Run("XML: missing/empty Labels generates names without changing protocol values",delegate {
                    foreach(string labels in new string[] {"","<Labels />"}) {
                        File.WriteAllText(fixture,XmlPage(labels,"<Values><unsignedShort>1234</unsignedShort><unsignedShort>5678</unsignedShort></Values>"),new UTF8Encoding(true));
                        var loaded=Protocol.LoadXml(fixture);
                        Assert(loaded.Count==1 && loaded[0].Labels.Length==2,"generated labels");
                        Assert(loaded[0].Values.SequenceEqual(new ushort[] {1234,5678}),"values preserved");
                        Assert(loaded[0].Rank==2 && loaded[0].Menu==0,"page codes preserved");
                        Invalid(loaded[0]); // Current menu0 still requires four values, not two.
                    }
                });
                Run("XML: duplicate/missing Values and out-of-range u16 rejected",delegate {
                    foreach(string values in new string[] {
                        "<Values><unsignedShort>1</unsignedShort></Values><Values><unsignedShort>2</unsignedShort></Values>",
                        "", "<Values><unsignedShort>65536</unsignedShort></Values>",
                        "<Values><unsignedShort>-1</unsignedShort></Values>"
                    }) {
                        File.WriteAllText(fixture,XmlPage("<Labels><string>one</string></Labels>",values),new UTF8Encoding(true));
                        Throws(delegate {Protocol.LoadXml(fixture);},"ambiguous/missing values");
                    }
                });
                Run("XML: external entities are disabled",delegate {
                    string xml="<!DOCTYPE ArrayOfPageSpec [<!ENTITY x SYSTEM 'file:///does-not-exist'>]><ArrayOfPageSpec><PageSpec><Name>&x;</Name></PageSpec></ArrayOfPageSpec>";
                    File.WriteAllText(fixture,xml,new UTF8Encoding(true));
                    Throws(delegate {Protocol.LoadXml(fixture);},"DTD rejected");
                });
            }
            finally { if(File.Exists(fixture)) File.Delete(fixture); }

            Console.WriteLine("RESULT: " + passed + " passed, " + failed + " failed.");
            Console.WriteLine("Firmware compile/flashing, actual UART receipt, screen pixels and power: not verified.");
            return failed==0 ? 0 : 1;
        }

        private static PageSpec Page(byte rank,byte menu,int count)
        {
            return new PageSpec {Name="test",Sensor=1,Rank=rank,Menu=menu,Focus=0,Delay=1500,Labels=Labels(count),Values=new ushort[count]};
        }
        private static string[] Labels(int count) {return Enumerable.Range(0,count).Select(x=>"P"+x).ToArray();}
        private static void Invalid(PageSpec p) {
            Assert(!String.IsNullOrEmpty(Protocol.Validate(p)),"validation must reject");
            Throws(delegate {Protocol.Frame(p);},"Frame must reject");
        }
        private static void Assert(bool ok,string why) {if(!ok) throw new Exception(why);}
        private static void Equal(byte[] expected,byte[] actual,string why) {
            Assert(actual!=null && expected.SequenceEqual(actual),why + " expected=" + Protocol.Hex(expected) + " actual=" + (actual==null ? "null" : Protocol.Hex(actual)));
        }
        private static void Throws(Action action,string why) {
            bool thrown=false;
            try {action();}
            catch(Exception ex) {
                if(ex is ArgumentException || ex is FormatException || ex is InvalidOperationException || ex is System.Xml.XmlException) thrown=true;
                else throw;
            }
            Assert(thrown,why);
        }
        private static void Run(string name,Action action) {
            try {action(); passed++; Console.WriteLine("PASS " + name);}
            catch(Exception ex) {failed++; Console.WriteLine("FAIL " + name + ": " + ex.Message);}
        }
        private static byte[] Copy(byte[] b) {return (byte[])b.Clone();}
        private static byte[] Concat(params byte[][] chunks) {return chunks.SelectMany(x=>x).ToArray();}
        private static byte Sum(byte[] b,int first,int endExclusive) {
            int sum=0; for(int i=first;i<endExclusive;i++) sum+=b[i]; return unchecked((byte)sum);
        }
        // Independent reference builder for command-only requests and sensor replies.
        private static byte[] Wire(byte cmd,byte[] payload) {
            int len=1+payload.Length; byte[] frame=new byte[len+6];
            frame[0]=0xAA;frame[1]=0xEE;frame[2]=(byte)(len>>8);frame[3]=(byte)len;frame[4]=cmd;
            Array.Copy(payload,0,frame,5,payload.Length);
            frame[frame.Length-2]=Sum(frame,4,frame.Length-2);frame[frame.Length-1]=0x0A;return frame;
        }
        private static string XmlPage(string labels,string values) {
            return "<?xml version=\"1.0\" encoding=\"utf-8\"?><ArrayOfPageSpec><PageSpec><Name>test</Name><Enabled>true</Enabled><Sensor>1</Sensor><Rank>2</Rank><Menu>0</Menu><Focus>0</Focus><Delay>1500</Delay>"+labels+values+"</PageSpec></ArrayOfPageSpec>";
        }
    }
}
