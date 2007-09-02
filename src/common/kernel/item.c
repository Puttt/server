/* vi: set ts=2:
 *
 *
 *  Eressea PB(E)M host Copyright (C) 1998-2003
 *      Christian Schlittchen (corwin@amber.kn-bremen.de)
 *      Katja Zedel (katze@felidae.kn-bremen.de)
 *      Henning Peters (faroul@beyond.kn-bremen.de)
 *      Enno Rehling (enno@eressea.de)
 *      Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
 *
 * This program may not be used, modified or distributed without
 * prior permission by the authors of Eressea.
 */

#include <config.h>
#include "eressea.h"
#include "item.h"

#include <attributes/key.h>

#include "alchemy.h"
#include "build.h"
#include "faction.h"
#include "message.h"
#include "pool.h"
#include "race.h"
#include "region.h"
#include "save.h"
#include "skill.h"
#include "terrain.h"
#include "unit.h"

/* triggers includes */
#include <triggers/changerace.h>
#include <triggers/timeout.h>

/* util includes */
#include <util/attrib.h>
#include <util/event.h>
#include <util/functions.h>
#include <util/goodies.h>
#include <util/message.h>
#include <util/umlaut.h>
#include <util/rng.h>

/* libc includes */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

resource_type * resourcetypes;
luxury_type * luxurytypes;
potion_type * potiontypes;

#define IMAXHASH 127
static item_type * itemtypes[IMAXHASH];

enum {
  H_PLAIN_1,
  H_PLAIN_2,
  H_PLAIN_3,
  H_FOREST_1,
  H_FOREST_2,
  H_FOREST_3,
  H_SWAMP_1,
  H_SWAMP_2,
  H_SWAMP_3,
  H_DESERT_1,
  H_DESERT_2,
  H_DESERT_3,
  H_HIGHLAND_1,
  H_HIGHLAND_2,
  H_HIGHLAND_3,
  H_MOUNTAIN_1,
  H_MOUNTAIN_2,
  H_MOUNTAIN_3,
  H_GLACIER_1,
  H_GLACIER_2,
  H_GLACIER_3,
  MAX_HERBS
};

static int
res_changeaura(unit * u, const resource_type * rtype, int delta)
{
  assert(rtype!=NULL);
  change_spellpoints(u, delta);
  return 0;
}

static int
res_changeperson(unit * u, const resource_type * rtype, int delta)
{
  assert(rtype!=NULL || !"not implemented");
  scale_number(u, u->number+delta);
  return 0;
}

static int
res_changepermaura(unit * u, const resource_type * rtype, int delta)
{
  assert(rtype!=NULL);
  change_maxspellpoints(u, delta);
  return 0;
}

static int
res_changehp(unit * u, const resource_type * rtype, int delta)
{
  assert(rtype!=NULL);
  u->hp+=delta;
  return 0;
}

static int
res_changepeasants(unit * u, const resource_type * rtype, int delta)
{
  assert(rtype!=NULL && u->region->land);
  u->region->land->peasants+=delta;
  return 0;
}

int
res_changeitem(unit * u, const resource_type * rtype, int delta)
{
  int num;
  if (rtype == oldresourcetype[R_STONE] && u->race==new_race[RC_STONEGOLEM] && delta<=0) {
    int reduce = delta / GOLEM_STONE;
    if (delta % GOLEM_STONE != 0) --reduce;
    scale_number(u, u->number+reduce);
    num = u->number;
  } else if (rtype == oldresourcetype[R_IRON] && u->race==new_race[RC_IRONGOLEM] && delta<=0) {
    int reduce = delta / GOLEM_IRON;
    if (delta % GOLEM_IRON != 0) --reduce;
    scale_number(u, u->number+reduce);
    num = u->number;
  } else {
    const item_type * itype = resource2item(rtype);
    item * i;
    assert(itype!=NULL);
    i = i_change(&u->items, itype, delta);
    if (i==NULL) return 0;
    num = i->number;
  }
  return num;
}

const char *
resourcename(const resource_type * rtype, int flags)
{
  int i = 0;

  if (rtype->name) return rtype->name(rtype, flags);

  if (flags & NMF_PLURAL) i = 1;
  if (flags & NMF_APPEARANCE && rtype->_appearance[i]) {
    return rtype->_appearance[i];
  }
  return rtype->_name[i];
}

resource_type *
new_resourcetype(const char ** names, const char ** appearances, int flags)
{
  resource_type * rtype = rt_find(names[0]);

  if (rtype==NULL) {
    int i;
    rtype = calloc(sizeof(resource_type), 1);

    for (i=0; i!=2; ++i) {
      rtype->_name[i] = strdup(names[i]);
      if (appearances) rtype->_appearance[i] = strdup(appearances[i]);
      else rtype->_appearance[i] = NULL;
    }
    rt_register(rtype);
  }
#ifndef NDEBUG
  else {
    /* TODO: check that this is the same type */
  }
#endif
  rtype->flags |= flags;
  return rtype;
}

void
it_register(item_type * itype)
{
  int hash = hashstring(itype->rtype->_name[0]);
  int key = hash % IMAXHASH;
  item_type ** p_itype = &itemtypes[key];
  while (*p_itype && *p_itype != itype) p_itype = &(*p_itype)->next;
  if (*p_itype==NULL) {
    itype->rtype->itype = itype;
    *p_itype = itype;
    rt_register(itype->rtype);
  }
}

item_type *
new_itemtype(resource_type * rtype,
             int iflags, int weight, int capacity)
{
  item_type * itype;
  assert (resource2item(rtype) == NULL);

  assert(rtype->flags & RTF_ITEM);
  itype = calloc(sizeof(item_type), 1);

  itype->rtype = rtype;
  itype->weight = weight;
  itype->capacity = capacity;
  itype->flags |= iflags;
  it_register(itype);

  rtype->uchange = res_changeitem;

  return itype;
}

static void
lt_register(luxury_type * ltype)
{
  ltype->itype->rtype->ltype = ltype;
  ltype->next = luxurytypes;
  luxurytypes = ltype;
}

luxury_type *
new_luxurytype(item_type * itype, int price)
{
  luxury_type * ltype;

  assert(resource2luxury(itype->rtype) == NULL);

  ltype = calloc(sizeof(luxury_type), 1);
  ltype->itype = itype;
  ltype->price = price;
  lt_register(ltype);

  return ltype;
}

weapon_type *
new_weapontype(item_type * itype,
               int wflags, double magres, const char* damage[], int offmod, int defmod, int reload, skill_t sk, int minskill)
{
  weapon_type * wtype;

  assert(resource2weapon(itype->rtype)==NULL);

  wtype = calloc(sizeof(weapon_type), 1);
  if (damage) {
    wtype->damage[0] = strdup(damage[0]);
    wtype->damage[1] = strdup(damage[1]);
  }
  wtype->defmod = defmod;
  wtype->flags |= wflags;
  wtype->itype = itype;
  wtype->magres = magres;
  wtype->minskill = minskill;
  wtype->offmod = offmod;
  wtype->reload = reload;
  wtype->skill = sk;
  itype->rtype->wtype = wtype;

  return wtype;
}


armor_type *
new_armortype(item_type * itype, double penalty, double magres, int prot, unsigned int flags)
{
  armor_type * atype;

  assert(itype->rtype->atype==NULL);

  atype = calloc(sizeof(armor_type), 1);

  atype->itype = itype;
  atype->penalty = penalty;
  atype->magres = magres;
  atype->prot = prot;
  atype->flags = flags;
  itype->rtype->atype = atype;

  return atype;
}

static void
pt_register(potion_type * ptype)
{
  ptype->itype->rtype->ptype = ptype;
  ptype->next = potiontypes;
  potiontypes = ptype;
}

potion_type *
new_potiontype(item_type * itype,
               int level)
{
  potion_type * ptype;

  assert(resource2potion(itype->rtype)==NULL);

  ptype = calloc(sizeof(potion_type), 1);
  ptype->itype = itype;
  ptype->level = level;
  pt_register(ptype);

  return ptype;
}


void
rt_register(resource_type * rtype)
{
  resource_type ** prtype = &resourcetypes;

  if (!rtype->hashkey) {
    rtype->hashkey = hashstring(rtype->_name[0]);
  }
  while (*prtype && *prtype!=rtype) prtype=&(*prtype)->next;
  if (*prtype == NULL) {
    *prtype = rtype;
  }
}

const resource_type *
item2resource(const item_type * itype)
{
  return itype?itype->rtype:NULL;
}

const item_type *
resource2item(const resource_type * rtype)
{
  return rtype?rtype->itype:NULL;
}

const weapon_type *
resource2weapon(const resource_type * rtype) {
  return rtype->wtype;
}

const luxury_type *
resource2luxury(const resource_type * rtype)
{
#ifdef AT_LTYPE
  attrib * a = a_find(rtype->attribs, &at_ltype);
  if (a) return (const luxury_type *)a->data.v;
  return NULL;
#else
  return rtype->ltype;
#endif
}

const potion_type *
resource2potion(const resource_type * rtype)
{
#ifdef AT_PTYPE
  attrib * a = a_find(rtype->attribs, &at_ptype);
  if (a) return (const potion_type *)a->data.v;
  return NULL;
#else
  return rtype->ptype;
#endif
}

resource_type *
rt_find(const char * name)
{
  unsigned int hash = hashstring(name);
  resource_type * rtype;

  for (rtype=resourcetypes; rtype; rtype=rtype->next) {
    if (rtype->hashkey==hash && !strcmp(rtype->_name[0], name)) break;
  }

  return rtype;
}

static const char * it_aliases[][2] = {
  { "Runenschwert", "runesword" },
  { "p12", "truthpotion" },
  { "p1", "goliathwater" },
  { "p5", "peasantblood" },
  { "p8", "nestwarmth" },
  { NULL, NULL },
};

static const char *
it_alias(const char * zname)
{
  int i;
  for (i=0;it_aliases[i][0];++i) {
    if (strcmp(it_aliases[i][0], zname)==0) return it_aliases[i][1];
  }
  return zname;
}

item_type *
it_find(const char * zname)
{
  const char * name = it_alias(zname);
  unsigned int hash = hashstring(name);
  item_type * itype;
  unsigned int key = hash % IMAXHASH;

  for (itype=itemtypes[key]; itype; itype=itype->next) {
    if (itype->rtype->hashkey==hash && strcmp(itype->rtype->_name[0], name) == 0) {
      break;
    }
  }
  if (itype==NULL) {
    for (itype=itemtypes[key]; itype; itype=itype->next) {
      if (strcmp(itype->rtype->_name[1], name) == 0) break;
    }
  }
  return itype;
}

item **
i_find(item ** i, const item_type * it)
{
  while (*i && (*i)->type!=it) i = &(*i)->next;
  return i;
}


int
i_get(const item * i, const item_type * it)
{
  i = *i_find((item**)&i, it);
  if (i) return i->number;
  return 0;
}

item *
i_add(item ** pi, item * i)
{
  assert(i && i->type && !i->next);
  while (*pi) {
    int d = strcmp((*pi)->type->rtype->_name[0], i->type->rtype->_name[0]);
    if (d>=0) break;
    pi = &(*pi)->next;
  }
  if (*pi && (*pi)->type==i->type) {
    (*pi)->number += i->number;
    i_free(i);
  } else {
    i->next = *pi;
    *pi = i;
  }
  return *pi;
}

void
i_merge(item ** pi, item ** si)
{
  item * i = *si;
  while (i) {
    item * itmp;
    while (*pi) {
      int d = strcmp((*pi)->type->rtype->_name[0], i->type->rtype->_name[0]);
      if (d>=0) break;
      pi=&(*pi)->next;
    }
    if (*pi && (*pi)->type==i->type) {
      (*pi)->number += i->number;
      i_free(i_remove(&i, i));
    } else {
      itmp = i->next;
      i->next=*pi;
      *pi = i;
      i = itmp;
    }
  }
  *si=NULL;
}

item *
i_change(item ** pi, const item_type * itype, int delta)
{
  assert(itype);
  while (*pi) {
    int d = strcmp((*pi)->type->rtype->_name[0], itype->rtype->_name[0]);
    if (d>=0) break;
    pi=&(*pi)->next;
  }
  if (!*pi || (*pi)->type!=itype) {
    item * i;
    if (delta==0) return NULL;
    i = i_new(itype, delta);
    i->next = *pi;
    *pi = i;
  } else {
    item * i = *pi;
    i->number+=delta;
    assert(i->number >= 0);
    if (i->number==0) {
      *pi = i->next;
      i_free(i);
      return NULL;
    }
  }
  return *pi;
}

item *
i_remove(item ** pi, item * i)
{
  assert(i);
  while ((*pi)->type!=i->type) pi = &(*pi)->next;
  assert(*pi);
  *pi = i->next;
  i->next = NULL;
  return i;
}

static item * icache;
static int icache_size;
#define ICACHE_MAX 100

void
i_free(item * i)
{
  if (icache_size>=ICACHE_MAX) {
    free(i);
  } else {
    i->next = icache;
    icache = i;
    ++icache_size;
  }
}

void
i_freeall(item **i) {
  item *in;

  while(*i) {
    in = (*i)->next;
    i_free(*i);
    *i = in;
  }
}


item *
i_new(const item_type * itype, int size)
{
  item * i;
  if (icache_size>0) {
    i = icache;
    icache = i->next;
    --icache_size;
  } else {
    i = malloc(sizeof(item));
  }
  assert(itype);
  i->next = NULL;
  i->type = itype;
  i->number = size;
  return i;
}

#include "region.h"

static boolean
give_horses(const unit * s, const unit * d, const item_type * itype, int n, struct order * ord)
{
  if (d==NULL && itype == olditemtype[I_HORSE])
    rsethorses(s->region, rhorses(s->region) + n);
  return true;
}

#define R_MINOTHER R_SILVER
#define R_MINHERB R_PLAIN_1
#define R_MINPOTION R_FAST
#define R_MINITEM R_IRON
#define MAXITEMS MAX_ITEMS
#define MAXRESOURCES MAX_RESOURCES
#define MAXHERBS MAX_HERBS
#define MAXPOTIONS MAX_POTIONS
#define MAXHERBSPERPOTION 6
#define FIRSTLUXURY     (I_BALM)
#define LASTLUXURY      (I_INCENSE +1)
#define MAXLUXURIES (LASTLUXURY - FIRSTLUXURY)

const item_type * olditemtype[MAXITEMS+1];
const resource_type * oldresourcetype[MAXRESOURCES+1];
const potion_type * oldpotiontype[MAXPOTIONS+1];

/*** alte items ***/

int
get_item(const unit * u, item_t it)
{
  const item_type * type = olditemtype[it];
  item * i = *i_find((item**)&u->items, type);
  if (i) assert(i->number>=0);
  return i?i->number:0;
}

int
set_item(unit * u, item_t it, int value)
{
  const item_type * type = olditemtype[it];
  item * i = *i_find(&u->items, type);
  if (!i) {
    i = i_add(&u->items, i_new(type, value));
  } else {
    i->number = value;
  }
  return value;
}

void use_birthdayamulet(region * r, unit * magician, int amount, struct order * ord);

/* t_item::flags */
#define FL_ITEM_CURSED  (1<<0)
#define FL_ITEM_NOTLOST (1<<1)
#define FL_ITEM_NOTINBAG  (1<<2)  /* nicht im Bag Of Holding */
#define FL_ITEM_ANIMAL  (1<<3)  /* ist ein Tier */
#define FL_ITEM_MOUNT ((1<<4) | FL_ITEM_ANIMAL) /* ist ein Reittier */

/* ------------------------------------------------------------- */
/* Kann auch von Nichtmagier benutzt werden, modifiziert Taktik f�r diese
 * Runde um -1 - 4 Punkte. */
static void
use_tacticcrystal(region * r, unit * u, int amount, struct order * ord)
{
  int i;
  for (i=0;i!=amount;++i) {
    int duration = 1; /* wirkt nur eine Runde */
    int power = 5; /* Widerstand gegen Antimagiespr�che, ist in diesem
                      Fall egal, da der curse f�r den Kampf gelten soll,
                      der vor den Antimagiezaubern passiert */
    curse * c;
    variant effect;

    effect.i = rng_int()%6 - 1;
    c = create_curse(u, &u->attribs, ct_find("skillmod"), power,
      duration, effect, u->number);
    c->data.i = SK_TACTICS;
    unused(ord);
  }
  use_pooled(u, oldresourcetype[R_TACTICCRYSTAL], GET_DEFAULT, amount);
  ADDMSG(&u->faction->msgs, msg_message("use_tacticcrystal",
    "unit region", u, r));
  return;
}

typedef struct t_item {
  const char *name;
  /* [0]: Einzahl f�r eigene; [1]: Mehrzahl f�r eigene;
  * [2]: Einzahl f�r Fremde; [3]: Mehrzahl f�r Fremde */
  boolean is_resource;
  skill_t skill;
  int minskill;
  int gewicht;
  int preis;
  unsigned int flags;
  void (*benutze_funktion) (struct region *, struct unit *, int amount, struct order *);
} t_item;

const char * itemnames[MAXITEMS] = {
  "iron", "stone", "horse", "aoh",
  "aots", "roi", "rop", "ao_chastity",
  "laen", "fairyboot", "aoc", "pegasus",
  "elvenhorse", "dolphin", "roqf", "trollbelt",
  "presspass", "aurafocus", "sphereofinv", "magicbag",
  "magicherbbag", "dreameye"
};

#include "move.h"

static int
mod_elves_only(const unit * u, const region * r, skill_t sk, int value)
{
  if (u->race == new_race[RC_ELF]) return value;
  unused(r);
  return -118;
}

static void
init_olditems(void)
{
  item_t i;
#if 0
  resource_type * rtype;
  const struct locale * lang = find_locale("de");
  assert(lang);
#endif

  for (i=0; i!=MAXITEMS; ++i) {
    /* item is defined in XML file, but IT_XYZ enum still in use */
    const item_type * itype = it_find(itemnames[i]);

    assert(itype);
    olditemtype[i] = itype;
    oldresourcetype[i] = itype->rtype;
    continue;
  }
}

static int
heal(unit * user, int effect)
{
  int req = unit_max_hp(user) * user->number - user->hp;
  if (req>0) {
    req = min(req, effect);
    effect -= req;
    user->hp += req;
  }
  return effect;
}

static int
use_healingpotion(struct unit *user, const struct item_type *itype, int amount, struct order * ord)
{
  int effect = amount * 400;
  unit * u = user->region->units;
  effect = heal(user, effect);
  while (effect>0 && u!=NULL) {
    if (u->faction==user->faction) {
      effect = heal(u, effect);
    }
    u = u->next;
  }
  use_pooled(user, itype->rtype, GET_SLACK|GET_RESERVE|GET_POOLED_SLACK, amount);
  usetpotionuse(user, itype->rtype->ptype);

  ADDMSG(&user->faction->msgs, msg_message("usepotion",
    "unit potion", user, itype->rtype));
  return 0;
}

static int
use_warmthpotion(struct unit *u, const struct item_type *itype, int amount, struct order * ord)
{
  if (u->faction->race == new_race[RC_INSECT]) {
    fset(u, UFL_WARMTH);
  } else {
    /* nur f�r insekten: */
    cmistake(u, ord, 163, MSG_EVENT);
    return ECUSTOM;
  }
  use_pooled(u, itype->rtype, GET_SLACK|GET_RESERVE|GET_POOLED_SLACK, amount);
  usetpotionuse(u, itype->rtype->ptype);

  ADDMSG(&u->faction->msgs, msg_message("usepotion",
    "unit potion", u, itype->rtype));
  return 0;
}

static int
use_foolpotion(struct unit *u, int targetno, const struct item_type *itype, int amount, struct order * ord)
{
  unit * target = findunit(targetno);
  if (target==NULL || u->region!=target->region) {
    ADDMSG(&u->faction->msgs, msg_feedback(u, ord, "feedback_unit_not_found", ""));
    return ECUSTOM;
  }
  if (effskill(u, SK_STEALTH)<=effskill(target, SK_OBSERVATION)) {
    cmistake(u, ord, 64, MSG_EVENT);
    return ECUSTOM;
  }
  ADDMSG(&u->faction->msgs, msg_message("givedumb",
    "unit recipient amount", u, target, amount));

  change_effect(target, itype->rtype->ptype, amount);
  use_pooled(u, itype->rtype, GET_DEFAULT, amount);
  return 0;
}

static int
use_bloodpotion(struct unit *u, const struct item_type *itype, int amount, struct order * ord)
{
  if (u->race == new_race[RC_DAEMON]) {
    change_effect(u, itype->rtype->ptype, 100*amount);
  } else if (u->irace == u->race) {
    static race * rcfailure;
    if (!rcfailure) {
      rcfailure = rc_find("smurf");
      if (!rcfailure) rcfailure = rc_find("toad");
    }
    if (rcfailure) {
      trigger * trestore = trigger_changerace(u, u->race, u->irace);
      if (trestore) {
        int duration = 2 + rng_int() % 8;

        add_trigger(&u->attribs, "timer", trigger_timeout(duration, trestore));
        u->irace = u->race = rcfailure;
      }
    }
  }
  use_pooled(u, itype->rtype, GET_SLACK|GET_RESERVE|GET_POOLED_SLACK, amount);
  usetpotionuse(u, itype->rtype->ptype);

  ADDMSG(&u->faction->msgs, msg_message("usepotion",
    "unit potion", u, itype->rtype));
  return 0;
}

#include <attributes/fleechance.h>
static int
use_mistletoe(struct unit * user, const struct item_type * itype, int amount, struct order * ord)
{
  int mtoes = get_pooled(user, itype->rtype, GET_SLACK|GET_RESERVE|GET_POOLED_SLACK, user->number);

  if (user->number>mtoes) {
    ADDMSG(&user->faction->msgs, msg_message("use_singleperson",
                                             "unit item region command", user, itype->rtype, user->region, ord));
    return -1;
  }
  use_pooled(user, itype->rtype, GET_SLACK|GET_RESERVE|GET_POOLED_SLACK, user->number);
  a_add(&user->attribs, make_fleechance((float)1.0));
  ADDMSG(&user->faction->msgs,
         msg_message("use_item", "unit item", user, itype->rtype));

  return 0;
}

static int
use_magicboost(struct unit * user, const struct item_type * itype, int amount, struct order * ord)
{
  int mtoes = get_pooled(user, itype->rtype, GET_SLACK|GET_RESERVE|GET_POOLED_SLACK, user->number);
  faction * f = user->faction;
  if (user->number>mtoes) {
    ADDMSG(&user->faction->msgs, msg_message("use_singleperson",
      "unit item region command", user, itype->rtype, user->region, ord));
    return -1;
  }
  if (!is_mage(user) || find_key(f->attribs, atoi36("mbst"))!=NULL) {
    cmistake(user, user->thisorder, 214, MSG_EVENT);
    return -1;
  }
  use_pooled(user, itype->rtype, GET_SLACK|GET_RESERVE|GET_POOLED_SLACK, user->number);

  a_add(&f->attribs, make_key(atoi36("mbst")));
  set_level(user, sk_find("magic"), 3);

  ADDMSG(&user->faction->msgs, msg_message("use_item",
    "unit item", user, itype->rtype));

  return 0;
}

static int
use_snowball(struct unit * user, const struct item_type * itype, int amount, struct order * ord)
{
  return 0;
}

static void
init_oldpotions(void)
{
  const char * potionnames[MAX_POTIONS] = {
    "p0", "goliathwater", "p2", "p3", "p4", "peasantblood", "p6",
    "p7", "nestwarmth", "p9", "p10", "p11", "truthpotion", "p13",  "p14"
  };
  int p;

  for (p=0;p!=MAXPOTIONS;++p) {
    item_type * itype = it_find(potionnames[p]);
    if (itype!=NULL) {
      oldpotiontype[p] = itype->rtype->ptype;
    }
  }
}

resource_type * r_silver;
resource_type * r_aura;
resource_type * r_permaura;
resource_type * r_unit;
resource_type * r_hp;

resource_type * r_silver;
item_type * i_silver;

static const char * names[] = {
  "money", "money_p",
  "person", "person_p",
  "permaura", "permaura_p",
  "hp", "hp_p",
  "peasant", "peasant_p",
  "aura", "aura_p",
  "unit", "unit_p"
};

void
init_resources(void)
{
  static boolean initialized = false;
  if (initialized) return;
  initialized = true;

  /* silver was never an item: */
  r_silver = new_resourcetype(&names[0], NULL, RTF_ITEM|RTF_POOLED);
  i_silver = new_itemtype(r_silver, ITF_NONE, 1/*weight*/, 0);
  r_silver->uchange = res_changeitem;

  r_permaura = new_resourcetype(&names[4], NULL, RTF_NONE);
  r_permaura->uchange = res_changepermaura;

  r_hp = new_resourcetype(&names[6], NULL, RTF_NONE);
  r_hp->uchange = res_changehp;

  r_aura = new_resourcetype(&names[10], NULL, RTF_NONE);
  r_aura->uchange = res_changeaura;

  r_unit = new_resourcetype(&names[12], NULL, RTF_NONE);
  r_unit->uchange = res_changeperson;

  oldresourcetype[R_SILVER] = r_silver;
  oldresourcetype[R_AURA] = r_aura;
  oldresourcetype[R_PERMAURA] = r_permaura;

  /* alte typen registrieren: */
  init_olditems();
  init_oldpotions();
}

int
get_money(const unit * u)
{
  const item * i = u->items;
  while (i && i->type!=i_silver) i=i->next;
  if (i==NULL) return 0;
  return i->number;
}

int
set_money(unit * u, int v)
{
  item ** ip = &u->items;
  while (*ip && (*ip)->type!=i_silver) ip = &(*ip)->next;
  if ((*ip)==NULL && v) {
    i_add(&u->items, i_new(i_silver, v));
    return v;
  }
  if ((*ip)!=NULL) {
    if (v) (*ip)->number = v;
    else i_remove(ip, *ip);
  }
  return v;
}

int
change_money(unit * u, int v)
{
  item ** ip = &u->items;
  while (*ip && (*ip)->type!=i_silver) ip = &(*ip)->next;
  if ((*ip)==NULL && v) {
    i_add(&u->items, i_new(i_silver, v));
    return v;
  }
  if ((*ip)!=NULL) {
    item * i = *ip;
    if (i->number + v != 0) {
      i->number += v;
      return i->number;
    }
    else i_free(i_remove(ip, *ip));
  }
  return 0;
}


static local_names * rnames;

const resource_type *
findresourcetype(const char * name, const struct locale * lang)
{
  local_names * rn = rnames;
  variant token;

  while (rn) {
    if (rn->lang==lang) break;
    rn = rn->next;
  }
  if (!rn) {
    const resource_type * rtl = resourcetypes;
    rn = calloc(sizeof(local_names), 1);
    rn->next = rnames;
    rn->lang = lang;
    while (rtl) {
      token.v = (void*)rtl;
      addtoken(&rn->names, locale_string(lang, rtl->_name[0]), token);
      addtoken(&rn->names, locale_string(lang, rtl->_name[1]), token);
      rtl=rtl->next;
    }
    rnames = rn;
  }

  if (findtoken(&rn->names, name, &token)==E_TOK_NOMATCH) return NULL;
  return (const resource_type*)token.v;
}

attrib_type at_showitem = {
  "showitem"
};

static local_names * inames;

void
init_itemnames(void)
{
  int i;
  for (i=0;localenames[i];++i) {
    const struct locale * lang = find_locale(localenames[i]);
    boolean exist = false;
    local_names * in = inames;

    while (in!=NULL) {
      if (in->lang==lang) {
        exist = true;
        break;
      }
      in = in->next;
    }
    if (in==NULL) in = calloc(sizeof(local_names), 1);
    in->next = inames;
    in->lang = lang;

    if (!exist) {
      int key;
      for (key=0;key!=IMAXHASH;++key) {
        const item_type * itl;
        for (itl=itemtypes[key];itl;itl=itl->next) {
          variant var;
          const char * iname = locale_string(lang, itl->rtype->_name[0]);
          if (findtoken(&in->names, iname, &var)==E_TOK_NOMATCH || var.v!=itl) {
            var.v = (void*)itl;
            addtoken(&in->names, iname, var);
            addtoken(&in->names, locale_string(lang, itl->rtype->_name[1]), var);
          }
        }
      }
    }
    inames = in;
  }
}

const item_type *
finditemtype(const char * name, const struct locale * lang)
{
  local_names * in = inames;
  variant var;

  while (in!=NULL) {
    if (in->lang==lang) break;
    in=in->next;
  }
  if (in==NULL) {
    init_itemnames();
    for (in=inames;in->lang!=lang;in=in->next) ;
  }
  if (findtoken(&in->names, name, &var)==E_TOK_NOMATCH) return NULL;
  return (const item_type*)var.v;
}

static void
init_resourcelimit(attrib * a)
{
  a->data.v = calloc(sizeof(resource_limit), 1);
}

static void
finalize_resourcelimit(attrib * a)
{
  free(a->data.v);
}

attrib_type at_resourcelimit = {
  "resourcelimit",
  init_resourcelimit,
  finalize_resourcelimit,
};

void
register_resources(void)
{
  register_function((pf_generic)mod_elves_only, "mod_elves_only");
  register_function((pf_generic)res_changeitem, "changeitem");
  register_function((pf_generic)res_changeperson, "changeperson");
  register_function((pf_generic)res_changepeasants, "changepeasants");
  register_function((pf_generic)res_changepermaura, "changepermaura");
  register_function((pf_generic)res_changehp, "changehp");
  register_function((pf_generic)res_changeaura, "changeaura");

  register_function((pf_generic)use_potion, "usepotion");
  register_function((pf_generic)use_tacticcrystal, "use_tacticcrystal");
  register_function((pf_generic)use_birthdayamulet, "use_birthdayamulet");
  register_function((pf_generic)use_warmthpotion, "usewarmthpotion");
  register_function((pf_generic)use_bloodpotion, "usebloodpotion");
  register_function((pf_generic)use_healingpotion, "usehealingpotion");
  register_function((pf_generic)use_foolpotion, "usefoolpotion");
  register_function((pf_generic)use_mistletoe, "usemistletoe");
  register_function((pf_generic)use_magicboost, "usemagicboost");
  register_function((pf_generic)use_snowball, "usesnowball");

  register_function((pf_generic)give_horses, "givehorses");

  /* make sure noone has deleted an I_ tpe without deleting the R_ type that goes with it! */
  assert((int)I_TACTICCRYSTAL == (int)R_TACTICCRYSTAL);
}
