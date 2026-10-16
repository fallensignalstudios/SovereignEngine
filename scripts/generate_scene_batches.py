#!/usr/bin/env python3
"""Generates the command batch that BUILDS the S0 Axis Lab test fixture through the engine's own
command pipeline (sovereign-cli scene new + scene apply), in the owner's command batch contract v1
(schemas/command_batch.schema.json). The resulting scene is ordinary editable authoring data.

First Light and the example batch are NOT generated: they are the owner's packet fixtures
(examples/first_light.scene.json, examples/place_banner.command_batch.json, verbatim), and
content/authoring/first_light.scene.json is created from the former by `sovereign-cli scene import`.

Output (deterministic: UUIDv5 from entity names, fixed key order):
  tests/fixtures/axis_lab.build.command_batch.json

Run from the repository root:  python3 scripts/generate_scene_batches.py
Then build the scene with:     scripts/build_reference_scenes.sh <path-to-sovereign-cli>
"""

import json
import math
import os
import uuid

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NS = uuid.UUID("6f1c2a3e-9b4d-5e8f-a1b2-c3d4e5f60718")  # fixed namespace for reference content


def uid(name):
    return str(uuid.uuid5(NS, name))


def quat_axis_angle(axis, degrees):
    ax, ay, az = axis
    n = math.sqrt(ax * ax + ay * ay + az * az)
    s = math.sin(math.radians(degrees) / 2) / n
    return [ax * s, ay * s, az * s, math.cos(math.radians(degrees) / 2)]


def quat_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return [aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz]


def euler(pitch=0.0, yaw=0.0, roll=0.0):
    """Engine display convention: q = qY(yaw) * qX(pitch) * qZ(roll). Rounded to 12 digits, which
    keeps |q|^2 within the contract's 1e-6 normalization tolerance by a wide margin."""
    q = quat_mul(quat_mul(quat_axis_angle((0, 1, 0), yaw), quat_axis_angle((1, 0, 0), pitch)),
                 quat_axis_angle((0, 0, 1), roll))
    return [round(c, 12) for c in q]


IDENTITY = [0.0, 0.0, 0.0, 1.0]


def material(base_rgb, emissive=(0.0, 0.0, 0.0)):
    return {"base_color_rgba": [*base_rgb, 1.0], "emissive_rgb": list(emissive)}


def create(name, pos, scale=(1, 1, 1), rot=IDENTITY, primitive=None, mat=None, parent=None):
    comps = {"Transform": {"position": list(pos), "rotation_xyzw": list(rot), "scale": list(scale),
                           "parent_id": uid(parent) if parent else None}}
    if primitive:
        comps["PrimitiveMesh"] = {"primitive": primitive}
        comps["MaterialParameters"] = mat
    return {"op": "entity.create", "entity": {"id": uid(name), "name": name, "components": comps}}


def batch(request_name, actor, scene_id, base_revision, key, ops, dry_run):
    return {"protocol_version": 1, "request_id": uid("request:" + request_name), "actor": actor,
            "scene_id": scene_id, "base_revision": base_revision, "idempotency_key": key, "dry_run": dry_run,
            "operations": ops}


def write(rel, obj):
    path = os.path.join(ROOT, rel)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f, indent=2)
        f.write("\n")
    print("wrote", rel)


# --------------------------------------------------------------------------------------------
# S0 Axis Lab: asymmetric axis markers, depth overlap, front/back-facing planes, parent/child,
# material swatches. Emissive markers on black base color keep rendered colors predictable for
# readback regardless of the R0 fixed lighting.
# --------------------------------------------------------------------------------------------
AXIS_LAB_ID = uid("scene:axis-lab")
AXIS_LAB_CAMERA = uid("Axis Lab Camera")
BLACK = (0.0, 0.0, 0.0)


def glow(rgb, strength=1.0):
    return material(BLACK, tuple(c * strength for c in rgb))


al = []
al.append(create("X Marker (long box)", (2.0, 0, 0), (2.0, 0.3, 0.3), primitive="box", mat=glow((1, 0, 0))))
al.append(create("Y Marker (cylinder)", (0, 2.0, 0), (0.3, 2.0, 0.3), primitive="cylinder", mat=glow((0, 1, 0))))
al.append(create("Z Marker (flat box)", (0, 0, 1.5), (0.5, 0.15, 1.5), primitive="box", mat=glow((0, 0, 1))))
al.append(create("Origin Cube", (0, 0, 0), (0.4, 0.4, 0.4), primitive="box", mat=glow((1, 1, 1))))
# Depth overlap: the near box partly covers the larger far box.
al.append(create("Depth Near Box", (-3.0, -2.0, 1.0), (1.0, 1.0, 1.0), primitive="box", mat=glow((0, 1, 1))))
al.append(create("Depth Far Box", (-3.5, -2.0, -2.0), (2.5, 2.5, 1.0), primitive="box", mat=glow((1, 1, 0))))
# Culling: a single-sided plane facing the camera is drawn; one facing away is culled.
al.append(create("Front-Facing Plane", (3.5, -2.0, 0), (1.5, 1, 1.5), rot=euler(pitch=90), primitive="plane",
                 mat=glow((1, 0.5, 0))))
al.append(create("Back-Facing Plane", (3.5, 2.5, 0), (1.5, 1, 1.5), rot=euler(pitch=-90), primitive="plane",
                 mat=glow((1, 0, 1))))
# Parent/child: rotated, uniformly scaled pivot with an offset child.
al.append(create("Hierarchy Parent", (-4.0, 2.0, 0), (2.0, 2.0, 2.0), rot=euler(roll=45), primitive="box",
                 mat=glow((0.2, 0.2, 0.6), 0.5)))
al.append(create("Hierarchy Child", (0.9, 0, 0), (0.2, 0.2, 0.2), primitive="box", mat=glow((1, 0, 0.5)),
                 parent="Hierarchy Parent"))
# Lit material swatches (bronze, deep red, charcoal) on a floor plane.
al.append(create("Swatch Floor", (0, -3.6, 0), (12, 1, 5), primitive="plane", mat=material((0.18, 0.18, 0.18))))
for i, (nm, rgb) in enumerate((("Bronze", (0.48, 0.29, 0.12)), ("Deep Red", (0.30, 0.022, 0.026)),
                               ("Charcoal", (0.03, 0.03, 0.034)))):
    al.append(create(f"Swatch {nm}", (-1.5 + 1.5 * i, -3.2, 2.0), (0.8, 0.8, 0.8), primitive="box", mat=material(rgb)))
al.append({"op": "transform.set", "entity_id": AXIS_LAB_CAMERA,
           "transform": {"position": [0.0, 0.0, 10.0], "rotation_xyzw": IDENTITY, "scale": [1.0, 1.0, 1.0],
                         "parent_id": None}})

write("tests/fixtures/axis_lab.build.command_batch.json",
      batch("axis-lab-build", "tool:scene-builder", AXIS_LAB_ID, 0, "axis-lab-build-v1", al, dry_run=False))

print(f"Axis Lab: {len(al)} operations")
print("ids:", json.dumps({"axis_lab_scene": AXIS_LAB_ID, "axis_lab_camera": AXIS_LAB_CAMERA}))
