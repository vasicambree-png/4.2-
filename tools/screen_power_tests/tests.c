#include <stdint.h>
#include <stdio.h>
#include "screen_power.h"

static unsigned checks;
static unsigned failures;

#define CHECK(expression) do { \
    ++checks; \
    if (!(expression)) { \
        ++failures; \
        printf("FAIL line %u: %s\n", (unsigned)__LINE__, #expression); \
    } \
} while (0)

static screen_power_t visible(uint32_t now, uint16_t seconds)
{
    screen_power_t s = {0};
    screen_power_note_page(&s, now, 1u, 0u, 0u, 1u, 0u, seconds);
    return s;
}

static void startup_and_first_frame(void)
{
    screen_power_t s = {0};
    CHECK(screen_power_is_off(&s, 0u));
    CHECK(screen_power_is_off(&s, UINT32_MAX));
    screen_power_note_page(&s, 100u, 1u, 0u, 0u, 1u, 0u, 30u);
    CHECK(!screen_power_is_off(&s, 100u));
    CHECK(!screen_power_is_off(&s, 100u + 30u * SCREEN_TICKS_PER_SEC - 1u));
    CHECK(screen_power_is_off(&s, 100u + 30u * SCREEN_TICKS_PER_SEC));
}

static void always_on(void)
{
    screen_power_t s = visible(100u, 0u);
    CHECK(!screen_power_is_off(&s, 100u));
    CHECK(!screen_power_is_off(&s, 100u + 3600u * SCREEN_TICKS_PER_SEC));
    CHECK(!screen_power_is_off(&s, UINT32_MAX));
    CHECK(!screen_power_is_off(&s, 0u));
}

static void exact_deadline(void)
{
    static const uint16_t seconds[] = {1u, 5u, 30u, 60u, 65535u};
    unsigned i;
    for (i = 0u; i < sizeof(seconds) / sizeof(seconds[0]); ++i)
    {
        uint32_t start = 1234u;
        uint32_t elapsed = (uint32_t)seconds[i] * SCREEN_TICKS_PER_SEC;
        screen_power_t s = visible(start, seconds[i]);
        CHECK(!screen_power_is_off(&s, start + elapsed - 1u));
        CHECK(screen_power_is_off(&s, start + elapsed));
        CHECK(screen_power_is_off(&s, start + elapsed + 1u));
    }
}

static void repeated_frames_do_not_extend_deadline(void)
{
    screen_power_t s = visible(0u, 5u);
    uint32_t now;
    for (now = SCREEN_TICKS_PER_SEC; now <= 10u * SCREEN_TICKS_PER_SEC;
         now += SCREEN_TICKS_PER_SEC)
    {
        screen_power_note_page(&s, now, 1u, 0u, 0u, 1u, 0u, 5u);
        CHECK(s.last_activity == 0u);
        CHECK(screen_power_is_off(&s, now) == (now >= 5u * SCREEN_TICKS_PER_SEC));
    }
}

static void navigation_changes_wake(void)
{
    unsigned kind;
    for (kind = 0u; kind < 5u; ++kind)
    {
        screen_power_t s = visible(0u, 1u);
        uint8_t rank = 1u, menu = 0u, focus = 0u, subpage = 0u;
        uint16_t page = 1u;
        uint32_t activity = 2u * SCREEN_TICKS_PER_SEC;
        CHECK(screen_power_is_off(&s, activity));
        switch (kind)
        {
            case 0u: rank = 2u; break;
            case 1u: menu = 1u; break;
            case 2u: focus = 1u; break;
            case 3u: page = 2u; break;
            case 4u: subpage = 1u; break;
        }
        screen_power_note_page(&s, activity, rank, menu, focus, page, subpage, 1u);
        CHECK(!screen_power_is_off(&s, activity));
        CHECK(!screen_power_is_off(&s, activity + SCREEN_TICKS_PER_SEC - 1u));
        CHECK(screen_power_is_off(&s, activity + SCREEN_TICKS_PER_SEC));
        screen_power_note_page(&s, activity + SCREEN_TICKS_PER_SEC,
                               rank, menu, focus, page, subpage, 1u);
        CHECK(screen_power_is_off(&s, activity + SCREEN_TICKS_PER_SEC));
    }
}

static void setting_changes_restart_timer(void)
{
    screen_power_t s = visible(0u, 1u);
    uint32_t activity = 2u * SCREEN_TICKS_PER_SEC;
    CHECK(screen_power_is_off(&s, activity));
    screen_power_note_page(&s, activity, 1u, 0u, 0u, 1u, 0u, 30u);
    CHECK(!screen_power_is_off(&s, activity));
    CHECK(!screen_power_is_off(&s, activity + 30u * SCREEN_TICKS_PER_SEC - 1u));
    CHECK(screen_power_is_off(&s, activity + 30u * SCREEN_TICKS_PER_SEC));
    activity += 31u * SCREEN_TICKS_PER_SEC;
    screen_power_note_page(&s, activity, 1u, 0u, 0u, 1u, 0u, 0u);
    CHECK(!screen_power_is_off(&s, activity + 1000u * SCREEN_TICKS_PER_SEC));
    activity += 1000u * SCREEN_TICKS_PER_SEC;
    screen_power_note_page(&s, activity, 1u, 0u, 0u, 1u, 0u, 1u);
    CHECK(!screen_power_is_off(&s, activity + SCREEN_TICKS_PER_SEC - 1u));
    CHECK(screen_power_is_off(&s, activity + SCREEN_TICKS_PER_SEC));
}

static void explicit_wake_on_unchanged_page(void)
{
    screen_power_t s = visible(0u, 1u);
    uint32_t activity = 2u * SCREEN_TICKS_PER_SEC;
    CHECK(screen_power_is_off(&s, activity));
    screen_power_wake(&s, activity);
    CHECK(!screen_power_is_off(&s, activity));
    CHECK(s.timeout_sec == 1u && s.rank == 1u && s.page == 1u);
    screen_power_note_page(&s, activity + 100u, 1u, 0u, 0u, 1u, 0u, 1u);
    CHECK(!screen_power_is_off(&s, activity + SCREEN_TICKS_PER_SEC - 1u));
    CHECK(screen_power_is_off(&s, activity + SCREEN_TICKS_PER_SEC));
}

static void wrapping_clock(void)
{
    uint32_t start = UINT32_MAX - 500u;
    screen_power_t s = visible(start, 1u);
    CHECK(!screen_power_is_off(&s, start + SCREEN_TICKS_PER_SEC - 1u));
    CHECK(screen_power_is_off(&s, start + SCREEN_TICKS_PER_SEC));
    screen_power_wake(&s, 2000u);
    CHECK(!screen_power_is_off(&s, 2000u + SCREEN_TICKS_PER_SEC - 1u));
    CHECK(screen_power_is_off(&s, 2000u + SCREEN_TICKS_PER_SEC));
    s = visible(start, 65535u);
    CHECK(!screen_power_is_off(&s, start + 65535u * SCREEN_TICKS_PER_SEC - 1u));
    CHECK(screen_power_is_off(&s, start + 65535u * SCREEN_TICKS_PER_SEC));
}

static void expired_stays_off_after_full_clock_cycle(void)
{
    uint32_t start = 1234u;
    uint64_t full_cycle = UINT64_C(1) << 32;
    uint32_t wrapped_now = (uint32_t)((uint64_t)start + full_cycle + 100u);
    screen_power_t s = visible(start, 1u);
    CHECK(s.expired == 0u);
    CHECK(screen_power_is_off(&s, start + SCREEN_TICKS_PER_SEC));
    CHECK(s.expired == 1u);
    CHECK(screen_power_is_off(&s, UINT32_MAX));
    CHECK(screen_power_is_off(&s, 0u));
    CHECK(screen_power_is_off(&s, wrapped_now));
    screen_power_note_page(&s, wrapped_now, 1u, 0u, 0u, 1u, 0u, 1u);
    CHECK(s.expired == 1u);
    CHECK(s.last_activity == start);
    CHECK(screen_power_is_off(&s, wrapped_now));
    screen_power_note_page(&s, wrapped_now, 1u, 0u, 1u, 1u, 0u, 1u);
    CHECK(s.expired == 0u);
    CHECK(!screen_power_is_off(&s, wrapped_now));
    CHECK(screen_power_is_off(&s, wrapped_now + SCREEN_TICKS_PER_SEC));
    screen_power_wake(&s, wrapped_now + 2u * SCREEN_TICKS_PER_SEC);
    CHECK(s.expired == 0u);
    CHECK(!screen_power_is_off(&s, wrapped_now + 2u * SCREEN_TICKS_PER_SEC));
}

static void third_level_stays_on_and_exit_restarts_timer(void)
{
    uint8_t menu, exit_rank;
    for (menu = 0u; menu < 8u; ++menu)
    {
        screen_power_t s = visible(0u, 1u);
        uint32_t entered = 2u * SCREEN_TICKS_PER_SEC;
        CHECK(screen_power_is_off(&s, entered));
        screen_power_note_page(&s, entered, 3u, menu, 1u, 1u, 0u, 1u);
        CHECK(!screen_power_is_off(&s, entered));
        CHECK(!screen_power_is_off(&s, entered + SCREEN_TICKS_PER_SEC));
        CHECK(!screen_power_is_off(&s, UINT32_MAX));
        CHECK(!screen_power_is_off(&s, 0u));
        CHECK(s.expired == 0u);
        screen_power_note_page(&s, 100u, 3u, menu, 1u, 1u, 0u, 1u);
        CHECK(s.last_activity == entered);
        CHECK(!screen_power_is_off(&s, 100u + SCREEN_TICKS_PER_SEC));
    }
    for (exit_rank = 1u; exit_rank <= 4u; ++exit_rank)
    {
        screen_power_t s = visible(0u, 5u);
        uint32_t exited = 3600u * SCREEN_TICKS_PER_SEC;
        if (exit_rank == 3u) continue;
        screen_power_note_page(&s, 100u, 3u, 5u, 1u, 1u, 0u, 5u);
        CHECK(!screen_power_is_off(&s, exited));
        screen_power_note_page(&s, exited, exit_rank, 5u, 1u, 1u, 0u, 5u);
        CHECK(s.last_activity == exited && s.expired == 0u);
        CHECK(!screen_power_is_off(&s, exited + 5u * SCREEN_TICKS_PER_SEC - 1u));
        CHECK(screen_power_is_off(&s, exited + 5u * SCREEN_TICKS_PER_SEC));
    }
}

int main(void)
{
    startup_and_first_frame();
    always_on();
    exact_deadline();
    repeated_frames_do_not_extend_deadline();
    navigation_changes_wake();
    setting_changes_restart_timer();
    explicit_wake_on_unchanged_page();
    wrapping_clock();
    expired_stays_off_after_full_clock_cycle();
    third_level_stays_on_and_exit_restarts_timer();
    printf("Screen power policy: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
