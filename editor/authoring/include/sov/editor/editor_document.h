#pragma once

#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "sov/commands/scene_store.h"
#include "sov/runtime/simulation.h"
#include "sov/world/reflection.h"
#include "sov/world/world.h"

// Editor authoring services: the single object the editor's panels call. Every edit becomes a
// typed operation through SceneSession (the same service sovereign-cli uses); panels never touch
// JSON or ECS storage directly. No UI library dependency, so it is fully testable headless.
namespace sov::editor {

class EditorDocument {
 public:
  // Opens the scene and holds its writer lock for the document's lifetime (the CLI's offline
  // writes are refused meanwhile).
  static Result<std::unique_ptr<EditorDocument>> Open(const std::filesystem::path& scene_path,
                                                      std::string actor = "editor");
  ~EditorDocument();

  const std::filesystem::path& path() const { return store_->path(); }
  const std::string& actor() const { return actor_; }
  const scene::SceneDocument& document() const { return session_->document(); }
  cmd::SceneSession& session() { return *session_; }
  const cmd::SceneStore& store() const { return *store_; }

  std::uint64_t revision() const { return session_->revision(); }
  std::uint64_t saved_revision() const { return saved_revision_; }
  bool dirty() const { return session_->revision() != saved_revision_; }

  // Writes the document and the transactions committed since the last save (journal + atomic save).
  Result<void> Save();

  // --- selection (by stable UUID, never by runtime slot) -------------------------------------
  const std::optional<Uuid>& selection() const { return selection_; }
  void Select(std::optional<Uuid> id);

  // --- edit actions: each is one transaction, base_revision = current revision ---------------
  // Complete transform, including the parent (transform.set is a full replacement).
  cmd::CommandResult SetTransform(const Uuid& id, const scene::TransformComponent& t);
  cmd::CommandResult Rename(const Uuid& id, std::string name);
  // One inspector field: sent as material.set with the complete material (current values + this field).
  cmd::CommandResult SetMaterialField(const Uuid& id, const scene::FieldDescriptor& field, scene::FieldValue value);
  cmd::CommandResult CreatePrimitive(scene::PrimitiveShape shape, const Vec3& position, std::string name);
  cmd::CommandResult Delete(const Uuid& id);
  cmd::CommandResult Undo();
  cmd::CommandResult Redo();
  // Applies a command batch file through the same SceneSession::ApplyBatch path as the CLI.
  cmd::CommandResult ApplyBatchFile(const std::filesystem::path& batch_path);

  // Last results (most recent first) for the diagnostics/history panels.
  const std::deque<cmd::CommandResult>& recent_results() const { return recent_; }

  // --- authoring world (rendered by the viewport; rebuilt after every committed change) -------
  World& authoring_world() { return *authoring_world_; }
  std::uint64_t authoring_world_generation() const { return world_generation_; }

  // --- play: a separate runtime world built from a snapshot of the validated document ---------
  Result<void> BeginPlay(std::unique_ptr<runtime::GameModule> game);
  void EndPlay();
  bool playing() const { return play_ != nullptr; }
  runtime::Simulation* play() { return play_.get(); }

 private:
  EditorDocument() = default;
  cmd::CommandResult Record(cmd::CommandResult r);
  void RebuildAuthoringWorld();
  cmd::CommandResult Edit(std::vector<cmd::Operation> ops, std::string label);

  std::unique_ptr<cmd::SceneStore> store_;
  std::unique_ptr<cmd::SceneSession> session_;
  std::string actor_;
  std::uint64_t saved_revision_ = 0;
  std::optional<Uuid> selection_;
  std::deque<cmd::CommandResult> recent_;
  std::unique_ptr<World> authoring_world_;
  std::uint64_t world_generation_ = 0;
  std::unique_ptr<runtime::Simulation> play_;
};

}  // namespace sov::editor
