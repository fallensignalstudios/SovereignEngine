#include "sov/editor/editor_document.h"

#include <format>

#include "sov/core/fs.h"
#include "sov/core/log.h"

namespace sov::editor {
namespace {

cmd::CommandResult Rejection(const scene::SceneDocument& doc, ErrorCode code, std::string message) {
  cmd::CommandResult r;
  r.status = cmd::ResultStatus::Rejected;
  r.scene_id = doc.scene_id;
  r.revision_before = r.revision_after = doc.revision;
  r.errors.push_back(MakeError(code, std::move(message)));
  return r;
}

}  // namespace

Result<std::unique_ptr<EditorDocument>> EditorDocument::Open(const std::filesystem::path& scene_path, std::string actor) {
  auto store = cmd::SceneStore::Open(scene_path, cmd::OpenMode::Writer, "sovereign-editor");
  if (!store) return store.take_error();
  std::unique_ptr<EditorDocument> doc(new EditorDocument());
  doc->store_ = std::move(store).value();
  doc->session_ = std::make_unique<cmd::SceneSession>(doc->store_->MakeSession());
  doc->actor_ = std::move(actor);
  doc->saved_revision_ = doc->session_->revision();
  for (const auto& note : doc->store_->journal_status().notes) LogWarn("editor", "journal: {}", note);
  for (const auto& w : doc->store_->load_warnings()) LogWarn("editor", "{}", w);
  doc->RebuildAuthoringWorld();
  return doc;
}

EditorDocument::~EditorDocument() { EndPlay(); }

void EditorDocument::RebuildAuthoringWorld() {
  auto world = World::FromDocument(session_->document(), WorldKind::Authoring);
  if (!world) {
    // Cannot happen for a document that passed staging; keep the previous world rather than crash.
    LogError("editor", "authoring world rebuild failed: {}", world.error().Describe());
    return;
  }
  authoring_world_ = std::move(world).value();
  ++world_generation_;
  if (selection_ && !session_->document().Find(*selection_)) selection_.reset();
}

cmd::CommandResult EditorDocument::Record(cmd::CommandResult r) {
  if (r.status == cmd::ResultStatus::Committed) RebuildAuthoringWorld();
  if (r.status == cmd::ResultStatus::Rejected) {
    for (const auto& e : r.errors) LogWarn("editor", "edit rejected: {}", e.Describe());
  }
  recent_.push_front(r);
  if (recent_.size() > 64) recent_.pop_back();
  return r;
}

cmd::CommandResult EditorDocument::Edit(std::vector<cmd::Operation> ops, std::string label) {
  if (playing()) {
    return Record(Rejection(document(), ErrorCode::InvalidState, "authoring edits are disabled while playing; stop play first"));
  }
  return Record(session_->ApplyEdit(actor_, session_->revision(), std::move(ops), std::move(label)));
}

Result<void> EditorDocument::Save() {
  if (auto r = store_->Commit(session_->document(), session_->PendingRecords()); !r) return r;
  session_->MarkAllPersisted();
  saved_revision_ = session_->revision();
  LogInfo("editor", "saved {} at revision {}", store_->path().string(), saved_revision_);
  return Ok();
}

void EditorDocument::Select(std::optional<Uuid> id) {
  if (id && !session_->document().Find(*id)) id.reset();
  selection_ = id;
}

cmd::CommandResult EditorDocument::SetTransform(const Uuid& id, const scene::TransformComponent& t, bool include_parent) {
  return Edit({cmd::Operation::SetTransform(id, t, include_parent)}, "Set transform");
}

cmd::CommandResult EditorDocument::Rename(const Uuid& id, std::string name) {
  return Edit({cmd::Operation::Rename(id, std::move(name))}, "Rename");
}

cmd::CommandResult EditorDocument::SetMaterialField(const Uuid& id, const scene::FieldDescriptor& field,
                                                    scene::FieldValue value) {
  return Edit({cmd::Operation::SetMaterial(id, {{&field, std::move(value)}})}, std::format("Set {}", field.label));
}

cmd::CommandResult EditorDocument::CreatePrimitive(scene::PrimitiveShape shape, const Vec3& position, std::string name) {
  scene::EntityData e;
  e.id = GenerateUuid();
  e.name = std::move(name);
  e.transform.position = position;
  e.primitive_mesh = scene::PrimitiveMeshComponent{shape};
  e.material.emplace();
  const Uuid id = e.id;
  auto r = Edit({cmd::Operation::Create(std::move(e))}, std::format("Create {}", scene::ToString(shape)));
  if (r.status == cmd::ResultStatus::Committed) Select(id);
  return r;
}

cmd::CommandResult EditorDocument::Delete(const Uuid& id) { return Edit({cmd::Operation::Delete(id)}, "Delete"); }

cmd::CommandResult EditorDocument::Undo() {
  if (playing()) return Record(Rejection(document(), ErrorCode::InvalidState, "undo is disabled while playing"));
  return Record(session_->Undo(actor_, session_->revision()));
}

cmd::CommandResult EditorDocument::Redo() {
  if (playing()) return Record(Rejection(document(), ErrorCode::InvalidState, "redo is disabled while playing"));
  return Record(session_->Redo(actor_, session_->revision()));
}

cmd::CommandResult EditorDocument::ApplyBatchFile(const std::filesystem::path& batch_path) {
  if (playing()) return Record(Rejection(document(), ErrorCode::InvalidState, "stop play before applying batches"));
  auto text = fs::ReadFile(batch_path, cmd::BatchLimits::kMaxBatchBytes);
  if (!text) return Record(Rejection(document(), text.error().code, text.error().message));
  auto batch = cmd::ParseCommandBatchText(*text);
  if (!batch) {
    cmd::CommandResult r = Rejection(document(), batch.error().code, batch.error().message);
    r.errors.front() = batch.error();
    return Record(r);
  }
  return Record(session_->ApplyBatch(*batch));
}

Result<void> EditorDocument::BeginPlay(std::unique_ptr<runtime::GameModule> game) {
  if (playing()) return MakeError(ErrorCode::InvalidState, "already playing");
  // The simulation copies what it needs from the document; nothing in the play world references it.
  auto sim = runtime::Simulation::Create(session_->document(), WorldKind::Play, std::move(game));
  if (!sim) return sim.take_error();
  play_ = std::move(sim).value();
  LogInfo("editor", "play started (world {}, game {})", play_->world().id(), play_->game_name());
  return Ok();
}

void EditorDocument::EndPlay() {
  if (!play_) return;
  LogInfo("editor", "play stopped after {} ticks; runtime world discarded", play_->tick_count());
  play_.reset();
}

}  // namespace sov::editor
