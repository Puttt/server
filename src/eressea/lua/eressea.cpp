#include <config.h>
#include <eressea.h>

#include "script.h"

#include <attributes/key.h>

// gamecode includes
#include <gamecode/laws.h>
#include <gamecode/monster.h>

// kernel includes
#include <kernel/alliance.h>
#include <kernel/faction.h>
#include <kernel/item.h>
#include <kernel/plane.h>
#include <kernel/race.h>
#include <kernel/reports.h>
#include <kernel/save.h>
#include <kernel/unit.h>

// lua includes
#include <lua.hpp>
#include <luabind/luabind.hpp>
#include <luabind/iterator_policy.hpp>

// util includes
#include <util/language.h>
#include <util/base36.h>

#include <cstring>
#include <ctime>

using namespace luabind;

static int
lua_addequipment(const char * iname, int number)
{
  const struct item_type * itype = it_find(iname);
  if (itype==NULL) return -1;
  add_equipment(itype, number);
  return 0;
}

static int
get_turn(void)
{
  return turn;
}

static int
read_game(const char * filename)
{
  return readgame(filename, false);
}

static int
write_game(const char *filename)
{
  free_units();
  remove_empty_factions(true);

  return writegame(filename, 0);
}

static int
write_reports()
{
  reports();
  return 0;
}

extern int process_orders(void);

static int
find_plane_id(const char * name)
{
  plane * pl = getplanebyname(name);
  return pl?pl->id:0;
}

static bool
get_flag(const char * name)
{
  int flag = atoi36(name);
  attrib * a = find_key(global.attribs, flag);
  return (a!=NULL);
}

static void
set_flag(const char * name, bool value)
{
  int flag = atoi36(name);
  attrib * a = find_key(global.attribs, flag);
  if (a==NULL && value) {
    add_key(&global.attribs, flag);
  } else if (a!=NULL && !value) {
    a_remove(&global.attribs, a);
  }
}

static const char *
get_gamename(void)
{
  return global.gamename;
}

static void
lua_setstring(const char * lname, const char * key, const char * str)
{
  struct locale * lang = find_locale(lname);
  locale_setstring(lang, key, str);
}

static const char *
lua_getstring(const char * lname, const char * key)
{
  struct locale * lang = find_locale(lname);
  return locale_getstring(lang, key);
}

static void
lua_planmonsters(void)
{
  unit * u;
  faction * f = findfaction(MONSTER_FACTION);

  if (f==NULL) return;
  if (turn == 0) srand(time((time_t *) NULL));
  else srand(turn);
  plan_monsters();
  for (u=f->units;u;u=u->nextF) {
    call_script(u);
  }
}

static void 
race_setscript(const char * rcname, const functor<void>& f)
{
  race * rc = rc_find(rcname);
  if (rc!=NULL) {
    luabind::functor<void> * fptr = new luabind::functor<void>(f);
    setscript(&rc->attribs, fptr);
  }
}

#ifdef LUABIND_NO_EXCEPTIONS
static void
error_callback(lua_State * L)
{
}
#endif

static int
get_direction(const char * name)
{
  for (int i=0;i!=MAXDIRECTIONS;++i) {
    if (strcasecmp(directions[i], name)==0) return i;
  }
  return NODIRECTION;
}

void
bind_eressea(lua_State * L)
{
  module(L)[
    def("atoi36", &atoi36),
    def("itoa36", &itoa36),
    def("read_game", &read_game),
    def("write_game", &write_game),
    def("write_passwords", &writepasswd),
    def("write_reports", &write_reports),
    def("read_orders", &readorders),
    def("process_orders", &process_orders),
    def("add_equipment", &lua_addequipment),
    def("get_turn", &get_turn),
    def("remove_empty_units", &remove_empty_units),

    /* scripted monsters */
    def("plan_monsters", &lua_planmonsters),
    def("set_brain", &race_setscript),

    /* string to enum */
    def("direction", &get_direction),

    /* localization: */
    def("set_string", &lua_setstring),
    def("get_string", &lua_getstring),

    def("set_flag", &set_flag),
    def("get_flag", &get_flag),

    def("get_gamename", &get_gamename),
    /* planes not really implemented */
    def("get_plane_id", &find_plane_id)
  ];
#ifdef LUABIND_NO_EXCEPTIONS
  luabind::set_error_callback(error_callback);
#endif
}
