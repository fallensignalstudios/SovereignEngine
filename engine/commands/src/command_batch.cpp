#include "sov/commands/command_batch.h"

#include <format>

#include "sov/core/build_info.h"
#include "sov/core/sha256.h"

namespace sov::cmd {
namespace {

bool KeyChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
         c == ':' || c == '-';
}

}  // namespace

// Contract: any string of 1..128 characters. The engine additionally refuses control characters
// and invalid UTF-8 so actor labels stay safe to print in logs and history.
bool IsValidActor(std::string_view actor) {
  for (unsigned char c : actor) {
    if (c < 0x20 || c == 0x7f) return false;
  }
  const auto chars = scene::Utf8CodePoints(actor);
  return chars && *chars >= 1 && *chars <= BatchLimits::kMaxActorChars;
}

bool IsValidIdempotencyKey(std::string_view key) {
  if (key.empty() || key.size() > BatchLimits::kMaxIdempotencyKeyBytes) return false;
  for (char c : key) {
    if (!KeyChar(c)) return false;
  }
  return true;
}

std::string CommandBatch::ContentHash() const {
  Json content = Json::object();
  content["protocol_version"] = protocol_version;
  content["scene_id"] = scene_id.ToString();
  content["base_revision"] = base_revision;
  Json ops = Json::array();
  for (const auto& op : operations) ops.push_back(OperationToJson(op));
  content["operations"] = std::move(ops);
  return Sha256Hex(CompactJson(content));
}

Result<CommandBatch> ParseCommandBatch(const Json& j) {
  if (!j.is_object()) return MakeError(ErrorCode::SchemaError, "command batch must be a JSON object", "");
  auto ver = j.find("protocol_version");
  if (ver == j.end() || !ver->is_number_integer()) {
    return MakeError(ErrorCode::SchemaError, "\"protocol_version\" must be an integer", "/protocol_version");
  }
  if (ver->get<std::int64_t>() != kCommandProtocolVersion) {
    return MakeError(ErrorCode::UnknownSchemaVersion,
                     std::format("command protocol_version {} is not supported (supports {})", ver->get<std::int64_t>(),
                                 kCommandProtocolVersion),
                     "/protocol_version");
  }
  for (auto it = j.begin(); it != j.end(); ++it) {
    static constexpr std::string_view kAllowed[] = {"protocol_version", "request_id",      "actor",   "scene_id",
                                                    "base_revision",    "idempotency_key", "dry_run", "operations"};
    bool known = false;
    for (auto k : kAllowed) known = known || k == it.key();
    if (!known) {
      return MakeError(ErrorCode::SchemaError, std::format("unknown field \"{}\"", it.key()), JsonPointerAppend("", it.key()));
    }
  }

  CommandBatch b;
  const auto uuid_field = [&](std::string_view key, Uuid& out) -> Result<void> {
    auto it = j.find(key);
    auto id = (it != j.end() && it->is_string()) ? Uuid::Parse(it->get_ref<const std::string&>()) : std::nullopt;
    if (!id || id->IsNil()) {
      return MakeError(ErrorCode::SchemaError, std::format("\"{}\" must be a non-nil canonical lowercase UUID", key),
                       JsonPointerAppend("", key));
    }
    out = *id;
    return Ok();
  };
  if (auto r = uuid_field("request_id", b.request_id); !r) return r.take_error();
  if (auto r = uuid_field("scene_id", b.scene_id); !r) return r.take_error();

  auto actor = j.find("actor");
  if (actor == j.end() || !actor->is_string() || !IsValidActor(actor->get_ref<const std::string&>())) {
    return MakeError(ErrorCode::SchemaError, "\"actor\" must be a string of 1-128 characters without control characters",
                     "/actor");
  }
  b.actor = actor->get<std::string>();

  auto rev = j.find("base_revision");
  if (rev == j.end() || !rev->is_number_unsigned()) {
    return MakeError(ErrorCode::SchemaError, "\"base_revision\" must be a non-negative integer", "/base_revision");
  }
  b.base_revision = rev->get<std::uint64_t>();

  auto key = j.find("idempotency_key");
  if (key == j.end() || !key->is_string() || !IsValidIdempotencyKey(key->get_ref<const std::string&>())) {
    return MakeError(ErrorCode::SchemaError, "\"idempotency_key\" must be 1-128 characters of [A-Za-z0-9._:-]",
                     "/idempotency_key");
  }
  b.idempotency_key = key->get<std::string>();

  auto dry = j.find("dry_run");
  if (dry == j.end() || !dry->is_boolean()) {
    return MakeError(ErrorCode::SchemaError, "\"dry_run\" is required and must be a boolean (true never writes)", "/dry_run");
  }
  b.dry_run = dry->get<bool>();

  auto ops = j.find("operations");
  if (ops == j.end() || !ops->is_array()) {
    return MakeError(ErrorCode::SchemaError, "\"operations\" must be an array", "/operations");
  }
  if (ops->empty()) return MakeError(ErrorCode::SchemaError, "\"operations\" must not be empty", "/operations");
  if (ops->size() > BatchLimits::kMaxOperations) {
    return MakeError(ErrorCode::LimitExceeded,
                     std::format("{} operations; limit is {}", ops->size(), BatchLimits::kMaxOperations), "/operations");
  }
  for (std::size_t i = 0; i < ops->size(); ++i) {
    auto op = ParseOperation((*ops)[i], JsonPointerAppend("/operations", i));
    if (!op) {
      Error e = op.take_error();
      e.op_index = static_cast<int>(i);
      return e;
    }
    b.operations.push_back(std::move(op).value());
  }
  return b;
}

Result<CommandBatch> ParseCommandBatchText(std::string_view text) {
  JsonLimits limits;
  limits.max_bytes = BatchLimits::kMaxBatchBytes;
  limits.max_depth = 12;
  limits.max_string_bytes = 4096;
  auto j = ParseStrictJson(text, limits);
  if (!j) return j.take_error();
  return ParseCommandBatch(*j);
}

Json CommandBatchToJson(const CommandBatch& b) {
  Json j = Json::object();
  j["protocol_version"] = b.protocol_version;
  j["request_id"] = b.request_id.ToString();
  j["actor"] = b.actor;
  j["scene_id"] = b.scene_id.ToString();
  j["base_revision"] = b.base_revision;
  j["idempotency_key"] = b.idempotency_key;
  j["dry_run"] = b.dry_run;
  Json ops = Json::array();
  for (const auto& op : b.operations) ops.push_back(OperationToJson(op));
  j["operations"] = std::move(ops);
  return j;
}

}  // namespace sov::cmd
