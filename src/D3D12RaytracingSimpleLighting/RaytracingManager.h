#pragma once
class RaytracingManager {
public:
  RaytracingManager();
  ~RaytracingManager();

    Index indices[] =
    {
        3,1,0,
        2,1,3,

        6,4,5,
        7,4,6,

        11,9,8,
        10,9,11,

        14,12,13,
        15,12,14,

        19,17,16,
        18,17,19,

        22,20,21,
        23,20,22
    };

    // Cube vertices positions and corresponding triangle normals.
    Vertex vertices[] =
    {
        { XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
        { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
        { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },

        { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
        { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
        { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
        { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },

        { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
        { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
        { XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },

        { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
        { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
        { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
        { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },

        { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
        { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
        { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
        { XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },

        { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
        { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
        { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
    };

  void Init(DXSample* sample, UINT frame_count)
  {
    this->sample = sample;
    this->frame_count = frame_count;

    device_resources = std::make_shared<DX::DeviceResources>(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_UNKNOWN,
        frame_count,
        D3D_FEATURE_LEVEL_11_0,
        // Sample shows handling of use cases with tearing support, which is OS dependent and has been supported since TH2.
        // Since the Fallback Layer requires Fall Creator's update (RS3), we don't need to handle non-tearing cases.
        DX::DeviceResources::c_RequireTearingSupport,
        sample->m_adapterIDoverride
        );
    device_resources->RegisterDeviceNotify(sample);
    device_resources->SetWindow(Win32Application::GetHwnd(), sample->GetWidth(), sample->GetHeight());
    device_resources->InitializeDXGIAdapter();

    ray_tracing_api = std::make_shared<RaytracingApi>();
    ray_tracing_api->CheckDXRSupport(device_resources->GetAdapter());

    device_resources->CreateDeviceResources();
    device_resources->CreateWindowSizeDependentResources();

  }
  // Update camera matrices passed into the shader.
void RaytracingManager::UpdateCameraMatrices(XMVECTOR eye, XMVECTOR at, XMVECTOR up)
{
    m_eye = eye;
    m_at = at;
    m_up = up;
    auto frameIndex = device_resources->GetCurrentFrameIndex();

    m_sceneCB[frameIndex].cameraPosition = m_eye;
    float fovAngleY = 45.0f;
    XMMATRIX view = XMMatrixLookAtLH(m_eye, m_at, m_up);
    float m_aspectRatio = 1.3;
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), m_aspectRatio, 1.0f, 125.0f);
    XMMATRIX viewProj = view * proj;

    m_sceneCB[frameIndex].projectionToWorld = XMMatrixInverse(nullptr, viewProj);
}
  // Initialize scene rendering parameters.
void InitializeScene()
{
    auto frameIndex = device_resources->GetCurrentFrameIndex();

    // Setup camera.
    {
        // Initialize the view and projection inverse matrices.
        m_eye = { 0.0f, 2.0f, -5.0f, 1.0f };
        m_at = { 0.0f, 0.0f, 0.0f, 1.0f };
        XMVECTOR right = { 1.0f, 0.0f, 0.0f, 0.0f };

        XMVECTOR direction = XMVector4Normalize(m_at - m_eye);
        m_up = XMVector3Normalize(XMVector3Cross(direction, right));

        // Rotate camera around Y axis.
        XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(45.0f));
        m_eye = XMVector3Transform(m_eye, rotate);
        m_up = XMVector3Transform(m_up, rotate);
        
        UpdateCameraMatrices(m_eye, m_at, m_up);
    }

    // Setup lights.
    {
        // Initialize the lighting parameters.
        XMFLOAT4 lightPosition;
        XMFLOAT4 lightAmbientColor;
        XMFLOAT4 lightDiffuseColor;

        lightPosition = XMFLOAT4(0.0f, 1.8f, -3.0f, 0.0f);
        m_sceneCB[frameIndex].lightPosition = XMLoadFloat4(&lightPosition);

        lightAmbientColor = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
        m_sceneCB[frameIndex].lightAmbientColor = XMLoadFloat4(&lightAmbientColor);

        lightDiffuseColor = XMFLOAT4(0, 0.0f, 0.5f, 1.0f);
        m_sceneCB[frameIndex].lightDiffuseColor = XMLoadFloat4(&lightDiffuseColor);
    }

    // Apply the initial values to all frames' buffer instances.
    for (auto& sceneCB : m_sceneCB)
    {
        sceneCB = m_sceneCB[frameIndex];
    }
}

  // Create raytracing device and command list.
  void CreateRaytracingInterfaces()
  {
      auto device = device_resources->GetD3DDevice();
      auto commandList = device_resources->GetCommandList();

      if (*ray_tracing_api == RaytracingApiType::Fallback)
      {
          CreateRaytracingFallbackDeviceFlags createDeviceFlags =
            CreateRaytracingFallbackDeviceFlags::None;
          //TODO test CreateRaytracingFallbackDeviceFlags::ForceComputeFallback
          ThrowIfFailed(D3D12CreateRaytracingFallbackDevice(device, createDeviceFlags, 0, IID_PPV_ARGS(&fallback_device)));
          fallback_device->QueryRaytracingCommandList(commandList, IID_PPV_ARGS(&fallback_command_list));
      }
      else // DirectX Raytracing
      {
          ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&dxr_device)), L"Couldn't get DirectX Raytracing interface for the device.\n");
          ThrowIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&dxr_command_list)), L"Couldn't get DirectX Raytracing interface for the command list.\n");
      }
  }

  template<typename VertexType>
  void CreatePreInfoBottomASVertices(std::vector<VertexType> vertices, std::vector<UINT> indices)
  {
    auto device = device_resources->GetD3DDevice();
    auto command_list = device_resources->GetCommandList();
    auto command_allocator = device_resources->GetCommandAllocator();

    //TODO change this to default heap
    ComPtr<ID3D12Resource> vertex_resource;
    nv_helpers_dx12::CreateBuffer(device, vertices,
                                  sizeof(VertexType) * vertices.size(),
                                  vertex_resource);

    ComPtr<ID3D12Resource> index_resource;
    nv_helpers_dx12::CreateBuffer(device, indices,
                                  sizeof(UINT) * indices.size(),
                                  vertex_resource);

    //TODO remove, hardcoded
    m_vertexBuffer = vertex_resource;

    std::unique_ptr<RaytracingAcclerationStructures::BottomLevelAsPreInfo> bottom_level_as_pre_info = raytracing_accleration_structure.CreateBottomLevelAS({{vertex_resource, sizeof(VertexType), index_resource, indices.size()}});
    bottom_level_preinfos.emplace_back(std::move(bottom_level_as_pre_info));
  }

  void CreateTopLevelPreInfo(UINT num_instances)
  {
    std::unique_ptr<RaytracingAcclerationStructures::TopLevelAsPreInfo> top_level_as_pre_info = raytracing_accleration_structure.ComputePreInfoTopLevelAS(num_instances);
    bottom_level_preinfos.emplace_back(std::move(top_level_as_pre_info));
  }

  AccelerationStructureBuffers AllocateBottomAS(std::unique_ptr<RaytracingAcclerationStructures::BottomLevelAsPreInfo> bottom_level_as_pre_info, ComPtr<ID3D12DescriptorHeap> descriptor_heap)
  {
    AccelerationStructureBuffers allocate_bottom_level_as = raytracing_accleration_structure.AllocateBottomLevelAS(std::move(bottom_level_as_pre_info), descriptor_heap);
    return allocate_bottom_level_as;
  }

  AccelerationStructureBuffers AllocateTopAS(std::unique_ptr<RaytracingAcclerationStructures::TopLevelAsPreInfo> top_level_as_pre_info, std::vector<nv_helpers_dx12::Instance> instances)
  {
    AccelerationStructureBuffers allocate_top_level_as = raytracing_accleration_structure.AllocateTopLevelAS(std::move(top_level_as_pre_info), instances);
    return allocate_top_level_as;
  }

  template<typename VertexType>
  void CreateAcclerationStrutures(ComPtr<ID3D12DescriptorHeap> descriptor_heap, ComPtr<ID3D12Resource> vertex_resource, ComPtr<ID3D12Resource> indices_resource = nullptr, UINT indices_count = 0)
  {
    auto command_list = device_resources->GetCommandList();
    auto command_allocator = device_resources->GetCommandAllocator();

    command_list->Reset(command_allocator, nullptr);

    // Build the bottom AS from the Triangle vertex buffer
    CreatePreInfoBottomASVertices(std::vector(vertices), std::vector(indices));
    CreateTopLevelPreInfo(1);

    CreateRayGenSignature()


    // Just one instance for now
    auto m_instances = {{bottomLevelBuffers, XMMatrixIdentity()}};
    raytracing_accleration_structure.CreateTopLevelAS(descriptor_heap, m_instances);

    // Kick off acceleration structure construction.
    device_resources->ExecuteCommandList();

    // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
    device_resources->WaitForGpu();

    command_allocator->Reset();
    command_list->Reset(command_allocator, nullptr);

    //TODO remove
    m_bottomLevelAS = bottomLevelBuffers.accelerationStructure;
  }



  ComPtr<ID3D12RootSignature> CreateRayGenSignature()
  {
    ray_gen_root_allocator.Init(device_resources, 
                  fallback_device, fallback_command_list, fallback_state_object,
                  dxr_device, dxr_command_list, dxr_state_object,
                  ray_tracing_api);

    ray_gen_descriptor_table = ray_gen_root_allocator.AllocateRootDescriptorTable();
    uav0 = std::make_shared<RootResourceUnorderedAccessView>(
      [this](std::shared_ptr<DX::DeviceResources> device_resources, IRootResource* root_resource)
    {
      ComPtr<ID3D12Resource> raytracing_output = nullptr;

      RootResourceUnorderedAccessView *root_resource_uav = static_cast<RootResourceUnorderedAccessView *>(root_resource);

      auto device = device_resources->GetD3DDevice();
      auto backbufferFormat = device_resources->GetBackBufferFormat();

      // Create the output resource. The dimensions and format should match the swap-chain.
      auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(backbufferFormat, GetDXSample()->GetWidth(), GetDXSample()->GetHeight(), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

      auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
      ThrowIfFailed(device->CreateCommittedResource(
          &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&raytracing_output)));
      NAME_D3D12_OBJECT(raytracing_output);

      D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
      uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
      root_resource_uav->desc = uav_desc;

      return raytracing_output;
    });

    uav1 = std::make_shared<RootResourceUnorderedAccessView>(
    [this](std::shared_ptr<DX::DeviceResources> device_resources, IRootResource* root_resource)
    {
      ComPtr<ID3D12Resource> raytracing_output = nullptr;

      RootResourceUnorderedAccessView *root_resource_uav = static_cast<RootResourceUnorderedAccessView *>(root_resource);

      auto device = device_resources->GetD3DDevice();
      auto backbufferFormat = device_resources->GetBackBufferFormat();

      // Create the output resource. The dimensions and format should match the swap-chain.
      D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
      uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
      uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
      uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;

      //TODO
      uavDesc.Buffer.NumElements = bufferNumElements;

      auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
      ThrowIfFailed(device->CreateCommittedResource(
          &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&raytracing_output)));
      NAME_D3D12_OBJECT(raytracing_output);

      D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
      uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
      root_resource_uav->desc = uav_desc;

      return raytracing_output;
    });

    std::shared_ptr<RootDescriptor> uav_descriptor = ray_gen_descriptor_table->AllocateDescriptor(0);
    uav_descriptor->SetResource(uav0);

    /*
    vertices_resource_view = std::make_shared<RootResourceShaderResourceView>(
      [this](std::shared_ptr<DX::DeviceResources> device_resources, IRootResource* root_resource)
    {
      // Vertex buffer is passed to the shader along with index buffer as a descriptor table.
      // Vertex buffer descriptor must follow index buffer descriptor in the descriptor heap.
      ComPtr<ID3D12Resource> index_resource = nullptr;
      RootResourceShaderResourceView *root_resource_srv = static_cast<RootResourceShaderResourceView *>(root_resource);
      auto device = device_resources->GetD3DDevice();

      //TODO change this to use resource helper
      AllocateUploadBuffer(device, indices, sizeof(indices), &index_resource);
      NAME_D3D12_OBJECT(index_resource);
      UINT num_elements = sizeof(indices)/4;
      UINT element_size = 0;

      D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
      srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
      srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      srv_desc.Buffer.NumElements = num_elements;
      
      if (element_size == 0)
      {
          srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
          srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
          srv_desc.Buffer.StructureByteStride = 0;
      }
      else
      {
          srv_desc.Format = DXGI_FORMAT_UNKNOWN;
          srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
          srv_desc.Buffer.StructureByteStride = element_size;
      }

      root_resource_srv->desc = srv_desc;

      return index_resource;
    });
    
    indices_vertices_view = std::make_shared<RootResourceShaderResourceView>(
      [this](std::shared_ptr<DX::DeviceResources> device_resources, IRootResource* root_resource)
    {
      // Vertex buffer is passed to the shader along with index buffer as a descriptor table.
      // Vertex buffer descriptor must follow index buffer descriptor in the descriptor heap.
      ComPtr<ID3D12Resource> vertex_resource = nullptr;
      RootResourceShaderResourceView *root_resource_srv = static_cast<RootResourceShaderResourceView *>(root_resource);
      auto device = device_resources->GetD3DDevice();

      //TODO change this to use resource helper
      AllocateUploadBuffer(device, vertices, sizeof(vertices), &vertex_resource);
      NAME_D3D12_OBJECT(vertex_resource);
      UINT num_elements = ARRAYSIZE(vertices);
      UINT element_size = sizeof(vertices[0]);

      D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
      srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
      srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      srv_desc.Buffer.NumElements = num_elements;
      
      if (element_size == 0)
      {
          srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
          srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
          srv_desc.Buffer.StructureByteStride = 0;
      }
      else
      {
          srv_desc.Format = DXGI_FORMAT_UNKNOWN;
          srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
          srv_desc.Buffer.StructureByteStride = element_size;
      }

      root_resource_srv->desc = srv_desc;

      return vertex_resource;
    });
    
    std::shared_ptr<RootDescriptor> srv1_descriptor = ray_gen_descriptor_table->AllocateDescriptor(1);
    srv1_descriptor->SetResource(vertices_resource_view);
    
    std::shared_ptr<RootDescriptor> srv2_descriptor = ray_gen_descriptor_table->AllocateDescriptor(2);
    srv2_descriptor->SetResource(indices_vertices_view);
    */
    return ray_gen_root_allocator.Serialize();
  }

  ComPtr<ID3D12RootSignature> CreateRayMissSignature()
  {
    ray_miss_root_allocator.Init(device_resources, 
                  fallback_device, fallback_command_list, fallback_state_object,
                  dxr_device, dxr_command_list, dxr_state_object,
                  ray_tracing_api);

    return ray_miss_root_allocator.Serialize();
  }

  ComPtr<ID3D12RootSignature> CreateRayHitSignature()
  {
    ray_hit_root_allocator.Init(device_resources, 
                  fallback_device, fallback_command_list, fallback_state_object,
                  dxr_device, dxr_command_list, dxr_state_object,
                  ray_tracing_api);

    return ray_hit_root_allocator.Serialize();
  }

  void CreateRaytracingPipeline()
  {
    auto device = device_resources->GetD3DDevice();
    auto command_list = device_resources->GetCommandList();

    nv_helpers_dx12::RayTracingPipelineGenerator pipeline;
    pipeline.Init(device_resources, 
                  fallback_device, fallback_command_list, fallback_state_object,
                  dxr_device, dxr_command_list, dxr_state_object,
                  ray_tracing_api);

    // The pipeline contains the DXIL code of all the shaders potentially executed during the
    // raytracing process. This section compiles the HLSL code into a set of DXIL libraries. We chose
    // to separate the code in several libraries by semantic (ray generation, hit, miss) for clarity.
    // Any code layout can be used.
    ComPtr<IDxcBlob> ray_gen_library = nv_helpers_dx12::CompileShaderLibrary(L"RayGen.hlsl");
    ComPtr<IDxcBlob> miss_library = ray_gen_library;//nv_helpers_dx12::CompileShaderLibrary(L"Miss.hlsl");
    ComPtr<IDxcBlob> hit_library = ray_gen_library;//nv_helpers_dx12::CompileShaderLibrary(L"Hit.hlsl");

    // In a way similar to DLLs, each library is associated with a number of exported symbols. This
    // has to be done explicitly in the lines below. Note that a single library can contain an
    // arbitrary number of symbols, whose semantic is given in HLSL using the [shader("xxx")] syntax
    pipeline.AddLibrary(ray_gen_library.Get(), {L"MyRaygenShader"});
    pipeline.AddLibrary(miss_library.Get(), {L"MyClosestHitShader"});
    pipeline.AddLibrary(hit_library.Get(), {L"MyMissShader"});

    // To be used, each DX12 shader needs a root signature defining which parameters and buffers will
    // be accessed. Note that it is possible to define a root signature that contains buffers not
    // actually needed by the shader. For code simplicity this is what we do by reusing the
    // CreateHitSignature call for both regular hit shader and shadows
    ComPtr<ID3D12RootSignature> ray_gen_signature = CreateRayGenSignature();
    ComPtr<ID3D12RootSignature> ray_miss_signature  = CreateRayMissSignature();
    ComPtr<ID3D12RootSignature> ray_hit_signature  = CreateRayHitSignature();

    // 3 different shaders can be invoked to obtain an intersection: an intersection shader is called
    // when hitting the bounding box of non-triangular geometry. This is beyond the scope of this
    // tutorial. An any-hit shader is called on potential intersections. This shader can, for example,
    // perform alpha-testing and discard some intersections. Finally, the closest-hit program is
    // invoked on the intersection point closest to the ray origin. Those 3 shaders are bound together
    // into a hit group.

    // Note that for triangular geometry the intersection shader is built-in. An empty any-hit shader
    // is also defined by default, so in our simple case each hit group contains only the closest hit
    // shader. Note that since the exported symbols are defined above the shaders can be simply
    // referred to by name.

    // Hit group for the triangles, with a shader simply interpolating vertex colors
    pipeline.AddHitGroup(L"MyHitGroup", L"MyClosestHitShader");

    // The following section associates the root signature to each shader. Note that we can explicitly
    // show that some shaders share the same root signature (eg. Miss and ShadowMiss). Note that the
    // hit shaders are now only referred to as hit groups, meaning that the underlying intersection,
    // any-hit and closest-hit shaders share the same root signature.
    pipeline.AddRootSignatureAssociation(ray_gen_signature.Get(), {L"MyRaygenShader"});
    pipeline.AddRootSignatureAssociation(ray_miss_signature.Get(), {L"MyMissShader"});
    pipeline.AddRootSignatureAssociation(ray_hit_signature.Get(), {L"MyClosestHitShader"});

    // The payload size defines the maximum size of the data carried by the rays, ie. the the data
    // exchanged between shaders, such as the HitInfo structure in the HLSL code. It is important to
    // keep this value as low as possible as a too high value would result in unnecessary memory
    // consumption and cache trashing.
    pipeline.SetMaxPayloadSize(4 * sizeof(float)); // RGB + distance

    // Upon hitting a surface, DXR can provide several attributes to the hit. In our sample we just
    // use the barycentric coordinates defined by the weights u,v of the last two vertices of the
    // triangle. The actual barycentrics can be obtained using float3 barycentrics = float3(1.f-u-v,
    // u, v);
    pipeline.SetMaxAttributeSize(2 * sizeof(float)); // barycentric coordinates

    // The raytracing process can shoot rays from existing hit points, resulting in nested TraceRay
    // calls. Our sample code traces only primary rays, which then requires a trace depth of 1. Note
    // that this recursion depth should be kept to a minimum for best performance. Path tracing
    // algorithms can be easily flattened into a simple loop in the ray generation.
    pipeline.SetMaxRecursionDepth(1);

    // Compile the pipeline for execution on the GPU
    pipeline.Generate();

    // Cast the state object into a properties object, allowing to later access the shader pointers by
    // name
    if (ray_tracing_api->IsFallback())
    {
      ThrowIfFailed(fallback_state_object->QueryInterface(IID_PPV_ARGS(&state_object_properties)));
    } 
    else 
    {
      ThrowIfFailed(dxr_state_object->QueryInterface(IID_PPV_ARGS(&state_object_properties)));
    }
  }

  //--------------------------------------------------------------------------------------------------
  //
  // The Shader Binding Table (SBT) is the cornerstone of the raytracing setup: this is where the
  // shader resources are bound to the shaders, in a way that can be interpreted by the raytracer on
  // GPU. In terms of layout, the SBT contains a series of shader IDs with their resource pointers.
  // The SBT contains the ray generation shader, the miss shaders, then the hit groups. Using the
  // helper class, those can be specified in arbitrary order.
  //
  void CreateShaderBindingTable()
  {
    auto device = device_resources->GetD3DDevice();

    // The SBT helper class collects calls to Add*Program.  If called several times, the helper must
    // be emptied before re-adding shaders.
    shader_binding_table_generator.Reset();

    // The pointer to the beginning of the heap is the only parameter required by shaders without root
    // parameters
    D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle = ray_gen_descriptor_table->descriptor_heap->GetGPUDescriptorHandleForHeapStart();

    // The helper treats both root parameter pointers and heap pointers as void*, while DX12 uses the
    // D3D12_GPU_DESCRIPTOR_HANDLE to define heap pointers. The pointer in this struct is a UINT64,
    // which then has to be reinterpreted as a pointer.
    auto heapPointer = reinterpret_cast<UINT64*>(srvUavHeapHandle.ptr);

    // The ray generation only uses heap data
    shader_binding_table_generator.AddRayGenerationProgram(L"RayGen", {heapPointer});

    // The miss shaders do not access any external resources: instead they communicate their results
    // through the ray payload
    shader_binding_table_generator.AddMissProgram(L"Miss", {});

    // Adding the triangle hit shader
    shader_binding_table_generator.AddHitGroup(L"HitGroup", {(void*)(m_vertexBuffer->GetGPUVirtualAddress())});

    // Compute the size of the SBT given the number of shaders and their parameters
    uint32_t sbtSize = shader_binding_table_generator.ComputeSBTSize();

    // Create the SBT on the upload heap. This is required as the helper will use mapping to write the
    // SBT contents. After the SBT compilation it could be copied to the default heap for performance.
    shader_binder_table_storage = nv_helpers_dx12::CreateBuffer(device, sbtSize, D3D12_RESOURCE_FLAG_NONE,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ,
                                                 nv_helpers_dx12::kUploadHeapProps);
    if (!shader_binder_table_storage)
    {
      throw std::logic_error("Could not allocate the shader binding table");
    }

    // Compile the SBT from the shader and parameters info
    shader_binding_table_generator.Generate(shader_binder_table_storage.Get());
  }

  void Render()
  {
    device_resources->Prepare();
    Raytrace();
    CopyRaytracedResultToBackBuffer();
    device_resources->Present(D3D12_RESOURCE_STATE_PRESENT);
  }

  void Raytrace()
  {
    auto device = device_resources->GetD3DDevice();
    auto command_allocator = device_resources->GetCommandAllocator();
    auto command_list = device_resources->GetCommandList();
    auto frame_index = device_resources->GetCurrentFrameIndex();

    auto DispatchRays = [this](auto* commandList, auto* stateObject, auto* dispatchDesc)
    {
      // Since each shader table has only one shader record, the stride is same as the size.

      // The layout of the SBT is as follows: ray generation shaders, miss shaders, hit groups. As
      // described in the CreateShaderBindingTable method, all SBT entries have the same size to allow
      // a fixed stride.

      // The ray generation shaders are always at the beginning of the SBT. In this example we have
      // only one RG, so the size of this SBT sections is m_sbtEntrySize.
      uint32_t ray_generation_section_size_in_bytes = shader_binding_table_generator.GetRayGenSectionSize();
      dispatchDesc->RayGenerationShaderRecord.StartAddress = shader_binder_table_storage->GetGPUVirtualAddress();
      dispatchDesc->RayGenerationShaderRecord.SizeInBytes = ray_generation_section_size_in_bytes;

      // The miss shaders are in the second SBT section, right after the ray generation shader. We
      // have one miss shader for the camera rays and one for the shadow rays, so this section has a
      // size of 2*m_sbtEntrySize. We also indicate the stride between the two miss shaders, which is
      // the size of a SBT entry
      uint32_t miss_section_size_in_bytes = shader_binding_table_generator.GetMissSectionSize();
      dispatchDesc->MissShaderTable.StartAddress = shader_binder_table_storage->GetGPUVirtualAddress() + miss_section_size_in_bytes;
      dispatchDesc->MissShaderTable.SizeInBytes = miss_section_size_in_bytes;
      dispatchDesc->MissShaderTable.StrideInBytes = shader_binding_table_generator.GetMissEntrySize();

      // The hit groups section start after the miss shaders. In this sample we have 4 hit groups: 2
      // for the triangles (1 used when hitting the geometry from a camera ray, 1 when hitting the
      // same geometry from a shadow ray) and 2 for the plane. We also indicate the stride between the
      // two miss shaders, which is the size of a SBT entry #Pascal: experiment with different sizes
      // for the SBT entries
      uint32_t hit_groups_section_size = shader_binding_table_generator.GetHitGroupSectionSize();
      dispatchDesc->HitGroupTable.StartAddress = shader_binder_table_storage->GetGPUVirtualAddress() + ray_generation_section_size_in_bytes + miss_section_size_in_bytes;
      dispatchDesc->HitGroupTable.SizeInBytes = hit_groups_section_size;
      dispatchDesc->HitGroupTable.StrideInBytes = shader_binding_table_generator.GetHitGroupEntrySize();

      // Dimensions of the image to render, identical to a kernel launch dimension
      dispatchDesc->Width = sample->GetWidth();
      dispatchDesc->Height = sample->GetHeight();
      dispatchDesc->Depth = 1;
      commandList->SetPipelineState1(stateObject);
      commandList->DispatchRays(dispatchDesc);
    };

    auto SetCommonPipelineState = [&, this](auto* descriptorSetCommandList)
    {
        // Bind the descriptor heap giving access to the top-level acceleration structure, as well as
        // the raytracing output
        descriptorSetCommandList->SetDescriptorHeaps(1, this->ray_gen_descriptor_table->descriptor_heap.GetAddressOf());
        // Set index and successive vertex buffer decriptor tables
        //commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::VertexBuffersSlot, m_indexBuffer.gpuDescriptorHandle);
        //commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputViewSlot, m_raytracingOutputResourceUAVGpuDescriptor);
    };

    command_list->SetComputeRootSignature(ray_gen_root_allocator.d3d12_root_signature.Get());

    // Copy the updated scene constant buffer to GPU.
    //memcpy(&m_mappedConstantData[frameIndex].constants, &m_sceneCB[frameIndex], sizeof(m_sceneCB[frameIndex]));
    //auto cbGpuAddress = m_perFrameConstants->GetGPUVirtualAddress() + frameIndex * sizeof(m_mappedConstantData[0]);
    //commandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams::SceneConstantSlot, cbGpuAddress);
   
    // Bind the heaps, acceleration structure and dispatch rays.
    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
    if (ray_tracing_api->IsFallback())
    {
        SetCommonPipelineState(fallback_command_list.Get());
        fallback_command_list->SetTopLevelAccelerationStructure(1, raytracing_accleration_structure.top_level_gpu_pointer);
        DispatchRays(fallback_command_list.Get(), fallback_state_object.Get(), &dispatchDesc);
    }
    else // DirectX Raytracing
    {
        SetCommonPipelineState(command_list);
        command_list->SetComputeRootShaderResourceView(1, raytracing_accleration_structure.top_level_acceleration_structure->GetGPUVirtualAddress());
        DispatchRays(dxr_command_list.Get(), dxr_state_object.Get(), &dispatchDesc);
    }
  }

  void CopyRaytracedResultToBackBuffer()
  {
    auto commandList = device_resources->GetCommandList();
    auto renderTarget = device_resources->GetRenderTarget();

    D3D12_RESOURCE_BARRIER preCopyBarriers[2];
    preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
    preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(uav0->resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    commandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

    commandList->CopyResource(renderTarget, uav0->resource.Get());

    D3D12_RESOURCE_BARRIER postCopyBarriers[2];
    postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
    postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(uav0->resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    commandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
  }

  std::shared_ptr<RaytracingApi> GetRayTracingApi()
  {
    return ray_tracing_api;
  }

  DXSample *GetDXSample()
  { 
    return sample;
  }

  std::shared_ptr<DX::DeviceResources> GetDeviceResoures() const
  {
    return device_resources;
  }

  ComPtr<ID3D12RaytracingFallbackDevice> GetFallbackDevice() const
  {
    return fallback_device;
  }

  ComPtr<ID3D12Resource> GetRaytracedUAVOutputResource()
  {
    return uav0->resource;
  }

  void Release()
  {
    //TODO
  }

private:
  //TODO temporary
  ComPtr<ID3D12Resource> m_vertexBuffer;
  ComPtr<ID3D12Resource> m_bottomLevelAS;

  std::shared_ptr<RootDescriptorTable> ray_gen_descriptor_table;

  std::shared_ptr<RootResourceUnorderedAccessView> uav0;
  //TODO
  //struct Multiple resources for each bottom level
  std::shared_ptr<RootResourceUnorderedAccessView> uav1;
  std::shared_ptr<RootResourceUnorderedAccessView> uav2;
  std::shared_ptr<RootResourceShaderResourceView> vertices_resource_view;
  std::shared_ptr<RootResourceShaderResourceView> indices_vertices_view;
  ////

  RootAllocator ray_gen_root_allocator;
  RootAllocator ray_miss_root_allocator;
  RootAllocator ray_hit_root_allocator;

  DXSample *sample;

  std::shared_ptr<DX::DeviceResources> device_resources;
  ComPtr<ID3D12RaytracingFallbackDevice> fallback_device;
  ComPtr<ID3D12RaytracingFallbackCommandList> fallback_command_list;
  ComPtr<ID3D12RaytracingFallbackStateObject> fallback_state_object;
  ComPtr<ID3D12Device5> dxr_device;
  ComPtr<ID3D12GraphicsCommandList5> dxr_command_list;
  ComPtr<ID3D12StateObject> dxr_state_object;
  std::shared_ptr<RaytracingApi> ray_tracing_api;

  ComPtr<ID3D12StateObjectProperties> state_object_properties;

  UINT frame_count = 3;

  //SceneConstantBuffer m_sceneCB[frame_count];
  SceneConstantBuffer m_sceneCB[3];
  XMVECTOR m_eye;
  XMVECTOR m_at;
  XMVECTOR m_up;

  RaytracingAcclerationStructures raytracing_accleration_structure;
  nv_helpers_dx12::ShaderBindingTableGenerator shader_binding_table_generator;
  ComPtr<ID3D12Resource> shader_binder_table_storage;

  std::vector<std::unique_ptr<RaytracingAcclerationStructures::BottomLevelAsPreInfo>> bottom_level_preinfos;
  std::vector<std::unique_ptr<RaytracingAcclerationStructures::TopLevelAsPreInfo>> top_level_preinfos;
};
