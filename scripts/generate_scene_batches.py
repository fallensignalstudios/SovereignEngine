#!/usr/bin/env python3
"""Generates the command batches that BUILD the reference scenes through the engine's own
command pipeline (sovereign-cli scene new + scene apply). The resulting scene documents are
ordinary editable authoring data; nothing here is rendered or hard-coded at runtime.

Outputs (deterministic: UUIDv5 from entity names, fixed key order):
  content/source/first_light.environment.json
  content/source/first_light.build.command_batch.json
  tests/fixtures/axis_lab.environment.json
  tests/fixtures/axis_lab.build.command_batch.json
  examples/place_banner.command_batch.json

Run from the repository root:  python3 scripts/generate_scene_batches.py
Then build the scenes with:    scripts/build_reference_scenes.sh <path-to-sovereign-cli>
"""

import json
import math
import os
import sys
import uuid

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NS = uuid.UUID("6f1c2a3e-9b4d-5e8f-a1b2-c3d4e5f60718")  # fixed namespace for reference content


def uid(name):
    return str(uuid.uuid5(NS, name))


def quat_axis_angle(axis, degrees):
    ax, ay, az = axis
    n = math.sqrt(ax * ax + ay * ay + az * az)
    s = math.sin(math.radians(degrees) / 2) / n
    return [round(ax * s, 9), round(ay * s, 9), round(az * s, 9), round(math.cos(math.radians(degrees) / 2), 9)]


def quat_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return [aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz]


def euler(pitch=0.0, yaw=0.0, roll=0.0):
    """Engine display convention: q = qY(yaw) * qX(pitch) * qZ(roll)."""
    q = quat_mul(quat_mul(quat_axis_angle((0, 1, 0), yaw), quat_axis_angle((1, 0, 0), pitch)),
                 quat_axis_angle((0, 0, 1), roll))
    return [round(c, 9) for c in q]


IDENTITY = [0.0, 0.0, 0.0, 1.0]


def mat(base, roughness=0.6, metallic=0.0, emissive=(0, 0, 0), strength=0.0):
    return {"base_color": list(base), "roughness": roughness, "metallic": metallic,
            "emissive": list(emissive), "emissive_strength": strength}


def create(name, pos, scale=(1, 1, 1), rot=IDENTITY, shape=None, material=None, parent=None, extra=None):
    comps = {"transform": {"parent": uid(parent) if parent else None, "position": list(pos),
                           "rotation": list(rot), "scale": list(scale)}}
    if shape:
        comps["primitive_mesh"] = {"shape": shape}
    if material:
        comps["material"] = material
    if extra:
        comps.update(extra)
    return {"op": "entity.create", "entity_id": uid(name), "name": name, "components": comps}


def batch(request_name, actor, scene_id, base_revision, key, ops, dry_run):
    return {"protocol": "sovereign.command_batch", "protocol_version": 1, "request_id": uid("request:" + request_name),
            "actor": actor, "scene_id": scene_id, "base_revision": base_revision, "idempotency_key": key,
            "dry_run": dry_run, "operations": ops}


def write(rel, obj):
    path = os.path.join(ROOT, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="\n") as f:
        json.dump(obj, f, indent=2)
        f.write("\n")
    print("wrote", rel)


# --------------------------------------------------------------------------------------------
# S1 First Light: ~30 m x 18 m ceremonial hall, walkway, columns, raised platform.
# Palette (linear RGB): charcoal, deep red, bronze, restrained amber emissive accents.
# --------------------------------------------------------------------------------------------
FIRST_LIGHT_ID = uid("scene:first-light")
FIRST_LIGHT_CAMERA = uid("First Light Camera")

CHARCOAL = mat((0.030, 0.030, 0.034), roughness=0.85)
STONE = mat((0.050, 0.046, 0.048), roughness=0.70)
DEEP_RED = mat((0.300, 0.022, 0.026), roughness=0.55)
RED_STONE = mat((0.220, 0.030, 0.030), roughness=0.40)
BRONZE = mat((0.480, 0.290, 0.120), roughness=0.32, metallic=1.0)
DARK_BRONZE = mat((0.300, 0.170, 0.070), roughness=0.45, metallic=1.0)
AMBER = mat((0.08, 0.04, 0.01), roughness=0.5, emissive=(1.0, 0.45, 0.08), strength=1.6)
EMBER = mat((0.08, 0.03, 0.01), roughness=0.5, emissive=(1.0, 0.34, 0.05), strength=3.0)
SEAM = mat((0.06, 0.01, 0.01), roughness=0.5, emissive=(1.0, 0.18, 0.04), strength=2.2)

fl = []
fl.append(create("Hall Floor", (0, -0.25, 0), (18, 0.5, 30), shape="box", material=STONE))
fl.append(create("Walkway Runner", (0, 0.02, 1.5), (3.2, 0.04, 23), shape="box", material=DEEP_RED))
for side, x in (("West", -1), ("East", 1)):
    fl.append(create(f"Walkway Trim ({side})", (x * 1.75, 0.03, 1.5), (0.15, 0.06, 23), shape="box", material=BRONZE))
    fl.append(create(f"Walkway Light Strip ({side})", (x * 2.25, 0.012, 1.5), (0.06, 0.024, 23), shape="box", material=AMBER))

COLUMN_Z = [-8.0, -4.0, 0.0, 4.0, 8.0, 12.0]
for side, x in (("West", -5.5), ("East", 5.5)):
    for i, z in enumerate(COLUMN_Z, start=1):
        pivot = f"Column {side[0]}{i}"
        fl.append(create(pivot, (x, 0, z)))  # group pivot: moving it moves the whole column
        fl.append(create(f"{pivot} Base", (0, 0.3, 0), (1.6, 0.6, 1.6), shape="box", material=CHARCOAL, parent=pivot))
        fl.append(create(f"{pivot} Shaft", (0, 4.1, 0), (0.9, 7.0, 0.9), shape="cylinder", material=RED_STONE, parent=pivot))
        fl.append(create(f"{pivot} Band", (0, 2.2, 0), (0.96, 0.12, 0.96), shape="cylinder", material=AMBER, parent=pivot))
        fl.append(create(f"{pivot} Capital", (0, 7.85, 0), (1.5, 0.5, 1.5), shape="box", material=BRONZE, parent=pivot))

# Raised platform (dais) and approach steps.
fl.append(create("Dais", (0, 0.6, -12.25), (12, 1.2, 5.5), shape="box", material=CHARCOAL))
fl.append(create("Dais Step (Lower)", (0, 0.2, -8.55), (7, 0.4, 0.6), shape="box", material=CHARCOAL))
fl.append(create("Dais Step (Upper)", (0, 0.4, -9.2), (7, 0.8, 0.7), shape="box", material=CHARCOAL))
fl.append(create("Dais Edge Trim", (0, 1.23, -9.52), (12.1, 0.06, 0.08), shape="box", material=BRONZE))
fl.append(create("Dais Edge Glow", (0, 1.05, -9.505), (11.6, 0.05, 0.03), shape="box", material=AMBER))
fl.append(create("Altar", (0, 1.8, -12.0), (2.6, 1.2, 1.3), shape="box", material=BRONZE))
fl.append(create("Monolith", (0, 4.2, -14.0), (2.2, 6.0, 0.6), shape="box", material=CHARCOAL))
# Child of a non-uniformly scaled parent: local scale (0.04, 0.9, 1.05) -> world ~(0.09 x 5.4 x 0.63) m.
fl.append(create("Monolith Seam", (0, 0, 0.02), (0.04, 0.9, 1.05), shape="box", material=SEAM, parent="Monolith"))
for side, x in (("West", -4.6), ("East", 4.6)):
    fl.append(create(f"Brazier ({side})", (x, 1.7, -11.2), (0.8, 1.0, 0.8), shape="cylinder", material=DARK_BRONZE))
    fl.append(create(f"Brazier Ember ({side})", (x, 2.24, -11.2), (0.66, 0.08, 0.66), shape="cylinder", material=EMBER))

# Enclosure.
fl.append(create("Wall (West)", (-9.25, 5, 0), (0.5, 10, 30), shape="box", material=CHARCOAL))
fl.append(create("Wall (East)", (9.25, 5, 0), (0.5, 10, 30), shape="box", material=CHARCOAL))
fl.append(create("Wall (North)", (0, 5, -15.25), (19, 10, 0.5), shape="box", material=CHARCOAL))
for side, x in (("West", -5.0), ("East", 5.0)):
    fl.append(create(f"Back Banner ({side})", (x, 5.6, -14.96), (2.2, 7.0, 0.06), shape="box", material=DEEP_RED))
    fl.append(create(f"Back Banner Rod ({side})", (x, 9.2, -14.9), (2.7, 0.12, 0.12), shape="box", material=BRONZE))
for side, x in (("West", -8.97), ("East", 8.97)):
    for i, z in enumerate((-6.0, 2.0, 10.0), start=1):
        fl.append(create(f"Wall Sconce ({side} {i})", (x, 4.2, z), (0.06, 0.9, 0.35), shape="box", material=AMBER))
for i, z in enumerate((-8.0, 2.0, 12.0), start=1):
    fl.append(create(f"Ceiling Beam {i}", (0, 9.6, z), (18.5, 0.5, 0.6), shape="box", material=DARK_BRONZE))

# Key light: warm, from above-right-front, travelling down into the hall.
fl.append(create("Key Light", (0, 12, 0), rot=euler(pitch=-52, yaw=28),
                 extra={"directional_light": {"color": [1.0, 0.86, 0.70], "intensity": 2.4}}))

# The camera entity is created by `scene new`; place it at the entrance looking down the walkway.
fl.append({"op": "transform.set", "entity_id": FIRST_LIGHT_CAMERA,
           "transform": {"position": [0.0, 2.3, 14.6], "rotation": euler(pitch=-3.0), "scale": [1.0, 1.0, 1.0]}})

write("content/source/first_light.environment.json", {
    "ambient_color": [0.42, 0.30, 0.26], "ambient_intensity": 0.22,
    "background_color": [0.004, 0.0035, 0.005], "fog_color": [0.035, 0.020, 0.018],
    "fog_density": 0.018, "exposure": 1.1})
write("content/source/first_light.build.command_batch.json",
      batch("first-light-build", "tool:scene-builder", FIRST_LIGHT_ID, 0, "first-light-build-v1", fl, dry_run=False))

# place_banner example: dry run by default (the protocol default); base revision 1 = First Light as built.
banner_ops = [
    create("Banner (West Wall, Dais)", (-8.96, 5.4, -11.0), (0.06, 6.4, 2.2), shape="box", material=DEEP_RED),
    create("Banner Rod (West Wall, Dais)", (-8.92, 8.75, -11.0), (0.12, 0.12, 2.7), shape="box", material=BRONZE),
    {"op": "material.set", "entity_id": uid("Monolith Seam"), "material": {"emissive_strength": 3.2}},
]
write("examples/place_banner.command_batch.json",
      batch("place-banner-example", "agent:place-banner-example", FIRST_LIGHT_ID, 1, "place-banner-example-0001",
            banner_ops, dry_run=True))

# --------------------------------------------------------------------------------------------
# S0 Axis Lab: asymmetric axis markers, depth overlap, front/back-facing planes, parent/child,
# material swatches. Emissive-dominant materials keep rendered colors predictable for readback.
# --------------------------------------------------------------------------------------------
AXIS_LAB_ID = uid("scene:axis-lab")
AXIS_LAB_CAMERA = uid("Axis Lab Camera")


def glow(rgb, strength=1.0):
    return mat((0, 0, 0), roughness=1.0, emissive=rgb, strength=strength)


al = []
al.append(create("X Marker (long box)", (2.0, 0, 0), (2.0, 0.3, 0.3), shape="box", material=glow((1, 0, 0))))
al.append(create("Y Marker (cylinder)", (0, 2.0, 0), (0.3, 2.0, 0.3), shape="cylinder", material=glow((0, 1, 0))))
al.append(create("Z Marker (flat box)", (0, 0, 1.5), (0.5, 0.15, 1.5), shape="box", material=glow((0, 0, 1))))
al.append(create("Origin Cube", (0, 0, 0), (0.4, 0.4, 0.4), shape="box", material=glow((1, 1, 1))))
# Depth overlap: the near box partly covers the larger far box.
al.append(create("Depth Near Box", (-3.0, -2.0, 1.0), (1.0, 1.0, 1.0), shape="box", material=glow((0, 1, 1))))
al.append(create("Depth Far Box", (-3.5, -2.0, -2.0), (2.5, 2.5, 1.0), shape="box", material=glow((1, 1, 0))))
# Culling: a single-sided plane facing the camera is drawn; one facing away is culled.
al.append(create("Front-Facing Plane", (3.5, -2.0, 0), (1.5, 1, 1.5), rot=euler(pitch=90), shape="plane",
                 material=glow((1, 0.5, 0))))
al.append(create("Back-Facing Plane", (3.5, 2.5, 0), (1.5, 1, 1.5), rot=euler(pitch=-90), shape="plane",
                 material=glow((1, 0, 1))))
# Parent/child: rotated, uniformly scaled pivot with an offset child.
al.append(create("Hierarchy Parent", (-4.0, 2.0, 0), (2.0, 2.0, 2.0), rot=euler(roll=45), shape="box",
                 material=glow((0.2, 0.2, 0.6), 0.5)))
al.append(create("Hierarchy Child", (0.9, 0, 0), (0.2, 0.2, 0.2), shape="box", material=glow((1, 0, 0.5)),
                 parent="Hierarchy Parent"))
# Lit material swatches (bronze, deep red, charcoal) on a floor plane.
al.append(create("Swatch Floor", (0, -3.6, 0), (12, 1, 5), shape="plane", material=mat((0.18, 0.18, 0.18), 0.9)))
for i, (nm, m) in enumerate((("Bronze", BRONZE), ("Deep Red", DEEP_RED), ("Charcoal", CHARCOAL))):
    al.append(create(f"Swatch {nm}", (-1.5 + 1.5 * i, -3.2, 2.0), (0.8, 0.8, 0.8), shape="box", material=m))
al.append(create("Axis Lab Light", (0, 10, 0), rot=euler(pitch=-60, yaw=20),
                 extra={"directional_light": {"color": [1.0, 1.0, 1.0], "intensity": 2.0}}))
al.append({"op": "transform.set", "entity_id": AXIS_LAB_CAMERA,
           "transform": {"position": [0.0, 0.0, 10.0], "rotation": IDENTITY, "scale": [1.0, 1.0, 1.0]}})

write("tests/fixtures/axis_lab.environment.json", {
    "ambient_color": [1.0, 1.0, 1.0], "ambient_intensity": 0.15, "background_color": [0.0, 0.0, 0.0],
    "fog_color": [0.0, 0.0, 0.0], "fog_density": 0.0, "exposure": 1.0})
write("tests/fixtures/axis_lab.build.command_batch.json",
      batch("axis-lab-build", "tool:scene-builder", AXIS_LAB_ID, 0, "axis-lab-build-v1", al, dry_run=False))

print(f"First Light: {len(fl)} operations; Axis Lab: {len(al)} operations")
print("ids:", json.dumps({"first_light_scene": FIRST_LIGHT_ID, "first_light_camera": FIRST_LIGHT_CAMERA,
                          "axis_lab_scene": AXIS_LAB_ID, "axis_lab_camera": AXIS_LAB_CAMERA}))
