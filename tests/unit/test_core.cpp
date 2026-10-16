#include <set>

#include "sov/core/fs.h"
#include "sov/core/json.h"
#include "sov/core/sha256.h"
#include "sov/core/uuid.h"
#include "sov_test.h"

using namespace sov;

SOV_TEST(core, uuid_canonical_roundtrip) {
  const auto id = Uuid::Parse("0f8b3c2a-5d1e-4b7a-9c3d-2e1f0a9b8c7d");
  SOV_REQUIRE(id.has_value());
  SOV_CHECK_EQ(id->ToString(), std::string("0f8b3c2a-5d1e-4b7a-9c3d-2e1f0a9b8c7d"));
  SOV_CHECK(!id->IsNil());
  SOV_CHECK(Uuid::Parse("00000000-0000-0000-0000-000000000000")->IsNil());
}

SOV_TEST(core, uuid_rejects_non_canonical_forms) {
  SOV_CHECK(!Uuid::Parse("0F8B3C2A-5D1E-4B7A-9C3D-2E1F0A9B8C7D").has_value());   // uppercase
  SOV_CHECK(!Uuid::Parse("{0f8b3c2a-5d1e-4b7a-9c3d-2e1f0a9b8c7d}").has_value()); // braces
  SOV_CHECK(!Uuid::Parse("0f8b3c2a5d1e4b7a9c3d2e1f0a9b8c7d").has_value());       // no hyphens
  SOV_CHECK(!Uuid::Parse("0f8b3c2a-5d1e-4b7a-9c3d-2e1f0a9b8c7").has_value());    // short
  SOV_CHECK(!Uuid::Parse("0f8b3c2a-5d1e-4b7a-9c3d_2e1f0a9b8c7d").has_value());   // bad separator
  SOV_CHECK(!Uuid::Parse("0f8b3c2a-5d1e-4b7a-9c3d-2e1f0a9b8c7g").has_value());   // non-hex
  SOV_CHECK(!Uuid::Parse("").has_value());
}

SOV_TEST(core, uuid_generator_v4_unique_and_seedable) {
  UuidGenerator a(42), b(42);
  std::set<std::string> seen;
  for (int i = 0; i < 1000; ++i) {
    const Uuid x = a.Next();
    SOV_CHECK_EQ(x, b.Next());
    SOV_CHECK_EQ(x.bytes[6] >> 4, 4);           // version 4
    SOV_CHECK_EQ(x.bytes[8] & 0xc0, 0x80);      // RFC variant
    SOV_CHECK(Uuid::Parse(x.ToString()) == x);  // canonical text round-trip
    seen.insert(x.ToString());
  }
  SOV_CHECK_EQ(seen.size(), std::size_t{1000});
}

SOV_TEST(core, sha256_known_vectors) {
  SOV_CHECK_EQ(Sha256Hex(""), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
  SOV_CHECK_EQ(Sha256Hex("abc"), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  SOV_CHECK_EQ(Sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
               std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
  std::string million(1000000, 'a');
  SOV_CHECK_EQ(Sha256Hex(million), std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
  // Incremental updates across block boundaries match one-shot hashing.
  Sha256 h;
  for (char c : million) h.Update(&c, 1);
  const auto d = h.Finish();
  SOV_CHECK_EQ(ToHex(d.data(), d.size()), Sha256Hex(million));
}

SOV_TEST(core, strict_json_rejects_duplicate_keys) {
  auto r = ParseStrictJson(R"({"a": 1, "b": {"x": 1, "x": 2}})");
  SOV_REQUIRE(!r.ok());
  SOV_CHECK_EQ(r.error().code, ErrorCode::SchemaError);
  SOV_CHECK(r.error().message.find("duplicate object key \"x\"") != std::string::npos);
  // Same key in sibling objects is fine.
  SOV_CHECK(ParseStrictJson(R"([{"x": 1}, {"x": 2}])").ok());
}

SOV_TEST(core, strict_json_enforces_limits) {
  JsonLimits limits;
  limits.max_depth = 4;
  SOV_CHECK(ParseStrictJson("[[[[1]]]]", limits).ok());
  auto deep = ParseStrictJson("[[[[[1]]]]]", limits);
  SOV_REQUIRE(!deep.ok());
  SOV_CHECK_EQ(deep.error().code, ErrorCode::LimitExceeded);

  limits = {};
  limits.max_bytes = 8;
  auto big = ParseStrictJson(R"({"abc": 12345})", limits);
  SOV_REQUIRE(!big.ok());
  SOV_CHECK_EQ(big.error().code, ErrorCode::LimitExceeded);

  limits = {};
  limits.max_string_bytes = 4;
  SOV_CHECK(!ParseStrictJson(R"({"k": "toolong"})", limits).ok());
}

SOV_TEST(core, strict_json_rejects_malformed_and_nonfinite) {
  SOV_CHECK(!ParseStrictJson("{").ok());
  SOV_CHECK(!ParseStrictJson("{\"a\": NaN}").ok());
  SOV_CHECK(!ParseStrictJson("[1e999]").ok());
  SOV_CHECK(!ParseStrictJson("{\"a\": 1} trailing").ok());
  SOV_CHECK(!ParseStrictJson("{\"a\": \"\xff\xfe\"}").ok());  // invalid UTF-8
  SOV_CHECK(!ParseStrictJson("// comment\n{}").ok());
}

SOV_TEST(core, canonical_json_is_stable) {
  Json doc = Json::object();
  doc["name"] = "Hall";
  doc["position"] = Json::array({1.5, 0.0, -2.0});
  doc["neg_zero"] = -0.0;
  doc["tenth"] = 0.1;
  doc["list"] = Json::array({Json{{"id", 1}}, Json{{"id", 2}}});
  const std::string text = CanonicalJson(doc);
  SOV_CHECK_EQ(text, std::string("{\n"
                                 "  \"name\": \"Hall\",\n"
                                 "  \"position\": [1.5, 0.0, -2.0],\n"
                                 "  \"neg_zero\": 0.0,\n"
                                 "  \"tenth\": 0.1,\n"
                                 "  \"list\": [\n"
                                 "    {\n"
                                 "      \"id\": 1\n"
                                 "    },\n"
                                 "    {\n"
                                 "      \"id\": 2\n"
                                 "    }\n"
                                 "  ]\n"
                                 "}\n"));
  // Parse -> write is a fixed point.
  auto reparsed = ParseStrictJson(text);
  SOV_REQUIRE_OK(reparsed);
  SOV_CHECK_EQ(CanonicalJson(*reparsed), text);
}

SOV_TEST(core, atomic_write_replaces_and_preserves_on_failure) {
  const auto dir = ctx.TempDir();
  const auto target = dir / "doc.json";
  SOV_REQUIRE_OK(fs::AtomicWriteFile(target, "first"));
  SOV_CHECK_EQ(*fs::ReadFile(target, 1024), std::string("first"));
  SOV_REQUIRE_OK(fs::AtomicWriteFile(target, "second"));
  SOV_CHECK_EQ(*fs::ReadFile(target, 1024), std::string("second"));

  fs::SetFailPointForTesting(fs::FailPoint::AtomicWriteBeforeRename);
  auto failed = fs::AtomicWriteFile(target, "third");
  SOV_CHECK(!failed.ok());
  SOV_CHECK_EQ(failed.error().code, ErrorCode::IoFailure);
  SOV_CHECK_EQ(*fs::ReadFile(target, 1024), std::string("second"));  // last valid file preserved

  // No temporary files left behind in either path.
  int entries = 0;
  for (const auto& e : std::filesystem::directory_iterator(dir)) {
    (void)e;
    ++entries;
  }
  SOV_CHECK_EQ(entries, 1);
}

SOV_TEST(core, read_file_limit_checked_before_read) {
  const auto target = ctx.TempDir() / "big.bin";
  SOV_REQUIRE_OK(fs::AtomicWriteFile(target, std::string(2048, 'x')));
  auto r = fs::ReadFile(target, 1024);
  SOV_REQUIRE(!r.ok());
  SOV_CHECK_EQ(r.error().code, ErrorCode::LimitExceeded);
  auto missing = fs::ReadFile(ctx.TempDir() / "missing", 1024);
  SOV_REQUIRE(!missing.ok());
  SOV_CHECK_EQ(missing.error().code, ErrorCode::IoFailure);
}

SOV_TEST(core, file_lock_is_exclusive_and_released) {
  const auto lock_path = ctx.TempDir() / "scene.json.lock";
  auto first = fs::FileLock::TryAcquire(lock_path, R"({"tool":"test-a"})");
  SOV_REQUIRE_OK(first);
  // flock locks belong to the open file description, so a second acquire conflicts even in-process.
  auto second = fs::FileLock::TryAcquire(lock_path, R"({"tool":"test-b"})");
  SOV_REQUIRE(!second.ok());
  SOV_CHECK_EQ(second.error().code, ErrorCode::LockUnavailable);
  SOV_CHECK(second.error().message.find("test-a") != std::string::npos);  // holder reported
  first.value().reset();
  auto third = fs::FileLock::TryAcquire(lock_path, R"({"tool":"test-c"})");
  SOV_CHECK(third.ok());
}

#include "sov/core/png.h"

// The encoder must produce a stream a standard decoder accepts. Python's zlib is used as an
// independent decoder when available (test skips that half otherwise and says so).
SOV_TEST(core, png_encoder_roundtrips_through_independent_decoder) {
  ImageRgba8 img;
  img.width = 37;
  img.height = 23;
  img.pixels.resize(std::size_t(img.width) * img.height * 4);
  for (std::uint32_t y = 0; y < img.height; ++y) {
    for (std::uint32_t x = 0; x < img.width; ++x) {
      std::uint8_t* p = &img.pixels[(std::size_t(y) * img.width + x) * 4];
      p[0] = static_cast<std::uint8_t>(x * 7);
      p[1] = static_cast<std::uint8_t>(y * 11);
      p[2] = static_cast<std::uint8_t>((x / 4 + y / 4) % 2 ? 200 : 20);  // blocks -> long matches
      p[3] = 255;
    }
  }
  const auto png_path = ctx.TempDir() / "test.png";
  SOV_REQUIRE_OK(WritePng(png_path, img, {{"Source", "unit test"}}));
  const std::string raw_path = (ctx.TempDir() / "pixels.raw").string();
  SOV_REQUIRE_OK(fs::AtomicWriteFile(raw_path, std::string(img.pixels.begin(), img.pixels.end())));
  const std::string script = (ctx.SourceDir() / "scripts" / "png_decode_check.py").string();
  const std::string cmd = "python3 -I '" + script + "' '" + png_path.string() + "' '" + raw_path + "' > /dev/null 2>&1";
  const int rc = std::system(cmd.c_str());
  if (rc != 0 && std::system("python3 -c 1 > /dev/null 2>&1") != 0) {
    ctx.Note("python3 unavailable: independent PNG decode check NOT run");
    return;
  }
  SOV_CHECK_EQ(rc, 0);
}
