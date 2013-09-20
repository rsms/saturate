#pragma once
#include <stdint.h>
#include <stddef.h>

namespace hash {

constexpr uint32_t fnv1a32(const char *const cstr);
constexpr uint32_t fnv1a32(const char *const p, const size_t len);
constexpr uint64_t fnv1a64(const char *const cstr);
constexpr uint64_t fnv1a64(const char *const p, const size_t len);
constexpr intptr_t twang(intptr_t);
constexpr uint32_t twang(uint32_t);
constexpr uint64_t twang(uint64_t);

// -----------------------------------------------------------------------------------------------
// Implementations

static constexpr uint32_t FNV1A_PRIME_32 = 0x1000193u;       // pow(2,24) + pow(2,8) + 0x93
static constexpr uint64_t FNV1A_PRIME_64 = 0x100000001b3ull; // pow(2,40) + pow(2,8) + 0xb3
static constexpr uint32_t FNV1A_INIT_32  = 0x811c9dc5u;
static constexpr uint64_t FNV1A_INIT_64  = 0xcbf29ce484222325ull;

constexpr inline uint32_t fnv1a32_(const char* const str, const uint32_t v, const bool) {
  return *str ? fnv1a32_(str+1, (v ^ uint8_t(*str)) * FNV1A_PRIME_32, true) : v; }
constexpr inline uint32_t fnv1a32(const char* const str) {
  return fnv1a32_(str, FNV1A_INIT_32, true); }

constexpr inline uint32_t fnv1a32_(const char* const str, const size_t len, const uint32_t v) {
  return len ? fnv1a32_(str+1, len-1, (v ^ uint8_t(*str)) * FNV1A_PRIME_32) : v; }
constexpr inline uint32_t fnv1a32(const char* const str, const size_t len) {
  return fnv1a32_(str, len, FNV1A_INIT_32); }

constexpr inline uint64_t fnv1a64_(const char* const str, const uint64_t v, const bool) {
  return *str ? fnv1a64_(str+1, (v ^ uint8_t(*str)) * FNV1A_PRIME_64, true) : v; }
constexpr inline uint64_t fnv1a64(const char* const str) {
  return fnv1a64_(str, FNV1A_INIT_64, true); }

constexpr inline uint64_t fnv1a64_(const char* const str, const size_t len, const uint64_t v) {
  return len ? fnv1a64_(str+1, len-1, (v ^ uint8_t(*str)) * FNV1A_PRIME_64) : v; }
constexpr inline uint64_t fnv1a64(const char* const str, const size_t len) {
  return fnv1a64_(str, len, FNV1A_INIT_64); }

constexpr intptr_t twang(intptr_t v) {
  return (sizeof(intptr_t) == 8) ? twang(uint64_t(v)) : twang(uint32_t(v)); }

constexpr uint32_t twang_0(uint32_t v) { return (v + 0x7ed55d16) + (v << 12); }
constexpr uint32_t twang_1(uint32_t v) { return (twang_0(v) ^ 0xc761c23c) ^ (twang_0(v) >> 19); }
constexpr uint32_t twang_2(uint32_t v) { return (twang_1(v) + 0x165667b1) + (twang_1(v) << 5); }
constexpr uint32_t twang_3(uint32_t v) { return (twang_2(v) + 0xd3a2646c) ^ (twang_2(v) << 9); }
constexpr uint32_t twang_4(uint32_t v) { return (twang_3(v) + 0xfd7046c5) + (twang_3(v) << 3); }
constexpr uint32_t twang(uint32_t v)   { return (twang_4(v) ^ 0xb55a4f09) ^ (twang_4(v) >> 16); }

constexpr uint64_t twang_0(uint64_t v) { return (~v) + (v << 21); }
constexpr uint64_t twang_1(uint64_t v) { return twang_0(v) ^ (twang_0(v) >> 24); }
constexpr uint64_t twang_2(uint64_t v) { return twang_1(v) + (twang_1(v) << 3) + (twang_1(v) <<8);}
constexpr uint64_t twang_3(uint64_t v) { return twang_2(v) ^ (twang_2(v) >> 14); }
constexpr uint64_t twang_4(uint64_t v) { return twang_3(v) + (twang_3(v) << 2) + (twang_3(v) <<4);}
constexpr uint64_t twang_5(uint64_t v) { return twang_4(v) ^ (twang_4(v) >> 28); }
constexpr uint64_t twang(uint64_t v)   { return twang_5(v) + (twang_5(v) << 31); }


} // namespace hash
