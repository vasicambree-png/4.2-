/* Hardware/library boundaries only. Parser, task and power policy are production code. */
#define __HIGH_CODE
#define PRINT(...) ((void)0)
#define SYS_EVENT_MSG 0x8000u
#define MAX_CH_NUM 20
#define MAX_DEVICES 40
#define SCAN_MODE_BIND 1
#define SCAN_MODE_DATA 2
#define ROUND_REPORT_FALLBACK_SEC 10
#define RESP_TIMEOUT_SEC 30
#define BIND_STORE_STATUS_OK 0
#define BIND_STORE_STATUS_EMPTY 1
typedef struct { int unused; } device_t;
typedef struct { int unused; } device_info_t;
typedef struct { int unused; } u8g2_t;
typedef struct { int unused; } app_drv_fifo_t;
typedef struct { int unused; } tmos_event_hdr_t;
typedef int app_drv_fifo_result_t;
#define APP_DRV_FIFO_RESULT_SUCCESS 0
static uint32_t mock_clock;
static uint8_t mock_save_status, last_message, last_response_cmd, last_response_status;
static uint16_t last_response_len;
static unsigned draws, off_calls, on_calls, drains, round_ticks, report_calls;
static unsigned cmd03_requests, cmd03_ticks, cmd03_answers, response_calls;
static unsigned init_calls, saves, clears, ble_ticks, name_ticks;
static unsigned mock_round_complete, mock_cmd03_ready;
static uint8_t g_scan_mode, g_binding_count, g_store_dirty;
data_LIST Data_list1, last_drawn;
device_t CH_com_buf[MAX_CH_NUM];
device_info_t device_list[MAX_DEVICES];
uint8_t UI_Select;
u8g2_t u8g2;
static uint8_t queued[8][512];
static uint16_t queued_len[8];
static unsigned queue_head, queue_tail;
uint8_t parse_received_frame(uint8_t *, uint16_t, data_LIST *);
static uint32_t TMOS_GetSystemClock(void) { return mock_clock; }
static void u8g2_SetPowerSave(u8g2_t *s, uint8_t off)
{ (void)s; if (off) off_calls++; else on_calls++; }
static void UI_Control(data_LIST *s) { draws++; last_drawn = *s; }
static void *tmos_msg_receive(uint8_t id) { (void)id; return NULL; }
static void tmos_msg_deallocate(void *p) { (void)p; }
static void tmos_start_task(uint8_t id, uint16_t evt, uint32_t ticks)
{ (void)id; (void)evt; (void)ticks; }
static void observer_adv_drain(unsigned n) { (void)n; drains++; }
static void u8g2Init(u8g2_t *s) { (void)s; init_calls++; }
static uint8_t binding_store_clear(void) { clears++; return BIND_STORE_STATUS_OK; }
static uint8_t binding_store_save(void) { saves++; return mock_save_status; }
static void ui_show_msg(uint8_t msg) { last_message = msg; host_real_ui_show_msg(msg); }
static void observer_round_tick(void) { round_ticks++; }
static uint8_t observer_round_is_complete(void) { return mock_round_complete; }
static uint8_t observer_round_has_new_data(void) { return 0; }
static unsigned observer_round_seconds_since_report(void) { return 0; }
static void observer_round_clear(void) { mock_round_complete = 0; }
void send_round_report(void) { report_calls++; }
static void name_change_monitor_tick(void) { name_ticks++; }
static void observer_cmd03_sec_tick(void) { cmd03_ticks++; }
static uint8_t observer_cmd03_ready(void) { return mock_cmd03_ready; }
static void send_sensor_data_response(void) { cmd03_answers++; }
static void observer_cmd03_answered(void) { mock_cmd03_ready = 0; }
static void observer_ble_stat_tick(void) { ble_ticks++; }
static void observer_ble_diag_tick(void) { }
static uint8_t observer_cmd03_request(void) { cmd03_requests++; return 1; }
static uint8_t mock_clear_had = 1;
static unsigned ram_clear_calls;
static uint8_t observer_clear_all_bindings(void)
{ ram_clear_calls++; g_binding_count = 0; return mock_clear_had; }
static void send_response_frame(uint8_t cmd, uint8_t *data, uint16_t len)
{ last_response_cmd = cmd; last_response_len = len;
  last_response_status = len ? data[0] : 0; response_calls++; }
static uint16_t app_drv_fifo_length(app_drv_fifo_t *fifo)
{ (void)fifo; return queue_head == queue_tail ? 0 : queued_len[queue_head]; }
static app_drv_fifo_result_t app_drv_fifo_read_pack(app_drv_fifo_t *fifo,
                                                  uint8_t *out, uint16_t *len)
{
    (void)fifo;
    if (queue_head == queue_tail) return 1;
    *len = queued_len[queue_head];
    memcpy(out, queued[queue_head], *len);
    queue_head++;
    return APP_DRV_FIFO_RESULT_SUCCESS;
}
static void app_drv_fifo_flush(app_drv_fifo_t *fifo)
{ (void)fifo; queue_head = queue_tail; }
