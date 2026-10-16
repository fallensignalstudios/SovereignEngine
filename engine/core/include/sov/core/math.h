#pragma once

#include <array>
#include <cmath>
#include <cstddef>

// Engine math conventions (docs/architecture/conventions.md):
//   - right-handed basis, +X right, +Y up, +Z toward the viewer; cameras look along local -Z
//   - distances in meters, angles in radians at interfaces (editor displays degrees)
//   - column vectors: v' = M * v; transforms compose as M = T * R * S
//   - Mat4 storage is column-major: element (row r, column c) is m[c * 4 + r]
//   - quaternions are unit, stored/serialized as [x, y, z, w]
//   - CPU math is double precision; GPU packers convert to float32
namespace sov {

constexpr double kPi = 3.14159265358979323846;
constexpr double DegToRad(double d) { return d * (kPi / 180.0); }
constexpr double RadToDeg(double r) { return r * (180.0 / kPi); }

struct Vec2 {
  double x = 0, y = 0;
};

struct Vec3 {
  double x = 0, y = 0, z = 0;
  constexpr Vec3() = default;
  constexpr Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
  constexpr Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  constexpr Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  constexpr Vec3 operator-() const { return {-x, -y, -z}; }
  constexpr Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
  constexpr Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
  Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
  constexpr bool operator==(const Vec3&) const = default;
};

struct Vec4 {
  double x = 0, y = 0, z = 0, w = 0;
  constexpr bool operator==(const Vec4&) const = default;
};

constexpr double Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3 Cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double Length(const Vec3& v) { return std::sqrt(Dot(v, v)); }
inline Vec3 Normalize(const Vec3& v) {
  const double len = Length(v);
  return len > 0 ? v / len : Vec3{};
}
constexpr Vec3 Hadamard(const Vec3& a, const Vec3& b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }

struct Quat {
  double x = 0, y = 0, z = 0, w = 1;
  constexpr bool operator==(const Quat&) const = default;

  static Quat Identity() { return {}; }
  // Rotation of `radians` about `axis` (right-hand rule; axis need not be normalized).
  static Quat FromAxisAngle(const Vec3& axis, double radians);
};

Quat operator*(const Quat& a, const Quat& b);  // a * b applies b first, then a
Quat Normalize(const Quat& q);
Quat Conjugate(const Quat& q);
double Length(const Quat& q);
Vec3 Rotate(const Quat& q, const Vec3& v);

// Editor display convention: Euler angles in degrees applied as yaw (about +Y), then pitch
// (about local +X), then roll (about local +Z): q = qY(yaw) * qX(pitch) * qZ(roll).
struct EulerDegrees {
  double pitch = 0;  // X
  double yaw = 0;    // Y
  double roll = 0;   // Z
};
Quat QuatFromEulerDegrees(const EulerDegrees& e);
EulerDegrees EulerDegreesFromQuat(const Quat& q);

struct Mat4 {
  // Column-major: m[c * 4 + r]
  std::array<double, 16> m{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

  double& at(int row, int col) { return m[static_cast<std::size_t>(col * 4 + row)]; }
  double at(int row, int col) const { return m[static_cast<std::size_t>(col * 4 + row)]; }

  static Mat4 Identity() { return {}; }
  static Mat4 Translation(const Vec3& t);
  static Mat4 Rotation(const Quat& q);
  static Mat4 Scale(const Vec3& s);
  // M = T * R * S
  static Mat4 Compose(const Vec3& translation, const Quat& rotation, const Vec3& scale);

  bool operator==(const Mat4&) const = default;
};

Mat4 operator*(const Mat4& a, const Mat4& b);
Vec4 operator*(const Mat4& a, const Vec4& v);
Vec3 TransformPoint(const Mat4& m, const Vec3& p);      // w = 1
Vec3 TransformDirection(const Mat4& m, const Vec3& d);  // w = 0
Mat4 Transpose(const Mat4& m);
// General 4x4 inverse; returns false (and leaves out untouched) when singular.
bool Inverse(const Mat4& m, Mat4& out);
// Inverse-transpose of the upper 3x3, embedded in a Mat4 (for normals under non-uniform scale).
Mat4 NormalMatrix(const Mat4& model);
Vec3 GetTranslation(const Mat4& m);

// Vulkan clip space for a camera looking along -Z:
//   - reversed-Z: view distance `near_m` maps to depth 1, `far_m` maps to depth 0
//     (far_m = +infinity is allowed and maps far to 0 asymptotically)
//   - Y is flipped in the projection matrix (clip y = -f * y_view) so +Y up in view space appears
//     at the top of the framebuffer with a standard positive-height viewport.
// With this matrix the front face is VK_FRONT_FACE_COUNTER_CLOCKWISE for triangles wound
// counter-clockwise when viewed from their front side.
Mat4 PerspectiveReversedZ(double vertical_fov_radians, double aspect, double near_m, double far_m);

// Projects a world point to normalized device coordinates (x,y in [-1,1] with y down; z = depth).
// Returns false if the point is behind the camera (w <= 0).
bool ProjectToNdc(const Mat4& view_projection, const Vec3& world, Vec3& ndc);

// NDC (y down) to pixel coordinates (origin top-left, +y down), pixel centers at +0.5.
Vec2 NdcToPixel(const Vec3& ndc, double width, double height);

bool NearlyEqual(double a, double b, double eps = 1e-9);
bool NearlyEqual(const Vec3& a, const Vec3& b, double eps = 1e-9);
bool NearlyEqual(const Mat4& a, const Mat4& b, double eps = 1e-9);
// Same rotation (q and -q are equivalent).
bool SameRotation(const Quat& a, const Quat& b, double eps = 1e-9);

}  // namespace sov
