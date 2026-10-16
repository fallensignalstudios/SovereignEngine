#include "sov/core/json.h"

#include <cmath>
#include <format>
#include <set>
#include <vector>

namespace sov {
namespace {

struct StrictViolation {
  ErrorCode code;
  std::string message;
};

void WriteNumber(std::string& out, const Json& v) {
  if (v.is_number_float()) {
    double d = v.get<double>();
    if (d == 0.0) d = 0.0;  // normalizes -0.0
    out += Json(d).dump();
  } else {
    out += v.dump();
  }
}

bool IsScalar(const Json& v) { return !v.is_object() && !v.is_array(); }

void WriteValue(std::string& out, const Json& v, int indent, bool pretty) {
  const auto newline = [&](int level) {
    if (!pretty) return;
    out.push_back('\n');
    out.append(static_cast<std::size_t>(level) * 2, ' ');
  };
  if (v.is_object()) {
    if (v.empty()) {
      out += "{}";
      return;
    }
    out.push_back('{');
    bool first = true;
    for (auto it = v.begin(); it != v.end(); ++it) {
      if (!first) out.push_back(',');
      first = false;
      newline(indent + 1);
      out += Json(it.key()).dump();
      out += pretty ? ": " : ":";
      WriteValue(out, it.value(), indent + 1, pretty);
    }
    newline(indent);
    out.push_back('}');
    return;
  }
  if (v.is_array()) {
    if (v.empty()) {
      out += "[]";
      return;
    }
    bool all_scalar = true;
    for (const auto& e : v) all_scalar = all_scalar && IsScalar(e);
    out.push_back('[');
    bool first = true;
    for (const auto& e : v) {
      if (!first) out += (pretty && all_scalar) ? ", " : ",";
      first = false;
      if (!all_scalar) newline(indent + 1);
      WriteValue(out, e, indent + 1, pretty);
    }
    if (!all_scalar) newline(indent);
    out.push_back(']');
    return;
  }
  if (v.is_number()) {
    WriteNumber(out, v);
    return;
  }
  out += v.dump(-1, ' ', false, nlohmann::json::error_handler_t::strict);
}

}  // namespace

Result<Json> ParseStrictJson(std::string_view text, const JsonLimits& limits) {
  if (text.size() > limits.max_bytes) {
    return MakeError(ErrorCode::LimitExceeded,
                     std::format("document is {} bytes; limit is {} bytes", text.size(), limits.max_bytes));
  }

  // Per-object key sets for duplicate detection; depth tracked across objects and arrays.
  std::vector<std::set<std::string>> key_stack;
  int depth = 0;

  const Json::parser_callback_t callback = [&](int /*parser_depth*/, Json::parse_event_t event, Json& parsed) -> bool {
    switch (event) {
      case Json::parse_event_t::object_start:
        key_stack.emplace_back();
        [[fallthrough]];
      case Json::parse_event_t::array_start:
        if (++depth > limits.max_depth) {
          throw StrictViolation{ErrorCode::LimitExceeded,
                                std::format("nesting deeper than {} levels", limits.max_depth)};
        }
        break;
      case Json::parse_event_t::object_end:
        if (!key_stack.empty()) key_stack.pop_back();
        [[fallthrough]];
      case Json::parse_event_t::array_end:
        --depth;
        break;
      case Json::parse_event_t::key: {
        const auto& key = parsed.get_ref<const std::string&>();
        if (key.size() > limits.max_string_bytes) {
          throw StrictViolation{ErrorCode::LimitExceeded, "object key exceeds the string length limit"};
        }
        if (!key_stack.empty() && !key_stack.back().insert(key).second) {
          throw StrictViolation{ErrorCode::SchemaError, std::format("duplicate object key \"{}\"", key)};
        }
        break;
      }
      case Json::parse_event_t::value:
        if (parsed.is_number_float() && !std::isfinite(parsed.get<double>())) {
          throw StrictViolation{ErrorCode::SchemaError, "non-finite number"};
        }
        if (parsed.is_string() && parsed.get_ref<const std::string&>().size() > limits.max_string_bytes) {
          throw StrictViolation{ErrorCode::LimitExceeded, "string exceeds the string length limit"};
        }
        break;
    }
    return true;
  };

  try {
    Json value = Json::parse(text.begin(), text.end(), callback, /*allow_exceptions=*/true,
                             /*ignore_comments=*/false);
    return value;
  } catch (const StrictViolation& v) {
    return MakeError(v.code, v.message);
  } catch (const nlohmann::json::exception& e) {
    return MakeError(ErrorCode::SchemaError, std::format("invalid JSON: {}", e.what()));
  }
}

std::string CanonicalJson(const Json& value) {
  std::string out;
  WriteValue(out, value, 0, true);
  out.push_back('\n');
  return out;
}

std::string CompactJson(const Json& value) {
  std::string out;
  WriteValue(out, value, 0, false);
  return out;
}

std::string JsonPointerAppend(std::string_view base, std::string_view token) {
  std::string out(base);
  out.push_back('/');
  for (char c : token) {
    if (c == '~') {
      out += "~0";
    } else if (c == '/') {
      out += "~1";
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::string JsonPointerAppend(std::string_view base, std::size_t index) {
  return std::string(base) + "/" + std::to_string(index);
}

}  // namespace sov
