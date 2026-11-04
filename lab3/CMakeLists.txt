cmake_minimum_required(VERSION 3.25)
project(CG_Lab_3 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
if(NOT DEFINED ENV{VULKAN_SDK})
  message(FATAL_ERROR "Set VULKAN_SDK to ~/VulkanSDK/1.4.321.0/macOS before configuring")
endif()
find_package(Vulkan REQUIRED)
find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
if(NOT TARGET glfw)
  add_subdirectory("${CG_ROOT}/third_party/glfw" "${CMAKE_CURRENT_BINARY_DIR}/glfw" EXCLUDE_FROM_ALL)
endif()
set(IMGUI_DIR "${CG_ROOT}/third_party/imgui")
add_executable(cg_lab3
  src/main.cpp src/stb_image.cpp
  ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp)
target_include_directories(cg_lab3 PRIVATE
  src "${CG_ROOT}/third_party/glm" "${CG_ROOT}/third_party/stb"
  ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(cg_lab3 PRIVATE Vulkan::Vulkan glfw)
target_compile_definitions(cg_lab3 PRIVATE
  $<$<CONFIG:Debug>:APP_USE_VULKAN_DEBUG_REPORT>
  GLM_FORCE_DEPTH_ZERO_TO_ONE CG_LAB_STAGE=3
  CG_SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}/shaders"
  CG_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
set_target_properties(cg_lab3 PROPERTIES BUILD_RPATH "$ENV{VULKAN_SDK}/lib")
set(SHADER_OUTPUTS)
foreach(SHADER scene.vert scene.frag)
  set(INPUT "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER}")
  set(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER}.spv")
  add_custom_command(OUTPUT ${OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
    COMMAND ${GLSLC} ${INPUT} -o ${OUTPUT}
    DEPENDS ${INPUT} VERBATIM)
  list(APPEND SHADER_OUTPUTS ${OUTPUT})
endforeach()
add_custom_target(cg_lab3_shaders DEPENDS ${SHADER_OUTPUTS})
add_dependencies(cg_lab3 cg_lab3_shaders)
