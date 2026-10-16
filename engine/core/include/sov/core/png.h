#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "sov/core/result.h"

namespace sov {

// 8-bit RGBA image in row-major order, top row first. Pixel values are already encoded
// (sRGB for color captures) exactly as read back from the GPU.
struct ImageRgba8 {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::vector<std::uint8_t> pixels;  // width * height * 4

  const std::uint8_t* At(std::uint32_t x, std::uint32_t y) const { return &pixels[(std::size_t(y) * width + x) * 4]; }
};

// Text metadata embedded as PNG tEXt chunks (keyword, value). ASCII only; used to record
// capture provenance (source, device, backend, commit) inside every evidence image.
using PngText = std::vector<std::pair<std::string, std::string>>;

// Original PNG encoder: per-row adaptive filtering + deflate (fixed Huffman, LZ77) in a zlib stream.
std::string EncodePng(const ImageRgba8& image, const PngText& text = {});
Result<void> WritePng(const std::filesystem::path& path, const ImageRgba8& image, const PngText& text = {});

}  // namespace sov
