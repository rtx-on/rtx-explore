#pragma once

class RaytracingAcclerationStructure {
public:
  AccelerationStructureBuffers CreateFallbackBottomLevelAccelerationStructure(ComPtr<ID3D12RaytracingFallbackCommandList> fallback_command_list, ComPtr<ID3D12GraphicsCommandList> command_list, ComPtr<ID3D12RaytracingFallbackDevice> fallback_device, ComPtr<ID3D12Device> device, ComPtr<ID3D12Resource> vertices, UINT vertices_count, UINT vertex_stride, ComPtr<ID3D12Resource> indices, UINT indices_count)
  {
    D3D12_RAYTRACING_GEOMETRY_DESC geometry_desc{};
    geometry_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    
    // Mark the geometry as opaque. 
    // PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
    // Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
    geometry_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    geometry_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometry_desc.Triangles.VertexCount = vertices_count;
    geometry_desc.Triangles.VertexBuffer.StartAddress = vertices->GetGPUVirtualAddress();
    geometry_desc.Triangles.VertexBuffer.StrideInBytes = vertex_stride;

    if (indices != nullptr)
    {
      geometry_desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
      geometry_desc.Triangles.IndexBuffer = indices->GetGPUVirtualAddress();
      geometry_desc.Triangles.IndexCount = indices_count;
    }

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags =
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = build_flags;
    inputs.NumDescs = 1;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.pGeometryDescs = &geometry_desc;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info{};
    fallback_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

    AccelerationStructureBuffers acceleration_structure_buffers;
    acceleration_structure_buffers.scratch = ResourceHelper::CreateBuffer(
        device, prebuild_info.ScratchDataSizeInBytes,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), 
        L"bottom level as structure",
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    D3D12_RESOURCE_STATES initial_resource_state = fallback_device->GetAccelerationStructureResourceState();

    acceleration_structure_buffers.accelerationStructure = ResourceHelper::CreateBuffer(
        device, prebuild_info.ScratchDataSizeInBytes,
        initial_resource_state,
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), 
        L"bottom level as structure",
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC acceleration_structure_desc{};
    acceleration_structure_desc.ScratchAccelerationStructureData =
        acceleration_structure_buffers.scratch->GetGPUVirtualAddress();
    acceleration_structure_desc.DestAccelerationStructureData =
        acceleration_structure_buffers.accelerationStructure->GetGPUVirtualAddress();
    acceleration_structure_desc.Inputs = inputs;

    fallback_command_list->BuildRaytracingAccelerationStructure(
        &acceleration_structure_desc, 0, nullptr);

    command_list->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::UAV(
               acceleration_structure_buffers.accelerationStructure.Get()));

    return acceleration_structure_buffers;
  }

  AccelerationStructureBuffers CreateFallbackTopLevelAccelerationStructure(ComPtr<ID3D12RaytracingFallbackCommandList> fallback_command_list, ComPtr<ID3D12GraphicsCommandList> command_list, ComPtr<ID3D12RaytracingFallbackDevice> fallback_device, ComPtr<ID3D12Device> device, ComPtr<ID3D12Resource> bottom_level_acceleration_structure_resource)
  {
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags =
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    //change this to number of instances you want
    UINT num_instances = 1;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = build_flags;
    inputs.NumDescs = num_instances;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info{};
    fallback_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

    AccelerationStructureBuffers acceleration_structure_buffers;
    acceleration_structure_buffers.scratch = ResourceHelper::CreateBuffer(
        device, prebuild_info.ScratchDataSizeInBytes,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), 
        L"top level as structure",
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    D3D12_RESOURCE_STATES initial_resource_state = fallback_device->GetAccelerationStructureResourceState();

    acceleration_structure_buffers.accelerationStructure = ResourceHelper::CreateBuffer(
        device, prebuild_info.ScratchDataSizeInBytes,
        initial_resource_state,
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), 
        L"top level as structure",
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    acceleration_structure_buffers.instanceDesc = ResourceHelper::CreateBuffer(
        device, 
        sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * num_instances,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), 
        L"top level as structure",
        D3D12_RESOURCE_FLAG_NONE);

    //map to cpu
    D3D12_RAYTRACING_INSTANCE_DESC *raytracing_instance_desc;
    acceleration_structure_buffers.instanceDesc->Map(
        0, nullptr, reinterpret_cast<void**>(&raytracing_instance_desc));

    //glm::mat4 369
    //TODO matrices
    for (uint32_t i = 0; i <13; i++)
    {
        raytracing_instance_desc[i].InstanceID = i; // This value will be exposed to the shader via InstanceID()
        raytracing_instance_desc[i].InstanceContributionToHitGroupIndex = 0; // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
        raytracing_instance_desc[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        glm::mat4 m = glm::transpose(glm::mat4()); // GLM is column major, the INSTANCE_DESC is row major
        memcpy(raytracing_instance_desc[i].Transform, &m, sizeof(raytracing_instance_desc[i].Transform));
        raytracing_instance_desc[i].AccelerationStructure = bottom_level_acceleration_structure_resource->GetGPUVirtualAddress();
        raytracing_instance_desc[i].InstanceMask = 0xFF;
    }

    acceleration_structure_buffers.instanceDesc->Unmap(0, nullptr);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC acceleration_structure_desc{};
    acceleration_structure_desc.ScratchAccelerationStructureData =
        acceleration_structure_buffers.scratch->GetGPUVirtualAddress();
    acceleration_structure_desc.DestAccelerationStructureData =
        acceleration_structure_buffers.accelerationStructure->GetGPUVirtualAddress();
    acceleration_structure_desc.Inputs = inputs;
    acceleration_structure_desc.Inputs.InstanceDescs = 
        acceleration_structure_buffers.instanceDesc->GetGPUVirtualAddress();

    fallback_command_list->BuildRaytracingAccelerationStructure(
        &acceleration_structure_desc, 0, nullptr);

    command_list->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::UAV(
               acceleration_structure_buffers.accelerationStructure.Get()));

    return acceleration_structure_buffers;
  }

  void CreateFallbackAccelerationStructure(
      CommandQueue& queue,
      ComPtr<ID3D12RaytracingFallbackCommandList> fallback_command_list,
      ComPtr<ID3D12RaytracingFallbackDevice> fallback_device,
      ComPtr<ID3D12Resource> vertices,
      UINT vertices_count, UINT vertex_stride, ComPtr<ID3D12Resource> indices,
      UINT indices_count)
  {
    ComPtr<ID3D12GraphicsCommandList> command_list = *queue.GetCommandList();
    ComPtr<ID3D12Device5> device = queue.GetDevice();
    AccelerationStructureBuffers fallback_bottom_level_acceleration_structure = CreateFallbackBottomLevelAccelerationStructure(
      fallback_command_list, command_list, fallback_device, device, vertices,
      vertices_count, vertex_stride, indices, indices_count);

    AccelerationStructureBuffers fallback_top_level_acceleration_structure = CreateFallbackTopLevelAccelerationStructure(
      fallback_command_list, command_list, fallback_device, device,
      fallback_bottom_level_acceleration_structure.accelerationStructure);

    //TODO 971 R
    //descriptor heap

    queue.Flush();

    bottom_level_acceleration_structure = fallback_bottom_level_acceleration_structure.accelerationStructure;
    top_level_acceleration_structure = fallback_top_level_acceleration_structure.accelerationStructure;
  }

  ComPtr<ID3D12Resource> bottom_level_acceleration_structure;
  ComPtr<ID3D12Resource> top_level_acceleration_structure;
};
