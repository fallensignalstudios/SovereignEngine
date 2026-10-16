#pragma once

#include "sov/world/scene_document.h"

// In-code scene fixtures for headless tests.
namespace sov::fixtures {

inline const Uuid kSceneId = *Uuid::Parse("5c0e0000-0000-4000-8000-000000000001");
inline const Uuid kCameraId = *Uuid::Parse("5c0e0000-0000-4000-8000-0000000000c0");
inline const Uuid kParentId = *Uuid::Parse("5c0e0000-0000-4000-8000-0000000000a1");
inline const Uuid kChildId = *Uuid::Parse("5c0e0000-0000-4000-8000-0000000000a2");
inline const Uuid kLightId = *Uuid::Parse("5c0e0000-0000-4000-8000-0000000000b1");

// Camera + light + parent box + child cylinder. Revision 3.
inline scene::SceneDocument SmallScene() {
  using namespace scene;
  SceneDocument doc = MakeNewScene(kSceneId, "Small Test Scene", kCameraId);
  doc.revision = 3;

  EntityData parent;
  parent.id = kParentId;
  parent.name = "Parent Box";
  parent.transform.position = {1, 0.5, -2};
  parent.transform.rotation = Quat::FromAxisAngle({0, 1, 0}, kPi / 4);
  parent.transform.scale = {2, 1, 1};
  parent.primitive_mesh = PrimitiveMeshComponent{PrimitiveShape::Box};
  parent.material = MaterialComponent{{0.6, 0.4, 0.2}, 0.4, 1.0, {0, 0, 0}, 0.0};
  doc.Insert(parent);

  EntityData child;
  child.id = kChildId;
  child.name = "Child Cylinder";
  child.transform.parent = kParentId;
  child.transform.position = {0, 1.5, 0};
  child.primitive_mesh = PrimitiveMeshComponent{PrimitiveShape::Cylinder};
  child.material = MaterialComponent{{0.3, 0.05, 0.05}, 0.7, 0.0, {1, 0.5, 0.1}, 2.0};
  doc.Insert(child);

  EntityData light;
  light.id = kLightId;
  light.name = "Key Light";
  light.transform.rotation = Quat::FromAxisAngle({1, 0, 0}, -kPi / 3);
  light.directional_light = DirectionalLightComponent{{1, 0.9, 0.8}, 3.0};
  doc.Insert(light);
  return doc;
}

}  // namespace sov::fixtures
