//
// Small single-allocation byte string with precomputed fnv1a hash.
// It comes with in two bridge-free implementations:
//  - Runtime-dynamic, heap allocated, reference counted.
//  - Constexpr, stack allocated.
//
// Suitable for "interning" or other cases where efficient hash map lookups are needed.
//
// Example of defining and printing a constexpr string:
//
//   static auto hello = ConstStr("Hello");
//   int main() {
//     auto world = ConstStr("world");
//     std::cout << hello << ' ' << world << std::endl;
//     return 0;
//   }
//
// Outputs "Hello".
// Same results using a dynamic string:
//
//   int main() {
//     Str hello{"Hello"};
//     std::cout << "hello = " << kHello << std::endl;
//     return 0;
//   }
//
// Same results but wrapping a ConstStr in a Str:
//
//   static constexpr auto kHello = ConstStr("Hello");
//   int main() {
//     Str hello{kHello};
//     std::cout << "hello = " << kHello << std::endl;
//     return 0;
//   }
//
// The `Str` struct is really just a pointer with some fancy compile-time retain, release and
// rvalue move calls at boundaries etc.
//
#pragma once
#include "common.h"
#include "hash.hh"
#include <ostream>
#include <unordered_set>
#include <unordered_map>

namespace sat {

struct Str {
  template <size_t N> struct Const; struct Imp; struct Wrap;

  Str() : self{0} {}; // == false == nullptr
  Str(const char* s, uint32_t length);
  Str(const char* cstr) : Str(cstr, ::strlen(cstr)) {};
  template <size_t N> Str(const Const<N>& cs) : self((Imp*)(&cs)) {}

  constexpr static uint32_t hash(const char* cstr) { return hash::fnv1a32(cstr); }
  constexpr static uint32_t hash(const char* p, size_t len) { return hash::fnv1a32(p, len); }

  template <size_t N> static constexpr Imp* imp_cast(const Const<N>& cs) {
    return (Imp*)static_cast<const Const<N>*>(&cs); }
  template <size_t N> static constexpr Imp* imp_cast(const Const<N>* cs) { return (Imp*)cs; }

  uint32_t hash() const;
  uint32_t size() const;
  const char* c_str() const;
  bool equals(const Str& other) const;
  bool ends_with(const char* bytes, size_t len) const;
  bool ends_with(const char* cstr) const;

  struct Equal;
    // Equality tester for use with e.g. stl containers. Handles both Str and Imp* types.

  struct Hash;
    // Hasher for use with e.g. stl containers. Handles both Str and Imp* types.

  struct WeakRef;
    // Represents a "weak reference" to a string. When the udnerlying string no longer has any
    // string references, the `self` value of this object is automatically set to NULL.

  struct Set;
    // Container that holds strong references to unique strings.

  struct WeakSet;
    // Container that holds weak references to unique strings.
    // As long as a string is in use, it will remain in the set. But when the string is deallocated,
    // the slot in the set used to hold that string will be invalidated, and marked for reuse.

  template<typename V> using Map =
    typename std::unordered_map<Str,V,Str::Hash,Str::Equal>;
    // Uniquely maps strings to values of type `V`

  SAT_REF_MIXIN_NOVTABLE_IMPL(Str, Imp)
  static void __dealloc(Imp*);
};

template<size_t N> constexpr Str::Const<N> ConstStr(const char(&s)[N]);
  // Create a Str::Const from a constant c-string.

constexpr Str::Wrap ConstStr(const char* cstr, uint32_t len);
  // Create a Str::Wrap with a pointer to a constant c-string.

// -----------------------------------------------------------------------------------------------


struct Str::Equal {
  bool operator()(const Str& a, const Str& b) const { return a.equals(b); }
  bool operator()(const Imp* a, const Imp* b) const;
};

struct Str::Hash {
  size_t operator()(const Str& a) const { return a.hash(); }
  size_t operator()(const Imp* a) const;
};


static constexpr size_t kStrConstPMagic = 1;

template <size_t N> struct __attribute__((packed)) Str::Const {
  constexpr const char* c_str() const { return _cstr; }
  constexpr uint32_t hash() const { return _hash; }
  constexpr uint32_t size() const { return _size; }

  // Note that these must be in-sync with the members of the Str struct.
  SAT_REF_COUNT_MEMBER;
  uint32_t _hash;
  uint32_t _size;
  size_t _ignored;
  const char _cstr[N];
};


struct __attribute__((packed)) Str::Wrap {
  // must be in-sync with the other Str impl structs
  refcount_t x;// SAT_REF_COUNT_MEMBER;
  uint32_t _hash;
  uint32_t _size;
  const char* const _p;
  const char _cstr[1];
};

constexpr inline Str::Wrap ConstStr(const char* cstr, uint32_t len) {
  return Str::Wrap{
    SAT_REF_COUNT_CONSTANT, // not subject to reference counting -- managed by caller
    Str::hash(cstr, len),
    len,
    cstr,
    {'\0'}
  };
}

extern const char* kStrEmptyCStr; // an empty c string

struct __attribute__((packed)) Str::Imp : ref_counted_novtable {
  static Imp* create(const char* s, uint32_t length, uint32_t hash);

  static Imp* create(const char* s, uint32_t length) {
    return create(s, length, Str::hash(s, length));
  }

  bool equals(const Imp* other) const {
    return other &&
           (_hash == other->_hash) &&
           (_size == other->_size) &&
           (std::memcmp(*_cstr ? _cstr : _p.ps,
                        *other->_cstr ? other->_cstr : (const char*)other->_p.ps,
                        _size) == 0);
  }

  const char* c_str() const { return *_cstr ? _cstr : _p.ps; }

  // Note that these must be in-sync with the members of the Str::Const struct.
  uint32_t _hash;
  uint32_t _size;
  union {
    WeakRef* weak_self = 0;
      // Used by Imp instances that have a weak reference, which is cleared on deallocation.
    const char* ps;
      // Used by Wrap type to point to the actual c-string.
  } _p;
  const char _cstr[];
    // For Wrap types, the first byte is null. This is guaranteed to contain at least one byte.
};

inline bool Str::Equal::operator()(const Imp* a, const Imp* b) const { return a->equals(b); }
inline size_t Str::Hash::operator()(const Imp* a) const { return a->_hash; }

inline Str::Str(const char* s, uint32_t length) : self(Imp::create(s, length)) {}

inline uint32_t Str::hash() const { return self ? self->_hash : 0; }
inline uint32_t Str::size() const { return self ? self->_size : 0; }
inline const char* Str::c_str() const { return self ? self->c_str() : ""; }

inline bool Str::equals(const Str& other) const { return self ? self->equals(other.self) : false; }

inline bool Str::ends_with(const char* s, size_t len) const {
  return self && size() >= len &&
         memcmp((const void*)(((const char*)c_str())+(size()-len)),
                (const void*)s,
                len) == 0;
}

inline bool Str::ends_with(const char* cstr) const {
  return ends_with(cstr, strlen(cstr));
}

inline static std::ostream& operator<< (std::ostream& os, const Str::Imp* s) {
  return os << s->c_str();
}
inline static std::ostream& operator<< (std::ostream& os, const Str::Imp& s) {
  return os << s.c_str();
}

inline static std::ostream& operator<< (std::ostream& os, const Str& s) {
  return os << s.self;
}


#if 0
#define WS_TRACE(fmt, ...) printf("(WeakRef@%p self=%p) %s [%d] " fmt "\n", \
    this, self, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#else
#define WS_TRACE(...) ((void)0)
#endif

struct Str::WeakRef {
  WeakRef() : self(0) {}
  WeakRef(const Str& s) : self(s.self) { if (self) _bind(); }
  WeakRef(Str::Imp* self) : self(self) { if (self) _bind(); }
  WeakRef(WeakRef&&) = default;
  WeakRef(const WeakRef& other) : self(other.self) {
    WS_TRACE("");
    const_cast<WeakRef&>(other).reset();
    // Warning: This is not really a copy constructor but rather a last-caller-wins move
  }
  ~WeakRef() {
    WS_TRACE("");
    reset();
  }

  WeakRef& operator=(const Str& s) { reset(s.self); return *this; }
  
  bool operator==(const WeakRef& other) const { return self == other.self; }
  bool operator!=(const WeakRef& other) const { return self != other.self; }
  bool operator==(std::nullptr_t) const { return self == 0; }
  bool operator!=(std::nullptr_t) const { return self != 0; }
  bool operator==(bool b) const { return ((bool)*this) == b; }
  bool operator!=(bool b) const { return ((bool)*this) != b; }
  operator bool() const { return self != 0; }
  operator Str::Imp*() const { return self; }
  operator Str() const { return Str{self, true/* return a +1 reference*/}; }

  void invalidate() {
    // This is called when the target has been deallocated and sets self to NULL.
    // printf("WeakRef::invalidate: this = %p\n", this); fflush(stdout);
    WS_TRACE("");
    self = 0;
  }

  void reset(Str::Imp* p=0) {
    if (self != p) {
      if (self && self->_p.weak_self == this) {
        WS_TRACE("");
        self->_p.weak_self = 0;
      }
      self = p;
      WS_TRACE("");
      if (p) _bind();
    }
  }

  Str::Imp& operator->() const { return *self; }

  void _bind() {
    if (*self->_cstr && self->_p.weak_self != (WeakRef*)kStrConstPMagic) {
      WS_TRACE("");
      if (self->_p.weak_self) {
        WS_TRACE("self->_p.weak_self=%p self->_cstr=%s", self->_p.weak_self, self->_cstr);
        // The target is bound to a different WeakRef object
        assert(self->_p.weak_self != this);
        self->_p.weak_self->reset();
        WS_TRACE("");
      }
      self->_p.weak_self = this;
    }
    // or this is either a Const or Wrap type, which never deallocates
  }

  struct Equal {
    bool operator()(const WeakRef& a, const WeakRef& b) const {
      return ( a.self == b.self ) || ( a.self && a.self->equals(b.self) ); }
  };
  struct EqualNullTrue {
    // Returns true if any operand is NULL. Used by WeakSet to allow reuse of invalidated
    // WeakRef objects.
    bool operator()(const WeakRef& a, const WeakRef& b) const {
      return ( a.self == b.self ) || !a.self || !b.self || a.self->equals(b.self); }
  };
  struct Hash {
    size_t operator()(const WeakRef& a) const { return a.self ? a.self->_hash : 0; }
  };

  Str::Imp* self;
};


inline void Str::__dealloc(Imp* self) {
  assert(*self->_cstr || self->_p.ps == kStrEmptyCStr); // or this is a Wrap type
    // Wrap should not be subject to ref counting.
    // Const objects are not subject to ref counting.
  // printf("Str::__dealloc: self = %p\n"
  //        "                _p.weak_self = %p\n"
  //        "                self->_p.ps == kStrEmptyCStr: %s\n"
  //        "                c_str():   '%s'\n"
  //        "Str::__dealloc: c_str()[0]: 0x%x\n"
  //        ,self
  //        ,self->_p.weak_self
  //        ,(self->_p.ps == kStrEmptyCStr ? "true" : "false")
  //        ,self->c_str()
  //        ,self->c_str()[0] );
  if (self->_p.ps != kStrEmptyCStr && self->_p.weak_self) {
    self->_p.weak_self->invalidate();
  }
  std::free(self);
}


// A constant-expression initializable string which is bridge-free
// compatible with a Str::Imp.

template<size_t ... Indices>
struct indices_holder {};

template<size_t index_to_add, typename Indices=indices_holder<> >
struct make_indices_impl;

template<size_t index_to_add, size_t...existing_indices>
struct make_indices_impl<index_to_add,indices_holder<existing_indices...> > {
  typedef typename make_indices_impl<
    index_to_add-1,
    indices_holder<index_to_add-1,existing_indices...> >::type type;
};

template<size_t... existing_indices>
struct make_indices_impl<0,indices_holder<existing_indices...> > {
  typedef indices_holder<existing_indices...>  type;
};

template<size_t max_index>
constexpr typename make_indices_impl<max_index>::type make_indices() {
  return typename make_indices_impl<max_index>::type();
}

template<size_t N, size_t... Indexes>
constexpr inline Str::Const<N> ConstStrx(const char(&s)[N], indices_holder<Indexes...>) {
  return Str::Const<N>{
    SAT_REF_COUNT_CONSTANT,
    Str::hash(s),
    uint32_t(N-1),
    kStrConstPMagic,
    {(Indexes < N-1 ? s[Indexes] : char())...}
  };
}

template<size_t N>
constexpr inline Str::Const<N> ConstStr(const char(&s)[N]) {
  return ConstStrx<N>(s, make_indices<N>());
}

template<size_t N>
inline static std::ostream& operator<< (std::ostream& os, const Str::Const<N>* s) {
  return os << s->_cstr;
}

template<size_t N>
inline static std::ostream& operator<< (std::ostream& os, const Str::Const<N>& s) {
  return os << s._cstr;
}

// -----------------------------------------------------------------------------------------------

struct Str::Set {
  // Container that holds strong references to unique strings and provides efficient C-string
  // lookup and insertion.

  Set(std::initializer_list<Str::Imp*> items, size_t min_buckets=8)
    : _set{items, min_buckets} {}
    // Initialize the set with `items`. Use a minimum of `min_buckets` for hashing.

  template <typename... Args> Set(Args... items)
    : Set{ Str::imp_cast(items)... } {}
    // Convenience constructor that accepts any Str implementation type that can be casted to Imp.

  Str get(const char* s, uint32_t len=0xffffffffu);
    // Return a Str representing the byte array `s` of `len` size, with a +1 reference count.

  Str find(const char* s, uint32_t len=0xffffffffu);
    // Return a Str if the set contains `s` of `len`. Otherwise a null Str is returned.

protected:
  typedef std::unordered_set<Str::Imp*, Str::Hash, Str::Equal> set_type;
  set_type _set;
};


struct Str::WeakSet {
  // Container that holds weak references to unique strings and provides efficient C-string
  // lookup and insertion.
  // As long as a string is in use, it will remain in the set. But when the string is deallocated,
  // the slot in the set used to hold that string will be invalidated, and marked for reuse.

  WeakSet(std::initializer_list<WeakRef> items, size_t min_buckets=8)
    : _set{items, min_buckets} {}
    // Initialize the set with `items`. Use a minimum of `min_buckets` for hashing.

  template <typename... Args> WeakSet(Args... items)
    : WeakSet{ std::move(WeakRef{Str::imp_cast(items)})... } {}
    // Convenience constructor that accepts any Str implementation type that can be casted to Imp.

  Str get(const char* s, uint32_t len=0xffffffffu);
    // Return a Str representing the byte array `s` of `len` size, with a +1 reference count.

  Str find(const char* s, uint32_t len=0xffffffffu);
    // Return a Str if the set contains `s` of `len`. Otherwise a null Str is returned.

protected:
  typedef std::unordered_set<WeakRef, WeakRef::Hash, WeakRef::EqualNullTrue> set_type;
  set_type _set;
};

} // namespace sat
