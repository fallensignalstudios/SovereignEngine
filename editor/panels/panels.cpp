// Editor panels. Every edit goes through EditorDocument (typed operations via SceneSession);
// widgets preview drags on the authoring world and commit one coalesced transaction on release.
#include <algorithm>
#include <cstring>
#include <format>
#include <map>
#include <vector>

#include <imgui.h>

#include "../application/editor_app.h"
#include "sov/world/reflection.h"

namespace sov::editor_app::panels {
namespace {

const ImVec4 kWarn{1.0f, 0.62f, 0.25f, 1.0f};
const ImVec4 kError{1.0f, 0.35f, 0.30f, 1.0f};
const ImVec4 kOk{0.55f, 0.85f, 0.55f, 1.0f};
const ImVec4 kDim{0.6f, 0.6f, 0.6f, 1.0f};

void Tooltip(std::string_view text) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%.*s", static_cast<int>(text.size()), text.data());
}

std::string ComponentTags(const scene::EntityData& e) {
  std::string t;
  if (e.camera) t += " [camera]";
  if (e.directional_light) t += " [light]";
  if (!e.primitive_mesh && !e.camera && !e.directional_light) t += " [group]";
  return t;
}

void ShowResultErrors(const cmd::CommandResult& r) {
  for (const auto& e : r.errors) ImGui::TextColored(kError, "%s", e.Describe().c_str());
}

// Descendants of `id` (cannot become its parent).
bool IsDescendant(const scene::SceneDocument& d, const Uuid& candidate, const Uuid& root) {
  const scene::EntityData* e = d.Find(candidate);
  for (int guard = 0; e && guard < 70; ++guard) {
    if (e->id == root) return true;
    e = e->transform.parent ? d.Find(*e->transform.parent) : nullptr;
  }
  return false;
}

}  // namespace

void MainMenu(EditorApp& app) {
  static char batch_path[512] = "examples/place_banner.command_batch.json";
  bool open_batch = false;
  auto& doc = app.doc();
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Save", "Ctrl+S", false, !app.playing())) app.Save();
      if (ImGui::MenuItem("Reopen from disk", nullptr, false, !app.playing())) {
        if (auto r = app.Reopen(); !r) LogError("editor", "reopen failed: {}", r.error().Describe());
      }
      if (ImGui::MenuItem("Apply command batch...", nullptr, false, !app.playing())) open_batch = true;
      ImGui::Separator();
      if (ImGui::MenuItem("Capture window")) app.RequestWindowCapture(std::format("editor_window_{}.png", app.frame_number()));
      if (ImGui::MenuItem("Capture viewport")) app.RequestViewportCapture(std::format("editor_viewport_{}.png", app.frame_number()));
      ImGui::Separator();
      if (ImGui::MenuItem("Quit")) app.RequestQuit();
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z", false, doc.session().CanUndo() && !app.playing())) doc.Undo();
      if (ImGui::MenuItem("Redo", "Ctrl+Y", false, doc.session().CanRedo() && !app.playing())) doc.Redo();
      if (ImGui::MenuItem("Delete selected", "Del", false, doc.selection().has_value() && !app.playing())) {
        doc.Delete(*doc.selection());
      }
      if (ImGui::MenuItem("Focus selected", "F", false, doc.selection().has_value())) app.FocusCameraOnSelection();
      if (ImGui::MenuItem("Camera to scene camera")) app.ResetCameraToSceneCamera();
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Create", !app.playing())) {
      const Mat4 cam = app.camera().World();
      const Vec3 at = GetTranslation(cam) + Normalize(TransformDirection(cam, {0, 0, -1})) * 5.0;
      if (ImGui::MenuItem("Box")) doc.CreatePrimitive(scene::PrimitiveShape::Box, at, "Box");
      if (ImGui::MenuItem("Plane")) doc.CreatePrimitive(scene::PrimitiveShape::Plane, at, "Plane");
      if (ImGui::MenuItem("Cylinder")) doc.CreatePrimitive(scene::PrimitiveShape::Cylinder, at, "Cylinder");
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Play")) {
      if (ImGui::MenuItem("Play", "F5", false, !app.playing())) app.StartPlay();
      if (ImGui::MenuItem("Stop", "F5", false, app.playing())) app.StopPlay();
      ImGui::EndMenu();
    }
    const std::string status = app.status_line();
    const float w = ImGui::CalcTextSize(status.c_str()).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - w - 16);
    ImGui::TextColored(doc.dirty() ? kWarn : kDim, "%s", status.c_str());
    ImGui::EndMainMenuBar();
  }
  if (open_batch) ImGui::OpenPopup("Apply command batch");
  if (ImGui::BeginPopupModal("Apply command batch", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("Batches go through the same command service as sovereign-cli.");
    ImGui::TextDisabled("A batch is a dry run unless it sets \"dry_run\": false.");
    ImGui::SetNextItemWidth(520);
    ImGui::InputText("Path", batch_path, sizeof(batch_path));
    if (ImGui::Button("Apply")) {
      const auto r = doc.ApplyBatchFile(batch_path);
      LogInfo("editor", "batch {}: {} (revision {} -> {})", batch_path, cmd::ToString(r.status), r.revision_before, r.revision_after);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}

void Hierarchy(EditorApp& app) {
  static char filter[64] = "";
  ImGui::Begin("Hierarchy");
  auto& doc = app.doc();
  const auto& d = doc.document();
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##filter", "filter by name", filter, sizeof(filter));
  ImGui::TextDisabled("%zu entities", d.entities.size());
  ImGui::Separator();
  std::map<std::optional<Uuid>, std::vector<const scene::EntityData*>> children;
  for (const auto& e : d.entities) children[e.transform.parent].push_back(&e);
  for (auto& [parent, list] : children) {
    std::sort(list.begin(), list.end(), [](auto* a, auto* b) { return a->name < b->name; });
  }
  const std::string f = filter;
  std::function<void(const std::optional<Uuid>&)> draw = [&](const std::optional<Uuid>& parent) {
    auto it = children.find(parent);
    if (it == children.end()) return;
    for (const scene::EntityData* e : it->second) {
      const bool has_kids = children.count(e->id) > 0;
      const bool match = f.empty() || e->name.find(f) != std::string::npos;
      if (!f.empty() && !match && !has_kids) continue;
      ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
      if (!has_kids) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
      if (doc.selection() == e->id) flags |= ImGuiTreeNodeFlags_Selected;
      const std::string label = e->name + ComponentTags(*e) + "##" + e->id.ToString();
      const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) doc.Select(e->id);
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("%s", e->id.ToString().c_str());
      if (has_kids && open) {
        draw(e->id);
        ImGui::TreePop();
      }
    }
  };
  draw(std::nullopt);
  ImGui::End();
}

void Inspector(EditorApp& app) {
  ImGui::Begin("Inspector");
  auto& doc = app.doc();
  if (app.playing()) ImGui::TextColored(kWarn, "Playing: the authoring document is read-only. Stop play (F5) to edit.");
  if (!doc.selection()) {
    ImGui::TextDisabled("Select an entity in the Hierarchy, or click it in the Viewport.");
    ImGui::End();
    return;
  }
  const scene::EntityData* e = doc.document().Find(*doc.selection());
  if (!e) {
    ImGui::End();
    return;
  }
  InspectorState& st = app.inspector();
  if (st.entity != e->id || (!st.editing && st.loaded_world_generation != doc.authoring_world_generation())) {
    st.entity = e->id;
    st.transform = e->transform;
    st.euler = EulerDegreesFromQuat(e->transform.rotation);
    st.material = e->material.value_or(scene::MaterialComponent{});
    st.name = e->name;
    st.loaded_world_generation = doc.authoring_world_generation();
  }
  World& world = doc.authoring_world();
  const EntityHandle handle = world.Find(e->id);
  bool active = false;

  ImGui::BeginDisabled(app.playing());
  char name_buf[scene::SceneLimits::kMaxNameBytes + 1];
  std::snprintf(name_buf, sizeof(name_buf), "%s", st.name.c_str());
  if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) st.name = name_buf;
  active |= ImGui::IsItemActive();
  if (ImGui::IsItemDeactivatedAfterEdit() && st.name != e->name) doc.Rename(e->id, st.name);
  ImGui::TextDisabled("ID %s", e->id.ToString().c_str());
  ImGui::SameLine();
  if (ImGui::SmallButton("Copy ID")) ImGui::SetClipboardText(e->id.ToString().c_str());

  // --- Transform (labels, ranges and tooltips from reflection descriptors) ------------------
  const scene::ComponentDescriptor& tc = *scene::FindComponent("transform");
  ImGui::SeparatorText(std::string(tc.label).c_str());
  Tooltip(tc.description);
  {
    const scene::FieldDescriptor& parent_f = *scene::FindField(tc, "parent");
    const scene::EntityData* parent = e->transform.parent ? doc.document().Find(*e->transform.parent) : nullptr;
    if (ImGui::BeginCombo(std::string(parent_f.label).c_str(), parent ? parent->name.c_str() : "(scene root)")) {
      if (ImGui::Selectable("(scene root)", !parent)) {
        scene::TransformComponent t = e->transform;
        t.parent.reset();
        doc.SetTransform(e->id, t, true);
      }
      for (const auto& candidate : doc.document().entities) {
        if (candidate.id == e->id || IsDescendant(doc.document(), candidate.id, e->id)) continue;
        if (ImGui::Selectable((candidate.name + "##p" + candidate.id.ToString()).c_str(), parent && parent->id == candidate.id)) {
          scene::TransformComponent t = e->transform;
          t.parent = candidate.id;
          doc.SetTransform(e->id, t, true);  // local values kept; validation rejects cycles
        }
      }
      ImGui::EndCombo();
    }
    Tooltip(parent_f.description);
  }
  bool transform_changed = false, transform_commit = false;
  const auto drag3 = [&](const scene::FieldDescriptor& f, double* v, float speed, const char* fmt) {
    const double lo = f.min, hi = f.max;
    if (ImGui::DragScalarN(std::string(f.label).c_str(), ImGuiDataType_Double, v, 3, speed, &lo, &hi, fmt)) {
      transform_changed = true;
    }
    Tooltip(f.description);
    active |= ImGui::IsItemActive();
    transform_commit |= ImGui::IsItemDeactivatedAfterEdit();
  };
  double pos[3] = {st.transform.position.x, st.transform.position.y, st.transform.position.z};
  drag3(*scene::FindField(tc, "position"), pos, 0.02f, "%.3f m");
  double rot[3] = {st.euler.pitch, st.euler.yaw, st.euler.roll};
  {
    const scene::FieldDescriptor& rf = *scene::FindField(tc, "rotation");
    if (ImGui::DragScalarN("Rotation (pitch/yaw/roll)", ImGuiDataType_Double, rot, 3, 0.25f, nullptr, nullptr, "%.2f deg")) {
      transform_changed = true;
    }
    Tooltip(rf.description);
    active |= ImGui::IsItemActive();
    transform_commit |= ImGui::IsItemDeactivatedAfterEdit();
  }
  double scl[3] = {st.transform.scale.x, st.transform.scale.y, st.transform.scale.z};
  drag3(*scene::FindField(tc, "scale"), scl, 0.01f, "%.3f");
  if (transform_changed) {
    st.transform.position = {pos[0], pos[1], pos[2]};
    st.euler = {rot[0], rot[1], rot[2]};
    st.transform.rotation = QuatFromEulerDegrees(st.euler);
    st.transform.scale = {scl[0], scl[1], scl[2]};
    // Live preview on the authoring world only; the document changes on release.
    world.SetLocalTransform(handle, st.transform.position, st.transform.rotation, st.transform.scale);
  }
  if (transform_commit) {
    const auto r = doc.SetTransform(e->id, st.transform, false);
    if (!r.ok()) ShowResultErrors(r);
  }

  // --- Material ------------------------------------------------------------------------------
  if (e->material) {
    const scene::ComponentDescriptor& mc = scene::MaterialDescriptor();
    ImGui::SeparatorText(std::string(mc.label).c_str());
    Tooltip(mc.description);
    scene::EntityData scratch = *e;
    scratch.material = st.material;
    for (const auto& f : mc.fields) {
      bool changed = false;
      scene::FieldValue value = f.get(scratch);
      if (f.type == scene::FieldType::Color) {
        const Vec3 v = std::get<Vec3>(value);
        float c[3] = {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
        changed = ImGui::ColorEdit3(std::string(f.label).c_str(), c, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
        if (changed) value = Vec3{c[0], c[1], c[2]};
      } else if (f.type == scene::FieldType::Number) {
        double v = std::get<double>(value);
        const double lo = f.min, hi = f.max;
        changed = ImGui::DragScalar(std::string(f.label).c_str(), ImGuiDataType_Double, &v, 0.005f, &lo, &hi, "%.3f",
                                    ImGuiSliderFlags_AlwaysClamp);
        if (changed) value = v;
      }
      Tooltip(f.description);
      active |= ImGui::IsItemActive();
      if (changed) {
        f.set(scratch, value);
        st.material = *scratch.material;
        world.SetMaterial(handle, st.material);  // preview
      }
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        const auto r = doc.SetMaterialField(e->id, f, f.get(scratch));
        if (!r.ok()) ShowResultErrors(r);
      }
    }
  }

  // --- read-only components (no G1 command writes them) --------------------------------------
  for (const auto& c : scene::Components()) {
    if (c.key == "transform" || c.key == "material" || !c.has(*e)) continue;
    ImGui::SeparatorText(std::string(c.label).c_str());
    Tooltip(c.description);
    for (const auto& f : c.fields) {
      ImGui::TextDisabled("%s: %s  (%s)", std::string(f.label).c_str(), CompactJson(scene::FieldValueToJson(f, f.get(*e))).c_str(),
                          std::string(scene::ToString(f.access)).c_str());
      Tooltip(f.description);
    }
  }
  ImGui::Separator();
  if (ImGui::Button("Delete entity")) {
    const auto r = doc.Delete(e->id);
    if (!r.ok()) LogWarn("editor", "delete refused: {}", r.errors.empty() ? "" : r.errors[0].message);
  }
  ImGui::EndDisabled();
  st.editing = active;
  if (!doc.recent_results().empty() && doc.recent_results().front().status == cmd::ResultStatus::Rejected) {
    ImGui::Separator();
    ImGui::TextColored(kError, "Last edit rejected:");
    ShowResultErrors(doc.recent_results().front());
  }
  ImGui::End();
}

void Viewport(EditorApp& app) {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleVar();
  ViewportState& vp = app.viewport();
  // Toolbar.
  ImGui::SetCursorPos(ImVec2(8, ImGui::GetCursorPosY() + 4));
  if (app.playing()) {
    if (ImGui::Button("Stop (F5)")) app.StopPlay();
    ImGui::SameLine();
    ImGui::TextColored(kWarn, "PLAYING - runtime world; hold RMB to fly the play camera, Esc releases the mouse");
  } else {
    if (ImGui::Button("Play (F5)")) app.StartPlay();
    ImGui::SameLine();
    ImGui::TextDisabled("LMB select  |  hold RMB + WASD/QE fly (Shift faster)  |  F focus");
  }
  const auto& dev = app.renderer().device();
  ImGui::SameLine();
  ImGui::TextColored(dev.is_cpu_implementation ? kWarn : kOk, "| %s", dev.is_cpu_implementation ? "SOFTWARE-RENDERED (CPU Vulkan)" : "GPU");

  const ImVec2 avail = ImGui::GetContentRegionAvail();
  vp.desired_w = static_cast<std::uint32_t>(std::max(64.0f, avail.x));
  vp.desired_h = static_cast<std::uint32_t>(std::max(64.0f, avail.y));
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImTextureID tex = app.ui().ViewTexture(vp.view);
  if (tex) ImGui::Image(ImTextureRef(tex), ImVec2(static_cast<float>(vp.width), static_cast<float>(vp.height)));
  vp.hovered = ImGui::IsItemHovered();
  if (vp.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !app.playing()) {
    const ImVec2 m = ImGui::GetMousePos();
    const auto x = static_cast<std::uint32_t>(std::clamp(m.x - origin.x, 0.0f, static_cast<float>(vp.width - 1)));
    const auto y = static_cast<std::uint32_t>(std::clamp(m.y - origin.y, 0.0f, static_cast<float>(vp.height - 1)));
    vp.pending_click = std::array<std::uint32_t, 2>{x, y};
  }
  if (vp.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !vp.flying) {
    vp.flying = true;
    app.window().SetRelativeMouse(true);
  }
  if (vp.hovered && ImGui::GetIO().MouseWheel != 0.0f) {
    app.camera().speed_mps = std::clamp(app.camera().speed_mps * (ImGui::GetIO().MouseWheel > 0 ? 1.25 : 0.8), 0.5, 80.0);
  }
  // Stats overlay.
  const auto& fs = app.renderer().last_frame();
  ImGui::SetCursorScreenPos(ImVec2(origin.x + 8, origin.y + 6));
  ImGui::TextColored(ImVec4(0.9f, 0.85f, 0.75f, 0.9f), "%ux%u  %u instances  %llu tris  %.2f ms CPU  cam %.1f m/s", vp.width,
                     vp.height, fs.instances, static_cast<unsigned long long>(fs.triangles), fs.cpu_ms, app.camera().speed_mps);
  ImGui::End();
}

void History(EditorApp& app) {
  ImGui::Begin("History");
  auto& s = app.doc().session();
  ImGui::BeginDisabled(app.playing());
  if (ImGui::Button("Undo")) app.doc().Undo();
  ImGui::SameLine();
  if (ImGui::Button("Redo")) app.doc().Redo();
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::TextDisabled("revision %llu | %zu transaction(s), %zu not yet saved", static_cast<unsigned long long>(s.revision()),
                      s.history().size(), s.history().size() - s.persisted_count());
  if (ImGui::BeginTable("history", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("rev", ImGuiTableColumnFlags_WidthFixed, 70);
    ImGui::TableSetupColumn("kind", ImGuiTableColumnFlags_WidthFixed, 50);
    ImGui::TableSetupColumn("actor", ImGuiTableColumnFlags_WidthFixed, 170);
    ImGui::TableSetupColumn("label");
    ImGui::TableSetupColumn("state", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableHeadersRow();
    const auto next_undo = s.NextUndo();
    const auto& h = s.history();
    for (std::size_t i = h.size(); i-- > 0;) {
      const auto& r = h[i];
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%llu->%llu", static_cast<unsigned long long>(r.base_revision), static_cast<unsigned long long>(r.new_revision));
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(std::string(cmd::ToString(r.kind)).c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(r.actor.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(r.label.c_str());
      if (next_undo && *next_undo == r.commit_id) {
        ImGui::SameLine();
        ImGui::TextColored(kOk, "<- next undo");
      }
      ImGui::TableNextColumn();
      ImGui::TextColored(i < s.persisted_count() ? kDim : kWarn, i < s.persisted_count() ? "saved" : "unsaved");
    }
    ImGui::EndTable();
  }
  ImGui::End();
}

void Diagnostics(EditorApp& app) {
  ImGui::Begin("Diagnostics");
  auto& r = app.renderer();
  if (ImGui::BeginTabBar("diag")) {
    if (ImGui::BeginTabItem("Device")) {
      const auto& d = r.device();
      ImGui::TextColored(d.is_cpu_implementation ? kWarn : kOk, "Evidence class: %s", d.evidence_class.c_str());
      ImGui::Text("Device: %s [%s]", d.name.c_str(), d.device_type.c_str());
      ImGui::Text("Driver: %s", d.driver.c_str());
      ImGui::Text("Vulkan device API %s, loader %s, conformance %s", d.api_version.c_str(), d.instance_version.c_str(),
                  d.conformance_version.c_str());
      ImGui::Text("Swapchain: %s", d.swapchain.c_str());
      std::string feats;
      for (const auto& f : d.requested_features) feats += f + "  ";
      ImGui::TextWrapped("Requested features: %s", feats.c_str());
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Validation")) {
      const auto v = r.validation();
      ImGui::TextColored(v.errors ? kError : kOk, "Validation layer %s, synchronization validation %s: %u error(s), %u warning(s)",
                         v.layer_enabled ? "ON" : "OFF", v.synchronization_validation ? "ON" : "OFF", v.errors, v.warnings);
      for (const auto& m : v.messages) ImGui::TextWrapped("%s", m.c_str());
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Commands")) {
      for (const auto& note : app.doc().store().journal_status().notes) ImGui::TextColored(kWarn, "journal: %s", note.c_str());
      for (const auto& res : app.doc().recent_results()) {
        const bool bad = res.status == cmd::ResultStatus::Rejected;
        ImGui::TextColored(bad ? kError : kDim, "%s %s r%llu->r%llu %s", std::string(cmd::ToString(res.kind)).c_str(),
                           std::string(cmd::ToString(res.status)).c_str(), static_cast<unsigned long long>(res.revision_before),
                           static_cast<unsigned long long>(res.revision_after), res.actor.c_str());
        for (const auto& e : res.errors) ImGui::TextColored(kError, "    %s", e.Describe().c_str());
      }
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Performance")) {
      const auto& f = r.last_frame();
      const auto res = r.resources();
      ImGui::Text("frame %llu: %u draws, %u instances, %llu triangles, %.2f ms CPU (recording)", static_cast<unsigned long long>(f.frame),
                  f.draw_calls, f.instances, static_cast<unsigned long long>(f.triangles), f.cpu_ms);
      ImGui::Text("GPU objects: %lld buffers, %lld images, %lld views; %llu VMA allocations (%.1f MiB); %zu pending retirements",
                  static_cast<long long>(res.buffers), static_cast<long long>(res.images), static_cast<long long>(res.image_views),
                  static_cast<unsigned long long>(res.vma_allocations), static_cast<double>(res.vma_bytes) / (1024.0 * 1024.0),
                  res.pending_retirements);
      ImGui::TextDisabled("Timings on a CPU Vulkan implementation are not GPU performance measurements.");
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Log")) {
      for (const auto& line : app.log()) {
        ImGui::TextColored(line.level == LogLevel::Error ? kError : line.level == LogLevel::Warn ? kWarn : kDim, "%s",
                           line.text.c_str());
      }
      if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
}

}  // namespace sov::editor_app::panels
