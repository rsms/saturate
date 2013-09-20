#!/bin/bash
set -e
cd "$(dirname "$0")"
if [ "$CXX" == "" ]; then CXX=clang; fi
if [ "$CXXFLAGS" == "" ]; then CXXFLAGS="-std=c++11 -stdlib=libc++"; fi
if [ "$LDFLAGS" == "" ]; then LDFLAGS="-stdlib=libc++ -lc++"; fi
CXXFLAGS="$CXXFLAGS -O0 -g"

function run_test {
  set -e
  bn="$(basename "$1" .cc)"
  if [ "$1" == "$bn" ]; then
    echo "Invalid name '$1' (not ending in '.cc')" >&2
    exit 1
  fi
  SOURCES="$1"
  # Find dependencies from first line starting with "//!DEP " followed by a space-separated list
  # of source files to compile together with the test
  if deps=$(python -c 'f = open("'"$1"'"); s = f.readline().split(); f.close(); print(" ".join(s[1:])) if len(s) and s[0] == "//!DEP" else exit(1)'); then
    SOURCES="$SOURCES $deps"
  fi
  echo "$bn:"
  echo " " $CXX $CXXFLAGS $LDFLAGS -o "$bn.bin" $SOURCES
  $CXX $CXXFLAGS $LDFLAGS -o "$bn.bin" $SOURCES
  echo -n "  ./$bn.bin ... "
  set +e
  if ! "./$bn.bin"; then
    echo "FAIL"
    exit 1
  fi
  echo "OK"
}

if [ $# -lt 1 ]; then
  echo "usage: $0 <test_foo.cc> ..." >&2
  exit 1
else
  for f in $@; do
    run_test "$f"
  done
fi
