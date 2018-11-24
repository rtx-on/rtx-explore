#pragma once

class RaytracingAcclerationStructure : public RaytracingDeviceHolder {
public:
  //--------------------------------------------------------------------------------------------------
  //
  // Create a bottom-level acceleration structure based on a list of vertex buffers in GPU memory
  // along with their vertex count. The build is then done in 3 steps: gathering the geometry,
  // computing the sizes of the required buffers, and building the actual AS
  //
  struct BottomLevelASPack
  {
    ComPtr<ID3D12Resource> vertex_buffer = nullptr;
    UINT vertex_element_size = 0;
    ComPtr<ID3D12Resource> index_buffer = nullptr;
    UINT index_element_count = 0;
    bool opaque = true;
  };

  AccelerationStructureBuffers CreateBottomLevelAS(std::vector<BottomLevelASPack> bottom_as_packs)
  {
    auto device = device_resources->GetD3DDevice();
    auto command_list = device_resources->GetCommandList();
    nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS;

    // Adding all vertex buffers and not transforming their position.
    for (const auto& bottom_as_pack : bottom_as_packs)
    {
      bottomLevelAS.AddVertexBuffer(
          bottom_as_pack.vertex_buffer.Get(), 0, static_cast<UINT>(bottom_as_pack.vertex_buffer->GetDesc().Width) / bottom_as_pack.vertex_element_size, bottom_as_pack.vertex_element_size, 
          bottom_as_pack.index_buffer.Get(), 0, bottom_as_pack.index_element_count, 
          nullptr, 0,
          bottom_as_pack.opaque);
    }

    // The AS build requires some scratch space to store temporary information. The amount of scratch
    // memory is dependent on the scene complexity.
    UINT64 scratchSizeInBytes = 0;
    // The final AS also needs to be stored in addition to the existing vertex buffers. It size is
    // also dependent on the scene complexity.
    UINT64 resultSizeInBytes = 0;
    bottomLevelAS.ComputeASBufferSizes(ray_tracing_api->IsFallback(), fallback_device, dxr_device, false, &scratchSizeInBytes, &resultSizeInBytes);

    // Once the sizes are obtained, the application is responsible for allocating the necessary
    // buffers. Since the entire generation will be done on the GPU, we can directly allocate those on
    // the default heap
    AccelerationStructureBuffers buffers;
    buffers.scratch = nv_helpers_dx12::CreateBuffer(
        device, scratchSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nv_helpers_dx12::kDefaultHeapProps);

    D3D12_RESOURCE_STATES initial_resource_state;

    if(ray_tracing_api->IsFallback())
    {
      initial_resource_state = fallback_device->GetAccelerationStructureResourceState();
    }
    else
    {
      initial_resource_state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    }

    buffers.accelerationStructure = nv_helpers_dx12::CreateBuffer(
        device, resultSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        initial_resource_state, nv_helpers_dx12::kDefaultHeapProps);

    // Before Win10 RS5, we need to cast the command list into a raytracing command list to access the
    // AS building method IGNORED
    //ID3D12CommandListRaytracingPrototype* rtCmdList;
    //ThrowIfFailed(m_commandList->QueryInterface(IID_PPV_ARGS(&rtCmdList)));

    // Build the acceleration structure. Note that this call integrates a barrier
    // on the generated AS, so that it can be used to compute a top-level AS right
    // after this method.
    //TODO update ID3D12DESCRIPTORHEAP passed in
    bottomLevelAS.Generate(command_list, ray_tracing_api->IsFallback(), ComPtr<ID3D12DescriptorHeap>(), fallback_command_list, dxr_command_list, buffers.scratch.Get(),
                           buffers.accelerationStructure.Get(), false, nullptr);

    return buffers;
  }

  //--------------------------------------------------------------------------------------------------
  // Create the main acceleration structure that holds all instances of the scene. Similarly to the
  // bottom-level AS generation, it is done in 3 steps: gathering the instances, computing the memory
  // requirements for the AS, and building the AS itself
  //

  struct TopLevelASPack
  {
    ComPtr<ID3D12Resource> bottom_level_as_buffer; //buffers.accelerationStructure
    DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity();
    UINT instance_id = 0;
    UINT hit_group_id = 0;
  };

  void CreateTopLevelAS(std::vector<TopLevelASPack> instances) // pair of bottom level AS and matrix of the instance
  {
    auto device = device_resources->GetD3DDevice();
    auto command_list = device_resources->GetCommandList();
    nv_helpers_dx12::TopLevelASGenerator topLevelAS;
    // Gather all the instances into the builder helper
    for (size_t i = 0; i < instances.size(); i++)
    {
      topLevelAS.AddInstance(instances[i].bottom_level_as_buffer.Get(), instances[i].transform,
                                       instances[i].instance_id, instances[i].hit_group_id);
    }

    // As for the bottom-level AS, the building the AS requires some scratch space to store temporary
    // data in addition to the actual AS. In the case of the top-level AS, the instance descriptors
    // also need to be stored in GPU memory. This call outputs the memory requirements for each
    // (scratch, results, instance descriptors) so that the application can allocate the corresponding
    // memory
    UINT64 scratchSize, resultSize, instanceDescsSize;
    topLevelAS.ComputeASBufferSizes(ray_tracing_api->IsFallback(), fallback_device, dxr_device, true, &scratchSize, &resultSize,
                                             &instanceDescsSize);

    // Create the scratch and result buffers. Since the build is all done on GPU, those can be
    // allocated on the default heap
    AccelerationStructureBuffers buffers;
    buffers.scratch = nv_helpers_dx12::CreateBuffer(
        device, scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nv_helpers_dx12::kDefaultHeapProps);

    D3D12_RESOURCE_STATES initial_resource_state;

    if(ray_tracing_api->IsFallback())
    {
      initial_resource_state = fallback_device->GetAccelerationStructureResourceState();
    }
    else
    {
      initial_resource_state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    }

    buffers.accelerationStructure = nv_helpers_dx12::CreateBuffer(
        device, resultSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        initial_resource_state, nv_helpers_dx12::kDefaultHeapProps);

    // The buffer describing the instances: ID, shader binding information, matrices ... Those will be
    // copied into the buffer by the helper through mapping, so the buffer has to be allocated on the
    // upload heap.
    buffers.instanceDesc = nv_helpers_dx12::CreateBuffer(
        device, instanceDescsSize, D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

    // Before Win10 RS5, we need to cast the command list into a raytracing command list to access the
    // AS building method
    ID3D12CommandListRaytracingPrototype* rtCmdList;
    ThrowIfFailed(m_commandList->QueryInterface(IID_PPV_ARGS(&rtCmdList)));

    // After all the buffers are allocated, or if only an update is required, we can build the
    // acceleration structure. Note that in the case of the update we also pass the existing AS as the
    // 'previous' AS, so that it can be refitted in place.
    m_topLevelASGenerator.Generate(m_commandList.Get(), rtCmdList, m_topLevelASBuffers.pScratch.Get(),
                                 m_topLevelASBuffers.pResult.Get(),
                                 m_topLevelASBuffers.pInstanceDesc.Get());
  }

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
    for (uint32_t i = 0; i < 3; i++)
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
