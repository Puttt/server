/* vi: set ts=2:
 *
 *	
 *	Eressea PB(E)M host Copyright (C) 1998-2003
 *      Christian Schlittchen (corwin@amber.kn-bremen.de)
 *      Katja Zedel (katze@felidae.kn-bremen.de)
 *      Henning Peters (faroul@beyond.kn-bremen.de)
 *      Enno Rehling (enno@eressea-pbem.de)
 *      Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
 *
 * This program may not be used, modified or distributed without
 * prior permission by the authors of Eressea.
 */

#ifndef H_KRNL_MAGIC
#define H_KRNL_MAGIC
#ifdef __cplusplus
extern "C" {
#endif

#include "curse.h"
struct fighter;
struct building;

/* ------------------------------------------------------------- */

#define MAXCOMBATSPELLS 3    /* PRECOMBAT COMBAT POSTCOMBAT */
#define MAX_SPELLRANK 9      /* Standard-Rank 5 */
#define MAXINGREDIENT	5      /* bis zu 5 Komponenten pro Zauber */
#define CHAOSPATZERCHANCE 10 /* +10% Chance zu Patzern */

/* ------------------------------------------------------------- */

#define IRONGOLEM_CRUMBLE   15  /* monatlich Chance zu zerfallen */
#define STONEGOLEM_CRUMBLE  10  /* monatlich Chance zu zerfallen */

/* ------------------------------------------------------------- */
/* Spruchparameter
 * Wir suchen beim Parsen des Befehls erstmal nach lokalen Objekten,
 * erst in verify_targets wird dann global gesucht, da in den meisten
 * F�llen das Zielobjekt lokal sein d�rfte */

/* siehe auch typ_t in objtypes.h */
typedef enum {
	SPP_REGION,   /* "r" : findregion(x,y) -> *region */
	SPP_UNIT,     /*  -  : atoi36() -> int */
  SPP_TEMP,     /*  -  : temp einheit */
	SPP_BUILDING, /*  -  : atoi() -> int */
	SPP_SHIP,     /*  -  : atoi() -> int */
	SPP_STRING,   /* "c" */
	SPP_INT,      /* "i" : atoi() -> int */
} sppobj_t;

typedef struct spllprm{
	sppobj_t typ;
	int flag;
	union {
		struct region *r;
		struct unit *u;
		struct building *b;
		struct ship *sh;
		char *s;
    char * xs;
		int i;
	} data;
} spllprm;

typedef struct spellparameter{
	int length;     /* Anzahl der Elemente */
	struct spllprm **param;
} spellparameter;

typedef struct strarray {
	int length;     /* Anzahl der Elemente */
	char **strings;
} strarray;

#define TARGET_RESISTS (1<<0)
#define TARGET_NOTFOUND (1<<1)

/* ------------------------------------------------------------- */
/* Magierichtungen */

/* typedef unsigned char magic_t; */
enum {
	M_GRAU,       /* none */
	M_TRAUM,      /* Illaun */
	M_ASTRAL,     /* Tybied */
	M_BARDE,      /* Cerddor */
	M_DRUIDE,     /* Gwyrrd */
	M_CHAOS,      /* Draig */
	MAXMAGIETYP,
	M_NONE = (magic_t) -1
};
extern const char *magietypen[MAXMAGIETYP];

/* ------------------------------------------------------------- */
/* Magier:
 * - Magierichtung
 * - Magiepunkte derzeit
 * - Malus (neg. Wert)/ Bonus (pos. Wert) auf maximale Magiepunkte
 *   (k�nnen sich durch Questen absolut ver�ndern und durch Gegenst�nde
 *   tempor�r). Auch f�r Artefakt ben�tigt man permanente MP
 * - Anzahl bereits gezauberte Spr�che diese Runde
 * - Kampfzauber (3) (vor/w�hrend/nach)
 * - Spruchliste
 */

typedef struct combatspell {
  int level;
  const struct spell * sp;
} combatspell;

typedef struct sc_mage {
  magic_t magietyp;
  int spellpoints;
  int spchange;
  int spellcount;
  combatspell combatspells[MAXCOMBATSPELLS];
  struct spell_list * spells;
} sc_mage;

/* ------------------------------------------------------------- */
/* Zauberliste */

typedef struct castorder {
  struct castorder *next;
  union {
    struct unit * u;
    struct fighter * fig;
  } magician; /* Magier (kann vom Typ struct unit oder fighter sein) */
  struct unit *familiar; /* Vertrauter, gesetzt, wenn der Spruch durch
                         den Vertrauten gezaubert wird */
  const struct spell *sp; /* Spruch */
  int level;             /* gew�nschte Stufe oder Stufe des Magiers */
  double force;          /* St�rke des Zaubers */
  struct region *rt;     /* Zielregion des Spruchs */
  int distance;          /* Entfernung zur Zielregion */
  struct order * order;           /* Befehl */
  struct spellparameter *par;  /* f�r weitere Parameter */
} castorder;

/* irgendwelche zauber: */
typedef void (*spell_f) (void*);
/* normale zauber: */
typedef int (*nspell_f)(castorder*);
/* kampfzauber: */
typedef int (*cspell_f) (struct fighter*, int, double, const struct spell * sp);
/* zauber-patzer: */
typedef void (*pspell_f) (castorder *);

typedef struct spell_component {
  const struct resource_type * type;
  int amount;
  int cost;
} spell_component;

typedef struct spell {
  spellid_t id;
  char *sname;
  char *syntax;
  char *parameter;
  magic_t magietyp;
  int sptyp;
  int rank;  /* Reihenfolge der Zauber */
  int level;  /* Stufe des Zaubers */
  struct spell_component * components;
  spell_f sp_function;
  void (*patzer) (castorder*);
} spell;

typedef struct spell_list {
  struct spell_list * next;
  spell * data;
} spell_list;

extern void spelllist_add(spell_list ** lspells, struct spell * sp);
extern spell_list ** spelllist_find(spell_list ** lspells, const struct spell * sp);
/* ------------------------------------------------------------- */

/* besondere Spruchtypen */
#define FARCASTING      (1<<0)	/* ZAUBER [struct region x y] */
#define SPELLLEVEL      (1<<1)	/* ZAUBER [STUFE x] */

/* ID's k�nnen zu drei unterschiedlichen Entit�ten geh�ren: Einheiten,
 * Geb�uden und Schiffen. */
#define UNITSPELL       (1<<2)	/* ZAUBER .. <Einheit-Nr> [<Einheit-Nr> ..] */
#define SHIPSPELL       (1<<3)	/* ZAUBER .. <Schiff-Nr> [<Schiff-Nr> ..] */
#define BUILDINGSPELL   (1<<4)	/* ZAUBER .. <Geb�ude-Nr> [<Geb�ude-Nr> ..] */
#define REGIONSPELL     (1<<5)  /* wirkt auf struct region */
#define ONETARGET       (1<<6)	/* ZAUBER .. <Ziel-Nr> */

#define PRECOMBATSPELL	(1<<7)	/* PR�KAMPFZAUBER .. */
#define COMBATSPELL     (1<<8)	/* KAMPFZAUBER .. */
#define POSTCOMBATSPELL	(1<<9)	/* POSTKAMPFZAUBER .. */
#define ISCOMBATSPELL   (PRECOMBATSPELL|COMBATSPELL|POSTCOMBATSPELL)

#define OCEANCASTABLE   (1<<10)	/* K�nnen auch nicht-Meermenschen auf
																	 hoher See zaubern */
#define ONSHIPCAST      (1<<11) /* kann auch auf von Land ablegenden
																	 Schiffen stehend gezaubert werden */
/*  */
#define NOTFAMILIARCAST (1<<12)
#define TESTRESISTANCE  (1<<13) /* alle Zielobjekte (u, s, b, r) auf
																	 Magieresistenz pr�fen */
#define SEARCHGLOBAL    (1<<14) /* Ziel global anstatt nur in target_region
																	 suchen */
#define TESTCANSEE      (1<<15) /* alle Zielunits auf cansee pr�fen */

/* Flag Spruchkostenberechnung: */
enum{
	SPC_FIX,      /* Fixkosten */
	SPC_LEVEL,    /* Komponenten pro Level */
	SPC_LINEAR    /* Komponenten pro Level und m�ssen vorhanden sein */
};

enum {
	RS_DUMMY,
	RS_FARVISION,
	MAX_REGIONSPELLS
};

/* ------------------------------------------------------------- */
/* Prototypen */

void magic(void);

void regeneration_magiepunkte(void);

extern struct attrib_type at_seenspell;
extern struct attrib_type at_mage;
extern struct attrib_type at_familiarmage;
extern struct attrib_type at_familiar;
extern struct attrib_type at_clonemage;
extern struct attrib_type at_clone;
extern struct attrib_type at_reportspell;
extern struct attrib_type at_icastle;

typedef struct icastle_data {
	const struct building_type * type;
	struct building * building; /* reverse pointer to dissolve the object */
	int time;
} icastle_data;


/* ------------------------------------------------------------- */
/* Kommentare:
 *
 * Spruchzauberrei und Gegenstandszauberrei werden getrennt behandelt.
 * Das macht u.a. bestimmte Fehlermeldungen einfacher, das
 * identifizieren der Komponennten �ber den Missversuch ist nicht
 * m�glich
 * Spruchzauberrei: 'ZAUBER [struct region x y] [STUFE a] "Spruchname" [Ziel]'
 * Gegenstandszauberrei: 'BENUTZE "Gegenstand" [Ziel]'
 *
 * Die Funktionen:
 */

/* Magier */
sc_mage * create_mage(struct unit *u, magic_t mtyp);
	/*	macht die struct unit zu einem neuen Magier: legt die struct u->mage an
	 *	und	initialisiert den Magiertypus mit mtyp.  */
sc_mage * get_mage(const struct unit *u);
	/*	gibt u->mage zur�ck, bei nicht-Magiern *NULL */
magic_t find_magetype(const struct unit *u);
	/*	gibt den Magietyp der struct unit zur�ck, bei nicht-Magiern 0 */
boolean is_mage(const struct unit *u);
	/*	gibt true, wenn u->mage gesetzt.  */
boolean is_familiar(const struct unit *u);
	/*	gibt true, wenn eine Familiar-Relation besteht.  */

/* Spr�che */
int get_combatspelllevel(const struct unit *u, int nr);
	/*  versucht, eine eingestellte maximale Kampfzauberstufe
	 *  zur�ckzugeben. 0 = Maximum, -1 u ist kein Magier. */
const spell *get_combatspell(const struct unit *u, int nr);
	/*	gibt den Kampfzauber nr [pre/kampf/post] oder NULL zur�ck */
void set_combatspell(struct unit *u, spell *sp, struct order * ord, int level);
	/* 	setzt Kampfzauber */
void unset_combatspell(struct unit *u, spell *sp);
	/* 	l�scht Kampfzauber */
void add_spell(struct sc_mage *mage, spell *sp);
	/* f�gt den Spruch mit der Id spellid der Spruchliste der Einheit hinzu. */
boolean has_spell(const struct unit *u, const struct spell * sp);
	/* pr�ft, ob der Spruch in der Spruchliste der Einheit steht. */
void updatespelllist(struct unit *u);
	/* f�gt alle Zauber des Magiegebietes der Einheit, deren Stufe kleiner
	 * als das aktuelle Magietalent ist, in die Spruchliste der Einheit
	 * ein */
boolean knowsspell(const struct region * r, const struct unit * u, const spell * sp);
	/* pr�ft, ob die Einheit diesen Spruch gerade beherrscht, dh
	 * mindestens die erforderliche Stufe hat. Hier k�nnen auch Abfragen
	 * auf spezielle Antimagiezauber auf Regionen oder Einheiten eingef�gt
	 * werden
	 */


/* Magiepunkte */
int get_spellpoints(const struct unit *u);
	/*	Gibt die aktuelle Anzahl der Magiepunkte der Einheit zur�ck */
void set_spellpoints(struct unit * u, int sp);
	/* setzt die Magiepunkte auf sp */
int change_spellpoints(struct unit *u, int mp);
	/*	ver�ndert die Anzahl der Magiepunkte der Einheit um +mp */
int max_spellpoints(const struct region *r, const struct unit *u);
	/*	gibt die aktuell maximal m�glichen Magiepunkte der Einheit zur�ck */
int change_maxspellpoints(struct unit * u, int csp);
   /* ver�ndert die maximalen Magiepunkte einer Einheit */

/* Zaubern */
extern double spellpower(struct region *r, struct unit *u, const spell *sp, int cast_level, struct order * ord);
	/*	ermittelt die St�rke eines Spruchs */
boolean fumble (struct region *r, struct unit *u, const spell *sp, int cast_level);
	/*	true, wenn der Zauber misslingt, bei false gelingt der Zauber */


typedef struct spellrank {
  struct castorder * begin;
  struct castorder ** end;
} spellrank;

castorder *new_castorder(void *u, struct unit *familiar, const spell *sp, struct region *r,
		int lev, double force, int distance, struct order * ord, spellparameter *p);
	/* Zwischenspreicher f�r Zauberbefehle, notwendig f�r Priorit�ten */
void add_castorder(struct spellrank *cll, struct castorder *co);
	/* H�nge c-order co an die letze c-order von cll an */
void free_castorders(struct castorder *co);
	/* Speicher wieder freigeben */

/* Pr�froutinen f�r Zaubern */
int countspells(struct unit *u, int step);
	/*	erh�ht den Counter f�r Zauberspr�che um 'step' und gibt die neue
	 *	Anzahl der gezauberten Spr�che zur�ck. */
int spellcost(struct unit *u, const spell *sp);
	/*	gibt die f�r diesen Spruch derzeit notwendigen Magiepunkte auf der
	 *	geringstm�glichen Stufe zur�ck, schon um den Faktor der bereits
	 *	zuvor gezauberten Spr�che erh�ht */
boolean cancast (struct unit *u, const spell *spruch, int eff_stufe, int distance, struct order * ord);
	/*	true, wenn Einheit alle Komponenten des Zaubers (incl. MP) f�r die
	 *	geringstm�gliche Stufe hat und den Spruch beherrscht */
void pay_spell(struct unit *u, const spell *sp, int eff_stufe, int distance);
	/*	zieht die Komponenten des Zaubers aus dem Inventory der Einheit
	 *	ab. Die effektive Stufe des gezauberten Spruchs ist wichtig f�r
	 *	die korrekte Bestimmung der Magiepunktkosten */
int eff_spelllevel(struct unit *u, const spell * sp, int cast_level, int distance);
	/*	ermittelt die effektive Stufe des Zaubers. Dabei ist cast_level
	 *	die gew�nschte maximale Stufe (im Normalfall Stufe des Magiers,
	 *	bei Farcasting Stufe*2^Entfernung) */
boolean is_magic_resistant(struct unit *magician, struct unit *target, int
	resist_bonus);
	/*	Mapperfunktion f�r target_resists_magic() vom Typ struct unit. */
extern double magic_resistance(struct unit *target);
	/*	gibt die Chance an, mit der einem Zauber widerstanden wird. Je
	 *	gr��er, desto resistenter ist da Opfer */
boolean target_resists_magic(struct unit *magician, void *obj, int objtyp,
		int resist_bonus);
	/*	gibt false zur�ck, wenn der Zauber gelingt, true, wenn das Ziel
	 *	widersteht */


/* Spr�che in der struct region */
   /* (sind in curse)*/
extern struct unit * get_familiar(const struct unit *u);
extern struct unit * get_familiar_mage(const struct unit *u);
extern struct unit * get_clone(const struct unit *u);
extern struct unit * get_clone_mage(const struct unit *u);
extern struct attrib_type at_familiar;
extern struct attrib_type at_familiarmage;
extern void remove_familiar(struct unit * mage);
extern boolean create_newfamiliar(struct unit * mage, struct unit * familiar);
extern void create_newclone(struct unit * mage, struct unit * familiar);
extern struct unit * has_clone(struct unit * mage);

extern const char * spell_info(const struct spell * sp, const struct locale * lang);
extern const char * spell_name(const struct spell * sp, const struct locale * lang);
extern const char * curse_name(const struct curse_type * ctype, const struct locale * lang);

extern struct message * msg_unitnotfound(const struct unit * mage, struct order * ord, const struct spllprm * spobj);

#ifdef __cplusplus
}
#endif
#endif
