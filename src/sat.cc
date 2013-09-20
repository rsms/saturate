// clang -std=c++11 -stdlib=libc++ -O1 -g -Wall -lc++ -o sat-parse sat2.cc && ./sat-parse < foo.sat
#include "common.h"
#include "hash.hh"
#include "str.hh"
#include "list.hh"
#include "defer.hh"
#include "expr.hh"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <assert.h>
#include <string>
#include <deque>
#include <list>
#include <map>
#include <sstream>
#include <iostream>
#include <iomanip>

#include <unistd.h> // sysconf(), isatty()

namespace sat {

template <typename... Tn> static void printx(Tn... args) {
  pass( (std::clog << args)... );
  std::clog << std::endl;
}

// todo: move these into `list` ns:
template <typename T, typename F>
static inline void foreach_const(const T* e, F f) {
  f(e); if ((e = (T*)e->_next_link)) foreach_const<T,F>(e, f); }

template <typename T, typename F>
static inline void foreach(T* e, F f) {
  if (!e) return;
  T* e2 = e->_next_link;
  f(e);
  foreach<T,F>(e2, f);
}

template <typename T, typename F>
std::string join_const(const T* e, const std::string& glue, F filter) {
  std::ostringstream ss;
  bool nth = false;
  foreach_const(e, [&](const T* e) {
    if (nth) { ss << glue; } else { nth = true; }
    ss << filter(e);
  });
  return ss.str();
}

// ----------------------------------------------------------------------------------------------

#define SAT_ERRORS \
  _(Parse) \
  _(Syntax) \
  _(Indentation) \
  _(Memory) \

enum class Error {
  #define _(name) name,
  SAT_ERRORS
  #undef _
};

static const char* ErrorName(Error e) { switch (e) {
  #define _(name) case Error::name: return #name;
  SAT_ERRORS
  #undef _
}}

// ----------------------------------------------------------------------------------------------
// Interned strings and symbols

#define CONST_SYMBOLS \
  F(user, "user") \
  F(user_ns, "user:") \

#define F(name, cstr) \
static constexpr auto kStr_##name = ConstStr(cstr);
CONST_SYMBOLS
#undef F

static Str::WeakSet strings{
  // Initialize the map with our constant symbols
  #define F(name, cstr) &kStr_##name,
  CONST_SYMBOLS
  #undef F
};

// --------------------------------------------------------------------------------------------

struct Namespace {
  // Maps names to expressions

  Namespace(Str&& qname) : _qname{qname} {}
  Namespace(Str&& qname, const Str::Map<Expr*>& import_names)
    : _qname{qname}, _names{import_names} {}

  const Str& name() const { return _qname; }

  Str             _qname;
    // Qualified name, i.e. "user:foo:bar:". Always ends in ":".
  Str::Map<Expr*> _names;
    // Unqualified names to expressions defined in this namespace. Filled in during parsing and
    // accessed during evaluation (for looking up symbols).
};


#define SAT_SCOPE_TYPES \
  /* Should match the names of the `Expr::Type` they represent */ \
  _(LIST) \
  _(BLOCK) \
  _(INLINE_BLOCK) \
  _(GROUP) \

struct Scope {
  // Represents some kind of list of expressions during parsing

  enum class Type {
    #define _(NAME) NAME,
    SAT_SCOPE_TYPES
    #undef _
  };
  static const char* type_name(Type t) { switch (t) {
    #define _(NAME) case Type::NAME: return #NAME;
    SAT_SCOPE_TYPES
    #undef _
  }}

  Scope(Type t, int il, Namespace* ns)
    : _type(t), _indent_level(il), _ns(ns) {}

  Type type() const { return _type; }
  int indent_level() const { return _indent_level; }
  Namespace* ns() const { return _ns; }

  Expr* expr_list() const { return _list; }
  void expr_list_append(Expr* expr) {
    if (!_list_tail) {
      Expr::Type list_type;
      switch (_type) {
        #define _(NAME) case Type::NAME: list_type = Expr::Type::NAME; break;
        SAT_SCOPE_TYPES
        #undef _
      }
      _list = new Expr{list_type};
      _list->_value.head = expr;
    } else {
      _list_tail->_next_link = expr;
    }
    _list_tail = expr;
  }

  // data
  Type        _type;
  int         _indent_level;
  Namespace*  _ns; // weak
    // What namespace this scope is operating in. In most cases this is no different from its
    // parent scope.
  Expr* _list = 0;
  Expr* _list_tail = 0;
};


// Read system memory page size. Parser::Buf uses this value in an advisory manner.
static long MEM_PAGE_SIZE = -1;
struct _MEM_PAGE_SIZE { _MEM_PAGE_SIZE() {
  #if defined(PAGESIZE)
  MEM_PAGE_SIZE = sysconf(PAGESIZE);
  #elif defined(PAGE_SIZE)
  MEM_PAGE_SIZE = sysconf(PAGE_SIZE);
  #elif defined(_SC_PAGESIZE)
  MEM_PAGE_SIZE = sysconf(_SC_PAGESIZE);
  #else
  MEM_PAGE_SIZE = -1;
  #endif
  if (MEM_PAGE_SIZE == -1 || (MEM_PAGE_SIZE/8)*8 != MEM_PAGE_SIZE) { MEM_PAGE_SIZE = 4096; }
}} __MEM_PAGE_SIZE;


struct Parser {
  enum class Status {
    ERROR,  // There was an error. Caller should either stop parsing or repair the error and retry
    RESULT, // There's results available by calling `next_result()`
    MORE,   // Parser needs more data. Call `fill()` and `parse()` to resume.
    DONE,   // There's nothing more to parse.
  };

  Parser(Str ns_qname, Namespace* parent_ns=0) {
    Namespace* ns = new Namespace{std::move(ns_qname)};
    Scope* scope = new Scope{Scope::Type::BLOCK, 0, ns};
    _scope_stack.emplace_front(scope);
  }

  size_t lineno() const { return _lineno+1; }
  size_t colno() const { return (size_t)(_buf.p - _buf.line_s)+1; }
  Scope& top_scope() const { assert(!_scope_stack.empty()); return *_scope_stack.front(); }

  // -------------------------------------------
  // BEGIN logging

  struct SEndl {
    SEndl(std::ostream& os) : _valid(true), _os(os) {}
    SEndl(const SEndl& other) : _valid(true), _os(other._os) {
      const_cast<SEndl&>(other)._valid = false; }
    ~SEndl() { if (_valid) { _os << std::endl; } }
    template <typename... Args>
    SEndl& operator<<(Args&&... args) { pass( (_os << args)... ); return *this; }
    // template <typename T> SEndl& operator<<(T& v) { _os << v; return *this; }
    bool _valid;
    std::ostream& _os;
  };

  SEndl dlog(std::ostream& os=std::cerr) {
    Namespace* ns = current_ns();
    return SEndl{
      os << std::left << std::setw(25) << (ns ? ns->name() : "")
         << std::setw((top_scope().indent_level()*2)+1) << ' ' };
         // << std::setw((_scope_stack.size()*2)+1) << ' ' };
  }

  struct ELog {
    ELog(std::ostream& os, ssize_t lineno, ssize_t column, const char* startp, const char* endp)
      : _valid(true), _os(os), _lineno(lineno), _column(column), _startp(startp), _endp(endp) {}
    ELog(const ELog& other)
      : _valid(true), _os(other._os), _lineno(other._lineno), _column(other._column)
      , _startp(other._startp), _endp(other._endp)
    {
      const_cast<ELog&>(other)._valid = false;
    }
    ~ELog() {
      if (_valid) {
        if (_lineno || _column) {
          _os << " at " << _lineno << ':' << _column;
        }
        _os << std::endl;
        if (_startp) {
          if (!_endp) { _endp = _startp; while (*_endp && *_endp != '\n') { ++_endp; } }
          _os << "  " << std::string(_startp, size_t(_endp - _startp)) << std::endl
              << "  " << std::right << std::setw(_column) << "^" << std::endl;
        }
      }
    }
    template <typename... Args>
    ELog& operator<<(Args&&... args) { pass( (_os << args)... ); return *this; }
    operator bool() const { return false; }
    operator Status() const { return Status::ERROR; }

    bool _valid;
    std::ostream& _os;
    ssize_t _lineno;
    ssize_t _column;
    const char* _startp;
    const char* _endp;
  };

  ELog report_error(
    Error e,
    const char* startp = (const char*)-1,
    const char* endp = 0,
    ssize_t line = -1,
    ssize_t col = 0)
  {
    // set to start and end of current line
    if (startp == (const char*)-1) {
      // use current values
      startp = _buf.line_s;
      endp = _buf.p > startp ? _buf.p : startp;
      while (endp != _buf.e && *endp != '\n') { ++endp; }
    }
    if (line == -1) {
      line = lineno();
      col = colno();
    }
    return ELog{std::cerr, line, col, startp, endp} << ErrorName(e) << "Error: ";
  }

  // END logging
  // -------------------------------------------


  std::string scope_path() {
    std::string s;
    int i = 0;
    auto I = _scope_stack.crbegin();
    auto E = _scope_stack.crend();
    for (;I != E; ++I) {
      if (i++) {
        s.append(1, '/');
        s.append("_");
      } else s.append("@");
    }
    return s;
  }

  bool is_root_scope(const Scope& s) {
    return &s == _scope_stack.back();
  }

  Namespace* current_ns() {
    if (_scope_stack.empty()) return 0;
    return _scope_stack.front()->ns();
  }

  bool enter_scope(Scope::Type scope_type) {
    Namespace* ns = current_ns();
    Scope* scope = new Scope{scope_type, _curr_indent_level, ns};
    _scope_stack.emplace_front(scope);
    dlog() << "-> " << Scope::type_name(scope->type())
           << " at level " << scope->indent_level();
    return true;
  }


  bool leave_scope(Scope::Type scope_type) {
    dlog() << "<< leave " << Scope::type_name(scope_type) << " scope"
           << " to level " << _curr_indent_level;
    assert(!_scope_stack.empty());
    
    if (top_scope().type() != scope_type) {
      // Error: Type mis-match
      auto descr = [](Scope::Type scope_type) -> const char* {
        switch (scope_type) {
        case Scope::Type::LIST:         return "linebreak to same indentation level or ';'";
        case Scope::Type::BLOCK:        return "block dentation";
        case Scope::Type::INLINE_BLOCK: return "'}'";
        case Scope::Type::GROUP:        return "')'";
      }};
      return report_error(Error::Indentation)
        << "Unexpected " << descr(scope_type)
        << " when expecting " << descr(top_scope().type()) << ".";
    }

    do {
      if (!pop_scope()) return false;
      if (scope_type != Scope::Type::BLOCK) return true;

      // Now, consider the parent scope. Are we at the target indent level?
      Scope& scope = top_scope();

      if (scope.type() == Scope::Type::GROUP) {
        dlog() << "-- inline group";
        return true;
      }

      if (_curr_indent_level > scope.indent_level()) {
        // Fell below target -- misalinged indentation. Break to error case.
        break;
      } else if (_curr_indent_level == scope.indent_level()) {
        // We are at the correct scope. Return with success.
        return true;
      }
    } while (!_scope_stack.empty());

    return report_error(Error::Indentation)
      << "unindent does not match any outer indentation level";
  }


  bool pop_scope() {
    assert(!_scope_stack.empty());
    Scope* prev_scope = _scope_stack.front();
    dlog() << "<- " << Scope::type_name(prev_scope->type())
           << " at level " << prev_scope->indent_level();
    _scope_stack.pop_front();

    // Take care of any expressions in the scope we just left
    Expr* prev_expr_list = prev_scope->expr_list();
    if (prev_expr_list) {
      assert(prev_expr_list->is_list());
      if (is_root_scope(top_scope())) {
        // As we are at the root scope, yield results
        yield_result(prev_expr_list);
      } else {
        top_scope().expr_list_append(prev_expr_list);
      }
    }

    delete prev_scope;
      // We no longer need prev_scope

    return true;
  }

  void yield_result(Expr* expr) {
    // dlog() << "yield_result " << expr;
    _results.push_back(expr);
  }

  #define TOKEN_NAMES \
    F(COMMENT) \
    F(NAME) \
    F(QUALNAME) \
    F(ASSIGNMENT) \

  #define READ_STATES \
    F(ROOT) \
    F(COMMENT) \
    F(LINEBREAK) \
    F(NAME) \
    F(QUALNAME) \
    F(ASSIGNMENT) \

  enum class Token {
    #define F(n) n,
    TOKEN_NAMES
    #undef F
  };

  static const char* token_name(Token v) {
    switch (v) {
      #define F(n) case Token::n: return #n;
      TOKEN_NAMES
      #undef F
    }
  }

  std::string copy_symbol_name(size_t skipn_tail=0) {
    return std::string(_buf.ts, (size_t)(_buf.te - _buf.ts - skipn_tail));
  }

  bool on_token(Token t) {
    dlog() << "■ " << token_name(t)
           << " \"" << std::string(_buf.ts, size_t(_buf.te-_buf.ts)) << "\"";
    assert(!_scope_stack.empty());

    Expr::Type type;
    size_t len = (u32)(_buf.te - _buf.ts);

    switch (t) {
      case Token::COMMENT:   type = Expr::Type::COMMENT; break;
      case Token::ASSIGNMENT: {
        assert(len > 0);
        assert(_buf.ts[len-1] == ':'); // or our parse code is bad
        --len; // skip 
        type = Expr::Type::ASSIGNMENT;
        break;
      }
      default: type = Expr::Type::SYM;
    }

    if (len > (size_t)0xffffffffu) {
      return report_error(Error::Memory, "String too large");
    }
    
    Str s = (t == Token::COMMENT) ? std::move(Str{_buf.ts, (u32)len}) :
                                    strings.get(_buf.ts, (u32)len);
                                    // intern all but comments
    Expr* expr = new Expr{type, s.steal_self()};
    top_scope().expr_list_append(expr);
    return true;
  }

  // --------------------------------------------------------------------
  // Reading

  enum class ReadState {
    #define F(n) n,
    READ_STATES
    #undef F
  };

  static const char* read_state_name(ReadState v) {
    switch (v) {
      #define F(n) case ReadState::n: return #n;
      READ_STATES
      #undef F
    }
  }

  struct Buf {
    bool  is_end = false; // true if there will be no more buffer fills
    char* line_s = 0;     // current line start in buffer

    char* ts = 0;    // current/last token start in buffer
    char* te = 0;    // current/last token end in buffer

    size_t size = 0; // size of memory region pointed to by `s`
    char* s = 0;     // buffer start
    char* p = 0;     // current buffer position
    char* e = 0;     // end of data in buffer

    char* ensure_fillable(size_t& bytes_available) {
      const size_t SIZE_LOW_WATERMARK = 512;
      bytes_available = size - (size_t)(p - s);
      if (bytes_available < SIZE_LOW_WATERMARK) {
        size += MEM_PAGE_SIZE;
        bytes_available += MEM_PAGE_SIZE;
        char* s2 = (char*)realloc((void*)s, size);
        if (!s2) { return 0; } // errno ENOMEM
        if (s2 != s) {
          // reallocate pointers if we were moved to a different region
          line_s = s2 + (line_s - s);
          ts     = s2 + (ts - s);
          te     = s2 + (te - s);
          p      = s2 + (p - s);
          e      = s2 + (e - s);
        }
        s = s2;
      }
      return e;
    }
    ~Buf() { if (s) free(s); }
  };


  char* get_read_buf(size_t& bytes_available) {
    assert(_buf.p == _buf.e); // or previous call to parse() failed with an error
    return _buf.ensure_fillable(bytes_available);
  }


  void fill(char* p, size_t len, bool is_end) {
    assert(p >= &_buf.s[0] && p < (&_buf.s[0])+_buf.size);
      // or this is a pointer to something else
    _buf.e += len;
    _buf.is_end = is_end;
  }
  

  Status parse() {
    //
    // Input bytes:    foo ba | r baz lolc | at\n
    //                 0....5   0........9   0.2
    //
    // Parsed tokens:  foo, bar, baz, lolcat, \n
    //   foo -> foo
    //   ba...
    //   r -> bar
    //   baz -> baz
    //   lolc...
    //   at -> lolcat
    // -------------------------------------------
    #define B /* current byte */ ((unsigned char)*_buf.p)
    #define Bn(n) /* nth byte */ ((unsigned char)(_buf.p < _buf.e-(n) ? *(_buf.p+(n)) : 0))
    #define CONSUME ++_buf.p;
    #define SET_TOK_START _buf.ts = _buf.p;
    #define SET_TOK_END   _buf.te = _buf.p;

    #define SWITCH_TO(state_name) { \
      /*dprintf(">> %s --> " #state_name, read_state_name(_read_state));*/ \
      _read_state = ReadState::state_name; \
      goto read_loop; \
    }
    #define CONSUME_AND_CONTINUE_AS(state_name) { \
      CONSUME \
      SWITCH_TO(state_name) \
    }
    #define TRANSITION_TO(state_name) { \
      SET_TOK_START \
      SWITCH_TO(state_name) \
    }
    #define TRANSITION_TO_AND_CONSUME(state_name) { \
      SET_TOK_START \
      CONSUME \
      SWITCH_TO(state_name) \
    }
    #define CONSUME_AND_TRANSITION_TO(state_name) { \
      CONSUME \
      SET_TOK_START \
      SWITCH_TO(state_name) \
    }

    #define ACT_ENTER_LINEBREAK { \
      _curr_indent_level = 0; \
      ++_lineno; \
      _buf.line_s = _buf.p+1; /* +1 since linebreak has not yet been CONSUMEd */ \
    }

    #define ACT_ON_SPACE { \
      if (_indent_c == 0) { \
        _indent_c = B; \
      } else if (_indent_c != B) { \
        return report_error(Error::Indentation) \
          << "Mixed line indentation"; \
      } \
      ++_curr_indent_level; \
    }

    #define LEAVE_BLOCK_SCOPE \
      if (!leave_scope(Scope::Type::LIST) || !leave_scope(Scope::Type::BLOCK)) { \
        return Status::ERROR; \
      }

    #define LEAVE_BLOCK_SCOPE_FROM_ENDPAREN \
      if (_scope_stack.size() < 3) { \
        return report_error(Error::Syntax) << "Unexpected ')'"; \
      } \
      _curr_indent_level = _scope_stack[2]->indent_level(); \
      LEAVE_BLOCK_SCOPE \
      assert(_scope_stack.size() > 1); \
      assert(_scope_stack[0]->type() == Scope::Type::LIST); \
      assert(_scope_stack[1]->type() == Scope::Type::GROUP); \
      _prev_indent_level = _curr_indent_level;

    // #define IS_SPACE \
    //   (  B == 0x09 /* CHARACTER TABULATION */ \
    //   || B == 0x20 /* SPACE */ \
    //   || B == 0xa0 /* NO-BREAK SPACE */ \
    //   ) // todo: Beyond ASCII UTF-8 spaces

    #define IS_CTRL \
      ( B < 0x9 || B == 0xb || B == 0xc || (B > 0xd && B < 0x20) )

    #define IS_NAME (B > 0x20 && B != '\\' && !(B > 0x7e && B < 0xa1) \
                     && B != '(' && B != ')' \
                     && B != '{' && B != '}' \
                     && B != ';' \
                     /*&& B != '='*/ )

    read_loop:
    while (_buf.p != _buf.e) switch (_read_state) {

      // ---------------------------------------------------------------------------
      case ReadState::ROOT: {
      if (!_results.empty()) {
        return Status::RESULT;
      }
      switch (B) {
        case '\n': {
          ACT_ENTER_LINEBREAK
          CONSUME_AND_TRANSITION_TO(LINEBREAK)
        }
        case '#': {
          CONSUME_AND_TRANSITION_TO(COMMENT)
        }
        case '(': {
          if (!enter_scope(Scope::Type::GROUP) || !enter_scope(Scope::Type::LIST)) {
            return Status::ERROR;
          }
          CONSUME
          break;
        }
        case ')': {
          assert(_scope_stack.size() > 1);
          if (_scope_stack[1]->type() == Scope::Type::BLOCK) {
            // Special case: Leaving a block scope inside a group w/o a trailing linebreak
            //   a
            //     (b
            //      c)
            //       ^-- We are here and should leave to...
            //     ^-- ...here
            LEAVE_BLOCK_SCOPE_FROM_ENDPAREN
          }
          if (!leave_scope(Scope::Type::LIST) || !leave_scope(Scope::Type::GROUP)) {
            return Status::ERROR;
          }
          CONSUME
          break;
        }
        case '{': {
          if (!enter_scope(Scope::Type::INLINE_BLOCK) || !enter_scope(Scope::Type::LIST)) {
            return Status::ERROR;
          }
          CONSUME
          break;
        }
        case '}': {
          if (!leave_scope(Scope::Type::LIST) || !leave_scope(Scope::Type::INLINE_BLOCK)) {
            return Status::ERROR;
          }
          CONSUME
          break;
        }
        case ';': {
          if (!leave_scope(Scope::Type::LIST) || !enter_scope(Scope::Type::LIST)) {
            return Status::ERROR;
          }
          CONSUME
          break;
        }
        default: {
          if (B < 0x21) {
            // ignore control chars et al
            CONSUME
            break;
          }
          if (IS_NAME) {
            TRANSITION_TO_AND_CONSUME(NAME)
          }
          return report_error(Error::Parse)
            << "Unexpected input '" << B << "' 0x" << std::hex << (unsigned)B;
        }
      } break; };

      // ---------------------------------------------------------------------------
      case ReadState::NAME: if (!IS_NAME) {
        // ! "x"
        SET_TOK_END
        if (copy_symbol_name() == "__END__") {
          _buf.is_end = true;
          _buf.e = _buf.p;
          break;
        }
        if (!on_token(Token::NAME)) return Status::ERROR;
        TRANSITION_TO(ROOT)
      } else if (B == ':') {
        // "x:"
        // not NAME "x", but ASSIGNMENT "x:" and possibly QUALNAME "x:y"
        CONSUME_AND_CONTINUE_AS(ASSIGNMENT)
      } else {
        CONSUME
      } break;

      // ---------------------------------------------------------------------------
      case ReadState::ASSIGNMENT: if (!IS_NAME) {
        // ! ("x:" | "x:y:")
        SET_TOK_END
        if (!on_token(Token::ASSIGNMENT)) return Status::ERROR;
        TRANSITION_TO(ROOT)
      } else if (B == ':') {
        // "x::"
        return report_error(Error::Syntax) << "Unexpected extra ':'";
      } else {
        // "x:y"
        CONSUME_AND_CONTINUE_AS(QUALNAME)
      } break;

      // ---------------------------------------------------------------------------
      case ReadState::QUALNAME: if (!IS_NAME) {
        // ! "x:y"
        SET_TOK_END
        if (!on_token(Token::QUALNAME)) return Status::ERROR;
        TRANSITION_TO(ROOT)
      } else if (B == ':') {
        // "x:y:"
        CONSUME_AND_CONTINUE_AS(ASSIGNMENT)
      } else {
        CONSUME
      } break;

      // ---------------------------------------------------------------------------
      case ReadState::LINEBREAK: switch (B) {
        case '\n': {
          ACT_ENTER_LINEBREAK
          CONSUME
          break;
        }
        case ' ': case '\t': case 0xa0: /* NBSP */ {
          ACT_ON_SPACE
          CONSUME
          break;
        }
        case ')': {
          LEAVE_BLOCK_SCOPE_FROM_ENDPAREN
          TRANSITION_TO(ROOT)
        }
        default: {
          if (IS_CTRL) {
            CONSUME
            break;
          }
          // Leave
          SET_TOK_END
          if (_prev_indent_level == -1) {
            // Special case: We just passed inital whitespace in input buffer
            // dlog() << "Passed initial whitespace in input buffer";
            if (_curr_indent_level != 0 /*&& _lineno != 1*/) {
              // First non-comment line of input must be at level 0
              return report_error(Error::Indentation) << "Unexpected indent";
            }
            if (!enter_scope(Scope::Type::LIST)) {
              return Status::ERROR;
            }

          } else if (_prev_indent_level < _curr_indent_level) {
            // Indentation increased
            //   |a
            //   |  b
            //   |  ^-- we are here
            //  ...
            if (!enter_scope(Scope::Type::BLOCK) || !enter_scope(Scope::Type::LIST)) {
              return Status::ERROR;
            }

          } else if (_prev_indent_level > _curr_indent_level) {
            // Indentation decreased
            //   |a
            //   |  b
            //   |c
            //   |^-- we are here
            //  ...
            LEAVE_BLOCK_SCOPE
            if (!leave_scope(Scope::Type::LIST) || !enter_scope(Scope::Type::LIST)) {
              return Status::ERROR;
            }

          } else {
            // newline to same indentation level means "new line scope"
            // if (Bn(1) != '\\') ... // <- TODO: "A\n\B" == "A B"
            if (!leave_scope(Scope::Type::LIST) || !enter_scope(Scope::Type::LIST)) {
              return Status::ERROR;
            }
          }

          _prev_indent_level = _curr_indent_level;
          TRANSITION_TO(ROOT)
        }
      } break;

      // ---------------------------------------------------------------------------
      case ReadState::COMMENT: switch (B) {
        case '\n': {
          SET_TOK_END
          if (!on_token(Token::COMMENT)) return Status::ERROR;
          TRANSITION_TO(ROOT)
        }
        default: {
          CONSUME
          break;
        }
      } break;

      // ---------------------------------------------------------------------------
    } // read_loop: while (_buf.p != _buf.e) switch (_read_state)

    if (_buf.is_end) {
      // We have reached the end of input
      assert(!_scope_stack.empty());
      if (!is_root_scope(top_scope())) {
        // Special case: The source ends with an indented block. Leave to our root block.
        _curr_indent_level = 0;
        if (_prev_indent_level != -1) {
          // There was at least one thing in the input, which means there's a line scope we must
          // leave before leaving the root block scope.
          if (!leave_scope(Scope::Type::LIST)) {
            return Status::ERROR;
          }
        } // else: Empty input

        if (!is_root_scope(top_scope())) {
          report_error(Error::Parse) << "Unexpected end of input"; // TODO: Work on this one
        }
      }
      // Note: Calling end_list when the scope is empty has no effect, so it's safe to call this
      // multiple times, i.e. if the caller invokes `parse()` again after it returns `DONE`.
      return _results.empty() ? Status::DONE : Status::RESULT;
    }

    return Status::MORE;
  }


  Expr* next_result() {
    return _results.pop_front();
  }


  Buf                 _buf;
  size_t              _lineno = 0;              // current line number
  int                 _prev_indent_level = -1;  // previous line indentation level
  int                 _curr_indent_level = 0;  // current line indentation level
  char                _indent_c = 0;            // type of line indentation
  std::deque<Scope*>  _scope_stack;
  Expr*               _expr_tail = 0;           // tail of current expression list
  ReadState           _read_state = ReadState::LINEBREAK;
  list::FIFO<Expr>    _results;  // Queue of expressions ready to e.g. be evaulated
};

// ------------------------------------------------------------------------------------------------


} // namespace sat

// ------------------------------------------------------------------------------------------------

using namespace sat;

int main(int argc, const char** argv) {
  FILE* fp = stdin;

  if (argc > 1) {
    if ( (fp = fopen(argv[1], "r")) == NULL ) {
      fprintf(stderr, "%s: No such file '%s'\n", argv[0], argv[1]);
      return 1;
    }
  } else if (isatty(0)) {
    fprintf(stderr, "usage: %s <file>\n", argv[0]);
    return 1;
  } // else printf("Reading from stdin\n");

  Parser P(kStr_user_ns);
  bool is_eof = false;

  while (!is_eof) {
    size_t bufsize;
    char* buf = P.get_read_buf(bufsize);
    // printf("bufsize: %zu\n", bufsize); bufsize = 4; // debug
    assert(bufsize > 0 && "TODO: parser: fill move pending in buffer");
    assert(bufsize < SIZE_MAX/2);
    size_t len = fread(buf, 1, bufsize, fp);
    is_eof = (len < bufsize);
    P.fill(buf, len, is_eof);

    parse:
    switch (P.parse()) {
      case Parser::Status::ERROR: {
        printf("main: Parser::Status::ERROR\n");
        return 1;
      }
      case Parser::Status::RESULT: {
        printf("main: Parser::Status::RESULT\n");
        Expr* e;
        while ( (e = P.next_result()) ) {
          std::cout << "result: " << e << std::endl;
          delete e;
        }
        // todo eval
        goto parse;
      }
      case Parser::Status::MORE: {
        printf("main: Parser::Status::MORE\n");
        assert(!is_eof);
        break;
      }
      case Parser::Status::DONE: {
        printf("main: Parser::Status::DONE\n");
        assert(is_eof);
        break;
      }
    }
  }

  return 0;
}
