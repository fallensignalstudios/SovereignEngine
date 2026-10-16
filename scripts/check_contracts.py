#!/usr/bin/env python3
"""Validates scene documents and command batches against the owner's contracts
(schemas/scene.schema.json, schemas/command_batch.schema.json, verbatim from the First Ignition
packet) with an independent validator (jsonschema), so engine-written files are checked by
something other than the engine.

Usage: check_contracts.py [FILE_OR_DIR ...]   (default: the repository's scenes and batches)
Files ending in .scene.json are scenes; files ending in .command_batch.json are batches; for a
directory, both kinds are collected recursively. Exit 0 if all conform, 1 if any does not,
77 if jsonschema is not installed (CTest reports that as skipped, never passed).
"""
import json
import math
import sys
from pathlib import Path

try:
    from jsonschema import Draft202012Validator, FormatChecker
    from referencing import Registry, Resource
except ImportError:
    print("jsonschema/referencing not installed: contract check skipped", file=sys.stderr)
    raise SystemExit(77)

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ["content", "examples", "tests/fixtures"]


def reject_constant(value):
    raise ValueError(f"non-finite JSON constant: {value}")


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f, parse_constant=reject_constant)


scene_schema = load(ROOT / "schemas/scene.schema.json")
batch_schema = load(ROOT / "schemas/command_batch.schema.json")
registry = Registry().with_resources([(scene_schema["$id"], Resource.from_contents(scene_schema)),
                                      (batch_schema["$id"], Resource.from_contents(batch_schema))])
validators = {
    "scene": Draft202012Validator(scene_schema, registry=registry, format_checker=FormatChecker()),
    "batch": Draft202012Validator(batch_schema, registry=registry, format_checker=FormatChecker()),
}


def kind_of(path):
    name = path.name
    if name.endswith(".scene.json"):
        return "scene"
    if name.endswith(".command_batch.json"):
        return "batch"
    return None


def semantic_scene_problems(scene):
    """The packet checker's extra scene rules (normalized quaternions, references, far > near)."""
    problems = []
    by_id = {e["id"]: e for e in scene["entities"]}
    if len(by_id) != len(scene["entities"]):
        problems.append("duplicate entity id")
    cam = by_id.get(scene["active_camera_id"])
    if cam is None or "Camera" not in cam["components"]:
        problems.append("active_camera_id does not name an entity with a Camera")
    for e in scene["entities"]:
        t = e["components"]["Transform"]
        if not math.isclose(sum(v * v for v in t["rotation_xyzw"]), 1.0, abs_tol=1e-6):
            problems.append(f"{e['id']}: rotation_xyzw not normalized")
        if t["parent_id"] is not None and t["parent_id"] not in by_id:
            problems.append(f"{e['id']}: missing parent {t['parent_id']}")
        c = e["components"].get("Camera")
        if c and c["far_m"] <= c["near_m"]:
            problems.append(f"{e['id']}: far_m <= near_m")
    return problems


def collect(args):
    paths = []
    for a in args or [str(ROOT / d) for d in DEFAULT]:
        p = Path(a)
        if p.is_dir():
            paths += sorted(q for q in p.rglob("*.json") if kind_of(q))
        elif kind_of(p):
            paths.append(p)
    return paths


failures = 0
files = collect(sys.argv[1:])
for path in files:
    kind = kind_of(path)
    rel = path.resolve().relative_to(ROOT) if path.resolve().is_relative_to(ROOT) else path
    try:
        doc = load(path)
        errors = [f"{'/'.join(map(str, e.absolute_path)) or '(root)'}: {e.message}"
                  for e in validators[kind].iter_errors(doc)]
        if kind == "scene" and not errors:
            errors = semantic_scene_problems(doc)
    except (ValueError, OSError) as exc:
        errors = [str(exc)]
    if errors:
        failures += 1
        print(f"FAIL {kind:5} {rel}")
        for e in errors[:10]:
            print(f"       {e}")
    else:
        print(f"ok   {kind:5} {rel}")
print(f"{len(files)} file(s), {failures} failure(s)")
sys.exit(1 if failures or not files else 0)
