#include "scene_math.hpp"
#include "sphere_mesh.hpp"
#include <cassert>
#include <iostream>
#include <limits>
static bool same(glm::mat4 a, glm::mat4 b) {
  for (int c = 0; c < 4; c++)
    for (int r = 0; r < 4; r++)
      if (std::abs(a[c][r] - b[c][r]) > 1e-5)
        return false;
  return true;
}
int main() {
  for (int fps : {10, 20, 30, 60, 144}) {
    double phase = 0;
    for (int i = 0; i < fps * 10; i++)
      advancePhase(phase, 1.0 / fps, .8, true);
    assert(std::abs(phase - 8) < 1e-10);
  }
  double phase = 1.2;
  auto before =
      sphereTransform({0, 0, 0}, {10, 20, 30}, {2, .5, 1}, phase, 1, .8);
  advancePhase(phase, 5, 1, false);
  assert(phase == 1.2);
  assert(same(before, sphereTransform({0, 0, 0}, {10, 20, 30}, {2, .5, 1},
                                      phase, 1, .8)));
  advancePhase(phase, 5, 0, true);
  assert(phase == 1.2);
  advancePhase(phase, .1, 2, true);
  assert(std::abs(phase - 1.4) < 1e-12);
  advancePhase(phase, std::numeric_limits<double>::quiet_NaN(), 1, true);
  assert(std::abs(phase - 1.4) < 1e-12);
  advancePhase(phase, -1, 1, true);
  assert(std::abs(phase - 1.4) < 1e-12);
  auto mesh = makeUvSphere(12, 8);
  assert(mesh.vertices.size() == 117);
  assert(mesh.indices.size() == 504);
  auto m = sphereTransform({3, -1, 2}, {15, 35, 70}, {2, .5, 1.3}, 1.3, 0, .8);
  auto a = glm::mat3(m);
  auto normal = glm::transpose(glm::inverse(a));
  for (auto v : mesh.vertices) {
    glm::vec3 n(v.normal[0], v.normal[1], v.normal[2]);
    assert(std::abs(glm::length(n) - 1) < 1e-5);
    auto t = glm::cross(n, glm::abs(n.y) < .9f ? glm::vec3(0, 1, 0)
                                               : glm::vec3(1, 0, 0));
    assert(std::abs(glm::dot(normal * n, a * t)) < 1e-5);
    auto p = glm::vec3(
        m * glm::vec4(v.position[0], v.position[1], v.position[2], 1));
    auto tip = p + .18f * glm::normalize(normal * n);
    assert(std::abs(glm::length(tip - p) - .18f) < 1e-5);
  }
  std::cout << "PASS: FPS independence, pause, zero/change speed, invalid dt, "
               "sphere, transformed normals\n";
}
