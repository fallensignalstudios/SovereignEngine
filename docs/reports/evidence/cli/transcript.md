# CLI demonstration (2026-10-16T21:41:11Z)
Binaries: `bin` build of commit 384eaea603d398b5f650cacd63644bfa94b10bd2 (clean). Paths below are repository-relative.

## 1. Version and validation (canonical scene)

### sovereign-cli --version

```
$ sovereign-cli --version
exit 0 (expected 0)
```

stdout:
```
sovereign-cli 0.1.0
commit: 384eaea603d398b5f650cacd63644bfa94b10bd2 (clean)
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
valid: "First Light" scene_id=8d4684d6-93cd-5d04-a868-b4d839bafda7 revision=1 entities=96

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
  "scene_id": "8d4684d6-93cd-5d04-a868-b4d839bafda7",
  "name": "First Light",
  "revision": 1,
  "entity_count": 96,
  "errors": [],
  "warnings": [],
  "canonical": true,
  "file_sha256": "f003be2e2a87bd21c861a009821a1874e9d47184a2c4757fbf497a2a5b028bd2",
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
  "request_id": "2789830a-afad-55eb-a024-8a0abeeedf81",
  "status": "dry_run",
  "delivery": "original",
  "kind": "edit",
  "actor": "agent:place-banner-example",
  "scene_id": "8d4684d6-93cd-5d04-a868-b4d839bafda7",
  "revision_before": 1,
  "revision_after": 1,
  "commit_id": null,
  "target_commit": null,
  "request_hash": "0aea9cc63e2f84929548717420871acf56c7af24020668c864f7ab27be7ffacf",
  "operations": [
    {
      "index": 0,
      "op": "entity.create",
      "entity_id": "973060cd-d005-51f6-9b5a-265a9ac9031e",
      "status": "valid"
    },
    {
      "index": 1,
      "op": "entity.create",
      "entity_id": "6b45d759-a0c1-5147-9db2-6e7bab9f474f",
      "status": "valid"
    },
    {
      "index": 2,
      "op": "material.set",
      "entity_id": "4b64139c-ec7c-5014-944d-69cb98f1a8d9",
      "status": "valid"
    }
  ],
  "errors": [],
  "warnings": ["dry run: nothing was written; committing against revision 1 would produce revision 2"],
  "diff": {
    "revision_before": 1,
    "revision_after": 1,
    "entities_created": [
      {
        "entity_id": "6b45d759-a0c1-5147-9db2-6e7bab9f474f",
        "name": "Banner Rod (West Wall, Dais)",
        "changes": [
          {
            "path": "name",
            "before": null,
            "after": "Banner Rod (West Wall, Dais)"
          },
          {
            "path": "components.transform.parent",
            "before": null,
            "after": null
          },
          {
            "path": "components.transform.position",
            "before": null,
            "after": [-8.92, 8.75, -11.0]
          },
          {
            "path": "components.transform.rotation",
            "before": null,
            "after": [0.0, 0.0, 0.0, 1.0]
          },
          {
            "path": "components.transform.scale",
            "before": null,
            "after": [0.12, 0.12, 2.7]
          },
          {
            "path": "components.primitive_mesh.shape",
            "before": null,
            "after": "box"
          },
          {
            "path": "components.material.base_color",
            "before": null,
            "after": [0.48, 0.29, 0.12]
          },
          {
            "path": "components.material.roughness",
            "before": null,
            "after": 0.32
          },
          {
            "path": "components.material.metallic",
            "before": null,
            "after": 1.0
          },
          {
            "path": "components.material.emissive",
            "before": null,
            "after": [0.0, 0.0, 0.0]
          },
          {
            "path": "components.material.emissive_strength",
            "before": null,
            "after": 0.0
          }
        ]
      },
      {
        "entity_id": "973060cd-d005-51f6-9b5a-265a9ac9031e",
        "name": "Banner (West Wall, Dais)",
        "changes": [
          {
            "path": "name",
            "before": null,
            "after": "Banner (West Wall, Dais)"
          },
          {
            "path": "components.transform.parent",
            "before": null,
            "after": null
          },
          {
            "path": "components.transform.position",
            "before": null,
            "after": [-8.96, 5.4, -11.0]
          },
          {
            "path": "components.transform.rotation",
            "before": null,
            "after": [0.0, 0.0, 0.0, 1.0]
          },
          {
            "path": "components.transform.scale",
            "before": null,
            "after": [0.06, 6.4, 2.2]
          },
          {
            "path": "components.primitive_mesh.shape",
            "before": null,
            "after": "box"
          },
          {
            "path": "components.material.base_color",
            "before": null,
            "after": [0.3, 0.022, 0.026]
          },
          {
            "path": "components.material.roughness",
            "before": null,
            "after": 0.55
          },
          {
            "path": "components.material.metallic",
            "before": null,
            "after": 0.0
          },
          {
            "path": "components.material.emissive",
            "before": null,
            "after": [0.0, 0.0, 0.0]
          },
          {
            "path": "components.material.emissive_strength",
            "before": null,
            "after": 0.0
          }
        ]
      }
    ],
    "entities_deleted": [],
    "entities_modified": [
      {
        "entity_id": "4b64139c-ec7c-5014-944d-69cb98f1a8d9",
        "name": "Monolith Seam",
        "changes": [
          {
            "path": "components.material.emissive_strength",
            "before": 2.2,
            "after": 3.2
          }
        ]
      }
    ],
    "scene_fields": [],
    "summary": "2 created, 0 deleted, 1 modified, 0 scene field(s) changed"
  },
  "conflict": null
}

```

stderr:
```
dry run: nothing was written (scene stays at revision 1)
```

scene sha256 before `f003be2e2a87bd21c861a009821a1874e9d47184a2c4757fbf497a2a5b028bd2`, after `f003be2e2a87bd21c861a009821a1874e9d47184a2c4757fbf497a2a5b028bd2`

- [x] status is dry_run

- [x] revision_after == revision_before == 1

- [x] scene file byte-identical after the dry run

- [x] no lock/journal/backup file created by the dry run

- [x] dry run reports the diff it would make (2 created, 1 modified)

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
  "request_id": "d6b2c6fb-c818-4ce7-b638-fa37e41422a0",
  "status": "committed",
  "delivery": "original",
  "kind": "edit",
  "actor": "agent:place-banner-example",
  "scene_id": "8d4684d6-93cd-5d04-a868-b4d839bafda7",
  "revision_before": 1,
  "revision_after": 2,
  "commit_id": "abb4ada7-22b7-40cc-af9c-24ada61e7a4a",
  "target_commit": null,
  "request_hash": "0aea9cc63e2f84929548717420871acf56c7af24020668c864f7ab27be7ffacf",
  "operations": [
    {
      "index": 0,
      "op": "entity.create",
      "entity_id": "973060cd-d005-51f6-9b5a-265a9ac9031e",
      "status": "applied"
    },
    {
      "index": 1,
      "op": "entity.create",
      "entity_id": "6b45d759-a0c1-5147-9db2-6e7bab9f474f",
      "status": "applied"
    },
    {
      "index": 2,
      "op": "material.set",
      "entity_id": "4b64139c-ec7c-5014-944d-69cb98f1a8d9",
      "status": "applied"
    }
  ],
  "errors": [],
  "warnings": [],
  "diff": {
    "revision_before": 1,
    "revision_after": 2,
    "entities_created": [
      {
        "entity_id": "6b45d759-a0c1-5147-9db2-6e7bab9f474f",
        "name": "Banner Rod (West Wall, Dais)",
        "changes": [
          {
            "path": "name",
            "before": null,
            "after": "Banner Rod (West Wall, Dais)"
          },
          {
            "path": "components.transform.parent",
            "before": null,
            "after": null
          },
          {
            "path": "components.transform.position",
            "before": null,
            "after": [-8.92, 8.75, -11.0]
          },
          {
            "path": "components.transform.rotation",
            "before": null,
            "after": [0.0, 0.0, 0.0, 1.0]
          },
          {
            "path": "components.transform.scale",
            "before": null,
            "after": [0.12, 0.12, 2.7]
          },
          {
            "path": "components.primitive_mesh.shape",
            "before": null,
            "after": "box"
          },
          {
            "path": "components.material.base_color",
            "before": null,
            "after": [0.48, 0.29, 0.12]
          },
          {
            "path": "components.material.roughness",
            "before": null,
            "after": 0.32
          },
          {
            "path": "components.material.metallic",
            "before": null,
            "after": 1.0
          },
          {
            "path": "components.material.emissive",
            "before": null,
            "after": [0.0, 0.0, 0.0]
          },
          {
            "path": "components.material.emissive_strength",
            "before": null,
            "after": 0.0
          }
        ]
      },
      {
        "entity_id": "973060cd-d005-51f6-9b5a-265a9ac9031e",
        "name": "Banner (West Wall, Dais)",
        "changes": [
          {
            "path": "name",
            "before": null,
            "after": "Banner (West Wall, Dais)"
          },
          {
            "path": "components.transform.parent",
            "before": null,
            "after": null
          },
          {
            "path": "components.transform.position",
            "before": null,
            "after": [-8.96, 5.4, -11.0]
          },
          {
            "path": "components.transform.rotation",
            "before": null,
            "after": [0.0, 0.0, 0.0, 1.0]
          },
          {
            "path": "components.transform.scale",
            "before": null,
            "after": [0.06, 6.4, 2.2]
          },
          {
            "path": "components.primitive_mesh.shape",
            "before": null,
            "after": "box"
          },
          {
            "path": "components.material.base_color",
            "before": null,
            "after": [0.3, 0.022, 0.026]
          },
          {
            "path": "components.material.roughness",
            "before": null,
            "after": 0.55
          },
          {
            "path": "components.material.metallic",
            "before": null,
            "after": 0.0
          },
          {
            "path": "components.material.emissive",
            "before": null,
            "after": [0.0, 0.0, 0.0]
          },
          {
            "path": "components.material.emissive_strength",
            "before": null,
            "after": 0.0
          }
        ]
      }
    ],
    "entities_deleted": [],
    "entities_modified": [
      {
        "entity_id": "4b64139c-ec7c-5014-944d-69cb98f1a8d9",
        "name": "Monolith Seam",
        "changes": [
          {
            "path": "components.material.emissive_strength",
            "before": 2.2,
            "after": 3.2
          }
        ]
      }
    ],
    "scene_fields": [],
    "summary": "2 created, 0 deleted, 1 modified, 0 scene field(s) changed"
  },
  "conflict": null
}

```

stderr:
```
committed: revision 1 -> 2 (commit abb4ada7-22b7-40cc-af9c-24ada61e7a4a)
```

- [x] status committed, delivery original, revision 1 -> 2, every operation applied

### scene diff before/after (structural)

```
$ sovereign-cli scene diff --before docs/reports/evidence/cli/scenes/first_light.rev1.before.scene.json --after docs/reports/evidence/cli/scenes/first_light.rev2.after_commit.scene.json --json
exit 0 (expected 0)
```

stdout:
```
{
  "revision_before": 1,
  "revision_after": 2,
  "entities_created": [
    {
      "entity_id": "6b45d759-a0c1-5147-9db2-6e7bab9f474f",
      "name": "Banner Rod (West Wall, Dais)",
      "changes": [
        {
          "path": "name",
          "before": null,
          "after": "Banner Rod (West Wall, Dais)"
        },
        {
          "path": "components.transform.parent",
          "before": null,
          "after": null
        },
        {
          "path": "components.transform.position",
          "before": null,
          "after": [-8.92, 8.75, -11.0]
        },
        {
          "path": "components.transform.rotation",
          "before": null,
          "after": [0.0, 0.0, 0.0, 1.0]
        },
        {
          "path": "components.transform.scale",
          "before": null,
          "after": [0.12, 0.12, 2.7]
        },
        {
          "path": "components.primitive_mesh.shape",
          "before": null,
          "after": "box"
        },
        {
          "path": "components.material.base_color",
          "before": null,
          "after": [0.48, 0.29, 0.12]
        },
        {
          "path": "components.material.roughness",
          "before": null,
          "after": 0.32
        },
        {
          "path": "components.material.metallic",
          "before": null,
          "after": 1.0
        },
        {
          "path": "components.material.emissive",
          "before": null,
          "after": [0.0, 0.0, 0.0]
        },
        {
          "path": "components.material.emissive_strength",
          "before": null,
          "after": 0.0
        }
      ]
    },
    {
      "entity_id": "973060cd-d005-51f6-9b5a-265a9ac9031e",
      "name": "Banner (West Wall, Dais)",
      "changes": [
        {
          "path": "name",
          "before": null,
          "after": "Banner (West Wall, Dais)"
        },
        {
          "path": "components.transform.parent",
          "before": null,
          "after": null
        },
        {
          "path": "components.transform.position",
          "before": null,
          "after": [-8.96, 5.4, -11.0]
        },
        {
          "path": "components.transform.rotation",
          "before": null,
          "after": [0.0, 0.0, 0.0, 1.0]
        },
        {
          "path": "components.transform.scale",
          "before": null,
          "after": [0.06, 6.4, 2.2]
        },
        {
          "path": "components.primitive_mesh.shape",
          "before": null,
          "after": "box"
        },
        {
          "path": "components.material.base_color",
          "before": null,
          "after": [0.3, 0.022, 0.026]
        },
        {
          "path": "components.material.roughness",
          "before": null,
          "after": 0.55
        },
        {
          "path": "components.material.metallic",
          "before": null,
          "after": 0.0
        },
        {
          "path": "components.material.emissive",
          "before": null,
          "after": [0.0, 0.0, 0.0]
        },
        {
          "path": "components.material.emissive_strength",
          "before": null,
          "after": 0.0
        }
      ]
    }
  ],
  "entities_deleted": [],
  "entities_modified": [
    {
      "entity_id": "4b64139c-ec7c-5014-944d-69cb98f1a8d9",
      "name": "Monolith Seam",
      "changes": [
        {
          "path": "components.material.emissive_strength",
          "before": 2.2,
          "after": 3.2
        }
      ]
    }
  ],
  "scene_fields": [],
  "summary": "2 created, 0 deleted, 1 modified, 0 scene field(s) changed"
}

```

Text diff of the scene file: `results/05_text.diff` (48 changed lines).

### scene validate (after commit)

```
$ sovereign-cli scene validate --scene docs/reports/evidence/cli/work/first_light.scene.json
exit 0 (expected 0)
```

stdout:
```
valid: "First Light" scene_id=8d4684d6-93cd-5d04-a868-b4d839bafda7 revision=2 entities=98

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
  "request_id": "d6b2c6fb-c818-4ce7-b638-fa37e41422a0",
  "status": "committed",
  "delivery": "replayed",
  "kind": "edit",
  "actor": "agent:place-banner-example",
  "scene_id": "8d4684d6-93cd-5d04-a868-b4d839bafda7",
  "revision_before": 1,
  "revision_after": 2,
  "commit_id": "abb4ada7-22b7-40cc-af9c-24ada61e7a4a",
  "target_commit": null,
  "request_hash": "0aea9cc63e2f84929548717420871acf56c7af24020668c864f7ab27be7ffacf",
  "operations": [
    {
      "index": 0,
      "op": "entity.create",
      "entity_id": "973060cd-d005-51f6-9b5a-265a9ac9031e",
      "status": "applied"
    },
    {
      "index": 1,
      "op": "entity.create",
      "entity_id": "6b45d759-a0c1-5147-9db2-6e7bab9f474f",
      "status": "applied"
    },
    {
      "index": 2,
      "op": "material.set",
      "entity_id": "4b64139c-ec7c-5014-944d-69cb98f1a8d9",
      "status": "applied"
    }
  ],
  "errors": [],
  "warnings": [],
  "diff": {
    "revision_before": 1,
    "revision_after": 2,
    "entities_created": [
      {
        "entity_id": "6b45d759-a0c1-5147-9db2-6e7bab9f474f",
        "name": "Banner Rod (West Wall, Dais)",
        "changes": [
          {
            "path": "name",
            "before": null,
            "after": "Banner Rod (West Wall, Dais)"
          },
          {
            "path": "components.transform.parent",
            "before": null,
            "after": null
          },
          {
            "path": "components.transform.position",
            "before": null,
            "after": [-8.92, 8.75, -11.0]
          },
          {
            "path": "components.transform.rotation",
            "before": null,
            "after": [0.0, 0.0, 0.0, 1.0]
          },
          {
            "path": "components.transform.scale",
            "before": null,
            "after": [0.12, 0.12, 2.7]
          },
          {
            "path": "components.primitive_mesh.shape",
            "before": null,
            "after": "box"
          },
          {
            "path": "components.material.base_color",
            "before": null,
            "after": [0.48, 0.29, 0.12]
          },
          {
            "path": "components.material.roughness",
            "before": null,
            "after": 0.32
          },
          {
            "path": "components.material.metallic",
            "before": null,
            "after": 1.0
          },
          {
            "path": "components.material.emissive",
            "before": null,
            "after": [0.0, 0.0, 0.0]
          },
          {
            "path": "components.material.emissive_strength",
            "before": null,
            "after": 0.0
          }
        ]
      },
      {
        "entity_id": "973060cd-d005-51f6-9b5a-265a9ac9031e",
        "name": "Banner (West Wall, Dais)",
        "changes": [
          {
            "path": "name",
            "before": null,
            "after": "Banner (West Wall, Dais)"
          },
          {
            "path": "components.transform.parent",
            "before": null,
            "after": null
          },
          {
            "path": "components.transform.position",
            "before": null,
            "after": [-8.96, 5.4, -11.0]
          },
          {
            "path": "components.transform.rotation",
            "before": null,
            "after": [0.0, 0.0, 0.0, 1.0]
          },
          {
            "path": "components.transform.scale",
            "before": null,
            "after": [0.06, 6.4, 2.2]
          },
          {
            "path": "components.primitive_mesh.shape",
            "before": null,
            "after": "box"
          },
          {
            "path": "components.material.base_color",
            "before": null,
            "after": [0.3, 0.022, 0.026]
          },
          {
            "path": "components.material.roughness",
            "before": null,
            "after": 0.55
          },
          {
            "path": "components.material.metallic",
            "before": null,
            "after": 0.0
          },
          {
            "path": "components.material.emissive",
            "before": null,
            "after": [0.0, 0.0, 0.0]
          },
          {
            "path": "components.material.emissive_strength",
            "before": null,
            "after": 0.0
          }
        ]
      }
    ],
    "entities_deleted": [],
    "entities_modified": [
      {
        "entity_id": "4b64139c-ec7c-5014-944d-69cb98f1a8d9",
        "name": "Monolith Seam",
        "changes": [
          {
            "path": "components.material.emissive_strength",
            "before": 2.2,
            "after": 3.2
          }
        ]
      }
    ],
    "scene_fields": [],
    "summary": "2 created, 0 deleted, 1 modified, 0 scene field(s) changed"
  },
  "conflict": null
}

```

stderr:
```
duplicate delivery: returned the recorded result of commit "abb4ada7-22b7-40cc-af9c-24ada61e7a4a"; nothing was applied
```

- [x] replayed: recorded result returned, delivery replayed, same commit id, revision still 2

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
  "request_id": "d6b2c6fb-c818-4ce7-b638-fa37e41422a0",
  "status": "committed",
  "delivery": "replayed",
  "kind": "edit",
  "actor": "agent:place-banner-example",
  "scene_id": "8d4684d6-93cd-5d04-a868-b4d839bafda7",
  "revision_before": 1,
  "revision_after": 2,
  "commit_id": "abb4ada7-22b7-40cc-af9c-24ada61e7a4a",
  "target_commit": null,
  "request_hash": "0aea9cc63e2f84929548717420871acf56c7af24020668c864f7ab27be7ffacf",
  "operations": [
    {
      "index": 0,
      "op": "entity.create",
      "entity_id": "973060cd-d005-51f6-9b5a-265a9ac9031e",
      "status": "applied"
    },
    {
      "index": 1,
      "op": "entity.create",
      "entity_id": "6b45d759-a0c1-5147-9db2-6e7bab9f474f",
      "status": "applied"
    },
    {
      "index": 2,
      "op": "material.set",
      "entity_id": "4b64139c-ec7c-5014-944d-69cb98f1a8d9",
      "status": "applied"
    }
  ],
  "errors": [],
  "warnings": [],
  "diff": {
    "revision_before": 1,
    "revision_after": 2,
    "entities_created": [
      {
        "entity_id": "6b45d759-a0c1-5147-9db2-6e7bab9f474f",
        "name": "Banner Rod (West Wall, Dais)",
        "changes": [
          {
            "path": "name",
            "before": null,
            "after": "Banner Rod (West Wall, Dais)"
          },
          {
            "path": "components.transform.parent",
            "before": null,
            "after": null
          },
          {
            "path": "components.transform.position",
            "before": null,
            "after": [-8.92, 8.75, -11.0]
          },
          {
            "path": "components.transform.rotation",
            "before": null,
            "after": [0.0, 0.0, 0.0, 1.0]
          },
          {
            "path": "components.transform.scale",
            "before": null,
            "after": [0.12, 0.12, 2.7]
          },
          {
            "path": "components.primitive_mesh.shape",
            "before": null,
            "after": "box"
          },
          {
            "path": "components.material.base_color",
            "before": null,
            "after": [0.48, 0.29, 0.12]
          },
          {
            "path": "components.material.roughness",
            "before": null,
            "after": 0.32
          },
          {
            "path": "components.material.metallic",
            "before": null,
            "after": 1.0
          },
          {
            "path": "components.material.emissive",
            "before": null,
            "after": [0.0, 0.0, 0.0]
          },
          {
            "path": "components.material.emissive_strength",
            "before": null,
            "after": 0.0
          }
        ]
      },
      {
        "entity_id": "973060cd-d005-51f6-9b5a-265a9ac9031e",
        "name": "Banner (West Wall, Dais)",
        "changes": [
          {
            "path": "name",
            "before": null,
            "after": "Banner (West Wall, Dais)"
          },
          {
            "path": "components.transform.parent",
            "before": null,
            "after": null
          },
          {
            "path": "components.transform.position",
            "before": null,
            "after": [-8.96, 5.4, -11.0]
          },
          {
            "path": "components.transform.rotation",
            "before": null,
            "after": [0.0, 0.0, 0.0, 1.0]
          },
          {
            "path": "components.transform.scale",
            "before": null,
            "after": [0.06, 6.4, 2.2]
          },
          {
            "path": "components.primitive_mesh.shape",
            "before": null,
            "after": "box"
          },
          {
            "path": "components.material.base_color",
            "before": null,
            "after": [0.3, 0.022, 0.026]
          },
          {
            "path": "components.material.roughness",
            "before": null,
            "after": 0.55
          },
          {
            "path": "components.material.metallic",
            "before": null,
            "after": 0.0
          },
          {
            "path": "components.material.emissive",
            "before": null,
            "after": [0.0, 0.0, 0.0]
          },
          {
            "path": "components.material.emissive_strength",
            "before": null,
            "after": 0.0
          }
        ]
      }
    ],
    "entities_deleted": [],
    "entities_modified": [
      {
        "entity_id": "4b64139c-ec7c-5014-944d-69cb98f1a8d9",
        "name": "Monolith Seam",
        "changes": [
          {
            "path": "components.material.emissive_strength",
            "before": 2.2,
            "after": 3.2
          }
        ]
      }
    ],
    "scene_fields": [],
    "summary": "2 created, 0 deleted, 1 modified, 0 scene field(s) changed"
  },
  "conflict": null
}

```

stderr:
```
duplicate delivery: returned the recorded result of commit "abb4ada7-22b7-40cc-af9c-24ada61e7a4a"; nothing was applied
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
  "request_id": "0a6d3a05-812e-4302-a315-ef456e820524",
  "status": "rejected",
  "delivery": "original",
  "kind": "edit",
  "actor": "agent:place-banner-example",
  "scene_id": "8d4684d6-93cd-5d04-a868-b4d839bafda7",
  "revision_before": 2,
  "revision_after": 2,
  "commit_id": null,
  "target_commit": null,
  "request_hash": "f9fd57262037e939781679e179c649816a6a1cde0af205e727a25f9cfe16b7b5",
  "operations": [],
  "errors": [
    {
      "code": "idempotency_conflict",
      "message": "idempotency key \"place-banner-commit-0001\" for actor \"agent:place-banner-example\" was already used by commit abb4ada7-22b7-40cc-af9c-24ada61e7a4a with different request content (recorded hash 0aea9cc63e2f84929548717420871acf56c7af24020668c864f7ab27be7ffacf, this request f9fd57262037e939781679e179c649816a6a1cde0af205e727a25f9cfe16b7b5). Use a new key for a new action.",
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
rejected: idempotency_conflict at /idempotency_key: idempotency key "place-banner-commit-0001" for actor "agent:place-banner-example" was already used by commit abb4ada7-22b7-40cc-af9c-24ada61e7a4a with different request content (recorded hash 0aea9cc63e2f84929548717420871acf56c7af24020668c864f7ab27be7ffacf, this request f9fd57262037e939781679e179c649816a6a1cde0af205e727a25f9cfe16b7b5). Use a new key for a new action.
```

- [x] rejected with idempotency_conflict; scene unchanged

## 5. Stale revision and invalid batch

### new key, base_revision 1 while scene is at 2

```
$ sovereign-cli scene apply --scene docs/reports/evidence/cli/work/first_light.scene.json --commands docs/reports/evidence/cli/requests/place_banner.stale_base_revision.command_batch.json
exit 1 (expected 1)
```

stdout:
```
{
  "protocol": "sovereign.command_result",
  "protocol_version": 1,
  "request_id": "d22214a5-544f-4add-af85-9bed4691a44b",
  "status": "rejected",
  "delivery": "original",
  "kind": "edit",
  "actor": "agent:place-banner-example",
  "scene_id": "8d4684d6-93cd-5d04-a868-b4d839bafda7",
  "revision_before": 2,
  "revision_after": 2,
  "commit_id": null,
  "target_commit": null,
  "request_hash": "0aea9cc63e2f84929548717420871acf56c7af24020668c864f7ab27be7ffacf",
  "operations": [],
  "errors": [
    {
      "code": "stale_revision",
      "message": "base_revision 1 is stale; the scene is at revision 2. Re-read the scene and resubmit against the current revision.",
      "path": "/base_revision",
      "op_index": null,
      "entity_id": null
    }
  ],
  "warnings": [],
  "diff": null,
  "conflict": {
    "base_revision": 1,
    "current_revision": 2,
    "commits_since_base": [
      {
        "commit_id": "abb4ada7-22b7-40cc-af9c-24ada61e7a4a",
        "kind": "edit",
        "actor": "agent:place-banner-example",
        "base_revision": 1,
        "new_revision": 2,
        "label": "batch d6b2c6fb-c818-4ce7-b638-fa37e41422a0 (3 operation(s))",
        "entities": ["4b64139c-ec7c-5014-944d-69cb98f1a8d9", "6b45d759-a0c1-5147-9db2-6e7bab9f474f", "973060cd-d005-51f6-9b5a-265a9ac9031e"]
      }
    ],
    "history_complete": true
  }
}

```

stderr:
```
rejected: stale_revision at /base_revision: base_revision 1 is stale; the scene is at revision 2. Re-read the scene and resubmit against the current revision.
```

- [x] stale_revision with conflict details (current 2, one newer commit)

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
  "request_id": "0a400d10-fb2b-478a-aee0-790c96bb860d",
  "status": "rejected",
  "delivery": "original",
  "kind": "edit",
  "actor": "agent:place-banner-example",
  "scene_id": "8d4684d6-93cd-5d04-a868-b4d839bafda7",
  "revision_before": 2,
  "revision_after": 2,
  "commit_id": null,
  "target_commit": null,
  "request_hash": "ae66d24745656a9add76896c0d281005c7ba674969997763fef0dc5bfe13764f",
  "operations": [
    {
      "index": 0,
      "op": "entity.rename",
      "entity_id": "973060cd-d005-51f6-9b5a-265a9ac9031e",
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

### scene undo --base-revision 2

```
$ sovereign-cli scene undo --scene docs/reports/evidence/cli/work/first_light.scene.json --base-revision 2 --actor cli:demo
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
  "scene_id": "8d4684d6-93cd-5d04-a868-b4d839bafda7",
  "revision_before": 2,
  "revision_after": 3,
  "commit_id": "e0d43287-a4db-4053-9878-98c9836e7f8e",
  "target_commit": "abb4ada7-22b7-40cc-af9c-24ada61e7a4a",
  "request_hash": null,
  "operations": [
    {
      "index": 0,
      "op": "material.set",
      "entity_id": "4b64139c-ec7c-5014-944d-69cb98f1a8d9",
      "status": "applied"
    },
    {
      "index": 1,
      "op": "entity.delete",
      "entity_id": "6b45d759-a0c1-5147-9db2-6e7bab9f474f",
      "status": "applied"
    },
    {
      "index": 2,
      "op": "entity.delete",
      "entity_id": "973060cd-d005-51f6-9b5a-265a9ac9031e",
      "status": "applied"
    }
  ],
  "errors": [],
  "warnings": [],
  "diff": {
    "revision_before": 2,
    "revision_after": 3,
    "entities_created": [],
    "entities_deleted": [
      {
        "entity_id": "6b45d759-a0c1-5147-9db2-6e7bab9f474f",
        "name": "Banner Rod (West Wall, Dais)",
        "changes": [
          {
            "path": "name",
            "before": "Banner Rod (West Wall, Dais)",
            "after": null
          },
          {
            "path": "components.transform.parent",
            "before": null,
            "after": null
          },
          {
            "path": "components.transform.position",
            "before": [-8.92, 8.75, -11.0],
            "after": null
          },
          {
            "path": "components.transform.rotation",
            "before": [0.0, 0.0, 0.0, 1.0],
            "after": null
          },
          {
            "path": "components.transform.scale",
            "before": [0.12, 0.12, 2.7],
            "after": null
          },
          {
            "path": "components.primitive_mesh.shape",
            "before": "box",
            "after": null
          },
          {
            "path": "components.material.base_color",
            "before": [0.48, 0.29, 0.12],
            "after": null
          },
          {
            "path": "components.material.roughness",
            "before": 0.32,
            "after": null
          },
          {
            "path": "components.material.metallic",
            "before": 1.0,
            "after": null
          },
          {
            "path": "components.material.emissive",
            "before": [0.0, 0.0, 0.0],
            "after": null
          },
          {
            "path": "components.material.emissive_strength",
            "before": 0.0,
            "after": null
          }
        ]
      },
      {
        "entity_id": "973060cd-d005-51f6-9b5a-265a9ac9031e",
        "name": "Banner (West Wall, Dais)",
        "changes": [
          {
            "path": "name",
            "before": "Banner (West Wall, Dais)",
            "after": null
          },
          {
            "path": "components.transform.parent",
            "before": null,
            "after": null
          },
          {
            "path": "components.transform.position",
            "before": [-8.96, 5.4, -11.0],
            "after": null
          },
          {
            "path": "components.transform.rotation",
            "before": [0.0, 0.0, 0.0, 1.0],
            "after": null
          },
          {
            "path": "components.transform.scale",
            "before": [0.06, 6.4, 2.2],
            "after": null
          },
          {
            "path": "components.primitive_mesh.shape",
            "before": "box",
            "after": null
          },
          {
            "path": "components.material.base_color",
            "before": [0.3, 0.022, 0.026],
            "after": null
          },
          {
            "path": "components.material.roughness",
            "before": 0.55,
            "after": null
          },
          {
            "path": "components.material.metallic",
            "before": 0.0,
            "after": null
          },
          {
            "path": "components.material.emissive",
            "before": [0.0, 0.0, 0.0],
            "after": null
          },
          {
            "path": "components.material.emissive_strength",
            "before": 0.0,
            "after": null
          }
        ]
      }
    ],
    "entities_modified": [
      {
        "entity_id": "4b64139c-ec7c-5014-944d-69cb98f1a8d9",
        "name": "Monolith Seam",
        "changes": [
          {
            "path": "components.material.emissive_strength",
            "before": 3.2,
            "after": 2.2
          }
        ]
      }
    ],
    "scene_fields": [],
    "summary": "0 created, 2 deleted, 1 modified, 0 scene field(s) changed"
  },
  "conflict": null
}

```

stderr:
```
committed: revision 2 -> 3 (commit e0d43287-a4db-4053-9878-98c9836e7f8e)
```

### inspect after undo

```
$ sovereign-cli scene inspect --scene docs/reports/evidence/cli/work/first_light.scene.json --json
exit 0 (expected 0)
```

stdout:
```
{
  "format": "sovereign.scene",
  "schema_version": 1,
  "scene_id": "8d4684d6-93cd-5d04-a868-b4d839bafda7",
  "name": "First Light",
  "revision": 3,
  "region": {
    "cell": [0, 0, 0]
  },
  "asset_references": [],
  "active_camera": "98fcda69-2d02-5a08-af47-a01a10aa7dd6",
  "environment": {
    "ambient_color": [0.42, 0.3, 0.26],
    "ambient_intensity": 0.22,
    "background_color": [0.004, 0.0035, 0.005],
    "fog_color": [0.035, 0.02, 0.018],
    "fog_density": 0.018,
    "exposure": 1.1
  },
  "entities": [
    {
      "id": "112b12dd-9bb3-5186-9e24-c12fcf54bfb9",
      "name": "Column E1",
      "components": {
        "transform": {
          "parent": null,
          "position": [5.5, 0.0, -8.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        }
      }
    },
    {
      "id": "1298a82a-2231-57dd-a713-9c28be5ebe3f",
      "name": "Walkway Light Strip (East)",
      "components": {
        "transform": {
          "parent": null,
          "position": [2.25, 0.012, 1.5],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [0.06, 0.024, 23.0]
        },
        "primitive_mesh": {
          "shape": "box"
        },
        "material": {
          "base_color": [0.08, 0.04, 0.01],
          "roughness": 0.5,
          "metallic": 0.0,
          "emissive": [1.0, 0.45, 0.08],
          "emissive_strength": 1.6
        }
      }
    },
    {
      "id": "16877fb2-97b9-53ca-822b-31b8397479cd",
      "name": "Column E1 Capital",
      "components": {
        "transform": {
          "parent": "112b12dd-9bb3-5186-9e24-c12fcf54bfb9",
          "position": [0.0, 7.85, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.5, 0.5, 1.5]
        },
        "primitive_mesh": {
          "shape": "box"
        },
        "material": {
          "base_color": [0.48, 0.29, 0.12],
          "roughness": 0.32,
          "metallic": 1.0,
          "emissive": [0.0, 0.0, 0.0],
          "emissive_strength": 0.0
        }
      }
    },
    {
      "id": "1b4c4755-6e5e-5def-b40c-95a322d8084d",
      "name": "Column W3 Base",
      "components": {
        "transform": {
          "parent": "797ba59f-6819-5db3-8f4d-8823a5891825",
          "position": [0.0, 0.3, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.6, 0.6, 1.6]
        },
        "primitive_mesh": {
          "shape": "box"
        },
        "material": {
          "base_color": [0.03, 0.03, 0.034],
          "roughness": 0.85,
          "metallic": 0.0,
          "emissive": [0.0, 0.0, 0.0],
          "emissive_strength": 0.0
        }
      }
    },
    {
      "id": "1d1d9327-ebdc-546f-93e6-57ded1a720ed",
      "name": "Column E4",
      "components": {
        "transform": {
          "parent": null,
          "position": [5.5, 0.0, 4.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        }
      }
    },
    {
      "id": "1d75e66e-cb39-5d49-8e9c-8ec7428ffa24",
      "name": "Column E6 Capital",
      "components": {
        "transform": {
          "parent": "97bed9e8-b320-594d-a781-9e375023f34e",
          "position": [0.0, 7.85, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.5, 0.5, 1.5]
        },
        "primitive_mesh": {
          "shape": "box"
        },
        "material": {
          "base_color": [0.48, 0.29, 0.12],
          "roughness": 0.32,
          "metallic": 1.0,
          "emissive": [0.0, 0.0, 0.0],
          "emissive_strength": 0.0
        }
      }
    },
    {
      "id": "1dc3879e-9648-559e-b3a9-e1d9e1d468ec",
      "name": "Column W1 Capital",
      "components": {
        "transform": {
          "parent": "9949acb1-9986-5162-b619-52e51af32e47",
          "position": [0.0, 7.85, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.5, 0.5, 1.5]
        },
        "primitive_mesh": {
          "shape": "box"
        },
        "material": {
          "base_color": [0.48, 0.29, 0.12],
          "roughness": 0.32,
          "metallic": 1.0,
          "emissive": [0.0, 0.0, 0.0],
          "emissive_strength": 0.0
        }
      }
    },
    {
      "id": "228ac414-5869-5518-b407-e82353c8cafb",
      "name": "Column W1 Shaft",
      "components": {
        "transform": {
          "parent": "9949acb1-9986-5162-b619-52e51af32e47",
          "position": [0.0, 4.1, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [0.9, 7.0, 0.9]
        },
        "primitive_mesh": {
          "shape": "cylinder"
        },
        "material": {
          "base_color": [0.22, 0.03, 0.03],
          "roughness": 0.4,
          "metallic": 0.0,
          "emissive": [0.0, 0.0, 0.0],
          "emissive_strength": 0.0
        }
      }
    },
    {
      "id": "2e7fa1d0-b3e0-5939-a134-c8a75a58e198",
      "name": "Column W4 Band",
      "components": {
        "transform": {
          "parent": "7fef6493-efce-5317-a904-81f1704a7627",
          "position": [0.0, 2.2, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [0.96, 0.12, 0.96]
        },
        "primitive_mesh": {
          "shape": "cylinder"
        },
        "material": {
          "base_color": [0.08, 0.04, 0.01],
          "roughness": 0.5,
          "metallic": 0.0,
          "emissive": [1.0, 0.45, 0.08],
          "emissive_strength": 1.6
        }
      }
    },
    {
      "id": "30e6248b-5eb4-581b-b0f7-aad817a025cb",
      "name": "Column W2 Band",
      "components": {
        "transform": {
          "parent": "40dc947e-18d5-5e30-94d6-c3deb1ea9bc1",
          "position": [0.0, 2.2, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [0.96, 0.12, 0.96]
        },
        "primitive_mesh": {
          "shape": "cylinder"
        },
        "material": {
          "base_color": [0.08, 0.04, 0.01],
          "roughness": 0.5,
          "metallic": 0.0,
          "emissive": [1.0, 0.45, 0.08],
          "emissive_strength": 1.6
        }

```

### scene redo --base-revision 3

```
$ sovereign-cli scene redo --scene docs/reports/evidence/cli/work/first_light.scene.json --base-revision 3 --actor cli:demo
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
  "scene_id": "8d4684d6-93cd-5d04-a868-b4d839bafda7",
  "revision_before": 3,
  "revision_after": 4,
  "commit_id": "357607f7-9b82-41e6-a89a-01fcb6be1b6d",
  "target_commit": "abb4ada7-22b7-40cc-af9c-24ada61e7a4a",
  "request_hash": null,
  "operations": [
    {
      "index": 0,
      "op": "entity.create",
      "entity_id": "973060cd-d005-51f6-9b5a-265a9ac9031e",
      "status": "applied"
    },
    {
      "index": 1,
      "op": "entity.create",
      "entity_id": "6b45d759-a0c1-5147-9db2-6e7bab9f474f",
      "status": "applied"
    },
    {
      "index": 2,
      "op": "material.set",
      "entity_id": "4b64139c-ec7c-5014-944d-69cb98f1a8d9",
      "status": "applied"
    }
  ],
  "errors": [],
  "warnings": [],
  "diff": {
    "revision_before": 3,
    "revision_after": 4,
    "entities_created": [
      {
        "entity_id": "6b45d759-a0c1-5147-9db2-6e7bab9f474f",
        "name": "Banner Rod (West Wall, Dais)",
        "changes": [
          {
            "path": "name",
            "before": null,
            "after": "Banner Rod (West Wall, Dais)"
          },
          {
            "path": "components.transform.parent",
            "before": null,
            "after": null
          },
          {
            "path": "components.transform.position",
            "before": null,
            "after": [-8.92, 8.75, -11.0]
          },
          {
            "path": "components.transform.rotation",
            "before": null,
            "after": [0.0, 0.0, 0.0, 1.0]
          },
          {
            "path": "components.transform.scale",
            "before": null,
            "after": [0.12, 0.12, 2.7]
          },
          {
            "path": "components.primitive_mesh.shape",
            "before": null,
            "after": "box"
          },
          {
            "path": "components.material.base_color",
            "before": null,
            "after": [0.48, 0.29, 0.12]
          },
          {
            "path": "components.material.roughness",
            "before": null,
            "after": 0.32
          },
          {
            "path": "components.material.metallic",
            "before": null,
            "after": 1.0
          },
          {
            "path": "components.material.emissive",
            "before": null,
            "after": [0.0, 0.0, 0.0]
          },
          {
            "path": "components.material.emissive_strength",
            "before": null,
            "after": 0.0
          }
        ]
      },
      {
        "entity_id": "973060cd-d005-51f6-9b5a-265a9ac9031e",
        "name": "Banner (West Wall, Dais)",
        "changes": [
          {
            "path": "name",
            "before": null,
            "after": "Banner (West Wall, Dais)"
          },
          {
            "path": "components.transform.parent",
            "before": null,
            "after": null
          },
          {
            "path": "components.transform.position",
            "before": null,
            "after": [-8.96, 5.4, -11.0]
          },
          {
            "path": "components.transform.rotation",
            "before": null,
            "after": [0.0, 0.0, 0.0, 1.0]
          },
          {
            "path": "components.transform.scale",
            "before": null,
            "after": [0.06, 6.4, 2.2]
          },
          {
            "path": "components.primitive_mesh.shape",
            "before": null,
            "after": "box"
          },
          {
            "path": "components.material.base_color",
            "before": null,
            "after": [0.3, 0.022, 0.026]
          },
          {
            "path": "components.material.roughness",
            "before": null,
            "after": 0.55
          },
          {
            "path": "components.material.metallic",
            "before": null,
            "after": 0.0
          },
          {
            "path": "components.material.emissive",
            "before": null,
            "after": [0.0, 0.0, 0.0]
          },
          {
            "path": "components.material.emissive_strength",
            "before": null,
            "after": 0.0
          }
        ]
      }
    ],
    "entities_deleted": [],
    "entities_modified": [
      {
        "entity_id": "4b64139c-ec7c-5014-944d-69cb98f1a8d9",
        "name": "Monolith Seam",
        "changes": [
          {
            "path": "components.material.emissive_strength",
            "before": 2.2,
            "after": 3.2
          }
        ]
      }
    ],
    "scene_fields": [],
    "summary": "2 created, 0 deleted, 1 modified, 0 scene field(s) changed"
  },
  "conflict": null
}

```

stderr:
```
committed: revision 3 -> 4 (commit 357607f7-9b82-41e6-a89a-01fcb6be1b6d)
```

first_light.rev1.before.scene.json revision 1 content-without-revision cad826321a68bfac600e99aac3605d0ff6d2319221ac82bbc5026a8e1f51abc6
first_light.rev2.after_commit.scene.json revision 2 content-without-revision 232e441265918cf9acf16726cddfa9f54c1c56aa4daf8577cf2d90b9f0dc1d66
first_light.rev3.after_undo.scene.json revision 3 content-without-revision cad826321a68bfac600e99aac3605d0ff6d2319221ac82bbc5026a8e1f51abc6
first_light.rev4.after_redo.scene.json revision 4 content-without-revision 232e441265918cf9acf16726cddfa9f54c1c56aa4daf8577cf2d90b9f0dc1d66

- [x] undo (rev 3) content == rev 1 content; redo (rev 4) content == rev 2 content; revisions 1,2,3,4

### scene history --json

```
$ sovereign-cli scene history --scene docs/reports/evidence/cli/work/first_light.scene.json --json
exit 0 (expected 0)
```

stdout:
```
{
  "revision": 4,
  "next_undo": "abb4ada7-22b7-40cc-af9c-24ada61e7a4a",
  "next_redo": null,
  "journal": {
    "present": true,
    "consistent": true,
    "notes": []
  },
  "transactions": [
    {
      "commit_id": "abb4ada7-22b7-40cc-af9c-24ada61e7a4a",
      "kind": "edit",
      "actor": "agent:place-banner-example",
      "base_revision": 1,
      "new_revision": 2,
      "label": "batch d6b2c6fb-c818-4ce7-b638-fa37e41422a0 (3 operation(s))",
      "idempotency_key": "place-banner-commit-0001",
      "operations": 3
    },
    {
      "commit_id": "e0d43287-a4db-4053-9878-98c9836e7f8e",
      "kind": "undo",
      "actor": "cli:demo",
      "base_revision": 2,
      "new_revision": 3,
      "label": "Undo: batch d6b2c6fb-c818-4ce7-b638-fa37e41422a0 (3 operation(s))",
      "idempotency_key": "",
      "operations": 3
    },
    {
      "commit_id": "357607f7-9b82-41e6-a89a-01fcb6be1b6d",
      "kind": "redo",
      "actor": "cli:demo",
      "base_revision": 3,
      "new_revision": 4,
      "label": "Redo: batch d6b2c6fb-c818-4ce7-b638-fa37e41422a0 (3 operation(s))",
      "idempotency_key": "",
      "operations": 3
    }
  ]
}

```

### undo with an outdated base revision

```
$ sovereign-cli scene undo --scene docs/reports/evidence/cli/work/first_light.scene.json --base-revision 2
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
  "scene_id": "8d4684d6-93cd-5d04-a868-b4d839bafda7",
  "revision_before": 4,
  "revision_after": 4,
  "commit_id": null,
  "target_commit": "abb4ada7-22b7-40cc-af9c-24ada61e7a4a",
  "request_hash": null,
  "operations": [],
  "errors": [
    {
      "code": "stale_revision",
      "message": "base_revision 2 is stale; the scene is at revision 4. Re-read the scene and resubmit against the current revision.",
      "path": "/base_revision",
      "op_index": null,
      "entity_id": null
    }
  ],
  "warnings": [],
  "diff": null,
  "conflict": {
    "base_revision": 2,
    "current_revision": 4,
    "commits_since_base": [
      {
        "commit_id": "e0d43287-a4db-4053-9878-98c9836e7f8e",
        "kind": "undo",
        "actor": "cli:demo",
        "base_revision": 2,
        "new_revision": 3,
        "label": "Undo: batch d6b2c6fb-c818-4ce7-b638-fa37e41422a0 (3 operation(s))",
        "entities": ["4b64139c-ec7c-5014-944d-69cb98f1a8d9", "6b45d759-a0c1-5147-9db2-6e7bab9f474f", "973060cd-d005-51f6-9b5a-265a9ac9031e"]
      },
      {
        "commit_id": "357607f7-9b82-41e6-a89a-01fcb6be1b6d",
        "kind": "redo",
        "actor": "cli:demo",
        "base_revision": 3,
        "new_revision": 4,
        "label": "Redo: batch d6b2c6fb-c818-4ce7-b638-fa37e41422a0 (3 operation(s))",
        "entities": ["4b64139c-ec7c-5014-944d-69cb98f1a8d9", "6b45d759-a0c1-5147-9db2-6e7bab9f474f", "973060cd-d005-51f6-9b5a-265a9ac9031e"]
      }
    ],
    "history_complete": true
  }
}

```

stderr:
```
rejected: stale_revision at /base_revision: base_revision 2 is stale; the scene is at revision 4. Re-read the scene and resubmit against the current revision.
```

## 7. Writer lock: offline write refused while another writer holds the scene

Here the lock is held by util-linux `flock` on the same lock file (same flock(2) lock the editor takes). The real-editor variant is `vulkan.editor_real_x11_input` (evidence/gui_real_input).

### committing apply while lock is held

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
valid: "First Light" scene_id=8d4684d6-93cd-5d04-a868-b4d839bafda7 revision=4 entities=98

```

- [x] scene unchanged while locked

Journal copied to `scenes/first_light.journal.jsonl` (7 lines).

## Summary

Unexpected exit codes or failed checks: **0**

