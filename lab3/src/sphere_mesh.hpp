#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

struct Vertex {
  float position[3];
  float normal[3];
  float uv[2];
  float color[3];
};
struct MeshData {
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
};

// UV sphere: normals are analytic (unit radial vectors), with seam vertices
// duplicated so texture coordinates remain continuous across the meridian.
inline MeshData makeUvSphere(std::uint32_t longitudeSegments = 32,
                             std::uint32_t latitudeSegments = 16) {
  constexpr float pi = 3.14159265358979323846f;
  MeshData mesh;
  if (longitudeSegments < 3 || latitudeSegments < 2)
    return mesh;
  mesh.vertices.reserve((longitudeSegments + 1) * (latitudeSegments + 1));
  for (std::uint32_t y = 0; y <= latitudeSegments; ++y) {
    const float v = static_cast<float>(y) / latitudeSegments;
    const float theta = v * pi;
    const float st = std::sin(theta), ct = std::cos(theta);
    for (std::uint32_t x = 0; x <= longitudeSegments; ++x) {
      const float u = static_cast<float>(x) / longitudeSegments;
      const float phi = u * 2.0f * pi;
      const float nx = st * std::cos(phi);
      const float ny = ct;
      const float nz = st * std::sin(phi);
      Vertex vertex{};
      vertex.position[0] = vertex.normal[0] = nx;
      vertex.position[1] = vertex.normal[1] = ny;
      vertex.position[2] = vertex.normal[2] = nz;
      vertex.uv[0] = u;
      vertex.uv[1] = 1.0f - v;
      // Procedural latitude/longitude color variation used by Lab 1.
      vertex.color[0] = 0.35f + 0.65f * (0.5f + 0.5f * nx);
      vertex.color[1] = 0.35f + 0.65f * (0.5f + 0.5f * ny);
      vertex.color[2] = 0.35f + 0.65f * (0.5f + 0.5f * nz);
      mesh.vertices.push_back(vertex);
    }
  }
  for (std::uint32_t y = 0; y < latitudeSegments; ++y) {
    for (std::uint32_t x = 0; x < longitudeSegments; ++x) {
      const std::uint32_t a = y * (longitudeSegments + 1) + x;
      const std::uint32_t b = a + longitudeSegments + 1;
      // One triangle per polar cell; avoid zero-area triangles at poles.
      if (y != 0)
        mesh.indices.insert(mesh.indices.end(), {a, a + 1, b});
      if (y + 1 != latitudeSegments)
        mesh.indices.insert(mesh.indices.end(), {a + 1, b + 1, b});
    }
  }
  return mesh;
}
