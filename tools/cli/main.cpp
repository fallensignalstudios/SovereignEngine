// sovereign-cli: offline validation, inspection and command batches for authoring scenes.
// Never initializes graphics. Writes go through SceneStore (writer lock + journal + atomic save).
//
// Exit codes (docs/architecture/command_protocol.md):
//   0 success (valid, committed, dry run, replayed duplicate, no change)
//   1 rejected (invalid document or batch, conflict, stale revision)
//   2 usage error
//   3 scene writer lock unavailable
//   4 I/O failure
//   5 internal error

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "sov/commands/command_batch.h"
#include "sov/commands/scene_store.h"
#include "sov/core/build_info.h"
#include "sov/core/fs.h"
#include "sov/core/log.h"
#include "sov/core/sha256.h"
#include "sov/world/scene_diff.h"

namespace {

using namespace sov;
using namespace sov::cmd;

constexpr int kExitOk = 0, kExitRejected = 1, kExitUsage = 2, kExitLock = 3, kExitIo = 4, kExitInternal = 5;

constexpr const char* kUsage = R"(usage: sovereign-cli <command> [options]

  --version
  scene validate --scene PATH [--json]
  scene inspect  --scene PATH [--json]
  scene new      --scene PATH --name NAME [--scene-id UUID] [--camera-id UUID] [--environment ENV.json]
  scene apply    --scene PATH --commands BATCH.json [--result OUT.json]
  scene undo     --scene PATH --base-revision N [--actor ACTOR] [--result OUT.json]
  scene redo     --scene PATH --base-revision N [--actor ACTOR] [--result OUT.json]
  scene history  --scene PATH [--json]
  scene diff     --before A.scene.json --after B.scene.json [--json]

Common options:
  --project-root DIR   confine every path argument to DIR (after normalization; symlink escapes rejected)

Batches are dry runs unless they set "dry_run": false. Results are printed as JSON on stdout.
)";

struct Args {
  std::vector<std::string> positional;
  std::map<std::string, std::string> options;
  bool json = false;
};

std::optional<Args> ParseArgs(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--json") {
      a.json = true;
    } else if (arg == "--version" || arg == "--help" || arg == "-h") {
      a.positional.push_back(arg);
    } else if (arg.rfind("--", 0) == 0) {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "option %s needs a value\n", arg.c_str());
        return std::nullopt;
      }
      a.options[arg.substr(2)] = argv[++i];
    } else {
      a.positional.push_back(arg);
    }
  }
  return a;
}

int ExitFor(ErrorCode code) {
  switch (code) {
    case ErrorCode::LockUnavailable: return kExitLock;
    case ErrorCode::IoFailure: return kExitIo;
    case ErrorCode::Internal: return kExitInternal;
    default: return kExitRejected;
  }
}

void PrintJson(const Json& j) { std::fputs(CanonicalJson(j).c_str(), stdout); }

Json ErrorJson(const Error& e) {
  return Json{{"code", ToString(e.code)},
              {"message", e.message},
              {"path", e.path.empty() ? Json(nullptr) : Json(e.path)},
              {"op_index", e.op_index >= 0 ? Json(e.op_index) : Json(nullptr)},
              {"entity_id", e.entity_id.empty() ? Json(nullptr) : Json(e.entity_id)}};
}

int Fail(const Error& e, bool as_json) {
  if (as_json) {
    PrintJson(Json{{"ok", false}, {"error", ErrorJson(e)}});
  } else {
    std::fprintf(stderr, "error: %s\n", e.Describe().c_str());
  }
  return ExitFor(e.code);
}

class Paths {
 public:
  explicit Paths(const Args& a) {
    if (auto it = a.options.find("project-root"); it != a.options.end()) {
      std::error_code ec;
      root_ = std::filesystem::weakly_canonical(std::filesystem::absolute(it->second), ec);
    }
  }
  // Resolves a path argument, enforcing confinement when --project-root was given.
  Result<std::filesystem::path> Resolve(const std::string& raw) const {
    std::filesystem::path p(raw);
    if (!root_) return p;
    if (p.is_absolute()) {
      return MakeError(ErrorCode::PermissionDenied, std::format("absolute path {} is not allowed with --project-root", raw));
    }
    std::error_code ec;
    const auto resolved = std::filesystem::weakly_canonical(*root_ / p, ec);  // resolves symlinks and ".."
    if (ec) return MakeError(ErrorCode::IoFailure, std::format("cannot resolve {}: {}", raw, ec.message()));
    const auto rel = resolved.lexically_relative(*root_);
    if (rel.empty() || *rel.begin() == "..") {
      return MakeError(ErrorCode::PermissionDenied, std::format("{} resolves outside the project root", raw));
    }
    return resolved;
  }

 private:
  std::optional<std::filesystem::path> root_;
};

Result<std::filesystem::path> Required(const Args& a, const Paths& paths, const char* name) {
  auto it = a.options.find(name);
  if (it == a.options.end()) return MakeError(ErrorCode::SchemaError, std::format("missing --{}", name));
  return paths.Resolve(it->second);
}

Result<void> WriteResultFile(const Args& a, const Paths& paths, const Json& result) {
  auto it = a.options.find("result");
  if (it == a.options.end()) return Ok();
  auto p = paths.Resolve(it->second);
  if (!p) return p.take_error();
  return fs::AtomicWriteFile(*p, CanonicalJson(result));
}

Json JournalJson(const JournalStatus& j) {
  return Json{{"present", j.present}, {"consistent", j.consistent}, {"notes", j.notes}};
}

// --- scene validate --------------------------------------------------------------------------
int CmdValidate(const Args& a, const Paths& paths) {
  auto path = Required(a, paths, "scene");
  if (!path) return Fail(path.error(), a.json);
  auto bytes = fs::ReadFile(*path, scene::SceneLimits::kMaxFileBytes);
  if (!bytes) return Fail(bytes.error(), a.json);
  scene::Diagnostics d;
  auto doc = scene::ParseSceneText(*bytes, &d);
  const bool canonical = doc.ok() && scene::SerializeScene(*doc) == *bytes;
  if (!canonical && doc.ok()) d.warnings.push_back("file is valid but not in canonical form; the next engine save will rewrite it");
  Json out = Json::object();
  out["ok"] = doc.ok();
  out["scene"] = path->string();
  if (doc.ok()) {
    out["scene_id"] = doc->scene_id.ToString();
    out["name"] = doc->name;
    out["revision"] = doc->revision;
    out["entity_count"] = doc->entities.size();
  }
  Json errs = Json::array();
  for (const auto& e : d.errors) errs.push_back(ErrorJson(e));
  out["errors"] = std::move(errs);
  out["warnings"] = d.warnings;
  out["canonical"] = canonical;
  out["file_sha256"] = Sha256Hex(*bytes);
  if (doc.ok()) {
    auto store = SceneStore::Open(*path, OpenMode::ReadOnly, "sovereign-cli");
    if (store) out["journal"] = JournalJson((*store)->journal_status());
  }
  if (a.json) {
    PrintJson(out);
  } else if (doc.ok()) {
    std::printf("valid: \"%s\" scene_id=%s revision=%llu entities=%zu%s\n", doc->name.c_str(),
                doc->scene_id.ToString().c_str(), static_cast<unsigned long long>(doc->revision), doc->entities.size(),
                canonical ? "" : " (non-canonical formatting)");
    for (const auto& w : d.warnings) std::printf("warning: %s\n", w.c_str());
  } else {
    std::printf("invalid: %s\n", path->string().c_str());
    for (const auto& e : d.errors) std::printf("  %s\n", e.Describe().c_str());
  }
  return doc.ok() ? kExitOk : kExitRejected;
}

// --- scene inspect ---------------------------------------------------------------------------
void PrintTree(const scene::SceneDocument& doc, const std::optional<Uuid>& parent, int depth) {
  for (const auto& e : doc.entities) {
    if (e.transform.parent != parent) continue;
    std::string comps;
    for (const auto& c : scene::Components()) {
      if (c.key != "transform" && c.has(e)) comps += (comps.empty() ? "" : ",") + std::string(c.key);
    }
    std::printf("%*s- %s  [%s]  {%s}\n", depth * 2, "", e.name.c_str(), e.id.ToString().c_str(), comps.c_str());
    PrintTree(doc, e.id, depth + 1);
  }
}

int CmdInspect(const Args& a, const Paths& paths) {
  auto path = Required(a, paths, "scene");
  if (!path) return Fail(path.error(), a.json);
  auto store = SceneStore::Open(*path, OpenMode::ReadOnly, "sovereign-cli");
  if (!store) return Fail(store.error(), a.json);
  const auto& doc = (*store)->document();
  const std::string holder = fs::FileLock::ReadHolder(SceneStore::LockPath(*path));
  if (a.json) {
    Json out = scene::SceneToJson(doc);
    out["journal"] = JournalJson((*store)->journal_status());
    out["history_length"] = (*store)->history().size();
    out["writer_lock_holder"] = holder.empty() ? Json(nullptr) : Json(holder);
    PrintJson(out);
    return kExitOk;
  }
  std::printf("scene: \"%s\" (%s)\nrevision: %llu\nentities: %zu\nactive camera: %s\n", doc.name.c_str(),
              doc.scene_id.ToString().c_str(), static_cast<unsigned long long>(doc.revision), doc.entities.size(),
              doc.active_camera ? doc.active_camera->ToString().c_str() : "(none)");
  std::printf("journal: %s, %zu committed transaction(s)%s\n",
              (*store)->journal_status().present ? "present" : "absent", (*store)->history().size(),
              (*store)->journal_status().consistent ? "" : " (INCONSISTENT; see notes)");
  for (const auto& n : (*store)->journal_status().notes) std::printf("  note: %s\n", n.c_str());
  std::printf("writer lock: %s\n", holder.empty() ? "free" : holder.c_str());
  std::printf("hierarchy:\n");
  PrintTree(doc, std::nullopt, 1);
  return kExitOk;
}

// --- scene new -------------------------------------------------------------------------------
int CmdNew(const Args& a, const Paths& paths) {
  auto path = Required(a, paths, "scene");
  if (!path) return Fail(path.error(), true);
  auto name_it = a.options.find("name");
  if (name_it == a.options.end()) return Fail(MakeError(ErrorCode::SchemaError, "missing --name"), true);
  if (auto err = scene::ValidateName(name_it->second)) return Fail(MakeError(ErrorCode::InvalidValue, *err), true);
  const auto uuid_opt = [&](const char* key) -> Result<Uuid> {
    auto it = a.options.find(key);
    if (it == a.options.end()) return GenerateUuid();
    auto id = Uuid::Parse(it->second);
    if (!id || id->IsNil()) return MakeError(ErrorCode::SchemaError, std::format("--{} must be a canonical UUID", key));
    return *id;
  };
  auto scene_id = uuid_opt("scene-id");
  if (!scene_id) return Fail(scene_id.error(), true);
  auto camera_id = uuid_opt("camera-id");
  if (!camera_id) return Fail(camera_id.error(), true);
  auto doc = scene::MakeNewScene(*scene_id, name_it->second, *camera_id);
  if (a.options.count("environment")) {
    auto env_path = paths.Resolve(a.options.at("environment"));
    if (!env_path) return Fail(env_path.error(), true);
    auto env_text = fs::ReadFile(*env_path, 64 * 1024);
    if (!env_text) return Fail(env_text.error(), true);
    auto env_json = ParseStrictJson(*env_text);
    if (!env_json) return Fail(env_json.error(), true);
    auto env = scene::ParseEnvironment(*env_json);
    if (!env) return Fail(env.error(), true);
    doc.environment = *env;
  }
  auto store = SceneStore::Create(*path, doc, "sovereign-cli");
  if (!store) return Fail(store.error(), true);
  PrintJson(Json{{"ok", true},
                 {"scene", path->string()},
                 {"scene_id", scene_id->ToString()},
                 {"camera_id", camera_id->ToString()},
                 {"revision", doc.revision}});
  return kExitOk;
}

// --- scene apply -----------------------------------------------------------------------------
int Finish(const Args& a, const Paths& paths, const CommandResult& r) {
  const Json j = r.ToJson();
  PrintJson(j);
  if (auto w = WriteResultFile(a, paths, j); !w) {
    std::fprintf(stderr, "error: could not write --result file: %s\n", w.error().Describe().c_str());
    return kExitIo;
  }
  if (r.replayed) {
    std::fprintf(stderr, "duplicate delivery: returned the recorded result of commit %s; nothing was applied\n",
                 j.value("commit_id", Json()).dump().c_str());
    return kExitOk;
  }
  switch (r.status) {
    case ResultStatus::Committed:
      std::fprintf(stderr, "committed: revision %llu -> %llu (commit %s)\n",
                   static_cast<unsigned long long>(r.revision_before), static_cast<unsigned long long>(r.revision_after),
                   r.commit_id ? r.commit_id->ToString().c_str() : "?");
      return kExitOk;
    case ResultStatus::DryRun:
      std::fprintf(stderr, "dry run: nothing was written (scene stays at revision %llu)\n",
                   static_cast<unsigned long long>(r.revision_before));
      return kExitOk;
    case ResultStatus::NoChange:
      std::fprintf(stderr, "no change: operations do not alter the scene\n");
      return kExitOk;
    case ResultStatus::Rejected:
      for (const auto& e : r.errors) std::fprintf(stderr, "rejected: %s\n", e.Describe().c_str());
      return r.errors.empty() ? kExitRejected : ExitFor(r.errors.front().code);
  }
  return kExitInternal;
}

int CmdApply(const Args& a, const Paths& paths) {
  auto path = Required(a, paths, "scene");
  if (!path) return Fail(path.error(), true);
  auto batch_path = Required(a, paths, "commands");
  if (!batch_path) return Fail(batch_path.error(), true);
  auto text = fs::ReadFile(*batch_path, BatchLimits::kMaxBatchBytes);
  if (!text) return Fail(text.error(), true);
  auto batch = ParseCommandBatchText(*text);
  if (!batch) return Fail(batch.error(), true);

  // Dry runs are read-only and do not need the writer lock; commits do.
  const OpenMode mode = batch->dry_run ? OpenMode::ReadOnly : OpenMode::Writer;
  auto store = SceneStore::Open(*path, mode, "sovereign-cli");
  if (!store) return Fail(store.error(), true);
  SceneSession session = (*store)->MakeSession();
  CommandResult r = session.ApplyBatch(*batch);
  for (const auto& n : (*store)->journal_status().notes) r.warnings.push_back("journal: " + n);
  if (r.status == ResultStatus::Committed) {
    if (auto c = (*store)->Commit(session.document(), session.PendingRecords()); !c) return Fail(c.error(), true);
  }
  return Finish(a, paths, r);
}

// --- scene undo / redo -----------------------------------------------------------------------
int CmdHistoryStep(const Args& a, const Paths& paths, bool undo) {
  auto path = Required(a, paths, "scene");
  if (!path) return Fail(path.error(), true);
  auto rev_it = a.options.find("base-revision");
  if (rev_it == a.options.end()) return Fail(MakeError(ErrorCode::SchemaError, "missing --base-revision"), true);
  std::uint64_t base = 0;
  try {
    std::size_t used = 0;
    base = std::stoull(rev_it->second, &used);
    if (used != rev_it->second.size()) throw std::invalid_argument("trailing characters");
  } catch (const std::exception&) {
    return Fail(MakeError(ErrorCode::SchemaError, "--base-revision must be a non-negative integer"), true);
  }
  const std::string actor = a.options.count("actor") ? a.options.at("actor") : "cli";
  if (!IsValidActor(actor)) return Fail(MakeError(ErrorCode::SchemaError, "invalid --actor"), true);
  auto store = SceneStore::Open(*path, OpenMode::Writer, "sovereign-cli");
  if (!store) return Fail(store.error(), true);
  SceneSession session = (*store)->MakeSession();
  CommandResult r = undo ? session.Undo(actor, base) : session.Redo(actor, base);
  if (r.status == ResultStatus::Committed) {
    if (auto c = (*store)->Commit(session.document(), session.PendingRecords()); !c) return Fail(c.error(), true);
  }
  return Finish(a, paths, r);
}

// --- scene history ---------------------------------------------------------------------------
int CmdHistory(const Args& a, const Paths& paths) {
  auto path = Required(a, paths, "scene");
  if (!path) return Fail(path.error(), a.json);
  auto store = SceneStore::Open(*path, OpenMode::ReadOnly, "sovereign-cli");
  if (!store) return Fail(store.error(), a.json);
  SceneSession s = (*store)->MakeSession();
  if (a.json) {
    Json list = Json::array();
    for (const auto& r : s.history()) {
      list.push_back(Json{{"commit_id", r.commit_id.ToString()},
                          {"kind", ToString(r.kind)},
                          {"actor", r.actor},
                          {"base_revision", r.base_revision},
                          {"new_revision", r.new_revision},
                          {"label", r.label},
                          {"idempotency_key", r.idempotency_key},
                          {"operations", r.forward.size()}});
    }
    PrintJson(Json{{"revision", s.revision()},
                   {"next_undo", s.NextUndo() ? Json(s.NextUndo()->ToString()) : Json(nullptr)},
                   {"next_redo", s.NextRedo() ? Json(s.NextRedo()->ToString()) : Json(nullptr)},
                   {"journal", JournalJson((*store)->journal_status())},
                   {"transactions", std::move(list)}});
    return kExitOk;
  }
  std::printf("revision %llu, %zu transaction(s) in journal\n", static_cast<unsigned long long>(s.revision()),
              s.history().size());
  for (const auto& r : s.history()) {
    std::printf("  r%llu -> r%llu  %-4s  %-18s  %s\n", static_cast<unsigned long long>(r.base_revision),
                static_cast<unsigned long long>(r.new_revision), std::string(ToString(r.kind)).c_str(), r.actor.c_str(),
                r.label.c_str());
  }
  std::printf("next undo: %s\nnext redo: %s\n", s.NextUndo() ? s.NextUndo()->ToString().c_str() : "(none)",
              s.NextRedo() ? s.NextRedo()->ToString().c_str() : "(none)");
  return kExitOk;
}

// --- scene diff ------------------------------------------------------------------------------
int CmdDiff(const Args& a, const Paths& paths) {
  auto before_path = Required(a, paths, "before");
  if (!before_path) return Fail(before_path.error(), a.json);
  auto after_path = Required(a, paths, "after");
  if (!after_path) return Fail(after_path.error(), a.json);
  auto before = scene::LoadSceneFile(*before_path);
  if (!before) return Fail(before.error(), a.json);
  auto after = scene::LoadSceneFile(*after_path);
  if (!after) return Fail(after.error(), a.json);
  if (before->document.scene_id != after->document.scene_id) {
    return Fail(MakeError(ErrorCode::SceneMismatch, "the two files are different scenes"), a.json);
  }
  const auto diff = scene::DiffScenes(before->document, after->document);
  if (a.json) {
    PrintJson(scene::DiffToJson(diff));
  } else {
    std::fputs(scene::DiffToText(diff).c_str(), stdout);
  }
  return kExitOk;
}

}  // namespace

int main(int argc, char** argv) {
  auto args = ParseArgs(argc, argv);
  if (!args) {
    std::fputs(kUsage, stderr);
    return kExitUsage;
  }
  const auto& pos = args->positional;
  if (!pos.empty() && pos[0] == "--version") {
    std::fputs(DescribeBuild("sovereign-cli").c_str(), stdout);
    std::fputs("graphics: sovereign-cli never initializes a graphics device\n", stdout);
    return kExitOk;
  }
  if (pos.empty() || pos[0] == "--help" || pos[0] == "-h" || pos.size() < 2 || pos[0] != "scene") {
    std::fputs(kUsage, pos.empty() || pos[0] == "--help" || pos[0] == "-h" ? stdout : stderr);
    return pos.empty() || pos[0] == "--help" || pos[0] == "-h" ? kExitOk : kExitUsage;
  }
  SetMinimumLogLevel(LogLevel::Warn);
  const Paths paths(*args);
  const std::string& verb = pos[1];
  try {
    if (verb == "validate") return CmdValidate(*args, paths);
    if (verb == "inspect") return CmdInspect(*args, paths);
    if (verb == "new") return CmdNew(*args, paths);
    if (verb == "apply") return CmdApply(*args, paths);
    if (verb == "undo") return CmdHistoryStep(*args, paths, true);
    if (verb == "redo") return CmdHistoryStep(*args, paths, false);
    if (verb == "history") return CmdHistory(*args, paths);
    if (verb == "diff") return CmdDiff(*args, paths);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "internal error: %s\n", e.what());
    return kExitInternal;
  }
  std::fprintf(stderr, "unknown command: scene %s\n\n%s", verb.c_str(), kUsage);
  return kExitUsage;
}
