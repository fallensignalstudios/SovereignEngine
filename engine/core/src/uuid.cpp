#include "sov/core/uuid.h"

namespace sov {
namespace {

int HexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;  // uppercase deliberately rejected: persisted IDs are canonical lowercase
}

}  // namespace

std::optional<Uuid> Uuid::Parse(std::string_view text) {
  if (text.size() != 36) return std::nullopt;
  Uuid out;
  std::size_t byte = 0;
  for (std::size_t i = 0; i < text.size();) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (text[i] != '-') return std::nullopt;
      ++i;
      continue;
    }
    const int hi = HexValue(text[i]);
    const int lo = (i + 1 < text.size()) ? HexValue(text[i + 1]) : -1;
    if (hi < 0 || lo < 0) return std::nullopt;
    out.bytes[byte++] = static_cast<std::uint8_t>((hi << 4) | lo);
    i += 2;
  }
  if (byte != 16) return std::nullopt;
  return out;
}

std::string Uuid::ToString() const {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string s;
  s.reserve(36);
  for (std::size_t i = 0; i < 16; ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10) s.push_back('-');
    s.push_back(kHex[bytes[i] >> 4]);
    s.push_back(kHex[bytes[i] & 0x0f]);
  }
  return s;
}

bool Uuid::IsNil() const {
  for (auto b : bytes) {
    if (b != 0) return false;
  }
  return true;
}

UuidGenerator::UuidGenerator() {
  std::random_device rd;
  std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
  rng_.seed(seq);
}

UuidGenerator::UuidGenerator(std::uint64_t seed) : rng_(seed) {}

Uuid UuidGenerator::Next() {
  Uuid id;
  const std::uint64_t a = rng_();
  const std::uint64_t b = rng_();
  for (std::size_t i = 0; i < 8; ++i) {
    id.bytes[i] = static_cast<std::uint8_t>(a >> (56 - 8 * i));
    id.bytes[8 + i] = static_cast<std::uint8_t>(b >> (56 - 8 * i));
  }
  id.bytes[6] = static_cast<std::uint8_t>((id.bytes[6] & 0x0f) | 0x40);  // version 4
  id.bytes[8] = static_cast<std::uint8_t>((id.bytes[8] & 0x3f) | 0x80);  // RFC variant
  return id;
}

Uuid GenerateUuid() {
  static UuidGenerator generator;
  return generator.Next();
}

}  // namespace sov
