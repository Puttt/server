/* vi: set ts=2:
 *
 *
 *  Eressea PB(E)M host Copyright (C) 1998-2003
 *      Christian Schlittchen (corwin@amber.kn-bremen.de)
 *      Katja Zedel (katze@felidae.kn-bremen.de)
 *      Henning Peters (faroul@beyond.kn-bremen.de)
 *      Enno Rehling (enno@eressea-pbem.de)
 *      Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
 *
 * This program may not be used, modified or distributed without
 * prior permission by the authors of Eressea.
 */

#include <config.h>
#include <eressea.h>
#include "creport.h"

/* tweakable features */
#define ENCODE_SPECIAL 1
#define RENDER_CRMESSAGES
#define BUFFERSIZE 32768
/* modules include */
#include <modules/score.h>

/* attributes include */
#include <attributes/follow.h>
#include <attributes/racename.h>
#include <attributes/orcification.h>
#include <attributes/otherfaction.h>
#include <attributes/raceprefix.h>

/* gamecode includes */
#include "laws.h"
#include "economy.h"

/* kernel includes */
#include <kernel/alchemy.h>
#include <kernel/alliance.h>
#include <kernel/border.h>
#include <kernel/building.h>
#include <kernel/faction.h>
#include <kernel/group.h>
#include <kernel/item.h>
#include <kernel/karma.h>
#include <kernel/magic.h>
#include <kernel/message.h>
#include <kernel/move.h>
#include <kernel/order.h>
#include <kernel/plane.h>
#include <kernel/race.h>
#include <kernel/region.h>
#include <kernel/reports.h>
#include <kernel/resources.h>
#include <kernel/ship.h>
#include <kernel/skill.h>
#include <kernel/teleport.h>
#include <kernel/terrain.h>
#include <kernel/unit.h>
#include <kernel/save.h>

/* util includes */
#include <util/attrib.h>
#include <util/base36.h>
#include <util/crmessage.h>
#include <util/goodies.h>
#include <util/language.h>
#include <util/log.h>
#include <util/message.h>
#include <util/nrmessage.h>

/* libc includes */
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* imports */
extern const char *directions[];
extern const char *spelldata[];
extern int quiet;
boolean opt_cr_absolute_coords = false;

/* globals */
#define C_REPORT_VERSION 64

#define TAG_LOCALE "de"
#ifdef TAG_LOCALE
static const char *
crtag(const char * key)
{
  static const struct locale * lang = NULL;
  if (!lang) lang = find_locale(TAG_LOCALE);
  return locale_string(lang, key);
}
#else
#define crtag(x) (x)
#endif
/*
 * translation table
 */
typedef struct translation {
  struct translation * next;
  char * key;
  const char * value;
} translation;

#define TRANSMAXHASH 257
static translation * translation_table[TRANSMAXHASH];
static translation * junkyard;

static const char *
add_translation(const char * key, const char * value)
{
  int kk = ((key[0] << 5) + key[0]) % TRANSMAXHASH;
  translation * t = translation_table[kk];
  while (t && strcmp(t->key, key)!=0) t=t->next;
  if (!t) {
    if (junkyard) {
      t = junkyard;
      junkyard = junkyard->next;
    } else t = malloc(sizeof(translation));
    t->key = strdup(key);
    t->value = value;
    t->next = translation_table[kk];
    translation_table[kk] = t;
  }
  return crtag(key);
}

static void
write_translations(FILE * F)
{
  int i;
  fputs("TRANSLATION\n", F);
  for (i=0;i!=TRANSMAXHASH;++i) {
    translation * t = translation_table[i];
    while (t) {
      fprintf(F, "\"%s\";%s\n", t->value, crtag(t->key));
      t = t->next;
    }
  }
}

static void
reset_translations(void)
{
  int i;
  for (i=0;i!=TRANSMAXHASH;++i) {
    translation * t = translation_table[i];
    while (t) {
      translation * c = t->next;
      free(t->key);
      t->next = junkyard;
      junkyard = t;
      t = c;
    }
    translation_table[i] = 0;
  }
}

#include "objtypes.h"

static void
print_items(FILE * F, item * items, const struct locale * lang)
{
  item * itm;

  for (itm=items; itm; itm=itm->next) {
    int in = itm->number;
    const char * ic = resourcename(itm->type->rtype, 0);
    if (itm==items) fputs("GEGENSTAENDE\n", F);
    fprintf(F, "%d;%s\n", in, add_translation(ic, LOC(lang, ic)));
  }
}

static void
print_curses(FILE * F, const faction * viewer, const void * obj, typ_t typ)
{
  boolean header = false;
  attrib *a = NULL;
  int self = 0;
  region *r;

  /* Die Sichtbarkeit eines Zaubers und die Zaubermeldung sind bei
   * Gebäuden und Schiffen je nach, ob man Besitzer ist, verschieden.
   * Bei Einheiten sieht man Wirkungen auf eigene Einheiten immer.
   * Spezialfälle (besonderes Talent, verursachender Magier usw. werde
   * bei jedem curse gesondert behandelt. */
  if (typ == TYP_SHIP){
    ship * sh = (ship*)obj;
    unit * owner = shipowner(sh);
    a = sh->attribs;
    r = sh->region;
    if (owner != NULL) {
      if (owner->faction == viewer){
        self = 2;
      } else { /* steht eine person der Partei auf dem Schiff? */
        unit *u = NULL;
        for (u = r->units; u; u = u->next) {
          if (u->ship == sh) {
            self = 1;
            break;
          }
        }
      }
    }
  } else if (typ == TYP_BUILDING) {
    building * b = (building*)obj;
    unit * owner;
    a = b->attribs;
    r = b->region;
    if((owner = buildingowner(r,b)) != NULL){
      if (owner->faction == viewer){
        self = 2;
      } else { /* steht eine Person der Partei in der Burg? */
        unit *u = NULL;
        for (u = r->units; u; u = u->next) {
          if (u->building == b) {
            self = 1;
            break;
          }
        }
      }
    }
  } else if (typ == TYP_UNIT) {
    unit *u = (unit *)obj;
    a = u->attribs;
    r = u->region;
    if (u->faction == viewer) {
      self = 2;
    }
  } else if (typ == TYP_REGION) {
    r = (region *)obj;
    a = r->attribs;
  } else {
    /* fehler */
  }

  while (a) {
    if (fval(a->type, ATF_CURSE)) {
      curse * c = (curse *)a->data.v;
      message * msg;

      if (c->type->cansee) {
        self = c->type->cansee(viewer, obj, typ, c, self);
      }
      msg = msg_curse(c, obj, typ, self);

      if (msg) {
        char buf[BUFFERSIZE];
        if (!header) {
          header = 1;
          fputs("EFFECTS\n", F);
        }
        nr_render(msg, viewer->locale, buf, sizeof(buf), viewer);
        fprintf(F, "\"%s\"\n", buf);
        msg_release(msg);
      }
    } else if (a->type==&at_effect && self) {
      effect_data * data = (effect_data *)a->data.v;
      if (data->value>0) {
        const char * key = resourcename(data->type->itype->rtype, 0);
        if (!header) {
          header = 1;
          fputs("EFFECTS\n", F);
        }
        fprintf(F, "\"%d %s\"\n", data->value, add_translation(key, locale_string(default_locale, key)));
      }
    }
    a = a->next;
  }
}

static int
cr_unit(variant var, char * buffer, const void * userdata)
{
  const faction * report = (const faction*)userdata;
  unit * u = (unit *)var.v;
  sprintf(buffer, "%d", u?u->no:-1);
  unused(report);
  return 0;
}

static int
cr_ship(variant var, char * buffer, const void * userdata)
{
  const faction * report = (const faction*)userdata;
  ship * u = (ship *)var.v;
  sprintf(buffer, "%d", u?u->no:-1);
  unused(report);
  return 0;
}

static int
cr_building(variant var, char * buffer, const void * userdata)
{
  const faction * report = (const faction*)userdata;
  building * u = (building *)var.v;
  sprintf(buffer, "%d", u?u->no:-1);
  unused(report);
  return 0;
}

static int
cr_faction(variant var, char * buffer, const void * userdata)
{
  const faction * report = (const faction*)userdata;
  faction * f = (faction *)var.v;
  sprintf(buffer, "%d", f?f->no:-1);
  unused(report);
  return 0;
}

static int
cr_region(variant var, char * buffer, const void * userdata)
{
  const faction * report = (const faction*)userdata;
  region * r = (region *)var.v;
  if (r) {
    plane * p = r->planep;
    if (!p || !(p->flags & PFL_NOCOORDS)) {
      sprintf(buffer, "%d %d %d", region_x(r, report), region_y(r, report), p?p->id:0);
      return 0;
    }
  }
  return -1;
}

static int
cr_resource(variant var, char * buffer, const void * userdata)
{
  const faction * report = (const faction*)userdata;
  const resource_type * r = (const resource_type *)var.v;
  if (r) {
    const char * key = resourcename(r, 0);
    sprintf(buffer, "\"%s\"",
      add_translation(key, locale_string(report->locale, key)));
    return 0;
  }
  return -1;
}

static int
cr_race(variant var, char * buffer, const void * userdata)
{
  const faction * report = (const faction*)userdata;
  const struct race * rc = (const race *)var.v;
  const char * key = rc_name(rc, 0);
  sprintf(buffer, "\"%s\"",
    add_translation(key, locale_string(report->locale, key)));
  return 0;
}

static int
cr_alliance(variant var, char * buffer, const void * userdata)
{
  const alliance * al = (const alliance *)var.v;
  if (al!=NULL) {
    sprintf(buffer, "%d", al->id);
  }
  unused(userdata);
  return 0;
}

static int
cr_skill(variant var, char * buffer, const void * userdata)
{
  const faction * report = (const faction*)userdata;
  skill_t sk = (skill_t)var.i;
  if (sk!=NOSKILL) sprintf(buffer, "\"%s\"",
    add_translation(skillnames[sk], skillname(sk, report->locale)));
  else strcpy(buffer, "\"\"");
  return 0;
}

static int
cr_order(variant var, char * buffer, const void * userdata)
{
  order * ord = (order*)var.v;
  if (ord!=NULL) {
    char * wp = buffer;
    char * cmd = getcommand(ord);
    const char * rp = cmd;

    *wp++ = '\"';
    while (*rp) {
      switch (*rp) {
      case '\"':
      case '\\':
        *wp++ = '\\';
      default:
        *wp++ = *rp++;
      }
    }
    *wp++ = '\"';
    *wp++ = 0;

    free(cmd);
  }
  else strcpy(buffer, "\"\"");
  return 0;
}

static int
cr_resources(variant var, char * buffer, const void * userdata)
{
  resource * rlist = (resource*)var.v;
  char * wp = buffer;
  if (rlist!=NULL) {
    wp += sprintf(wp, "\"%d %s", rlist->number,
      resourcename(rlist->type, rlist->number!=1));
    for (;;) {
      rlist = rlist->next;
      if (rlist==NULL) break;
      wp += sprintf(wp, ", %d %s", rlist->number,
        resourcename(rlist->type, rlist->number!=1));
    }
    strcat(wp, "\"");
  }
  return 0;
}

static int
cr_regions(variant var, char * buffer, const void * userdata)
{
  const arg_regions * rdata = (const arg_regions *)var.v;
  char * wp = buffer;
  if (rdata!=NULL && rdata->nregions>0) {
    region * r = rdata->regions[0];
    int i, z = r->planep?r->planep->id:0;
    wp += sprintf(wp, "\"%d %d %d", r->x, r->y, z);
    for (i=1;i!=rdata->nregions;++i) {
      r = rdata->regions[i];
      z = r->planep?r->planep->id:0;
      wp += sprintf(wp, ", %d %d %d", r->x, r->y, z);
    }
    strcat(wp, "\"");
  }
  return 0;
}

static int
cr_spell(variant var, char * buffer, const void * userdata)
{
  const faction * report = (const faction*)userdata;
  spell * sp = (spell*)var.v;
  if (sp!=NULL) sprintf(buffer, "\"%s\"", spell_name(sp, report->locale));
  else strcpy(buffer, "\"\"");
  return 0;
}

static int
cr_curse(variant var, char * buffer, const void * userdata)
{
  const faction * report = (const faction*)userdata;
  const curse_type * ctype = (const curse_type*)var.v;
  if (ctype!=NULL) {
    sprintf(buffer, "\"%s\"", curse_name(ctype, report->locale));
  } else strcpy(buffer, "\"\"");
  return 0;
}

/*static int msgno; */

#define MTMAXHASH 1021

static struct known_mtype {
  const struct message_type * mtype;
  struct known_mtype * nexthash;
} * mtypehash[MTMAXHASH];

static void
report_crtypes(FILE * F, const struct locale* lang)
{
  int i;
  for (i=0;i!=MTMAXHASH;++i) {
    struct known_mtype * kmt;
    for (kmt=mtypehash[i];kmt;kmt=kmt->nexthash) {
      const struct nrmessage_type * nrt = nrt_find(lang, kmt->mtype);
      if (nrt) {
        unsigned int hash = kmt->mtype->key;
        fprintf(F, "MESSAGETYPE %d\n", hash);
        fputc('\"', F);
        fputs(escape_string(nrt_string(nrt), NULL, 0), F);
        fputs("\";text\n", F);
        fprintf(F, "\"%s\";section\n", nrt_section(nrt));
      }
    }
    while (mtypehash[i]) {
      kmt = mtypehash[i];
      mtypehash[i] = mtypehash[i]->nexthash;
      free(kmt);
    }
  }
}

static unsigned int
messagehash(const struct message * msg)
{
  variant var;
  var.v = (void *)msg;
  return (unsigned int)var.i;
}

static void
render_messages(FILE * F, faction * f, message_list *msgs)
{
  struct mlist* m = msgs->begin;
  while (m) {
    char crbuffer[BUFFERSIZE]; /* gross, wegen spionage-messages :-( */
    boolean printed = false;
    const struct message_type * mtype = m->msg->type;
    unsigned int hash = mtype->key;
#ifdef RENDER_CRMESSAGES
    char nrbuffer[1024*32];
    nrbuffer[0] = '\0';
    if (nr_render(m->msg, f->locale, nrbuffer, sizeof(nrbuffer), f)>0 && nrbuffer[0]) {
      fprintf(F, "MESSAGE %u\n", messagehash(m->msg));
      fprintf(F, "%d;type\n", hash);
      fwritestr(F, nrbuffer);
      fputs(";rendered\n", F);
      printed = true;
    }
#endif
    crbuffer[0] = '\0';
    if (cr_render(m->msg, crbuffer, (const void*)f)==0) {
      if (!printed) fprintf(F, "MESSAGE %u\n", messagehash(m->msg));
      if (crbuffer[0]) fputs(crbuffer, F);
    } else {
      log_error(("could not render cr-message %p: %s\n", m->msg, m->msg->type->name));
    }
    if (printed) {
      unsigned int ihash = hash % MTMAXHASH;
      struct known_mtype * kmt = mtypehash[ihash];
      while (kmt && kmt->mtype != mtype) kmt = kmt->nexthash;
      if (kmt==NULL) {
        kmt = (struct known_mtype*)malloc(sizeof(struct known_mtype));
        kmt->nexthash = mtypehash[ihash];
        kmt->mtype = mtype;
        mtypehash[ihash] = kmt;
      }
    }
    m = m->next;
  }
}

static void
cr_output_messages(FILE * F, message_list *msgs, faction * f)
{
  if (msgs) render_messages(F, f, msgs);
}

/* prints a building */
static void
cr_output_buildings(FILE * F, building * b, const unit * owner, int fno, faction *f)
{
  const char * bname;
  static const struct building_type * bt_illusion;
  const building_type * type = b->type;

  if (!bt_illusion) bt_illusion = bt_find("illusion");

  fprintf(F, "BURG %d\n", b->no);

  if (b->type==bt_illusion) {
    const attrib * a = a_findc(b->attribs, &at_icastle);
    if (a!=NULL) {
      type = ((icastle_data*)a->data.v)->type;
    }
    bname = buildingtype(b->type, b, b->size);
    if (owner!=NULL && owner->faction==f) {
      fprintf(F, "\"%s\";wahrerTyp\n", add_translation(bname, LOC(f->locale, bname)));
    }
  }
  bname = buildingtype(type, b, b->size);
  fprintf(F, "\"%s\";Typ\n", add_translation(bname, LOC(f->locale, bname)));
  fprintf(F, "\"%s\";Name\n", b->name);
  if (b->display && b->display[0])
    fprintf(F, "\"%s\";Beschr\n", b->display);
  if (b->size)
    fprintf(F, "%d;Groesse\n", b->size);
  if (owner)
    fprintf(F, "%d;Besitzer\n", owner ? owner->no : -1);
  if (fno >= 0)
    fprintf(F, "%d;Partei\n", fno);
  if (b->besieged)
    fprintf(F, "%d;Belagerer\n", b->besieged);
  print_curses(F, f, b, TYP_BUILDING);
}
/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  */

/* prints a ship */
static void
cr_output_ship(FILE * F, const ship * sh, const unit * u, int fcaptain, const faction * f, const region * r)
{
  int w = 0;
  assert(sh);
  fprintf(F, "SCHIFF %d\n", sh->no);
  fprintf(F, "\"%s\";Name\n", sh->name);
  if (sh->display && sh->display[0])
    fprintf(F, "\"%s\";Beschr\n", sh->display);
  fprintf(F, "\"%s\";Typ\n", add_translation(sh->type->name[0], locale_string(f->locale, sh->type->name[0])));
  fprintf(F, "%d;Groesse\n", sh->size);
  if (sh->damage) {
    int percent = (sh->damage*100+DAMAGE_SCALE-1)/(sh->size*DAMAGE_SCALE);
    fprintf(F, "%d;Schaden\n", percent);
  }
  if (u)
    fprintf(F, "%d;Kapitaen\n", u ? u->no : -1);
  if (fcaptain >= 0)
    fprintf(F, "%d;Partei\n", fcaptain);

  /* calculate cargo */
  if (u && (u->faction == f || omniscient(f))) {
    int n = 0, p = 0, c = shipcapacity(sh);
    getshipweight(sh, &n, &p);

    fprintf(F, "%d;cargo\n", n);
    fprintf(F, "%d;capacity\n", c);

    n = (n+99) / 100; /* 1 Silber = 1 GE */
    fprintf(F, "%d;Ladung\n", n);
    fprintf(F, "%d;MaxLadung\n", c / 100);
  }
  /* shore */
  w = NODIRECTION;
  if (!fval(r->terrain, SEA_REGION)) w = sh->coast;
  if (w != NODIRECTION)
    fprintf(F, "%d;Kueste\n", w);

  print_curses(F, f, sh, TYP_SHIP);
}
/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  */

/* prints all that belongs to a unit */
static void
cr_output_unit(FILE * F, const region * r,
         const faction * f, /* observers faction */
         const unit * u, int mode)
{
  /* Race attributes are always plural and item attributes always
   * singular */
  const char * str;
  const item_type * lasttype;
  int pr;
  item *itm, *show;
  building * b;
  const char * pzTmp;
  skill * sv;
  const attrib *a_fshidden = NULL;
  boolean itemcloak = false;
  static const curse_type * itemcloak_ct = 0;
  static boolean init = false;

  if (!init) {
    init = true;
    itemcloak_ct = ct_find("itemcloak");
  }
  if (itemcloak_ct!=NULL) {
    itemcloak = curse_active(get_curse(u->attribs, itemcloak_ct));
  }

  assert(u);

#ifdef KARMA_MODULE
  if (fspecial(u->faction, FS_HIDDEN))
    a_fshidden = a_find(u->attribs, &at_fshidden);
#endif /* KARMA_MODULE */

  fprintf(F, "EINHEIT %d\n", u->no);
  fprintf(F, "\"%s\";Name\n", u->name);
  str = u_description(u, f->locale);
  if (str) {
    fprintf(F, "\"%s\";Beschr\n", str);
  }
  {
    /* print faction information */
    const faction * sf = visible_faction(f, u);
    const char * prefix = raceprefix(u);
    if (u->faction == f || omniscient(f)) {
      const attrib * a_otherfaction = a_find(u->attribs, &at_otherfaction);
      const faction * otherfaction = a_otherfaction?get_otherfaction(a_otherfaction):NULL;
      /* my own faction, full info */
      const attrib *a = NULL;
      unit * mage;

      if (fval(u, UFL_GROUP)) a = a_find(u->attribs, &at_group);
      if (a!=NULL) {
        const group * g = (const group*)a->data.v;
        fprintf(F, "%d;gruppe\n", g->gid);
      }
      fprintf(F, "%d;Partei\n", u->faction->no);
      if (sf!=u->faction) fprintf(F, "%d;Verkleidung\n", sf->no);
      if (fval(u, UFL_PARTEITARNUNG))
        fprintf(F, "%d;Parteitarnung\n", i2b(fval(u, UFL_PARTEITARNUNG)));
      if (otherfaction) {
        if (otherfaction!=u->faction) {
          fprintf(F, "%d;Anderepartei\n", otherfaction->no);
        }
      }
      mage = get_familiar_mage(u);
      if (mage) {
        fprintf(F, "%u;familiarmage\n", mage->no);
      }
    } else {
      if (fval(u, UFL_PARTEITARNUNG)) {
        /* faction info is hidden */
        fprintf(F, "%d;Parteitarnung\n", i2b(fval(u, UFL_PARTEITARNUNG)));
      } else {
        const attrib * a_otherfaction = a_find(u->attribs, &at_otherfaction);
        const faction * otherfaction = a_otherfaction?get_otherfaction(a_otherfaction):NULL;
        /* other unit. show visible faction, not u->faction */
        fprintf(F, "%d;Partei\n", sf->no);
        if (sf == f) {
          fprintf(F, "1;Verraeter\n");
        }
        if (a_otherfaction) {
          if (otherfaction!=u->faction) {
            if (alliedunit(u, f, HELP_FSTEALTH)) {
              fprintf(F, "%d;Anderepartei\n", otherfaction->no);
            }
          }
        }
      }
    }
    if (prefix) {
      prefix = mkname("prefix", prefix);
      fprintf(F, "\"%s\";typprefix\n", add_translation(prefix, LOC(f->locale, prefix)));
    }
  }
  if (u->faction != f && a_fshidden
      && a_fshidden->data.ca[0] == 1 && effskill(u, SK_STEALTH) >= 6) {
    fprintf(F, "-1;Anzahl\n");
  } else {
    fprintf(F, "%d;Anzahl\n", u->number);
  }

  pzTmp = get_racename(u->attribs);
  if (pzTmp) {
    fprintf(F, "\"%s\";Typ\n", pzTmp);
    if (u->faction==f && fval(u->race, RCF_SHAPESHIFTANY)) {
      const char * zRace = rc_name(u->race, 1);
      fprintf(F, "\"%s\";wahrerTyp\n",
        add_translation(zRace, locale_string(f->locale, zRace)));
    }
  } else {
    const char * zRace = rc_name(u->irace, 1);
    fprintf(F, "\"%s\";Typ\n",
      add_translation(zRace, locale_string(f->locale, zRace)));
    if (u->faction==f && u->irace!=u->race) {
      zRace = rc_name(u->race, 1);
      fprintf(F, "\"%s\";wahrerTyp\n",
        add_translation(zRace, locale_string(f->locale, zRace)));
    }
  }

  if (u->building)
    fprintf(F, "%d;Burg\n", u->building->no);
  if (u->ship)
    fprintf(F, "%d;Schiff\n", u->ship->no);
  if (getguard(u))
    fprintf(F, "%d;bewacht\n", getguard(u)?1:0);
  if ((b=usiege(u))!=NULL)
    fprintf(F, "%d;belagert\n", b->no);

  /* additional information for own units */
  if (u->faction == f || omniscient(f)) {
    order * ord;
    const char *xc;
    const char * c;
    int i;

    i = ualias(u);
    if (i>0)
      fprintf(F, "%d;temp\n", i);
    else if (i<0)
      fprintf(F, "%d;alias\n", -i);
    i = get_money(u);
    fprintf(F, "%d;Kampfstatus\n", u->status);
    fprintf(F, "%d;weight\n", weight(u));
    if (fval(u, UFL_NOAID)) {
      fputs("1;unaided\n", F);
    }
    if (fval(u, UFL_STEALTH)) {
      i = u_geteffstealth(u);
      if (i >= 0) {
        fprintf(F, "%d;Tarnung\n", i);
      }
    }
    xc = uprivate(u);
    if (xc) {
      fprintf(F, "\"%s\";privat\n", xc);
    }
    c = hp_status(u);
    if (c && *c && (u->faction == f || omniscient(f))) {
      fprintf(F, "\"%s\";hp\n", add_translation(c, locale_string(u->faction->locale, c)));
    }
#ifdef HEROES
    if (fval(u, UFL_HERO)) {
      fputs("1;hero\n", F);
    }
#endif

    if (fval(u, UFL_HUNGER) && (u->faction == f)) {
      fputs("1;hunger\n", F);
    }
    if (is_mage(u)) {
      fprintf(F, "%d;Aura\n", get_spellpoints(u));
      fprintf(F, "%d;Auramax\n", max_spellpoints(u->region,u));
    }
    /* default commands */
    fprintf(F, "COMMANDS\n");
#ifdef LASTORDER
    if (u->lastorder) {
      fwriteorder(F, u->lastorder, f->locale);
      fputc('\n', F);
    }
#else
    for (ord = u->old_orders; ord; ord = ord->next) {
      /* this new order will replace the old defaults */
      if (is_persistent(ord)) {
        fwriteorder(F, ord, f->locale);
        fputc('\n', F);
      }
    }
#endif
    for (ord = u->orders; ord; ord = ord->next) {
#ifdef LASTORDER
      if (ord==u->lastorder) continue;
#endif
      if (u->old_orders && is_repeated(ord)) continue; /* unit has defaults */
      if (is_persistent(ord)) {
        fwriteorder(F, ord, f->locale);
        fputc('\n', F);
      }
    }

    /* talents */
    pr = 0;
    for (sv = u->skills; sv != u->skills + u->skill_size; ++sv) {
      if (sv->level>0) {
        skill_t sk = sv->id;
        int esk = eff_skill(u, sk, r);
        if (!pr) {
          pr = 1;
          fprintf(F, "TALENTE\n");
        }
        fprintf(F, "%d %d;%s\n", u->number*level_days(sv->level), esk,
          add_translation(skillnames[sk], skillname(sk, f->locale)));
      }
    }
    /* spells */
    if (is_mage(u)) {
      sc_mage * mage = get_mage(u);
      spell_list * slist = mage->spells;
      if (slist) {
        int i;
        int t = effskill(u, SK_MAGIC);
        fprintf(F, "SPRUECHE\n");
        for (;slist; slist = slist->next) {
          spell * sp = slist->data;
          if (sp->level <= t) {
            const char * name = add_translation(mkname("spell", sp->sname), spell_name(sp, f->locale));
            fprintf(F, "\"%s\"\n", name);
          }
        }
        for (i=0;i!=MAXCOMBATSPELLS;++i) {
          const spell * sp = mage->combatspells[i].sp;
          if (sp) {
            const char * name = add_translation(mkname("spell", sp->sname), spell_name(sp, f->locale));
            fprintf(F, "KAMPFZAUBER %d\n", i);
            fprintf(F, "\"%s\";name\n", name);
            fprintf(F, "%d;level\n", mage->combatspells[i].level);
          }
        }
      }
    }
  }
  /* items */
  pr = 0;
  if (f == u->faction || omniscient(u->faction)) {
    show = u->items;
  } else if (itemcloak==false && mode>=see_unit && !(a_fshidden
      && a_fshidden->data.ca[1] == 1 && effskill(u, SK_STEALTH) >= 3)) {
    show = NULL;
    for (itm=u->items;itm;itm=itm->next) {
      item * ishow;
      const char * ic;
      int in;
      report_item(u, itm, f, NULL, &ic, &in, true);
      if (in>0 && ic && *ic) {
        for (ishow = show; ishow; ishow=ishow->next) {
          const char * sc;
          int sn;
          if (ishow->type==itm->type) sc=ic;
          else report_item(u, ishow, f, NULL, &sc, &sn, true);
          if (sc==ic || strcmp(sc, ic)==0) {
            ishow->number+=itm->number;
            break;
          }
        }
        if (ishow==NULL) {
          ishow = i_add(&show, i_new(itm->type, itm->number));
        }
      }
    }
  } else {
    show = NULL;
  }
  lasttype = NULL;
  for (itm=show; itm; itm=itm->next) {
    const char * ic;
    int in;
    assert(itm->type!=lasttype || !"error: list contains two objects of the same item");
    report_item(u, itm, f, NULL, &ic, &in, true);
    if (in==0) continue;
    if (!pr) {
      pr = 1;
      fputs("GEGENSTAENDE\n", F);
    }
    fprintf(F, "%d;%s\n", in, add_translation(ic, locale_string(f->locale, ic)));
  }
  if (show!=u->items) {
    /* free the temporary items */
    while (show) {
      i_free(i_remove(&show, show));
    }
  }

  print_curses(F, f, u, TYP_UNIT);
}
/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  */

/* prints allies */
static void
show_allies(FILE * F, const faction * f, const ally * sf)
{
  for (; sf; sf = sf->next) if (sf->faction) {
    int mode = alliedgroup(NULL, f, sf->faction, sf, HELP_ALL);
    if (mode!=0 && sf->status>0) {
      fprintf(F, "ALLIANZ %d\n", sf->faction->no);
      fprintf(F, "\"%s\";Parteiname\n", sf->faction->name);
      fprintf(F, "%d;Status\n", sf->status & HELP_ALL);
    }
  }
}

#ifdef ENEMIES
static void
show_enemies(FILE * F, const faction_list* flist)
{
  for (;flist!=NULL;flist=flist->next) {
    if (flist->data) {
      int fno = flist->data->no;
      fprintf(F, "ENEMY %u\n%u;partei\n", fno, fno);
    }
  }
}
#endif

/* prints all visible spells in a region */
static void
show_active_spells(const region * r)
{
  char fogwall[MAXDIRECTIONS];
#ifdef TODO /* alte Regionszauberanzeigen umstellen */
  unit *u;
  int env = 0;
#endif
  memset(fogwall, 0, sizeof(char) * MAXDIRECTIONS);

}
/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  */

/* this is a copy of laws.c->find_address output changed. */
static void
cr_find_address(FILE * F, const faction * uf, const faction_list * addresses)
{
  const faction_list * flist = addresses;
  while (flist!=NULL) {
    const faction * f = flist->data;
    if (uf!=f && f->no != MONSTER_FACTION) {
      fprintf(F, "PARTEI %d\n", f->no);
      fprintf(F, "\"%s\";Parteiname\n", f->name);
      fprintf(F, "\"%s\";email\n", f->email);
      fprintf(F, "\"%s\";banner\n", f->banner);
      fprintf(F, "\"%s\";locale\n", locale_name(f->locale));
      if (f->alliance!=NULL && f->alliance==uf->alliance) {
        fprintf(F, "%d;alliance\n", f->alliance->id);
        fprintf(F, "\"%s\";alliancename\n", f->alliance->name);
      }
#ifdef SHORTPWDS
      if (f->shortpwds) {
        shortpwd * spwd = f->shortpwds;
        while (spwd) {
          unsigned int vacation = 0;
          if (spwd->used) {
            fprintf(F, "VACATION %u\n", ++vacation);
            fprintf(F, "\"%s\";email\n", spwd->email);
          }
          spwd=spwd->next;
        }
      }
#endif
    }
    flist = flist->next;
  }
}
/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  */

static void
cr_reportspell(FILE * F,  spell *sp, const struct locale * lang)
{
  int k;
  const char * name = add_translation(mkname("spell", sp->sname), spell_name(sp, lang));

  fprintf(F, "ZAUBER %d\n", hashstring(sp->sname));
  fprintf(F, "\"%s\";name\n", name);
  fprintf(F, "%d;level\n", sp->level);
  fprintf(F, "%d;rank\n", sp->rank);
  fprintf(F, "\"%s\";info\n", spell_info(sp, lang));
  if (sp->parameter) fprintf(F, "\"%s\";syntax\n", sp->parameter);
  else fputs("\"\";syntax\n", F);

  if (sp->sptyp & PRECOMBATSPELL) fputs("\"precombat\";class\n", F);
  else if (sp->sptyp & COMBATSPELL) fputs("\"combat\";class\n", F);
  else if (sp->sptyp & POSTCOMBATSPELL) fputs("\"postcombat\";class\n", F);
  else fputs("\"normal\";class\n", F);

  if (sp->sptyp & FARCASTING) fputs("1;far\n", F);
  if (sp->sptyp & OCEANCASTABLE) fputs("1;ocean\n", F);
  if (sp->sptyp & ONSHIPCAST) fputs("1;ship\n", F);
  if (!(sp->sptyp & NOTFAMILIARCAST)) fputs("1;familiar\n", F);
  fputs("KOMPONENTEN\n", F);

  for (k = 0; sp->components[k].type; ++k) {
    const resource_type * rtype = sp->components[k].type;
    int itemanz = sp->components[k].amount;
    int costtyp = sp->components[k].cost;
    if (itemanz > 0) {
      const char * name = resourcename(rtype, 0);
      fprintf(F, "%d %d;%s\n", itemanz, costtyp == SPC_LEVEL || costtyp == SPC_LINEAR,
        add_translation(name, LOC(lang, name)));
    }
  }
}

static unsigned int
encode_region(const faction * f, const region * r) {
  unsigned int id;
  char *cp, c;
  /* obfuscation */
  assert(sizeof(int)==sizeof(char)*4);
  id = (((((r->x ^ f->no) % 1024) << 20) | ((r->y ^ f->no) % 1024)));
  cp = (char*)&id;
  c = cp[0];
  cp[0] = cp[2];
  cp[2] = cp[1];
  cp[1] = cp[3];
  cp[3] = c;
  return id;
}

static char *
report_resource(char * buf, const char * name, const struct locale * loc, int amount, int level)
{
  buf += sprintf(buf, "RESOURCE %u\n", hashstring(name));
  buf += sprintf(buf, "\"%s\";type\n", add_translation(name, LOC(loc, name)));
  if (amount>=0) {
    if (level>=0) buf += sprintf(buf, "%d;skill\n", level);
    buf += sprintf(buf, "%d;number\n", amount);
  }
  return buf;
}

static void
cr_borders(seen_region ** seen, const region * r, const faction * f, int seemode, FILE * F)
{
  direction_t d;
  int g = 0;
  for (d = 0; d != MAXDIRECTIONS; d++)
  { /* Nachbarregionen, die gesehen werden, ermitteln */
    const region * r2 = rconnect(r, d);
    const border * b;
    if (!r2) continue;
    if (seemode==see_neighbour) {
      seen_region * sr = find_seen(seen, r2);
      if (sr==NULL || sr->mode<=see_neighbour) continue;
    }
    b = get_borders(r, r2);
    while (b) {
      boolean cs = b->type->fvisible(b, f, r);

      if (!cs) {
        cs = b->type->rvisible(b, r);
        if (!cs) {
          unit * us = r->units;
          while (us && !cs) {
            if (us->faction==f) {
              cs = b->type->uvisible(b, us);
              if (cs) break;
            }
            us=us->next;
          }
        }
      }
      if (cs) {
        fprintf(F, "GRENZE %d\n", ++g);
        fprintf(F, "\"%s\";typ\n", b->type->name(b, r, f, GF_NONE));
        fprintf(F, "%d;richtung\n", d);
        if (!b->type->transparent(b, f)) fputs("1;opaque\n", F);
        /* pfusch: */
        if (b->type==&bt_road) {
          int p = rroad(r, d)*100/r->terrain->max_road;
          fprintf(F, "%d;prozent\n", p);
        }
      }
      b = b->next;
    }
  }
}

/* main function of the creport. creates the header and traverses all regions */
static int
report_computer(const char * filename, report_context * ctx, const char * charset)
{
  int i;
  faction * f = ctx->f;
  const char * prefix;
  region * r;
  building *b;
  ship *sh;
  unit *u;
  const char * mailto = locale_string(f->locale, "mailto");
  const attrib * a;
  seen_region * sr = NULL;
#ifdef SCORE_MODULE
  int score = 0, avgscore = 0;
#endif
  FILE * F = fopen(filename, "wt");
  if (F==NULL) {
    perror(filename);
    return -1;
  }

  /* must call this to get all the neighbour regions */
  /* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
  /* initialisations, header and lists */

  fprintf(F, "VERSION %d\n", C_REPORT_VERSION);
  fprintf(F, "\"%s\";charset\n", charset);
  fprintf(F, "\"%s\";locale\n", locale_name(f->locale));
  fprintf(F, "%d;noskillpoints\n", 1);
  fprintf(F, "%ld;date\n", ctx->report_time);
  fprintf(F, "\"%s\";Spiel\n", global.gamename);
  fprintf(F, "\"%s\";Konfiguration\n", "Standard");
  fprintf(F, "\"%s\";Koordinaten\n", "Hex");
  fprintf(F, "%d;Basis\n", 36);
  fprintf(F, "%d;Runde\n", turn);
  fputs("2;Zeitalter\n", F);
  if (mailto!=NULL) {
    fprintf(F, "\"%s\";mailto\n", mailto);
    fprintf(F, "\"%s\";mailcmd\n", locale_string(f->locale, "mailcmd"));
  }
  fprintf(F, "PARTEI %d\n", f->no);
  fprintf(F, "\"%s\";locale\n", locale_name(f->locale));
  fprintf(F, "%d;age\n", f->age);
  fprintf(F, "%d;Optionen\n", f->options);
#ifdef SCORE_MODULE
  if (f->options & want(O_SCORE) && f->age>DISPLAYSCORE) {
    score = f->score;
    avgscore = average_score_of_age(f->age, f->age / 24 + 1);
  }
  fprintf(F, "%d;Punkte\n", score);
  fprintf(F, "%d;Punktedurchschnitt\n", avgscore);
#endif
  {
    const char * zRace = rc_name(f->race, 1);
    fprintf(F, "\"%s\";Typ\n", add_translation(zRace, LOC(f->locale, zRace)));
  }
  prefix = get_prefix(f->attribs);
  if (prefix!=NULL) {
    prefix = mkname("prefix", prefix);
    fprintf(F, "\"%s\";typprefix\n",
      add_translation(prefix, LOC(f->locale, prefix)));
  }
  fprintf(F, "%d;Rekrutierungskosten\n", f->race->recruitcost);
  fprintf(F, "%d;Anzahl Personen\n", count_all(f));
  fprintf(F, "\"%s\";Magiegebiet\n", magietypen[f->magiegebiet]);

  if (f->race == new_race[RC_HUMAN]) {
    fprintf(F, "%d;Anzahl Immigranten\n", count_migrants(f));
    fprintf(F, "%d;Max. Immigranten\n", count_maxmigrants(f));
  }

#ifdef HEROES
  i = countheroes(f);
  if (i>0) fprintf(F, "%d;heroes\n", i);
  i = maxheroes(f);
  if (i>0) fprintf(F, "%d;max_heroes\n", i);
#endif

  if (f->age > 1 && f->lastorders != turn) {
    fprintf(F, "%d;nmr\n", turn-f->lastorders);
  }

  fprintf(F, "\"%s\";Parteiname\n", f->name);
  fprintf(F, "\"%s\";email\n", f->email);
  fprintf(F, "\"%s\";banner\n", f->banner);
  print_items(F, f->items, f->locale);
  fputs("OPTIONEN\n", F);
  for (i=0;i!=MAXOPTIONS;++i) {
    fprintf(F, "%d;%s\n", (f->options&want(i))?1:0, options[i]);
  }
#ifdef ENEMIES
  show_enemies(F, f->enemies);
#endif
  show_allies(F, f, f->allies);
  {
    group * g;
    for (g=f->groups;g;g=g->next) {

      fprintf(F, "GRUPPE %d\n", g->gid);
      fprintf(F, "\"%s\";name\n", g->name);
      prefix = get_prefix(g->attribs);
      if (prefix!=NULL) {
        prefix = mkname("prefix", prefix);
        fprintf(F, "\"%s\";typprefix\n",
          add_translation(prefix, LOC(f->locale, prefix)));
      }
      show_allies(F, f, g->allies);
    }
  }

  cr_output_messages(F, f->msgs, f);
  {
    struct bmsg * bm;
    for (bm=f->battles;bm;bm=bm->next) {
      if (!bm->r->planep) fprintf(F, "BATTLE %d %d\n", region_x(bm->r, f), region_y(bm->r, f));
      else {
        if (bm->r->planep->flags & PFL_NOCOORDS) fprintf(F, "BATTLESPEC %d %d\n", encode_region(f, bm->r), bm->r->planep->id);
        else fprintf(F, "BATTLE %d %d %d\n", region_x(bm->r, f), region_y(bm->r, f), bm->r->planep->id);
      }
      cr_output_messages(F, bm->msgs, f);
    }
  }

  cr_find_address(F, f, ctx->addresses);
  a = a_find(f->attribs, &at_reportspell);
  while (a && a->type==&at_reportspell) {
    spell *sp = (spell *)a->data.v;
    cr_reportspell(F, sp, f->locale);
    a = a->next;
  }
  for (a=a_find(f->attribs, &at_showitem);a && a->type==&at_showitem;a=a->next) {
    const potion_type * ptype = resource2potion(((const item_type*)a->data.v)->rtype);
    requirement * m;
    const char * ch;
    const char * description = NULL;

    if (ptype==NULL) continue;
    m = ptype->itype->construction->materials;
    ch = resourcename(ptype->itype->rtype, 0);
    fprintf(F, "TRANK %d\n", hashstring(ch));
    fprintf(F, "\"%s\";Name\n", add_translation(ch, locale_string(f->locale, ch)));
    fprintf(F, "%d;Stufe\n", ptype->level);

    if (description==NULL) {
      const char * pname = resourcename(ptype->itype->rtype, 0);
      const char * potiontext = mkname("potion", pname);
      description = LOC(f->locale, potiontext);
    }

    fprintf(F, "\"%s\";Beschr\n", description);
    fprintf(F, "ZUTATEN\n");

    while (m->number) {
      ch = resourcename(m->rtype, 0);
      fprintf(F, "\"%s\"\n", add_translation(ch, locale_string(f->locale, ch)));
      m++;
    }
  }

  /* traverse all regions */
  for (r=ctx->first;sr==NULL && r!=ctx->last;r=r->next) {
    sr = find_seen(ctx->seen, r);
  }
  for (;sr!=NULL;sr=sr->next) {
    region * r = sr->r;
    const char * tname;

    if (!r->planep) {
      if (opt_cr_absolute_coords) {
        fprintf(F, "REGION %d %d\n", r->x, r->x);
      } else {
        fprintf(F, "REGION %d %d\n", region_x(r, f), region_y(r, f));
      }
    } else {
#if ENCODE_SPECIAL
      if (r->planep->flags & PFL_NOCOORDS) fprintf(F, "SPEZIALREGION %d %d\n", encode_region(f, r), r->planep->id);
#else
      if (r->planep->flags & PFL_NOCOORDS) continue;
#endif
      else fprintf(F, "REGION %d %d %d\n", region_x(r, f), region_y(r, f), r->planep->id);
    }
    if (r->land) {
      const char * str = rname(r, f->locale);
      if (str && str[0]) {
        fprintf(F, "\"%s\";Name\n", str);
      }
    }
    tname = terrain_name(r);

    fprintf(F, "\"%s\";Terrain\n", add_translation(tname, locale_string(f->locale, tname)));
    if (sr->mode!=see_unit) fprintf(F, "\"%s\";visibility\n", visibility[sr->mode]);

    {
      faction * owner = region_owner(r);
      if (owner) {
        fprintf(F, "%d;owner\n", owner->no);
      }
    }
    if (sr->mode == see_neighbour) {
      cr_borders(ctx->seen, r, f, sr->mode, F);
    } else {
      int stealthmod = stealth_modifier(sr->mode);
#define RESOURCECOMPAT
      char cbuf[BUFFERSIZE], *pos = cbuf;
#ifdef RESOURCECOMPAT
      if (r->display && r->display[0])
          fprintf(F, "\"%s\";Beschr\n", r->display);
#endif
      if (fval(r->terrain, LAND_REGION)) {
        int trees = rtrees(r, 2);
        int saplings = rtrees(r, 1);
#ifdef RESOURCECOMPAT
        if (trees > 0) fprintf(F, "%d;Baeume\n", trees);
        if (saplings > 0) fprintf(F, "%d;Schoesslinge\n", saplings);
        if (fval(r, RF_MALLORN) && (trees > 0 || saplings > 0))
            fprintf(F, "1;Mallorn\n");
#endif
        if (!fval(r, RF_MALLORN)) {
          if (saplings) pos = report_resource(pos, "rm_sapling", f->locale, saplings, -1);
          if (trees) pos = report_resource(pos, "rm_trees", f->locale, trees, -1);
        } else {
          if (saplings) pos = report_resource(pos, "rm_mallornsapling", f->locale, saplings, -1);
          if (trees) pos = report_resource(pos, "rm_mallorn", f->locale, trees, -1);
        }
        fprintf(F, "%d;Bauern\n", rpeasants(r));
        if(fval(r, RF_ORCIFIED)) {
          fprintf(F, "1;Verorkt\n");
        }
        fprintf(F, "%d;Pferde\n", rhorses(r));

        if (sr->mode>=see_unit) {
          struct demand * dmd = r->land->demands;
          struct rawmaterial * res = r->resources;
          fprintf(F, "%d;Silber\n", rmoney(r));
          fprintf(F, "%d;Unterh\n", entertainmoney(r));

          if (is_cursed(r->attribs, C_RIOT, 0)){
            fprintf(F, "0;Rekruten\n");
          } else {
            fprintf(F, "%d;Rekruten\n", rpeasants(r) / RECRUITFRACTION);
          }
          if (production(r)) {
            fprintf(F, "%d;Lohn\n", wage(r, f, f->race));
          }

          while (res) {
            int maxskill = 0;
            int level = -1;
            int visible = -1;
            const item_type * itype = resource2item(res->type->rtype);
            if (res->type->visible==NULL) {
              visible = res->amount;
              level = res->level + itype->construction->minskill - 1;
            } else {
              const unit * u;
              for (u=r->units; visible!=res->amount && u!=NULL; u=u->next) {
                if (u->faction == f) {
                  int s = eff_skill(u, itype->construction->skill, r);
                  if (s>maxskill) {
                    if (s>=itype->construction->minskill) {
                      assert(itype->construction->minskill>0);
                      level = res->level + itype->construction->minskill - 1;
                    }
                    maxskill = s;
                    visible = res->type->visible(res, maxskill);
                  }
                }
              }
            }
            if (level>=0 && visible >=0) {
              pos = report_resource(pos, res->type->name, f->locale, visible, level);
#ifdef RESOURCECOMPAT
              if (visible>=0) fprintf(F, "%d;%s\n", visible, crtag(res->type->name));
#endif
            }
            res = res->next;
          }
          /* trade */
          if (!TradeDisabled() && rpeasants(r)/TRADE_FRACTION > 0) {
            fputs("PREISE\n", F);
            while (dmd) {
              const char * ch = resourcename(dmd->type->itype->rtype, 0);
              fprintf(F, "%d;%s\n", (dmd->value
                                     ? dmd->value*dmd->type->price
                                     : -dmd->type->price),
                      add_translation(ch, locale_string(f->locale, ch)));
              dmd=dmd->next;
            }
          }
        }
        if (pos!=cbuf) fputs(cbuf, F);
      }
      if (r->land) {
        print_items(F, r->land->items, f->locale);
      }
      print_curses(F, f, r, TYP_REGION);
      cr_borders(ctx->seen, r, f, sr->mode, F);
      if (sr->mode==see_unit && r->planep==get_astralplane() && !is_cursed(r->attribs, C_ASTRALBLOCK, 0))
      {
        /* Sonderbehandlung Teleport-Ebene */
        region_list *rl = astralregions(r, inhabitable);

        if (rl) {
          region_list *rl2 = rl;
          while(rl2) {
            region * r = rl2->data;
            fprintf(F, "SCHEMEN %d %d\n", region_x(r, f), region_y(r, f));
            fprintf(F, "\"%s\";Name\n", rname(r, f->locale));
            rl2 = rl2->next;
          }
          free_regionlist(rl);
        }
      }

      /* describe both passed and inhabited regions */
      show_active_spells(r);
      if (fval(r, RF_TRAVELUNIT)) {
        boolean seeunits = false, seeships = false;
        const attrib * ru;
        /* show units pulled through region */
        for (ru = a_find(r->attribs, &at_travelunit); ru && ru->type==&at_travelunit; ru = ru->next) {
          unit * u = (unit*)ru->data.v;
          if (cansee_durchgezogen(f, r, u, 0) && r!=u->region) {
            if (!u->ship || !fval(u, UFL_OWNER)) continue;
            if (!seeships) fprintf(F, "DURCHSCHIFFUNG\n");
            seeships = true;
            fprintf(F, "\"%s\"\n", shipname(u->ship));
          }
        }
        for (ru = a_find(r->attribs, &at_travelunit); ru && ru->type==&at_travelunit; ru = ru->next) {
          unit * u = (unit*)ru->data.v;
          if (cansee_durchgezogen(f, r, u, 0) && r!=u->region) {
            if (u->ship) continue;
            if (!seeunits) fprintf(F, "DURCHREISE\n");
            seeunits = true;
            fprintf(F, "\"%s\"\n", unitname(u));
          }
        }
      }
      cr_output_messages(F, r->msgs, f);
      {
        message_list * mlist = r_getmessages(r, f);
        if (mlist) cr_output_messages(F, mlist, f);
      }
      /* buildings */
      for (b = rbuildings(r); b; b = b->next) {
        int fno = -1;
        u = buildingowner(r, b);
        if (u && !fval(u, UFL_PARTEITARNUNG)) {
          const faction * sf = visible_faction(f,u);
          fno = sf->no;
        }
        cr_output_buildings(F, b, u, fno, f);
      }

      /* ships */
      for (sh = r->ships; sh; sh = sh->next) {
        int fno = -1;
        u = shipowner(sh);
        if (u && !fval(u, UFL_PARTEITARNUNG)) {
          const faction * sf = visible_faction(f,u);
          fno = sf->no;
        }

        cr_output_ship(F, sh, u, fno, f, r);
      }

      /* visible units */
      for (u = r->units; u; u = u->next) {

        if (u->building || u->ship || (stealthmod>INT_MIN && cansee(f, r, u, stealthmod))) {
          cr_output_unit(F, r, f, u, sr->mode);
        }
      }
    } /* region traversal */
  }
  report_crtypes(F, f->locale);
  write_translations(F);
  reset_translations();
  fclose(F);
  return 0;
}

int
crwritemap(const char * filename)
{
  FILE * F = fopen(filename, "w+");
  region * r;
  for (r=regions;r;r=r->next) {
    plane * p = r->planep;
    fprintf(F, "REGION %d %d %d\n", r->x, r->y, p?p->id:0);
    fprintf(F, "\"%s\";Name\n\"%s\";Terrain\n", rname(r, default_locale), LOC(default_locale, terrain_name(r)));
  }
  fclose(F);
  return 0;
}

void
creport_init(void)
{
  tsf_register("report", &cr_ignore);
  tsf_register("string", &cr_string);
  tsf_register("order", &cr_order);
  tsf_register("spell", &cr_spell);
  tsf_register("curse", &cr_curse);
  tsf_register("int", &cr_int);
  tsf_register("unit", &cr_unit);
  tsf_register("region", &cr_region);
  tsf_register("faction", &cr_faction);
  tsf_register("ship", &cr_ship);
  tsf_register("building", &cr_building);
  tsf_register("skill", &cr_skill);
  tsf_register("resource", &cr_resource);
  tsf_register("race", &cr_race);
  tsf_register("direction", &cr_int);
  tsf_register("alliance", &cr_alliance);
  tsf_register("resources", &cr_resources);
  tsf_register("regions", &cr_regions);

  register_reporttype("cr", &report_computer, 1<<O_COMPUTER);
}

void
creport_cleanup(void)
{
  while (junkyard) {
    translation * t = junkyard;
    junkyard = junkyard->next;
    free(t);
  }
  junkyard = 0;
}

