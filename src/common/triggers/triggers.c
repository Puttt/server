/* vi: set ts=2:
 *
 *	
 *	Eressea PB(E)M host Copyright (C) 1998-2003
 *      Christian Schlittchen (corwin@amber.kn-bremen.de)
 *      Katja Zedel (katze@felidae.kn-bremen.de)
 *      Henning Peters (faroul@beyond.kn-bremen.de)
 *      Enno Rehling (enno@eressea.de)
 *      Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
 *
 * This program may not be used, modified or distributed without
 * prior permission by the authors of Eressea.
 */

/** module: eressea/triggers
 * Registration and setup of all the triggers that are used in an Eressea
 * world.
 */

#include <config.h>
#include <eressea.h>

/* triggers includes */
#include <triggers/changefaction.h>
#include <triggers/changerace.h>
#include <triggers/createcurse.h>
#include <triggers/createunit.h>
#include <triggers/gate.h>
#include <triggers/unguard.h>
#include <triggers/giveitem.h>
#include <triggers/killunit.h>
#include <triggers/removecurse.h>
#include <triggers/shock.h>
#include <triggers/timeout.h>
#include <triggers/unitmessage.h>
#include <triggers/clonedied.h>

/* util includes */
#include <event.h>

/* libc includes */
#include <stdio.h>

void
init_triggers(void)
{
  if (quiet<2) printf("- registering triggers\n");
	tt_register(&tt_changefaction);
	tt_register(&tt_changerace);
	tt_register(&tt_createcurse);
	tt_register(&tt_createunit);
	tt_register(&tt_gate);
	tt_register(&tt_unguard);
	tt_register(&tt_giveitem);
	tt_register(&tt_killunit);
	tt_register(&tt_removecurse);
	tt_register(&tt_shock);
	tt_register(&tt_unitmessage);
	tt_register(&tt_timeout);
	tt_register(&tt_clonedied);
}
