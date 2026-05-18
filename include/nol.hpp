#ifndef NOL_HPP
#define NOL_HPP
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <stdexcept>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

namespace NOL {
class Value;
using Object = std::map<std::string, Value>;
using Array = std::vector<Value>;
class Value {
public:
    enum Type { Null, Bool, Int, Float, String, ArrayType, ObjectType };
    std::variant<std::nullptr_t, bool, int64_t, double, std::string, Array, Object> data;
    Value() : data(nullptr) {}
    Value(std::nullptr_t) : data(nullptr) {}
    Value(bool b) : data(b) {}
    Value(int64_t i) : data(i) {}
    Value(int i) : data((int64_t)i) {}
    Value(double d) : data(d) {}
    Value(const char* s) : data(std::string(s)) {}
    Value(std::string s) : data(std::move(s)) {}
    Value(Array a) : data(std::move(a)) {}
    Value(Object o) : data(std::move(o)) {}
    Type type() const { return (Type)data.index(); }
    bool isNull() const { return type() == Null; }
    bool isBool() const { return type() == Bool; }
    bool isInt() const { return type() == Int; }
    bool isFloat() const { return type() == Float; }
    bool isString() const { return type() == String; }
    bool isArray() const { return type() == ArrayType; }
    bool isObject() const { return type() == ObjectType; }
    int64_t asInt() const { return std::get<int64_t>(data); }
    double asFloat() const { return std::get<double>(data); }
    bool asBool() const { return std::get<bool>(data); }
    const std::string& asString() const { return std::get<std::string>(data); }
    const Array& asArray() const { return std::get<Array>(data); }
    const Object& asObject() const { return std::get<Object>(data); }
    Object& asObject() { return std::get<Object>(data); }
    Value& operator[](const std::string& key) { return std::get<Object>(data)[key]; }
    const Value& operator[](const std::string& key) const { return std::get<Object>(data).at(key); }
    std::string dump(int indent = 2, int level = 0, bool root = false) const {
        if (isNull()) return "null"; if (isBool()) return asBool() ? "true" : "false";
        if (isInt()) return std::to_string(asInt());
        if (isFloat()) { std::ostringstream oss; oss << std::fixed << std::setprecision(5) << asFloat(); std::string s = oss.str(); while (s.size() > 1 && s.back() == '0') s.pop_back(); if (s.back() == '.') s += '0'; return s; }
        if (isString()) return root ? asString() : "\"" + asString() + "\"";
        if (isArray()) { std::string s = "["; for (size_t i = 0; i < asArray().size(); ++i) { s += asArray()[i].dump(indent, level + 1); if (i + 1 < asArray().size()) s += ", "; } return s + "]"; }
        if (isObject()) {
            std::string pad(level * indent, ' '), next_pad((level + 1) * indent, ' '); std::string s = root ? "" : "{";
            bool first = true; for (auto const& [k, v] : asObject()) { if (k.empty() || (k[0] == '_' && !root)) continue; if (!first && !root) s += ","; if (!root) s += "\n" + next_pad; s += k + ": " + v.dump(indent, level + 1); if (root) s += "\n"; first = false; }
            return root ? s : (asObject().empty() ? "{}" : s + "\n" + pad + "}");
        }
        return "";
    }
};

struct Parser {
    std::string text; size_t pos = 0; std::chrono::steady_clock::time_point start; int depth = 0;
    Parser(std::string t) : text(std::move(t)), start(std::chrono::steady_clock::now()) {}
    char peek() { return pos < text.size() ? text[pos] : '\0'; }
    char advance() { return pos < text.size() ? text[pos++] : '\0'; }
    void skip_ws() { while (pos < text.size()) { if (isspace((unsigned char)text[pos])) advance(); else if (text[pos] == '#') { advance(); if (peek() == '#') { advance(); while (pos + 1 < text.size() && !(text[pos] == '#' && text[pos + 1] == '#')) advance(); if (pos < text.size()) { advance(); advance(); } } else { while (pos < text.size() && text[pos] != '\n') advance(); } } else break; } }
    std::string read_key() { skip_ws(); std::string k; if (peek() == '"' || peek() == '\'') { char q = advance(); while (peek() && peek() != q) { if (peek() == '\\') { advance(); char e = advance(); if (e == 'n') k += '\n'; else k += e; } else k += advance(); } if (peek() == q) advance(); } else { while (isalnum((unsigned char)peek()) || peek() == '_' || peek() == '-') k += advance(); } return k; }
    void parse_pair(Object& o) { skip_ws(); std::string k = read_key(); if (k=="_env"||k=="_meta"||k=="_interpolate") throw std::runtime_error("Reserved: " + k); skip_ws(); if (peek() == ':') advance(); Value v = parse_value(); if (o.count(k)) throw std::runtime_error("Dup: " + k); o[k] = v; }
    Value parse_value() {
        skip_ws(); if (std::chrono::steady_clock::now() - start > std::chrono::seconds(1)) throw std::runtime_error("Timeout"); if (++depth > 100) throw std::runtime_error("Depth");
        char c = peek(); Value r;
        if (c == '{') { advance(); Object o; while (peek() && peek() != '}') { skip_ws(); if (peek() == '}') break; parse_pair(o); skip_ws(); if (peek() == ',') advance(); } if (peek() == '}') advance(); r = o; }
        else if (c == '[') { advance(); Array a; while (peek() && peek() != ']') { skip_ws(); if (peek() == ']') break; a.push_back(parse_value()); skip_ws(); if (peek() == ',') advance(); } if (peek() == ']') advance(); r = a; }
        else if (c == '"' || c == '\'') { char q = advance(); std::string b; while (peek() && peek() != q) { if (peek() == '\\') { advance(); char e = advance(); if (e == 'n') b += '\n'; else b += e; } else b += advance(); } if (peek() == q) advance(); r = b; }
        else if (isdigit((unsigned char)c) || c == '-') { std::string b; while (isdigit((unsigned char)peek()) || strchr(".-eE+", peek())) b += advance(); if (strchr(b.c_str(), '.')) r = std::stod(b); else r = (int64_t)std::stoll(b); }
        else { std::string b; while (isalpha((unsigned char)peek())) b += advance(); if (b == "true") r = true; else if (b == "false") r = false; else if (b == "null") r = nullptr; }
        depth--; return r;
    }
    Value parse() { Object root; while (pos < text.size()) { skip_ws(); if (!peek()) break; if (peek() == '[') { advance(); std::string path; while (peek() && peek() != ']') path += advance(); if (peek() == ']') advance(); size_t s = 0, e; Object* curr = &root; while (true) { e = path.find('.', s); std::string p = path.substr(s, e - s); bool last = (e == std::string::npos); if (curr->find(p) == curr->end()) (*curr)[p] = Value(Object{}); if (last) { parse_into(std::get<Object>((*curr)[p].data)); break; } curr = &std::get<Object>((*curr)[p].data); s = e + 1; } } else parse_pair(root); } return root; }
    void parse_into(Object& o) { while (pos < text.size()) { skip_ws(); if (peek() == '[' || !peek()) break; parse_pair(o); } }
};
inline Value parse(std::string text) { return Parser(std::move(text)).parse(); }
}
#endif
