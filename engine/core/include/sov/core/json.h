#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "sov/core/result.h"

namespace sov {

using Json = nlohmann::ordered_json;

struct JsonLimits {
  std::size_t max_bytes = 16u * 1024u * 1024u;
  int max_depth = 32;
  std::size_t max_string_bytes = 64u * 1024u;
};

// Strict, bounded parse:
//   - input larger than max_bytes, nesting deeper than max_depth, or strings longer than
//     max_string_bytes -> LimitExceeded
//   - syntax errors, invalid UTF-8, duplicate object keys, non-finite numbers -> SchemaError
// Object member order is preserved (ordered_json) for diagnostics; canonical output order is
// decided by the writer, not by the input.
Result<Json> ParseStrictJson(std::string_view text, const JsonLimits& limits = {});

// Canonical text form used for every engine-written document:
//   - 2-space indentation, LF line endings, trailing newline
//   - members in the order they were inserted into the Json value (writers insert in schema order)
//   - arrays whose elements are all scalars are written on one line: [1, 0.5, -2]
//   - numbers: integers as integers; doubles in shortest round-trip form; -0.0 is written as 0.0
std::string CanonicalJson(const Json& value);

// Compact single-line form (journal records). Same number rules as CanonicalJson.
std::string CompactJson(const Json& value);

// JSON pointer escaping helper: "/entities/3/name"
std::string JsonPointerAppend(std::string_view base, std::string_view token);
std::string JsonPointerAppend(std::string_view base, std::size_t index);

}  // namespace sov
