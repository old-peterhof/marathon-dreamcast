/*
 *	dc_vmu_hud.cpp -- the VMU screen as a second HUD.
 *
 *	48x32, one bit. Top line: weapon name and spare magazines. Middle:
 *	the magazine as a grid of bullets (one per round, the way the panel draws
 *	it) or an energy bar, one per trigger when two weapons are up. Bottom: the
 *	shield and oxygen bars with tick marks, the way the 1995 panel had them.
 *
 *	The raw LCD buffer is the image rotated 180 degrees with the leftmost pixel
 *	in a byte's high bit (see vmu_xbm_to_bitmap in KOS), which is how a VMU
 *	reads when it sits in a controller. plot() hides that. Frames go out at
 *	most every four game frames and only when something changed; a busy Maple
 *	bus (MAPLE_EAGAIN) is retried next frame. Outside play the screen shows a
 *	title card.
 */
#ifdef DC
#include <string.h>
#include <stdint.h>

#include "cseries.h"
#include "map.h"
#include "player.h"
#include "weapons.h"
#include "items.h"
#include "HUDRenderer.h"

extern "C" void dc_trace(int slot, const char *fmt, ...);
extern "C" int dc_trace_on(void);
extern "C" unsigned long dc_ms(void);
extern "C" int dc_vmu_lcd_send(const void *bitmap);	/* dc_compat.c: 0 sent, -1 bus busy, -2 no VMU */
extern "C" unsigned dc_rumble_status(void);
short dc_weapon_ammunition_item(short weapon_type, short which_trigger);

namespace {

const int W = 48, H = 32;
uint8_t Frame[W * H / 8] __attribute__((aligned(4)));
uint8_t Sent[W * H / 8];
bool Pending = false;
bool Fresh = false;
bool InGame = false;

inline void plot(int x, int y)
{
	if (x < 0 || y < 0 || x >= W || y >= H) return;
	int rx = W - 1 - x, ry = H - 1 - y;
	Frame[ry * (W / 8) + rx / 8] |= (uint8_t)(0x80 >> (rx & 7));
}
inline void unplot(int x, int y)
{
	if (x < 0 || y < 0 || x >= W || y >= H) return;
	int rx = W - 1 - x, ry = H - 1 - y;
	Frame[ry * (W / 8) + rx / 8] &= (uint8_t)~(0x80 >> (rx & 7));
}
void hline(int x0, int x1, int y) { for (int x = x0; x <= x1; ++x) plot(x, y); }
void vline(int x, int y0, int y1) { for (int y = y0; y <= y1; ++y) plot(x, y); }
void box(int x0, int y0, int x1, int y1) { hline(x0, x1, y0); hline(x0, x1, y1); vline(x0, y0, y1); vline(x1, y0, y1); }
void fill(int x0, int y0, int x1, int y1) { for (int y = y0; y <= y1; ++y) hline(x0, x1, y); }

// 3x5 font, rows top to bottom, three bits each (left is the high bit).
struct Glyph { char c; uint8_t rows[5]; };
const Glyph Font[] = {
	{'0',{7,5,5,5,7}}, {'1',{2,6,2,2,7}}, {'2',{7,1,7,4,7}}, {'3',{7,1,7,1,7}}, {'4',{5,5,7,1,1}},
	{'5',{7,4,7,1,7}}, {'6',{7,4,7,5,7}}, {'7',{7,1,1,1,1}}, {'8',{7,5,7,5,7}}, {'9',{7,5,7,1,7}},
	{'A',{7,5,7,5,5}}, {'B',{6,5,6,5,6}}, {'C',{7,4,4,4,7}}, {'D',{6,5,5,5,6}}, {'E',{7,4,7,4,7}},
	{'F',{7,4,7,4,4}}, {'G',{7,4,5,5,7}}, {'H',{5,5,7,5,5}}, {'I',{7,2,2,2,7}}, {'J',{1,1,1,5,7}},
	{'K',{5,5,6,5,5}}, {'L',{4,4,4,4,7}}, {'M',{5,7,7,5,5}}, {'N',{6,5,5,5,5}}, {'O',{7,5,5,5,7}},
	{'P',{7,5,7,4,4}}, {'Q',{7,5,5,7,1}}, {'R',{7,5,6,5,5}}, {'S',{7,4,7,1,7}}, {'T',{7,2,2,2,2}},
	{'U',{5,5,5,5,7}}, {'V',{5,5,5,5,2}}, {'W',{5,5,7,7,5}}, {'X',{5,5,2,5,5}}, {'Y',{5,5,7,2,2}},
	{'Z',{7,1,2,4,7}}, {'-',{0,0,7,0,0}}, {'.',{0,0,0,0,2}}, {'x',{0,5,2,5,0}}, {'/',{1,1,2,4,4}},
	{' ',{0,0,0,0,0}},
};
void text(int x, int y, const char *s)
{
	for (; *s; ++s, x += 4)
	{
		const Glyph *g = NULL;
		for (unsigned i = 0; i < sizeof Font / sizeof Font[0]; ++i)
			if (Font[i].c == *s) { g = &Font[i]; break; }
		if (!g) continue;
		for (int r = 0; r < 5; ++r)
			for (int c = 0; c < 3; ++c)
				if (g->rows[r] & (4 >> c)) plot(x + c, y + r);
	}
}
int textw(const char *s) { return (int)strlen(s) * 4 - 1; }

// The same font at twice the size, for a code that has to be read across a room.
void text2x(int x, int y, const char *s)
{
	for (; *s; ++s, x += 8)
	{
		const Glyph *g = NULL;
		for (unsigned i = 0; i < sizeof Font / sizeof Font[0]; ++i)
			if (Font[i].c == *s) { g = &Font[i]; break; }
		if (!g) continue;
		for (int r = 0; r < 5; ++r)
			for (int c = 0; c < 3; ++c)
				if (g->rows[r] & (4 >> c)) fill(x + 2*c, y + 2*r, x + 2*c + 1, y + 2*r + 1);
	}
}

const char *const WeaponNames[MAXIMUM_NUMBER_OF_WEAPONS] = {
	"FISTS", "MAGNUM", "FUSION", "MA-75B", "SPNKR", "TOZT-7", "ALIEN", "WSTE", "BALL", "KKV-7"
};

// A magazine drawn as bullets, or a beam weapon as a bar, inside x0..x1, y0..y1.
void ammo_panel(int x0, int y0, int x1, int y1, const weapon_interface_ammo_data& d, int count)
{
	const int aw = x1 - x0 + 1, ah = y1 - y0 + 1;
	if (d.type == _uses_energy)
	{
		const int by0 = y0 + (ah - 7) / 2, by1 = by0 + 6;
		box(x0, by0, x1, by1);
		const int inner = aw - 2;
		int n = d.ammo_across > 0 ? (count * inner) / d.ammo_across : 0;
		if (n > inner) n = inner;
		if (n > 0) fill(x0 + 1, by0 + 1, x0 + n, by1 - 1);
		// quarter ticks cut into the fill, like the panel's segments
		for (int q = 1; q < 4; ++q) { int tx = x0 + 1 + (inner * q) / 4; unplot(tx, by0 + 1); unplot(tx, by1 - 1); }
		return;
	}
	const int across = d.ammo_across > 0 ? d.ammo_across : 1;
	const int down = d.ammo_down > 0 ? d.ammo_down : 1;
	int cw = aw / across; if (cw > 5) cw = 5; if (cw < 1) cw = 1;
	int ch = ah / down;   if (ch > 5) ch = 5; if (ch < 1) ch = 1;
	const int bw = cw > 1 ? cw - 1 : 1, bh = ch > 1 ? ch - 1 : 1;
	const int gridw = across * cw, gridh = down * ch;
	const int gx = x0 + (aw - gridw) / 2, gy = y0 + (ah - gridh) / 2;
	int max = across * down; if (count > max) count = max;
	for (int i = 0; i < count; ++i)
	{
		int row = i / across, col = i % across;
		if (d.right_to_left) col = across - 1 - col;
		int bx = gx + col * cw, by = gy + row * ch;
		fill(bx, by, bx + bw - 1, by + bh - 1);
	}
}

void bar(int y, char label, int value, int max)
{
	char l[2] = { label, 0 };
	text(0, y, l);
	const int x0 = 5, x1 = W - 1;
	box(x0, y, x1, y + 4);
	const int inner = x1 - x0 - 1;
	int base = value > max ? max : value;
	int n = max > 0 ? (base * inner) / max : 0;
	if (n > 0) fill(x0 + 1, y + 1, x0 + n, y + 3);
	// thirds, where the panel changes colour, cut into the fill
	for (int t = 1; t < 3; ++t) { int tx = x0 + 1 + (inner * t) / 3; unplot(tx, y + 1); unplot(tx, y + 3); }
	// past the maximum (2x, 3x shields): a hatched second layer
	if (value > max)
	{
		int extra = ((value - max) * inner) / max; if (extra > inner) extra = inner;
		for (int x = 0; x < extra; ++x) if ((x & 1) == 0) plot(x0 + 1 + x, y + 2);
		for (int x = 0; x < extra; ++x) if ((x & 1) == 1) { plot(x0 + 1 + x, y + 1); plot(x0 + 1 + x, y + 3); }
	}
}

void compose()
{
	memset(Frame, 0, sizeof Frame);
	player_data *player = get_player_data(local_player_index);
	if (!player) return;

	// Header: weapon, spare magazines
	short weapon = get_player_desired_weapon(local_player_index);
	const char *name = (weapon >= 0 && weapon < MAXIMUM_NUMBER_OF_WEAPONS) ? WeaponNames[weapon] : "";
	text(0, 0, name);
	if (weapon >= 0)
	{
		short item = dc_weapon_ammunition_item(weapon, _primary_weapon);
		if (item != NONE && item >= 0 && item < NUMBER_OF_ITEMS && player->items[item] >= 0)
		{
			char s[8]; int n = player->items[item]; if (n > 99) n = 99;
			s[0] = 'x'; if (n >= 10) { s[1] = '0' + n / 10; s[2] = '0' + n % 10; s[3] = 0; } else { s[1] = '0' + n; s[2] = 0; }
			text(25, 0, s);
		}
	}
	hline(0, W - 1, 6);
	// Rumble pack, at the right end of the rule: outline = pack seen, solid =
	// a command was queued by KOS, a notch = the last send was refused. The console
	// has no serial log; this is how "did rumble work" gets answered.
	{
		unsigned st = dc_rumble_status();
		if (st & 1) { for (int x = 43; x < W; ++x) unplot(x, 6); box(43, 5, W - 1, 7); }
		if (st & 2) fill(44, 6, W - 2, 6);
		if (st & 4) { unplot(45, 5); unplot(45, 7); }
	}

	// Ammunition: rows 8..19
	if (weapon >= 0 && weapon < 10)
	{
		const weapon_interface_data& wd = weapon_interface_definitions[weapon];
		int counts[2];
		bool shown[2];
		for (int t = 0; t < 2; ++t)
		{
			counts[t] = get_player_weapon_ammo_count(local_player_index, weapon, t == 0 ? _primary_weapon : _secondary_weapon);
			shown[t] = wd.ammo_data[t].type != _unused_interface_data && counts[t] != NONE;
		}
		if (shown[0] && shown[1])
		{
			ammo_panel(0, 8, 22, 19, wd.ammo_data[0], counts[0]);
			ammo_panel(25, 8, W - 1, 19, wd.ammo_data[1], counts[1]);
		}
		else if (shown[0]) ammo_panel(0, 8, W - 1, 19, wd.ammo_data[0], counts[0]);
		else if (shown[1]) ammo_panel(0, 8, W - 1, 19, wd.ammo_data[1], counts[1]);
	}

	// Shield and oxygen: rows 21..25 and 27..31
	bar(21, 'S', player->suit_energy, PLAYER_MAXIMUM_SUIT_ENERGY);
	bar(27, 'O', player->suit_oxygen, PLAYER_MAXIMUM_SUIT_OXYGEN);
}

// Between levels and in the menus.
void compose_idle()
{
	memset(Frame, 0, sizeof Frame);
	text(8, 6, "MARATHON");
	hline(4, W - 5, 13);
	text(6, 16, "DREAMCAST");
	box(0, 0, W - 1, H - 1);
}

void dump()
{
	// The screen as text, for a debug log: what the VMU shows, seen upright.
	for (int y = 0; y < H; ++y)
	{
		char line[W + 1];
		for (int x = 0; x < W; ++x)
		{
			int rx = W - 1 - x, ry = H - 1 - y;
			line[x] = (Frame[ry * (W / 8) + rx / 8] & (0x80 >> (rx & 7))) ? '#' : '.';
		}
		line[W] = 0;
		dc_trace(71, "vmu|%s|", line);
	}
}

} // namespace

extern "C" void dc_vmu_hud_set_ingame(int yes)
{
	if (InGame == (yes != 0)) return;
	InGame = yes != 0;
	memset(Sent, 0xff, sizeof Sent);
	if (InGame) Fresh = true;
	else { compose_idle(); Pending = true; }
}

/*
 *	A loading breadcrumb (see DC_MARK in dc_vmu_hud.h). Sent at once, with a
 *	few retries if the bus is busy, and left showing until the HUD or the
 *	title card next redraws. Nothing polls the pad during a load, so the code
 *	stays up exactly as long as the stage it names.
 */
extern "C" void dc_vmu_mark(const char *tag)
{
	dc_trace(74, "mark %s", tag);
	memset(Frame, 0, sizeof Frame);
	text(10, 3, "LOADING");
	text2x((W - ((int)strlen(tag) * 8 - 2)) / 2, 12, tag);
	box(0, 0, W - 1, H - 1);
	for (int tries = 0; tries < 4; ++tries)
	{
		if (dc_vmu_lcd_send(Frame) != -1) break;
		unsigned long t = dc_ms();
		while (dc_ms() - t < 8) ;
	}
	memset(Sent, 0xff, sizeof Sent);	// whatever comes next must be resent
	if (!InGame) { compose_idle(); Pending = true; }
}

extern "C" void dc_vmu_hud_poll(void)
{
	static unsigned tick = 0;
	unsigned long now = dc_ms();
	if (InGame && ((++tick & 3) == 0 || Fresh))
	{
		Fresh = false;
		compose();
		if (memcmp(Frame, Sent, sizeof Frame) != 0) Pending = true;
	}
	if (!Pending) return;
	if (dc_vmu_lcd_send(Frame) != 0) return;
	memcpy(Sent, Frame, sizeof Sent);
	Pending = false;
	static unsigned long dumped = 0;
	if (dc_trace_on() && now - dumped > 2000) { dumped = now; dump(); }
}
#endif
