# ADR-015: Build DXC from source on non-Windows hosts

Status: Accepted as a precaution. **Needs Matthew's review** (third-party rights question).
Date: 2026-10-16.

## Context

The handoff pins DXC for HLSL to SPIR-V. The first implementation downloaded Microsoft's prebuilt Linux
package for v1.8.2505.1 (`linux_dxc_2025_07_14.x86_64.tar.gz`, SHA-256 in the lock). The package's
own files disagree about its terms:

- `LICENSE-MS.txt` (a Microsoft software licence) says: "you may install and use any number of copies
  of the software, and **solely for use on Windows**."
- `ReleaseNotes.md` in the same archive says `LICENSE-MIT.txt` applies to `d3d12shader.h` and
  `LICENSE-LLVM.txt` applies to "all other files". (No `LICENSE-MIT.txt` is in the Linux archive.)

Using a binary on Linux under terms that may say "solely for use on Windows" is an unclear-rights case,
and the handoff says to stop and report for unclear third-party rights rather than guess.

## Decision

Avoid the question instead of resolving it on Matthew's behalf:

1. On Linux/macOS, the default compiler is DXC **built from the pinned open-source tree**: the same tag
   `v1.8.2505.1`, commit `b106a961d09221b3c5bdb37be45b679257da08b8` (verified after clone), under the
   University of Illinois/NCSA licence (`LICENSE.TXT`, `ThirdPartyNotices.txt`).
   `scripts/build_dxc_from_source.sh` builds only the `dxc` target into
   `build/tools/dxc-b106a961d092/`, which `cmake/Dependencies.cmake` picks up and checks against the lock.
2. The prebuilt Linux package stays available only behind `-DSOV_DXC_ALLOW_PREBUILT=ON`, with a
   configure-time warning. Windows uses the prebuilt Windows package (whose `solely for use on Windows`
   clause is then satisfied).
3. `-DSOV_DXC_EXECUTABLE=/path/to/dxc` overrides both (for a host-installed Vulkan SDK DXC).
4. The compiler actually used is written into every capture's provenance (`ShaderCompiler`).

DXC is a build tool only: its binaries are never shipped; the SPIR-V it emits is engine-owned output.

## Consequences

- First `dev-vulkan` configure on a Linux host needs a one-time DXC build (~16 min of compilation on
  the 4-vCPU recording host).
- Evidence earlier in this assignment was produced with the prebuilt package; all graphics evidence in
  the final report was regenerated with the source-built compiler.
- For Matthew: decide whether the prebuilt Linux package is acceptable (e.g. after reading Microsoft's
  intent for the Linux release). If yes, flip the default back; nothing else changes.
