#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sov/commands/operation.h"
#include "sov/core/json.h"
#include "sov/core/result.h"
#include "sov/core/uuid.h"

namespace sov::cmd {

struct BatchLimits {
  static constexpr std::size_t kMaxBatchBytes = 1u * 1024u * 1024u;
  static constexpr std::size_t kMaxOperations = 128;          // contract: operations maxItems
  static constexpr std::size_t kMaxActorChars = 128;          // contract: actor maxLength
  static constexpr std::size_t kMaxIdempotencyKeyBytes = 128; // contract: [A-Za-z0-9_.:-]{1,128}
};

// Command batch contract v1 (schemas/command_batch.schema.json, urn:sovereign-engine:schema:command-batch:1).
struct CommandBatch {
  int protocol_version = 1;
  Uuid request_id;
  std::string actor;            // a label, not authentication; scope for idempotency keys
  Uuid scene_id;
  std::uint64_t base_revision = 0;
  std::string idempotency_key;  // required; scoped to (scene, actor)
  bool dry_run = true;          // required on the wire; true never writes
  std::vector<Operation> operations;

  // SHA-256 over the normalized request content: protocol_version, scene_id, base_revision and
  // the normalized operations. request_id, actor, idempotency_key and dry_run are excluded.
  std::string ContentHash() const;
};

Result<CommandBatch> ParseCommandBatch(const Json& json);
Result<CommandBatch> ParseCommandBatchText(std::string_view text);
Json CommandBatchToJson(const CommandBatch& batch);

bool IsValidActor(std::string_view actor);
bool IsValidIdempotencyKey(std::string_view key);

}  // namespace sov::cmd
