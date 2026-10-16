#include "automation.h"

#include <format>

#include "sov/core/fs.h"
#include "sov/core/sha256.h"
#include "sov/world/reflection.h"

namespace sov::editor_app {

Result<AutomationRunner> AutomationRunner::Load(const std::filesystem::path& script) {
  auto text = fs::ReadFile(script, 1u << 20);
  if (!text) return text.take_error();
  auto j = ParseStrictJson(*text);
  if (!j) return j.take_error();
  if (!j->contains("steps") || !(*j)["steps"].is_array()) {
    return MakeError(ErrorCode::SchemaError, "automation script needs a \"steps\" array");
  }
  AutomationRunner r;
  for (const auto& s : (*j)["steps"]) {
    if (!s.is_object() || !s.contains("do")) return MakeError(ErrorCode::SchemaError, "each step needs a \"do\" field");
    r.steps_.push_back(s);
  }
  return r;
}

void AutomationRunner::Record(const Json& step, bool ok, std::string detail) {
  if (!ok) ++failures_;
  if (ok) {
    LogInfo("automation", "ok   {} {}", CompactJson(step), detail);
  } else {
    LogError("automation", "FAIL {} {}", CompactJson(step), detail);
  }
  transcript_.push_back(Json{{"step", step}, {"ok", ok}, {"detail", detail}, {"at", CurrentUtcTimestamp()}});
}

const scene::EntityData* AutomationRunner::Named(EditorApp& app, const Json& step, const char* key) {
  const std::string name = step.value(key, std::string());
  for (const auto& e : app.doc().document().entities) {
    if (e.name == name) return &e;
  }
  return nullptr;
}

bool AutomationRunner::Tick(EditorApp& app) {
  if (index_ >= steps_.size()) return false;
  if (wait_ > 0) {
    --wait_;
    return true;
  }
  const Json& step = steps_[index_];
  if (!step_started_) {
    step_started_ = true;
    step_frame_ = app.frame_number();
  }
  if (Step(app, step)) {
    ++index_;
    step_started_ = false;
    wait_ = 2;  // let the UI and renderer settle between steps
  }
  return index_ < steps_.size();
}

bool AutomationRunner::Step(EditorApp& app, const Json& step) {
  const std::string what = step["do"].get<std::string>();
  auto& doc = app.doc();
  const std::uint64_t elapsed = app.frame_number() - step_frame_;

  if (what == "wait_frames") {
    if (elapsed < step.value("n", 1ull)) return false;
    Record(step, true, "");
    return true;
  }
  if (what == "select") {
    const auto* e = Named(app, step);
    if (e) doc.Select(e->id);
    Record(step, e != nullptr, e ? e->id.ToString() : "no such entity");
    return true;
  }
  if (what == "pick") {
    // Click the entity's projected centre through the viewport's pick path (GPU ID readback).
    const auto* e = Named(app, step);
    if (!e) {
      Record(step, false, "no such entity");
      return true;
    }
    auto& vp = app.viewport();
    if (elapsed == 0) {
      doc.Select(std::nullopt);
      World& w = doc.authoring_world();
      const Vec3 p = GetTranslation(*w.GetWorldMatrix(w.Find(e->id)));
      const render::ViewCamera cam = app.camera().View();
      Vec3 ndc;
      const bool visible = ProjectToNdc(cam.Projection(double(vp.width) / double(vp.height)) * cam.ViewMatrix(), p, ndc);
      if (!visible || std::abs(ndc.x) > 1 || std::abs(ndc.y) > 1) {
        Record(step, false, "entity centre is not in view");
        return true;
      }
      const Vec2 px = NdcToPixel(ndc, vp.width, vp.height);
      vp.last_pick.reset();
      vp.pending_click = std::array<std::uint32_t, 2>{static_cast<std::uint32_t>(px.x), static_cast<std::uint32_t>(px.y)};
      return false;
    }
    if (!vp.last_pick && elapsed < 30) return false;
    const bool ok = doc.selection() == e->id;
    Record(step, ok,
           vp.last_pick ? std::format("pixel ({}, {}) -> pick index {} -> {}", vp.last_pick->x, vp.last_pick->y,
                                      vp.last_pick->pick_index,
                                      vp.last_pick->entity ? vp.last_pick->entity->ToString() : std::string("(none)"))
                        : std::string("pick never resolved"));
    return true;
  }
  if (what == "move") {
    const auto* e = Named(app, step);
    if (!e) {
      Record(step, false, "no such entity");
      return true;
    }
    scene::TransformComponent t = e->transform;
    const auto& d = step["delta"];
    t.position = t.position + Vec3{d[0].get<double>(), d[1].get<double>(), d[2].get<double>()};
    const auto r = doc.SetTransform(e->id, t);  // same call as the inspector's commit
    Record(step, r.status == cmd::ResultStatus::Committed, std::format("revision {} -> {}", r.revision_before, r.revision_after));
    return true;
  }
  if (what == "set_material") {
    const auto* e = Named(app, step);
    const auto* f = scene::FindField(scene::MaterialDescriptor(), step.value("field", std::string()));
    if (!e || !f) {
      Record(step, false, "no such entity or material field");
      return true;
    }
    auto v = scene::ParseFieldValue(*f, step["value"], "/value");
    if (!v) {
      Record(step, false, v.error().Describe());
      return true;
    }
    const auto r = doc.SetMaterialField(e->id, *f, *v);
    Record(step, r.status == cmd::ResultStatus::Committed, std::format("revision {} -> {}", r.revision_before, r.revision_after));
    return true;
  }
  if (what == "rename") {
    const auto* e = Named(app, step);
    const auto r = e ? doc.Rename(e->id, step.value("to", std::string())) : cmd::CommandResult{};
    Record(step, e && r.status == cmd::ResultStatus::Committed, e ? std::format("revision {}", r.revision_after) : "no such entity");
    return true;
  }
  if (what == "create") {
    auto shape = scene::ParsePrimitiveShape(step.value("shape", std::string("box")));
    const auto& p = step["position"];
    const auto r = doc.CreatePrimitive(shape.value_or(scene::PrimitiveShape::Box),
                                       {p[0].get<double>(), p[1].get<double>(), p[2].get<double>()},
                                       step.value("name", std::string("Box")));
    Record(step, r.status == cmd::ResultStatus::Committed, std::format("revision {}", r.revision_after));
    return true;
  }
  if (what == "undo" || what == "redo") {
    const auto r = what == "undo" ? doc.Undo() : doc.Redo();
    Record(step, r.status == cmd::ResultStatus::Committed, std::format("revision {} -> {}", r.revision_before, r.revision_after));
    return true;
  }
  if (what == "save") {
    app.Save();
    Record(step, !doc.dirty(), std::format("saved at revision {}", doc.saved_revision()));
    return true;
  }
  if (what == "reopen") {
    auto r = app.Reopen();
    Record(step, r.ok(), r.ok() ? std::format("revision {}", app.doc().revision()) : r.error().Describe());
    return true;
  }
  if (what == "apply_batch") {
    const auto r = doc.ApplyBatchFile(step.value("path", std::string()));
    Record(step, r.ok(), std::format("{} revision {} -> {}", cmd::ToString(r.status), r.revision_before, r.revision_after));
    return true;
  }
  if (what == "play") {
    app.StartPlay();
    Record(step, app.playing(), "");
    return true;
  }
  if (what == "stop") {
    app.StopPlay();
    Record(step, !app.playing(), "");
    return true;
  }
  if (what == "play_ticks") {
    if (!app.playing()) {
      Record(step, false, "not playing");
      return true;
    }
    if (doc.play()->tick_count() < step.value("n", 60ull)) return false;
    Record(step, true, std::format("{} ticks", doc.play()->tick_count()));
    return true;
  }
  if (what == "camera") {
    auto& cam = app.camera();
    const auto& p = step["position"];
    cam.position = {p[0].get<double>(), p[1].get<double>(), p[2].get<double>()};
    cam.yaw_deg = step.value("yaw", 0.0);
    cam.pitch_deg = step.value("pitch", 0.0);
    Record(step, true, "");
    return true;
  }
  if (what == "resize_window") {
    if (elapsed == 0) {
      app.window().SetSize(step.value("w", 1280), step.value("h", 720));
      return false;
    }
    if (elapsed < 15) return false;
    int w = 0, h = 0;
    app.window().PixelSize(w, h);
    Record(step, true, std::format("window now {}x{}", w, h));
    return true;
  }
  if (what == "minimize" || what == "restore") {
    if (elapsed == 0) {
      what == "minimize" ? app.window().Minimize() : app.window().Restore();
      return false;
    }
    if (elapsed < 20) return false;
    Record(step, true, std::format("frame rendered after step: {}", app.last_frame_rendered()));
    return true;
  }
  if (what == "capture_window" || what == "capture_viewport") {
    const std::filesystem::path path = step.value("path", std::string("capture.png"));
    if (elapsed == 0) {
      std::error_code ec;
      std::filesystem::remove(path, ec);
      what == "capture_window" ? app.RequestWindowCapture(path) : app.RequestViewportCapture(path);
      return false;
    }
    if (!std::filesystem::exists(path) && elapsed < 60) return false;
    Record(step, std::filesystem::exists(path), path.string());
    return true;
  }
  if (what == "expect") {
    std::vector<std::string> problems;
    if (step.contains("revision") && step["revision"].get<std::uint64_t>() != doc.revision()) {
      problems.push_back(std::format("revision is {}", doc.revision()));
    }
    if (step.contains("dirty") && step["dirty"].get<bool>() != doc.dirty()) problems.push_back(std::format("dirty is {}", doc.dirty()));
    if (step.contains("playing") && step["playing"].get<bool>() != app.playing()) problems.push_back("playing mismatch");
    if (step.contains("selected")) {
      const auto* e = Named(app, step, "selected");
      if (!e || doc.selection() != e->id) problems.push_back("selection mismatch");
    }
    if (step.contains("document_hash_equals_saved") && step["document_hash_equals_saved"].get<bool>()) {
      auto bytes = fs::ReadFile(doc.path(), scene::SceneLimits::kMaxFileBytes);
      if (!bytes || *bytes != scene::SerializeScene(doc.document())) problems.push_back("in-memory document differs from file");
    }
    std::string detail;
    for (const auto& p : problems) detail += p + "; ";
    Record(step, problems.empty(), detail);
    return true;
  }
  if (what == "cycles") {
    // Repeated save / play / stop / reopen; GPU resources and validation must return to baseline.
    const int n = step.value("n", 10);
    switch (cycle_phase_) {
      case 0:
        app.renderer().WaitIdle();
        cycle_baseline_ = app.renderer().resources();
        cycle_validation_baseline_ = app.renderer().validation().errors;
        cycle_ = 0;
        cycle_phase_ = 1;
        return false;
      case 1:
        app.Save();
        app.StartPlay();
        step_frame_ = app.frame_number();
        cycle_phase_ = 2;
        return false;
      case 2:
        if (elapsed < 15) return false;
        app.StopPlay();
        if (auto r = app.Reopen(); !r) {
          Record(step, false, r.error().Describe());
          cycle_phase_ = 0;
          return true;
        }
        cycle_phase_ = ++cycle_ < n ? 1 : 3;
        return false;
      default: {
        app.renderer().WaitIdle();
        const auto after = app.renderer().resources();
        const auto errors = app.renderer().validation().errors;
        const bool ok = after.buffers == cycle_baseline_.buffers && after.images == cycle_baseline_.images &&
                        after.vma_allocations == cycle_baseline_.vma_allocations && errors == cycle_validation_baseline_ &&
                        World::LiveWorldCount() == 1;
        Record(step, ok,
               std::format("{} cycles; buffers {}->{} images {}->{} allocations {}->{} validation errors {}->{} live worlds {}",
                           n, cycle_baseline_.buffers, after.buffers, cycle_baseline_.images, after.images,
                           cycle_baseline_.vma_allocations, after.vma_allocations, cycle_validation_baseline_, errors,
                           World::LiveWorldCount()));
        cycle_phase_ = 0;
        return true;
      }
    }
  }
  if (what == "report") {
    const auto v = app.renderer().validation();
    const auto res = app.renderer().resources();
    const auto& d = app.renderer().device();
    Json report = Json::object();
    report["scene"] = doc.path().string();
    report["revision"] = doc.revision();
    report["saved_revision"] = doc.saved_revision();
    report["dirty"] = doc.dirty();
    auto bytes = fs::ReadFile(doc.path(), scene::SceneLimits::kMaxFileBytes);
    report["file_sha256"] = bytes ? Sha256Hex(*bytes) : "";
    report["device"] = Json{{"name", d.name}, {"type", d.device_type}, {"driver", d.driver}, {"evidence_class", d.evidence_class},
                            {"swapchain", d.swapchain}};
    report["validation"] = Json{{"layer", v.layer_enabled}, {"synchronization", v.synchronization_validation},
                                {"errors", v.errors}, {"warnings", v.warnings}, {"messages", v.messages}};
    report["resources"] = Json{{"buffers", res.buffers}, {"images", res.images}, {"image_views", res.image_views},
                               {"vma_allocations", res.vma_allocations}, {"vma_bytes", res.vma_bytes}};
    report["frames"] = app.frame_number();
    report["automation_failures"] = failures_;
    report["transcript"] = transcript_;
    const auto w = fs::AtomicWriteFile(step.value("path", std::string("editor_report.json")), CanonicalJson(report));
    Record(step, w.ok(), step.value("path", std::string("editor_report.json")));
    return true;
  }
  if (what == "quit") {
    app.RequestQuit();
    Record(step, true, "");
    return true;
  }
  Record(step, false, "unknown automation step");
  return true;
}

}  // namespace sov::editor_app
