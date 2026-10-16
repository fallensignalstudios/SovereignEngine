# CLI demonstration (2026-10-16T23:23:20Z)
Binaries: `bin` build of commit fcbc52cc6b0f55680a40c99ca5f41266d5ce2630 (clean). Paths below are repository-relative.

## 1. Version and validation (canonical scene)

### sovereign-cli --version

```
$ sovereign-cli --version
exit 0 (expected 0)
```

stdout:
```
sovereign-cli 0.1.0
commit: fcbc52cc6b0f55680a40c99ca5f41266d5ce2630 (clean)
dependency-lock: sha256:c2ad80e256bb24a21da9321f9c562a1a2cb1d2393845987f493e1e80cb5a1065
build: Debug | GNU 13.3.0 | Linux x86_64
contracts: sovereign.scene schema v1, sovereign.command_batch protocol v1, journal format v1
graphics: sovereign-cli never initializes a graphics device

```

### scene validate

```
$ sovereign-cli scene validate --scene content/authoring/first_light.scene.json
exit 0 (expected 0)
```

stdout:
```
valid: "First Light" scene_id=10000000-0000-4000-8000-000000000001 revision=0 entities=12

```

### scene validate --json

```
$ sovereign-cli scene validate --scene content/authoring/first_light.scene.json --json
exit 0 (expected 0)
```

stdout:
```
{
  "ok": true,
  "scene": "content/authoring/first_light.scene.json",
  "scene_id": "10000000-0000-4000-8000-000000000001",
  "name": "First Light",
  "revision": 0,
  "entity_count": 12,
  "errors": [],
  "warnings": [],
  "canonical": true,
  "file_sha256": "f6aa89c0b31c4fd31b4522adaaaf3fdd7366af08eb6fad4c086a8bc2f78b4ee3",
  "journal": {
    "present": true,
    "consistent": true,
    "notes": []
  }
}

```

## 2. The example batch is a dry run and changes nothing

### scene apply (example batch as shipped)

```
$ sovereign-cli scene apply --scene content/authoring/first_light.scene.json --commands examples/place_banner.command_batch.json
exit 0 (expected 0)
```

stdout:
```
{
  "protocol": "sovereign.command_result",
  "protocol_version": 1,
  "request_id": "10000000-0000-4000-8000-000000009001",
  "status": "dry_run",
  "delivery": "original",
  "kind": "edit",
  "actor": "caelis",
  "scene_id": "10000000-0000-4000-8000-000000000001",
  "revision_before": 0,
  "revision_after": 0,
  "commit_id": null,
  "target_commit": null,
  "request_hash": "b9c1625bbef03e28c01cd8ffebb293de9ec26a7080ab4cb360a85757426e3273",
  "operations": [
    {
      "index": 0,
      "op": "entity.create",
      "entity_id": "10000000-0000-4000-8000-000000000140",
      "status": "valid"
    },
    {
      "index": 1,
      "op": "entity.rename",
      "entity_id": "10000000-0000-4000-8000-000000000103",
      "status": "valid"
    }
  ],
  "errors": [],
  "warnings": ["dry run: nothing was written; committing against revision 0 would produce revision 1"],
  "diff": {
    "revision_before": 0,
    "revision_after": 0,
    "entities_created": [
      {
        "entity_id": "10000000-0000-4000-8000-000000000140",
        "name": "Test banner",
        "changes": [
          {
            "path": "name",
            "before": null,
            "after": "Test banner"
          },
          {
            "path": "components.Transform.position",
            "before": null,
            "after": [0.0, 6.0, -8.75]
          },
          {
            "path": "components.Transform.rotation_xyzw",
            "before": null,
            "after": [0.0, 0.0, 0.0, 1.0]
          },
          {
            "path": "components.Transform.scale",
            "before": null,
            "after": [4.0, 5.0, 0.05]
          },
          {
            "path": "components.Transform.parent_id",
            "before": null,
            "after": null
          },
          {
            "path": "components.PrimitiveMesh.primitive",
            "before": null,
            "after": "box"
          },
          {
            "path": "components.MaterialParameters.base_color_rgba",
            "before": null,
            "after": [0.24, 0.022, 0.016, 1.0]
          },
          {
            "path": "components.MaterialParameters.emissive_rgb",
            "before": null,
            "after": [0.0, 0.0, 0.0]
          }
        ]
      }
    ],
    "entities_deleted": [],
    "entities_modified": [
      {
        "entity_id": "10000000-0000-4000-8000-000000000103",
        "name": "First Light dais",
        "changes": [
          {
            "path": "name",
            "before": "Raised platform",
            "after": "First Light dais"
          }
        ]
      }
    ],
    "scene_fields": [],
    "summary": "1 created, 0 deleted, 1 modified, 0 scene field(s) changed"
  },
  "conflict": null
}

```

stderr:
```
dry run: nothing was written (scene stays at revision 0)
```

scene sha256 before `f6aa89c0b31c4fd31b4522adaaaf3fdd7366af08eb6fad4c086a8bc2f78b4ee3`, after `f6aa89c0b31c4fd31b4522adaaaf3fdd7366af08eb6fad4c086a8bc2f78b4ee3`

- [x] status is dry_run

- [x] revision_after == revision_before == 0 (the example's base_revision)

- [x] scene file byte-identical after the dry run

- [x] no lock/journal/backup file created by the dry run

- [x] dry run reports the diff it would make (1 created: Test banner; 1 modified: rename to First Light dais)

- [x] every operation is reported 'valid' (passed validation), none 'applied'

## 3. A new, explicitly committed request

### scene apply (dry_run: false)

```
$ sovereign-cli scene apply --scene docs/reports/evidence/cli/work/first_light.scene.json --commands docs/reports/evidence/cli/requests/place_banner.commit.command_batch.json
exit 0 (expected 0)
```

stdout:
```
{
  "protocol": "sovereign.command_result",
  "protocol_version": 1,
  "request_id": "0061ed85-ee00-4f76-86e2-375d447ec198",
  "status": "committed",
  "delivery": "original",
  "kind": "edit",
  "actor": "caelis",
  "scene_id": "10000000-0000-4000-8000-000000000001",
  "revision_before": 0,
  "revision_after": 1,
  "commit_id": "2ad228ec-a0e6-4857-a01e-be4b1ca182fa",
  "target_commit": null,
  "request_hash": "b9c1625bbef03e28c01cd8ffebb293de9ec26a7080ab4cb360a85757426e3273",
  "operations": [
    {
      "index": 0,
      "op": "entity.create",
      "entity_id": "10000000-0000-4000-8000-000000000140",
      "status": "applied"
    },
    {
      "index": 1,
      "op": "entity.rename",
      "entity_id": "10000000-0000-4000-8000-000000000103",
      "status": "applied"
    }
  ],
  "errors": [],
  "warnings": [],
  "diff": {
    "revision_before": 0,
    "revision_after": 1,
    "entities_created": [
      {
        "entity_id": "10000000-0000-4000-8000-000000000140",
        "name": "Test banner",
        "changes": [
          {
            "path": "name",
            "before": null,
            "after": "Test banner"
          },
          {
            "path": "components.Transform.position",
            "before": null,
            "after": [0.0, 6.0, -8.75]
          },
          {
            "path": "components.Transform.rotation_xyzw",
            "before": null,
            "after": [0.0, 0.0, 0.0, 1.0]
          },
          {
            "path": "components.Transform.scale",
            "before": null,
            "after": [4.0, 5.0, 0.05]
          },
          {
            "path": "components.Transform.parent_id",
            "before": null,
            "after": null
          },
          {
            "path": "components.PrimitiveMesh.primitive",
            "before": null,
            "after": "box"
          },
          {
            "path": "components.MaterialParameters.base_color_rgba",
            "before": null,
            "after": [0.24, 0.022, 0.016, 1.0]
          },
          {
            "path": "components.MaterialParameters.emissive_rgb",
            "before": null,
            "after": [0.0, 0.0, 0.0]
          }
        ]
      }
    ],
    "entities_deleted": [],
    "entities_modified": [
      {
        "entity_id": "10000000-0000-4000-8000-000000000103",
        "name": "First Light dais",
        "changes": [
          {
            "path": "name",
            "before": "Raised platform",
            "after": "First Light dais"
          }
        ]
      }
    ],
    "scene_fields": [],
    "summary": "1 created, 0 deleted, 1 modified, 0 scene field(s) changed"
  },
  "conflict": null
}

```

stderr:
```
committed: revision 0 -> 1 (commit 2ad228ec-a0e6-4857-a01e-be4b1ca182fa)
```

- [x] status committed, delivery original, revision 0 -> 1, every operation applied

### scene diff before/after (structural)

```
$ sovereign-cli scene diff --before docs/reports/evidence/cli/scenes/first_light.rev0.before.scene.json --after docs/reports/evidence/cli/scenes/first_light.rev1.after_commit.scene.json --json
exit 0 (expected 0)
```

stdout:
```
{
  "revision_before": 0,
  "revision_after": 1,
  "entities_created": [
    {
      "entity_id": "10000000-0000-4000-8000-000000000140",
      "name": "Test banner",
      "changes": [
        {
          "path": "name",
          "before": null,
          "after": "Test banner"
        },
        {
          "path": "components.Transform.position",
          "before": null,
          "after": [0.0, 6.0, -8.75]
        },
        {
          "path": "components.Transform.rotation_xyzw",
          "before": null,
          "after": [0.0, 0.0, 0.0, 1.0]
        },
        {
          "path": "components.Transform.scale",
          "before": null,
          "after": [4.0, 5.0, 0.05]
        },
        {
          "path": "components.Transform.parent_id",
          "before": null,
          "after": null
        },
        {
          "path": "components.PrimitiveMesh.primitive",
          "before": null,
          "after": "box"
        },
        {
          "path": "components.MaterialParameters.base_color_rgba",
          "before": null,
          "after": [0.24, 0.022, 0.016, 1.0]
        },
        {
          "path": "components.MaterialParameters.emissive_rgb",
          "before": null,
          "after": [0.0, 0.0, 0.0]
        }
      ]
    }
  ],
  "entities_deleted": [],
  "entities_modified": [
    {
      "entity_id": "10000000-0000-4000-8000-000000000103",
      "name": "First Light dais",
      "changes": [
        {
          "path": "name",
          "before": "Raised platform",
          "after": "First Light dais"
        }
      ]
    }
  ],
  "scene_fields": [],
  "summary": "1 created, 0 deleted, 1 modified, 0 scene field(s) changed"
}

```

Text diff of the scene file: `results/05_text.diff` (23 changed lines).

### scene validate (after commit)

```
$ sovereign-cli scene validate --scene docs/reports/evidence/cli/work/first_light.scene.json
exit 0 (expected 0)
```

stdout:
```
valid: "First Light" scene_id=10000000-0000-4000-8000-000000000001 revision=1 entities=13

```

## 4. Retry behaviour

### retry: identical request delivered again

```
$ sovereign-cli scene apply --scene docs/reports/evidence/cli/work/first_light.scene.json --commands docs/reports/evidence/cli/requests/place_banner.commit.command_batch.json
exit 0 (expected 0)
```

stdout:
```
{
  "protocol": "sovereign.command_result",
  "protocol_version": 1,
  "request_id": "0061ed85-ee00-4f76-86e2-375d447ec198",
  "status": "committed",
  "delivery": "replayed",
  "kind": "edit",
  "actor": "caelis",
  "scene_id": "10000000-0000-4000-8000-000000000001",
  "revision_before": 0,
  "revision_after": 1,
  "commit_id": "2ad228ec-a0e6-4857-a01e-be4b1ca182fa",
  "target_commit": null,
  "request_hash": "b9c1625bbef03e28c01cd8ffebb293de9ec26a7080ab4cb360a85757426e3273",
  "operations": [
    {
      "index": 0,
      "op": "entity.create",
      "entity_id": "10000000-0000-4000-8000-000000000140",
      "status": "applied"
    },
    {
      "index": 1,
      "op": "entity.rename",
      "entity_id": "10000000-0000-4000-8000-000000000103",
      "status": "applied"
    }
  ],
  "errors": [],
  "warnings": [],
  "diff": {
    "revision_before": 0,
    "revision_after": 1,
    "entities_created": [
      {
        "entity_id": "10000000-0000-4000-8000-000000000140",
        "name": "Test banner",
        "changes": [
          {
            "path": "name",
            "before": null,
            "after": "Test banner"
          },
          {
            "path": "components.Transform.position",
            "before": null,
            "after": [0.0, 6.0, -8.75]
          },
          {
            "path": "components.Transform.rotation_xyzw",
            "before": null,
            "after": [0.0, 0.0, 0.0, 1.0]
          },
          {
            "path": "components.Transform.scale",
            "before": null,
            "after": [4.0, 5.0, 0.05]
          },
          {
            "path": "components.Transform.parent_id",
            "before": null,
            "after": null
          },
          {
            "path": "components.PrimitiveMesh.primitive",
            "before": null,
            "after": "box"
          },
          {
            "path": "components.MaterialParameters.base_color_rgba",
            "before": null,
            "after": [0.24, 0.022, 0.016, 1.0]
          },
          {
            "path": "components.MaterialParameters.emissive_rgb",
            "before": null,
            "after": [0.0, 0.0, 0.0]
          }
        ]
      }
    ],
    "entities_deleted": [],
    "entities_modified": [
      {
        "entity_id": "10000000-0000-4000-8000-000000000103",
        "name": "First Light dais",
        "changes": [
          {
            "path": "name",
            "before": "Raised platform",
            "after": "First Light dais"
          }
        ]
      }
    ],
    "scene_fields": [],
    "summary": "1 created, 0 deleted, 1 modified, 0 scene field(s) changed"
  },
  "conflict": null
}

```

stderr:
```
duplicate delivery: returned the recorded result of commit "2ad228ec-a0e6-4857-a01e-be4b1ca182fa"; nothing was applied
```

- [x] replayed: recorded result returned, delivery replayed, same commit id, revision still 1

- [x] scene unchanged by the replay

### retry: same content, new request_id (transport field)

```
$ sovereign-cli scene apply --scene docs/reports/evidence/cli/work/first_light.scene.json --commands docs/reports/evidence/cli/requests/place_banner.commit.retry_new_request_id.command_batch.json
exit 0 (expected 0)
```

stdout:
```
{
  "protocol": "sovereign.command_result",
  "protocol_version": 1,
  "request_id": "0061ed85-ee00-4f76-86e2-375d447ec198",
  "status": "committed",
  "delivery": "replayed",
  "kind": "edit",
  "actor": "caelis",
  "scene_id": "10000000-0000-4000-8000-000000000001",
  "revision_before": 0,
  "revision_after": 1,
  "commit_id": "2ad228ec-a0e6-4857-a01e-be4b1ca182fa",
  "target_commit": null,
  "request_hash": "b9c1625bbef03e28c01cd8ffebb293de9ec26a7080ab4cb360a85757426e3273",
  "operations": [
    {
      "index": 0,
      "op": "entity.create",
      "entity_id": "10000000-0000-4000-8000-000000000140",
      "status": "applied"
    },
    {
      "index": 1,
      "op": "entity.rename",
      "entity_id": "10000000-0000-4000-8000-000000000103",
      "status": "applied"
    }
  ],
  "errors": [],
  "warnings": [],
  "diff": {
    "revision_before": 0,
    "revision_after": 1,
    "entities_created": [
      {
        "entity_id": "10000000-0000-4000-8000-000000000140",
        "name": "Test banner",
        "changes": [
          {
            "path": "name",
            "before": null,
            "after": "Test banner"
          },
          {
            "path": "components.Transform.position",
            "before": null,
            "after": [0.0, 6.0, -8.75]
          },
          {
            "path": "components.Transform.rotation_xyzw",
            "before": null,
            "after": [0.0, 0.0, 0.0, 1.0]
          },
          {
            "path": "components.Transform.scale",
            "before": null,
            "after": [4.0, 5.0, 0.05]
          },
          {
            "path": "components.Transform.parent_id",
            "before": null,
            "after": null
          },
          {
            "path": "components.PrimitiveMesh.primitive",
            "before": null,
            "after": "box"
          },
          {
            "path": "components.MaterialParameters.base_color_rgba",
            "before": null,
            "after": [0.24, 0.022, 0.016, 1.0]
          },
          {
            "path": "components.MaterialParameters.emissive_rgb",
            "before": null,
            "after": [0.0, 0.0, 0.0]
          }
        ]
      }
    ],
    "entities_deleted": [],
    "entities_modified": [
      {
        "entity_id": "10000000-0000-4000-8000-000000000103",
        "name": "First Light dais",
        "changes": [
          {
            "path": "name",
            "before": "Raised platform",
            "after": "First Light dais"
          }
        ]
      }
    ],
    "scene_fields": [],
    "summary": "1 created, 0 deleted, 1 modified, 0 scene field(s) changed"
  },
  "conflict": null
}

```

stderr:
```
duplicate delivery: returned the recorded result of commit "2ad228ec-a0e6-4857-a01e-be4b1ca182fa"; nothing was applied
```

- [x] still replayed; scene unchanged

### same idempotency key, changed content

```
$ sovereign-cli scene apply --scene docs/reports/evidence/cli/work/first_light.scene.json --commands docs/reports/evidence/cli/requests/place_banner.changed_content_same_key.command_batch.json
exit 1 (expected 1)
```

stdout:
```
{
  "protocol": "sovereign.command_result",
  "protocol_version": 1,
  "request_id": "c8924b12-5b36-430a-882b-68b1797a22da",
  "status": "rejected",
  "delivery": "original",
  "kind": "edit",
  "actor": "caelis",
  "scene_id": "10000000-0000-4000-8000-000000000001",
  "revision_before": 1,
  "revision_after": 1,
  "commit_id": null,
  "target_commit": null,
  "request_hash": "ce4bba72783771a51ea19237f0dc9799db355b1c7aecafe0af585ab9fed982dc",
  "operations": [],
  "errors": [
    {
      "code": "idempotency_conflict",
      "message": "idempotency key \"place-banner-commit-0001\" for actor \"caelis\" was already used by commit 2ad228ec-a0e6-4857-a01e-be4b1ca182fa with different request content (recorded hash b9c1625bbef03e28c01cd8ffebb293de9ec26a7080ab4cb360a85757426e3273, this request ce4bba72783771a51ea19237f0dc9799db355b1c7aecafe0af585ab9fed982dc). Use a new key for a new action.",
      "path": "/idempotency_key",
      "op_index": null,
      "entity_id": null
    }
  ],
  "warnings": [],
  "diff": null,
  "conflict": null
}

```

stderr:
```
rejected: idempotency_conflict at /idempotency_key: idempotency key "place-banner-commit-0001" for actor "caelis" was already used by commit 2ad228ec-a0e6-4857-a01e-be4b1ca182fa with different request content (recorded hash b9c1625bbef03e28c01cd8ffebb293de9ec26a7080ab4cb360a85757426e3273, this request ce4bba72783771a51ea19237f0dc9799db355b1c7aecafe0af585ab9fed982dc). Use a new key for a new action.
```

- [x] rejected with idempotency_conflict; scene unchanged

## 5. Stale revision and invalid batch

### new key, base_revision 0 while scene is at 1

```
$ sovereign-cli scene apply --scene docs/reports/evidence/cli/work/first_light.scene.json --commands docs/reports/evidence/cli/requests/place_banner.stale_base_revision.command_batch.json
exit 1 (expected 1)
```

stdout:
```
{
  "protocol": "sovereign.command_result",
  "protocol_version": 1,
  "request_id": "8d514c45-617b-4717-91f6-9870b935322d",
  "status": "rejected",
  "delivery": "original",
  "kind": "edit",
  "actor": "caelis",
  "scene_id": "10000000-0000-4000-8000-000000000001",
  "revision_before": 1,
  "revision_after": 1,
  "commit_id": null,
  "target_commit": null,
  "request_hash": "b9c1625bbef03e28c01cd8ffebb293de9ec26a7080ab4cb360a85757426e3273",
  "operations": [],
  "errors": [
    {
      "code": "stale_revision",
      "message": "base_revision 0 is stale; the scene is at revision 1. Re-read the scene and resubmit against the current revision.",
      "path": "/base_revision",
      "op_index": null,
      "entity_id": null
    }
  ],
  "warnings": [],
  "diff": null,
  "conflict": {
    "base_revision": 0,
    "current_revision": 1,
    "commits_since_base": [
      {
        "commit_id": "2ad228ec-a0e6-4857-a01e-be4b1ca182fa",
        "kind": "edit",
        "actor": "caelis",
        "base_revision": 0,
        "new_revision": 1,
        "label": "batch 0061ed85-ee00-4f76-86e2-375d447ec198 (2 operation(s))",
        "entities": ["10000000-0000-4000-8000-000000000103", "10000000-0000-4000-8000-000000000140"]
      }
    ],
    "history_complete": true
  }
}

```

stderr:
```
rejected: stale_revision at /base_revision: base_revision 0 is stale; the scene is at revision 1. Re-read the scene and resubmit against the current revision.
```

- [x] stale_revision with conflict details (current 1, one newer commit)

### valid op 0, invalid op 1 (unknown entity)

```
$ sovereign-cli scene apply --scene docs/reports/evidence/cli/work/first_light.scene.json --commands docs/reports/evidence/cli/requests/invalid_second_op_unknown_entity.command_batch.json
exit 1 (expected 1)
```

stdout:
```
{
  "protocol": "sovereign.command_result",
  "protocol_version": 1,
  "request_id": "d2f3c2e4-69d3-4a69-994b-59df0e1702d0",
  "status": "rejected",
  "delivery": "original",
  "kind": "edit",
  "actor": "caelis",
  "scene_id": "10000000-0000-4000-8000-000000000001",
  "revision_before": 1,
  "revision_after": 1,
  "commit_id": null,
  "target_commit": null,
  "request_hash": "857315b8100c52f5dbc30007449416bcaa532b1fe89379ade3c7c7388b51074c",
  "operations": [
    {
      "index": 0,
      "op": "entity.rename",
      "entity_id": "10000000-0000-4000-8000-000000000140",
      "status": "valid"
    },
    {
      "index": 1,
      "op": "entity.delete",
      "entity_id": "00000000-0000-4000-8000-0000000000ff",
      "status": "failed"
    }
  ],
  "errors": [
    {
      "code": "unknown_entity",
      "message": "entity 00000000-0000-4000-8000-0000000000ff does not exist",
      "path": "/operations/1",
      "op_index": 1,
      "entity_id": "00000000-0000-4000-8000-0000000000ff"
    }
  ],
  "warnings": [],
  "diff": null,
  "conflict": null
}

```

stderr:
```
rejected: unknown_entity at /operations/1 (operation 1): entity 00000000-0000-4000-8000-0000000000ff does not exist
```

- [x] whole batch rejected: op0 'valid' but not applied, op1 'failed', revision unchanged, file unchanged

## 6. Undo / redo (new revisions, content restored)

### scene undo --base-revision 1

```
$ sovereign-cli scene undo --scene docs/reports/evidence/cli/work/first_light.scene.json --base-revision 1 --actor cli:demo
exit 0 (expected 0)
```

stdout:
```
{
  "protocol": "sovereign.command_result",
  "protocol_version": 1,
  "request_id": null,
  "status": "committed",
  "delivery": "original",
  "kind": "undo",
  "actor": "cli:demo",
  "scene_id": "10000000-0000-4000-8000-000000000001",
  "revision_before": 1,
  "revision_after": 2,
  "commit_id": "cf3ada30-4de6-44a1-b82f-0e3408e378b6",
  "target_commit": "2ad228ec-a0e6-4857-a01e-be4b1ca182fa",
  "request_hash": null,
  "operations": [
    {
      "index": 0,
      "op": "entity.rename",
      "entity_id": "10000000-0000-4000-8000-000000000103",
      "status": "applied"
    },
    {
      "index": 1,
      "op": "entity.delete",
      "entity_id": "10000000-0000-4000-8000-000000000140",
      "status": "applied"
    }
  ],
  "errors": [],
  "warnings": [],
  "diff": {
    "revision_before": 1,
    "revision_after": 2,
    "entities_created": [],
    "entities_deleted": [
      {
        "entity_id": "10000000-0000-4000-8000-000000000140",
        "name": "Test banner",
        "changes": [
          {
            "path": "name",
            "before": "Test banner",
            "after": null
          },
          {
            "path": "components.Transform.position",
            "before": [0.0, 6.0, -8.75],
            "after": null
          },
          {
            "path": "components.Transform.rotation_xyzw",
            "before": [0.0, 0.0, 0.0, 1.0],
            "after": null
          },
          {
            "path": "components.Transform.scale",
            "before": [4.0, 5.0, 0.05],
            "after": null
          },
          {
            "path": "components.Transform.parent_id",
            "before": null,
            "after": null
          },
          {
            "path": "components.PrimitiveMesh.primitive",
            "before": "box",
            "after": null
          },
          {
            "path": "components.MaterialParameters.base_color_rgba",
            "before": [0.24, 0.022, 0.016, 1.0],
            "after": null
          },
          {
            "path": "components.MaterialParameters.emissive_rgb",
            "before": [0.0, 0.0, 0.0],
            "after": null
          }
        ]
      }
    ],
    "entities_modified": [
      {
        "entity_id": "10000000-0000-4000-8000-000000000103",
        "name": "Raised platform",
        "changes": [
          {
            "path": "name",
            "before": "First Light dais",
            "after": "Raised platform"
          }
        ]
      }
    ],
    "scene_fields": [],
    "summary": "0 created, 1 deleted, 1 modified, 0 scene field(s) changed"
  },
  "conflict": null
}

```

stderr:
```
committed: revision 1 -> 2 (commit cf3ada30-4de6-44a1-b82f-0e3408e378b6)
```

### inspect after undo

```
$ sovereign-cli scene inspect --scene docs/reports/evidence/cli/work/first_light.scene.json --json
exit 0 (expected 0)
```

stdout:
```
{
  "schema_version": 1,
  "scene_id": "10000000-0000-4000-8000-000000000001",
  "name": "First Light",
  "revision": 2,
  "active_camera_id": "10000000-0000-4000-8000-000000000201",
  "background_linear_rgb": [0.012, 0.014, 0.022],
  "entities": [
    {
      "id": "10000000-0000-4000-8000-000000000101",
      "name": "Hall floor",
      "components": {
        "Transform": {
          "position": [0.0, -0.15, 0.0],
          "rotation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "scale": [30.0, 0.3, 18.0],
          "parent_id": null
        },
        "PrimitiveMesh": {
          "primitive": "box"
        },
        "MaterialParameters": {
          "base_color_rgba": [0.055, 0.06, 0.07, 1.0],
          "emissive_rgb": [0.0, 0.0, 0.0]
        }
      }
    },
    {
      "id": "10000000-0000-4000-8000-000000000102",
      "name": "Central walkway",
      "components": {
        "Transform": {
          "position": [0.0, 0.025, 0.0],
          "rotation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "scale": [5.0, 0.05, 17.0],
          "parent_id": null
        },
        "PrimitiveMesh": {
          "primitive": "box"
        },
        "MaterialParameters": {
          "base_color_rgba": [0.18, 0.025, 0.02, 1.0],
          "emissive_rgb": [0.0, 0.0, 0.0]
        }
      }
    },
    {
      "id": "10000000-0000-4000-8000-000000000103",
      "name": "Raised platform",
      "components": {
        "Transform": {
          "position": [0.0, 0.5, -6.5],
          "rotation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "scale": [10.0, 1.0, 4.0],
          "parent_id": null
        },
        "PrimitiveMesh": {
          "primitive": "box"
        },
        "MaterialParameters": {
          "base_color_rgba": [0.23, 0.115, 0.045, 1.0],
          "emissive_rgb": [0.0, 0.0, 0.0]
        }
      }
    },
    {
      "id": "10000000-0000-4000-8000-000000000104",
      "name": "Rear wall",
      "components": {
        "Transform": {
          "position": [0.0, 5.0, -9.0],
          "rotation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "scale": [30.0, 10.0, 0.35],
          "parent_id": null
        },
        "PrimitiveMesh": {
          "primitive": "box"
        },
        "MaterialParameters": {
          "base_color_rgba": [0.04, 0.045, 0.055, 1.0],
          "emissive_rgb": [0.0, 0.0, 0.0]
        }
      }
    },
    {
      "id": "10000000-0000-4000-8000-000000000110",
      "name": "Column 1",
      "components": {
        "Transform": {
          "position": [-7.0, 3.5, -5.5],
          "rotation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.6, 7.0, 1.6],
          "parent_id": null
        },
        "PrimitiveMesh": {
          "primitive": "cylinder"
        },
        "MaterialParameters": {
          "base_color_rgba": [0.35, 0.16, 0.045, 1.0],
          "emissive_rgb": [0.0, 0.0, 0.0]
        }
      }
    },
    {
      "id": "10000000-0000-4000-8000-000000000111",
      "name": "Column 2",
      "components": {
        "Transform": {
          "position": [-7.0, 3.5, 0.0],
          "rotation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.6, 7.0, 1.6],
          "parent_id": null
        },
        "PrimitiveMesh": {
          "primitive": "cylinder"
        },
        "MaterialParameters": {
          "base_color_rgba": [0.35, 0.16, 0.045, 1.0],
          "emissive_rgb": [0.0, 0.0, 0.0]
        }
      }
    },
    {
      "id": "10000000-0000-4000-8000-000000000112",
      "name": "Column 3",
      "components": {
        "Transform": {
          "position": [-7.0, 3.5, 5.5],
          "rotation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.6, 7.0, 1.6],
          "parent_id": null
        },
        "PrimitiveMesh": {
          "primitive": "cylinder"
        },
        "MaterialParameters": {
          "base_color_rgba": [0.35, 0.16, 0.045, 1.0],
          "emissive_rgb": [0.0, 0.0, 0.0]
        }
      }
    },
    {
      "id": "10000000-0000-4000-8000-000000000113",
      "name": "Column 4",
      "components": {
        "Transform": {
          "position": [7.0, 3.5, -5.5],
          "rotation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.6, 7.0, 1.6],
          "parent_id": null
        },
        "PrimitiveMesh": {
          "primitive": "cylinder"
        },
        "MaterialParameters": {
          "base_color_rgba": [0.35, 0.16, 0.045, 1.0],
          "emissive_rgb": [0.0, 0.0, 0.0]
        }
      }
    },
    {
      "id": "10000000-0000-4000-8000-000000000114",
      "name": "Column 5",
      "components": {
        "Transform": {
          "position": [7.0, 3.5, 0.0],
          "rotation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.6, 7.0, 1.6],
          "parent_id": null
        },
        "PrimitiveMesh": {
          "primitive": "cylinder"
        },
        "MaterialParameters": {
          "base_color_rgba": [0.35, 0.16, 0.045, 1.0],
          "emissive_rgb": [0.0, 0.0, 0.0]
        }
      }
    },
    {
      "id": "10000000-0000-4000-8000-000000000115",
      "name": "Column 6",
      "components": {
        "Transform": {
          "position": [7.0, 3.5, 5.5],
          "rotation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.6, 7.0, 1.6],
          "parent_id": null
        },
        "PrimitiveMesh": {
          "primitive": "cylinder"
        },
        "MaterialParameters": {
          "base_color_rgba": [0.35, 0.16, 0.045, 1.0],
          "emissive_rgb": [0.0, 0.0, 0.0]
        }
      }
    },
    {
      "id": "10000000-0000-4000-8000-000000000130",
      "name": "Platform light strip",
      "components": {
        "Transform": {
          "position": [0.0, 1.05, -4.55],
          "rotation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "scale": [9.0, 0.08, 0.08],
          "parent_id": null
        },
        "PrimitiveMesh": {
          "primitive": "box"
        },
        "MaterialParameters": {
          "base_color_rgba": [0.35, 0.16, 0.03, 1.0],
          "emissive_rgb": [1.4, 0.45, 0.06]
        }
      }
    },
    {
      "id": "100
```

### scene redo --base-revision 2

```
$ sovereign-cli scene redo --scene docs/reports/evidence/cli/work/first_light.scene.json --base-revision 2 --actor cli:demo
exit 0 (expected 0)
```

stdout:
```
{
  "protocol": "sovereign.command_result",
  "protocol_version": 1,
  "request_id": null,
  "status": "committed",
  "delivery": "original",
  "kind": "redo",
  "actor": "cli:demo",
  "scene_id": "10000000-0000-4000-8000-000000000001",
  "revision_before": 2,
  "revision_after": 3,
  "commit_id": "4787cd00-7bc2-4c3d-9471-d80d6aa0a4f9",
  "target_commit": "2ad228ec-a0e6-4857-a01e-be4b1ca182fa",
  "request_hash": null,
  "operations": [
    {
      "index": 0,
      "op": "entity.create",
      "entity_id": "10000000-0000-4000-8000-000000000140",
      "status": "applied"
    },
    {
      "index": 1,
      "op": "entity.rename",
      "entity_id": "10000000-0000-4000-8000-000000000103",
      "status": "applied"
    }
  ],
  "errors": [],
  "warnings": [],
  "diff": {
    "revision_before": 2,
    "revision_after": 3,
    "entities_created": [
      {
        "entity_id": "10000000-0000-4000-8000-000000000140",
        "name": "Test banner",
        "changes": [
          {
            "path": "name",
            "before": null,
            "after": "Test banner"
          },
          {
            "path": "components.Transform.position",
            "before": null,
            "after": [0.0, 6.0, -8.75]
          },
          {
            "path": "components.Transform.rotation_xyzw",
            "before": null,
            "after": [0.0, 0.0, 0.0, 1.0]
          },
          {
            "path": "components.Transform.scale",
            "before": null,
            "after": [4.0, 5.0, 0.05]
          },
          {
            "path": "components.Transform.parent_id",
            "before": null,
            "after": null
          },
          {
            "path": "components.PrimitiveMesh.primitive",
            "before": null,
            "after": "box"
          },
          {
            "path": "components.MaterialParameters.base_color_rgba",
            "before": null,
            "after": [0.24, 0.022, 0.016, 1.0]
          },
          {
            "path": "components.MaterialParameters.emissive_rgb",
            "before": null,
            "after": [0.0, 0.0, 0.0]
          }
        ]
      }
    ],
    "entities_deleted": [],
    "entities_modified": [
      {
        "entity_id": "10000000-0000-4000-8000-000000000103",
        "name": "First Light dais",
        "changes": [
          {
            "path": "name",
            "before": "Raised platform",
            "after": "First Light dais"
          }
        ]
      }
    ],
    "scene_fields": [],
    "summary": "1 created, 0 deleted, 1 modified, 0 scene field(s) changed"
  },
  "conflict": null
}

```

stderr:
```
committed: revision 2 -> 3 (commit 4787cd00-7bc2-4c3d-9471-d80d6aa0a4f9)
```

first_light.rev0.before.scene.json revision 0 content-without-revision 35b86bcd3080246c710e21d8d2d5406b4e0c3a56244467243f79e4a1a7db27e5
first_light.rev1.after_commit.scene.json revision 1 content-without-revision acd8aa6f12783e1a01c5489336214ca149293486732f8cac617053a7be8dd78f
first_light.rev2.after_undo.scene.json revision 2 content-without-revision 35b86bcd3080246c710e21d8d2d5406b4e0c3a56244467243f79e4a1a7db27e5
first_light.rev3.after_redo.scene.json revision 3 content-without-revision acd8aa6f12783e1a01c5489336214ca149293486732f8cac617053a7be8dd78f

- [x] undo (rev 2) content == rev 0 content; redo (rev 3) content == rev 1 content; revisions 0,1,2,3

### scene history --json

```
$ sovereign-cli scene history --scene docs/reports/evidence/cli/work/first_light.scene.json --json
exit 0 (expected 0)
```

stdout:
```
{
  "revision": 3,
  "next_undo": "2ad228ec-a0e6-4857-a01e-be4b1ca182fa",
  "next_redo": null,
  "journal": {
    "present": true,
    "consistent": true,
    "notes": []
  },
  "transactions": [
    {
      "commit_id": "2ad228ec-a0e6-4857-a01e-be4b1ca182fa",
      "kind": "edit",
      "actor": "caelis",
      "base_revision": 0,
      "new_revision": 1,
      "label": "batch 0061ed85-ee00-4f76-86e2-375d447ec198 (2 operation(s))",
      "idempotency_key": "place-banner-commit-0001",
      "operations": 2
    },
    {
      "commit_id": "cf3ada30-4de6-44a1-b82f-0e3408e378b6",
      "kind": "undo",
      "actor": "cli:demo",
      "base_revision": 1,
      "new_revision": 2,
      "label": "Undo: batch 0061ed85-ee00-4f76-86e2-375d447ec198 (2 operation(s))",
      "idempotency_key": "",
      "operations": 2
    },
    {
      "commit_id": "4787cd00-7bc2-4c3d-9471-d80d6aa0a4f9",
      "kind": "redo",
      "actor": "cli:demo",
      "base_revision": 2,
      "new_revision": 3,
      "label": "Redo: batch 0061ed85-ee00-4f76-86e2-375d447ec198 (2 operation(s))",
      "idempotency_key": "",
      "operations": 2
    }
  ]
}

```

### undo with an outdated base revision

```
$ sovereign-cli scene undo --scene docs/reports/evidence/cli/work/first_light.scene.json --base-revision 1
exit 1 (expected 1)
```

stdout:
```
{
  "protocol": "sovereign.command_result",
  "protocol_version": 1,
  "request_id": null,
  "status": "rejected",
  "delivery": "original",
  "kind": "undo",
  "actor": "cli",
  "scene_id": "10000000-0000-4000-8000-000000000001",
  "revision_before": 3,
  "revision_after": 3,
  "commit_id": null,
  "target_commit": "2ad228ec-a0e6-4857-a01e-be4b1ca182fa",
  "request_hash": null,
  "operations": [],
  "errors": [
    {
      "code": "stale_revision",
      "message": "base_revision 1 is stale; the scene is at revision 3. Re-read the scene and resubmit against the current revision.",
      "path": "/base_revision",
      "op_index": null,
      "entity_id": null
    }
  ],
  "warnings": [],
  "diff": null,
  "conflict": {
    "base_revision": 1,
    "current_revision": 3,
    "commits_since_base": [
      {
        "commit_id": "cf3ada30-4de6-44a1-b82f-0e3408e378b6",
        "kind": "undo",
        "actor": "cli:demo",
        "base_revision": 1,
        "new_revision": 2,
        "label": "Undo: batch 0061ed85-ee00-4f76-86e2-375d447ec198 (2 operation(s))",
        "entities": ["10000000-0000-4000-8000-000000000103", "10000000-0000-4000-8000-000000000140"]
      },
      {
        "commit_id": "4787cd00-7bc2-4c3d-9471-d80d6aa0a4f9",
        "kind": "redo",
        "actor": "cli:demo",
        "base_revision": 2,
        "new_revision": 3,
        "label": "Redo: batch 0061ed85-ee00-4f76-86e2-375d447ec198 (2 operation(s))",
        "entities": ["10000000-0000-4000-8000-000000000103", "10000000-0000-4000-8000-000000000140"]
      }
    ],
    "history_complete": true
  }
}

```

stderr:
```
rejected: stale_revision at /base_revision: base_revision 1 is stale; the scene is at revision 3. Re-read the scene and resubmit against the current revision.
```

## 7. Writer lock: offline write refused while another writer holds the scene

Here the lock is held by util-linux `flock` on the same lock file (same flock(2) lock the editor takes). The real-editor variant is `vulkan.editor_real_x11_input` (evidence/gui_real_input).

### committing apply while lock is held (error envelope, no batch evaluated)

```
$ sovereign-cli scene apply --scene docs/reports/evidence/cli/work/first_light.scene.json --commands docs/reports/evidence/cli/requests/place_banner.stale_base_revision.command_batch.json
exit 3 (expected 3)
```

stdout:
```
{
  "ok": false,
  "error": {
    "code": "lock_unavailable",
    "message": "scene writer lock docs/reports/evidence/cli/work/first_light.scene.json.lock is held by another writer",
    "path": null,
    "op_index": null,
    "entity_id": null
  }
}

```

### read-only validate while lock is held

```
$ sovereign-cli scene validate --scene docs/reports/evidence/cli/work/first_light.scene.json
exit 0 (expected 0)
```

stdout:
```
valid: "First Light" scene_id=10000000-0000-4000-8000-000000000001 revision=3 entities=13

```

- [x] scene unchanged while locked

## 8. Every scene the CLI wrote and every request above, against the owner's schemas

Independent validator (python jsonschema + the packet checker's semantic rules), not the engine.

### check_contracts.py on the demo's scenes and requests

```
$ python3 -I scripts/check_contracts.py docs/reports/evidence/cli/scenes docs/reports/evidence/cli/requests
exit 0 (expected 0)
```

stdout:
```
ok   scene  docs/reports/evidence/cli/scenes/first_light.rev0.before.scene.json
ok   scene  docs/reports/evidence/cli/scenes/first_light.rev1.after_commit.scene.json
ok   scene  docs/reports/evidence/cli/scenes/first_light.rev2.after_undo.scene.json
ok   scene  docs/reports/evidence/cli/scenes/first_light.rev3.after_redo.scene.json
ok   batch  docs/reports/evidence/cli/requests/invalid_second_op_unknown_entity.command_batch.json
ok   batch  docs/reports/evidence/cli/requests/place_banner.changed_content_same_key.command_batch.json
ok   batch  docs/reports/evidence/cli/requests/place_banner.command_batch.json
ok   batch  docs/reports/evidence/cli/requests/place_banner.commit.command_batch.json
ok   batch  docs/reports/evidence/cli/requests/place_banner.commit.retry_new_request_id.command_batch.json
ok   batch  docs/reports/evidence/cli/requests/place_banner.stale_base_revision.command_batch.json
10 file(s), 0 failure(s)

```

Journal copied to `scenes/first_light.journal.jsonl` (7 lines).

## Summary

Unexpected exit codes or failed checks: **0**

