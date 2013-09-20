// Instrusive list-type data structures
#pragma once

namespace sat {
namespace list {

// ------------------------------------------------------------------------------------------------
// order: FIFO, thread-safety: none, links: 1

template <typename T> struct FIFO {
  FIFO() {} // empty list
  FIFO(T* e) {
    // e->_next_link = 0; // See push_back for explanation
    _front = _back = e;
  }
  template <typename... TL> inline FIFO(T* e, TL... rest) : FIFO(e) { push_back(rest...); }
  bool empty() const { return !_front; }
  T* front() const { return _front; }
  T* back() const { return _back; }

  void push_back(T* e) {
    assert(e != NULL);
    // e->_next_link = 0; // Don't do this, or we can't join lists
    if (_back) {
      assert(_back != e);
      _back->_next_link = e;
    } else {
      _front = e;
    }
    _back = e;
  }
  template <typename... TL> inline void push_back(T* e, TL... rest) {
    push_back(e); push_back(rest...);
  }

  T* pop_front() {
    T* e = _front;
    if (e == _back) {
      _front = 0;
      _back = 0;
    } else {
      _front = e->_next_link;
      e->_next_link = 0;
    }
    return e;
  }
protected:
  T* _front = 0;
  T* _back = 0;
};

// ------------------------------------------------------------------------------------------------
// foreach(T start, F(T))
//   F = ?(const T*)
//     | ?(const T&)
//     | ?(T*)
//     | ?(T&)

// These lil' tricks allow for highly efficient compiler optimizations
template <typename T, typename F>
static inline void _foreach_const(const T* e, F f) {
  f(e);
  if ((e = e->_next_link)) _foreach_const<T,F>(e, f); }

template <typename T, typename F>
static inline void _foreach_const(const T* e, size_t i, F f) {
  f(e, i);
  if ((e = e->_next_link)) _foreach_const<T,F>(e, i+1, f); }


template <typename T, typename F> void foreach(const T& e, F f) {
  f(e);
  if (e._next_link) foreach<T,F>((const T&)*e._next_link, f);
}

template <typename T, typename F> void foreach(const T* e, F f) {
  if (e) _foreach_const<T,F>(e, f); }
template <typename T, typename F> void foreach(const T* e, size_t i, F f) {
  if (e) _foreach_const<T,F>(e, i, f); }

template <typename T, typename F> void foreach(T& e, F f) {
  T* e2 = e._next_link;
  f(e);
  if (e2) foreach<T,F>(*e2, f);
}

template <typename T, typename F> void foreach(T* e, F f) {
  if (!e) return;
  T* e2 = e->_next_link;
  f(e);
  foreach<T,F>(e2, f);
}


}} // namespace sat::list
