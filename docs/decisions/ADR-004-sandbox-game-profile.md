# ADR-004: First game profile is an original sandbox

Status: Accepted. Date: 2026-10-16.

`games/sandbox` is the only game module: free camera flight driven by the input action map and one
transient "Play Probe" entity that exists only in the play world (it proves play-world isolation). No
production game, rules, assets or names are used. The runtime and the editor's play mode load the
same module through `runtime::GameModule`.
