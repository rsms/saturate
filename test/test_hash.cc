#include "test.hh"
#include "../src/hash.hh"

using namespace sat;

int main(int argc, const char** argv) {

  // Test the two different implementations
  assert_eq(hash::fnv1a32("foo"), hash::fnv1a32("foo", 3));

  // Known collisions for FNV-1a 32
  assert_eq(hash::fnv1a32("costarring"), hash::fnv1a32("liquid"));
  assert_eq(hash::fnv1a32("declinate"), hash::fnv1a32("macallums"));
  assert_eq(hash::fnv1a32("altarage"), hash::fnv1a32("zinke"));
  assert_eq(hash::fnv1a32("altarages"), hash::fnv1a32("zinkes"));

  return 0;
}
