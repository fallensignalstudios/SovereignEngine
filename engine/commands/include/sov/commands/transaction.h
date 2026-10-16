#pragma once

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "sov/commands/operation.h"
#include "sov/world/scene_document.h"

namespace sov::cmd {

// Valid: passed staged validation but was not applied (dry run, no-change or rejected batch).
// Applied: only in committed results.
enum class OpStatus { Applied, Valid, Failed, NotEvaluated };
std::string_view ToString(OpStatus status);

struct OpOutcome {
  int index = 0;
  OpKind kind = OpKind::EntityRename;
  Uuid entity_id;
  OpStatus status = OpStatus::NotEvaluated;
};

// Result of staging a batch against a copy of the document.
//
// Contract (docs/architecture/command_protocol.md):
//   - operations apply in array order to a staged copy; each is validated against the state left
//     by the operations before it (so "delete child, then delete parent" works and the reverse fails)
//   - the first failing operation stops staging; later operations are reported NotEvaluated
//   - after all operations, the staged copy must pass full structural validation
//   - on any error the caller discards the staged copy: no partial batch is ever committed
//   - `inverse` restores the base document exactly when applied to `staged`
struct StageResult {
  scene::SceneDocument staged;
  std::vector<Operation> inverse;
  std::vector<OpOutcome> outcomes;
  std::vector<Error> errors;
  bool ok() const { return errors.empty(); }
};

StageResult StageOperations(const scene::SceneDocument& base, std::span<const Operation> ops);

}  // namespace sov::cmd
