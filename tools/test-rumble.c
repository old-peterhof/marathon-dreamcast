/* Host regression of the real rumble scheduler; only Maple/time are mocked.
 * cc -std=c11 -Wall -Wextra -Werror tools/test-rumble.c -o /tmp/test-rumble
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define MAPLE_FUNC_CONTROLLER 1
#define MAPLE_FUNC_PURUPURU 2
#define MAPLE_EOK 0
#define MAPLE_EAGAIN -1
typedef struct { int port, unit, valid; struct { unsigned functions; } info; } maple_device_t;
typedef union {
    uint32_t raw;
    struct { unsigned cont:1, res:3, motor:4, bpow:3, div:1, fpow:3, conv:1, freq:8, inc:8; };
} purupuru_effect_t;
static maple_device_t pad = {0,0,1,{MAPLE_FUNC_CONTROLLER}};
static maple_device_t pack = {0,2,1,{MAPLE_FUNC_PURUPURU}};
static uint64_t clock_ms;
static unsigned sends;
static int busy;
static purupuru_effect_t last;
static uint64_t timer_ms_gettime64(void) { return clock_ms; }
static maple_device_t *maple_enum_type(int n, unsigned f) { (void)n; (void)f; return pad.valid ? &pad : NULL; }
static maple_device_t *maple_enum_dev(int p, int u) { return pack.valid && pack.port == p && pack.unit == u ? &pack : NULL; }
static int purupuru_rumble(maple_device_t *d, const purupuru_effect_t *e) {
    assert(d == &pack);
    if (busy) return MAPLE_EAGAIN;
    ++sends; last = *e; return MAPLE_EOK;
}
void dc_trace(int slot, const char *fmt, ...) { (void)slot; (void)fmt; }
#define DC_RUMBLE_HOST_TEST
#include "../dc/dc_rumble.c"
int main(void)
{
    dc_rumble_set_ingame(1);
    dc_rumble_pulse(7, 220); dc_rumble_poll();
    assert(sends == 1 && last.fpow == 7 && last.motor == 1);
    clock_ms = 30; dc_rumble_pulse(2,70); dc_rumble_poll();
    assert(sends == 1); /* weak shot does not truncate missile */
    clock_ms = 220; dc_rumble_poll();
    assert(sends == 2 && last.fpow == 0);
    busy = 1; dc_rumble_pulse(4,70); dc_rumble_poll();
    clock_ms = 250; busy = 0; dc_rumble_poll();
    assert(sends == 3 && last.fpow == 4);
    clock_ms = 319; dc_rumble_poll(); assert(sends == 3);
    clock_ms = 320; busy = 1; dc_rumble_poll(); assert(sends == 3);
    busy = 0; dc_rumble_poll(); assert(sends == 4 && last.fpow == 0);
    dc_rumble_pulse(4,70); dc_rumble_poll();
    dc_rumble_set_ingame(0); assert(last.fpow == 0);
    unsigned before = sends;
    dc_rumble_pulse(7,220); dc_rumble_poll(); assert(sends == before);
    dc_rumble_set_ingame(1);
    pack.valid = 0; dc_rumble_pulse(4,70); dc_rumble_poll();
    pack.valid = 1; dc_rumble_poll(); assert(sends == before);
    busy = 1; dc_rumble_pulse(4,70); dc_rumble_poll();
    clock_ms += 1000; busy = 0; dc_rumble_poll();
    assert(sends == before); /* stale unsent event must expire */
    puts("rumble scheduling tests passed");
    return 0;
}
