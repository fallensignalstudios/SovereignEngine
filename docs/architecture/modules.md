# Modules and dependency direction

Each module is a static library `sov::<name>` with public headers under `include/sov/<name>/`.
Dependencies point downward only; CMake link lines are the enforcement and static checks guard the
two boundaries that matter most for G1.

```
                 sovereign-editor            sovereign-runtime         sovereign-cli
                 /     |       \                 |      \                  |
   editor/application  |    imgui            games/sandbox \               |
          |       editor/authoring                 |        \              |
          |        /        \                      |         \             |
     renderer   commands   runtime <---------------+          \            |
      |    \        |        |                                 \           |
 rhi/vulkan platform|        |                          renderer, platform |
   |  (SDL3)  \     |        |                                             |
 volk/VMA      \    +--> world (EnTT private) <----------------------------+
                \          |
                 +-------> core (nlohmann/json)
```

| Module | Depends on | Responsibility |
|---|---|---|
| `core` | nlohmann/json | `Result`/`Error`, UUID, SHA-256, logging, strict JSON + canonical writer, durable file I/O (atomic replace, durable append, writer lock), math, PNG encoder, build info |
| `world` | core; EnTT (private) | `SceneDocument` (authoritative authored data), reflection descriptors, strict reader/writer, validation, diff, content hash; `World` (EnTT runtime storage behind a handle API), transform propagation, render snapshot extraction, primitive geometry |
| `commands` | world, core | operations, batch parsing, staging, `SceneSession` (revisions, idempotency, undo/redo), `SceneStore` (journal, lock, atomic commit, recovery) |
| `runtime` | world, core | `Simulation` (60 Hz fixed step, max 4 catch-up steps), `GameModule` seam, `ActionState` |
| `platform` | SDL3, runtime, core | video init, `Window`, event pump, input action mapping |
| `rhi_vulkan` | volk, VMA, core | instance/device selection, validation messenger, timeline-semaphore submission, deferred destruction, buffers/images/barriers, swapchain |
| `renderer` | world, core, rhi_vulkan (public), platform (private) | R0 forward renderer: views, HDR + tone map, pick IDs, selection outline, readback/capture with provenance. Public API exposes no Vulkan types; `vulkan_native.h` is the opt-in seam for the editor's ImGui backend |
| `editor_authoring` | commands, runtime, world, core | `EditorDocument`: open (holds writer lock), select, edit, undo/redo, save, play/stop. No UI or graphics |
| `sovereign-editor` | editor_authoring, renderer, platform, imgui, game_sandbox | tool UI (panels), editor camera, viewport, automation runner |
| `game_sandbox` | runtime | original sandbox game module (camera flight, transient probe entity) |
| `sovereign-runtime` | game_sandbox, renderer, platform, runtime, world, core | standalone player. **No editor, ImGui or command code** |
| `sovereign-cli` | commands, world, core | offline validation and command batches. **No graphics** |

## Boundaries enforced by tests

- `static.runtime_has_no_editor_code`: `sovereign-runtime` contains no `ImGui`, `sov::editor`,
  `EditorDocument`, `sov::cmd::SceneSession` or `sov::cmd::SceneStore` symbols.
- `static.headless_sovereign-cli_has_no_graphics`, `static.headless_sovereign-tests_has_no_graphics`:
  the `dev-headless` binaries contain no `SDL_`, `vkCreate`, `volk` or `ImGui` symbols and link no
  Vulkan, SDL or X11 library; `dev-headless` configures without fetching any graphics dependency.
- `static.no_unreal_dependency`: no Unreal headers, module rules, `.uplugin/.uproject`, or Unreal
  include paths anywhere in the tree.

## Differences from the TDD §5.1 proposed layout

- `engine/rhi/null/` was **not** created. Headless targets do not link a renderer at all, which is a
  stronger guarantee than a Null backend and avoids a renderer that "passes" without drawing
  (TDD §1.3). ADR-016 places the G1 backend boundary at the renderer's public API.
- `engine/commands/` is a module of its own (the TDD folds transactions into the editor/world
  description); this is what lets the CLI and editor share one implementation.
- `engine/assets`, `physics`, `animation`, `navigation`, `audio`, `gameplay` are G2+ and not created.
