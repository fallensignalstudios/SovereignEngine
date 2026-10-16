#include <set>
#include "scene_fixtures.h"
#include "sov/commands/command_batch.h"
#include "sov/commands/scene_session.h"
#include "sov_test.h"

using namespace sov;
using namespace sov::cmd;
using scene::EntityData;

namespace {

const Uuid kNewId = *Uuid::Parse("5c0e0000-0000-4000-8000-0000000000d1");
const Uuid kNewId2 = *Uuid::Parse("5c0e0000-0000-4000-8000-0000000000d2");

EntityData NewBox(const Uuid& id, const char* name) {
  EntityData e;
  e.id = id;
  e.name = name;
  e.transform.position = {0, 1, 0};
  e.primitive_mesh = scene::PrimitiveMeshComponent{scene::PrimitiveShape::Box};
  e.material.emplace();
  return e;
}

struct FakeClock {
  std::int64_t now = 1'800'000'000'000;  // ms since epoch
};

SceneSession MakeSession(FakeClock* clock = nullptr) {
  SessionOptions o;
  if (clock) o.now_ms = [clock] { return clock->now; };
  static UuidGenerator ids(7);
  o.new_id = [] { return ids.Next(); };
  return SceneSession(fixtures::SmallScene(), {}, o);
}

CommandBatch Batch(std::uint64_t base, std::vector<Operation> ops, std::string key = "key-1", bool dry_run = false) {
  CommandBatch b;
  b.request_id = *Uuid::Parse("5c0e0000-0000-4000-8000-00000000e001");
  b.actor = "agent:test";
  b.scene_id = fixtures::kSceneId;
  b.base_revision = base;
  b.idempotency_key = std::move(key);
  b.dry_run = dry_run;
  b.operations = std::move(ops);
  return b;
}

scene::TransformComponent MovedTo(const Vec3& p) {
  scene::TransformComponent t;
  t.position = p;
  return t;
}

std::vector<FieldAssignment> Roughness(double v) {
  return {{scene::FindField(scene::MaterialDescriptor(), "roughness"), v}};
}

}  // namespace

SOV_TEST(commands, each_operation_kind_applies) {
  SceneSession s = MakeSession();
  const auto r1 = s.ApplyEdit("editor", 3, {Operation::Create(NewBox(kNewId, "Banner"))}, "create");
  SOV_REQUIRE_MSG(r1.status == ResultStatus::Committed, CompactJson(r1.ToJson()));
  SOV_CHECK(s.document().Find(kNewId) != nullptr);
  const auto r2 = s.ApplyEdit("editor", 4, {Operation::Rename(kNewId, "Banner (Left)")}, "rename");
  SOV_CHECK(r2.status == ResultStatus::Committed);
  SOV_CHECK_EQ(s.document().Find(kNewId)->name, std::string("Banner (Left)"));
  const auto r3 = s.ApplyEdit("editor", 5, {Operation::SetTransform(kNewId, MovedTo({3, 4, 5}), false)}, "move");
  SOV_CHECK(r3.status == ResultStatus::Committed);
  SOV_CHECK(s.document().Find(kNewId)->transform.position == Vec3(3, 4, 5));
  const auto r4 = s.ApplyEdit("editor", 6, {Operation::SetMaterial(kNewId, Roughness(0.25))}, "material");
  SOV_CHECK(r4.status == ResultStatus::Committed);
  SOV_CHECK_EQ(s.document().Find(kNewId)->material->roughness, 0.25);
  const auto r5 = s.ApplyEdit("editor", 7, {Operation::Delete(kNewId)}, "delete");
  SOV_CHECK(r5.status == ResultStatus::Committed);
  SOV_CHECK(s.document().Find(kNewId) == nullptr);
  SOV_CHECK_EQ(s.revision(), std::uint64_t{8});
  SOV_CHECK_EQ(s.history().size(), std::size_t{5});
}

SOV_TEST(commands, batch_increments_revision_once_regardless_of_operation_count) {
  SceneSession s = MakeSession();
  const auto r = s.ApplyBatch(Batch(3, {Operation::Create(NewBox(kNewId, "A")), Operation::Create(NewBox(kNewId2, "B")),
                                        Operation::Rename(kNewId, "A2"), Operation::SetMaterial(kNewId2, Roughness(0.9))}));
  SOV_REQUIRE_MSG(r.status == ResultStatus::Committed, CompactJson(r.ToJson()));
  SOV_CHECK_EQ(r.revision_before, std::uint64_t{3});
  SOV_CHECK_EQ(r.revision_after, std::uint64_t{4});
  SOV_CHECK_EQ(s.revision(), std::uint64_t{4});
  SOV_CHECK_EQ(r.ToJson()["diff"]["entities_created"].size(), std::size_t{2});
}

SOV_TEST(commands, failed_batch_rolls_back_entirely) {
  SceneSession s = MakeSession();
  const auto before = s.document();
  const auto r = s.ApplyBatch(Batch(3, {Operation::Create(NewBox(kNewId, "A")),
                                        Operation::SetTransform(fixtures::kParentId, MovedTo({9, 9, 9}), false),
                                        Operation::Rename(*Uuid::Parse("5c0e0000-0000-4000-8000-0000000000ff"), "ghost"),
                                        Operation::Delete(kNewId)}));
  SOV_CHECK(r.status == ResultStatus::Rejected);
  SOV_REQUIRE(r.errors.size() == 1);
  SOV_CHECK_EQ(r.errors[0].code, ErrorCode::UnknownEntity);
  SOV_CHECK_EQ(r.errors[0].op_index, 2);
  SOV_REQUIRE(r.operations.size() == 4);
  // Ops before the failure passed staging but nothing was applied: "valid", never "applied".
  SOV_CHECK(r.operations[0].status == OpStatus::Valid);
  SOV_CHECK(r.operations[1].status == OpStatus::Valid);
  SOV_CHECK(r.operations[2].status == OpStatus::Failed);
  SOV_CHECK(r.operations[3].status == OpStatus::NotEvaluated);
  // No authored change at all: document, revision and history are exactly as before.
  SOV_CHECK(s.document() == before);
  SOV_CHECK_EQ(s.revision(), std::uint64_t{3});
  SOV_CHECK(s.history().empty());
  SOV_CHECK(!s.CanUndo());
}

SOV_TEST(commands, stale_revision_rejected_with_conflict_details) {
  SceneSession s = MakeSession();
  SOV_REQUIRE(s.ApplyEdit("editor:matthew", 3, {Operation::Rename(fixtures::kParentId, "Human Edit")}, "rename").ok());
  const auto before = s.document();
  // An agent still working from revision 3 must not overwrite the newer human edit.
  const auto r = s.ApplyBatch(Batch(3, {Operation::Rename(fixtures::kParentId, "Agent Edit")}));
  SOV_CHECK(r.status == ResultStatus::Rejected);
  SOV_REQUIRE(!r.errors.empty());
  SOV_CHECK_EQ(r.errors[0].code, ErrorCode::StaleRevision);
  SOV_CHECK(s.document() == before);
  const Json j = r.ToJson();
  SOV_CHECK_EQ(j["conflict"]["current_revision"].get<int>(), 4);
  SOV_CHECK_EQ(j["conflict"]["commits_since_base"].size(), std::size_t{1});
  SOV_CHECK_EQ(j["conflict"]["commits_since_base"][0]["actor"].get<std::string>(), std::string("editor:matthew"));
  SOV_CHECK(j["conflict"]["history_complete"].get<bool>());
  // Future revision is equally stale.
  SOV_CHECK(s.ApplyBatch(Batch(99, {Operation::Rename(fixtures::kParentId, "x")}, "k2")).errors[0].code ==
            ErrorCode::StaleRevision);
}

SOV_TEST(commands, dry_run_changes_nothing_and_reserves_nothing) {
  SceneSession s = MakeSession();
  const auto before = s.document();
  const auto dry = s.ApplyBatch(Batch(3, {Operation::Create(NewBox(kNewId, "Banner"))}, "banner-1", /*dry_run=*/true));
  SOV_CHECK(dry.status == ResultStatus::DryRun);
  SOV_CHECK_EQ(dry.revision_after, std::uint64_t{3});
  SOV_CHECK(!dry.commit_id.has_value());
  SOV_CHECK_EQ(dry.ToJson()["diff"]["entities_created"].size(), std::size_t{1});
  SOV_REQUIRE(dry.operations.size() == 1);
  SOV_CHECK(dry.operations[0].status == OpStatus::Valid);  // a dry run never reports an applied edit
  SOV_CHECK_EQ(dry.ToJson()["operations"][0]["status"].get<std::string>(), std::string("valid"));
  SOV_CHECK(s.document() == before);
  SOV_CHECK(s.history().empty());
  // The key was not reserved: a real commit with the same key and content applies normally.
  const auto real = s.ApplyBatch(Batch(3, {Operation::Create(NewBox(kNewId, "Banner"))}, "banner-1", false));
  SOV_CHECK(real.status == ResultStatus::Committed);
  SOV_CHECK(!real.replayed);
  SOV_CHECK(real.operations[0].status == OpStatus::Applied);
}

SOV_TEST(commands, dry_run_still_validates) {
  SceneSession s = MakeSession();
  const auto r = s.ApplyBatch(Batch(3, {Operation::Delete(fixtures::kParentId)}, "k", true));
  SOV_CHECK(r.status == ResultStatus::Rejected);
  SOV_CHECK_EQ(r.errors[0].code, ErrorCode::MissingDependency);
}

SOV_TEST(commands, duplicate_delivery_returns_recorded_result_without_reapplying) {
  SceneSession s = MakeSession();
  const CommandBatch b = Batch(3, {Operation::SetTransform(fixtures::kParentId, MovedTo({1, 2, 3}), false)}, "move-1");
  const auto first = s.ApplyBatch(b);
  SOV_REQUIRE(first.status == ResultStatus::Committed);
  const auto after_first = s.document();
  const auto second = s.ApplyBatch(b);  // identical retry, now against a newer revision
  SOV_CHECK(second.replayed);
  const Json j1 = first.ToJson(), j2 = second.ToJson();
  SOV_CHECK_EQ(j2["delivery"].get<std::string>(), std::string("replayed"));
  SOV_CHECK_EQ(j2["commit_id"], j1["commit_id"]);
  SOV_CHECK_EQ(j2["revision_after"], j1["revision_after"]);
  SOV_CHECK(s.document() == after_first);
  SOV_CHECK_EQ(s.revision(), std::uint64_t{4});
  SOV_CHECK_EQ(s.history().size(), std::size_t{1});
  // request_id does not participate in content identity: a retry with a new transport id replays too.
  CommandBatch retry = b;
  retry.request_id = *Uuid::Parse("5c0e0000-0000-4000-8000-00000000e002");
  SOV_CHECK(s.ApplyBatch(retry).replayed);
}

SOV_TEST(commands, reused_key_with_changed_content_is_rejected) {
  SceneSession s = MakeSession();
  SOV_REQUIRE(s.ApplyBatch(Batch(3, {Operation::Rename(fixtures::kParentId, "One")}, "rename-key")).ok());
  const auto before = s.document();
  const auto r = s.ApplyBatch(Batch(4, {Operation::Rename(fixtures::kParentId, "Two")}, "rename-key"));
  SOV_CHECK(r.status == ResultStatus::Rejected);
  SOV_CHECK_EQ(r.errors[0].code, ErrorCode::IdempotencyConflict);
  SOV_CHECK(s.document() == before);
  // Keys are scoped per actor: another actor may use the same key string.
  CommandBatch other = Batch(4, {Operation::Rename(fixtures::kParentId, "Two")}, "rename-key");
  other.actor = "agent:other";
  SOV_CHECK(s.ApplyBatch(other).status == ResultStatus::Committed);
}

SOV_TEST(commands, expired_key_reports_explicit_status) {
  FakeClock clock;
  SceneSession s = MakeSession(&clock);
  const CommandBatch b = Batch(3, {Operation::Rename(fixtures::kParentId, "Once")}, "old-key");
  SOV_REQUIRE(s.ApplyBatch(b).ok());
  clock.now += 8LL * 24 * 3600 * 1000;  // beyond the 7-day retention window
  const auto r = s.ApplyBatch(b);
  SOV_CHECK(r.status == ResultStatus::Rejected);
  SOV_CHECK_EQ(r.errors[0].code, ErrorCode::IdempotencyExpired);
  SOV_CHECK(!r.replayed);
  SOV_CHECK_EQ(s.revision(), std::uint64_t{4});
}

SOV_TEST(commands, scene_mismatch_rejected) {
  SceneSession s = MakeSession();
  CommandBatch b = Batch(3, {Operation::Rename(fixtures::kParentId, "x")});
  b.scene_id = *Uuid::Parse("5c0e0000-0000-4000-8000-0000000000ee");
  SOV_CHECK_EQ(s.ApplyBatch(b).errors[0].code, ErrorCode::SceneMismatch);
}

SOV_TEST(commands, delete_respects_dependencies_without_cascade) {
  SceneSession s = MakeSession();
  // Parent has a child: rejected, nothing removed.
  auto r = s.ApplyEdit("editor", 3, {Operation::Delete(fixtures::kParentId)}, "delete parent");
  SOV_CHECK(r.status == ResultStatus::Rejected);
  SOV_CHECK_EQ(r.errors[0].code, ErrorCode::MissingDependency);
  SOV_CHECK(r.errors[0].message.find(fixtures::kChildId.ToString()) != std::string::npos);
  SOV_CHECK(s.document().Find(fixtures::kChildId) != nullptr);
  // Wrong order in one batch fails as a whole.
  r = s.ApplyEdit("editor", 3, {Operation::Delete(fixtures::kParentId), Operation::Delete(fixtures::kChildId)}, "x");
  SOV_CHECK(r.status == ResultStatus::Rejected);
  SOV_CHECK(s.document().Find(fixtures::kChildId) != nullptr);
  // Explicit dependent operation first: allowed.
  r = s.ApplyEdit("editor", 3, {Operation::Delete(fixtures::kChildId), Operation::Delete(fixtures::kParentId)}, "x");
  SOV_CHECK(r.status == ResultStatus::Committed);
  SOV_CHECK(s.document().Find(fixtures::kParentId) == nullptr);
  // The active camera is a scene-level dependency.
  r = s.ApplyEdit("editor", 4, {Operation::Delete(fixtures::kCameraId)}, "x");
  SOV_CHECK(r.status == ResultStatus::Rejected);
  SOV_CHECK_EQ(r.errors[0].code, ErrorCode::MissingDependency);
}

SOV_TEST(commands, create_validates_identity_parent_and_limits) {
  SceneSession s = MakeSession();
  auto r = s.ApplyEdit("editor", 3, {Operation::Create(NewBox(fixtures::kParentId, "dup"))}, "x");
  SOV_CHECK_EQ(r.errors[0].code, ErrorCode::DuplicateEntity);
  EntityData orphan = NewBox(kNewId, "orphan");
  orphan.transform.parent = *Uuid::Parse("5c0e0000-0000-4000-8000-0000000000ff");
  r = s.ApplyEdit("editor", 3, {Operation::Create(orphan)}, "x");
  SOV_CHECK_EQ(r.errors[0].code, ErrorCode::MissingDependency);
  EntityData light = NewBox(kNewId, "second light");
  light.directional_light.emplace();
  r = s.ApplyEdit("editor", 3, {Operation::Create(light)}, "x");
  SOV_CHECK_EQ(r.errors[0].code, ErrorCode::LimitExceeded);
  // Parent created earlier in the same batch is visible to later operations.
  EntityData child = NewBox(kNewId2, "child of new");
  child.transform.parent = kNewId;
  r = s.ApplyEdit("editor", 3, {Operation::Create(NewBox(kNewId, "new parent")), Operation::Create(child)}, "x");
  SOV_CHECK(r.status == ResultStatus::Committed);
  SOV_CHECK_EQ(s.revision(), std::uint64_t{4});
}

SOV_TEST(commands, reparenting_rejects_cycles_and_inverse_restores_parent) {
  SceneSession s = MakeSession();
  scene::TransformComponent t = s.document().Find(fixtures::kParentId)->transform;
  t.parent = fixtures::kChildId;  // parent under its own child
  auto r = s.ApplyEdit("editor", 3, {Operation::SetTransform(fixtures::kParentId, t, true)}, "cycle");
  SOV_CHECK_EQ(r.errors[0].code, ErrorCode::CycleDetected);

  scene::TransformComponent ct = s.document().Find(fixtures::kChildId)->transform;
  ct.parent.reset();  // move child to scene root
  r = s.ApplyEdit("editor", 3, {Operation::SetTransform(fixtures::kChildId, ct, true)}, "unparent");
  SOV_CHECK(r.status == ResultStatus::Committed);
  SOV_CHECK(!s.document().Find(fixtures::kChildId)->transform.parent.has_value());
  SOV_REQUIRE(s.Undo("editor", 4).ok());
  SOV_CHECK(s.document().Find(fixtures::kChildId)->transform.parent == fixtures::kParentId);
}

SOV_TEST(commands, material_set_requires_material_component) {
  SceneSession s = MakeSession();
  const auto r = s.ApplyEdit("editor", 3, {Operation::SetMaterial(fixtures::kCameraId, Roughness(0.5))}, "x");
  SOV_CHECK_EQ(r.errors[0].code, ErrorCode::ComponentMissing);
}

SOV_TEST(commands, no_change_edit_does_not_bump_revision) {
  SceneSession s = MakeSession();
  const auto& t = s.document().Find(fixtures::kParentId)->transform;
  const auto r = s.ApplyEdit("editor", 3, {Operation::SetTransform(fixtures::kParentId, t, false)}, "same");
  SOV_CHECK(r.status == ResultStatus::NoChange);
  SOV_CHECK_EQ(s.revision(), std::uint64_t{3});
  SOV_CHECK(s.history().empty());
}

SOV_TEST(commands, undo_redo_restore_content_and_revisions_stay_monotonic) {
  SceneSession s = MakeSession();
  const std::string h0 = scene::ContentHash(s.document());
  SOV_REQUIRE(s.ApplyEdit("editor", 3, {Operation::SetTransform(fixtures::kParentId, MovedTo({7, 0, 0}), false)}, "move").ok());
  const std::string h1 = scene::ContentHash(s.document());
  SOV_REQUIRE(s.ApplyEdit("editor", 4, {Operation::SetMaterial(fixtures::kParentId, Roughness(0.1)),
                                        Operation::Delete(fixtures::kChildId)}, "material+delete").ok());
  const std::string h2 = scene::ContentHash(s.document());
  std::uint64_t last = s.revision();

  auto step = [&](CommandResult r, const std::string& expected_hash) {
    SOV_REQUIRE_MSG(r.status == ResultStatus::Committed, CompactJson(r.ToJson()));
    SOV_CHECK(s.revision() > last);  // undo/redo produce new revisions
    SOV_CHECK_EQ(r.revision_after, last + 1);
    last = s.revision();
    SOV_CHECK_EQ(scene::ContentHash(s.document()), expected_hash);
  };
  step(s.Undo("editor", last), h1);  // deleted child restored with its exact data
  SOV_CHECK(*s.document().Find(fixtures::kChildId) == *fixtures::SmallScene().Find(fixtures::kChildId));
  step(s.Undo("editor", last), h0);
  SOV_CHECK(!s.CanUndo());
  SOV_CHECK_EQ(s.Undo("editor", last).errors[0].code, ErrorCode::NothingToUndo);
  step(s.Redo("editor", last), h1);
  step(s.Redo("editor", last), h2);
  SOV_CHECK(!s.CanRedo());
  step(s.Undo("editor", last), h1);
  // A new edit clears the redo stack.
  SOV_REQUIRE(s.ApplyEdit("editor", last, {Operation::Rename(fixtures::kParentId, "Fresh")}, "rename").ok());
  last = s.revision();
  SOV_CHECK(!s.CanRedo());
  SOV_CHECK_EQ(s.Redo("editor", last).errors[0].code, ErrorCode::NothingToRedo);
  SOV_CHECK_EQ(s.revision(), std::uint64_t{3 + 2 + 5 + 1});
}

SOV_TEST(commands, undo_requires_current_revision) {
  SceneSession s = MakeSession();
  SOV_REQUIRE(s.ApplyEdit("editor", 3, {Operation::Rename(fixtures::kParentId, "x")}, "rename").ok());
  const auto r = s.Undo("cli", 3);
  SOV_CHECK_EQ(r.errors[0].code, ErrorCode::StaleRevision);
  SOV_CHECK_EQ(s.document().Find(fixtures::kParentId)->name, std::string("x"));
}

SOV_TEST(commands, history_rebuilds_undo_stack_from_records) {
  SceneSession a = MakeSession();
  SOV_REQUIRE(a.ApplyEdit("editor", 3, {Operation::Rename(fixtures::kParentId, "one")}, "1").ok());
  SOV_REQUIRE(a.ApplyEdit("editor", 4, {Operation::Rename(fixtures::kParentId, "two")}, "2").ok());
  SOV_REQUIRE(a.Undo("editor", 5).ok());
  // A new session over the same document + records has the same undo/redo state.
  SceneSession b(a.document(), a.history());
  SOV_CHECK_EQ(b.NextUndo(), a.NextUndo());
  SOV_CHECK_EQ(b.NextRedo(), a.NextRedo());
  SOV_REQUIRE(b.Redo("editor", 6).ok());
  SOV_CHECK_EQ(b.document().Find(fixtures::kParentId)->name, std::string("two"));
}

SOV_TEST(commands, batch_parsing_is_strict) {
  const auto parse = [](const char* text) { return ParseCommandBatchText(text); };
  const char* base = R"({"protocol":"sovereign.command_batch","protocol_version":1,
    "request_id":"5c0e0000-0000-4000-8000-00000000e001","actor":"agent:t",
    "scene_id":"5c0e0000-0000-4000-8000-000000000001","base_revision":3,"idempotency_key":"k",
    "operations":[{"op":"entity.rename","entity_id":"5c0e0000-0000-4000-8000-0000000000a1","name":"N"}]})";
  auto ok = parse(base);
  SOV_REQUIRE_OK(ok);
  SOV_CHECK(ok->dry_run);  // dry run unless explicitly disabled

  auto with = [&](const std::string& from, const std::string& to) {
    std::string s = base;
    const auto pos = s.find(from);
    s.replace(pos, from.size(), to);
    return ParseCommandBatchText(s);
  };
  SOV_CHECK_EQ(with("\"protocol_version\":1", "\"protocol_version\":2").error().code, ErrorCode::UnknownSchemaVersion);
  SOV_CHECK_EQ(with("\"entity.rename\"", "\"entity.undo\"").error().code, ErrorCode::SchemaError);
  SOV_CHECK_EQ(with("\"name\":\"N\"", "\"name\":\"N\",\"extra\":1").error().code, ErrorCode::SchemaError);
  SOV_CHECK_EQ(with("\"idempotency_key\":\"k\"", "\"idempotency_key\":\"bad key!\"").error().code, ErrorCode::SchemaError);
  SOV_CHECK_EQ(with("\"base_revision\":3", "\"base_revision\":-3").error().code, ErrorCode::SchemaError);
  SOV_CHECK_EQ(with("\"actor\":\"agent:t\",", "").error().code, ErrorCode::SchemaError);
  SOV_CHECK_EQ(with("\"base_revision\":3", "\"base_revision\":3,\"target\":\"x\"").error().code, ErrorCode::SchemaError);
  auto missing_scale = with(R"({"op":"entity.rename","entity_id":"5c0e0000-0000-4000-8000-0000000000a1","name":"N"})",
                            R"({"op":"transform.set","entity_id":"5c0e0000-0000-4000-8000-0000000000a1",
                                "transform":{"position":[0,0,0],"rotation":[0,0,0,1]}})");
  SOV_CHECK_EQ(missing_scale.error().code, ErrorCode::SchemaError);
  SOV_CHECK_EQ(missing_scale.error().op_index, 0);
  auto readonly = with(R"({"op":"entity.rename","entity_id":"5c0e0000-0000-4000-8000-0000000000a1","name":"N"})",
                       R"({"op":"material.set","entity_id":"5c0e0000-0000-4000-8000-0000000000a1",
                           "material":{"roughness":2.0}})");
  SOV_CHECK_EQ(readonly.error().code, ErrorCode::InvalidValue);

  std::string many = base;
  std::string op = R"({"op":"entity.delete","entity_id":"5c0e0000-0000-4000-8000-0000000000a1"})";
  std::string ops;
  for (int i = 0; i < 513; ++i) ops += (i ? "," : "") + op;
  many.replace(many.find("[{"), many.rfind("}]") + 2 - many.find("[{"), "[" + ops + "]");
  SOV_CHECK_EQ(ParseCommandBatchText(many).error().code, ErrorCode::LimitExceeded);
  SOV_CHECK_EQ(ParseCommandBatchText(std::string(BatchLimits::kMaxBatchBytes + 1, ' ')).error().code,
               ErrorCode::LimitExceeded);
}

SOV_TEST(commands, request_hash_binds_content_not_transport_fields) {
  const CommandBatch a = Batch(3, {Operation::Rename(fixtures::kParentId, "x")});
  CommandBatch b = a;
  b.request_id = *Uuid::Parse("5c0e0000-0000-4000-8000-00000000e009");
  b.dry_run = !a.dry_run;
  SOV_CHECK_EQ(a.ContentHash(), b.ContentHash());
  b.operations[0].name = "y";
  SOV_CHECK(a.ContentHash() != b.ContentHash());
  CommandBatch c = a;
  c.base_revision = 4;
  SOV_CHECK(a.ContentHash() != c.ContentHash());
  // Round trip through JSON keeps the hash.
  auto reparsed = ParseCommandBatch(CommandBatchToJson(a));
  SOV_REQUIRE_OK(reparsed);
  SOV_CHECK_EQ(reparsed->ContentHash(), a.ContentHash());
}

#include "sov/core/fs.h"

SOV_TEST(commands, schema_contracts_match_engine_codes_and_operations) {
  auto result_text = fs::ReadFile(ctx.SourceDir() / "schemas" / "command_result.schema.json", 1 << 20);
  auto batch_text = fs::ReadFile(ctx.SourceDir() / "schemas" / "command_batch.schema.json", 1 << 20);
  SOV_REQUIRE_OK(result_text);
  SOV_REQUIRE_OK(batch_text);
  auto result_schema = ParseStrictJson(*result_text);
  auto batch_schema = ParseStrictJson(*batch_text);
  SOV_REQUIRE_OK(result_schema);
  SOV_REQUIRE_OK(batch_schema);
  // Every engine error code is in the published enum, and the enum has nothing extra.
  const Json& codes = (*result_schema)["properties"]["errors"]["items"]["properties"]["code"]["enum"];
  std::set<std::string> published;
  for (const auto& c : codes) published.insert(c.get<std::string>());
  std::set<std::string> engine;
  for (int i = 0; i <= static_cast<int>(ErrorCode::Internal); ++i) engine.insert(std::string(ToString(static_cast<ErrorCode>(i))));
  SOV_CHECK(published == engine);
  // The batch schema's operation kinds are exactly the engine's.
  std::set<std::string> ops;
  for (const auto& ref : (*batch_schema)["$defs"]["operation"]["oneOf"]) {
    const std::string def = ref["$ref"].get<std::string>().substr(std::string("#/$defs/").size());
    ops.insert((*batch_schema)["$defs"][def]["properties"]["op"]["const"].get<std::string>());
  }
  std::set<std::string> engine_ops;
  for (auto k : {OpKind::EntityCreate, OpKind::EntityRename, OpKind::TransformSet, OpKind::MaterialSet, OpKind::EntityDelete}) {
    engine_ops.insert(std::string(ToString(k)));
  }
  SOV_CHECK(ops == engine_ops);
  // The shipped example parses with the engine and defaults to a dry run.
  auto example = fs::ReadFile(ctx.SourceDir() / "examples" / "place_banner.command_batch.json", 1 << 20);
  SOV_REQUIRE_OK(example);
  auto batch = ParseCommandBatchText(*example);
  SOV_REQUIRE_OK(batch);
  SOV_CHECK(batch->dry_run);
}
