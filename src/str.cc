#include "str.hh"

namespace sat {

const char* kStrEmptyCStr = "";

Str::Imp* Str::Imp::create(const char* s, uint32_t length, uint32_t hash) {
  uint32_t cstr_size = length+1;
  Imp* self = (Imp*)malloc(sizeof(Imp) + cstr_size);
  if (self) {
    self->_hash = hash;
    self->_size = length;
    self->__refcount = SAT_REF_COUNT_INIT;
    memcpy((void*)&self->_cstr, (const void*)s, cstr_size);
    const_cast<char*>(self->_cstr)[length] = '\0';
    self->_p.ps = (cstr_size == 1) ? kStrEmptyCStr : 0;
      // cstr_size==1 means that the string is empty. Since we rely on some really funky hacks
      // where we check _cstr[0] for NUL, and fall back on _p.ps, we need to set _p.ps to a
      // constant C-string (the empty string).
  }
  return self;
}


// ------------------------------------------------------------------------------------------------

#define STRSET_TMPWRAP \
  assert(s); \
  if (len == 0xffffffffu) len = strlen(s); \
  auto sw = ConstStr(s, len); \
  Str::Imp* obj = (Str::Imp*)&sw;


Str Str::Set::get(const char* s, uint32_t len) {
  // Return or create a Str object representing the byte array of `len` at `s`
  STRSET_TMPWRAP

  auto P = _set.emplace(obj);
  if (P.second) {
    // Did insert. Update inserted pointer with the address of a new Str::Imp
    obj = Str::Imp::create(s, len);
    Str::Imp** vp = const_cast<Str::Imp**>(&*P.first); // Str::Imp*const*
    *vp = obj;
  } else {
    // Already exists
    obj = *P.first;
  }

  return std::move(Str{obj, true/* +1 reference */});
}

Str Str::Set::find(const char* s, uint32_t len) {
  STRSET_TMPWRAP
  auto I = _set.find(obj);
  return I == _set.end() ? nullptr : std::move(Str{*I});
}


Str Str::WeakSet::get(const char* s, uint32_t len) {
  // Return or create a Str object representing the byte array of `len` at `s`
  STRSET_TMPWRAP
    // Here we wrap `s` so that we can look it up without copying the string. In the case that `s`
    // is already represented in the set, the whole operation is cheap in the sense that no copies
    // are created to deallocated.

  auto P = _set.emplace(obj);
  if (P.second || !P.first->self) {
    // Did insert. Update inserted pointer with the address of a new Str::Imp
    // printf("intern MISS (%s) '%s'\n", (!P.first->self ? "reuse-slot" : "new-slot"), s);
    obj = Str::Imp::create(s, len);
    WeakRef* ws = ((WeakRef*)(&*P.first));
    ws->self = obj;
    ws->_bind();
  } else {
    // Does exist
    // printf("intern HIT '%s'\n", s);
    obj = P.first->self;
    obj->__retain();
  }

  return std::move(Str{obj, false/* give reference as we already incremented it */});
}

Str Str::WeakSet::find(const char* s, uint32_t len) {
  STRSET_TMPWRAP
  auto I = _set.find(obj);
  return I == _set.end() ? nullptr : std::move(Str{I->self, true/* +1 reference */});
}

#undef STRSET_TMPWRAP

} // namespace sat
