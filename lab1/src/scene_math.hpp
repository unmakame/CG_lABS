#pragma once
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Preserve elapsed time at low FPS. Suspended windows reset their clock in
// main.
inline double frameSeconds(double dt) {
  return std::isfinite(dt) && dt > 0 ? dt : 0;
}
inline void advancePhase(double &phase, double dt, double speed, bool playing) {
  if (playing && std::isfinite(speed) && speed >= 0)
    phase += frameSeconds(dt) * speed;
}
inline glm::mat4 sphereTransform(glm::vec3 position, glm::vec3 degrees,
                                 glm::vec3 scale, double phase, size_t index,
                                 float radius) {
  double base = double(index) * 2.0943951023931953;
  double a = phase + base;
  position += radius * glm::vec3(std::cos(a) - std::cos(base),
                                 .3 * (std::sin(2 * a) - std::sin(2 * base)),
                                 std::sin(a) - std::sin(base));
  auto m = glm::translate(glm::mat4(1), position);
  m = glm::rotate(m, glm::radians(degrees.x), glm::vec3(1, 0, 0));
  m = glm::rotate(m,
                  glm::radians(degrees.y) +
                      float(std::remainder(phase, 6.283185307179586)),
                  glm::vec3(0, 1, 0));
  m = glm::rotate(m, glm::radians(degrees.z), glm::vec3(0, 0, 1));
  return glm::scale(m, glm::max(scale, glm::vec3(.01f)));
}
