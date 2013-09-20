#pragma once
#include <stdlib.h>

#ifndef _SAT_INDIRECT_INCLUDE_
#error "do not include this file directly"
#endif

//-- begin SAT_TARGET_ARCH_*
#if defined(__i386) || defined(__i386__) || defined(_M_IX86)
  #define SAT_TARGET_ARCH_X86 1
#elif defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || \
      defined(_M_AMD64)
  #define SAT_TARGET_ARCH_X64 1
#elif defined(__arm__) || defined(__arm) || defined(__ARM__) || defined(__ARM)
  #define SAT_TARGET_ARCH_ARM 1
// #elif defined(__ppc__) || defined(__ppc) || defined(__PPC__) || \
//       defined(__PPC) || defined(__powerpc__) || defined(__powerpc) || \
//       defined(__POWERPC__) || defined(__POWERPC) || defined(_M_PPC)
//   #ifdef __NO_FPRS__
//     #define SAT_TARGET_ARCH_PPCSPE 1
//   #else
//     #define SAT_TARGET_ARCH_PPC 1
//   #endif
// #elif defined(__mips__) || defined(__mips) || defined(__MIPS__) || \
//       defined(__MIPS)
//   #define SAT_TARGET_ARCH_MIPS 1
#else
  #error "Unsupported target architecture"
#endif

#if SAT_TARGET_ARCH_X64
  #define SAT_TARGET_ARCH_NAME     "x64"
  #define SAT_TARGET_ARCH_SIZE     64
  #define SAT_TARGET_LITTLE_ENDIAN 1
#elif SAT_TARGET_ARCH_X86
  #define SAT_TARGET_ARCH_NAME     "x86"
  #define SAT_TARGET_ARCH_SIZE     32
  #define SAT_TARGET_LITTLE_ENDIAN 1
#elif SAT_TARGET_ARCH_ARM
  #if defined(__ARMEB__)
    #error "Unsupported target architecture: Big endian ARM"
  #endif
  #define SAT_TARGET_ARCH_NAME     "arm"
  #define SAT_TARGET_ARCH_SIZE     32
  #define SAT_TARGET_LITTLE_ENDIAN 1
#else
  #define SAT_TARGET_ARCH_NAME     "?"
  #define SAT_TARGET_ARCH_SIZE     0
  #define SAT_TARGET_LITTLE_ENDIAN 0
#endif
//-- end SAT_TARGET_ARCH_*

//-- begin SAT_TARGET_OS_*
#if defined(_WIN32) && !defined(_XBOX_VER)
  #define SAT_TARGET_OS_WINDOWS 1
  #define SAT_TARGET_OS_NAME "win32"
#elif defined(__linux__)
  #define SAT_TARGET_OS_LINUX 1
  #define SAT_TARGET_OS_POSIX 1
  #define SAT_TARGET_OS_NAME "linux"
#elif defined(__MACH__) && defined(__APPLE__)
  #define SAT_TARGET_OS_DARWIN 1
  #define SAT_TARGET_OS_POSIX 1
  #if defined(__MAC_OS_X_VERSION_MIN_REQUIRED)
    #define SAT_TARGET_OS_OSX 1
    #define SAT_TARGET_OS_NAME "osx"
  #elif defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR
    #define SAT_TARGET_OS_IOS 1
    #define SAT_TARGET_OS_IOS_SIMULATOR 1
    #define SAT_TARGET_OS_NAME "ios-simulator"
  #elif defined(__IPHONE_OS_VERSION_MIN_REQUIRED)
    #define SAT_TARGET_OS_IOS 1
    #define SAT_TARGET_OS_NAME "ios"
  #else
    #define SAT_TARGET_OS_NAME "darwin"
  #endif
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || \
      defined(__NetBSD__) || defined(__OpenBSD__)
  #define SAT_TARGET_OS_BSD 1
  #define SAT_TARGET_OS_POSIX 1
#elif (defined(__sun__) && defined(__svr4__)) || defined(__solaris__) || \
      defined(__CYGWIN__)
  #define SAT_TARGET_OS_POSIX 1
#else
  #define SAT_TARGET_OS_UNKNOWN 1
#endif
//-- end SAT_TARGET_OS_*

//-- begin SAT_TARGET_BUILD_*
#ifndef __GXX_RTTI
  #define SAT_TARGET_BUILD_NO_CXX_RTTI
#endif
#ifndef __EXCEPTIONS
  #define SAT_TARGET_BUILD_NO_CXX_EXCEPTIONS
#endif
//-- end SAT_TARGET_BUILD_*
