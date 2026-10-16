#include "sov/core/png.h"

#include <array>
#include <cstdlib>
#include <cstring>

#include "sov/core/fs.h"

namespace sov {
namespace {

// --- checksums -------------------------------------------------------------------------------
std::uint32_t Crc32(const std::uint8_t* data, std::size_t n, std::uint32_t crc = 0) {
  static const auto table = [] {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t i = 0; i < 256; ++i) {
      std::uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      t[i] = c;
    }
    return t;
  }();
  crc = ~crc;
  for (std::size_t i = 0; i < n; ++i) crc = table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
  return ~crc;
}

std::uint32_t Adler32(const std::uint8_t* data, std::size_t n) {
  std::uint32_t a = 1, b = 0;
  for (std::size_t i = 0; i < n; ++i) {
    a = (a + data[i]) % 65521;
    b = (b + a) % 65521;
  }
  return (b << 16) | a;
}

// --- deflate (fixed Huffman) -----------------------------------------------------------------
class BitWriter {
 public:
  void Bits(std::uint32_t value, int count) {  // LSB-first
    for (int i = 0; i < count; ++i) {
      cur_ |= ((value >> i) & 1u) << nbits_;
      if (++nbits_ == 8) Flush();
    }
  }
  void Code(std::uint32_t code, int length) {  // Huffman codes are MSB-first
    for (int i = length - 1; i >= 0; --i) Bits((code >> i) & 1u, 1);
  }
  std::string Finish() {
    if (nbits_ > 0) Flush();
    return std::move(out_);
  }

 private:
  void Flush() {
    out_.push_back(static_cast<char>(cur_));
    cur_ = 0;
    nbits_ = 0;
  }
  std::string out_;
  std::uint32_t cur_ = 0;
  int nbits_ = 0;
};

void Literal(BitWriter& w, std::uint32_t sym) {
  if (sym <= 143) {
    w.Code(0x30 + sym, 8);
  } else if (sym <= 255) {
    w.Code(0x190 + (sym - 144), 9);
  } else if (sym <= 279) {
    w.Code(sym - 256, 7);
  } else {
    w.Code(0xC0 + (sym - 280), 8);
  }
}

constexpr std::uint16_t kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
                                        67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::uint8_t kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::uint16_t kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
                                         1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::uint8_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
                                         9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

void Match(BitWriter& w, int length, int distance) {
  int li = 28;
  while (kLenBase[li] > length) --li;
  Literal(w, 257 + static_cast<std::uint32_t>(li));
  w.Bits(static_cast<std::uint32_t>(length - kLenBase[li]), kLenExtra[li]);
  int di = 29;
  while (kDistBase[di] > distance) --di;
  w.Code(static_cast<std::uint32_t>(di), 5);
  w.Bits(static_cast<std::uint32_t>(distance - kDistBase[di]), kDistExtra[di]);
}

std::string Deflate(const std::uint8_t* data, std::size_t n) {
  constexpr int kWindow = 32768, kHashBits = 15, kMaxChain = 48, kMaxMatch = 258;
  std::vector<int> head(1 << kHashBits, -1), prev(n, -1);
  const auto hash = [&](std::size_t i) {
    return ((std::uint32_t{data[i]} << 10) ^ (std::uint32_t{data[i + 1]} << 5) ^ data[i + 2]) & ((1u << kHashBits) - 1);
  };
  BitWriter w;
  w.Bits(1, 1);  // BFINAL
  w.Bits(1, 2);  // BTYPE = fixed Huffman
  std::size_t i = 0;
  const auto insert = [&](std::size_t pos) {
    if (pos + 2 < n) {
      const auto h = hash(pos);
      prev[pos] = head[h];
      head[h] = static_cast<int>(pos);
    }
  };
  while (i < n) {
    int best_len = 0, best_dist = 0;
    if (i + 2 < n) {
      int cand = head[hash(i)];
      for (int chain = 0; cand >= 0 && chain < kMaxChain; ++chain, cand = prev[static_cast<std::size_t>(cand)]) {
        const int dist = static_cast<int>(i) - cand;
        if (dist > kWindow) break;
        const int limit = static_cast<int>(std::min<std::size_t>(kMaxMatch, n - i));
        int len = 0;
        while (len < limit && data[static_cast<std::size_t>(cand) + static_cast<std::size_t>(len)] == data[i + static_cast<std::size_t>(len)]) ++len;
        if (len > best_len) {
          best_len = len;
          best_dist = dist;
          if (len == limit) break;
        }
      }
    }
    if (best_len >= 3) {
      Match(w, best_len, best_dist);
      for (int k = 0; k < best_len; ++k) insert(i + static_cast<std::size_t>(k));
      i += static_cast<std::size_t>(best_len);
    } else {
      Literal(w, data[i]);
      insert(i);
      ++i;
    }
  }
  Literal(w, 256);  // end of block
  return w.Finish();
}

// --- PNG -------------------------------------------------------------------------------------
void Put32(std::string& s, std::uint32_t v) {
  for (int sh = 24; sh >= 0; sh -= 8) s.push_back(static_cast<char>((v >> sh) & 0xff));
}

void Chunk(std::string& out, const char type[4], const std::string& payload) {
  Put32(out, static_cast<std::uint32_t>(payload.size()));
  std::string typed(type, 4);
  typed += payload;
  out += typed;
  Put32(out, Crc32(reinterpret_cast<const std::uint8_t*>(typed.data()), typed.size()));
}

std::uint8_t Paeth(int a, int b, int c) {
  const int p = a + b - c;
  const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
  if (pa <= pb && pa <= pc) return static_cast<std::uint8_t>(a);
  return static_cast<std::uint8_t>(pb <= pc ? b : c);
}

}  // namespace

std::string EncodePng(const ImageRgba8& img, const PngText& text) {
  const std::size_t stride = std::size_t(img.width) * 4;
  std::vector<std::uint8_t> raw;
  raw.reserve((stride + 1) * img.height);
  std::vector<std::uint8_t> candidate(stride), best(stride);
  for (std::uint32_t y = 0; y < img.height; ++y) {
    const std::uint8_t* row = &img.pixels[y * stride];
    const std::uint8_t* up = y > 0 ? &img.pixels[(y - 1) * stride] : nullptr;
    long best_cost = -1;
    std::uint8_t best_filter = 0;
    for (std::uint8_t f = 0; f < 5; ++f) {
      long cost = 0;
      for (std::size_t x = 0; x < stride; ++x) {
        const int a = x >= 4 ? row[x - 4] : 0;
        const int b = up ? up[x] : 0;
        const int c = (up && x >= 4) ? up[x - 4] : 0;
        int pred = 0;
        switch (f) {
          case 1: pred = a; break;
          case 2: pred = b; break;
          case 3: pred = (a + b) / 2; break;
          case 4: pred = Paeth(a, b, c); break;
          default: break;
        }
        candidate[x] = static_cast<std::uint8_t>(row[x] - pred);
        cost += static_cast<std::int8_t>(candidate[x]) < 0 ? -static_cast<std::int8_t>(candidate[x]) : candidate[x];
      }
      if (best_cost < 0 || cost < best_cost) {
        best_cost = cost;
        best_filter = f;
        best.swap(candidate);
      }
    }
    raw.push_back(best_filter);
    raw.insert(raw.end(), best.begin(), best.end());
  }

  std::string z;
  z.push_back(static_cast<char>(0x78));
  z.push_back(static_cast<char>(0x01));
  z += Deflate(raw.data(), raw.size());
  Put32(z, Adler32(raw.data(), raw.size()));

  std::string out("\x89PNG\r\n\x1a\n", 8);
  std::string ihdr;
  Put32(ihdr, img.width);
  Put32(ihdr, img.height);
  ihdr += std::string("\x08\x06\x00\x00\x00", 5);  // 8-bit RGBA, deflate, adaptive filter, no interlace
  Chunk(out, "IHDR", ihdr);
  for (const auto& [key, value] : text) Chunk(out, "tEXt", key + std::string(1, '\0') + value);
  Chunk(out, "IDAT", z);
  Chunk(out, "IEND", "");
  return out;
}

Result<void> WritePng(const std::filesystem::path& path, const ImageRgba8& image, const PngText& text) {
  if (image.pixels.size() != std::size_t(image.width) * image.height * 4) {
    return MakeError(ErrorCode::InvalidValue, "image pixel buffer size does not match dimensions");
  }
  return fs::AtomicWriteFile(path, EncodePng(image, text));
}

}  // namespace sov
