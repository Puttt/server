/* vi: set ts=2:
 *
 *	Eressea PB(E)M host Copyright (C) 1998-2003
 *      Christian Schlittchen (corwin@amber.kn-bremen.de)
 *      Katja Zedel (katze@felidae.kn-bremen.de)
 *      Henning Peters (faroul@beyond.kn-bremen.de)
 *      Enno Rehling (enno@eressea.de)
 *      Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
 *
 *  based on:
 *
 * Atlantis v1.0  13 September 1993 Copyright 1993 by Russell Wallace
 * Atlantis v1.7                    Copyright 1996 by Alex Schr�der
 *
 * This program may not be used, modified or distributed without
 * prior permission by the authors of Eressea.
 * This program may not be sold or used commercially without prior written
 * permission from the authors.
 */

#ifndef H_KRNL_BUILD
#define H_KRNL_BUILD
#ifdef __cplusplus
extern "C" {
#endif

/* Die enums fuer Gebauede werden nie gebraucht, nur bei der Bestimmung
 * des Schutzes durch eine Burg wird die Reihenfolge und MAXBUILDINGS
 * wichtig
 */

struct xml_tag;

typedef struct requirement {
	const struct resource_type * rtype;
	int number;
	double recycle; /* recycling quota */
} requirement;

typedef struct construction {
	skill_t skill; /* skill req'd per point of size */
	int minskill;  /* skill req'd per point of size */

	int maxsize;   /* maximum size of this type */
	int reqsize;   /* size of object using up 1 set of requirement. */
	requirement * materials; /* material req'd to build one object */

	struct construction * improvement;
		/* next level, if upgradable. if more than one of these items
		 * can be built (weapons, armour) per turn, must not be NULL,
		 * but point to the same type again:
		 *   const_sword.improvement = &const_sword
		 * last level of a building points to NULL, as do objects of
		 * an unlimited size.
		 */
	struct attrib * attribs;
		/* stores skill modifiers and other attributes */
} construction;

extern int destroy_cmd(struct unit * u, struct order * ord);
extern int leave_cmd(struct unit * u, struct order * ord);

extern boolean can_contact(const struct region *r, const struct unit *u, const struct unit *u2);

void do_siege(struct region *r);
void build_road(struct region * r, struct unit * u, int size, direction_t d);
void create_ship(struct region * r, struct unit * u, const struct ship_type * newtype, int size, struct order * ord);
void continue_ship(struct region * r, struct unit * u, int size);

struct building * getbuilding(const struct region * r);
struct ship *getship(const struct region * r);

void do_misc(struct region *r, boolean tries);

void reportevent(struct region * r, char *s);

void shash(struct ship * sh);
void sunhash(struct ship * sh);

extern int build(struct unit * u, const construction * ctype, int completed, int want);
extern int maxbuild(const struct unit *u, const construction *cons);
extern struct message * msg_materials_required(struct unit * u, struct order * ord, const struct construction * ctype, int multi);
/** error messages that build may return: */
#define ELOWSKILL -1
#define ENEEDSKILL -2
#define ECOMPLETE -3
#define ENOMATERIALS -4

#ifdef __cplusplus
}
#endif
#endif

