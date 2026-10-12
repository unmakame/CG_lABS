#pragma once
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include "imgui.h"
#include "sphere_mesh.hpp"
#include "stb_image.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef CG_LAB_STAGE
#define CG_LAB_STAGE 3
#endif
#ifndef CG_SHADER_DIR
#define CG_SHADER_DIR "shaders"
#endif
#ifndef CG_ASSET_DIR
#define CG_ASSET_DIR "assets"
#endif

struct GpuPointLight {
  glm::vec4 position;
  glm::vec4 colorIntensity;
};
struct GpuSpotLight {
  glm::vec4 position;
  glm::vec4 directionInnerCos;
  glm::vec4 colorIntensity;
  glm::vec4 outerCosAndPadding;
};
struct alignas(16) SceneUniforms {
  glm::mat4 viewProjection{1.0f};
  glm::vec4 cameraPosition{0.0f};
  glm::vec4 ambientColor{0.13f, 0.15f, 0.19f, 1.0f};
  glm::vec4 directionalDirection{-0.5f, -1.0f, -0.4f, 0.0f};
  glm::vec4 directionalColor{1.0f, 0.94f, 0.82f, 0.7f};
};
struct alignas(16) ModelUniforms {
  glm::mat4 model{1.0f};
  glm::mat4 normalMatrix{1.0f};
  glm::vec4 albedoTint{1.0f};
  glm::vec4 specularTintAndPower{1.0f, 1.0f, 1.0f, 48.0f};
};
struct MaterialPush {
  float time;
  std::uint32_t animatedSampling, showVertexColor, unlit;
};
struct GpuBuffer {
  VkBuffer handle = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
  void *mapped = nullptr;
};
struct GpuTexture {
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
};
struct PointLight {
  glm::vec3 position{2.4f, 1.2f, 1.8f};
  glm::vec3 color{1.0f, 0.45f, 0.22f};
  float intensity = 5.0f;
};
struct SpotLight {
  glm::vec3 position{-2.0f, 2.1f, 2.4f};
  glm::vec3 direction{0.55f, -0.55f, -0.62f};
  glm::vec3 color{0.35f, 0.62f, 1.0f};
  float intensity = 16.0f;
  float innerDegrees = 18.0f;
  float outerDegrees = 28.0f;
};
struct ModelState {
  glm::vec3 position{0};
  glm::vec3 rotation{0};
  glm::vec3 scale{1};
  glm::vec3 tint{1};
  glm::vec3 specular{1};
  float power = 48;
  int texture = 0;
  bool animate = true;
  bool animatedSampling = false;
};

class SceneRenderer {
  static constexpr std::uint32_t MaxObjects = 8, MaxPointLights = 32,
                                 MaxSpotLights = 32, MaxDraws = 72;
  VkPhysicalDevice physical_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkCommandPool uploadPool_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout sceneLayout_ = VK_NULL_HANDLE,
                        modelLayout_ = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  VkDescriptorSet sceneSet_ = VK_NULL_HANDLE;
  GpuBuffer vertices_, indices_, sceneBuffer_, modelBuffer_, pointBuffer_,
      spotBuffer_;
  std::vector<GpuTexture> textures_;
  MeshData mesh_;
  std::uint32_t objectIndexCount_ = 0;
  VkDeviceSize modelStride_ = 0;
  std::vector<ModelState> models_{{}};
  std::vector<PointLight> points_{{}};
  std::vector<SpotLight> spots_{{}};
  glm::vec3 cameraEye_{0.0f, 0.5f, 7.0f};
  float cameraYaw_ = 0.0f, cameraPitch_ = -0.07f, fovDegrees_ = 45.0f;
  bool perspective_ = true, animationPlaying_ = false,
       showVertexColor_ = CG_LAB_STAGE == 1, animateUV_ = true,
       showLightMarkers_ = CG_LAB_STAGE >= 2;
  bool showGrid_ = CG_LAB_STAGE == 1;
  bool directionalEnabled_ = true, textureAnimationPlaying_ = true;
  glm::vec3 ambient_{0.13f, 0.15f, 0.19f};
  float textureTime_ = 0.0f, time_ = 0.0f, animationSpeed_ = 0.8f,
        orbitRadius_ = 0.8f;
  glm::vec3 directionalDirection_{-0.5f, -1.0f, -0.4f},
      directionalColor_{1.0f, 0.94f, 0.82f};
  float directionalIntensity_ = 0.7f;
  std::array<VkDescriptorSet, MaxObjects * 2> allModelSets_{};

  static void check(VkResult r, const char *what) {
    if (r != VK_SUCCESS)
      throw std::runtime_error(std::string(what) +
                               " failed: " + std::to_string(r));
  }
  std::uint32_t memoryType(std::uint32_t bits,
                           VkMemoryPropertyFlags flags) const {
    VkPhysicalDeviceMemoryProperties p{};
    vkGetPhysicalDeviceMemoryProperties(physical_, &p);
    for (std::uint32_t i = 0; i < p.memoryTypeCount; ++i)
      if ((bits & (1u << i)) &&
          (p.memoryTypes[i].propertyFlags & flags) == flags)
        return i;
    throw std::runtime_error(
        "No Vulkan memory type matches the requested flags");
  }
  GpuBuffer makeBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                       VkMemoryPropertyFlags flags, bool map = false) {
    GpuBuffer b{};
    b.size = size;
    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = size;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(device_, &ci, nullptr, &b.handle), "vkCreateBuffer");
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device_, b.handle, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = memoryType(req.memoryTypeBits, flags);
    check(vkAllocateMemory(device_, &ai, nullptr, &b.memory),
          "vkAllocateMemory(buffer)");
    check(vkBindBufferMemory(device_, b.handle, b.memory, 0),
          "vkBindBufferMemory");
    if (map)
      check(vkMapMemory(device_, b.memory, 0, size, 0, &b.mapped),
            "vkMapMemory");
    return b;
  }
  void freeBuffer(GpuBuffer &b) {
    if (b.mapped)
      vkUnmapMemory(device_, b.memory);
    if (b.handle)
      vkDestroyBuffer(device_, b.handle, nullptr);
    if (b.memory)
      vkFreeMemory(device_, b.memory, nullptr);
    b = {};
  }
  VkCommandBuffer beginUpload() {
    VkCommandBufferAllocateInfo ai{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = uploadPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    check(vkAllocateCommandBuffers(device_, &ai, &cmd),
          "vkAllocateCommandBuffers");
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer");
    return cmd;
  }
  void endUpload(VkCommandBuffer cmd) {
    check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    check(vkQueueSubmit(queue_, 1, &si, VK_NULL_HANDLE),
          "vkQueueSubmit(upload)");
    check(vkQueueWaitIdle(queue_), "vkQueueWaitIdle");
    vkFreeCommandBuffers(device_, uploadPool_, 1, &cmd);
  }
  void copyToDevice(const void *data, VkDeviceSize size,
                    VkBufferUsageFlags finalUsage, GpuBuffer &dst) {
    auto staging = makeBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              true);
    std::memcpy(staging.mapped, data, (size_t)size);
    dst = makeBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | finalUsage,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    auto cmd = beginUpload();
    VkBufferCopy copy{0, 0, size};
    vkCmdCopyBuffer(cmd, staging.handle, dst.handle, 1, &copy);
    endUpload(cmd);
    freeBuffer(staging);
  }
  GpuTexture loadTexture(const std::string &file) {
    int w = 0, h = 0, n = 0;
    const std::string path = std::string(CG_ASSET_DIR) + "/" + file;
    stbi_set_flip_vertically_on_load(1);
    unsigned char *pixels = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!pixels)
      throw std::runtime_error("Cannot load texture " + path + ": " +
                               stbi_failure_reason());
    const VkDeviceSize bytes = (VkDeviceSize)w * h * 4;
    auto stage = makeBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            true);
    std::memcpy(stage.mapped, pixels, (size_t)bytes);
    stbi_image_free(pixels);
    GpuTexture t{};
    VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent = {(uint32_t)w, (uint32_t)h, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    check(vkCreateImage(device_, &ici, nullptr, &t.image),
          "vkCreateImage(texture)");
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device_, t.image, &req);
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex =
        memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkAllocateMemory(device_, &mai, nullptr, &t.memory),
          "vkAllocateMemory(texture)");
    check(vkBindImageMemory(device_, t.image, t.memory, 0),
          "vkBindImageMemory");
    auto cmd = beginUpload();
    VkImageMemoryBarrier toCopy{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toCopy.srcAccessMask = 0;
    toCopy.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toCopy.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toCopy.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toCopy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toCopy.image = t.image;
    toCopy.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &toCopy);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {(uint32_t)w, (uint32_t)h, 1};
    vkCmdCopyBufferToImage(cmd, stage.handle, t.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    VkImageMemoryBarrier toRead{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toRead.image = t.image;
    toRead.subresourceRange = toCopy.subresourceRange;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &toRead);
    endUpload(cmd);
    freeBuffer(stage);
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = t.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    check(vkCreateImageView(device_, &vi, nullptr, &t.view),
          "vkCreateImageView");
    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.addressModeU = si.addressModeV = si.addressModeW =
        VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.maxAnisotropy = 1.0f;
    si.maxLod = 0.0f;
    check(vkCreateSampler(device_, &si, nullptr, &t.sampler),
          "vkCreateSampler");
    return t;
  }
  VkShaderModule shader(const std::string &name) {
    std::ifstream f(std::string(CG_SHADER_DIR) + "/" + name,
                    std::ios::binary | std::ios::ate);
    if (!f)
      throw std::runtime_error("Cannot open shader: " + name);
    auto sz = f.tellg();
    std::vector<char> bytes((size_t)sz);
    f.seekg(0);
    f.read(bytes.data(), sz);
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = bytes.size();
    ci.pCode = reinterpret_cast<const uint32_t *>(bytes.data());
    VkShaderModule m;
    check(vkCreateShaderModule(device_, &ci, nullptr, &m),
          "vkCreateShaderModule");
    return m;
  }
  glm::vec3 cameraPosition() const { return cameraEye_; }
  glm::vec3 cameraForward() const {
    return {std::cos(cameraPitch_) * std::sin(cameraYaw_),
            std::sin(cameraPitch_),
            -std::cos(cameraPitch_) * std::cos(cameraYaw_)};
  }
  glm::mat4 viewProjection(int w, int h) const {
    auto eye = cameraPosition();
    auto view = glm::lookAt(eye, eye + cameraForward(), glm::vec3(0, 1, 0));
    float aspect = (h > 0) ? (float)w * 0.72f / (float)h : 1.0f;
    glm::mat4 proj = perspective_ ? glm::perspective(glm::radians(fovDegrees_),
                                                     aspect, 0.05f, 100.0f)
                                  : glm::ortho(-2.6f * aspect, 2.6f * aspect,
                                               -2.6f, 2.6f, 0.05f, 100.0f);
    proj[1][1] *= -1.0f;
    return proj * view;
  }
  glm::mat4 modelMatrix(const ModelState &m, std::size_t index) const {
    glm::vec3 p = m.position;
    if (m.animate) {
      float a = time_ + (float)index * 2.094f;
      float base = (float)index * 2.094f;
      p.x += orbitRadius_ * (std::cos(a) - std::cos(base));
      p.z += orbitRadius_ * (std::sin(a) - std::sin(base));
      p.y += 0.3f * orbitRadius_ * (std::sin(2 * a) - std::sin(2 * base));
    }
    glm::mat4 mat = glm::translate(glm::mat4(1), p);
    mat = glm::rotate(mat, glm::radians(m.rotation.x), {1, 0, 0});
    mat = glm::rotate(mat,
                      glm::radians(m.rotation.y) + (m.animate ? time_ : 0.0f),
                      {0, 1, 0});
    mat = glm::rotate(mat, glm::radians(m.rotation.z), {0, 0, 1});
    return glm::scale(mat, glm::max(m.scale, glm::vec3(0.01f)));
  }
  void createDescriptors() {
    std::array<VkDescriptorSetLayoutBinding, 3> sb{};
    sb[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
             VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
             nullptr};
    sb[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
             VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    sb[2] = {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
             VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo si{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    si.bindingCount = 3;
    si.pBindings = sb.data();
    check(vkCreateDescriptorSetLayout(device_, &si, nullptr, &sceneLayout_),
          "scene descriptor layout");
    std::array<VkDescriptorSetLayoutBinding, 4> mb{};
    mb[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
             VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
             nullptr};
    for (int i = 1; i < 4; i++)
      mb[i] = {(uint32_t)i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
               VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    si.bindingCount = 4;
    si.pBindings = mb.data();
    check(vkCreateDescriptorSetLayout(device_, &si, nullptr, &modelLayout_),
          "model descriptor layout");
    VkDescriptorSetAllocateInfo ai{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = descriptorPool_;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &sceneLayout_;
    check(vkAllocateDescriptorSets(device_, &ai, &sceneSet_),
          "scene descriptor set");
    VkDescriptorBufferInfo bi0{sceneBuffer_.handle, 0, sizeof(SceneUniforms)},
        bi1{pointBuffer_.handle, 0, pointBuffer_.size},
        bi2{spotBuffer_.handle, 0, spotBuffer_.size};
    VkWriteDescriptorSet writes[3]{};
    for (int i = 0; i < 3; i++) {
      writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[i].dstSet = sceneSet_;
      writes[i].dstBinding = i;
      writes[i].descriptorCount = 1;
      writes[i].descriptorType = i == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                        : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    writes[0].pBufferInfo = &bi0;
    writes[1].pBufferInfo = &bi1;
    writes[2].pBufferInfo = &bi2;
    vkUpdateDescriptorSets(device_, 3, writes, 0, nullptr);
    std::array<VkDescriptorSetLayout, CG_LAB_STAGE == 3 ? 2 : MaxObjects * 2>
        layouts{};
    layouts.fill(modelLayout_);
    ai.descriptorSetCount = (uint32_t)layouts.size();
    ai.pSetLayouts = layouts.data();
    check(vkAllocateDescriptorSets(device_, &ai, allModelSets_.data()),
          "model descriptor sets");
    for (std::uint32_t m = 0; m < layouts.size(); m++) {
      VkDescriptorBufferInfo ubo{modelBuffer_.handle, 0, sizeof(ModelUniforms)};
      auto tex = [&](int i) {
        auto &t = textures_[i];
        return VkDescriptorImageInfo{t.sampler, t.view,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
      };
      VkDescriptorImageInfo al = tex(m % 2),
                            sp = tex(CG_LAB_STAGE == 3 && m % 2 == 1 ? 4 : 2),
                            em = tex(CG_LAB_STAGE == 3 && m % 2 == 1 ? 4 : 3);
      VkWriteDescriptorSet w[4]{};
      for (int i = 0; i < 4; i++) {
        w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[i].dstSet = allModelSets_[m];
        w[i].dstBinding = i;
        w[i].descriptorCount = 1;
        w[i].descriptorType = i == 0
                                  ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
                                  : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      }
      w[0].pBufferInfo = &ubo;
      w[1].pImageInfo = &al;
      w[2].pImageInfo = &sp;
      w[3].pImageInfo = &em;
      vkUpdateDescriptorSets(device_, 4, w, 0, nullptr);
    }
  }
  VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
  void createPipeline(VkRenderPass pass) {
    auto vs = shader("scene.vert.spv"), fs = shader("scene.frag.spv");
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";
    VkVertexInputBindingDescription bind{0, sizeof(Vertex),
                                         VK_VERTEX_INPUT_RATE_VERTEX};
    std::array<VkVertexInputAttributeDescription, 4> attrs{
        {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
         {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
         {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)},
         {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)}}};
    VkPipelineVertexInputStateCreateInfo vi{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = attrs.size();
    vi.pVertexAttributeDescriptions = attrs.data();
    VkPipelineInputAssemblyStateCreateInfo ia{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState ba{};
    ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo bs{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    bs.attachmentCount = 1;
    bs.pAttachments = &ba;
    VkDynamicState dyns[] = {VK_DYNAMIC_STATE_VIEWPORT,
                             VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dyns;
    VkDescriptorSetLayout sets[] = {sceneLayout_, modelLayout_};
    VkPushConstantRange pcr{VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                            sizeof(MaterialPush)};
    VkPipelineLayoutCreateInfo pl{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 2;
    pl.pSetLayouts = sets;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pcr;
    check(vkCreatePipelineLayout(device_, &pl, nullptr, &pipelineLayout_),
          "pipeline layout");
    VkGraphicsPipelineCreateInfo pi{
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pi.stageCount = 2;
    pi.pStages = stages;
    pi.pVertexInputState = &vi;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vp;
    pi.pRasterizationState = &rs;
    pi.pMultisampleState = &ms;
    VkPipelineDepthStencilStateCreateInfo depth{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;
    pi.pDepthStencilState = &depth;
    pi.pColorBlendState = &bs;
    pi.pDynamicState = &dyn;
    pi.layout = pipelineLayout_;
    pi.renderPass = pass;
    pi.subpass = 0;
    check(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr,
                                    &pipeline_),
          "graphics pipeline");
    vkDestroyShaderModule(device_, vs, nullptr);
    vkDestroyShaderModule(device_, fs, nullptr);
  }
  void updateGpuData(int w, int h) {
    SceneUniforms s{};
    s.viewProjection = viewProjection(w, h);
    auto cp = cameraPosition();
    s.cameraPosition = glm::vec4(cp, 1);
    s.ambientColor = glm::vec4(ambient_, 1);
    s.directionalDirection = glm::vec4(directionalDirection_, 0);
    s.directionalColor =
        glm::vec4(directionalColor_,
                  directionalIntensity_ * (directionalEnabled_ ? 1.0f : 0.0f));
    std::memcpy(sceneBuffer_.mapped, &s, sizeof(s));
    struct Header {
      std::uint32_t count, pad[3];
    };
    Header ph{(std::uint32_t)points_.size(), {0, 0, 0}},
        sh{(std::uint32_t)spots_.size(), {0, 0, 0}};
    std::memcpy(pointBuffer_.mapped, &ph, sizeof(ph));
    for (size_t i = 0; i < points_.size(); ++i) {
      GpuPointLight g{glm::vec4(points_[i].position, 1),
                      glm::vec4(points_[i].color, points_[i].intensity)};
      std::memcpy((char *)pointBuffer_.mapped + 16 + i * sizeof(g), &g,
                  sizeof(g));
    }
    std::memcpy(spotBuffer_.mapped, &sh, sizeof(sh));
    for (size_t i = 0; i < spots_.size(); ++i) {
      auto &l = spots_[i];
      glm::vec3 safeDir = glm::dot(l.direction, l.direction) > 1e-8f
                              ? glm::normalize(l.direction)
                              : glm::vec3(0, -1, 0);
      GpuSpotLight g{
          glm::vec4(l.position, 1),
          glm::vec4(safeDir, std::cos(glm::radians(l.innerDegrees))),
          glm::vec4(l.color, l.intensity),
          glm::vec4(std::cos(glm::radians(l.outerDegrees)), 0, 0, 0)};
      std::memcpy((char *)spotBuffer_.mapped + 16 + i * sizeof(g), &g,
                  sizeof(g));
    }
  }
  struct DrawItem {
    glm::mat4 model;
    glm::vec3 color;
    std::uint32_t texture;
    std::uint32_t unlit;
    glm::vec3 specular{1};
    float power = 48;
  };
  void makeDrawList(std::vector<DrawItem> &draws) {
    for (size_t i = 0; i < models_.size(); ++i)
      draws.push_back({modelMatrix(models_[i], i), models_[i].tint,
                       (std::uint32_t)models_[i].texture,
                       CG_LAB_STAGE == 1 ? 1u : 0u, models_[i].specular,
                       models_[i].power});
    if (showLightMarkers_) {
      for (auto &l : points_)
        draws.push_back({glm::scale(glm::translate(glm::mat4(1), l.position),
                                    glm::vec3(.085f)),
                         l.color, 0, 2u});
      for (auto &l : spots_)
        draws.push_back({glm::scale(glm::translate(glm::mat4(1), l.position),
                                    glm::vec3(.105f)),
                         l.color, 0, 2u});
    }
  }

public:
  void Init(VkInstance /*instance*/, VkPhysicalDevice phys, VkDevice dev,
            uint32_t qf, VkQueue q, VkRenderPass pass, VkDescriptorPool pool) {
    physical_ = phys;
    device_ = dev;
    queue_ = q;
    descriptorPool_ = pool;
    mesh_ = makeUvSphere(12, 8);
    objectIndexCount_ = (uint32_t)mesh_.indices.size();
    VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cp.queueFamilyIndex = qf;
    cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    check(vkCreateCommandPool(dev, &cp, nullptr, &uploadPool_),
          "upload command pool");
    copyToDevice(mesh_.vertices.data(), mesh_.vertices.size() * sizeof(Vertex),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices_);
    copyToDevice(mesh_.indices.data(), mesh_.indices.size() * sizeof(uint32_t),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices_);
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(phys, &props);
    modelStride_ = (sizeof(ModelUniforms) +
                    props.limits.minUniformBufferOffsetAlignment - 1) &
                   ~(props.limits.minUniformBufferOffsetAlignment - 1);
    sceneBuffer_ =
        makeBuffer(sizeof(SceneUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   true);
    modelBuffer_ =
        makeBuffer(modelStride_ * MaxDraws, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   true);
    pointBuffer_ = makeBuffer(16 + MaxPointLights * sizeof(GpuPointLight),
                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              true);
    spotBuffer_ = makeBuffer(16 + MaxSpotLights * sizeof(GpuSpotLight),
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             true);
#if CG_LAB_STAGE == 3
    textures_.push_back(loadTexture("sphere_albedo.ppm"));
    textures_.push_back(loadTexture("marble_albedo.ppm"));
    textures_.push_back(loadTexture("sphere_specular.ppm"));
    textures_.push_back(loadTexture("sphere_emissive.ppm"));
    textures_.push_back(loadTexture("black.ppm"));
#elif CG_LAB_STAGE == 2
    textures_.push_back(loadTexture("white.ppm"));
    textures_.push_back(loadTexture("white.ppm"));
    textures_.push_back(loadTexture("white.ppm"));
    textures_.push_back(loadTexture("black.ppm"));
#else
    textures_.push_back(loadTexture("white.ppm"));
    textures_.push_back(loadTexture("white.ppm"));
    textures_.push_back(loadTexture("white.ppm"));
    textures_.push_back(loadTexture("black.ppm"));
#endif
    createDescriptors();
    createPipeline(pass);
    models_.resize(2);
    models_[0].position = {-1.25f, 0, 0};
    models_[1].position = {1.25f, 0, 0};
    models_[0].scale = models_[1].scale = glm::vec3(0.9f);
    models_[0].animatedSampling = true;
    models_[1].texture = CG_LAB_STAGE == 3 ? 1 : 0;
    if (CG_LAB_STAGE == 1) {
      points_.clear();
      spots_.clear();
    }
  }
  void Update(float dt, int w, int h) {
    vkQueueWaitIdle(queue_);
    float step = std::max(0.0f, std::min(dt, 0.05f));
    if (animationPlaying_)
      time_ += step * animationSpeed_;
    if (textureAnimationPlaying_)
      textureTime_ += step;
    updateGpuData(w, h);
  }
  void RecordBeforePass(VkCommandBuffer cmd, int w, int h) {
    (void)cmd;
    (void)w;
    (void)h;
  }
  void Draw(VkCommandBuffer cmd, int w, int h) {
    std::vector<DrawItem> draws;
    makeDrawList(draws);
    if (draws.size() > MaxDraws)
      draws.resize(MaxDraws);
    int left = (int)(w * 0.28f);
    VkViewport viewport{(float)left, 0, (float)(w - left), (float)h, 0, 1};
    VkRect2D scissor{{left, 0}, {(uint32_t)(w - left), (uint32_t)h}};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout_, 0, 1, &sceneSet_, 0, nullptr);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertices_.handle, &offset);
    vkCmdBindIndexBuffer(cmd, indices_.handle, 0, VK_INDEX_TYPE_UINT32);
    for (size_t i = 0; i < draws.size(); ++i) {
      ModelUniforms u{};
      u.model = draws[i].model;
      u.normalMatrix = glm::transpose(glm::inverse(draws[i].model));
      u.albedoTint = glm::vec4(draws[i].color, 1);
      u.specularTintAndPower = glm::vec4(draws[i].specular, draws[i].power);
      std::memcpy((char *)modelBuffer_.mapped + i * modelStride_, &u,
                  sizeof(u));
      uint32_t dyn = (uint32_t)(i * modelStride_);
      auto set = allModelSets_[(CG_LAB_STAGE == 3 ? 0 : i % MaxObjects) * 2 +
                               draws[i].texture % 2];
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipelineLayout_, 1, 1, &set, 1, &dyn);
      MaterialPush pc{textureTime_,
                      (CG_LAB_STAGE == 3 && animateUV_ && i < models_.size() &&
                       models_[i].animatedSampling)
                          ? 1u
                          : 0u,
                      (showVertexColor_ ? 1u : 0u) | (showGrid_ ? 2u : 0u),
                      draws[i].unlit};
      vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(pc), &pc);
      vkCmdDrawIndexed(cmd, objectIndexCount_, 1, 0, 0, 0);
    }
  }
  void DrawUi() {
    const char *title = CG_LAB_STAGE == 1
                            ? "CG Lab 1 | Variant 5"
                            : (CG_LAB_STAGE == 2 ? "CG Lab 2 | Variant 5"
                                                 : "CG Lab 3 | Variant 5");
    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(display.x * .28f, display.y));
    ImGui::Begin(title, nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Vulkan sphere | 117 vertices | 12 x 8 segments");
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Projection and camera",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::RadioButton("Perspective", &projectionRadio_, 0);
      ImGui::SameLine();
      ImGui::RadioButton("Orthographic", &projectionRadio_, 1);
      perspective_ = projectionRadio_ == 0;
      ImGui::SliderFloat("FOV", &fovDegrees_, 25, 100, "%.0f deg");
      ImGui::SliderFloat("Camera yaw", &cameraYaw_, -3.14f, 3.14f);
      ImGui::SliderFloat("Camera pitch", &cameraPitch_, -1.2f, 1.2f);
      ImGui::DragFloat3("Camera position", glm::value_ptr(cameraEye_), .03f);
      if (ImGui::Button("Reset camera")) {
        cameraEye_ = {0, .5f, 7};
        cameraYaw_ = 0;
        cameraPitch_ = -.07f;
        fovDegrees_ = 45;
      }
    }
    if (ImGui::CollapsingHeader("Models and animation",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::Button("Add sphere") && models_.size() < MaxObjects) {
        ModelState m{};
        m.position = {0.9f * (float)models_.size(), 0, 0};
        m.scale = glm::vec3(.48f);
        m.animate = false;
        models_.push_back(m);
      }
      ImGui::SameLine();
      ImGui::Text("%d / %u", (int)models_.size(), MaxObjects);
      ImGui::Checkbox("Play animation", &animationPlaying_);
      ImGui::SliderFloat("Animation speed", &animationSpeed_, 0, 3);
      ImGui::SliderFloat("Orbit radius", &orbitRadius_, 0, 2);
      if (CG_LAB_STAGE == 1) {
        ImGui::Checkbox("Procedural vertex color", &showVertexColor_);
        ImGui::Checkbox("Surface grid", &showGrid_);
      }
      for (size_t i = 0; i < models_.size(); ++i) {
        std::string label = "Sphere " + std::to_string(i + 1);
        if (ImGui::TreeNode(label.c_str())) {
          ImGui::DragFloat3("Position", glm::value_ptr(models_[i].position),
                            .02f);
          ImGui::DragFloat3("Rotation", glm::value_ptr(models_[i].rotation),
                            1.0f);
          ImGui::DragFloat3("Scale", glm::value_ptr(models_[i].scale), .01f,
                            .1f, 3.0f);
          ImGui::ColorEdit3("Tint", glm::value_ptr(models_[i].tint));
          if (CG_LAB_STAGE >= 2) {
            ImGui::ColorEdit3("Specular color",
                              glm::value_ptr(models_[i].specular));
            ImGui::SliderFloat("Shininess", &models_[i].power, 2, 128);
          }
          if (CG_LAB_STAGE == 3) {
            const char *names[] = {"Earthlike",
                                   "Marble (no specular/emission)"};
            ImGui::Combo("Albedo texture", &models_[i].texture, names, 2);
            ImGui::Checkbox("Animated sampling for this model",
                            &models_[i].animatedSampling);
          }
          ImGui::Checkbox("Orbit this model", &models_[i].animate);
          if (models_.size() > 1 && ImGui::Button("Remove model")) {
            models_.erase(models_.begin() + i);
            ImGui::TreePop();
            break;
          }
          ImGui::TreePop();
        }
      }
    }
#if CG_LAB_STAGE >= 2
    if (ImGui::CollapsingHeader("Lighting and material",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Directional light", &directionalEnabled_);
      ImGui::DragFloat3("Direction", glm::value_ptr(directionalDirection_),
                        .01f);
      ImGui::ColorEdit3("Light color", glm::value_ptr(directionalColor_));
      ImGui::SliderFloat("Directional intensity", &directionalIntensity_, 0, 4);
      ImGui::ColorEdit3("Ambient", glm::value_ptr(ambient_));
      ImGui::Checkbox("Show light markers", &showLightMarkers_);
    }
    if (ImGui::CollapsingHeader("Point lights (SSBO)")) {
      if (ImGui::Button("Add point light") && points_.size() < MaxPointLights)
        points_.push_back(PointLight{});
      for (size_t i = 0; i < points_.size(); ++i) {
        std::string label = "Point " + std::to_string(i + 1);
        if (ImGui::TreeNode(label.c_str())) {
          ImGui::DragFloat3("Position", glm::value_ptr(points_[i].position),
                            .03f);
          ImGui::ColorEdit3("Color", glm::value_ptr(points_[i].color));
          ImGui::SliderFloat("Intensity", &points_[i].intensity, 0, 40);
          if (ImGui::Button("Delete point")) {
            points_.erase(points_.begin() + i);
            ImGui::TreePop();
            break;
          }
          ImGui::TreePop();
        }
      }
    }
    if (ImGui::CollapsingHeader("Spot lights (SSBO)")) {
      if (ImGui::Button("Add spot light") && spots_.size() < MaxSpotLights)
        spots_.push_back(SpotLight{});
      for (size_t i = 0; i < spots_.size(); ++i) {
        std::string label = "Spot " + std::to_string(i + 1);
        if (ImGui::TreeNode(label.c_str())) {
          ImGui::DragFloat3("Position", glm::value_ptr(spots_[i].position),
                            .03f);
          ImGui::DragFloat3("Direction", glm::value_ptr(spots_[i].direction),
                            .01f);
          ImGui::ColorEdit3("Color", glm::value_ptr(spots_[i].color));
          ImGui::SliderFloat("Intensity", &spots_[i].intensity, 0, 80);
          ImGui::SliderFloat("Inner angle", &spots_[i].innerDegrees, 1, 70);
          ImGui::SliderFloat("Outer angle", &spots_[i].outerDegrees, 1, 85);
          if (spots_[i].outerDegrees < spots_[i].innerDegrees)
            spots_[i].outerDegrees = spots_[i].innerDegrees;
          if (ImGui::Button("Delete spot")) {
            spots_.erase(spots_.begin() + i);
            ImGui::TreePop();
            break;
          }
          ImGui::TreePop();
        }
      }
    }
#endif
#if CG_LAB_STAGE >= 3
    if (ImGui::CollapsingHeader("Textures and sampling",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Enable sampling effects", &animateUV_);
      ImGui::Checkbox("Play texture animation", &textureAnimationPlaying_);
      ImGui::Text(
          "Albedo, specular and Perlin emissive maps loaded from assets/");
      ImGui::Text("Dynamic UBO stride: %llu bytes",
                  (unsigned long long)modelStride_);
    }
#endif
    ImGui::Text("Frame %.2f ms (%.0f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                ImGui::GetIO().Framerate);
    ImGui::End();
    if (showLightMarkers_ && !spots_.empty()) {
      ImVec2 screen = ImGui::GetIO().DisplaySize;
      glm::mat4 vp = viewProjection((int)screen.x, (int)screen.y);
      auto project = [&](glm::vec3 p, ImVec2 &out) {
        glm::vec4 c = vp * glm::vec4(p, 1);
        if (c.w <= 0.01f)
          return false;
        out =
            ImVec2(screen.x * .28f + (c.x / c.w * .5f + .5f) * screen.x * .72f,
                   (c.y / c.w * .5f + .5f) * screen.y);
        return true;
      };
      ImDrawList *dl = ImGui::GetBackgroundDrawList();
      for (const auto &l : spots_) {
        ImVec2 origin, tip;
        if (!project(l.position, origin) ||
            !project(l.position + (glm::dot(l.direction, l.direction) > 1e-8f
                                       ? glm::normalize(l.direction)
                                       : glm::vec3(0, -1, 0)) *
                                      1.0f,
                     tip))
          continue;
        dl->AddLine(origin, tip, IM_COL32(255, 255, 255, 220), 2.0f);
        glm::vec3 axis = glm::dot(l.direction, l.direction) > 1e-8f
                             ? glm::normalize(l.direction)
                             : glm::vec3(0, -1, 0);
        glm::vec3 right = glm::normalize(
            glm::cross(axis, glm::abs(axis.y) < .9f ? glm::vec3(0, 1, 0)
                                                    : glm::vec3(1, 0, 0)));
        glm::vec3 up = glm::normalize(glm::cross(axis, right));
        float radius = std::tan(glm::radians(l.outerDegrees)) * 1.2f;
        ImVec2 prev{};
        bool have = false;
        for (int k = 0; k <= 32; k++) {
          float a = 6.2831853f * k / 32.0f;
          glm::vec3 q = l.position + axis * 1.2f +
                        radius * (std::cos(a) * right + std::sin(a) * up);
          ImVec2 cur;
          if (project(q, cur)) {
            if (k % 8 == 0)
              dl->AddLine(origin, cur, IM_COL32(255, 255, 255, 180), 1.0f);
            if (have)
              dl->AddLine(prev, cur, IM_COL32(220, 230, 255, 150), 1.0f);
            prev = cur;
            have = true;
          }
        }
      }
    }
  }
  void Shutdown() {
    if (device_)
      vkDeviceWaitIdle(device_);
    if (pipeline_)
      vkDestroyPipeline(device_, pipeline_, nullptr);
    if (pipelineLayout_)
      vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    if (sceneLayout_)
      vkDestroyDescriptorSetLayout(device_, sceneLayout_, nullptr);
    if (modelLayout_)
      vkDestroyDescriptorSetLayout(device_, modelLayout_, nullptr);
    for (auto &t : textures_) {
      if (t.sampler)
        vkDestroySampler(device_, t.sampler, nullptr);
      if (t.view)
        vkDestroyImageView(device_, t.view, nullptr);
      if (t.image)
        vkDestroyImage(device_, t.image, nullptr);
      if (t.memory)
        vkFreeMemory(device_, t.memory, nullptr);
    }
    freeBuffer(vertices_);
    freeBuffer(indices_);
    freeBuffer(sceneBuffer_);
    freeBuffer(modelBuffer_);
    freeBuffer(pointBuffer_);
    freeBuffer(spotBuffer_);
    if (uploadPool_)
      vkDestroyCommandPool(device_, uploadPool_, nullptr);
  }

private:
  int projectionRadio_ = 0;
};
