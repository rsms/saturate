#pragma once

#define _SAT_INDIRECT_INCLUDE_

// The classic stringifier macro
#define SAT_STR1(str) #str
#define SAT_STR(str) SAT_STR1(str)

// And the token-join macro.
// SAT_JOIN(foo, __LINE__) -> foo123
#define SAT_JOIN1(x, y) x ## y
#define SAT_JOIN(x, y) SAT_JOIN1(x, y)

// Configuration
#ifndef SAT_DEBUG
  #define SAT_DEBUG 0
#elif SAT_DEBUG && defined(NDEBUG)
  #undef NDEBUG
#endif

// Defines the host target
#include "common-target.h"

#define SAT_ABORT(fmt, ...) do { \
    fprintf(stderr, "*** " fmt " at " __FILE__ ":" SAT_STR(__LINE__) "\n", \
      ##__VA_ARGS__); abort(); } while (0)

#define SAT_NOT_IMPLEMENTED \
  SAT_ABORT("NOT IMPLEMENTED in %s", __PRETTY_FUNCTION__)

#define SAT_MAX(a,b) \
  ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define SAT_MIN(a,b) \
  ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#ifndef __has_attribute
  #define __has_attribute(x) 0
#endif
#ifndef __has_builtin
  #define __has_builtin(x) 0
#endif

#if __has_attribute(always_inline)
  #define SAT_ALWAYS_INLINE __attribute__((always_inline))
#else
  #define SAT_ALWAYS_INLINE
#endif
#if __has_attribute(noinline)
  #define SAT_NO_INLINE __attribute__((noinline))
#else
  #define SAT_NO_INLINE
#endif
#if __has_attribute(unused)
  // Attached to a function, means that the function is meant to be possibly
  // unused. The compiler will not produce a warning for this function.
  #define SAT_UNUSED __attribute__((unused))
#else
  #define SAT_UNUSED
#endif
#if __has_attribute(warn_unused_result)
  #define SAT_WUNUSEDR __attribute__((warn_unused_result))
#else
  #define SAT_WUNUSEDR
#endif
#if __has_attribute(packed)
  #define SAT_PACKED __attribute((packed))
#else
  #define SAT_PACKED
#endif
#if __has_attribute(aligned)
  #define SAT_ALIGNED(bytes) __attribute__((aligned (bytes)))
#else
  #warning "No align attribute available. Things might break"
  #define SAT_ALIGNED
#endif

#if __has_builtin(__builtin_unreachable)
  #define SAT_UNREACHABLE do { \
    assert(!"Declared UNREACHABLE but was reached"); __builtin_unreachable(); } while(0);
#else
  #define SAT_UNREACHABLE assert(!"Declared UNREACHABLE but was reached");
#endif

#if __has_attribute(noreturn)
  #define SAT_NORETURN __attribute__((noreturn))
#else
  #define SAT_NORETURN 
#endif

#if defined(__clang__)
  #define SAT_FALLTHROUGH [[clang::fallthrough]];
  #pragma GCC diagnostic error "-Wimplicit-fallthrough"
#else
  #define SAT_FALLTHROUGH
#endif

typedef signed char           i8;
typedef unsigned char         u8;
typedef short                 i16;
typedef unsigned short        u16;
typedef int                   i32;
typedef unsigned int          u32;
#if defined(_WIN32)
  typedef __int64             i64;
  typedef unsigned __int64    u64;
#else
  typedef long long           i64;
  typedef unsigned long long  u64;
#endif

#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 4))
  #define SAT_I128_INTEGRAL 1
  typedef __uint128_t         u128;
// #else
//   #define SAT_I128_STRUCT 1
//   typedef struct { unsigned long long high; unsigned long long low; } u128;
#endif

#if SAT_TARGET_ARCH_X86 || SAT_TARGET_ARCH_X64
  #define SAT_TRAP_TO_DEBUGGER  __asm__ ("int3");
#endif

// libc
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "common-atomic.h"
#ifdef __cplusplus
  // Caution: Must only include header-only libc++ headers here since we don't link with libc++
  #include <utility> // std::move, et al
  
  #if defined(SAT_NO_STDLIBCXX)
  // Since we don't link with libc++, we need to provide `new` and `delete`
  struct nothrow_t {};
  inline void* operator new (size_t z) { return malloc(z); }
  inline void* operator new (size_t z, const nothrow_t&) noexcept { return malloc(z); }
  // inline void* operator new (size_t, void* p) noexcept { return p; }
  inline void operator delete (void* p) noexcept { free(p); }
  inline void operator delete (void* p, const nothrow_t&) noexcept { free(p); }
  // inline void operator delete (void* p, void*) noexcept {}
  #endif // defined(SAT_NO_STDLIBCXX)

  template<typename... Args> inline void pass(Args&&...) {}

  #include "common-ref.h"  // SAT_REF*, sat::ref_counted
#endif

#undef _SAT_INDIRECT_INCLUDE_
