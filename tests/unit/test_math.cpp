#include <limits>

#include "sov/core/math.h"
#include "sov_test.h"

using namespace sov;

SOV_TEST(math, right_handed_y_up_basis) {
  const Vec3 x{1, 0, 0}, y{0, 1, 0}, z{0, 0, 1};
  SOV_CHECK(Cross(x, y) == z);  // right-handed
  SOV_CHECK(Cross(y, z) == x);
  // +90 deg about +Y turns +X toward -Z (right-hand rule).
  const Vec3 r = Rotate(Quat::FromAxisAngle(y, kPi / 2), x);
  SOV_CHECK(NearlyEqual(r, Vec3{0, 0, -1}, 1e-12));
}

SOV_TEST(math, trs_compose_order_is_t_r_s) {
  const Vec3 t{10, 0, 0};
  const Quat r = Quat::FromAxisAngle({0, 0, 1}, kPi / 2);
  const Vec3 s{2, 1, 1};
  const Mat4 m = Mat4::Compose(t, r, s);
  // Point (1,0,0): scale -> (2,0,0); rotate 90 about Z -> (0,2,0); translate -> (10,2,0).
  SOV_CHECK(NearlyEqual(TransformPoint(m, {1, 0, 0}), Vec3{10, 2, 0}, 1e-12));
  SOV_CHECK(NearlyEqual(m, Mat4::Translation(t) * Mat4::Rotation(r) * Mat4::Scale(s), 1e-12));
  // Column-major storage: translation lives in elements 12..14.
  SOV_CHECK_EQ(m.m[12], 10.0);
  SOV_CHECK_EQ(m.m[13], 0.0);
}

SOV_TEST(math, inverse_and_normal_matrix) {
  const Mat4 m = Mat4::Compose({1, 2, 3}, Normalize(Quat{0.2, 0.3, 0.1, 0.9}), {2, 3, 0.5});
  Mat4 inv;
  SOV_REQUIRE(Inverse(m, inv));
  SOV_CHECK(NearlyEqual(m * inv, Mat4::Identity(), 1e-12));
  SOV_CHECK(!Inverse(Mat4::Scale({1, 0, 1}), inv));
  // Normals stay perpendicular to surfaces under non-uniform scale.
  const Mat4 ns = Mat4::Scale({4, 1, 1});
  const Vec3 tangent = TransformDirection(ns, Normalize(Vec3{1, 1, 0}));
  const Vec3 normal = TransformDirection(NormalMatrix(ns), Normalize(Vec3{1, -1, 0}));
  SOV_CHECK(NearlyEqual(Dot(tangent, normal), 0.0, 1e-12));
}

SOV_TEST(math, quaternion_serialized_order_and_euler_display) {
  const Quat q = Quat::FromAxisAngle({0, 1, 0}, DegToRad(30));
  SOV_CHECK(NearlyEqual(q.y, std::sin(DegToRad(15)), 1e-12));  // [x, y, z, w]: y holds the axis term
  SOV_CHECK(NearlyEqual(q.w, std::cos(DegToRad(15)), 1e-12));
  const EulerDegrees e{20, -35, 10};
  const EulerDegrees back = EulerDegreesFromQuat(QuatFromEulerDegrees(e));
  SOV_CHECK(NearlyEqual(back.pitch, e.pitch, 1e-9));
  SOV_CHECK(NearlyEqual(back.yaw, e.yaw, 1e-9));
  SOV_CHECK(NearlyEqual(back.roll, e.roll, 1e-9));
  // yaw then pitch: camera at yaw 90 looks along -X (local -Z rotated +90 about Y).
  const Vec3 fwd = Rotate(QuatFromEulerDegrees({0, 90, 0}), {0, 0, -1});
  SOV_CHECK(NearlyEqual(fwd, Vec3{-1, 0, 0}, 1e-12));
}

SOV_TEST(math, reversed_z_projection_maps_near_to_one_far_to_zero) {
  const double n = 0.1, f = 100.0;
  const Mat4 p = PerspectiveReversedZ(DegToRad(60), 16.0 / 9.0, n, f);
  Vec3 ndc;
  SOV_REQUIRE(ProjectToNdc(p, {0, 0, -n}, ndc));
  SOV_CHECK(NearlyEqual(ndc.z, 1.0, 1e-12));
  SOV_REQUIRE(ProjectToNdc(p, {0, 0, -f}, ndc));
  SOV_CHECK(NearlyEqual(ndc.z, 0.0, 1e-12));
  // Depth decreases monotonically with distance (compare op: GREATER_OR_EQUAL keeps nearer).
  Vec3 a, b;
  SOV_REQUIRE(ProjectToNdc(p, {0, 0, -1}, a));
  SOV_REQUIRE(ProjectToNdc(p, {0, 0, -2}, b));
  SOV_CHECK(a.z > b.z);
  // Points behind the camera are rejected.
  SOV_CHECK(!ProjectToNdc(p, {0, 0, 1}, ndc));
  // Infinite far plane: depth = near / distance.
  const Mat4 pinf = PerspectiveReversedZ(DegToRad(60), 1.0, n, std::numeric_limits<double>::infinity());
  SOV_REQUIRE(ProjectToNdc(pinf, {0, 0, -10}, ndc));
  SOV_CHECK(NearlyEqual(ndc.z, n / 10.0, 1e-12));
}

SOV_TEST(math, vulkan_y_flip_puts_up_at_top_of_framebuffer) {
  const Mat4 p = PerspectiveReversedZ(DegToRad(90), 1.0, 0.1, 100.0);
  Vec3 ndc;
  SOV_REQUIRE(ProjectToNdc(p, {0, 1, -2}, ndc));  // above the view axis
  SOV_CHECK(ndc.y < 0);                           // Vulkan NDC: -1 is the top row
  const Vec2 px = NdcToPixel(ndc, 100, 100);
  SOV_CHECK(px.y < 50);
  SOV_REQUIRE(ProjectToNdc(p, {1, 0, -2}, ndc));  // to the right
  SOV_CHECK(ndc.x > 0);
  // 90 deg FOV: a point at 45 deg up lands exactly on the top edge.
  SOV_REQUIRE(ProjectToNdc(p, {0, 2, -2}, ndc));
  SOV_CHECK(NearlyEqual(ndc.y, -1.0, 1e-12));
}

SOV_TEST(math, front_face_winding_is_counter_clockwise_on_screen) {
  // A triangle wound CCW when seen from +Z (its front), viewed by a camera at the origin looking -Z.
  const Mat4 p = PerspectiveReversedZ(DegToRad(60), 1.0, 0.1, 100.0);
  const Vec3 tri[3] = {{-1, -1, -5}, {1, -1, -5}, {0, 1, -5}};
  Vec2 px[3];
  for (int i = 0; i < 3; ++i) {
    Vec3 ndc;
    SOV_REQUIRE(ProjectToNdc(p, tri[i], ndc));
    px[i] = NdcToPixel(ndc, 100, 100);
  }
  // Vulkan's signed area in framebuffer coordinates: a = -1/2 * sum(x_i*y_{i+1} - x_{i+1}*y_i).
  double sum = 0;
  for (int i = 0; i < 3; ++i) {
    const Vec2& p0 = px[i];
    const Vec2& p1 = px[(i + 1) % 3];
    sum += p0.x * p1.y - p1.x * p0.y;
  }
  const double area = -0.5 * sum;
  SOV_CHECK(area > 0);  // positive area => front-facing under VK_FRONT_FACE_COUNTER_CLOCKWISE
}
