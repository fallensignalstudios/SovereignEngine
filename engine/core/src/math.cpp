#include "sov/core/math.h"

#include <algorithm>
#include <limits>

namespace sov {

Quat Quat::FromAxisAngle(const Vec3& axis, double radians) {
  const Vec3 n = Normalize(axis);
  const double s = std::sin(radians * 0.5);
  return {n.x * s, n.y * s, n.z * s, std::cos(radians * 0.5)};
}

Quat operator*(const Quat& a, const Quat& b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
          a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
          a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

double Length(const Quat& q) { return std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w); }

Quat Normalize(const Quat& q) {
  const double len = Length(q);
  if (len <= 0) return {};
  return {q.x / len, q.y / len, q.z / len, q.w / len};
}

Quat Conjugate(const Quat& q) { return {-q.x, -q.y, -q.z, q.w}; }

Vec3 Rotate(const Quat& q, const Vec3& v) {
  // v' = v + 2w(u x v) + 2 u x (u x v), u = (x, y, z)
  const Vec3 u{q.x, q.y, q.z};
  const Vec3 t = Cross(u, v) * 2.0;
  return v + t * q.w + Cross(u, t);
}

Quat QuatFromEulerDegrees(const EulerDegrees& e) {
  const Quat qy = Quat::FromAxisAngle({0, 1, 0}, DegToRad(e.yaw));
  const Quat qx = Quat::FromAxisAngle({1, 0, 0}, DegToRad(e.pitch));
  const Quat qz = Quat::FromAxisAngle({0, 0, 1}, DegToRad(e.roll));
  return Normalize(qy * qx * qz);
}

EulerDegrees EulerDegreesFromQuat(const Quat& q_in) {
  // Rotation matrix R = Ry(yaw) Rx(pitch) Rz(roll). Elements used:
  //   R[1][2] = -sin(pitch)
  //   R[0][2] = cos(pitch) sin(yaw),  R[2][2] = cos(pitch) cos(yaw)
  //   R[1][0] = cos(pitch) sin(roll), R[1][1] = cos(pitch) cos(roll)
  const Mat4 r = Mat4::Rotation(Normalize(q_in));
  EulerDegrees e;
  const double sp = std::clamp(-r.at(1, 2), -1.0, 1.0);
  e.pitch = RadToDeg(std::asin(sp));
  if (std::abs(sp) < 0.9999999) {
    e.yaw = RadToDeg(std::atan2(r.at(0, 2), r.at(2, 2)));
    e.roll = RadToDeg(std::atan2(r.at(1, 0), r.at(1, 1)));
  } else {
    // Gimbal lock: attribute all remaining rotation to yaw.
    e.roll = 0;
    e.yaw = RadToDeg(std::atan2(-r.at(2, 0), r.at(0, 0)));
  }
  return e;
}

Mat4 Mat4::Translation(const Vec3& t) {
  Mat4 r;
  r.at(0, 3) = t.x;
  r.at(1, 3) = t.y;
  r.at(2, 3) = t.z;
  return r;
}

Mat4 Mat4::Rotation(const Quat& q) {
  const double xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
  const double xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
  const double wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
  Mat4 r;
  r.at(0, 0) = 1 - 2 * (yy + zz);
  r.at(0, 1) = 2 * (xy - wz);
  r.at(0, 2) = 2 * (xz + wy);
  r.at(1, 0) = 2 * (xy + wz);
  r.at(1, 1) = 1 - 2 * (xx + zz);
  r.at(1, 2) = 2 * (yz - wx);
  r.at(2, 0) = 2 * (xz - wy);
  r.at(2, 1) = 2 * (yz + wx);
  r.at(2, 2) = 1 - 2 * (xx + yy);
  return r;
}

Mat4 Mat4::Scale(const Vec3& s) {
  Mat4 r;
  r.at(0, 0) = s.x;
  r.at(1, 1) = s.y;
  r.at(2, 2) = s.z;
  return r;
}

Mat4 Mat4::Compose(const Vec3& translation, const Quat& rotation, const Vec3& scale) {
  return Translation(translation) * Rotation(rotation) * Scale(scale);
}

Mat4 operator*(const Mat4& a, const Mat4& b) {
  Mat4 r;
  for (int c = 0; c < 4; ++c) {
    for (int row = 0; row < 4; ++row) {
      double sum = 0;
      for (int k = 0; k < 4; ++k) sum += a.at(row, k) * b.at(k, c);
      r.at(row, c) = sum;
    }
  }
  return r;
}

Vec4 operator*(const Mat4& a, const Vec4& v) {
  return {a.at(0, 0) * v.x + a.at(0, 1) * v.y + a.at(0, 2) * v.z + a.at(0, 3) * v.w,
          a.at(1, 0) * v.x + a.at(1, 1) * v.y + a.at(1, 2) * v.z + a.at(1, 3) * v.w,
          a.at(2, 0) * v.x + a.at(2, 1) * v.y + a.at(2, 2) * v.z + a.at(2, 3) * v.w,
          a.at(3, 0) * v.x + a.at(3, 1) * v.y + a.at(3, 2) * v.z + a.at(3, 3) * v.w};
}

Vec3 TransformPoint(const Mat4& m, const Vec3& p) {
  const Vec4 r = m * Vec4{p.x, p.y, p.z, 1.0};
  return {r.x, r.y, r.z};
}

Vec3 TransformDirection(const Mat4& m, const Vec3& d) {
  const Vec4 r = m * Vec4{d.x, d.y, d.z, 0.0};
  return {r.x, r.y, r.z};
}

Mat4 Transpose(const Mat4& m) {
  Mat4 r;
  for (int row = 0; row < 4; ++row) {
    for (int c = 0; c < 4; ++c) r.at(row, c) = m.at(c, row);
  }
  return r;
}

bool Inverse(const Mat4& mat, Mat4& out) {
  const auto& m = mat.m;
  std::array<double, 16> inv{};
  inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
           m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
  inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
           m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
  inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
           m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
  inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
            m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
  inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
           m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
  inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
           m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
  inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
           m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
  inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
            m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
  inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
           m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
  inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
           m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
  inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
            m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
  inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
            m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
  inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
           m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
  inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
           m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
  inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
            m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
  inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
            m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
  const double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
  if (std::abs(det) < 1e-300) return false;
  const double inv_det = 1.0 / det;
  for (std::size_t i = 0; i < 16; ++i) out.m[i] = inv[i] * inv_det;
  return true;
}

Mat4 NormalMatrix(const Mat4& model) {
  Mat4 upper;
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) upper.at(r, c) = model.at(r, c);
  }
  Mat4 inv;
  if (!Inverse(upper, inv)) return Mat4::Identity();
  Mat4 n = Transpose(inv);
  n.at(0, 3) = n.at(1, 3) = n.at(2, 3) = 0;
  n.at(3, 0) = n.at(3, 1) = n.at(3, 2) = 0;
  n.at(3, 3) = 1;
  return n;
}

Vec3 GetTranslation(const Mat4& m) { return {m.at(0, 3), m.at(1, 3), m.at(2, 3)}; }

Mat4 PerspectiveReversedZ(double fovy, double aspect, double n, double f) {
  const double focal = 1.0 / std::tan(fovy * 0.5);
  Mat4 p;
  p.m.fill(0.0);
  p.at(0, 0) = focal / aspect;
  p.at(1, 1) = -focal;  // Vulkan framebuffer Y points down; flip here (and only here)
  if (std::isinf(f)) {
    p.at(2, 2) = 0.0;
    p.at(2, 3) = n;
  } else {
    p.at(2, 2) = n / (f - n);
    p.at(2, 3) = n * f / (f - n);
  }
  p.at(3, 2) = -1.0;
  return p;
}

bool ProjectToNdc(const Mat4& vp, const Vec3& world, Vec3& ndc) {
  const Vec4 clip = vp * Vec4{world.x, world.y, world.z, 1.0};
  if (clip.w <= 0) return false;
  ndc = {clip.x / clip.w, clip.y / clip.w, clip.z / clip.w};
  return true;
}

Vec2 NdcToPixel(const Vec3& ndc, double width, double height) {
  return {(ndc.x * 0.5 + 0.5) * width, (ndc.y * 0.5 + 0.5) * height};
}

bool NearlyEqual(double a, double b, double eps) { return std::abs(a - b) <= eps; }

bool NearlyEqual(const Vec3& a, const Vec3& b, double eps) {
  return NearlyEqual(a.x, b.x, eps) && NearlyEqual(a.y, b.y, eps) && NearlyEqual(a.z, b.z, eps);
}

bool NearlyEqual(const Mat4& a, const Mat4& b, double eps) {
  for (std::size_t i = 0; i < 16; ++i) {
    if (!NearlyEqual(a.m[i], b.m[i], eps)) return false;
  }
  return true;
}

bool SameRotation(const Quat& a, const Quat& b, double eps) {
  const double d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  return std::abs(std::abs(d) - 1.0) <= eps;
}

}  // namespace sov
