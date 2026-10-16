#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <string>
#include <string_view>

namespace sov {

// 128-bit identifier. The only persisted form is the canonical lowercase 8-4-4-4-12 string.
// Runtime slot handles (EntityHandle) are separate and never derived from or truncated into a Uuid.
struct Uuid {
  std::array<std::uint8_t, 16> bytes{};

  // Strict canonical parse: exactly 36 chars, lowercase hex, hyphens at 8/13/18/23.
  // Uppercase, braces, URNs and missing hyphens are rejected.
  static std::optional<Uuid> Parse(std::string_view text);

  std::string ToString() const;
  bool IsNil() const;

  friend auto operator<=>(const Uuid&, const Uuid&) = default;
  friend bool operator==(const Uuid&, const Uuid&) = default;
};

// Random (version 4, RFC 9562 variant) UUID source. Seedable for reproducible tests.
class UuidGenerator {
 public:
  UuidGenerator();                             // seeded from std::random_device
  explicit UuidGenerator(std::uint64_t seed);  // deterministic sequence
  Uuid Next();

 private:
  std::mt19937_64 rng_;
};

// Process-wide generator for editor/CLI convenience (not thread-safe; owner-thread use only).
Uuid GenerateUuid();

}  // namespace sov

template <>
struct std::hash<sov::Uuid> {
  std::size_t operator()(const sov::Uuid& id) const noexcept {
    std::uint64_t a = 0, b = 0;
    for (int i = 0; i < 8; ++i) a = (a << 8) | id.bytes[static_cast<std::size_t>(i)];
    for (int i = 8; i < 16; ++i) b = (b << 8) | id.bytes[static_cast<std::size_t>(i)];
    return static_cast<std::size_t>(a ^ (b * 0x9e3779b97f4a7c15ULL));
  }
};
