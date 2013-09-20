#include "expr.hh"
#include "defer.hh"

namespace sat {

static std::ostream& _repr_each(std::ostream&, const Expr* head, int, Expr::Type parent_type);

static std::ostream& _repr(
  const Expr& e,
  std::ostream& os,
  int indent_level,
  bool is_first,
  bool is_last,
  Expr::Type parent_type)
{
  // os << '<' << Expr::type_name(e.type()) << '>';
  // defer [&]{ os << "</" << Expr::type_name(e.type()) << '>'; };

  switch (e.type()) {
    // Pretty-print like this when we rewrite this function to keep track of printing indentation
    // level:
    case Expr::Type::BLOCK: {
      assert(e._value.head);
      // if (!is_first) os << '\n';
      return _repr_each(os, e._value.head, indent_level+1, e.type());
    }
    case Expr::Type::INLINE_BLOCK: {
      if (!is_first) os << ' ';
      os << "{ ";
      return _repr_each(os, e._value.head, indent_level, e.type()) << " }";
    }
    case Expr::Type::LIST: {
      if (parent_type == Expr::Type::INLINE_BLOCK) {
        if (!is_first)
          os << "; ";
      } else if ( (indent_level > 0 || !is_first) && parent_type != Expr::Type::GROUP) {
        os << '\n' << std::setw(indent_level*2) << "";
      }
      return _repr_each(os, e._value.head, indent_level, parent_type);
    }
    case Expr::Type::GROUP: {
      if (!is_first) os << ' ';
      os << '(';
      return _repr_each(os, e._value.head, indent_level, e.type()) << ")";
    }
    case Expr::Type::COMMENT: {
      if (!is_first) os << ' ';
      os << "#" << e.str_value();
      assert(is_last); // os << '\n';
      return os;
    }
    case Expr::Type::SYM:
    case Expr::Type::ATOM: {
      if (!is_first) os << ' ';
      return os << e.str_value();
    }
    case Expr::Type::ASSIGNMENT: {
      if (!is_first) os << ' ';
      return os << e.str_value() << ':';
    }
    default: {
      if (!is_first) os << ' ';
      os << "#!" << Expr::type_name(e.type());
      if (!is_last) os << '\n';
      return os;
    }
  } // switch (type)
}


static std::ostream& _repr_each(
  std::ostream& os,
  const Expr* head,
  int indent_level,
  Expr::Type parent_type)
{
  list::foreach(head, 0, [&](const Expr* e, size_t i) {
    _repr(*e, os, indent_level, i == 0, !e->_next_link, parent_type);
  });
  return os;
}


std::ostream& Expr::print(std::ostream& os, int indent_level) const {
  return _repr(*this, os, indent_level, true, !_next_link, Expr::Type::UNDEFINED);
}


Expr::~Expr() {
  // printf("~Expr: this = %p\n"
  //        "       _type = %s\n"
  //        "       _next_link = %p\n"
  //        "       repr => "
  //        ,this
  //        ,type_name(_type)
  //        ,_next_link );
  // std::cout << this << std::endl;

  if (is_list()) {
    if (_value.head) {
      delete _value.head;
    }
  } else if (is_str()) {
    // printf("~Expr: _value.s->__release() where\n"
    //        "  s = %p\n"
    //        "  s->c_str() = '%s'\n"
    //        "  s->__refcount = %u\n",
    //        _value.s, _value.s->c_str(), _value.s->__refcount);
    Str::__release(_value.s);
  }

  if (_next_link) {
    delete _next_link;
  }
}

} // namespace sat
