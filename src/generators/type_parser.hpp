#ifndef TYPE_PARSER_HPP
#define TYPE_PARSER_HPP

#include <cctype>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace coconut::generator {

// ── Type representation ──────────────────────────────────────────────────────

struct Type;

struct Type;

/// Parameter inside a fun(…): … type.
/// Uses unique_ptr<Type> instead of optional<Type> to break the
/// circular dependency (FunParam → Type → FunParam).
struct FunParam {
  std::string name;
  std::unique_ptr<Type> type; // nullptr = untyped (shorthand)
};

/// Field inside a { … } table type.
struct TableField {
  std::string name;
  std::unique_ptr<Type> type;
  bool optional = false;
};

/// A parsed LuaCATS type.
///
/// Recursive — union children, function params/return, and table fields all
/// hold nested Type values.
struct Type {
  enum class Kind {
    Named,     ///< Built-in (string, number, ...) or user identifier
    Array,     ///< element[]
    Union,     ///< left | right  (flattened list)
    Function,  ///< fun(params): return
    Table,     ///< { fields }
    Vararg,    ///< ... (varargs, only valid in function params)
  };

  Kind kind = Kind::Named;

  // Named:
  std::string name;

  // Array:
  std::unique_ptr<Type> element;

  // Union (flat list of alternatives):
  std::vector<Type> alternatives;

  // Function:
  std::vector<FunParam> fun_params;
  std::unique_ptr<Type> fun_return;

  // Table:
  std::vector<TableField> fields;

  static Type makeNamed(std::string n) {
    Type t;
    t.kind = Kind::Named;
    t.name = std::move(n);
    return t;
  }

  static Type makeArray(Type elem) {
    Type t;
    t.kind = Kind::Array;
    t.element = std::make_unique<Type>(std::move(elem));
    return t;
  }

  static Type makeUnion(std::vector<Type> alts) {
    Type t;
    t.kind = Kind::Union;
    t.alternatives = std::move(alts);
    return t;
  }

  static Type makeFunction(std::vector<FunParam> params, Type ret) {
    Type t;
    t.kind = Kind::Function;
    t.fun_params = std::move(params);
    t.fun_return = std::make_unique<Type>(std::move(ret));
    return t;
  }

  static Type makeTable(std::vector<TableField> flds) {
    Type t;
    t.kind = Kind::Table;
    t.fields = std::move(flds);
    return t;
  }

  static Type makeVararg() {
    Type t;
    t.kind = Kind::Vararg;
    t.name = "...";
    return t;
  }
};

// ── Recursive-descent parser ─────────────────────────────────────────────────

/// Exception thrown on malformed type strings.
class TypeParseError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

namespace detail {

/// Cursor over a type string.
struct Cursor {
  const std::string& s;
  size_t pos = 0;

  char peek() const { return pos < s.size() ? s[pos] : '\0'; }
  char advance() { return pos < s.size() ? s[pos++] : '\0'; }
  void skipWs() {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos])))
      ++pos;
  }
  bool atEnd() const { return pos >= s.size(); }
  [[noreturn]] void fail(const std::string& msg) const {
    throw TypeParseError(msg + " at position " + std::to_string(pos));
  }

  /// Match an exact string (case-sensitive).
  bool match(const std::string& token) {
    if (s.substr(pos, token.size()) == token) {
      pos += token.size();
      return true;
    }
    return false;
  }

  /// Read an identifier: [%a_][%w_.]*
  std::string readIdentifier() {
    skipWs();
    size_t start = pos;
    if (pos < s.size() && (std::isalpha(static_cast<unsigned char>(s[pos])) ||
                           s[pos] == '_')) {
      ++pos;
      while (pos < s.size() &&
             (std::isalnum(static_cast<unsigned char>(s[pos])) ||
              s[pos] == '_' || s[pos] == '.'))
        ++pos;
    }
    return s.substr(start, pos - start);
  }
};

// Forward declarations
Type parseType(Cursor& cur);

// ── Singular ──────────────────────────────────────────────────────────────

/// Parse a single "λ" production:
///   λ := identifier | fun(...) | { ... } | ( γ ) | ...
Type parseSingular(Cursor& cur) {
  cur.skipWs();

  // fun(...)
  if (cur.match("fun")) {
    cur.skipWs();
    if (cur.peek() != '(')
      cur.fail("expected '(' after 'fun'");
    cur.advance(); // '('

    std::vector<FunParam> params;

    // Parse parameter list (possibly empty)
    cur.skipWs();
    if (cur.peek() != ')') {
      // First param
      while (true) {
        cur.skipWs();

        // Check for vararg
        if (cur.match("...")) {
          FunParam p;
          p.name = "...";
          p.type = std::make_unique<Type>(Type::makeVararg());
          params.push_back(std::move(p));
        } else {
          std::string pname = cur.readIdentifier();
          cur.skipWs();
          FunParam p;
          p.name = pname;

          if (cur.peek() == ':') {
            cur.advance(); // ':'
            cur.skipWs();
            // Optional-type shorthand: `name[: type]` 
            // The '[' here is part of "optional annotation", not array.
            // We handle it by checking if ':' is followed by '['.
            // But in practice LuaCATS uses `name?: type` for optional.
            if (cur.peek() == '[') {
              // `[: type]` — optional type annotation
              cur.advance(); // '['
              cur.skipWs();
              p.type = std::make_unique<Type>(parseType(cur));
              cur.skipWs();
              if (cur.peek() != ']')
                cur.fail("expected ']' closing optional type");
              cur.advance(); // ']'
            } else {
              p.type = std::make_unique<Type>(parseType(cur));
            }
          }
          params.push_back(std::move(p));
        }

        cur.skipWs();
        if (cur.peek() == ',') {
          cur.advance();
          continue;
        }
        break;
      }
    }

    cur.skipWs();
    if (cur.peek() != ')')
      cur.fail("expected ')' closing fun params");
    cur.advance(); // ')'

    cur.skipWs();
    if (cur.peek() != ':')
      cur.fail("expected ':' after fun(...)");
    cur.advance(); // ':'
    cur.skipWs();

    Type ret = parseType(cur);
    return Type::makeFunction(std::move(params), std::move(ret));
  }

  // { ... } — table type
  if (cur.peek() == '{') {
    cur.advance(); // '{'
    cur.skipWs();

    std::vector<TableField> fields;

    if (cur.peek() != '}') {
      while (true) {
        cur.skipWs();
        std::string fname = cur.readIdentifier();
        cur.skipWs();

        // Optional field: `name?` or `name: type` or `name?: type`
        // In LuaCATS: `name?: type` means optional field
        bool optional = false;
        if (cur.peek() == '?') {
          optional = true;
          cur.advance();
          cur.skipWs();
        }

        std::unique_ptr<Type> ftype;
        if (cur.peek() == ':') {
          cur.advance(); // ':'
          cur.skipWs();
          ftype = std::make_unique<Type>(parseType(cur));
        } else {
          ftype = std::make_unique<Type>(Type::makeNamed("any"));
        }

        TableField f;
        f.name = fname;
        f.type = std::move(ftype);
        f.optional = optional;
        fields.push_back(std::move(f));

        cur.skipWs();
        if (cur.peek() == ',') {
          cur.advance();
          continue;
        }
        break;
      }
    }

    cur.skipWs();
    if (cur.peek() != '}')
      cur.fail("expected '}' closing table type");
    cur.advance(); // '}'

    return Type::makeTable(std::move(fields));
  }

  // ( γ ) — grouped type
  if (cur.peek() == '(') {
    cur.advance(); // '('
    Type inner = parseType(cur);
    cur.skipWs();
    if (cur.peek() != ')')
      cur.fail("expected ')' closing grouped type");
    cur.advance(); // ')'
    return inner;
  }

  // Identifier or built-in
  std::string id = cur.readIdentifier();
  if (id.empty())
    cur.fail("expected type identifier");
  return Type::makeNamed(std::move(id));
}

// ── Full type ────────────────────────────────────────────────────────────

/// Parse a full "γ" production:
///   γ := λ ([])* (| γ)*
Type parseType(Cursor& cur) {
  Type result = parseSingular(cur);

  while (true) {
    cur.skipWs();

    // Array suffix: λ[]
    if (cur.peek() == '[') {
      // Peek: '[' followed by ']'
      size_t saved = cur.pos;
      cur.advance(); // '['
      if (cur.peek() == ']') {
        cur.advance(); // ']'
        result = Type::makeArray(std::move(result));
        continue;
      }
      // Not '[]' — restore and break; might be a grouped type or something else.
      // Actually this shouldn't happen since '[' is only valid as '[]' here.
      cur.pos = saved;
      break;
    }

    // Union suffix: γ | γ
    if (cur.peek() == '|') {
      cur.advance(); // '|'
      Type right = parseType(cur);

      // Flatten into alternatives list
      std::vector<Type> alts;
      if (result.kind == Type::Kind::Union) {
        alts = std::move(result.alternatives);
      } else {
        alts.push_back(std::move(result));
      }
      if (right.kind == Type::Kind::Union) {
        for (auto& a : right.alternatives)
          alts.push_back(std::move(a));
      } else {
        alts.push_back(std::move(right));
      }
      result = Type::makeUnion(std::move(alts));
      continue;
    }

    break;
  }

  return result;
}

} // namespace detail

/// Parse a LuaCATS type string into a type tree.
inline Type parseType(const std::string& input) {
  detail::Cursor cur{input, 0};
  Type result = detail::parseType(cur);
  cur.skipWs();
  if (!cur.atEnd())
    throw TypeParseError("unexpected trailing content: '" +
                         input.substr(cur.pos) + "'");
  return result;
}

// ── Formatters ───────────────────────────────────────────────────────────────

namespace detail {

void formatTypeLua(const Type& t, std::string& out);
void formatTypeJS(const Type& t, std::string& out);
void formatTypeTS(const Type& t, std::string& out);

void formatFunParamsLua(const std::vector<FunParam>& params,
                         std::string& out) {
  out += '(';
  for (size_t i = 0; i < params.size(); ++i) {
    if (i > 0) out += ", ";
    out += params[i].name;
    if (params[i].type != nullptr) {
      out += ": ";
      formatTypeLua(*params[i].type, out);
    }
  }
  out += ')';
}

void formatFunParamsJS(const std::vector<FunParam>& params,
                        std::string& out) {
  out += '(';
  for (size_t i = 0; i < params.size(); ++i) {
    if (i > 0) out += ", ";
    out += params[i].name;
    if (params[i].type != nullptr) {
      out += ": ";
      formatTypeJS(*params[i].type, out);
    }
  }
  out += ')';
}

void formatFunParamsTS(const std::vector<FunParam>& params,
                        std::string& out) {
  out += '(';
  for (size_t i = 0; i < params.size(); ++i) {
    if (i > 0) out += ", ";
    out += params[i].name;
    if (params[i].type != nullptr) {
      out += ": ";
      formatTypeTS(*params[i].type, out);
    }
  }
  out += ')';
}

} // namespace detail

/// Format back to a canonical LuaCATS string.
inline std::string formatTypeLua(const Type& t) {
  std::string out;
  detail::formatTypeLua(t, out);
  return out;
}

/// Format to a JSDoc-compatible string.
inline std::string formatTypeJS(const Type& t) {
  std::string out;
  detail::formatTypeJS(t, out);
  return out;
}

/// Format to a TypeScript type string (.d.ts).
inline std::string formatTypeTS(const Type& t) {
  std::string out;
  detail::formatTypeTS(t, out);
  return out;
}

namespace detail {

void formatTypeLua(const Type& t, std::string& out) {
  switch (t.kind) {
  case Type::Kind::Named:
    out += t.name;
    break;
  case Type::Kind::Vararg:
    out += "...";
    break;
  case Type::Kind::Array:
    formatTypeLua(*t.element, out);
    out += "[]";
    break;
  case Type::Kind::Union:
    for (size_t i = 0; i < t.alternatives.size(); ++i) {
      if (i > 0) out += " | ";
      formatTypeLua(t.alternatives[i], out);
    }
    break;
  case Type::Kind::Function:
    out += "fun";
    formatFunParamsLua(t.fun_params, out);
    out += ": ";
    formatTypeLua(*t.fun_return, out);
    break;
  case Type::Kind::Table:
    out += "{ ";
    for (size_t i = 0; i < t.fields.size(); ++i) {
      if (i > 0) out += ", ";
      out += t.fields[i].name;
      if (t.fields[i].optional) out += "?";
      out += ": ";
      formatTypeLua(*t.fields[i].type, out);
    }
    out += " }";
    break;
  }
}

void formatTypeJS(const Type& t, std::string& out) {
  switch (t.kind) {
  case Type::Kind::Named: {
    // Map Lua built-ins to JS equivalents
    if (t.name == "nil")
      out += "null | undefined";
    else if (t.name == "boolean")
      out += "boolean";
    else if (t.name == "number")
      out += "number";
    else if (t.name == "string")
      out += "string";
    else if (t.name == "integer")
      out += "number";
    else if (t.name == "any")
      out += "any";
    else if (t.name == "table")
      out += "object";
    else if (t.name == "thread" || t.name == "userdata")
      out += "object";
    else
      out += t.name; // pass through (user-defined types)
    break;
  }
  case Type::Kind::Vararg:
    out += "...any[]";
    break;
  case Type::Kind::Array:
    formatTypeJS(*t.element, out);
    out += "[]";
    break;
  case Type::Kind::Union:
    for (size_t i = 0; i < t.alternatives.size(); ++i) {
      if (i > 0) out += " | ";
      formatTypeJS(t.alternatives[i], out);
    }
    break;
  case Type::Kind::Function:
    // JSDoc: (param: type, ...) => return
    {
      out += '(';
      for (size_t i = 0; i < t.fun_params.size(); ++i) {
        if (i > 0) out += ", ";
        out += t.fun_params[i].name;
        if (t.fun_params[i].type != nullptr) {
          out += ": ";
          formatTypeJS(*t.fun_params[i].type, out);
        }
      }
      out += ')';
      out += " => ";
      formatTypeJS(*t.fun_return, out);
    }
    break;
  case Type::Kind::Table:
    out += '{';
    for (size_t i = 0; i < t.fields.size(); ++i) {
      if (i > 0) out += ", ";
      out += t.fields[i].name;
      if (t.fields[i].optional) out += "?";
      out += ": ";
      formatTypeJS(*t.fields[i].type, out);
    }
    out += '}';
    break;
  }
}

void formatTypeTS(const Type& t, std::string& out) {
  switch (t.kind) {
  case Type::Kind::Named: {
    if (t.name == "nil")
      out += "null | undefined";
    else if (t.name == "boolean")
      out += "boolean";
    else if (t.name == "number" || t.name == "integer")
      out += "number";
    else if (t.name == "string")
      out += "string";
    else if (t.name == "any")
      out += "any";
    else if (t.name == "table")
      out += "Record<string, unknown>";
    else
      out += t.name;
    break;
  }
  case Type::Kind::Vararg:
    out += "...any[]";
    break;
  case Type::Kind::Array:
    formatTypeTS(*t.element, out);
    out += "[]";
    break;
  case Type::Kind::Union:
    for (size_t i = 0; i < t.alternatives.size(); ++i) {
      if (i > 0) out += " | ";
      formatTypeTS(t.alternatives[i], out);
    }
    break;
  case Type::Kind::Function:
    formatFunParamsTS(t.fun_params, out);
    out += " => ";
    formatTypeTS(*t.fun_return, out);
    break;
  case Type::Kind::Table:
    out += '{';
    for (size_t i = 0; i < t.fields.size(); ++i) {
      if (i > 0) out += "; ";
      out += t.fields[i].name;
      if (t.fields[i].optional) out += "?";
      out += ": ";
      formatTypeTS(*t.fields[i].type, out);
    }
    out += '}';
    break;
  }
}

} // namespace detail

} // namespace coconut::generator

#endif // TYPE_PARSER_HPP
