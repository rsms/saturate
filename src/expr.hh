#pragma once
#include "common.h"
#include "str.hh"
#include "list.hh"
#include <ostream>
#include <iomanip>

namespace sat {

#define SAT_EXPR_TYPES \
  _(LIST)          /* (Expr+ \n | Expr+ ;)  `a b c`               */ \
  _(BLOCK)         /* \n (SP+ List)+        `\n  a b c\n  d e f`  */ \
  _(INLINE_BLOCK)  /* "{" List+ "}"         `{ a b c }`           */ \
  _(GROUP)         /* "(" List+ ")"         `( a b c )`           */ \
  _(COMMENT)       /* "#" [^\n]+ \n         `# lolcat\n`          */ \
  _(SYM)           /* [a-z_] SymChar*       `a`, `aBob`           */ \
  _(ATOM)          /* [A-Z] SymChar*        `A`, `Bob`            */ \
  _(ASSIGNMENT)    /* Sym ":"               `foo:`                */ \

struct Expr {
  // types
  enum class Type {
    UNDEFINED,
    #define _(name) name,
    SAT_EXPR_TYPES
    #undef _
  };

  // type creation and destruction
  Expr(Type t) : _type(t) {}
  Expr(Type t, Str::Imp* s) : _type{t}, _value{s} {}
  ~Expr();

  // properties
  Type type() const { return _type; }
  const Expr* next() const { return _next_link; }
  Expr* next() { return _next_link; }

  bool is_list() const {
    return (_type == Type::LIST
         || _type == Type::INLINE_BLOCK
         || _type == Type::BLOCK
         || _type == Type::GROUP);
  }
  bool is_str() const { return (_type == Type::SYM
                             || _type == Type::ATOM
                             || _type == Type::ASSIGNMENT
                             || _type == Type::COMMENT); }

  const Str::Imp* str_value() const {
    assert(is_str());
    return _value.s;
  }

  // data
  Type _type;
  Expr* _next_link = 0;
  union Value {
    Expr* head; // used by LIST
    Str::Imp* s;
    i64 i;
    double f;
    Value() : s(0) {}
    Value(Str::Imp* s) : s(s) {}
  } _value;

  // functions

  static const char* type_name(Type t) { switch (t) {
    case Type::UNDEFINED: return "UNDEFINED";
    #define _(name) case Type::name: return #name;
    SAT_EXPR_TYPES
    #undef _
  }}

  std::ostream& print(std::ostream& os, int indent_level=0) const;
};


inline static std::ostream& operator<< (std::ostream& os, const Expr& e) {
  return e.print(os); }

inline static std::ostream& operator<< (std::ostream& os, const Expr* e) {
  return e ? e->print(os) : os << "NULL"; }

} // namespace sat
