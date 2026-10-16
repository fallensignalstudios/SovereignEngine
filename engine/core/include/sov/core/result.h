#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace sov {

// Stable, machine-readable error categories. The string forms (ToString) are part of the
// command protocol and the CLI's JSON output; do not rename them without a protocol version bump.
enum class ErrorCode : std::uint16_t {
  SchemaError,           // malformed document/request, unknown field, wrong type
  UnknownSchemaVersion,  // schema/protocol version this build does not understand
  InvalidValue,          // well-typed value outside its allowed range or set
  DuplicateEntity,       // entity UUID already exists
  UnknownEntity,         // referenced entity UUID does not exist
  MissingDependency,     // reference to a missing parent/camera, or delete of a referenced entity
  UnsupportedComponent,  // component type unknown to this build
  ComponentMissing,      // operation targets a component the entity does not have
  CycleDetected,         // parenting would create a cycle
  LimitExceeded,         // size, count, depth or length bound exceeded
  StaleRevision,         // base_revision does not match the current scene revision
  SceneMismatch,         // request targets a different scene_id
  IdempotencyConflict,   // idempotency key reused with different request content
  IdempotencyExpired,    // idempotency key is older than the retention window
  NothingToUndo,
  NothingToRedo,
  InvalidState,          // operation not allowed in the current state
  PermissionDenied,
  LockUnavailable,       // another writer holds the scene lock
  IoFailure,
  Internal,
};

std::string_view ToString(ErrorCode code);

struct Error {
  ErrorCode code = ErrorCode::Internal;
  std::string message;
  std::string path;       // JSON pointer into the offending document, when applicable
  int op_index = -1;      // command operation index, when applicable
  std::string entity_id;  // affected entity UUID, when applicable

  // "schema_error at /entities/2/name: message"
  std::string Describe() const;
};

Error MakeError(ErrorCode code, std::string message, std::string path = {});

// Minimal expected-like result. Holds either a value or an Error.
template <class T>
class [[nodiscard]] Result {
 public:
  Result(T value) : storage_(std::in_place_index<0>, std::move(value)) {}  // NOLINT(implicit)
  Result(Error error) : storage_(std::in_place_index<1>, std::move(error)) {}  // NOLINT(implicit)

  bool ok() const { return storage_.index() == 0; }
  explicit operator bool() const { return ok(); }

  T& value() & { return std::get<0>(storage_); }
  const T& value() const& { return std::get<0>(storage_); }
  T&& value() && { return std::get<0>(std::move(storage_)); }
  T* operator->() { return &value(); }
  const T* operator->() const { return &value(); }
  T& operator*() & { return value(); }
  const T& operator*() const& { return value(); }

  const Error& error() const { return std::get<1>(storage_); }
  Error&& take_error() { return std::get<1>(std::move(storage_)); }

 private:
  std::variant<T, Error> storage_;
};

template <>
class [[nodiscard]] Result<void> {
 public:
  Result() = default;
  Result(Error error) : error_(std::move(error)), ok_(false) {}  // NOLINT(implicit)

  bool ok() const { return ok_; }
  explicit operator bool() const { return ok_; }
  const Error& error() const { return error_; }
  Error&& take_error() { return std::move(error_); }

 private:
  Error error_;
  bool ok_ = true;
};

inline Result<void> Ok() { return {}; }

}  // namespace sov
