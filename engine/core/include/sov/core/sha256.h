#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace sov {

// FIPS 180-4 SHA-256. Used for content identity (scene file hashes, request-content binding),
// not for security decisions.
class Sha256 {
 public:
  Sha256();
  void Update(const void* data, std::size_t size);
  void Update(std::string_view text) { Update(text.data(), text.size()); }
  std::array<std::uint8_t, 32> Finish();

 private:
  void Block(const std::uint8_t* block);

  std::array<std::uint32_t, 8> state_{};
  std::array<std::uint8_t, 64> buffer_{};
  std::size_t buffered_ = 0;
  std::uint64_t total_bytes_ = 0;
};

std::string ToHex(const std::uint8_t* data, std::size_t size);
std::string Sha256Hex(std::string_view data);

}  // namespace sov
