#pragma once
class RaytracingManager {
public:
  RaytracingManager();
  ~RaytracingManager();

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
void RaytracingManager::UpdateCameraMatrices()
{
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
        
        UpdateCameraMatrices();
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

  void CreateRootSignatures()
  {
    root_signature_desc.AllocateRootDescriptorTable();

  }

  std::shared_ptr<RaytracingApi> GetRayTracingApi()
  {
    return ray_tracing_api;
  }

private:
  DXSample *sample;

  std::shared_ptr<DX::DeviceResources> device_resources;
  ComPtr<ID3D12RaytracingFallbackDevice> fallback_device;
  ComPtr<ID3D12RaytracingFallbackCommandList> fallback_command_list;
  ComPtr<ID3D12Device5> dxr_device;
  ComPtr<ID3D12GraphicsCommandList5> dxr_command_list;
  std::shared_ptr<RaytracingApi> ray_tracing_api;

  UINT frame_count = 3;

  //SceneConstantBuffer m_sceneCB[frame_count];
  SceneConstantBuffer m_sceneCB[3];
  XMVECTOR m_eye;
  XMVECTOR m_at;
  XMVECTOR m_up;

  RaytracingAcclerationStructure raytracing_accleration_structure;
  RootAllocator root_allocator;
};
