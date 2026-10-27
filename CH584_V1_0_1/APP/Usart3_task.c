#include "CONFIG.h"
#include "HAL.h"
#include "Usart3_task.h"
#include "app_drv_fifo.h"
#include "Flash.h"
#include "observer.h"
#include "u8g2.h"
#include "yuying_TFT.h"
#include "screen_power.h"
#include "screen_startup.h"
#include "ui_redraw_policy.h"
#include "ui_latency_diag.h"


#define START_IO_EVT                  0x0001 //

#define START_DATA_EVT                 0x0002 //处理数据

#define START_RX_DATA_EVT              0x0004 //半秒事件
#define START_TIMER_EVT               0x0008 //1秒事件

/* Match the 2.9 display targets without changing the 4.2 wire protocol.
 * Background power, CMD03 and BLE data do not create a visible target.
 * Local RST, rank 5 and first/resumed pages request the startup artwork.
 * Messages have priority; complete each target before enabling the panel.
 */
#define UI_HOLD_SEC        5                /* 帧龄 <= 5 秒才允许重画（仅此一处可调） */
#define FRAME_AGE_NEVER    0xFFFFFFFFu      /* "从未收到 0x01 帧"哨兵 */
uint32_t dis_flag_cnt = 0;
uint32_t g_frame_last_sec = FRAME_AGE_NEVER;    /* 帧龄（秒），门控用，见上 */
static screen_startup_t screen_startup;
static uint8_t displayed_target, test_dirty, restore_pending;
extern volatile uint8_t g_ui_msg;
static screen_power_t screen_power;

static uint8_t ui_page_available(void)
{
    uint8_t target = screen_startup_target(&screen_startup, g_ui_msg);
    return g_frame_last_sec <= UI_HOLD_SEC &&
           (target == SCREEN_TARGET_PAGE || target == SCREEN_TARGET_MSG);
}
/* V2 menu 7 owns binding. Stop discovery after a local action until the
 * user leaves this page; otherwise an unbind immediately binds devices again. */
static uint8_t s_binding_paused;
static uint8_t s_act_cmd, s_act_local, s_act_lock;
static uint8_t s_act_echo_pending;
static uint32_t s_act_since, s_act_echo_since;
static void act_tick_10ms(void);
static void protocol_send(uint8_t *data, uint16_t len);
extern data_LIST Data_list1;

static void update_binding_mode(void)
{
    /* ★ 2026-10-25 第二轮（以屏端画面为准）：
     * 屏上"设备绑定"是左列表第 3 项 —— rank2 == 2（yuying_TFT.c:73 `"设备绑定"`），
     * 它的"绑定设备"扫描画面是 rank2 == 2 且 re_flag == 2（observer.h:52-53 的约定，
     * 三处独立证据：ui_menu_42.h:226 / ui_static_page_key.h:19 / observer.h:53）。
     * 改前这里写的是 rank2_addr == 7u（旧 V2 菜单表里"设备绑定动作页"的序号），
     * 与本屏画面表不符 —— 站在"绑定设备"页时扫描模式不会切到 BIND，
     * 新设备永远发现不了。 */
    if (Data_list1.menu_rank != 3u || Data_list1.rank2_addr != 2u ||
        (uint8_t)Data_list1.UI_main.re_flag != 2u)
    {
        s_binding_paused = 0u;
        g_scan_mode = SCAN_MODE_DATA;
    }
    else
        g_scan_mode = s_binding_paused ? SCAN_MODE_DATA : SCAN_MODE_BIND;
}

/* ★ 契约 §2.7：CH584M 本地内容的实时刷新计数。
 *   START_IO_EVT 的周期已修正为 10ms（见下方 UI_IO_TICK_TICKS）。
 *
 *   ★ 2026-10-25 第二轮刷新提速（改前：每 200ms **无条件**置一次重画请求）：
 *     改前 UI_REFRESH_TICKS = 20（200ms），叠加每条 0x01 帧再置一次，
 *     稳态 5~6 帧/s × ≈120ms/帧（全屏 30000 B 上 SPI）⇒ 显示占 TMOS 60~70%，
 *     按键/BLE/串口都在这 60~70% 里抢时间 —— 现场就是"刷得慢、按键迟钝"。
 *     现在三道闸门一起上：
 *       ① 节拍 200ms -> 600ms（UI_REFRESH_TICKS 20 -> 60）；
 *       ② 只有"真的有新数据"(g_ui_data_dirty) 才置位；
 *       ③ 兜底心跳：每 3 拍（1.8s）强制一次，保证在没有 U0 帧的台架上，
 *          本地 BLE 数据（绑定数 / 信号 / 电压）仍然会刷新。
 *     配合屏端"脏行局部刷新"（ui42_dirty.h），没变化的帧只花 ~2ms 做行 CRC 比较，
 *     一行都不上 SPI。 */
#define UI_IO_TICK_TICKS   16    /* 16 × 0.625ms(TMOS 单位) = 10ms */
#define UI_REFRESH_TICKS   60    /* 60 × 10ms = 600ms → 请求一次重画（改前 20 = 200ms） */
static uint8_t ui_refresh_tick = 0;
static uint8_t s_ui_dirty_miss = 0;

/* ★ 第三轮（2026-10-25）：上一次真正画出去的页面身份
 *   （menu_rank / rank2 / re_flag / 主页页码）。
 *   U0 复位或唤醒后会强制发一帧主页；身份一变就作废脏行缓存并整屏重画一次，
 *   避免屏端以为没变化而一行不发（旧页残留）。 */
static uint16_t s_draw_page_id;
static uint8_t  s_draw_page_valid;

/* 1 = 自上次重画以来收到过新数据（0x01 页面帧 / 本地动作结果）。
   只有它为 1（或兜底心跳到点）才允许 600ms 闸门置重画请求。 */
volatile uint8_t g_ui_data_dirty = 1u;

/* Background redraws retain their 50 ms limit. Accepted navigation changes
 * request START_DATA_EVT immediately and use a 20 ms minimum idle interval.
 * Use the real TMOS clock, not the number of 10 ms task executions. */
static ui_redraw_policy_t ui_redraw;

/* Diagnostic builds only: -DUI_LATENCY_DIAG=1. No per-frame or ISR printing.
 * Statistics start at a complete frame reaching the parser, not a key edge. */
#if UI_LATENCY_DIAG
static uint32_t ui_diag_rx, ui_diag_nav, ui_diag_merge, ui_diag_bad;
static uint32_t ui_diag_wait_max, ui_diag_draw_max, ui_diag_last_report;
static uint32_t ui_diag_last_tick;
static uint32_t ui_diag_render_max, ui_diag_transfer_max;
uint32_t ui_profile_render_ticks, ui_profile_transfer_ticks;
#endif

/* ======================================================================
 * ★★ V1_0_2 第二轮修正：0x04 / 0x07 的"清 Flash"改到 1 秒事件里做
 * ----------------------------------------------------------------------
 * binding_store_clear() 内部是 **4096 字节 Data-Flash 整块擦除**
 * （Flash.c 的 EEPROM_BLOCK_SIZE），量级 20~35ms（最坏 ~70ms），
 * 而 STM32 发一条 7 字节命令只要 0.61ms。原来它直接在 app_uart_process()
 * 里执行 ⇒ 命令一连发，Flash 就被 100% 占满：TMOS 跑不动、BLE 饿死，
 * 而且块擦除次数暴增会很快耗掉 Flash 寿命（4KB 块 10 万次擦写）。
 *
 * 关键点：**应答内容不需要等 Flash 结果**。
 *   0x04 / 0x07 的 status 是"清之前有没有绑定"，完全由 RAM 里的
 *   g_binding_count 决定（observer_clear_all_bindings() 的返回值 had），
 *   所以可以**立即按原协议应答**（协议一字不改），把 Flash 擦除推到
 *   1 秒事件里做；RAM 已经清干净，晚一点擦 Flash 不影响任何语义。
 *   （0x05 的 status 里有"写 Flash 失败(02)"这一档，必须等写入结果，
 *     所以它保持原地执行不动。）
 * ====================================================================== */
volatile uint8_t g_store_clear_req = 0;

/* ★ cmd 0x03「传感器数据读取」应答的数据源：
 *   CH_com_buf[] 定义在 observer.c（非 static，可 extern），下标即通道号 0..19
 *   （MAX_CH_NUM 已按契约 §2.2 由 30 收敛为 20），本应答只取前 20 个
 *   （i = 0..19），与协议契约 §2.2 的通道索引一一对应。
 *   device_t / MAX_CH_NUM 来自 observer.h（本文件第 6 行已 include）。 */
extern device_t CH_com_buf[MAX_CH_NUM];
extern uint8_t UI_Select ;
uint8_t Rx_sleep_flag=1;  /* Synced at task startup; only main-loop/TMOS code accesses it. */
 uint8_t app_uart_rx_buffer[512] = {0};

// app_drv_fifo_t app_uart_tx_fifo;
 app_drv_fifo_t app_uart_rx_fifo;

#if DOWNLOAD_UART_DEBUG
static uint8_t download_rx_buffer[512];
static app_drv_fifo_t download_rx_fifo;
static uint16_t download_fifo_time;
static uint8_t download_fifo_pending;
static volatile uint8_t download_rx_fault;
volatile uint16_t uart1_line_err_cnt, uart1_rx_overflow_cnt;
#endif

static   uint8_t usartTaskId;
volatile uint8_t SW_SCAN_STATE  = 3; //0:不扫描   ;2:扫描名字 ;3:扫描数据;4:全扫描
// 串口发送缓冲区
#define MAX_TX_BUFFER_SIZE 256

uint32_t uart_to_ble_send_evt_cnt = 0;
uint16_t fifo_time_Cnt=0;

uint8_t  uart_rx_flag=0;
uint8_t for_uart_rx_black_hole = 0;
uint8_t fifo_len_flag=0;
uint8_t  TxBuff[200];
uint8_t  RxBuff[200];
uint8_t  tx_datbuf[8];
uint8_t  trigB;
static uint8_t tx_buffer[MAX_TX_BUFFER_SIZE];
volatile uint8_t Ble_cn_flg=0;
uint8_t num_cnt=0;

/* ================= 逐帧调试打印开关（功能清单 §6.3） =================
 * 旧代码在 app_uart_process() 里对**每一帧**都执行
 *     PRINT("pack_len == %d\r\n", pack_len);
 * 以及 parse_received_frame() 里每帧一条 menu_rank 打印。PRINT → printf → _write()
 * 是**阻塞式**串口发送（UART1 @115200 每字节 ≈ 87us），会让 TMOS 的其它任务
 * （含 BLE 协议栈）排队等待；正常工作时 STM32 会持续发帧（含每秒的 0x01 刷新），
 * 这两条打印属于纯开销。
 *
 *   USART3_DBG_FRAME_LOG == 0（默认）：两条逐帧打印在**预处理阶段**消失，
 *        不生成代码、不占 Flash 常量、不进热路径（与 observer.c 的
 *        SCAN_DBG_CH_LOG 完全同风格）。
 *   USART3_DBG_FRAME_LOG == 1：抓帧时恢复。
 * 注意：帧头/长度/SUM 不合法、命令字错误等**异常打印一律保留**，不受本开关影响。
 *
 * ★★ V1_0_2 现场问题修正（2026-09-23）：本开关曾被改成 1（连同 observer.c 的
 *    SCAN_DBG_CH_LOG），这就是"CH584M 处理过多数据就卡住"的直接原因，已改回 0。
 *    parse_received_frame() 由主循环的 app_uart_process() 逐帧调用，而 PRINT 是
 *    阻塞式发送（@115200 每字节 ≈87us）：每帧一两条日志就是 2~7ms 的纯阻塞。
 *    STM32 常态就会发 0x01 刷新帧，帧一多这些打印会把 Main_Circulation() 里的
 *    TMOS_SystemProcess() 推迟掉，BLE 扫描 / 上报 / 0x03 收齐判定一起变慢甚至
 *    看着像"卡死"。需要抓帧时再临时改成 1。
 * ==================================================================== */
#ifndef USART3_DBG_FRAME_LOG
#define USART3_DBG_FRAME_LOG   0
#endif

#if USART3_DBG_FRAME_LOG
#define FRAME_LOG(...)   PRINT(__VA_ARGS__)
#else
#define FRAME_LOG(...)   do { } while (0)
#endif

/* ======================================================================
 * ★★ V1_0_2 第二轮修正：把"每次收到/发出某条指令就打一行"的日志收进开关
 * ----------------------------------------------------------------------
 * PRINT 是**阻塞式** UART1 发送（@115200 每字节 ≈87us，一行 20~40 字节
 * 就是 1.7~3.5ms）。下面这些打印都落在**主循环**（app_uart_process ->
 * parse_received_frame，或 1 秒事件的发送函数）上，虽然不是中断，
 * 但每一次都会把 TMOS_SystemProcess() 往后推同样长的时间：
 *   · [CMD06] 每上报一轮就打一行 —— 一轮最快 1 秒一次（所有已绑定设备
 *     都送来新数据时），也就是每秒白白花掉 1.7~3.5ms；
 *   · [CMD03] hold / ignore —— 每收到一条 0x03 请求就打一行；
 *     STM32 万一连发 0x03，"ignore" 这条会跟着连打；
 *   · cmd03 rsp / [STORE] —— 每次应答 / 每次自动落盘各一行。
 * 这些信息对**现场定位问题**很有用，但对正常运行的 BLE 扫描/连接是纯负担，
 * 而且现在已经有 1 秒事件的 [BLE] adv 摘要可以看。所以：
 *     CMD_DBG_LOG == 0（默认）：编译期完全消失，零开销、不占 Flash；
 *     CMD_DBG_LOG == 1：临时打开抓流程。
 * 注意：真正的**异常**打印（帧头/长度/SUM 不合法、Flash 失败等）一律保留，
 * 不受本开关影响 —— 它们只在出错时出现，且能立刻指出问题。
 * ====================================================================== */
#ifndef CMD_DBG_LOG
#define CMD_DBG_LOG   0
#endif

#if CMD_DBG_LOG
#define CMD_LOG(...)   PRINT(__VA_ARGS__)
#else
#define CMD_LOG(...)   do { } while (0)
#endif

/* 0x06 主动上报（组包 + 阻塞发送），定义在本文件下方（见 build_sensor_data_response） */
void send_round_report(void);

/* 0x06「一轮采集完成主动上报」的统计（仅供调试观察，不参与协议逻辑） */
uint16_t cmd06_rpt_cnt  = 0;   /* 主动上报出去的 0x06 帧数 */
/* cmd 0x07 恢复出厂 的统计（同上，仅调试观察） */
uint16_t cmd07_rsp_cnt  = 0;   /* 成功回出的 0x07 应答帧数 */
uint16_t cmd07_drop_cnt = 0;   /* 被丢弃的 0x07 请求帧数 */
/* ★ 0x08「绑定统计」已按契约 §2.5 **整条删除**（case 0x08 + cmd08_rsp_cnt/cmd08_drop_cnt
 *   + 全部注释），STM32 侧与上位机同步删除；此处不再保留任何 0x08 相关符号。 */

/* cmd 0x03 应答端统计（仅供调试观察，不参与协议逻辑）
 *   ★ 契约 §2.6 改造后语义变更：0x03 收到请求时**不再立即回包**，
 *     所以"成功回出的应答帧数"与"受理的请求数"分两个计数器：
 *       cmd03_rsp_cnt    实际发出的 0x03 应答帧数（在 1 秒事件里发出）
 *       cmd03_hold_cnt   受理（进入等待）的 0x03 请求数
 *       cmd03_ignore_cnt 因已有请求在途而被忽略的 0x03 请求数
 *       cmd03_drop_cnt   长度/校验和不合法而被丢弃的 0x03 请求帧数 */
uint16_t cmd03_rsp_cnt  = 0;   /* 成功回出的 0x03 应答帧数 */
uint16_t cmd03_drop_cnt = 0;   /* 长度/校验和不合法而被丢弃的 0x03 请求帧数 */
uint16_t cmd03_hold_cnt = 0;   /* 受理并进入"等本轮收齐"的 0x03 请求数 */
uint16_t cmd03_ignore_cnt = 0; /* 在途期间被忽略的 0x03 请求数 */
/* cmd 0x04 解绑全部 / 0x05 保存绑定 的统计（同上，仅调试观察） */
uint16_t cmd04_rsp_cnt  = 0;   /* 成功回出的 0x04 应答帧数 */
uint16_t cmd04_drop_cnt = 0;   /* 被丢弃的 0x04 请求帧数 */
uint16_t cmd05_rsp_cnt  = 0;   /* 成功回出的 0x05 应答帧数 */
uint16_t cmd05_drop_cnt = 0;   /* 被丢弃的 0x05 请求帧数 */

/* V2 link liveness and local-data reporting are independent of screen power. */
#define CFG_LINK_HB_SEC 5u
#define CFG_REPORT_SEC  5u
static uint8_t s_link_hb_sec, s_rpt06_sec;
static uint8_t ch_com_has_valid(void)
{
    uint8_t i;

    for (i = 0u; i < (uint8_t)MAX_CH_NUM; i++)
    {
        if (CH_com_buf[i].valid && CH_com_buf[i].Type != TPYE_NONE)
        {
            return 1u;
        }
    }
    return 0u;
}
void send_link_heartbeat(void)
{
    uint8_t tx[16];
    uint8_t sum = 0x55u;
    uint8_t i, n = 0u;

    tx[n++] = 0xAAu; tx[n++] = 0xEEu;
    tx[n++] = 0x00u; tx[n++] = 0x05u;        /* LEN = 1 + len(DATA=4) */
    tx[n++] = 0x0Cu;                         /* CMD_NET_ACK = link heartbeat */
    tx[n++] = 0x01u;                         /* data[0] heartbeat tag */
    tx[n++] = (uint8_t)g_binding_count;
    tx[n++] = (uint8_t)(g_scan_enable ? 1u : 0u);
    tx[n++] = (uint8_t)(cmd06_rpt_cnt & 0xFFu);
    for (i = 4u; i < n; i++) { sum = (uint8_t)(sum + tx[i]); }
    tx[n++] = sum;
    tx[n++] = 0x0Au;
    protocol_send(tx, n);
    CMD_LOG("[LINK] hb sent (%u bytes) bind=%u scan=%u\r\n",
            (unsigned)n, (unsigned)g_binding_count, (unsigned)g_scan_enable);
}

static void u0_link_report_tick(void)
{
    uint8_t round_ready;
    if (++s_link_hb_sec >= CFG_LINK_HB_SEC)
    {
        s_link_hb_sec = 0u;
        send_link_heartbeat();
    }
    if (s_rpt06_sec < CFG_REPORT_SEC) ++s_rpt06_sec;
    round_ready = g_binding_count > 0u &&
        (observer_round_is_complete() ||
         (observer_round_has_new_data() &&
          observer_round_seconds_since_report() >= ROUND_REPORT_FALLBACK_SEC));
    if (round_ready || (s_rpt06_sec >= CFG_REPORT_SEC && ch_com_has_valid()))
    {
        send_round_report();
        observer_round_clear();
        s_rpt06_sec = 0u;
    }
}

extern u8g2_t u8g2;
extern device_info_t device_list[MAX_DEVICES];
/* ★ V1_0_9：提示页消息码（yuying_TFT.c 里定义）。用来判断"当前是否正在显示
   常驻提示"（软关机 / 正在关机中），从而在提示期间持续续期屏幕点亮。 */
extern volatile uint8_t g_ui_msg;

/* Display power is independent of UART/BLE processing. Parser only updates
 * policy; SPI commands and redraw requests are consumed in the 20 ms event. */
typedef struct
{
    Sensor_Tpye type;
    uint16_t page;
    uint8_t rank, menu, focus, subpage, message;
} ui_page_key_t;

static ui_page_key_t ui_current_page_key(void)
{
    ui_page_key_t key;
    key.type = Data_list1.Type;
    key.rank = Data_list1.menu_rank;
    key.menu = Data_list1.rank2_addr;
    key.focus = Data_list1.rank3_addr;
    key.page = key.rank == 1u ? Data_list1.UI_main.chu_num1 :
               ((key.rank == 2u || key.rank == 3u) ? Data_list1.UI_main.chu_num2 : 0u);
    key.subpage = (key.rank == 2u || key.rank == 3u) ? Data_list1.UI_main.re_flag : 0u;
    key.message = g_ui_msg;
    return key;
}

static uint8_t ui_page_changed(const ui_page_key_t *a, const ui_page_key_t *b)
{
    return a->type != b->type || a->rank != b->rank || a->menu != b->menu ||
           a->focus != b->focus || a->page != b->page || a->subpage != b->subpage ||
           a->message != b->message;
}

extern volatile uint16_t uart3_line_err_cnt;

static void ui_latency_report(void)
{
#if UI_LATENCY_DIAG
    uint32_t now = TMOS_GetSystemClock();
    if ((ui_diag_nav != ui_diag_last_report && !ui_redraw.navigation_pending) ||
        (uint32_t)(now - ui_diag_last_tick) >= MS1_TO_SYSTEM_TIME(5000))
    {
        ui_diag_last_tick = now;
        ui_diag_last_report = ui_diag_nav;
        PRINT("[UI] rx=%lu nav=%lu merge=%lu bad=%lu wait_max=%lu draw_max=%lu render_max=%lu transfer_max=%lu uarterr=%u pending=%u rank=%u off=%u sysclk=%lu spi_div=%u unit=625us\r\n",
              (unsigned long)ui_diag_rx, (unsigned long)ui_diag_nav,
              (unsigned long)ui_diag_merge, (unsigned long)ui_diag_bad,
              (unsigned long)ui_diag_wait_max, (unsigned long)ui_diag_draw_max,
              (unsigned long)ui_diag_render_max, (unsigned long)ui_diag_transfer_max,
              (unsigned int)uart3_line_err_cnt,
              (unsigned int)ui_redraw.navigation_pending,
              (unsigned int)Data_list1.menu_rank, (unsigned int)Rx_sleep_flag,
              (unsigned long)GetSysClock(), (unsigned int)R8_SPI0_CLOCK_DIV);
    }
#endif
}

void screen_reset_display_init(uint8_t reset_enabled, uint8_t reset_cause)
{
    /* S2 is hardware RST. No option-byte writes or GPIO key emulation. */
    screen_startup_init(&screen_startup, reset_enabled, reset_cause == RST_FLAG_MR);
    test_dirty = screen_startup.test;
    displayed_target = SCREEN_TARGET_NONE;
    restore_pending = 0u;
    UI42_InvalidateFull();
    Rx_sleep_flag = 1u; /* u8g2Init() leaves the panel off. */
    dis_flag_cnt = test_dirty;
    PRINT("[UI] reset_display_init: artwork=%u\r\n", (unsigned)test_dirty);
}

static uint8_t screen_display_is_off(void)
{
    uint8_t target = screen_startup_target(&screen_startup, g_ui_msg);
    if (target == SCREEN_TARGET_NONE) return 1u;
    if (target == SCREEN_TARGET_TEST || target == SCREEN_TARGET_MSG) return 0u;
    return screen_power_is_off(&screen_power, TMOS_GetSystemClock());
}

static uint8_t screen_power_process(void)
{
    uint8_t off = screen_display_is_off();
    if (off)
    {
        if (!Rx_sleep_flag) u8g2_SetPowerSave(&u8g2, 1u);
        Rx_sleep_flag = 1u;
    }
    else if (Rx_sleep_flag || displayed_target !=
             screen_startup_target(&screen_startup, g_ui_msg))
    {
        if (!restore_pending)
        {
            /* The 4.2 renderer caches row CRCs: every target restoration must
             * transfer a complete frame before display-on. */
            UI42_InvalidateFull();
        }
        dis_flag_cnt = restore_pending = 1u;
        g_ui_data_dirty = 1u;
    }
    return off;
}

/* ★★ 线路状态错误计数（只计数，绝不打印 —— ISR 里不能做阻塞式 PRINT） */
volatile uint16_t uart3_line_err_cnt = 0;
volatile uint16_t uart3_rx_overflow_cnt = 0;
static volatile uint8_t uart3_rx_fault;

/* UART3 is a separate producer. Once bytes are lost, stop assembling frames
 * until the main loop discards the damaged stream and resumes at a new frame. */
static void uart3_rx_enqueue(uint8_t *data, uint16_t len)
{
    if (len == 0u || uart3_rx_fault) return;
    if (app_drv_fifo_write_from_addr(&app_uart_rx_fifo, data, len) !=
        APP_DRV_FIFO_RESULT_SUCCESS)
    {
        uart3_rx_overflow_cnt++;
        uart3_rx_fault = 1u;
    }
}

static void uart3_rx_discard(void)
{
    PFIC_DisableIRQ(UART3_IRQn);
    app_uart_rx_fifo.begin = app_uart_rx_fifo.end;
    R8_UART3_FCR |= RB_FCR_RX_FIFO_CLR;
    uart3_rx_fault = 0u;
    fifo_time_Cnt = 0u;
    fifo_len_flag = 0u;
    PFIC_EnableIRQ(UART3_IRQn);
}

#if DOWNLOAD_UART_DEBUG
/* ISR only drains bytes. Parsing and all responses stay in the main loop. */
__INTERRUPT
__HIGH_CODE
void UART1_IRQHandler(void)
{
    uint8_t reason = UART1_GetITFlag();
    if (reason == UART_II_LINE_STAT)
    {
        uint8_t status = UART1_GetLinSTA();
        if (status & (RB_LSR_OVER_ERR | RB_LSR_PAR_ERR |
                      RB_LSR_FRAME_ERR | RB_LSR_BREAK_ERR))
        {
            uart1_line_err_cnt++;
            download_rx_fault = 1u;
        }
    }
    if (reason == UART_II_LINE_STAT || reason == UART_II_RECV_RDY ||
        reason == UART_II_RECV_TOUT)
    {
        /* Bound ISR work even when the host sends continuously. */
        for (uint8_t i = 0u; i < UART_FIFO_SIZE && R8_UART1_RFC; ++i)
        {
            uint8_t byte = R8_UART1_RBR;
            if (download_rx_fault) continue;
            /* Reserve one slot: the existing FIFO uses masked length. */
            if (app_drv_fifo_length(&download_rx_fifo) >=
                sizeof(download_rx_buffer) - 1u)
            {
                uart1_rx_overflow_cnt++;
                download_rx_fault = 1u;
                continue;
            }
            app_drv_fifo_write_from_addr(&download_rx_fifo, &byte, 1u);
        }
    }
}

static void download_rx_discard(void)
{
    PFIC_DisableIRQ(UART1_IRQn);
    download_rx_fifo.begin = download_rx_fifo.end;
    R8_UART1_FCR |= RB_FCR_RX_FIFO_CLR;
    download_rx_fault = 0u;
    download_fifo_time = 0u;
    download_fifo_pending = 0u;
    PFIC_EnableIRQ(UART1_IRQn);
}
#endif

__INTERRUPT
__HIGH_CODE
void UART3_IRQHandler(void)
{
    volatile uint16_t i=0;
    uint16_t error;
    uint8_t error1=55;

    switch(UART3_GetITFlag())
    {
        case UART_II_LINE_STAT: // 线路状态错误
        {
            /* Read LSR to acknowledge line status. On data loss, the main loop
             * discards hardware/software residual bytes before accepting frames. */
            uint8_t lsr = UART3_GetLinSTA();
            if (lsr & (RB_LSR_OVER_ERR | RB_LSR_PAR_ERR | RB_LSR_FRAME_ERR | RB_LSR_BREAK_ERR))
            {
                uart3_line_err_cnt++;
                uart3_rx_fault = 1u;
            }
            break;
        }

        case UART_II_RECV_RDY: // 数据达到设置触发点
        {
             /* Drain the hardware FIFO in one batch. Normal main-loop drawing
              * leaves IRQs active; overrun depends on IRQ latency/masking. */
             i = UART3_RecvString(TxBuff);
             if (i != 0U)
             {
                 uart3_rx_enqueue(&TxBuff[0], i);
             }
             break;
        }
        case UART_II_RECV_TOUT: // 接收超时，暂时一帧数据接收完成

             i = UART3_RecvString(TxBuff);
          //   UART1_SendString(TxBuff, i);//打印
             uart3_rx_enqueue(&TxBuff[0], i);
            // uart_rx_flag=1;
            break;

        case UART_II_THR_EMPTY: // 发送缓存区空，可继续发送
            break;

        case UART_II_MODEM_CHG: // 只支持串口0
            break;

        default:
            break;
    }
}

/* ======================================================================
 * app_uart_process ：主循环（Main_Circulation）里逐帧取包并解析。
 *
 * ★ V1_0_2 修正三处：
 *  1) 局部 `uint8_t packet[512] = {0};` 改成 **static** 并且不再逐个清零。
 *     它在 Main_Circulation() 的紧循环里被调用，每次调用都要把 512 字节
 *     全写一遍纯属浪费；而且 512 字节放在栈上也没必要（本函数单线程、
 *     不重入）。app_drv_fifo_read_pack() 只会填它自己返回的那 pack_len 个
 *     字节，解析侧也只读 pack_len 个字节，所以不需要预先清零。
 *  2) `if(pack_len<512)` 这个判断**来得太晚**：app_drv_fifo_read_pack()
 *     在它返回之前就已经把 frame_total_len 个字节 memcpy 进 packet 了，
 *     而那个函数原本允许 total_len 到 510（frame_total_len 最大 516）
 *     ⇒ 越界写 4 字节，判断放在后面根本拦不住。真正的长度收口已经改到
 *     app_drv_fifo.c 的 read_pack() 里（见那里的注释）。这里的判断保留，
 *     作为第二道防线（防 size 定义被改大）。
 *  3) `dis_flag_cnt++` 改成 `dis_flag_cnt = 1`（**合并重画请求**）。
 *     原来每解析一帧就 +1，而 START_DATA_EVT 每个事件只消费 1 次、
 *     每次消费都要跑一次**整屏重绘** UI_Control()。帧一多就堆积成一长串
 *     整屏重绘排队执行 ⇒ TMOS 被彻底占满，表现就是"数据一多 CH584M 卡住"。
 *     重画请求本来就只需要"有位"这一个信息，置 1 即可，绝不排队。
 *  4) 一次调用排空多个包（APP_UART_BURST_MAX 条）：原来一次只取一条，
 *     FIFO 里积压的帧要等下一次 Main_Circulation 才轮到，容易被
 *     1 秒事件里的"4 秒超时清空 FIFO"整片丢掉。
 * ====================================================================== */
#define APP_UART_BURST_MAX   4U
#define APP_UART_PACK_MAX    512U   /* 必须 >= app_uart_process 的目标缓冲 */

__HIGH_CODE
static void app_uart_process_fifo(app_drv_fifo_t *fifo,
                                  uint16_t *age, uint8_t *pending)
{
    static uint8_t packet[APP_UART_PACK_MAX];   /* static：见上面的说明 */
    uint16_t pack_len=0;
    uint16_t fifo_len=0;
    uint8_t  burst;
    app_drv_fifo_result_t x;

    for (burst = 0U; burst < APP_UART_BURST_MAX; burst++)
    {
        fifo_len=app_drv_fifo_length(fifo);
        if(fifo_len==0)
        {
            *age=0;
            *pending=0;
            break;                      /* 队列空了，本次就到这里 */
        }

        *pending=1;
        x=app_drv_fifo_read_pack(fifo, packet, &pack_len);
        /* The ISR may have reported a loss while read_pack copied bytes. */
        if (fifo == &app_uart_rx_fifo && uart3_rx_fault) break;
#if DOWNLOAD_UART_DEBUG
        /* An RX fault may arrive while read_pack copies a frame. */
        if (fifo == &download_rx_fifo && download_rx_fault) break;
#endif
        if(x!=APP_DRV_FIFO_RESULT_SUCCESS)
        {
            /* 半包 / 帧头还没到 / 校验不过：下一轮主循环再来 */
            break;
        }

        FRAME_LOG("pack_len == %d\r\n",pack_len);   /* 逐帧打印：默认编译期关掉 */
        *age=0;

        /* 第二道防线：read_pack() 已经保证 <= APP_UART_PACK_MAX，
           这里再挡一次，防止 packet[] 的尺寸将来被改小 */
        if(pack_len<APP_UART_PACK_MAX)
        {
            ui_page_key_t before = ui_current_page_key();
            uint32_t received_at = TMOS_GetSystemClock();
            uint8_t result = parse_received_frame(packet,pack_len,&Data_list1);
            if (result == 0u)
            {
                /* Sensor reads are a data transaction only. In particular,
                 * do not enqueue a display redraw or navigation on CMD03. */
                if (packet[4] == 0x03u) continue;
                ui_page_key_t after = ui_current_page_key();
#if UI_LATENCY_DIAG
                if (packet[4] == 0x01u) ui_diag_rx++;
#endif
                dis_flag_cnt = 1u; /* Coalesce background requests. */
                g_ui_data_dirty = 1u;   /* ★ 第二轮：真的来了新页面/新数据 */
                if (ui_page_changed(&before, &after))
                {
#if UI_LATENCY_DIAG
                    ui_diag_nav++;
                    if (ui_redraw.navigation_pending) ui_diag_merge++;
#endif
                    ui_redraw_navigation(&ui_redraw, received_at);
                    tmos_set_event(usartTaskId, START_DATA_EVT);
                }
            }
#if UI_LATENCY_DIAG
            else ui_diag_bad++;
#endif
        }
    }
}

__HIGH_CODE
void app_uart_process(void)
{
    if (uart3_rx_fault) uart3_rx_discard();
    app_uart_process_fifo(&app_uart_rx_fifo, &fifo_time_Cnt, &fifo_len_flag);
#if DOWNLOAD_UART_DEBUG
    if (download_rx_fault) download_rx_discard();
    app_uart_process_fifo(&download_rx_fifo, &download_fifo_time,
                          &download_fifo_pending);
#endif
}


void Usart3_Init(void)
{
    /* 配置串口3：先配置IO口模式，再配置串口 */

       //uart3 init
   // UART3_DefInit();
    GPIOA_SetBits(GPIO_Pin_5);
    GPIOA_ModeCfg(GPIO_Pin_5, GPIO_ModeOut_PP_5mA); // TXD-配置推挽输出，注意先让IO口输出高电平

    GPIOA_SetBits(GPIO_Pin_4);
    GPIOA_ModeCfg(GPIO_Pin_4, GPIO_ModeIN_PU);      // RXD-配置上拉输入

    UART3_DefInit();
    /* ★ 功能清单要求：两侧串口波特率统一为 115200。
     * 厂商的 UART3_DefInit() 内部写死了 UART3_BaudRateCfg(9600)
     * （见 StdPeriphDriver\CH58x_uart3.c:26），而 STM32 侧 USART1 是
     * 115200（Core\Src\usart.c:81）—— 两侧不一致会直接导致协议收不到。
     * 这里显式覆盖为 115200；只加一行调用，不改厂商驱动文件。 */
    UART3_BaudRateCfg(115200);
    UART3_ByteTrigCfg(UART_7BYTE_TRIG); /* Paired reference configuration. */
      trigB = 7;
    //enable interupt
    UART3_INTCfg(ENABLE, RB_IER_RECV_RDY | RB_IER_LINE_STAT);
    PFIC_EnableIRQ(UART3_IRQn);
#if DOWNLOAD_UART_DEBUG
    app_drv_fifo_init(&download_rx_fifo, download_rx_buffer,
                      sizeof(download_rx_buffer));
    UART1_BaudRateCfg(115200);
    UART1_ByteTrigCfg(UART_1BYTE_TRIG);
    R8_UART1_FCR |= RB_FCR_RX_FIFO_CLR;
    UART1_INTCfg(ENABLE, RB_IER_RECV_RDY | RB_IER_LINE_STAT);
    PFIC_EnableIRQ(UART1_IRQn);
#endif
}

__HIGH_CODE
uint16_t usart_ProcessEvent(uint8_t task_id, uint16_t events)
{
   tmos_event_hdr_t *test_message;
   if(events & SYS_EVENT_MSG) //系统事件 接受来自不同task的数据
     {
         uint8_t *pMsg;

         if((pMsg = tmos_msg_receive(usartTaskId)) != NULL)
         {
             tmos_msg_deallocate(pMsg);
         }

         return (events ^ SYS_EVENT_MSG);
     }
   if(events & START_IO_EVT)//ui显示
   {
       /* ★★ 契约 §2.2（2026-09-22）：广播回调异步化的**消费点**。
        *   BLE 回调 SACN_DATA() 只把广播包丢进环形队列（O(1)、零阻塞、
        *   零查表/解析/PRINT），真正的解析/绑定/填通道在这里按 FIFO 排空。
        *   每拍最多 16 条 ⇒ 容量 1600 包/秒（最坏 1 包/ms 的广播密度留 60% 余量）；
        *   单拍最坏 ≈ 16 × 50us = 0.8ms，占 10ms 的 8%。
        *   数据落地延迟 <= 10ms（对 1Hz 显示与 0x03 收齐判定无影响）。
        *   ⚠️ 与下面的 200ms 重画计数在同一个 10ms 事件内；**不新增更高频事件**。 */
       act_tick_10ms();
       observer_adv_drain(16);

       /* ★ 契约 §2.7：CH584M 本地内容（通道值/RSSI/电压/绑定数/告警数）也要实时刷新。
        *   改前只有收到 0x01 帧（app_uart_process 里 dis_flag_cnt++）才重画，
        *   因此本地采集到的内容最多 1 秒一次、且依赖 STM32 是否发帧。
        *
        *   做法：本 10ms tick 只累加计数，每 20 次（= 200ms）置一次重画请求
        *   （dis_flag_cnt = 1，由 START_DATA_EVT 既有的 UI_Control() 路径消费）。
        *   ⚠️ 绝不在本 tick 里直接全屏重绘（10ms 级重绘太耗，会拖垮 TMOS）。
        *
        *   ★ 周期修正说明：原代码 tmos_start_task(..., 6400) 按 TMOS 单位
        *     0.625ms 折算 = **4 秒**（并非原注释写的 10ms），无法凑出 200ms。
        *     现改为 UI_IO_TICK_TICKS(16) = 10ms，计数 20 次即 200ms。
        *     （1 秒事件用 1600 ⇒ 1600×0.625ms = 1s，同一单位制，已互相印证。） */
       if (++ui_refresh_tick >= UI_REFRESH_TICKS)
       {
           ui_refresh_tick = 0;
           /* Only request redraw with recent valid page data and display on.
              Background BLE parsing above continues with either gate closed. */
           if (ui_page_available() && !Rx_sleep_flag)
           {
               /* ★ 第二轮提速：改前这里是无条件 dis_flag_cnt = 1（5 帧/s）。
                  现在要求"真的有新数据"（g_ui_data_dirty），外加每 3 拍
                  （≈1.8s）一次的兜底心跳，保证没有 U0 帧时本地 BLE 数据也刷新。 */
               s_ui_dirty_miss++;
               if ((g_ui_data_dirty != 0u) || (s_ui_dirty_miss >= 3u))
               {
                   s_ui_dirty_miss = 0u;
                   dis_flag_cnt = 1;      /* 门开：请求一次重画（有变化 or 兜底心跳） */
               }
           }
       }

       tmos_start_task(usartTaskId, START_IO_EVT, UI_IO_TICK_TICKS);//10ms

       return (events ^ START_IO_EVT);
   }
   if(events & START_DATA_EVT) //数据事件
   {
     uint32_t next_ticks = MS1_TO_SYSTEM_TIME(20);
     uint8_t off = screen_power_process();
     uint8_t target = screen_startup_target(&screen_startup, g_ui_msg);
     /* Artwork/messages/restoration do not depend on baseboard telemetry.
      * The existing five-second limit still gates periodic page redraws. */
     if (off || (target == SCREEN_TARGET_PAGE && !restore_pending &&
                 g_frame_last_sec > UI_HOLD_SEC) ||
         (target == SCREEN_TARGET_TEST && !test_dirty && !restore_pending))
     {
        dis_flag_cnt = 0;              /* 门控关：清掉挂起请求（息屏期绝不落笔） */
        ui_redraw_cancel(&ui_redraw);
     }
     else if(dis_flag_cnt)
     {
        uint32_t now = TMOS_GetSystemClock();
        uint32_t wait = ui_redraw_wait(&ui_redraw, now);
        if (wait == 0u)
        {
#if UI_LATENCY_DIAG
           uint32_t start = now;
           if (ui_redraw.navigation_pending)
           {
               uint32_t waited = (uint32_t)(start - ui_redraw.navigation_since);
               if (waited > ui_diag_wait_max) ui_diag_wait_max = waited;
           }
#endif
           if (target == SCREEN_TARGET_TEST)
           {
               UI_TestArtwork(&u8g2);
               screen_startup_artwork_sent(&screen_startup);
           }
           else UI_Control(&Data_list1);
           if (Rx_sleep_flag) u8g2_SetPowerSave(&u8g2, 0u);
           Rx_sleep_flag = 0u;
           displayed_target = target;
           test_dirty = restore_pending = 0u;
           now = TMOS_GetSystemClock();
#if UI_LATENCY_DIAG
           if ((uint32_t)(now - start) > ui_diag_draw_max)
               ui_diag_draw_max = (uint32_t)(now - start);
           if (ui_profile_render_ticks > ui_diag_render_max)
               ui_diag_render_max = ui_profile_render_ticks;
           if (ui_profile_transfer_ticks > ui_diag_transfer_max)
               ui_diag_transfer_max = ui_profile_transfer_ticks;
#endif
           ui_redraw_complete(&ui_redraw, now);
           dis_flag_cnt = 0u;
           g_ui_data_dirty = 0u;      /* 本次重画已经把最新数据画上去了 */
        }
        else if (ui_redraw.navigation_pending && wait < next_ticks)
        {
           next_ticks = wait; /* Revisit at the navigation deadline. */
        }
     }
     tmos_start_task(usartTaskId, START_DATA_EVT, next_ticks);
     return (events ^ START_DATA_EVT);

   }
   if(events & START_TIMER_EVT) //1s事件
   {
       /* ★ 契约 §4.1：帧龄每秒 +1，饱和在 FRAME_AGE_NEVER（绝不回绕 ——
        *   回绕到 0 会让门控"凭空打开"）。 */
       if (g_frame_last_sec < FRAME_AGE_NEVER) g_frame_last_sec++;
        //PRINT("START_TIMER_EVT \r\n");
        if(fifo_len_flag==1)
        {
            fifo_time_Cnt++;
            if(fifo_time_Cnt>4) //解析队列超时，直接清空
            {
                PRINT("time out 4s clearing fifo all\r\n");
                uart3_rx_discard();
            }
        }
#if DOWNLOAD_UART_DEBUG
        if (download_fifo_pending && ++download_fifo_time > 4u)
            download_rx_discard();
#endif
        update_binding_mode();

        if (g_store_clear_req)
        {
            g_store_clear_req = 0;
            g_store_dirty     = 0;       /* 已经全部清掉，不要再自动存回去 */
            if (binding_store_clear() != BIND_STORE_STATUS_OK)
            {
                PRINT("[WARN] deferred cmd04/07: flash clear failed (ram already cleared)\r\n");
                ui_show_msg(UI_MSG_BIND_CLEAR_FAIL);
            }
            else
            {
                CMD_LOG("[STORE] deferred flash clear done\r\n");
                /* ★ 第六轮新增：清除结果也给页面提示（0x04 解绑 / 0x07 恢复出厂） */
                if (!s_act_lock && !s_act_cmd) ui_show_msg(UI_MSG_BIND_CLEAR_OK);
            }
            dis_flag_cnt = 1;            /* 1 秒事件里没有别的路径置这个位，显式请求重画 */
        }

        /* ★★★ V1_0_2 第六轮新增：保存/清除结果提示页的倒数。
         * 数到 0 就清掉提示并请求一次重画 —— 于是屏幕自动回到最新页面
         * （提示期间收到的普通页面帧已经把字段更新进 Data_list1 了）。 */
        /* ★ FIX(本轮)：原来这里的 if **少了函数体**，紧接着的那个裸块 { } 变成了
         *   "每秒无条件重画"：提示倒数到 0 的返回值被忽略，而且屏幕每秒被强制重画一次
         *   （400x300 全屏，占 CPU/总线，会挤占 BLE 与串口的时间）。
         *   现在把那个裸块收回成这个 if 的函数体：提示消失时重画一次。 */
        if (ui_msg_tick_sec() != 0U)
        {
            dis_flag_cnt = 1;        /* 提示倒数到 0：请求一次重画，回到最新页面 */
        }

        /* ★★★ V1_0_9：常驻提示（软关机 = "长按K1+K3开机"、正在关机中）显示期间
           持续续期屏幕点亮。
           原因：screen_power 策略是按 time_light 秒无"页面变化"就把屏幕熄掉，
           而常驻提示期间 STM32 根本不会发页面帧 —— 不续期的话提示页会在
           time_light 到点时被自己的省电策略熄灭，用户刚要长按开机屏幕就黑了。
           非常驻提示（保存结果等，2 秒）不需要续期，它们本来就会自动消失。 */
        if (g_ui_msg == UI_MSG_POWER_OFF || g_ui_msg == UI_MSG_SHUTTING_DOWN)
        {
            screen_power_wake(&screen_power, TMOS_GetSystemClock());
        }

        if (g_scan_mode == SCAN_MODE_BIND)
        {
            dis_flag_cnt = 1;
        }

        /* ★ 契约 §5：0x06「一轮采集完成主动上报」。
         *   时基用本 1 秒事件累加（不新开定时器）；发送点必须在这里
         *   （主循环/TMOS 上下文），**不能**放进 BLE 回调 SACN_DATA()——
         *   87 字节阻塞发送 @115200 ≈ 7.6ms，会推迟协议栈。
         *   触发条件（二者之一）：
         *     a) 本轮位图已覆盖全部已占用通道（= 一轮采集完成）；
         *     b) 兜底：距上次上报 > ROUND_REPORT_FALLBACK_SEC(10s) 且期间有新数据。
         *   上报后清零位图，开始下一轮。无任何绑定设备时不上报。 */
        observer_round_tick();
        u0_link_report_tick();
        /* ★ 绑定名变更监测（10 分钟巡检）的 1 秒时基：
         *   本处只提供"每秒一次"的心跳，累计满 600 次（10 分钟）
         *   在 observer.c 内执行一轮比对，并自动重新计时下一轮。 */
        name_change_monitor_tick();

        /* ★★ 契约 §2.3：绑定成功后的自动落盘。
         *   add_binding()（BLE 回调上下文）只置 g_store_dirty=1，真正的
         *   擦写 Flash 必须在这里 —— 1 秒事件 = 主循环/TMOS 上下文，
         *   擦除几十毫秒不会冻结 BLE 协议栈。
         *   0x05 指令保留，语义不变（手动保存仍可用）。 */
        if (g_store_dirty)
        {
            g_store_dirty = 0;              /* 先清标志：本秒内若又新增绑定会再次置 1 */
            binding_store_save();           /* 组镜像 → 擦除 → 写入 → 回读校验 */
            CMD_LOG("[STORE] auto save bindings after new bind\r\n");
        }

        /* ★★ 契约 §2.6：0x03「收齐本轮再应答」的每秒判定与发送。
         *   只在 1 秒事件（主循环/TMOS 上下文）推进秒计数、判定、发送 ——
         *   87 字节阻塞发送 @115200 ≈ 7.6ms，**绝不能在 BLE 回调里做**。
         *   判定式（observer_cmd03_ready）：
         *     收齐：所有 miss<3 的已绑定设备 fresh==1；
         *     超时：在途且距受理 >= RESP_TIMEOUT_SEC(30s) ⇒ 用旧数据应答。 */
        observer_cmd03_sec_tick();
        if (observer_cmd03_ready())
        {
            send_sensor_data_response();    /* 组 87 字节 0x03 应答并阻塞发送 */
            observer_cmd03_answered();      /* fresh→miss 归零 / 否则 +1 封顶 3；清 pending */
        }

        /* ★★ 契约 §2.3 + §2.1 补充（2026-09-22）：广播回调异步化的**可观测性**与
         *   **扫描看门狗**，都只在这一个 1 秒事件里做（主循环上下文，不在中断里）：
         *     1) 任一计数变化时打印一行摘要（明细日志 BLE_LOG 默认关闭时的补偿）：
         *        [BLE] adv: q_max=%u drop=%u scan rst=%u busy=%u err=%u | bind ok=%u reject=%u
         *     2) 连续 ADV_QUIET_RESTART_SEC(10) 秒没有任何广播包且 g_scan_enable=1 时
         *        重新 StartDiscovery —— 兜底 TGAP_DISC_SCAN=0 的语义未被真机实测的风险。
         *        返回码三分类：SUCCESS=真重启了 / 0x11(bleAlreadyInRequestedMode)=扫描
         *        本来就在跑（正常，会随现场无广播源每 10 秒 +1）/ 其它=异常。 */
        observer_ble_stat_tick();
        observer_ble_diag_tick();   /* ★ 第 9 轮：每 5 秒无条件打印扫描诊断 */
        ui_latency_report();        /* Diagnostic build only; at most once per second. */

    tmos_start_task(usartTaskId, START_TIMER_EVT, 1600);//10ms
    return (events ^ START_TIMER_EVT);
   }
   return 0;
}

__HIGH_CODE
void usart_task(void)
{
#if UI_LATENCY_DIAG
    PRINT("[UI-DIAG] UART1 115200 ready; timing v4; fast bitmap/box; render/transfer split; idle report every 5s\r\n");
#endif
    /* The first display event sends any local RST artwork before display-on. */

    usartTaskId = TMOS_ProcessEventRegister(usart_ProcessEvent);

    tmos_start_task(usartTaskId, START_IO_EVT, UI_IO_TICK_TICKS);

    tmos_set_event(usartTaskId, START_TIMER_EVT);   //立即开始

    tmos_set_event(usartTaskId, START_DATA_EVT);    //立即开始

}

__HIGH_CODE
void hex_p(uint8_t *data,uint16_t data_len)
{
  int i=0;

  for(i=0;i<data_len;i++)
  {
    PRINT ("0x%02X ",*(data+i));
  }
  PRINT (" \r\n");
}

/**
 * @brief 计算校验和（异或校验）
 * @param data 数据指针
 * @param len 数据长度
 * @return 校验和
 */

__HIGH_CODE
uint8_t calculate_checksum(uint8_t *data, uint16_t len) {
    uint8_t checksum = 0x55u;
    for (uint16_t i = 0; i < len; i++) {
        checksum += data[i];
    }
    return checksum;
}
/**
 * @brief 发送响应帧（标准协议格式，与 parse_received_frame() 同构）
 *
 * 帧格式： AA EE | LEN(2B 大端) | CMD | DATA | SUM | 0A
 *   LEN = 指令字节(1) + 数据字节(data_len)，大端序
 *   SUM = 0x55 + 从 CMD 起累加到 DATA 末字节，取低 8 位
 *   整帧长度 = LEN + 6 = data_len + 7
 *
 * ★ 修正记录（本次 0x03 应答端一并修正）：
 *   原实现用帧头 0xAA 0x55、帧尾 0x0D 0x0A，与接收解析器
 *   （要求 AA EE ... 0A，见本文件 parse_received_frame() 第 1 步）
 *   完全对不上，属于历史遗留的错误格式；此函数原先没有任何调用点。
 *   现改为契约规定的标准格式。注意：本函数是阻塞发送，
 *   只能在主循环 / TMOS 事件上下文调用，绝不能在中断里调用。
 *
 * @param response_cmd 响应指令
 * @param data         响应数据（可为 NULL，此时 data_len 必须为 0）
 * @param data_len     响应数据长度
 */
//__HIGH_CODE
static void protocol_send(uint8_t *data, uint16_t len)
{
    UART3_SendString(data, len);
#if DOWNLOAD_UART_DEBUG
    UART1_SendString(data, len);
#endif
}

void send_response_frame(uint8_t response_cmd, uint8_t *data, uint16_t data_len)
{
    uint16_t index = 0;
    uint8_t checksum=0;

    /* 缓冲区保护：整帧 = data_len + 7（帧头2 + 长度2 + CMD1 + 数据N + SUM1 + 尾1） */
    if ((uint32_t)data_len + 7 > MAX_TX_BUFFER_SIZE)
    {
        PRINT("[ERR] send_response_frame: data_len %d too large\r\n", data_len);
        return;
    }

    // 帧头
    tx_buffer[index++] = 0xAA;
    tx_buffer[index++] = 0xEE;

    // 长度（大端序存储）：指令(1) + 数据部分长度
    uint16_t total_data_len = 1 + data_len ;

    // 大端序存储：高位在前
    tx_buffer[index++] = (total_data_len >> 8) & 0xFF;  // 高字节
    tx_buffer[index++] = total_data_len & 0xFF;         // 低字节

    // 指令（响应指令）
    tx_buffer[index++] = response_cmd;

    // 数据部分
    if (data_len > 0 && data != NULL)
    {
        tmos_memcpy(&tx_buffer[index], data, data_len);
        index += data_len;
    }

    // 计算校验和（从指令开始到数据结束）
    checksum = calculate_checksum(&tx_buffer[4], total_data_len);
    tx_buffer[index++] = checksum;

    // 帧尾（单字节 0x0A，不再是 0x0D 0x0A）
    tx_buffer[index++] = 0x0A;
   // hex_p(tx_buffer, index);
    protocol_send(tx_buffer, index);
    //ble_notify_data(tx_buffer,index);
}

#define ACT_CMD_UNBIND      0x04u
#define ACT_CMD_BINDSAVE    0x05u
#define ACT_CMD_FACTORY     0x07u

/* 0x0D 载荷的动作码（与 U0 的 Usart_task.h 中 USART_ACT_xxx 一一对应） */
#define ACT_CODE_UNBIND     0x01u
#define ACT_CODE_BINDSAVE   0x02u
#define ACT_CODE_FACTORY    0x03u

/* 应答超时：30 x 10ms = 300ms。U0 的回包在收到请求后 ~1ms 就发出，
 * 300ms 已经是"丢帧 / 屏串口故障"的判据，而不是正常时延。 */
#define ACT_TIMEOUT_TICKS   30u
#define ACT_TIMEOUT_CLOCK   MS1_TO_SYSTEM_TIME(300)

/* 动作结论三档，与 0x04/0x05/0x07 的 status 口径一一对应 */
#define ACT_LEVEL_OK        0u
#define ACT_LEVEL_EMPTY     1u
#define ACT_LEVEL_FAIL      2u

uint16_t cmd0d_rsp_cnt       = 0;   /* 受理的 0x0D 触发数 */
uint16_t cmd0d_drop_cnt      = 0;   /* 被丢弃的 0x0D 帧数（长度/校验和/动作码非法）*/
uint16_t cmd_act_rsp_cnt     = 0;   /* 收到的 8 字节状态帧数（0x04/0x05/0x07 合计）*/
uint16_t cmd_act_timeout_cnt = 0;   /* 超时未收到状态帧的次数 */

/* 发 7 字节请求帧：AA EE 00 01 <cmd> <SUM> 0A （LEN = 1 = 只有 CMD，无 DATA） */
static void act_send_request(uint8_t cmd)
{
    uint8_t tx[8];

    tx[0] = 0xAAu;
    tx[1] = 0xEEu;
    tx[2] = 0x00u;                              /* LEN 高字节 */
    tx[3] = 0x01u;                              /* LEN 低字节 = 1(CMD) + 0(DATA) */
    tx[4] = cmd;
    tx[5] = calculate_checksum(&tx[4], 1u);     /* SUM = 0x55 + cmd */
    tx[6] = 0x0Au;
    protocol_send(tx, 7u);                   /* 7 字节 @115200 ≈ 0.61ms */
}

/* 远端 status -> 结论档位（0x04/0x07：00 有绑定已清 / 01 本来没有 / 其它 失败） */
static uint8_t act_level_from_status(uint8_t cmd, uint8_t status)
{
    if (cmd == ACT_CMD_BINDSAVE)
    {
        return (status == 0x00u) ? ACT_LEVEL_OK : ACT_LEVEL_FAIL;
    }
    if (status == 0x00u) return ACT_LEVEL_OK;
    if (status == 0x01u) return ACT_LEVEL_EMPTY;
    return ACT_LEVEL_FAIL;
}

/* 本地结论 × 远端结论：FAIL 最优先，其次"双方都没有"，否则取真正动过手的一方 */
static uint8_t act_combine(uint8_t local, uint8_t remote)
{
    if ((local == ACT_LEVEL_FAIL) || (remote == ACT_LEVEL_FAIL)) return ACT_LEVEL_FAIL;
    if (local == ACT_LEVEL_EMPTY) return remote;
    return local;
}

/* 结论 -> 提示码（与 U0 侧 UI_MSG_xxx 数值一致） */
static uint8_t act_msg_code(uint8_t cmd, uint8_t level)
{
    if (cmd == ACT_CMD_BINDSAVE)
    {
        if (level == ACT_LEVEL_FAIL)  return UI_MSG_BIND_SAVE_FAIL;
        if (level == ACT_LEVEL_EMPTY) return UI_MSG_BIND_SAVE_EMPTY;
        return UI_MSG_BIND_SAVE_OK;
    }
    if (level == ACT_LEVEL_FAIL)  return UI_MSG_BIND_CLEAR_FAIL;
    if (level == ACT_LEVEL_EMPTY) return UI_MSG_BIND_NONE;
    return UI_MSG_BIND_CLEAR_OK;
}

/* 本地那一半动作；返回结论档位 */
static uint8_t act_do_local(uint8_t cmd)
{
    if (cmd == ACT_CMD_BINDSAVE)
    {
        uint8_t st = binding_store_save();      /* 组镜像 -> 擦除 -> 写入 -> 回读校验 */
        if (st == BIND_STORE_STATUS_OK)    return ACT_LEVEL_OK;
        if (st == BIND_STORE_STATUS_EMPTY) return ACT_LEVEL_EMPTY;
        return ACT_LEVEL_FAIL;
    }

    /* 0x04 / 0x07：RAM 立即清（结论只依赖 RAM 里的 had），
       Flash 整块擦除交给 1 秒事件（见本文件头部 g_store_clear_req 的说明）。 */
    {
        uint8_t had = observer_clear_all_bindings();
        g_store_clear_req = 1;
        g_store_dirty     = 0;                  /* 已经全清，不要再自动存回去 */
        return had ? ACT_LEVEL_OK : ACT_LEVEL_EMPTY;
    }
}

/* 0x0D 触发：本地动作 + 发请求帧 + 起 300ms 超时 */
static void act_begin(uint8_t code)
{
    uint8_t cmd;

    if (code == ACT_CODE_UNBIND)         cmd = ACT_CMD_UNBIND;
    else if (code == ACT_CODE_BINDSAVE)  cmd = ACT_CMD_BINDSAVE;
    else if (code == ACT_CODE_FACTORY)   cmd = ACT_CMD_FACTORY;
    else
    {
        cmd0d_drop_cnt++;                       /* 动作码非法：丢弃，无副作用 */
        return;
    }

    /* 一个回合没走完之前不接受第二个触发（人手连点 / 长按重复） */
    if (s_act_cmd != 0u || s_act_lock != 0u)
    {
        CMD_LOG("[ACT] busy (0x%02X in flight), trigger code=%u ignored\r\n",
                s_act_cmd, code);
        return;
    }

    /* A pending erase must finish before any subsequent save/clear action. */
    if (g_store_clear_req) return;
    cmd0d_rsp_cnt++;
    s_binding_paused = 1u;
    g_scan_mode = SCAN_MODE_DATA;
    s_act_local = act_do_local(cmd);
    s_act_cmd   = cmd;
    s_act_since = TMOS_GetSystemClock();
    s_act_lock  = 1u;                            /* 从此刻起屏蔽 U0 的回显提示页 */
    act_send_request(cmd);
    CMD_LOG("[ACT] trigger code=%u -> req 0x%02X, local=%u\r\n",
            code, cmd, s_act_local);
}

/* 收到 8 字节状态帧：合成结论并画提示页 */
static void act_on_status(uint8_t cmd, uint8_t status)
{
    uint8_t level;

    if (s_act_cmd != cmd) return;
    cmd_act_rsp_cnt++;
    level = act_combine(s_act_local, act_level_from_status(cmd, status));
    /* U0 can save its own settings even with no BLE bindings on this MCU.
     * Its OK must not turn an empty local binding table into "saved". */
    if (cmd == ACT_CMD_BINDSAVE && s_act_local == ACT_LEVEL_EMPTY &&
        act_level_from_status(cmd, status) != ACT_LEVEL_FAIL)
        level = ACT_LEVEL_EMPTY;
    s_act_cmd = 0u;
    s_act_echo_pending = 1u;
    s_act_echo_since = TMOS_GetSystemClock();
    s_act_lock   = 1u;
    ui_show_msg(act_msg_code(cmd, level));
    /* 用户刚按了确认，结果必须**看得见**：顺手把屏幕点亮/续期
       （与 menu_rank=6 提示帧里的 screen_power_wake 同一处理）。 */
    screen_power_wake(&screen_power, TMOS_GetSystemClock());
    dis_flag_cnt = 1u;
    CMD_LOG("[ACT] rsp cmd=0x%02X status=%02X -> level=%u\r\n", cmd, status, level);
}

/* 10ms 事件：超时判定 + 释放提示页占用（绝不阻塞等待） */
static void act_tick_10ms(void)
{
    if (s_act_cmd != 0u &&
        (uint32_t)(TMOS_GetSystemClock() - s_act_since) >= ACT_TIMEOUT_CLOCK)
    {
        cmd_act_timeout_cnt++;
        s_act_cmd = 0u;
        s_act_lock = 1u;
        s_act_echo_pending = 1u;
        s_act_echo_since = TMOS_GetSystemClock();
        ui_show_msg(UI_MSG_OP_TIMEOUT);
        screen_power_wake(&screen_power, TMOS_GetSystemClock());
        dis_flag_cnt = 1u;
    }
    if (s_act_echo_pending &&
        (uint32_t)(TMOS_GetSystemClock() - s_act_echo_since) >=
            UI_MSG_HOLD_SEC * SCREEN_TICKS_PER_SEC)
        s_act_echo_pending = 0u;
    if (s_act_cmd == 0u && g_ui_msg == UI_MSG_NONE)
        s_act_lock = 0u;
}

/* ======================================================================
 *              cmd 0x03「传感器数据读取」应答端（CH584M → STM32）
 *              cmd 0x06「一轮采集完成主动上报」（CH584M → STM32）
 * 契约：D:\ai_work\协议_功能清单_页面码与协议冻结.md §2.1（0x03 规格见
 *       D:\ai_work\协议_0x03_传感器数据读取_规格冻结.md）
 *
 * 两帧**载荷完全同构**，只有命令字不同：
 *   0x03 应答： AA EE | 00 51 | 03 | <80 字节> | SUM | 0A   （请求-应答）
 *   0x06 上报： AA EE | 00 51 | 06 | <80 字节> | SUM | 0A   （主动，87 字节）
 *   LEN = 0x0051 = 81 = 1(CMD) + 80(DATA)
 *   SUM = 0x55 + 从 CMD(帧内下标 4) 累加到 DATA 末字节(下标 84)，取低 8 位
 *   80 字节 = 20 通道 × 4 字节：type(1B) | data(2B 大端) | voltage(1B)
 *   通道 i 位于帧内偏移 5 + i*4，i = 0..19
 *   ★ 因为 SUM 覆盖 CMD 字节，命令字不同则 SUM 天然不同，组包函数只需换 cmd。
 *
 * 类型字节直接透传 (uint8_t)Sensor_Tpye：
 *   ★ 依赖关系：CH584M 的 Sensor_Tpye（yuying_TFT.h）与 STM32 的 Data_tpye
 *     （LP_7_20_TOUCH）枚举数值已逐项核对完全一致（0=TPYE_NONE .. 13=TPYE_KK），
 *     故此处不做任何映射表，直接强转即可。若任一端枚举被改动，必须同步本处。
 * ==================================================================== */
#define SENSOR_RSP_CH_NUM       20                              /* 固定 20 通道，无"通道数"字段 */
#define SENSOR_RSP_DATA_LEN     (SENSOR_RSP_CH_NUM * 4)         /* 80 = 20 × 4 */
#define SENSOR_RSP_LEN_FIELD    (SENSOR_RSP_DATA_LEN + 1)       /* 81 = 0x0051 */
#define SENSOR_RSP_FRAME_LEN    (SENSOR_RSP_DATA_LEN + 7)       /* 87 = LEN + 6 */

/**
 * @brief 组 0x03 / 0x06 数据帧到 tx_buffer（只组包，不发送）
 * @param cmd 命令字：0x03 = 请求应答；0x06 = 一轮采集完成主动上报
 * @return 整帧字节数（固定 87）
 */
__HIGH_CODE
uint16_t build_sensor_data_response(uint8_t cmd)
{
    uint16_t idx = 0;
    uint8_t  i;

    /* 1. 帧头 */
    tx_buffer[idx++] = 0xAA;
    tx_buffer[idx++] = 0xEE;

    /* 2. 长度（大端序）：CMD(1) + 80 字节数据 = 81 = 0x0051 */
    tx_buffer[idx++] = (uint8_t)((SENSOR_RSP_LEN_FIELD >> 8) & 0xFF);  /* 0x00 */
    tx_buffer[idx++] = (uint8_t)(SENSOR_RSP_LEN_FIELD & 0xFF);         /* 0x51 */

    /* 3. 指令 */
    tx_buffer[idx++] = cmd;

    /* 4. 20 通道 × (type, data 高字节, data 低字节, voltage) */
    for (i = 0; i < SENSOR_RSP_CH_NUM; i++)
    {
        uint8_t  ch_type = (uint8_t)TPYE_NONE;
        uint16_t ch_data = 0;
        uint8_t  ch_vol  = 0;

        /* 未绑定 / 无数据的通道固定发 type=0x00, data=0x0000, voltage=0x00，
         * 与契约 §2.3 一致（STM32 侧据 type == T_None 判定该通道无效）。
         * 额外保证不变量：type 为 NONE 时 data/voltage 必为 0。 */
        if (CH_com_buf[i].valid && CH_com_buf[i].Type != TPYE_NONE)
        {
            ch_type = (uint8_t)CH_com_buf[i].Type;   /* 直接透传，枚举数值两端一致 */
            ch_data = CH_com_buf[i].CH_data;
            ch_vol  = CH_com_buf[i].voltage;
        }

        tx_buffer[idx++] = ch_type;
        tx_buffer[idx++] = (uint8_t)((ch_data >> 8) & 0xFF);  /* 大端：高字节在前 */
        tx_buffer[idx++] = (uint8_t)(ch_data & 0xFF);         /* 低字节 */
        tx_buffer[idx++] = ch_vol;
    }

    /* 5. 校验和：从 CMD（下标 4）累加到 DATA 末字节，取低 8 位 */
    tx_buffer[idx] = calculate_checksum(&tx_buffer[4], (uint16_t)(idx - 4));
    idx++;

    /* 6. 帧尾（单字节 0x0A） */
    tx_buffer[idx++] = 0x0A;

    return idx;   /* 固定 87 */
}

/**
 * @brief 组包并阻塞发送 0x03 应答帧（87 字节 @115200 ≈ 7.6 ms）
 * @note  ★ 契约 §2.6（2026-09-21）：本函数**只允许**由 START_TIMER_EVT（1 秒事件，
 *        主循环/TMOS 上下文）调用 —— 收到 0x03 请求时不再立即回包，
 *        要等"本轮收齐 或 30 秒超时"（observer_cmd03_ready）。
 *        绝不能在 BLE 回调 SACN_DATA() 里调用（7.6ms 阻塞会推迟协议栈）。
 */
__HIGH_CODE
void send_sensor_data_response(void)
{
    uint16_t len = build_sensor_data_response(0x03);
    protocol_send(tx_buffer, len);
    cmd03_rsp_cnt++;
    CMD_LOG("cmd03 rsp: %d bytes\r\n", len);   /* 收进 CMD_DBG_LOG（默认编译掉） */
}

/**
 * @brief 组包并阻塞发送 0x06「一轮采集完成主动上报」（87 字节，契约 §2.1）
 * @note  只允许由 Usart3_task.c 的 START_TIMER_EVT（1 秒事件）调用：
 *        该上下文属主循环/TMOS，阻塞式发送安全；**不得**在 BLE 广播回调
 *        SACN_DATA() 里调用（会推迟协议栈）。
 *        载荷与 0x03 应答完全同构，STM32 收到 0x06 后按 0x03 相同逻辑解析落库。
 */
__HIGH_CODE
void send_round_report(void)
{
    uint16_t len = build_sensor_data_response(0x06);
    protocol_send(tx_buffer, len);
    cmd06_rpt_cnt++;
    CMD_LOG("[CMD06] round report: %d bytes\r\n", len);   /* ★ 每轮上报一次，收进开关 */
}




/**
 * @brief 解析接收到的协议帧，将数据存入对应的结构体成员
 * @param rx_buffer  接收到的数据缓冲区（已包含帧头帧尾）
 * @param data_len   数据长度
 * @param pData      目标结构体指针
 * @return 0 成功，非0 失败
 */
__HIGH_CODE
uint8_t parse_received_frame(uint8_t *rx_buffer, uint16_t data_len, data_LIST *pData)
{
    if (rx_buffer == NULL || pData == NULL || data_len < 7u)
        return 1;
    // 1. 检查帧头帧尾
    if (rx_buffer[0] != 0xAA || rx_buffer[1] != 0xEE || rx_buffer[data_len-1] != 0x0A)
    {

        return 1;
    }

    // 2. 提取总长度并校验
    uint16_t total_len = (rx_buffer[2] << 8) | rx_buffer[3];
    if (total_len != data_len - 6)
    {

        return 2;
    }

    if (total_len < 1u ||
        calculate_checksum(&rx_buffer[4], total_len) != rx_buffer[4u + total_len])
        return 4;

    // 3. 解析固定字段
    uint8_t cmd = rx_buffer[4];
    /* ★ V1_0_2 修正：这里原来是无条件 PRINT（每帧一次、阻塞 2~7ms），
       已收进 FRAME_LOG —— 与 USART3_DBG_FRAME_LOG 同一个编译期开关，
       默认（0）在预处理阶段就消失，不进热路径。 */
    FRAME_LOG("cmd == %d\r\n",cmd);
    switch(cmd)
    {
      case 0x01:
        /* V2: CMD | rank | menu | focus | u16-LE params; no Type/count. */
        uint8_t menu_rank, rank2, rank3, param_cnt;
        uint16_t param_data[30];
        uint16_t param_bytes, idx;
        if (data_len < 10u) return 6;
        param_bytes = (uint16_t)(data_len - 10u);
        /* Check full-width count BEFORE narrowing: oversized frames cannot wrap. */
        if ((param_bytes & 1u) || param_bytes / 2u > 30u) return 6;
        param_cnt = (uint8_t)(param_bytes / 2u);
        menu_rank = rx_buffer[5];
        rank2 = rx_buffer[6];
        rank3 = rx_buffer[7];
        idx = 8u;
        for (uint8_t i = 0u; i < param_cnt; ++i)
        {
            param_data[i] = (uint16_t)(rx_buffer[idx] |
                ((uint16_t)rx_buffer[idx + 1u] << 8));
            idx += 2u;
        }

          /* Reject incomplete/unknown pages before changing page state or
             sleep policy. V2 home has ten status fields and twenty channels. */
          if (menu_rank == 1u)
          {
              if (param_cnt != 30u) return 5;
          }
          else if (menu_rank == 2u || menu_rank == 3u)
          {
              /* Each menu includes its data plus trailing page/re_flag. */
              /* V2 network: local/state/index + 3 addresses/counts/RSSI
               * magnitudes + trailing page/re_flag. */
          static const uint8_t min_params[8] = {4u, 14u, 16u, 23u, 8u, 5u, 10u, 2u};
              if (rank2 >= sizeof(min_params) || param_cnt < min_params[rank2]) return 5;
          }
          else if (menu_rank == 6u)
          {
              if (param_cnt < 1u) return 11;
          }
          else if (menu_rank != 4u && menu_rank != 5u)
          {
              return 10;
          }

          /* Rank 5 is the reference initialization state; rank 6 is a
             message and does not replace the cached page.
             menu_rank == 6 是"保存结果提示页"，它**不是页面状态**，
             所以绝不能写进 Data_list1 —— 否则提示消失后 UI_Control 会按 rank=6
             落到 default 分支，把屏幕画成一片空白（直到 STM32 再发一帧真实页面）。
             提示页只使用它的参数（msg 码），页面字段保持上一帧的值。 */
          /* Initialization and message commands must not replace the last
           * drawable page: repaint it after initialization if no new page
           * frame has arrived yet. */
          if (menu_rank != 6u && menu_rank != 5u)
          {
              pData->menu_rank = menu_rank;
              pData->rank2_addr = rank2;
              pData->rank3_addr = rank3;
          }
          FRAME_LOG("menu_rank=%d, rank2=%d, rank3=%d\n", menu_rank, rank2, rank3);

          // 7. 根据页面等级分发
          switch (menu_rank)
          {
              case 1:
              {
                  if (param_cnt < 10)
                      return 5;   // 十个固定参数
                  uint8_t pos = 0;
                  pData->UI_main.version       = param_data[pos++];
                  pData->UI_main.vbat          = param_data[pos++];
                  pData->UI_main.host_num      = param_data[pos++];
                  pData->UI_main.send_host_num = param_data[pos++];
                  pData->UI_main.sub_num       = param_data[pos++];
                  pData->UI_main.state         = (uint8_t)param_data[pos++];

                  pData->UI_main.Lora_rssi     = (uint8_t)param_data[pos++];

                  pData->UI_main.chu_num1      = param_data[pos++];
                  pData->UI_main.chu_num2      = param_data[pos++];
                  pData->UI_main.re_flag       = (uint8_t)param_data[pos++];
                 // PRINT("state == %d ;param_data[pos++] == %d \r\n",pData->UI_main.state,param_data[7]);
                  uint8_t data_cnt = 0;
                  while (pos < param_cnt && data_cnt < 20)
                  {
                      pData->UI_main.data[data_cnt++] = param_data[pos++];
                  }



              }
              break;
              case 2:
              case 3:
              {
                  /* ★ 契约 §2.3（2026-09-21 修正）：menu_rank==1 才发 UI_main 块，
                   *   menu_rank==2/3 时 STM32 在**参数块末尾**恒定追加 2 个 u16（小端）：
                   *     param_data[param_cnt-2] = chu_num2（二级子页页码 1/2）
                   *     param_data[param_cnt-1] = re_flag （二级子页选择）
                   *   "最后两个"这条规则对所有 rank2 统一成立，因此不会误读到真实参数；
                   *   必须放在任何 rank2 分派之前读取（下面各 case 的 if(param_cnt<N)
                   *   检查只增不减，依旧全部通过，旧解析逻辑无副作用）。 */
                  if (param_cnt >= 2)
                  {
                      pData->UI_main.chu_num2 = param_data[param_cnt - 2];
                      pData->UI_main.re_flag  = (uint8_t)param_data[param_cnt - 1];
                  }
                  UI_Select = rank2;   // 直接使用0~7索引
                  switch (rank2) {
                      case 0: {   // data_addr1
                          if (param_cnt < 2) return 5;
                          pData->Menu_rank1.set_host_num = param_data[0];
                          pData->Menu_rank1.set_sub_num  = param_data[1];
                          break;
                      }
                      case 1: {   // data_addr2（★ V1_0_10 新布局：三个地址一次全给）
                          if (param_cnt < 12) return 5;
                          pData->Menu_rank2.now_host_addr = param_data[0];
                          pData->Menu_rank2.net_flag      = param_data[1];
                          pData->Menu_rank2.cur_index     = param_data[2];
                          for (int i = 0; i < 3; i++) pData->Menu_rank2.addr[i] = param_data[3 + i];
                          for (int i = 0; i < 3; i++) pData->Menu_rank2.cnt[i]  = param_data[6 + i];
                          for (int i = 0; i < 3; i++) pData->Menu_rank2.rssi[i] = param_data[9 + i];
                          break;
                      }
                      case 2: {   // data_addr3
                          if (param_cnt < 14) return 5;
                          uint8_t pos = 0;
                          for (int i = 0; i < 3; i++) {
                              pData->Menu_rank3.biaoding_ad[i][0] = param_data[pos++];
                              pData->Menu_rank3.biaoding_ad[i][1] = param_data[pos++];
                          }
                          for (int i = 0; i < 4; i++) {
                              pData->Menu_rank3.pass_buf[i] = (uint8_t)(param_data[pos++] & 0xFF);
                          }
                          pData->Menu_rank3.pass_flag = param_data[pos++];
                          for (int i = 0; i < 3; i++) {
                              pData->Menu_rank3.biaoding_flag[i] = param_data[pos++];
                          }
                          break;
                      }
                      case 3: {   // data_addr4
                          if (param_cnt < 21) return 5;
                          for (int i = 0; i < 20; i++) {
                              pData->Menu_rank4.shishi_buf[i] = param_data[i];
                          }
                          pData->Menu_rank4.kaer_time = param_data[20];
                          break;
                      }
                      case 4: {   // data_addr5
                          if (param_cnt < 6) return 5;
                          /* U0 V2 sends edited addresses first, then the saved
                           * snapshot. Keep old/new semantics for all renderers. */
                          for (int i = 0; i < 3; i++) {
                              pData->Menu_rank5.new_send_addr[i] = param_data[i];
                          }
                          for (int i = 0; i < 3; i++) {
                              pData->Menu_rank5.old_send_addr[i] = param_data[3+i];
                          }
                          break;
                      }
                      case 5: {   // data_addr6
                          if (param_cnt < 3) return 5;
                          pData->Menu_rank6.power      = (int)param_data[0];
                          pData->Menu_rank6.time_light = param_data[1];
                          pData->Menu_rank6.state      = (uint8_t)param_data[2];
                          break;
                      }
                      case 6: {   // data_addr7
                          if (param_cnt < 8)
                              return 5;
                          uint8_t pos = 0;
                          for (int i = 0; i < 3; i++) {
                              pData->Menu_rank7.len_value[i] = param_data[pos++];
                          }
                          for (int i = 0; i < 3; i++) {
                              pData->Menu_rank7.ad_value[i] = param_data[pos++];
                          }
                          pData->Menu_rank7.old_len = param_data[pos++];
                          pData->Menu_rank7.new_len = param_data[pos++];
                          break;
                      }
                      case 7:     // V2 设备绑定动作页：仅公共 chu_num2/re_flag
                          break;
                      default:
                          return 5;   // 无效 rank2（0~7之外）
                  }
                  break;
              }
              case 4:
                  break;
              case 5:
                  /* Explicit visible startup, unlike background CMD03 reads.
                   * main() already initialized the panel. Reuse it: resetting
                   * it here would blank the display before the artwork.
                   * Keep the cached page and any persistent power message. */
                  screen_startup_request_artwork(&screen_startup);
                  test_dirty = 1u;
                  dis_flag_cnt = 1u;
                  ui_redraw_navigation(&ui_redraw, TMOS_GetSystemClock());
                  tmos_set_event(usartTaskId, START_DATA_EVT);
                  break;

              case 6:
                  /* ★★★ V1_0_2 第六轮新增：STM32 **参数保存结果**提示页
                   * ------------------------------------------------------------------
                   * STM32 侧把"参数存进它自己的 Data-EEPROM"的结果用这一帧下发：
                   *   AA EE | LEN(=8) | 01 | Type | 06 | rank2 | rank3 | 01 |
                   *   <msg 高字节> <msg 低字节> | SUM | 0A
                   * 参数只有 1 个 u16，取低字节 = UI_MSG_xxx（见 yuying_TFT.h）：
                   *   1 = 参数保存成功 / 2 = 参数保存失败
                   * 本机只把它画成提示页（UI_MSG_HOLD_SEC 秒后自动返回），
                   * **不修改任何页面/绑定状态**。页面的普通字段照常由同一帧的
                   * Type/menu_rank 之外的信息维护 —— 这一帧只用于提示。
                   * 校验和/帧长已在上层与 case 0x01 的公共段通过。 */
                  if (param_cnt < 1)
                  {
                      return 11;   /* 提示页必须带 1 个参数 */
                  }
                  /* Only the action echo is suppressed. Power messages and
                   * explicit msg=0 must still take effect during an action. */
                  if ((!s_act_lock && !s_act_echo_pending) || param_data[0] == UI_MSG_NONE ||
                      param_data[0] == UI_MSG_POWER_OFF || param_data[0] == UI_MSG_SHUTTING_DOWN)
                      ui_show_msg((uint8_t)(param_data[0] & 0xFFU));
                  if (s_act_cmd == 0u && s_act_echo_pending)
                      s_act_echo_pending = 0u;
                  if (g_ui_msg == UI_MSG_POWER_OFF || g_ui_msg == UI_MSG_SHUTTING_DOWN)
                      screen_startup.resume_intro_armed = 1u;
                  /* ★★★ V1_0_9：提示帧（menu_rank = 6）不参与屏幕点亮策略 ——
                     screen_power_note_page() 只在 rank 1..4 的页面帧里被调用，
                     提示帧既不点亮屏幕、也不刷新"最后活动时刻"。
                     于是"关机后按键唤醒只进提示页"时屏幕一直是黑的：
                     画面其实已经画进缓冲区，但面板没被点亮，用户什么都看不到
                     （现场表现："轻触任意键进入开机提示页，屏幕却不显示"），
                     一直要等 STM32 开机后发来普通页面帧才亮一下 —— 看上去
                     就是"提示页闪一下就被正常页顶掉"。
                     用户可见的提示必须点亮屏幕，这里补一次 screen_power_wake()。 */
                  if ((uint8_t)(param_data[0] & 0xFFU) != UI_MSG_NONE)
                  {
                      screen_power_wake(&screen_power, TMOS_GetSystemClock());
                  }
                  break;
              default:
                  return 10;   // 无效 menu_rank
          }
          /* ★ 第三轮（2026-10-25）：页面身份变了 -> 脏行缓存作废 + 整屏重画一次。
             U0 复位/唤醒后会强制发一帧主页（menu_rank=1）。屏端如果还留着上一页的
             行 CRC 缓存，就可能以为内容没变而一行都不发 -> 旧页面残留。
             这里在 (menu_rank, rank2, re_flag, 主页页码) 变化的瞬间作废缓存。
             事件很稀少（只在切页时），所以这里的一行 PRINT 不会成为常态负担。 */
          if (menu_rank >= 1u && menu_rank <= 4u)
          {
              uint16_t page_id = (uint16_t)(((uint16_t)menu_rank << 12) |
                                            ((uint16_t)(rank2 & 0x07u) << 8) |
                                            ((uint16_t)(pData->UI_main.re_flag & 0x0Fu) << 4) |
                                            (uint16_t)((menu_rank == 1u) ?
                                                       (pData->UI_main.chu_num1 & 0x0Fu) :
                                                       (pData->UI_main.chu_num2 & 0x0Fu)));

              if ((s_draw_page_valid == 0u) || (page_id != s_draw_page_id))
              {
                  s_draw_page_id    = page_id;
                  s_draw_page_valid = 1u;
                  UI42_InvalidateFull();
                  dis_flag_cnt = 1u;
                  PRINT("[UI] page %u/%u/re%u -> dirty cache invalidated, full repaint\r\n",
                        (unsigned)menu_rank, (unsigned)rank2,
                        (unsigned)pData->UI_main.re_flag);
              }
          }
          if (menu_rank >= 1u && menu_rank <= 4u)
          {
              uint32_t page_now = TMOS_GetSystemClock();
              uint8_t page_was_off = screen_startup.valid_page &&
                  !screen_startup.test && !screen_startup.intro_pending &&
                  !screen_startup.resume_intro_armed && g_ui_msg == UI_MSG_NONE &&
                  screen_power_is_off(&screen_power, page_now);
              g_frame_last_sec = 0u;
              /* First page or software boot: transmit the intro once even
               * when rank5 and this page arrive in the same UART burst. */
              if ((!screen_startup.resume_intro_armed && !screen_startup.valid_page &&
                   !screen_startup.test && !screen_startup.intro_pending) ||
                  (screen_startup.resume_intro_armed &&
                   g_ui_msg != UI_MSG_POWER_OFF && g_ui_msg != UI_MSG_SHUTTING_DOWN))
              {
                  screen_startup_request_artwork(&screen_startup);
                  test_dirty = 1u;
              }
              if (screen_startup.test)
                  screen_power_wake(&screen_power, page_now);
              screen_startup.test = 0u;
              screen_startup.valid_page = 1u;
              uint16_t page = (menu_rank == 1u) ? pData->UI_main.chu_num1 :
                              ((menu_rank == 2u || menu_rank == 3u) ?
                               pData->UI_main.chu_num2 : 0u);
              uint8_t subpage = (menu_rank == 2u || menu_rank == 3u) ?
                                pData->UI_main.re_flag : 0u;
              screen_power_note_page(&screen_power, page_now,
                                     menu_rank, rank2, rank3, page, subpage,
                                     pData->Menu_rank6.time_light);
              /* Identical telemetry cannot release an expired timeout. */
              if (page_was_off && !screen_power_is_off(&screen_power, page_now))
              {
                  screen_startup_request_artwork(&screen_startup);
                  screen_startup.test = 0u;
                  test_dirty = 1u;
              }
              update_binding_mode();
          }
        break;
        case 0x02:
            break;
        case 0x03:
        {
            /* ★ 新增：传感器数据读取请求（STM32 → CH584M）
             *   契约请求帧固定 7 字节：AA EE 00 01 03 58 0A
             *   （LEN = 0x0001，仅含 CMD，无数据部分）
             *
             *   此处沿用顶层已通过的校验：
             *     - 帧头 AA EE、帧尾 0A  → 已在第 1 步校验
             *     - total_len == data_len - 6 → 已在第 2 步校验
             *   再补一条 SUM 校验（从 CMD 起累加 total_len 个字节）：
             *     SUM 应位于 rx_buffer[4 + total_len]，即 7 字节请求帧的
             *     rx_buffer[5]；尾字节 rx_buffer[6] 已由第 1 步确认为 0x0A。
             *   校验失败则丢弃该帧，绝不回包、也绝不产生任何副作用。
             *
             *   ★★ 契约 §2.6（2026-09-21 核心改造）：**不再立即应答**。
             *     改前：这里直接 send_sensor_data_response() —— 用收到的
             *     （可能是上一轮的）旧数据回包，STM32 会读到过期值。
             *     改后：只在**主循环上下文**受理请求（observer_cmd03_request：
             *     清 g_dev_fresh[]、置 g_rsp_pending、记起始秒），
             *     应答帧由 1 秒事件在"本轮收齐 或 30 秒超时"时发送。
             *       - 已有请求在途 ⇒ 本次忽略（不重复入队、不回包）；
             *       - 本函数由主循环 app_uart_process() 调用，不在中断里。
             *
             *   ★ V1_0_2 第二轮修正：下面两条 [WARN] 原来是无条件 PRINT。
             *     0x03 是**唯一**由 STM32 自动、可能重复发送的指令，
             *     如果这条线上有噪声（帧头 AA EE 撞对但 SUM 不过），
             *     它就会"来一帧打一行"（每行 3.5ms 阻塞）把 TMOS 拖慢。
             *     所以收进 CMD_LOG（默认编译掉）；计数 cmd03_drop_cnt 照常累加，
             *     需要定位时把 CMD_DBG_LOG 改成 1 即可恢复完整报文级日志。
             *     （0x04/0x05/0x07 只由用户菜单一次性触发，保留无条件打印。）
             */
            if (total_len != 1)   /* 契约要求请求帧只含 CMD */
            {
                cmd03_drop_cnt++;
                CMD_LOG("[WARN] cmd03 bad LEN=%d, drop\r\n", total_len);
                break;
            }
            if (calculate_checksum(&rx_buffer[4], total_len) != rx_buffer[4 + total_len])
            {
                cmd03_drop_cnt++;
                CMD_LOG("[WARN] cmd03 bad SUM, drop\r\n");
                break;
            }

            if (observer_cmd03_request())
            {
                cmd03_hold_cnt++;
                /* ★ 收进 CMD_LOG：每受理一条 0x03 就打一行（默认编译掉） */
                CMD_LOG("[CMD03] hold: waiting for this round (or %d s timeout)\r\n",
                        RESP_TIMEOUT_SEC);
            }
            else
            {
                cmd03_ignore_cnt++;
                /* ★ 这条原来是**无条件** PRINT：STM32 万一连发 0x03，
                   它就会跟着连打（每行 3.5ms 阻塞）—— 收进 CMD_LOG。
                   计数 cmd03_ignore_cnt 仍然照常累加，可在 1 秒摘要里观察。 */
                CMD_LOG("[CMD03] ignore: a request is already in flight\r\n");
            }
            break;
        }
        case 0x04:
        {
            /* ★ 新增：解绑全部（STM32 → CH584M）
             *   契约：协议_0x04解绑_0x05保存_规格冻结.md §2
             *   请求帧固定 7 字节：AA EE 00 01 04 59 0A
             *                    （LEN = 0x0001，仅含 CMD，无数据部分）
             *   应答帧固定 8 字节：AA EE 00 02 04 <status> <SUM> 0A
             *                    SUM = 0x55 + 0x04 + status
             *
             *   校验方式与 0x03 **完全一致**：
             *     - 帧头 AA EE、帧尾 0A       → 已由第 1 步校验
             *     - total_len == data_len - 6 → 已由第 2 步校验
             *     - 这里再补：total_len == 1、SUM == calculate_checksum(&rx_buffer[4], total_len)
             *   任一不合法即丢弃：不回包、不产生任何副作用。
             *
             *   执行语义（解绑必须"彻底"）：
             *     1) observer_clear_all_bindings()：清 g_binding_list / g_binding_count /
             *        CH_com_buf[]（含 valid）/ g_name_snap[] / g_name_err_list[]（含条数）；
             *        清完 CH_com_buf[].valid 全 0 ⇒ 不再采集任何设备的通道数据。
             *     2) binding_store_clear()：**同时擦掉 Flash 里已保存的记录**，
             *        否则重启后又被 binding_store_load() 加载回来，解绑等于无效。
             *   status：00 = 成功（原绑定已清空）；01 = 当前本就没有绑定。
             *
             *   ★★ V1_0_2 第二轮修正：Flash 擦除**改到 1 秒事件执行**。
             *     binding_store_clear() 是 4096 字节整块擦除（20~35ms，最坏 ~70ms），
             *     而这条命令在线上只有 7 字节（0.61ms）。放在这里执行的话，
             *     命令一连发就能把 Flash 占满、把 TMOS/BLE 饿死，还伤 Flash 寿命。
             *     关键：**应答不需要等 Flash 结果** —— status 只看 RAM 里的 had，
             *     所以这里照原协议立即应答（协议一字不改），
             *     只把擦除动作置成请求，交给 1 秒事件（每个周期最多擦一次）。
             *     RAM 已经清干净，晚一点擦 Flash 不影响任何语义。 */
            uint8_t had;

            /* LEN=2 is the U0 status response, never another local action. */
            if (total_len == 2u)
            {
                act_on_status(0x04u, rx_buffer[5]);
                break;
            }

            if (total_len != 1)
            {
                cmd04_drop_cnt++;
                CMD_LOG("[WARN] cmd04 bad LEN=%d, drop\r\n", total_len);
                break;
            }
            if (calculate_checksum(&rx_buffer[4], total_len) != rx_buffer[4 + total_len])
            {
                cmd04_drop_cnt++;
                CMD_LOG("[WARN] cmd04 bad SUM, drop\r\n");
                break;
            }

            /* 1) 清 RAM：绑定表 + 通道表 + 名字快照 + 名字变更错误记录 */
            s_binding_paused = 1u;
            g_scan_mode = SCAN_MODE_DATA;
            had = observer_clear_all_bindings();

            /* 2) 清 Flash：**改成请求，由 1 秒事件执行**（见本 case 上方说明）。
             *    RAM 已经清干净，Flash 稍后擦掉即可；
             *    这样 0x04 连发时每个 1 秒周期最多擦一次，
             *    TMOS/BLE 不会被 20~70ms 的整块擦除占满。 */
            g_store_clear_req = 1;

            /* 3) 应答 8 字节（SUM = 0x55 + 0x04 + status，由 send_response_frame 计算）
             *    status 只看 RAM 的 had，与 Flash 擦除结果无关，所以可以立即回。 */
            {
                uint8_t status = had ? 0x00 : 0x01;
                send_response_frame(0x04, &status, 1);
                cmd04_rsp_cnt++;
                CMD_LOG("[CMD04] unbind all: had=%d status=%02X (flash clear queued)\r\n",
                        had, status);
            }
            break;
        }
        case 0x05:
        {
            /* ★ 新增：保存绑定到 Flash（STM32 → CH584M）
             *   契约：§3；请求帧固定 7 字节：AA EE 00 01 05 5A 0A
             *   应答帧固定 8 字节：AA EE 00 02 05 <status> <SUM> 0A，SUM = 0x55 + 0x05 + status
             *   status：00 保存成功（已写入并回读校验通过）/ 01 当前没有绑定 /
             *           02 写 Flash 失败或回读校验不一致
             *   校验方式与 0x03/0x04 完全一致。 */
            uint8_t st;

            /* LEN=2 is the U0 status response, never another local action. */
            if (total_len == 2u)
            {
                act_on_status(0x05u, rx_buffer[5]);
                break;
            }

            if (total_len != 1)
            {
                cmd05_drop_cnt++;
                CMD_LOG("[WARN] cmd05 bad LEN=%d, drop\r\n", total_len);
                break;
            }
            if (calculate_checksum(&rx_buffer[4], total_len) != rx_buffer[4 + total_len])
            {
                cmd05_drop_cnt++;
                CMD_LOG("[WARN] cmd05 bad SUM, drop\r\n");
                break;
            }

            /* ★ 0x05 **保持原地执行**：它的 status 里有"写 Flash 失败(02)"
             *   这一档，必须等写入+回读校验的结果才能应答，
             *   所以不能像 0x04/0x07 那样先回后擦。
             *   它只由用户在「设备绑定 → 保存设备」确认时触发（人手一次），
             *   不存在连发占满 Flash 的路径。 */
            if (g_store_clear_req)
            {
                st = BIND_STORE_STATUS_FAIL;
                send_response_frame(0x05, &st, 1);
                cmd05_rsp_cnt++;
                break;
            }
            s_binding_paused = 1u;
            g_scan_mode = SCAN_MODE_DATA;
            st = binding_store_save();       /* 组镜像 → 擦除 → 写入 → 回读校验 */
            send_response_frame(0x05, &st, 1);   /* ★ 先回包：STM32 在等这个 8 字节应答 */
            cmd05_rsp_cnt++;

            /* ★★★ V1_0_2 第六轮新增：把保存结果画到屏上
             * ------------------------------------------------------------------
             * 这就是"按键保存多次没有成功提醒"要补的东西：
             * 蓝牙绑定是存在 **CH584M 自己的 Data-Flash** 里的，所以结果只有本机
             * 知道，ST331 那边拿不到"到底写成功没有"，只能由本机自己提示。
             * 三种结果分别给三句提示：
             *   OK    -> "绑定保存成功"
             *   EMPTY -> "无绑定可保存"（当前没有绑定，无需写 Flash）
             *   FAIL  -> "绑定保存失败"（擦/写/回读校验任一步失败）
             * 提示页由 UI_Control() 优先绘制、2 秒后显示当前页面
             * （倒数是 1 秒事件里的 ui_msg_tick_sec()）。
             * 重画请求：本函数由 app_uart_process() 调用，解析成功后那里本来
             * 就会置 dis_flag_cnt = 1，所以这里不需要额外置位。 */
            if (st == BIND_STORE_STATUS_OK)         ui_show_msg(UI_MSG_BIND_SAVE_OK);
            else if (st == BIND_STORE_STATUS_EMPTY) ui_show_msg(UI_MSG_BIND_SAVE_EMPTY);
            else                                    ui_show_msg(UI_MSG_BIND_SAVE_FAIL);

            CMD_LOG("[CMD05] save bindings: status=%02X\r\n", st);
            break;
        }
        case 0x07:
        {
            /* ★ 新增：恢复出厂设置（STM32 → CH584M）
             *   契约：协议_功能清单_页面码与协议冻结.md §2.2
             *   请求帧固定 7 字节：AA EE 00 01 07 5C 0A
             *                    （LEN = 0x0001，仅含 CMD，无数据部分）
             *   应答帧固定 8 字节：AA EE 00 02 07 <status> <SUM> 0A
             *                    SUM = 0x07 + status（由 send_response_frame 计算）
             *
             *   校验方式与 0x03/0x04/0x05 **完全一致**：
             *     - 帧头 AA EE、帧尾 0A       → 已由第 1 步校验
             *     - total_len == data_len - 6 → 已由第 2 步校验
             *     - 这里再补：total_len == 1、SUM == calculate_checksum(&rx_buffer[4], total_len)
             *   任一不合法即丢弃：不回包、不产生任何副作用。
             *
             *   执行语义（与 0x04 解绑完全相同的两步，因为"恢复出厂"= 清掉
             *   全部绑定关系并落盘）：
             *     1) observer_clear_all_bindings()：清 g_binding_list / g_binding_count /
             *        CH_com_buf[]（含 valid）/ g_name_snap[] / g_name_err_list[]、
             *        电压异常计数、本轮采集位图、最近扫描名称缓存；
             *     2) binding_store_clear()：擦掉 Flash 里已保存的绑定记录，
             *        否则重启后又被 binding_store_load() 加载回来。
             *
             *   ★★ 严禁调用 Flash.c 的 factory_data_reset() / flash_all_zero()：
             *      那两个函数按**旧 Flash 布局**写（read_flash_buf[0]=ID 0x7A、
             *      写 200/400 字节），与当前绑定镜像（块 0、magic 'CHBD'、
             *      BIND_STORE_IMAGE_LEN = 8 + 40*32）不兼容，会破坏现有记录。
             *
             *   status：00 = 成功（清空前存在绑定）；01 = 当前本来就没有绑定。
             *   上下文：主循环 app_uart_process() 调用 → 阻塞擦写 Flash 安全。 */
            uint8_t had;

            /* LEN=2 is the U0 status response, never another local action. */
            if (total_len == 2u)
            {
                act_on_status(0x07u, rx_buffer[5]);
                break;
            }

            if (total_len != 1)
            {
                cmd07_drop_cnt++;
                PRINT("[WARN] cmd07 bad LEN=%d, drop\r\n", total_len);
                break;
            }
            if (calculate_checksum(&rx_buffer[4], total_len) != rx_buffer[4 + total_len])
            {
                cmd07_drop_cnt++;
                PRINT("[WARN] cmd07 bad SUM, drop\r\n");
                break;
            }

            s_binding_paused = 1u;
            g_scan_mode = SCAN_MODE_DATA;
            had = observer_clear_all_bindings();
            g_store_clear_req = 1u;
            g_store_dirty = 0u;

            {
                uint8_t status = had ? 0x00 : 0x01;
                send_response_frame(0x07, &status, 1);
                cmd07_rsp_cnt++;
                PRINT("[CMD07] factory reset: had=%d status=%02X\r\n", had, status);
            }
            break;
        }
        case 0x0D:
            if (total_len != 2u) return 6;
            act_begin(rx_buffer[5]);
            break;
        /* ★ 契约 §2.5：0x08「绑定统计」应答**整条删除**（含 case、注释与
         *   cmd08_rsp_cnt/cmd08_drop_cnt 计数器），STM32 侧与上位机同步删除。
         *   0x08 请求帧落到下面的 default ⇒ 静默忽略（不回包、无副作用）。 */
        default:
            /* 契约 §6.2：未知 CMD 静默忽略（不回包、无副作用） */
            break;
    }
    return 0;
}
