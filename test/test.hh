// Helpers for tests
//
// assert_eq(a, b)
// assert_not_eq(a, b)
// assert_eq_cstr(a, b)
// assert_true(a)
// assert_false(a)
// assert_null(a)
// assert_not_null(a)
// assert_not_reached(const char* message)
//
#ifndef _HI_TEST_H_
#define _HI_TEST_H_

// #ifdef NDEBUG
//   #undef NDEBUG
// #endif

#include <assert.h>
#include <unistd.h> // _exit
#include "../src/common.h"

#if SAT_TEST_SUIT_RUNNING
  #define print(...) ((void)0)
#else
  #define print(fmt, ...) do { \
    fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
    fflush(stderr); \
  } while(0)
#endif

// ------------------------------
#ifdef __cplusplus
#include <iostream>

namespace sat {

inline void SAT_UNUSED _assert_fail0(const char* funcname, const char* message,
                                     const char* source_name, int source_line)
{
    std::cerr << "\nAssertion failure in " << funcname << " at "
              << source_name << ":" << source_line << "\n"
              << message
              << std::endl;
    std::cerr.flush();
    _exit(30);
}

template <typename A, typename B> inline void SAT_UNUSED _assert_fail(
    const char* funcname,
    A value1, const char* name1,
    B value2, const char* name2,
    const char* op,
    const char* source_name, int source_line)
{
    std::cerr << "\nAssertion failure in " << funcname << " at "
              << source_name << ":" << source_line << "\n"
              << "  Expected: " << name1 << " " << op << " " << name2 << "\n"
              << "  Actual:   " << value1 << " " << op << " " << value2
              << std::endl;
    std::cerr.flush();
    _exit(30);
}

template <typename A, typename B> inline void SAT_UNUSED _assert_eq(
    const char* funcname,
    A value1, const char* name1,
    B value2, const char* name2,
    const char* source_name, int source_line)
{
  if (!(value1 == value2))
    _assert_fail(funcname, value1, name1, value2, name2, "==", source_name, source_line);
}

template <typename A, typename B> inline void SAT_UNUSED _assert_not_eq(
    const char* funcname,
    A value1, const char* name1,
    B value2, const char* name2,
    const char* source_name, int source_line)
{
  if (!(value1 != value2))
    _assert_fail(funcname, value1, name1, value2, name2, "!=", source_name, source_line);
}

template <typename A> inline void SAT_UNUSED _assert_null(
    const char* funcname,
    A v, const char* name, const char* source_name, int source_line)
{
  assert((size_t)nullptr == (size_t)NULL);
  if (!(v == (A)nullptr))
    _assert_fail(funcname, v, name, "nullptr", "nullptr", "==", source_name, source_line);
}

template <typename A> inline void SAT_UNUSED _assert_not_null(
    const char* funcname,
    A v, const char* name, const char* source_name, int source_line)
{
  if (!(v != (A)nullptr))
    _assert_fail(funcname, v, name, (A)nullptr, "nullptr", "!=", source_name, source_line);
}

inline void SAT_UNUSED _assert_eq_cstr(
    const char* funcname,
    const char* cstr1, const char* name1,
    const char* cstr2, const char* name2,
    const char* source_name, int source_line)
{
  if (std::strcmp(cstr1, cstr2) != 0) {
    std::cerr << "\nAssertion failure in " << funcname << " at "
              << source_name << ":" << source_line << "\n"
              << "  " << name1 << " == " << name2 << "\n"
              << "  \"" << cstr1 << "\" == \"" << cstr2 << "\""
              << std::endl;
    _exit(30);
  }
}

#define assert_eq(a, b) ::sat::_assert_eq(__PRETTY_FUNCTION__, \
  (a), #a, (b), #b, __FILE__, __LINE__)
#define assert_not_eq(a, b) ::sat::_assert_not_eq(__PRETTY_FUNCTION__, \
  (a), #a, (b), #b, __FILE__, __LINE__)
#define assert_eq_cstr(a, b) ::sat::_assert_eq_cstr(__PRETTY_FUNCTION__, \
  (a), #a, (b), #b, __FILE__, __LINE__)
#define assert_true(a) ::sat::_assert_eq(__PRETTY_FUNCTION__, \
  (bool)(a), #a, true, "true", __FILE__, __LINE__)
#define assert_false(a) ::sat::_assert_eq(__PRETTY_FUNCTION__, \
  (bool)(a), #a, false, "false", __FILE__, __LINE__)
#define assert_null(a) ::sat::_assert_null(__PRETTY_FUNCTION__, \
  (a), #a, __FILE__, __LINE__)
#define assert_not_null(a) ::sat::_assert_not_null(__PRETTY_FUNCTION__, \
  (a), #a, __FILE__, __LINE__)
#define assert_not_reached(a) ::sat::_assert_fail0(__PRETTY_FUNCTION__, \
  (a), __FILE__, __LINE__)

} // namespace
#endif // __cplusplus


#endif  // _HI_TEST_H_
