# Logging and errors

## Errors

Fallible engine functions return `sov::Result<T>` (`engine/core/include/sov/core/result.h`), a value
or an `Error { code, message, path, op_index, entity_id }`. No exceptions cross module boundaries;
third-party exceptions (nlohmann/json) are caught at the parse boundary and converted.

| Field | Meaning |
|---|---|
| `code` | stable machine code, serialized snake_case (table below); part of the wire contract |
| `message` | human sentence; may change between builds; never parsed |
| `path` | JSON pointer into the offending document or request (`/entities/3/components/transform/scale/1`) |
| `op_index` | index of the failing batch operation, `-1` otherwise |
| `entity_id` | entity UUID involved, when applicable |

| Code | Used for |
|---|---|
| `schema_error` | malformed JSON, unknown field, wrong type, duplicate key |
| `unknown_schema_version` | schema/protocol version this build does not read (checked before anything else) |
| `invalid_value` | well-typed value outside its range or set |
| `duplicate_entity` / `unknown_entity` | identity errors |
| `missing_dependency` | missing parent or camera; delete of a referenced entity |
| `unsupported_component` / `component_missing` | component type unknown / not present on entity |
| `cycle_detected` | parenting cycle |
| `limit_exceeded` | size, count, depth or length bound |
| `stale_revision` / `scene_mismatch` | revision or scene identity mismatch |
| `idempotency_conflict` / `idempotency_expired` | key reused with other content / older than retention |
| `nothing_to_undo` / `nothing_to_redo` / `invalid_state` | history and state errors |
| `permission_denied` / `lock_unavailable` / `io_failure` / `internal` | environment errors |

`commands.schema_contracts_match_engine_codes_and_operations` keeps this list, the C++ enum and
`schemas/command_result.schema.json` identical.

Validation collects **all** document diagnostics in one pass (`Diagnostics`) so a user sees every
problem at once; command staging stops at the first failing operation and marks the rest
`not_evaluated`, because later operations may depend on earlier ones.

## Logging

`sov::LogInfo/LogWarn/LogError/LogDebug(module, fmt, args...)` (`engine/core/include/sov/core/log.h`),
`std::format` syntax, compile-time checked. Line format on stderr:

```
2026-10-16T19:40:00.123Z [warn ] [vulkan] message
```

- Timestamps are ISO-8601 UTC with milliseconds. Modules are short lowercase tags (`core`, `scene`,
  `store`, `vulkan`, `renderer`, `editor`, `runtime`, `cli`).
- Additional sinks (editor Diagnostics panel, test capture) are added with `AddLogSink`.
- Machine output (CLI results) goes to **stdout** as JSON; logs go to **stderr**, so scripts can
  parse stdout without filtering.
- Vulkan validation messages are logged under `vulkan` and counted by type and severity; tests and
  executables fail on any validation-type error (exit 6). Performance-type warnings are reported
  separately and are not validation failures.
- No telemetry, network logging or upload exists. Logs do not record credentials. Paths in shared
  evidence are repository-relative where the tool allows it; absolute host paths
  (`/home/user/...`) appear only in local logs of this sandbox host.
