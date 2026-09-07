/*
 *	dc_vmu_hud.h -- the VMU screen as a second HUD (dc_vmu_hud.cpp).
 *
 *	DC_MARK("xx") draws a two-letter stage code on the VMU. It is the
 *	hardware's stand-in for a serial cable: whatever code is showing when the
 *	console freezes names the stage that never finished. Codes in use:
 *	  C1..C3  Continue Game: newest slot, disc marker, VMU restore
 *	  C4..C5  load_and_start_game: after the fade, after the load
 *	  L1..L6  load_game_from_file: set_map_file, level from map, parent
 *	          checksum, use_map_file, RunLevelScript, entering_map
 *	  S1..S5  start_game: entry, video mode + PowerVR back, OGL_StartRun done,
 *	          enter_screen done, draw_interface done
 */
#ifndef DC_VMU_HUD_H
#define DC_VMU_HUD_H

#ifdef __cplusplus
extern "C" {
#endif
void dc_vmu_hud_set_ingame(int yes);
void dc_vmu_hud_poll(void);
void dc_vmu_mark(const char *tag);
#ifdef __cplusplus
}
#endif

#ifdef DC
#define DC_MARK(tag) dc_vmu_mark(tag)
#else
#define DC_MARK(tag) ((void)0)
#endif

#endif
