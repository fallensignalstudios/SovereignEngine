#include "sov/core/result.h"

namespace sov {

std::string_view ToString(ErrorCode code) {
  switch (code) {
    case ErrorCode::SchemaError: return "schema_error";
    case ErrorCode::UnknownSchemaVersion: return "unknown_schema_version";
    case ErrorCode::InvalidValue: return "invalid_value";
    case ErrorCode::DuplicateEntity: return "duplicate_entity";
    case ErrorCode::UnknownEntity: return "unknown_entity";
    case ErrorCode::MissingDependency: return "missing_dependency";
    case ErrorCode::UnsupportedComponent: return "unsupported_component";
    case ErrorCode::ComponentMissing: return "component_missing";
    case ErrorCode::CycleDetected: return "cycle_detected";
    case ErrorCode::LimitExceeded: return "limit_exceeded";
    case ErrorCode::StaleRevision: return "stale_revision";
    case ErrorCode::SceneMismatch: return "scene_mismatch";
    case ErrorCode::IdempotencyConflict: return "idempotency_conflict";
    case ErrorCode::IdempotencyExpired: return "idempotency_expired";
    case ErrorCode::NothingToUndo: return "nothing_to_undo";
    case ErrorCode::NothingToRedo: return "nothing_to_redo";
    case ErrorCode::InvalidState: return "invalid_state";
    case ErrorCode::PermissionDenied: return "permission_denied";
    case ErrorCode::LockUnavailable: return "lock_unavailable";
    case ErrorCode::IoFailure: return "io_failure";
    case ErrorCode::Internal: return "internal";
  }
  return "internal";
}

std::string Error::Describe() const {
  std::string out(ToString(code));
  if (!path.empty()) {
    out += " at ";
    out += path;
  }
  if (op_index >= 0) {
    out += " (operation ";
    out += std::to_string(op_index);
    out += ")";
  }
  out += ": ";
  out += message;
  return out;
}

Error MakeError(ErrorCode code, std::string message, std::string path) {
  Error e;
  e.code = code;
  e.message = std::move(message);
  e.path = std::move(path);
  return e;
}

}  // namespace sov
