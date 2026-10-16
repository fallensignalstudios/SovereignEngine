#!/usr/bin/env python3
"""Validate this design packet's fixtures, not an engine implementation.

Requires Python 3.10+ and jsonschema (with referencing). Schemas resolve locally.
No engine is launched, no graphics are produced, and no scene is modified on disk.
"""
from __future__ import annotations

import copy
import json
import math
import sys
from pathlib import Path
from typing import Any, Callable

try:
    from jsonschema import Draft202012Validator, FormatChecker
    from referencing import Registry, Resource
except ImportError:
    print('This packet checker requires the Python jsonschema package and its referencing dependency.', file=sys.stderr)
    raise SystemExit(2)

ROOT = Path(__file__).resolve().parents[1]


def reject_constant(value: str) -> None:
    raise ValueError(f'Non-finite JSON constant is prohibited: {value}')


def load_json(relative: str) -> Any:
    with (ROOT / relative).open('r', encoding='utf-8') as handle:
        return json.load(handle, parse_constant=reject_constant)


def check_finite(value: Any) -> None:
    if isinstance(value, float) and not math.isfinite(value):
        raise ValueError('Non-finite number')
    if isinstance(value, dict):
        for item in value.values():
            check_finite(item)
    elif isinstance(value, list):
        for item in value:
            check_finite(item)


def check_scene_semantics(scene: dict[str, Any]) -> None:
    """Small fixture checks. Not a substitute for engine semantic validation."""
    check_finite(scene)
    entities = scene['entities']
    by_id = {entity['id']: entity for entity in entities}
    if len(by_id) != len(entities):
        raise ValueError('Duplicate entity ID')
    active = by_id.get(scene['active_camera_id'])
    if active is None or 'Camera' not in active['components']:
        raise ValueError('Active camera is missing or does not have a Camera component')
    parents: dict[str, str | None] = {}
    for entity in entities:
        transform = entity['components']['Transform']
        rotation = transform['rotation_xyzw']
        if not math.isclose(sum(v * v for v in rotation), 1.0, abs_tol=1e-6):
            raise ValueError(f"Quaternion is not normalized: {entity['id']}")
        parent = transform['parent_id']
        if parent is not None and parent not in by_id:
            raise ValueError(f'Missing parent: {parent}')
        parents[entity['id']] = parent
        camera = entity['components'].get('Camera')
        if camera and camera['far_m'] <= camera['near_m']:
            raise ValueError('Camera far plane must be greater than near plane')
    # Iterative walks keep this fixture check independent of Python recursion limits.
    for entity_id in parents:
        seen: set[str] = set()
        cursor: str | None = entity_id
        while cursor is not None:
            if cursor in seen:
                raise ValueError('Transform hierarchy contains a cycle')
            seen.add(cursor)
            cursor = parents[cursor]


def check_batch_fixture(batch: dict[str, Any], scene: dict[str, Any]) -> None:
    """Check this request's references using an in-memory proposed copy only.

    This does not implement commits, locks, undo, durable idempotency, permissions,
    or dry-run execution. Nothing is written and this is not engine test evidence.
    """
    check_finite(batch)
    if batch['scene_id'] != scene['scene_id']:
        raise ValueError('Scene ID mismatch')
    if batch['base_revision'] != scene['revision']:
        raise ValueError('Stale base revision in fixture')
    proposed = copy.deepcopy(scene)
    by_id = {entity['id']: entity for entity in proposed['entities']}
    for operation in batch['operations']:
        kind = operation['op']
        if kind == 'entity.create':
            entity = copy.deepcopy(operation['entity'])
            if entity['id'] in by_id:
                raise ValueError('Create uses an existing entity ID')
            by_id[entity['id']] = entity
            continue
        entity_id = operation['entity_id']
        if entity_id not in by_id:
            raise ValueError('Operation targets an unknown entity')
        if kind == 'entity.rename':
            by_id[entity_id]['name'] = operation['name']
        elif kind == 'transform.set':
            by_id[entity_id]['components']['Transform'] = copy.deepcopy(operation['transform'])
        elif kind == 'material.set':
            if 'PrimitiveMesh' not in by_id[entity_id]['components']:
                raise ValueError('Material operation targets an entity without geometry')
            by_id[entity_id]['components']['MaterialParameters'] = copy.deepcopy(operation['material'])
        elif kind == 'entity.delete':
            del by_id[entity_id]
        else:
            raise ValueError(f'Unsupported fixture operation: {kind}')
    proposed['entities'] = sorted(by_id.values(), key=lambda item: item['id'])
    scene_validator.validate(proposed)
    check_scene_semantics(proposed)


scene_schema = load_json('schemas/scene.schema.json')
batch_schema = load_json('schemas/command_batch.schema.json')
for schema in (scene_schema, batch_schema):
    Draft202012Validator.check_schema(schema)
registry = Registry().with_resources([
    (scene_schema['$id'], Resource.from_contents(scene_schema)),
    (batch_schema['$id'], Resource.from_contents(batch_schema)),
])
scene_validator = Draft202012Validator(scene_schema, registry=registry, format_checker=FormatChecker())
batch_validator = Draft202012Validator(batch_schema, registry=registry, format_checker=FormatChecker())
scene = load_json('examples/first_light.scene.json')
batch = load_json('examples/place_banner.command_batch.json')


def validate_scene(value: dict[str, Any]) -> None:
    scene_validator.validate(value)
    check_scene_semantics(value)


def validate_batch(value: dict[str, Any]) -> None:
    batch_validator.validate(value)
    check_batch_fixture(value, scene)


passed = 0


def expect_pass(name: str, action: Callable[[], None]) -> None:
    global passed
    action()
    print(f'PASS | {name}')
    passed += 1


def expect_rejection(name: str, action: Callable[[], None]) -> None:
    global passed
    from jsonschema.exceptions import ValidationError
    try:
        action()
    except (ValueError, ValidationError):
        print(f'PASS | Rejects {name}')
        passed += 1
        return
    raise AssertionError(f'Expected rejection: {name}')


def changed(original: dict[str, Any], mutate: Callable[[dict[str, Any]], Any]) -> dict[str, Any]:
    value = copy.deepcopy(original)
    mutate(value)
    return value


def validate_schema_definitions() -> None:
    for definition in (scene_schema, batch_schema):
        Draft202012Validator.check_schema(definition)


def main() -> int:
    print('SOVEREIGN ENGINE DESIGN PACKET VALIDATION')
    print('Scope: JSON schemas and example fixtures only. No native engine/GPU tests were run.\n')
    expect_pass('Both schemas satisfy Draft 2020-12', validate_schema_definitions)
    expect_pass('Original First Light scene: structure and limited semantics', lambda: validate_scene(scene))
    expect_pass('Example command batch: structure and reference consistency', lambda: validate_batch(batch))
    expect_pass('Example request is explicitly a dry run', lambda: assert_dry_run(batch))

    scene_cases = [
        ('unknown scene field', lambda x: x.update(unknown=True)),
        ('duplicate entity UUID', lambda x: x['entities'].append(copy.deepcopy(x['entities'][0]))),
        ('missing active camera', lambda x: x.update(active_camera_id='10000000-0000-4000-8000-000000999999')),
        ('self-parent cycle', lambda x: x['entities'][0]['components']['Transform'].update(parent_id=x['entities'][0]['id'])),
        ('unnormalized quaternion', lambda x: x['entities'][0]['components']['Transform'].update(rotation_xyzw=[0, 0, 0, 0])),
        ('non-finite numeric value', lambda x: x['entities'][0]['components']['Transform']['position'].__setitem__(0, float('nan'))),
    ]
    for name, mutate in scene_cases:
        fixture = changed(scene, mutate)
        expect_rejection(name, lambda fixture=fixture: validate_scene(fixture))

    batch_cases = [
        ('unknown command operation', lambda x: x['operations'][0].update(op='shell.execute')),
        ('unknown command field', lambda x: x.update(shell='not-allowed')),
        ('stale base revision', lambda x: x.update(base_revision=9)),
        ('missing per-operation target', lambda x: x['operations'][1].pop('entity_id')),
        ('creating duplicate entity ID', lambda x: x['operations'][0]['entity'].update(id=scene['entities'][0]['id'])),
        ('deleting the active camera', lambda x: x.update(operations=[{'op': 'entity.delete', 'entity_id': scene['active_camera_id']}]))
    ]
    for name, mutate in batch_cases:
        fixture = changed(batch, mutate)
        expect_rejection(name, lambda fixture=fixture: validate_batch(fixture))

    print(f'\n{passed} packet checks passed.')
    print('Not evaluated: C++ compilation, graphics, command persistence, locking, undo/redo, performance, or gameplay.')
    return 0


def assert_dry_run(value: dict[str, Any]) -> None:
    if value['dry_run'] is not True:
        raise AssertionError('Sample must remain a dry run')


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f'FAIL | {error}', file=sys.stderr)
        raise SystemExit(1)
