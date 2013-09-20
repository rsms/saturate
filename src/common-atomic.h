#pragma once
/*

T sat_atomic_swap(T *ptr, T value)
  Atomically swap integers or pointers in memory. Note that this is more than just CAS.
  E.g: int old_value = sat_atomic_swap(&value, new_value);

void sat_atomic_add32(i32* operand, i32 delta)
  Increment a 32-bit integer `operand` by `delta`. There's no return value.

T sat_atomic_add_fetch(T* operand, T delta)
  Add `delta` to `operand` and return the resulting value of `operand`

T sat_atomic_sub_fetch(T* operand, T delta)
  Subtract `delta` from `operand` and return the resulting value of `operand`

bool sat_atomic_cas_bool(T* ptr, T oldval, T newval)
  If the current value of *ptr is oldval, then write newval into *ptr. Returns true if the
  operation was successful and newval was written.

T sat_atomic_cas(T* ptr, T oldval, T newval)
  If the current value of *ptr is oldval, then write newval into *ptr. Returns the contents of
  *ptr before the operation.

-----------------------------------------------------------------------------*/

#ifndef _SAT_INDIRECT_INCLUDE_
#error "do not include this file directly"
#endif

#define _SAT_ATOMIC_HAS_SYNC_BUILTINS \
  defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 4))

// T sat_atomic_swap(T *ptr, T value)
#if SAT_WITHOUT_SMP
  #define sat_atomic_swap(ptr, value)  \
    ({ __typeof__ (value) oldval = *(ptr); \
       *(ptr) = (value); \
       oldval; })
#elif defined(__clang__)
  // This is more efficient than the below fallback
  #define sat_atomic_swap __sync_swap
#elif _SAT_ATOMIC_HAS_SYNC_BUILTINS
  static inline void* SAT_UNUSED _sat_atomic_swap(void* volatile* ptr, void* value) {
    void* oldval;
    do {
      oldval = *ptr;
    } while (__sync_val_compare_and_swap(ptr, oldval, value) != oldval);
    return oldval;
  }
  #define sat_atomic_swap(ptr, value) \
    _sat_atomic_swap((void* volatile*)(ptr), (void*)(value))
#else
  #error "Unsupported compiler: Missing support for atomic operations"
#endif

// void sat_atomic_add32(T* operand, T delta)
#if SAT_WITHOUT_SMP
  #define sat_atomic_add32(operand, delta) (*(operand) += (delta))
#elif SAT_TARGET_ARCH_X64 || SAT_TARGET_ARCH_X86
  inline static void SAT_UNUSED sat_atomic_add32(i32* operand, i32 delta) {
    // From http://www.memoryhole.net/kyle/2007/05/atomic_incrementing.html
    __asm__ __volatile__ (
      "lock xaddl %1, %0\n" // add delta to operand
      : // no output
      : "m" (*operand), "r" (delta)
    );
  }
  #ifdef __cplusplus
  inline static void SAT_UNUSED sat_atomic_add32(u32* o, u32 d) {
    sat_atomic_add32((i32*)o, static_cast<i32>(d)); }
  inline static void SAT_UNUSED sat_atomic_add32(volatile i32* o, volatile i32 d) {
    sat_atomic_add32((i32*)o, static_cast<i32>(d)); }
  inline static void SAT_UNUSED sat_atomic_add32(volatile u32* o, volatile u32 d) {
    sat_atomic_add32((i32*)o, static_cast<i32>(d)); }
  #endif
#elif _SAT_ATOMIC_HAS_SYNC_BUILTINS
  #define sat_atomic_add32 __sync_add_and_fetch
#else
  #error "Unsupported compiler: Missing support for atomic operations"
#endif

// T sat_atomic_sub_fetch(T* operand, T delta)
#if SAT_WITHOUT_SMP
  #define sat_atomic_sub_fetch(operand, delta) (*(operand) -= (delta))
#elif _SAT_ATOMIC_HAS_SYNC_BUILTINS
  #define sat_atomic_sub_fetch __sync_sub_and_fetch
#else
  #error "Unsupported compiler: Missing support for atomic operations"
#endif

// T sat_atomic_add_fetch(T* operand, T delta)
#if SAT_WITHOUT_SMP
  #define sat_atomic_add_fetch(operand, delta) (*(operand) += (delta))
#elif _SAT_ATOMIC_HAS_SYNC_BUILTINS
  #define sat_atomic_add_fetch __sync_add_and_fetch
#else
  #error "Unsupported compiler: Missing support for atomic operations"
#endif


// bool sat_atomic_cas_bool(T* ptr, T oldval, T newval)
#if SAT_WITHOUT_SMP
  #define sat_atomic_cas_bool(ptr, oldval, newval)  \
    (*(ptr) == (oldval) && (*(ptr) = (newval)))
#elif _SAT_ATOMIC_HAS_SYNC_BUILTINS
  #define sat_atomic_cas_bool(ptr, oldval, newval) \
    __sync_bool_compare_and_swap((ptr), (oldval), (newval))
#else
  #error "Unsupported compiler: Missing support for atomic operations"
#endif


// T sat_atomic_cas(T* ptr, T oldval, T newval)
#if SAT_WITHOUT_SMP
  #define sat_atomic_cas(ptr, oldval, newval)  \
    ({ __typeof__(oldval) prevv = *(ptr); \
       if (*(ptr) == (oldval)) { *(ptr) = (newval); } \
       prevv; })
#elif _SAT_ATOMIC_HAS_SYNC_BUILTINS
  #define sat_atomic_cas(ptr, oldval, newval) \
    __sync_val_compare_and_swap((ptr), (oldval), (newval))
#else
  #error "Unsupported compiler: Missing support for atomic operations"
#endif


// void sat_atomic_barrier()
#if SAT_WITHOUT_SMP
#define sat_atomic_barrier() do{}while(0)
#else
#define sat_atomic_barrier() __sync_synchronize()
#endif


// Spinlock
#ifdef __cplusplus
namespace sat {
typedef volatile i32 Spinlock;

#define SB_SPINLOCK_INIT 0  // usage: Spinlock lock = SB_SPINLOCK_INIT;

bool spinlock_try_lock(Spinlock& lock);
void spinlock_lock(Spinlock& lock);
void spinlock_unlock(Spinlock& lock);

bool spinlock_try_lock(Spinlock* lock);
void spinlock_lock(Spinlock* lock);
void spinlock_unlock(Spinlock* lock);

struct ScopedSpinlock {
  ScopedSpinlock(Spinlock& lock) : _lock(lock) { spinlock_lock(_lock); }
  ~ScopedSpinlock() { spinlock_unlock(_lock); }
private:
  Spinlock& _lock;
};

inline bool SAT_UNUSED spinlock_try_lock(Spinlock& lock) {
  return sat_atomic_cas_bool(&lock, (i32)0, (i32)1); }
inline void SAT_UNUSED spinlock_lock(Spinlock& lock) {
  while (!spinlock_try_lock(lock)); }
inline void SAT_UNUSED spinlock_unlock(Spinlock& lock) {
  lock = 0; }

inline bool SAT_UNUSED spinlock_try_lock(Spinlock* lock) {
  return sat_atomic_cas_bool(lock, (i32)0, (i32)1); }
inline void SAT_UNUSED spinlock_lock(Spinlock* lock) {
  while (!spinlock_try_lock(lock)); }
inline void SAT_UNUSED spinlock_unlock(Spinlock* lock) {
  *lock = 0; }

} // namespace
#endif // __cplusplus
