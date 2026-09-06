#ifndef DC_RUMBLE_H
#define DC_RUMBLE_H
#ifdef __cplusplus
extern "C" {
#endif
void dc_rumble_shot(int missile);
void dc_rumble_hit(void);
void dc_rumble_poll(void);
void dc_rumble_set_ingame(int enabled);
#ifdef __cplusplus
}
#endif
#endif
