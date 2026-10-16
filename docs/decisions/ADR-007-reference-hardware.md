# ADR-007: Reference hardware and finalized budgets

Status: **Pending.** Date: 2026-10-16.

The TDD default is "record the first available qualified machine". No qualified machine was available:
this host has no physical GPU (ADR-001). Consequently:

- No reference machine is recorded and no frame-time baseline exists. The software-rendered frame
  times seen on lavapipe are not a performance result and are not reported as one (ADR-013).
- The §21.2 budgets remain provisional. G1 First Light content is within the G1 content limits by
  construction (`docs/reports/G0_G1_REPORT.md` lists instance and triangle counts).

To resolve: run `docs/reports/GPU_TEST_PROCEDURE.md` on Matthew's chosen machine, record its
inventory here, and only then collect p50/p95/p99 timings per TDD §21.3.
