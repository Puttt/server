#ifndef LUA_OBJECTS_H
#define LUA_OBJECTS_H

#include <attributes/object.h>
#include <lua.hpp>
#include <luabind/luabind.hpp>
#include <luabind/object.hpp>

struct lua_State;

namespace eressea {

  class objects {
  public:
    objects(struct attrib ** attribs) : mAttribPtr(attribs) {}

    luabind::object get(const char * name);
    // void set(const char * name, int type, luabind::object& value);
    template <class V, object_type T> void 
    set(const char * name, V value);

  private:
    struct attrib ** mAttribPtr;
  };


  template<class T> eressea::objects
  get_objects(const T& parent)
  {
    return eressea::objects(&const_cast<attrib *&>(parent.attribs));
  }
};

#endif

