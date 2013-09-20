#pragma once

#ifndef _SAT_INDIRECT_INCLUDE_
#error "do not include this file directly"
#endif

namespace sat {

typedef u32 refcount_t;
#define SAT_REF_COUNT_MEMBER volatile refcount_t __refcount
#define SAT_REF_COUNT_CONSTANT ((refcount_t)0xffffffffu)
#define SAT_REF_COUNT_INIT ((refcount_t)1)

// Reference-counted POD-style objects
struct ref_counted {
  SAT_REF_COUNT_MEMBER = SAT_REF_COUNT_INIT;
  virtual ~ref_counted() = default;
    // force vtable so that implementations can use vtables (or __refcount offset will be incorrect)
  void __retain() {
    sat_atomic_add_fetch((i32*)&__refcount, 1);
  }
  bool __release() {
    if (sat_atomic_sub_fetch(&__refcount, 1) == 0) {
      __dealloc();
      return true;
    }
    return false;
  }
  virtual void __dealloc() {
    // A `Imp` struct can override this. In the case you do override this, you must declare your
    // `Imp` publicly, like this:
    //
    //   struct foo { SAT_REF_MIXIN(foo)
    //     struct Imp : ref_counted { void __dealloc(); };
    //   };
    //
    // Then internally define a subclass for actual implementation:
    //
    //   struct foo::Imp2 : foo::Imp {
    //     (actual members)
    //   };
    //
    // And finally provide an implementation for __dealloc:
    //
    //   void foo::Imp::__dealloc() {
    //     printf("About to deallocate foo::Imp\n");
    //     delete this;
    //   }
    //
    delete this;
  }
};

struct ref_counted_novtable {
  // If you don't want a vtable, this is the way (at the expense of no virtual destructors)
  SAT_REF_COUNT_MEMBER = SAT_REF_COUNT_INIT;
  void __retain() {
    if (__refcount != SAT_REF_COUNT_CONSTANT) sat_atomic_add_fetch((i32*)&__refcount, 1);
  }
  // Note: To release this object, call T::__release(thisobjptr)
};

// Example:
//
//   lolcat.hh:
//     struct lolcat { SAT_REF_MIXIN(lolcat)
//       lolcat(const char* name);
//       const char* name() const; };
//  
//   lolcat.cc:
//     struct lolcat::Imp : sat::ref_counted { const char* name; };
//     lolcat::lolcat(const char* name) : self(new Imp{name}) {}
//     const char* lolcat::name() const { return self->name; }
//  
//   main.cc:
//     lolcat make_furball(bool b) {
//       return cat(b); }
//     int main() {
//       lolcat cat = make_furball("Busta Rhymes");
//       std::cout << cat.name() << "\n";
//       return 0; } // `cat` deallocated here
//
#define SAT_REF_MIXIN(T) \
  public: \
    struct Imp; friend Imp; \
    SAT_REF_MIXIN_VTABLE_IMPL(T, Imp)

#define SAT_REF_MIXIN_VTABLE_IMPL(T,Imp) \
  static void __retain(Imp* p) { if (p) ((::sat::ref_counted*)p)->__retain(); } \
  static void __release(Imp* p) { if (p) ((::sat::ref_counted*)p)->__release(); } \
  _SAT_REF_MIXIN_IMPL(T, Imp)

#define SAT_REF_MIXIN_NOVTABLE_IMPL(T,Imp) \
  static void __retain(Imp* p) { if (p) ((::sat::ref_counted_novtable*)p)->__retain(); } \
  static void __release(Imp* p) { \
    if (p && ((::sat::ref_counted_novtable*)p)->__refcount != SAT_REF_COUNT_CONSTANT \
          && sat_atomic_sub_fetch(&((::sat::ref_counted_novtable*)p)->__refcount, 1) == 0) \
      T::__dealloc(p); \
  } \
  _SAT_REF_MIXIN_IMPL(T, Imp)

#define _SAT_REF_MIXIN_IMPL(T,Imp) \
  Imp* self = nullptr; \
  T(std::nullptr_t) : self(nullptr) {} \
  explicit T(Imp* p, bool add_ref=false) : self(p) { if (add_ref) { __retain(self); } } \
  T(T const& rhs) : self(rhs.self) { __retain(self); } \
  T(const T* rhs) : self(rhs->self) { __retain(self); } \
  T(T&& rhs) { self = std::move(rhs.self); rhs.self = 0; } \
  ~T() { __release(self); } \
  T& reset_self(const Imp* p = nullptr) const { \
    Imp* old = self; \
    __retain((const_cast<T*>(this)->self = const_cast<Imp*>(p))); \
    __release(old); \
    return *const_cast<T*>(this); \
  } \
  Imp* steal_self() { \
    Imp* p = self; \
    self = 0; \
    return std::move(p); \
  } \
  T& operator=(T&& rhs) { \
    if (self != rhs.self) { \
      __release(self); \
      self = rhs.self; \
    } \
    rhs.self = 0; \
    return *this; \
  } \
  T& operator=(const T& rhs) { return reset_self(rhs.self); } \
  T& operator=(Imp* rhs) { return reset_self(rhs); } \
  T& operator=(const Imp* rhs) { return reset_self(rhs); } \
  T& operator=(std::nullptr_t) { return reset_self(nullptr); } \
  Imp* operator->() const { return self; } \
  bool operator==(const T& other) const { return self == other.self; } \
  bool operator!=(const T& other) const { return self != other.self; } \
  bool operator==(std::nullptr_t) const { return self == nullptr; } \
  bool operator!=(std::nullptr_t) const { return self != nullptr; } \
  operator bool() const { return self != nullptr; }

// end of SAT_REF_MIXIN

} // namespace
