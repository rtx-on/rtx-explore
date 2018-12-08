#pragma once

#include <fstream>
#include <glm/glm/glm.hpp>
#include <iostream>
#include <sstream>
#include <vector>

#include "Model.h"

using namespace std;

class D3D12RaytracingSimpleLighting;

class Scene {
public:
  ifstream fp_in;

  void AllocateBufferOnGpu(void *pData, UINT64 width, ID3D12Resource **ppResource, std::wstring resource_name, CD3DX12_RESOURCE_DESC* resource_desc_ptr = nullptr);

  template<typename Callback>
  void RecurseGLTF(tinygltf::Model &model, tinygltf::Node &node, Callback callback);
  void ParseGLTF(std::string filename, bool make_light = true);
  void ParseScene(std::string filename);

  int loadMaterial(string materialid, std::string name = "");
  int loadDiffuseTexture(string texid);
  int loadNormalTexture(string texid);
  int loadModel(string modelid);
  int loadObject(string objectid, std::string name = "");
  int loadCamera();

  void LoadModelHelper(std::string path, int id, ModelLoading::Model& model);
  void LoadDiffuseTextureHelper(std::string path, int id, ModelLoading::Texture& newTexture);
  void LoadNormalTextureHelper(std::string path, int id, ModelLoading::Texture& newTexture);

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

  void AllocateResourcesInDescriptorHeap();

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

  std::map<int, ModelLoading::Model> modelMap;
  std::map<int, ModelLoading::Texture> diffuseTextureMap;
  std::map<int, ModelLoading::Texture> normalTextureMap;
  std::map<int, ModelLoading::MaterialResource> materialMap;

  ModelLoading::Camera camera;

  vector<ModelLoading::SceneObject> objects;
};
