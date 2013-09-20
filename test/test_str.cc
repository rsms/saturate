//!DEP ../src/str.cc
#include "test.hh"
#include "../src/str.hh"

using namespace sat;

void test_map() {
  // Simple map
  Str::Map<int> m;
  Str s1{"a"};
  m[s1] = 123;
  assert(m[s1] == 123);

  { // Map holds strong references
    Str::WeakRef wr;
    {
      Str s{"fred"};
      wr = s;
      m[s] = 1337;
    }
    assert(wr == true);
    assert(m[wr] == 1337);
  }

  { // Strs are not referenced if an equivalent is already in the map
    Str::WeakRef wr;
    {
      Str s{"fred"};
      wr = s;
      m[s] = 9000;
    }
    assert(wr == false);
      // Since `s` was never added to the map, as value "fred" is already associated with `1337`
    assert(m["fred"] == 9000);
      // But the value changed
  }

  // Implicit `Str{const char*}` constructor
  m["b"] = 456;
  assert(m["a"] == 123);
  assert(m["b"] == 456);

  // Replacing
  m["b"] = 500;
  assert(m["b"] == 500);

  // Constructing a map with an initializer list
  Str::Map<int> m2 = {
    {"foo", 100},
    {"bar", 200},
    {"baz", 300},
  };
  assert(m2["foo"] == 100);
  assert(m2["bar"] == 200);
  assert(m2["baz"] == 300);
}


#define CONST_SYMBOLS \
  F(A, "A") \
  F(B, "B") \

#define F(name, cstr) static constexpr auto kStr_##name = ConstStr(cstr);
CONST_SYMBOLS
#undef F

static Str::WeakSet weak_set{
  #define F(name, cstr) &kStr_##name,
  CONST_SYMBOLS
  #undef F
};

static Str::Set strong_set{
  #define F(name, cstr) &kStr_##name,
  CONST_SYMBOLS
  #undef F
};


template <typename ST>
void test_set_basics(ST& set) {
  Str s = set.get("A");
  assert(s.self == (Str::Imp*)&kStr_A);
  assert(std::strcmp(s.c_str(), "A") == 0);
  assert(set.get("B").self == (Str::Imp*)&kStr_B);
}

void test_hash_function() {
  char* p = (char*)malloc(10);
  memcpy(p, "AB user CD", 10);

  uint32_t h1 = Str::hash("user");
  uint32_t h2 = Str::hash("user", 4);
  uint32_t h3 = Str::hash(&p[3], 4);
  assert(h1 == h2);
  assert(h1 == h3);
}

template <typename ST>
uint32_t test_set_interned(ST& set) {
  Str s1 = set.get("lolcatz");
  assert_eq_cstr(s1.c_str(), "lolcatz");
  Str s2 = set.get("lolcatz");
  assert_eq_cstr(s2.c_str(), "lolcatz");
  assert_eq(s2, s1); // equality
  assert_eq(s2.self, s1.self); // same memory

  Str s3 = set.get("user", 4);
  Str s4 = set.get("user", 4);
  assert_eq(s3.self, s4.self); // same memory

  // Distinctively different cstr memory with equivalent content ("user")
  char* p = (char*)malloc(10);
  memcpy(p, "AB user CD", 10);
  Str s5 = set.get(&p[3], 4);
  assert_eq(s3, s5); // equality
  assert_eq(s3.self, s5.self); // same memory
  // There was once a bug in ConstStr() where Str::hash(cstr) was used instead of Str::hash(p, len)

  return s2.hash();
}

template <typename ST>
void test_set_interned_again_hash(ST& set, uint32_t prev_hash_value) {
  Str s1 = set.get("lolcatz");
  assert(std::strcmp(s1.c_str(), "lolcatz") == 0);
  assert(s1.hash() == prev_hash_value); // same hash value as now-deallocated `s2`
}

template <typename ST>
void test_set_find(ST& set) {
  Str s1 = set.find("A");
  assert(s1.self == (Str::Imp*)&kStr_A);

  Str s2 = set.find("C");
  assert(s2 == nullptr);
  assert(s2.c_str() == kStrEmptyCStr);

  Str s3 = set.get("C");
  assert(std::strcmp(s3.c_str(), "C") == 0);

  Str s4 = set.find("C");
  assert(s4 == s3);
  assert(s4.self == s3.self);
}

void test_strong_str_set() {
  test_set_basics(strong_set);
  uint32_t prev_hash_value = test_set_interned(strong_set);
  test_set_interned_again_hash(strong_set, prev_hash_value);
  test_set_find(strong_set);
}


void test_weak_str_set() {
  test_set_basics(weak_set);
  uint32_t prev_hash_value = test_set_interned(weak_set);
  test_set_interned_again_hash(weak_set, prev_hash_value);
  test_set_find(weak_set);
}


void test_weak_str() {
  Str::WeakRef ws1;
  {
    Str s1;
    {
      Str s2{"Hello"};
      ws1 = s2;
      s1 = s2;
      assert(ws1.self == s1.self); // s1 and s2 owns references
    }
    assert(ws1.self == s1.self); // s1 owns a reference
  }
  assert(ws1.self == 0); // no one owns a reference
}


void test_basics() {
  Str s{"lol"};
  assert(std::strcmp(s.c_str(), "lol") == 0);
}


int main(int argc, const char** argv) {
  test_hash_function();
  test_basics();
  test_weak_str();
  test_strong_str_set();
  test_weak_str_set();
  test_map();
}
