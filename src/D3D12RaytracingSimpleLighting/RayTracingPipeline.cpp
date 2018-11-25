#include "stdafx.h"
#include "RaytracingPipeline.h"

void RaytracingPipeline::CreateDxilLibrary(std::wstring hlsl_file,
                                           std::wstring raygen_shader_name,
                                           std::wstring closest_hit_shader_name,
                                           std::wstring miss_shader_name)
{
  CD3D12_DXIL_LIBRARY_SUBOBJECT* dxil_library_subobject = subobjects.CreateSubobject<CD3D12_DXIL_LIBRARY_SUBOBJECT>();

  //TODO change this
  //Find compiled shader
  D3D12_SHADER_BYTECODE shader_bytecode = CD3DX12_SHADER_BYTECODE((void *)g_pRaytracing, ARRAYSIZE(g_pRaytracing));
  
  //set the library to be the shader
  dxil_library_subobject->SetDXILLibrary(&shader_bytecode);

  dxil_library_subobject->DefineExport(raygen_shader_name.c_str());
  dxil_library_subobject->DefineExport(closest_hit_shader_name.c_str());
  dxil_library_subobject->DefineExport(miss_shader_name.c_str());
}

void RaytracingPipeline::CreateHitGroup(std::wstring hit_group_export_name,
                                        std::wstring closest_hit_shader_name,
                                        std::wstring any_hit_shader_name,
                                        std::wstring intersection_hit_shader_name
                                        )
{
  CD3D12_HIT_GROUP_SUBOBJECT* hit_group_subobject = subobjects.CreateSubobject<CD3D12_HIT_GROUP_SUBOBJECT>();
  if (!hit_group_export_name.empty())
  {
    hit_group_subobject->SetHitGroupExport(hit_group_export_name.c_str());    
  }
  if (!closest_hit_shader_name.empty())
  {
    hit_group_subobject->SetClosestHitShaderImport(closest_hit_shader_name.c_str());    
  }
  if (!any_hit_shader_name.empty())
  {
    hit_group_subobject->SetAnyHitShaderImport(any_hit_shader_name.c_str());
  }
  if (!intersection_hit_shader_name.empty())
  {
    hit_group_subobject->SetIntersectionShaderImport(intersection_hit_shader_name.c_str());
  }
}

CD3D12_LOCAL_ROOT_SIGNATURE_SUBOBJECT* RaytracingPipeline::CreateLocalRootSignature(ComPtr<ID3D12Device> device)
{
  CD3D12_LOCAL_ROOT_SIGNATURE_SUBOBJECT* local_root_signature_subobject = subobjects.CreateSubobject<CD3D12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();

  // Local root signature to be used in a hit group.
  ComPtr<ID3D12RootSignature> root_signature;
  //desc.SerializeAndCreateRaytracingRootSignature(device, &root_signature);
  local_root_signature_subobject->SetRootSignature(root_signature.Get());

  return local_root_signature_subobject;
}

CD3D12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT* RaytracingPipeline::CreateGlobalRootSignature(ComPtr<ID3D12Device> device)
{
  CD3D12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT* global_root_signature_subobject = subobjects.CreateSubobject<CD3D12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
  
  ComPtr<ID3D12RootSignature> root_signature;
  //desc.SerializeAndCreateRaytracingRootSignature(device, &root_signature);
  global_root_signature_subobject->SetRootSignature(root_signature.Get());

  return global_root_signature_subobject;
}

CD3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT* RaytracingPipeline::CreateExportAssiocation(
    const D3D12_STATE_SUBOBJECT &subobject,
    std::vector<std::wstring> export_strings)
{
  CD3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT* association_subobject = subobjects.CreateSubobject<CD3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
  association_subobject->SetSubobjectToAssociate(subobject);
  for (const auto& export_string : export_strings)
  {
    association_subobject->AddExport(export_string.c_str());
  }

  return association_subobject;
}

CD3D12_RAYTRACING_SHADER_CONFIG_SUBOBJECT* RaytracingPipeline::CreateShaderConfig(UINT payload_size,
                                            UINT attribute_size)
{
  CD3D12_RAYTRACING_SHADER_CONFIG_SUBOBJECT* shader_config_subobject = subobjects.CreateSubobject<CD3D12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
  shader_config_subobject->Config(payload_size, attribute_size);

  return shader_config_subobject;
}

CD3D12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT* RaytracingPipeline::CreatePipelineConfig(UINT max_recursion_depth)
{
  CD3D12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT* pipeline_config_subobject = subobjects.CreateSubobject<CD3D12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
  pipeline_config_subobject->Config(max_recursion_depth);

  return pipeline_config_subobject;
}

ComPtr<ID3D12RaytracingFallbackStateObject> RaytracingPipeline::CreateFallbackStateObject(
    ComPtr<ID3D12RaytracingFallbackDevice> device)
{
  ComPtr<ID3D12RaytracingFallbackStateObject> fallback_state_object;
  device->CreateStateObject(subobjects, IID_PPV_ARGS(&fallback_state_object));
  return fallback_state_object;
}

ComPtr<ID3D12StateObject>
RaytracingPipeline::CreateDXRStateObject(ComPtr<ID3D12Device5> device) {
  ComPtr<ID3D12StateObject> state_object;
  device->CreateStateObject(subobjects, IID_PPV_ARGS(&state_object));
  return state_object;
}
