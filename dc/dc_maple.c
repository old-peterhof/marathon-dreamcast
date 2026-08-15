/*
 *	dc_maple.c -- which KallistiOS subsystems start up.
 *
 *	Confirmed on hardware: with a rumble pack fitted the game never gets past the
 *	Sega licence screen; without it, it boots normally. That is long before any
 *	of our code draws anything, so the fault is in KOS's start-up rather than the
 *	port.
 *
 *	There are two candidates in kernel/arch/dreamcast/kernel/init.c:
 *
 *	  1. KOS_INIT_FLAG_CALL(maple_wait_scan), which resolves to
 *	         thd_poll((thd_cb_t)maple_scan_done, &maple_state, 0);
 *	     a wait with a timeout of 0 -- meaning forever -- for all four ports to
 *	     report in. An unbounded wait during init is exactly the shape of this
 *	     hang.
 *
 *	  2. purupuru_init, the rumble driver itself.
 *
 *	Only the second can be tested. `maple_wait_scan` is bound to INIT_MAPLE_ALL
 *	alongside `maple_init` itself (arch/init_flags.h), so switching it off would
 *	take the whole maple subsystem with it, controller included. INIT_PURUPURU is
 *	an independent flag and can be cleared on its own.
 *
 *	So this build clears it, as an experiment rather than a decision:
 *
 *	  - If the game now boots with a pack fitted, the driver's initialisation is
 *	    the culprit, and that is precisely the thing to fix in order to support
 *	    force feedback properly.
 *	  - If it still hangs, the driver is exonerated and the unbounded bus scan is
 *	    the remaining suspect, which needs a different approach entirely.
 *
 *	Either outcome is worth a test cycle. Rumble support for gun shots and taking
 *	hits is wanted (see BACKLOG.md) and this is a step toward it, not away: the
 *	flag is one constant to put back once the cause is understood.
 */

#include <kos/init.h>

KOS_INIT_FLAGS(INIT_DEFAULT & ~INIT_PURUPURU);
