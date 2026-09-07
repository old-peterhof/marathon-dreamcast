#ifndef DC_RUMBLE_H
#define DC_RUMBLE_H
#ifdef __cplusplus
extern "C" {
#endif
/* One pulse: strength 1-7, length in ms. A stronger pulse already running
   is not interrupted by a weaker request. */
void dc_rumble_pulse(unsigned power, unsigned milliseconds);
/* Damage taken; strength follows the amount. */
void dc_rumble_hit(unsigned damage);
/* Rolling hold for a weapon that is charging: call every tick it is. */
void dc_rumble_charge(unsigned power);
void dc_rumble_poll(void);
void dc_rumble_set_ingame(int enabled);
#ifdef __cplusplus
}
#endif
#endif
