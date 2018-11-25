#pragma once

class RaytracingAcclerationStructures : public RaytracingDeviceHolder {
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

  struct BottomLevelAsPreInfo
  {
    nv_helpers_dx12::BottomLevelASGenerator bottom_level_as_generator;
    UINT64 scratchSize, resultSize;
  };

  std::unique_ptr<BottomLevelAsPreInfo> CreateBottomLevelAS(std::vector<BottomLevelASPack> bottom_as_packs)
  {

    std::unique_ptr<BottomLevelAsPreInfo> bottom_level_as_pre_info = std::make_unique<BottomLevelAsPreInfo>();

    // Adding all vertex buffers and not transforming their position.
    for (const auto& bottom_as_pack : bottom_as_packs)
    {
      bottom_level_as_pre_info->bottom_level_as_generator.AddVertexBuffer(
          bottom_as_pack.vertex_buffer.Get(), 0, static_cast<UINT>(bottom_as_pack.vertex_buffer->GetDesc().Width) / bottom_as_pack.vertex_element_size, bottom_as_pack.vertex_element_size, 
          bottom_as_pack.index_buffer.Get(), 0, bottom_as_pack.index_element_count, 
          nullptr, 0,
          bottom_as_pack.opaque);
    }

    // The AS build requires some scratch space to store temporary information. The amount of scratch
    // memory is dependent on the scene complexity.
    //UINT64 scratchSizeInBytes = 0;
    // The final AS also needs to be stored in addition to the existing vertex buffers. It size is
    // also dependent on the scene complexity.
    //UINT64 resultSizeInBytes = 0;
    bottom_level_as_pre_info->bottom_level_as_generator.ComputeASBufferSizes(ray_tracing_api->IsFallback(), fallback_device, dxr_device, false, &bottom_level_as_pre_info->scratchSize, &bottom_level_as_pre_info->resultSize);

    return bottom_level_as_pre_info;
  }

  AccelerationStructureBuffers AllocateBottomLevelAS(std::unique_ptr<BottomLevelAsPreInfo> bottom_level_as_pre_info, ComPtr<ID3D12DescriptorHeap> descriptor_heap) 
  {
    auto device = device_resources->GetD3DDevice();
    auto command_list = device_resources->GetCommandList();
   
    // Once the sizes are obtained, the application is responsible for allocating the necessary
    // buffers. Since the entire generation will be done on the GPU, we can directly allocate those on
    // the default heap
    AccelerationStructureBuffers buffers;
    buffers.scratch = nv_helpers_dx12::CreateBuffer(
        device, bottom_level_as_pre_info->scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
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
        device, bottom_level_as_pre_info->resultSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        initial_resource_state, nv_helpers_dx12::kDefaultHeapProps);

    buffers.ResultDataMaxSizeInBytes = bottom_level_as_pre_info->resultSize;

    // Before Win10 RS5, we need to cast the command list into a raytracing command list to access the
    // AS building method IGNORED
    //ID3D12CommandListRaytracingPrototype* rtCmdList;
    //ThrowIfFailed(m_commandList->QueryInterface(IID_PPV_ARGS(&rtCmdList)));

    // Build the acceleration structure. Note that this call integrates a barrier
    // on the generated AS, so that it can be used to compute a top-level AS right
    // after this method.
    //TODO update ID3D12DESCRIPTORHEAP passed in
    bottom_level_as_pre_info->bottom_level_as_generator.Generate(command_list, ray_tracing_api->IsFallback(), descriptor_heap, fallback_command_list, dxr_command_list, buffers.scratch.Get(),
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
    AccelerationStructureBuffers bottom_level_as_buffer; //buffers.accelerationStructure
    DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity();

    UINT instance_id = 0;
    UINT hit_group_id = 0;
  };

  struct TopLevelAsPreInfo
  {
    nv_helpers_dx12::TopLevelASGenerator top_level_as_generator;
    UINT64 scratchSize, resultSize, instanceDescsSize;
  };

  std::unique_ptr<TopLevelAsPreInfo> ComputePreInfoTopLevelAS(UINT instances) // pair of bottom level AS and matrix of the instance
  {
    std::unique_ptr<TopLevelAsPreInfo> top_level_as_pre_info = std::make_unique<TopLevelAsPreInfo>();

    for (size_t i = 0; i < instances; i++)
    {
      top_level_as_pre_info->top_level_as_generator.AddInstance(nv_helpers_dx12::Instance{});
    }

    // As for the bottom-level AS, the building the AS requires some scratch space to store temporary
    // data in addition to the actual AS. In the case of the top-level AS, the instance descriptors
    // also need to be stored in GPU memory. This call outputs the memory requirements for each
    // (scratch, results, instance descriptors) so that the application can allocate the corresponding
    // memory
    top_level_as_pre_info->top_level_as_generator.ComputeASBufferSizes(ray_tracing_api->IsFallback(), fallback_device, dxr_device, true, &top_level_as_pre_info->scratchSize, &top_level_as_pre_info->resultSize,
                                             &top_level_as_pre_info->instanceDescsSize);

    top_level_as_pre_info->top_level_as_generator.ClearInstances();

    return top_level_as_pre_info;
  }

  AccelerationStructureBuffers AllocateTopLevelAS(std::unique_ptr<TopLevelAsPreInfo> top_level_as_pre_info, std::vector<nv_helpers_dx12::Instance> instances) 
  {
    auto device = device_resources->GetD3DDevice();
    auto command_list = device_resources->GetCommandList();
    
    for (size_t i = 0; i < instances.size(); i++)
    {
      top_level_as_pre_info->top_level_as_generator.AddInstance(instances[i]);
    }

    // Create the scratch and result buffers. Since the build is all done on GPU, those can be
    // allocated on the default heap
    AccelerationStructureBuffers buffers;
    buffers.scratch = nv_helpers_dx12::CreateBuffer(
        device, top_level_as_pre_info->scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
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
        device, top_level_as_pre_info->resultSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        initial_resource_state, nv_helpers_dx12::kDefaultHeapProps);

    // The buffer describing the instances: ID, shader binding information, matrices ... Those will be
    // copied into the buffer by the helper through mapping, so the buffer has to be allocated on the
    // upload heap.
    buffers.instanceDesc = nv_helpers_dx12::CreateBuffer(
        device, top_level_as_pre_info->instanceDescsSize, D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

    buffers.ResultDataMaxSizeInBytes = top_level_as_pre_info->resultSize;

    // Before Win10 RS5, we need to cast the command list into a raytracing command list to access the
    // AS building method
    //ID3D12CommandListRaytracingPrototype* rtCmdList;
    //ThrowIfFailed(m_commandList->QueryInterface(IID_PPV_ARGS(&rtCmdList)));

    // After all the buffers are allocated, or if only an update is required, we can build the
    // acceleration structure. Note that in the case of the update we also pass the existing AS as the
    // 'previous' AS, so that it can be refitted in place.
    top_level_gpu_pointer = top_level_as_pre_info->top_level_as_generator.Generate(device, fallback_device, command_list, ray_tracing_api->IsFallback(), fallback_command_list, dxr_command_list, heap_handle,
      buffers.scratch.Get(), buffers.accelerationStructure.Get(), buffers.ResultDataMaxSizeInBytes, buffers.instanceDesc.Get());

    return buffers;
  }

  WRAPPED_GPU_POINTER top_level_gpu_pointer;
};
