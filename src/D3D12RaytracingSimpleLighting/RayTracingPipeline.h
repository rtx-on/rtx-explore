#pragma once

class RaytracingPipeline {
public:
  CD3D12_STATE_OBJECT_DESC subobjects = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
  void CreateDxilLibrary(std::wstring hlsl_file,
                         std::wstring raygen_shader_name,
                         std::wstring closest_hit_shader_name,
                         std::wstring miss_shader_name);

  //triangle hit group, specifies which any hit, closest hit, export name
  void CreateHitGroup(std::wstring hit_group_export_name,
                      std::wstring closest_hit_shader_name,
                      std::wstring any_hit_shader_name,
                      std::wstring intersection_hit_shader_name);

  //creates both local root signature and exports association
  CD3D12_LOCAL_ROOT_SIGNATURE_SUBOBJECT* CreateLocalRootSignature(ComPtr<ID3D12Device> device, RootSignatureDesc desc);

  CD3D12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT* CreateGlobalRootSignature(ComPtr<ID3D12Device> device, RootSignatureDesc desc);

  CD3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT* CreateExportAssiocation(const D3D12_STATE_SUBOBJECT &subobject, std::vector<std::wstring> export_strings);

  CD3D12_RAYTRACING_SHADER_CONFIG_SUBOBJECT* CreateShaderConfig(UINT payload_size, UINT attribute_size);

  CD3D12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT* CreatePipelineConfig(UINT max_recursion_depth);

  ComPtr<ID3D12RaytracingFallbackStateObject> CreateFallbackStateObject(ComPtr<ID3D12RaytracingFallbackDevice> device);
  ComPtr<ID3D12StateObject> CreateDXRStateObject(ComPtr<ID3D12Device5> device);

};
