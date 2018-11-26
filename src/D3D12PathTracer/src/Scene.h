#pragma once

#include <fstream>
#include <glm/glm/glm.hpp>
#include <iostream>
#include <sstream>
#include <vector>

#include "Model.h"

using namespace std;

// static glm::vec3 calculate_geometric_normals(glm::vec3 p0, glm::vec3 p1,
// glm::vec3 p2);

class D3D12RaytracingSimpleLighting;

class Scene {
public:
  ifstream fp_in;
  int loadMaterial(string materialid);
  int loadTexture(string texid);
  int loadModel(string modelid);
  int loadObject(string objectid);
  int loadCamera();

  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC &GetTopLevelDesc();

  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO &
  GetTopLevelPrebuildInfo(
      bool is_fallback, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice,
      ComPtr<ID3D12Device5> m_dxrDevice);

  ComPtr<ID3D12Resource>
  GetTopLevelScratchAS(bool is_fallback, ComPtr<ID3D12Device> device,
                       ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice,
                       ComPtr<ID3D12Device5> m_dxrDevice);

  ComPtr<ID3D12Resource>
  GetTopAS(bool is_fallback, ComPtr<ID3D12Device> device,
           ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice,
           ComPtr<ID3D12Device5> m_dxrDevice);

  ComPtr<ID3D12Resource> GetInstanceDescriptors(
      bool is_fallback, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice,
      ComPtr<ID3D12Device5> m_dxrDevice);

  WRAPPED_GPU_POINTER
  GetWrappedGPUPointer(bool is_fallback,
                       ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice,
                       ComPtr<ID3D12Device5> m_dxrDevice);

  void FinalizeAS();

  void BuildAllAS(bool is_fallback,
                  ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice,
                  ComPtr<ID3D12Device5> m_dxrDevice,
                  ComPtr<ID3D12RaytracingFallbackCommandList> fbCmdLst,
                  ComPtr<ID3D12GraphicsCommandList5> rtxCmdList);

  void AllocateVerticesAndIndices();

  ComPtr<ID3D12Resource> m_topLevelAccelerationStructure;
  ComPtr<ID3D12Resource> scratchResource;
  ComPtr<ID3D12Resource> instanceDescs;
  bool top_level_build_desc_allocated = false;
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC top_level_build_desc{};
  bool top_level_prebuild_info_allocated = false;
  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO top_level_prebuild_info;

  Scene(string filename, D3D12RaytracingSimpleLighting *programState);
  ~Scene();

  D3D12RaytracingSimpleLighting *programState;

  std::unordered_map<int, ModelLoading::Model> modelMap;
  std::unordered_map<int, ModelLoading::Texture> textureMap;
  std::unordered_map<int, ModelLoading::Material> materialMap;

  ModelLoading::Camera camera;

  vector<ModelLoading::SceneObject> objects;
};