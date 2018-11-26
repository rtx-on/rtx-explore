#pragma once

#include "DirectXRaytracingHelper.h"
#include "Utilities.h"
#include "shaders/RayTracingHlslCompat.h"
#include <glm/glm/glm.hpp>

class D3D12RaytracingSimpleLighting;
class Scene;

namespace ModelLoading {

// Holds the buffer that contains the texture data
struct Texture {
  int id;

  D3DBuffer texBuffer;
  ID3D12Resource *textureBufferUploadHeap;
};

// Holds a pointer to each type of texture
struct TextureBundle {
  Texture *albedoTex;
  Texture *normalTex;
};

// Holds data for a specific material
struct Material {
  int id;

  glm::vec3 diffuse;
  glm::vec3 specular;
  float specularExp;
  float eta;
  float reflectiveness;
  float refractiveness;
  float emittance;
};

// Holds the vertex and index buffer (triangulated) for a loaded model
struct Model {
  int id;
  int vOffset;
  int iOffset;

  int iCount = 0;
  int vCount = 0;

  Scene *loadedScene;

  D3D12_RAYTRACING_GEOMETRY_DESC &GetGeomDesc();
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC &GetBottomLevelBuildDesc();

  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO &
  GetPreBuild(bool is_fallback,
              ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice,
              ComPtr<ID3D12Device5> m_dxrDevice);

  ComPtr<ID3D12Resource> GetBottomLevelScratchAS(
      bool is_fallback, ComPtr<ID3D12Device> device,
      ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice,
      ComPtr<ID3D12Device5> m_dxrDevice);

  ComPtr<ID3D12Resource>
  GetBottomAS(bool is_fallback, ComPtr<ID3D12Device> device,
              ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice,
              ComPtr<ID3D12Device5> m_dxrDevice);

  WRAPPED_GPU_POINTER
  GetFallBackWrappedPoint(
      D3D12RaytracingSimpleLighting *programState, bool is_fallback,
      ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice,
      ComPtr<ID3D12Device5> m_dxrDevice, UINT numBufferElements);

  void FinalizeAS();

  bool is_gpu_ptr_allocated = false;
  WRAPPED_GPU_POINTER gpuPtr{};
  D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc;
  bool is_m_bottomLevelAccelerationStructure_allocated = false;
  ComPtr<ID3D12Resource> m_bottomLevelAccelerationStructure;
  bool is_scratchResource_allocated = false;
  ComPtr<ID3D12Resource> scratchResource;

  bool bottom_level_build_desc_allocated = false;
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottom_level_build_desc{};
  bool bottom_level_prebuild_info_allocated = false;
  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO
      bottom_level_prebuild_info;
};

// Holds pointers to a model, textures, and a material
class SceneObject {
public:
  FLOAT *getTransform3x4();

  std::vector<Vertex> verticesVec;
  std::vector<Index> indicesVec;

  int id;

  Model *model;
  TextureBundle textures;
  Material *material;

  glm::vec3 translation; // parsed transform values
  glm::vec3 rotation;
  glm::vec3 scale;

private:
  FLOAT transform[3][4]; // instance desc transform
  bool transformBuilt = false;
};

struct Camera {
  int width;
  int height;
  glm::vec2 fov;

  glm::vec3 eye;
  glm::vec3 lookat;
  glm::vec3 up;
  glm::vec3 right;
  glm::vec3 forward;

  float focalDist;
  float lensRadius;

  int maxIterations;
  int maxDepth;
};
} // namespace ModelLoading