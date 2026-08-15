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
 *	b21 tested the second by clearing INIT_PURUPURU, and it **still hung**. So
 *	the rumble driver is exonerated -- it never initialised in that build -- and
 *	the unbounded bus scan is the cause.
 *
 *	The decisive observation was Max's: pulling the pack out *while hung* lets
 *	the boot continue. That is exactly a blocking wait on a scan that completes
 *	the moment the offending device leaves the bus. `maple_scan_done` requires
 *	`scan_ready_mask == 0xf`, all four ports; the pack makes one never report.
 *
 *	INIT_PURUPURU is therefore restored here. Leaving the driver off would carry
 *	a change that fixes nothing and quietly obstructs the force-feedback work in
 *	BACKLOG.md.
 *
 *	Bounding the wait is not available to us: `maple_wait_scan` is bound to
 *	INIT_MAPLE_ALL alongside `maple_init` itself (arch/init_flags.h), so clearing
 *	it would take the whole maple subsystem including the controller. The routes
 *	that remain -- patching KOS's maple, or taking over maple init ourselves --
 *	belong with the rumble feature rather than before it. See BUGS.md.
 */

#include <kos/init.h>

KOS_INIT_FLAGS(INIT_DEFAULT);
