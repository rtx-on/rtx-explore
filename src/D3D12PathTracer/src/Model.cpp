#include "stdafx.h"

#include "Model.h"
#include "DXSample.h"
#include "Utilities.h"
#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/type_ptr.hpp>

using namespace ModelLoading;

D3D12_RAYTRACING_GEOMETRY_DESC& Model::GetGeomDesc()
{
  
    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Triangles.IndexBuffer =
        indices.resource->GetGPUVirtualAddress();
    geometryDesc.Triangles.IndexCount =
        static_cast<UINT>(indices.resource->GetDesc().Width) / sizeof(Index);
    geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    geometryDesc.Triangles.Transform3x4 = 0;
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Triangles.VertexCount =
        static_cast<UINT>(vertices.resource->GetDesc().Width) / sizeof(Vertex);
    geometryDesc.Triangles.VertexBuffer.StartAddress =
        vertices.resource->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
    geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    return geometryDesc;
}

D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& Model::GetBottomLevelBuildDesc()
{
  
    if (!bottom_level_build_desc_allocated)
    {
      D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS &bottomLevelInputs =
          bottom_level_build_desc.Inputs;
      bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
      bottomLevelInputs.Flags =
          D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
      bottomLevelInputs.NumDescs = 1; // WATCHOUT
      bottomLevelInputs.Type =
          D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
      D3D12_RAYTRACING_GEOMETRY_DESC& d3_d12_raytracing_geometry_desc = GetGeomDesc();
      bottomLevelInputs.pGeometryDescs = &d3_d12_raytracing_geometry_desc;
      bottom_level_build_desc_allocated = true;
    }
    return bottom_level_build_desc;
}

D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO& Model::GetPreBuild(bool is_fallback, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice, ComPtr<ID3D12Device5> m_dxrDevice)
{
  
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& bottomLevelBuildDesc =
        GetBottomLevelBuildDesc();
    if (!bottom_level_prebuild_info_allocated)
    {
      if (is_fallback) {
        m_fallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(
          &bottomLevelBuildDesc.Inputs, &bottom_level_prebuild_info);
      } 
      else // DirectX Raytracing
      {
        m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(
            &bottomLevelBuildDesc.Inputs, &bottom_level_prebuild_info);
      }
      ThrowIfFalse(bottom_level_prebuild_info.ResultDataMaxSizeInBytes > 0);

      bottom_level_prebuild_info_allocated = true;
    }

    return bottom_level_prebuild_info;
}

ComPtr<ID3D12Resource> Model::GetBottomLevelScratchAS(bool is_fallback, ComPtr<ID3D12Device> device, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice, ComPtr<ID3D12Device5> m_dxrDevice)
{
  if (!is_scratchResource_allocated)
  {
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO&
        bottomLevelPrebuildInfo =
            GetPreBuild(is_fallback, m_fallbackDevice, m_dxrDevice);
    AllocateUAVBuffer(
        device.Get(), bottomLevelPrebuildInfo.ScratchDataSizeInBytes,
        &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        utilityCore::stringAndId(L"ScratchResource", id).c_str());
    is_scratchResource_allocated = true;
  }

    return scratchResource;
}

ComPtr<ID3D12Resource> Model::GetBottomAS(bool is_fallback, ComPtr<ID3D12Device> device, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice, ComPtr<ID3D12Device5> m_dxrDevice)
{
  if (!is_m_bottomLevelAccelerationStructure_allocated)
  {
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO&
    bottomLevelPrebuildInfo =
        GetPreBuild(is_fallback, m_fallbackDevice, m_dxrDevice);
    D3D12_RESOURCE_STATES initialResourceState;
    if (is_fallback) {
      initialResourceState =
          m_fallbackDevice->GetAccelerationStructureResourceState();
    } else // DirectX Raytracing
    {
      initialResourceState =
          D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    }

    AllocateUAVBuffer(device.Get(),
                      bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes,
                      &m_bottomLevelAccelerationStructure, initialResourceState,
                      utilityCore::stringAndId(L"BottomLevelAS", id).c_str());
    
    is_m_bottomLevelAccelerationStructure_allocated = true;

  }

    return m_bottomLevelAccelerationStructure;
}

WRAPPED_GPU_POINTER Model::GetFallBackWrappedPoint(D3D12RaytracingSimpleLighting* programState, bool is_fallback, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice, ComPtr<ID3D12Device5> m_dxrDevice, UINT numBufferElements)
{
    if (!is_gpu_ptr_allocated)
    {
      gpuPtr = programState->CreateFallbackWrappedPointer(GetBottomAS(is_fallback, programState->GetDeviceResources()->GetD3DDevice(), m_fallbackDevice, m_dxrDevice).Get(), numBufferElements);
      is_gpu_ptr_allocated = true;
    }
    return gpuPtr;
}

void Model::FinalizeAS()
{
    auto& bottomLevelBuildDesc = GetBottomLevelBuildDesc();
    bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
    bottomLevelBuildDesc.DestAccelerationStructureData = m_bottomLevelAccelerationStructure->GetGPUVirtualAddress();
}

FLOAT* SceneObject::getTransform3x4() {
	if (!transformBuilt) {
		transformBuilt = true;
		const float *matrix = (const float*)glm::value_ptr(utilityCore::buildTransformationMatrix(translation, rotation, scale));

		transform[0][0] = matrix[0];
		transform[0][1] = matrix[4];
		transform[0][2] = matrix[8];
		transform[0][3] = matrix[12];

		transform[1][0] = matrix[1];
		transform[1][1] = matrix[5];
		transform[1][2] = matrix[9];
		transform[1][3] = matrix[13];

		transform[2][0] = matrix[2];
		transform[2][1] = matrix[6];
		transform[2][2] = matrix[10];
		transform[2][3] = matrix[14];
	}
	return &transform[0][0];
}
