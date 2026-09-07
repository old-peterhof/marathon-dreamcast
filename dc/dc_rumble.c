/* Local feedback: no allocation, sleeps, simulation RNG or accessory waits. */
#include <stdint.h>
#include <stddef.h>
#include "dc_rumble.h"
#ifndef DC_RUMBLE_HOST_TEST
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <dc/maple/purupuru.h>
#include <arch/timer.h>
#endif
extern void dc_trace(int slot, const char *fmt, ...);
static int in_game;
static unsigned wanted_power, wanted_ms, sent_power, generation, sent_generation;
static uint64_t deadline;
static int active_port = -1, active_unit = -1;

static void request(unsigned power, unsigned milliseconds)
{
    uint64_t now;
    {
        static unsigned requests;
        /* the first few of any kind, and every heavy one (the missile branch is rare) */
        if (++requests <= 4 || power >= 7)
            dc_trace(64, "rumble: request power=%u ms=%u ingame=%d", power, milliseconds, in_game);
    }
    if (!in_game) return;
    now = timer_ms_gettime64();
    if (now < deadline && wanted_power > power) return;
    wanted_power = power;
    wanted_ms = milliseconds;
    /* Provisional; restarted when the pack actually accepts the command. On
       the console the first send can come back MAPLE_EAGAIN for a frame or
       two, and a 70 ms request measured from here expired before it was ever
       sent. Flycast never refuses, which is why the emulator felt it. */
    deadline = now + milliseconds;
    ++generation;
}
void dc_rumble_shot(int missile) { request(missile ? 7 : 4, missile ? 250 : 90); }
void dc_rumble_hit(void) { request(6, 120); }

static int send_effect(maple_device_t *pack, unsigned power)
{
    purupuru_effect_t effect;
    effect.raw = 0;
    effect.motor = 1;
    effect.fpow = effect.bpow = power;
    effect.freq = 32;
    effect.inc = 1;
    /* Continuous, stopped explicitly at the deadline (power 0). The one-shot
     * form with inc=1 at power 2 was not felt on a real pack at all. */
    effect.cont = power ? 1 : 0;
    return purupuru_rumble(pack, &effect);
}
void dc_rumble_poll(void)
{
    maple_device_t *controller = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    maple_device_t *pack = NULL;
    int unit;
    uint64_t now = timer_ms_gettime64();
    if (controller && controller->valid) {
        for (unit = 1; unit < 6; ++unit) {
            maple_device_t *candidate = maple_enum_dev(controller->port, unit);
            if (candidate && candidate->valid &&
                (candidate->info.functions & MAPLE_FUNC_PURUPURU)) {
                pack = candidate;
                break;
            }
        }
    }
    if (!pack || pack->port != active_port || pack->unit != active_unit) {
        dc_trace(64, "rumble: pack %s (port %d unit %d)", pack ? "found" : "absent",
                 pack ? pack->port : -1, pack ? pack->unit : -1);
        /* Re-enumerate: never keep a Maple pointer through hot-unplug. */
        if (active_port >= 0 && sent_power) {
            maple_device_t *old = maple_enum_dev(active_port, active_unit);
            if (old && old->valid && (old->info.functions & MAPLE_FUNC_PURUPURU)
                && send_effect(old, 0) == MAPLE_EAGAIN) return;
        }
        active_port = pack ? pack->port : -1;
        active_unit = pack ? pack->unit : -1;
        sent_power = 0;
        sent_generation = generation - 1;
    }
    if (!pack) { wanted_power = 0; return; }
    /* Only a request that has been sent can expire. */
    if (!in_game || (sent_generation == generation && now >= deadline)) wanted_power = 0;
    if (wanted_power == sent_power &&
        (!wanted_power || sent_generation == generation)) return;
    if (send_effect(pack, wanted_power) != MAPLE_EOK) return;
    if (wanted_power && sent_generation != generation) deadline = now + wanted_ms;
    sent_power = wanted_power;
    sent_generation = generation;
    {
        static unsigned commands;
        if (++commands <= 8 || commands % 64 == 0)
            dc_trace(64, "rumble: power=%u port=%d unit=%d command=%u",
                     sent_power, active_port, active_unit, commands);
    }
}
void dc_rumble_set_ingame(int enabled)
{
    in_game = enabled != 0;
    if (!in_game) {
        wanted_power = 0;
        dc_rumble_poll();
    }
}
