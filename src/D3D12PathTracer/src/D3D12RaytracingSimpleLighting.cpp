//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "model_loading/tiny_obj_loader.h"
#include "D3D12RaytracingSimpleLighting.h"
#include "DirectXRaytracingHelper.h"
#include "CompiledShaders\Raytracing.hlsl.h"
#include "TextureLoader.h"
#include "Mesh.h"
#include "MeshLoader.h"
#include <iostream>
#include <algorithm>
#include "stb_image_write.h"

using namespace std;
using namespace DX;

const wchar_t* D3D12RaytracingSimpleLighting::c_hitGroupName = L"MyHitGroup";
const wchar_t* D3D12RaytracingSimpleLighting::c_raygenShaderName = L"MyRaygenShader";
const wchar_t* D3D12RaytracingSimpleLighting::c_closestHitShaderName = L"MyClosestHitShader";
const wchar_t* D3D12RaytracingSimpleLighting::c_missShaderName = L"MyMissShader";
const float D3D12RaytracingSimpleLighting::c_rotateDegrees = 5.f;
const float D3D12RaytracingSimpleLighting::c_movementAmountFactor = 0.1f;
const unsigned int D3D12RaytracingSimpleLighting::c_maxIteration = 6000;

D3D12RaytracingSimpleLighting::D3D12RaytracingSimpleLighting(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_raytracingOutputResourceUAVDescriptorHeapIndex(UINT_MAX),
    m_isDxrSupported(false)
{
    m_forceComputeFallback = false;
    SelectRaytracingAPI(RaytracingAPI::FallbackLayer);
    UpdateForSizeChange(width, height);
}

void D3D12RaytracingSimpleLighting::EnableDirectXRaytracing(IDXGIAdapter1* adapter)
{
    // Fallback Layer uses an experimental feature and needs to be enabled before creating a D3D12 device.
    bool isFallbackSupported = EnableComputeRaytracingFallback(adapter);

    if (!isFallbackSupported)
    {
        OutputDebugString(
            L"Warning: Could not enable Compute Raytracing Fallback (D3D12EnableExperimentalFeatures() failed).\n" \
            L"         Possible reasons: your OS is not in developer mode.\n\n");
    }

    m_isDxrSupported = IsDirectXRaytracingSupported(adapter);

    if (!m_isDxrSupported)
    {
        OutputDebugString(L"Warning: DirectX Raytracing is not supported by your GPU and driver.\n\n");

        ThrowIfFalse(isFallbackSupported, 
            L"Could not enable compute based fallback raytracing support (D3D12EnableExperimentalFeatures() failed).\n"\
            L"Possible reasons: your OS is not in developer mode.\n\n");
        m_raytracingAPI = RaytracingAPI::FallbackLayer;
    }
}

void D3D12RaytracingSimpleLighting::OnInit()
{
    m_deviceResources = std::make_unique<DeviceResources>(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_UNKNOWN,
        FrameCount,
        D3D_FEATURE_LEVEL_11_0,
        // Sample shows handling of use cases with tearing support, which is OS dependent and has been supported since TH2.
        // Since the Fallback Layer requires Fall Creator's update (RS3), we don't need to handle non-tearing cases.
        DeviceResources::c_RequireTearingSupport,
        m_adapterIDoverride
        );
    m_deviceResources->RegisterDeviceNotify(this);
    m_deviceResources->SetWindow(Win32Application::GetHwnd(), m_width, m_height);
    m_deviceResources->InitializeDXGIAdapter();
    EnableDirectXRaytracing(m_deviceResources->GetAdapter());

    m_deviceResources->CreateDeviceResources();
    m_deviceResources->CreateWindowSizeDependentResources();

    InitializeScene();

    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

// Update camera matrices passed into the shader.
void D3D12RaytracingSimpleLighting::UpdateCameraMatrices()
{
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

    m_sceneCB[frameIndex].cameraPosition = m_eye;
    float fovAngleY = 45.0f;

    XMMATRIX view = XMMatrixLookAtLH(m_eye, m_at, m_up);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), m_aspectRatio, 1.0f, 125.0f);
    XMMATRIX viewProj = view * proj;

    m_sceneCB[frameIndex].projectionToWorld = XMMatrixInverse(nullptr, viewProj);
}

// Initialize scene rendering parameters.
void D3D12RaytracingSimpleLighting::InitializeScene()
{
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

    // Setup materials.
    {
        m_cubeCB.albedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Setup camera.
    {
        // Initialize the view and projection inverse matrices.
        m_eye = { 0.0f, 2.0f, -20.0f, 1.0f };
        m_at = { 0.0f, 0.0f, 1.0f, 1.0f };

        m_right = { 1.0f, 0.0f, 0.0f, 1.0f };
		m_up = { 0.0f, 1.0f, 0.0f, 1.0f };
		m_forward = { 0.0f, 0.0f, 1.0f, 1.0f };

		m_camChanged = true;
        
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

        lightDiffuseColor = XMFLOAT4(1.0, 0.5f, 0.5f, 1.0f);
        m_sceneCB[frameIndex].lightDiffuseColor = XMLoadFloat4(&lightDiffuseColor);
        m_sceneCB[frameIndex].lightDiffuseColor = XMLoadFloat4(&lightDiffuseColor);
    }

    // Setup path tracing state
    {
	    m_sceneCB[frameIndex].iteration = 1;
	    m_sceneCB[frameIndex].depth = 5;
	    m_sceneCB[frameIndex].features = AntiAliasing;
    }

    // Apply the initial values to all frames' buffer instances.
    for (auto& sceneCB : m_sceneCB)
    {
        sceneCB = m_sceneCB[frameIndex];
    }
}

// Create constant buffers.
void D3D12RaytracingSimpleLighting::CreateConstantBuffers()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto frameCount = m_deviceResources->GetBackBufferCount();
    
    // Create the constant buffer memory and map the CPU and GPU addresses
    const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // Allocate one constant buffer per frame, since it gets updated every frame.
    size_t cbSize = frameCount * sizeof(AlignedSceneConstantBuffer);
    const D3D12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &constantBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_perFrameConstants)));

    // Map the constant buffer and cache its heap pointers.
    // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_perFrameConstants->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedConstantData)));
}

// Create resources that depend on the device.
void D3D12RaytracingSimpleLighting::CreateDeviceDependentResources()
{
    // Initialize raytracing pipeline.

    // Create raytracing interfaces: raytracing device and commandlist.
    CreateRaytracingInterfaces();

    m_sceneLoaded = new Scene(p_sceneFileName, this); // this will load everything in the argument text file

    // Create root signatures for the shaders.
    CreateRootSignatures();

    // Create a raytracing pipeline state object which defines the binding of shaders, state and resources to be used during raytracing.
    CreateRaytracingPipelineStateObject();

    // Create a heap for descriptors.
    CreateDescriptorHeap();

    // Build geometry to be used in the sample.
    m_sceneLoaded->AllocateResourcesInDescriptorHeap();

    // Build raytracing acceleration structures from the generated geometry.
    BuildAccelerationStructures();

    // Create constant buffers for the geometry and the scene.
    CreateConstantBuffers();

    // Build shader tables, which define shaders and their local root arguments.
    BuildShaderTables();

    // Create an output 2D texture to store the raytracing result to.
    CreateRaytracingOutputResource();

    InitImGUI();
}

void D3D12RaytracingSimpleLighting::SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig)
{
    auto device = m_deviceResources->GetD3DDevice();
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    if (m_raytracingAPI == RaytracingAPI::FallbackLayer)
    {
        ThrowIfFailed(m_fallbackDevice->D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
        ThrowIfFailed(m_fallbackDevice->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
    }
    else // DirectX Raytracing
    {
        ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
        ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
    }
}

void D3D12RaytracingSimpleLighting::CreateRootSignatures()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Global Root Signature
    // This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
    {
        auto num_models = m_sceneLoaded->modelMap.size();
        auto num_objects = m_sceneLoaded->objects.size();
        auto num_diffuse_textures = m_sceneLoaded->diffuseTextureMap.size();
        auto num_normal_textures = m_sceneLoaded->normalTextureMap.size();
        auto num_materials = m_sceneLoaded->materialMap.size();

        //ensure that the models, textures and materials are not zero or else bad things will happen :)
        assert(num_models != 0);
        assert(num_objects != 0);
        assert(num_diffuse_textures != 0);
        assert(num_normal_textures != 0);
        assert(num_materials != 0);

        CD3DX12_DESCRIPTOR_RANGE ranges[7]; // Perfomance TIP: Order from most frequent to least frequent.
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0);  // 1 output texture
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, num_models, 0, 1);  // array of vertices
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, num_models, 0, 2);  // array of indices
        ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, num_objects, 0, 3);  // array of infos for each object
	ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, num_materials, 0, 4);  // array of materials
	ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, num_diffuse_textures, 0, 5);  // array of textures
	ranges[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, num_normal_textures, 0, 6);  // array of normal textures

        CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignatureParams::Count];
        rootParameters[GlobalRootSignatureParams::AccelerationStructureSlot].InitAsShaderResourceView(0);
        rootParameters[GlobalRootSignatureParams::SceneConstantSlot].InitAsConstantBufferView(0);
        rootParameters[GlobalRootSignatureParams::OutputViewSlot].InitAsDescriptorTable(1, &ranges[0]);
        rootParameters[GlobalRootSignatureParams::VertexBuffersSlot].InitAsDescriptorTable(1, &ranges[1]);
        rootParameters[GlobalRootSignatureParams::IndexBuffersSlot].InitAsDescriptorTable(1, &ranges[2]);
        rootParameters[GlobalRootSignatureParams::InfoBuffersSlot].InitAsDescriptorTable(1, &ranges[3]);
	rootParameters[GlobalRootSignatureParams::MaterialBuffersSlot].InitAsDescriptorTable(1, &ranges[4]);
        rootParameters[GlobalRootSignatureParams::TextureSlot].InitAsDescriptorTable(1, &ranges[5]);
	rootParameters[GlobalRootSignatureParams::NormalTextureSlot].InitAsDescriptorTable(1, &ranges[6]);

	// LOOKAT
	// create a static sampler
	D3D12_STATIC_SAMPLER_DESC sampler[2] = {};
	sampler[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler[0].MipLODBias = 0;
	sampler[0].MaxAnisotropy = 0;
	sampler[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler[0].MinLOD = 0.0f;
	sampler[0].MaxLOD = 1.0f;
	sampler[0].ShaderRegister = 0;
	sampler[0].RegisterSpace = 0;
	sampler[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        sampler[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        sampler[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        sampler[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        sampler[1].MipLODBias = 0;
        sampler[1].MaxAnisotropy = 0;
        sampler[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler[1].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler[1].MinLOD = 0.0f;
        sampler[1].MaxLOD = 1.0f;
        sampler[1].ShaderRegister = 1;
        sampler[1].RegisterSpace = 0;
        sampler[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters, 2, &sampler[0]);
        SerializeAndCreateRaytracingRootSignature(globalRootSignatureDesc, &m_raytracingGlobalRootSignature);
    }

    // Local Root Signature
    // This is a root signature that enables a shader to have unique arguments that come from shader tables.
    {
        CD3DX12_ROOT_PARAMETER rootParameters[LocalRootSignatureParams::Count];
        rootParameters[LocalRootSignatureParams::CubeConstantSlot].InitAsConstants(SizeOfInUint32(m_cubeCB), 1);
        CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
        localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
        SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &m_raytracingLocalRootSignature);
    }
}

// Create raytracing device and command list.
void D3D12RaytracingSimpleLighting::CreateRaytracingInterfaces()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();

    if (m_raytracingAPI == RaytracingAPI::FallbackLayer)
    {
        CreateRaytracingFallbackDeviceFlags createDeviceFlags = m_forceComputeFallback ? 
                                                    CreateRaytracingFallbackDeviceFlags::ForceComputeFallback : 
                                                    CreateRaytracingFallbackDeviceFlags::None;
        ThrowIfFailed(D3D12CreateRaytracingFallbackDevice(device, createDeviceFlags, 0, IID_PPV_ARGS(&m_fallbackDevice)));
        m_fallbackDevice->QueryRaytracingCommandList(commandList, IID_PPV_ARGS(&m_fallbackCommandList));
    }
    else // DirectX Raytracing
    {
        ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&m_dxrDevice)), L"Couldn't get DirectX Raytracing interface for the device.\n");
        ThrowIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&m_dxrCommandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");
    }
}

// Local root signature and shader association
// This is a root signature that enables a shader to have unique arguments that come from shader tables.
void D3D12RaytracingSimpleLighting::CreateLocalRootSignatureSubobjects(CD3D12_STATE_OBJECT_DESC* raytracingPipeline)
{
    // Ray gen and miss shaders in this sample are not using a local root signature and thus one is not associated with them.

    // Local root signature to be used in a hit group.
    auto localRootSignature = raytracingPipeline->CreateSubobject<CD3D12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    localRootSignature->SetRootSignature(m_raytracingLocalRootSignature.Get());
    // Define explicit shader association for the local root signature. 
    {
        auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
        rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
        rootSignatureAssociation->AddExport(c_hitGroupName);
    }
}

// Create a raytracing pipeline state object (RTPSO).
// An RTPSO represents a full set of shaders reachable by a DispatchRays() call,
// with all configuration options resolved, such as local signatures and other state.
void D3D12RaytracingSimpleLighting::CreateRaytracingPipelineStateObject()
{
    // Create 7 subobjects that combine into a RTPSO:
    // Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
    // Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
    // This simple sample utilizes default shader association except for local root signature subobject
    // which has an explicit association specified purely for demonstration purposes.
    // 1 - DXIL library
    // 1 - Triangle hit group
    // 1 - Shader config
    // 2 - Local root signature and association
    // 1 - Global root signature
    // 1 - Pipeline config
    CD3D12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };


    // DXIL library
    // This contains the shaders and their entrypoints for the state object.
    // Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
    auto lib = raytracingPipeline.CreateSubobject<CD3D12_DXIL_LIBRARY_SUBOBJECT>();
    D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void *)g_pRaytracing, ARRAYSIZE(g_pRaytracing));
    lib->SetDXILLibrary(&libdxil);
    // Define which shader exports to surface from the library.
    // If no shader exports are defined for a DXIL library subobject, all shaders will be surfaced.
    // In this sample, this could be ommited for convenience since the sample uses all shaders in the library. 
    {
        lib->DefineExport(c_raygenShaderName);
        lib->DefineExport(c_closestHitShaderName);
        lib->DefineExport(c_missShaderName);
    }
    
    // Triangle hit group
    // A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
    // In this sample, we only use triangle geometry with a closest hit shader, so others are not set.
    auto hitGroup = raytracingPipeline.CreateSubobject<CD3D12_HIT_GROUP_SUBOBJECT>();
    hitGroup->SetClosestHitShaderImport(c_closestHitShaderName);
    hitGroup->SetHitGroupExport(c_hitGroupName);
    hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    
    // Shader config
    // Defines the maximum sizes in bytes for the ray payload and attribute structure.
    auto shaderConfig = raytracingPipeline.CreateSubobject<CD3D12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = sizeof(XMFLOAT4) + sizeof(XMFLOAT3) * 2;    // float4 pixelColor
    UINT attributeSize = sizeof(XMFLOAT2);  // float2 barycentrics
    shaderConfig->Config(payloadSize, attributeSize);

    // Local root signature and shader association
    // This is a root signature that enables a shader to have unique arguments that come from shader tables.
    CreateLocalRootSignatureSubobjects(&raytracingPipeline);

    // Global root signature
    // This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
    auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3D12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    globalRootSignature->SetRootSignature(m_raytracingGlobalRootSignature.Get());

    // Pipeline config
    // Defines the maximum TraceRay() recursion depth.
    auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3D12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    // PERFOMANCE TIP: Set max recursion depth as low as needed 
    // as drivers may apply optimization strategies for low recursion depths.
    UINT maxRecursionDepth = 1; // ~ primary rays only. // TODO: Possibly fix when trying to recurse during pathtracing
    pipelineConfig->Config(maxRecursionDepth);

#if _DEBUG
    PrintStateObjectDesc(raytracingPipeline);
#endif

    // Create the state object.
    if (m_raytracingAPI == RaytracingAPI::FallbackLayer)
    {
        ThrowIfFailed(m_fallbackDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_fallbackStateObject)), L"Couldn't create DirectX Raytracing state object.\n");
    }
    else // DirectX Raytracing
    {
        ThrowIfFailed(m_dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_dxrStateObject)), L"Couldn't create DirectX Raytracing state object.\n");
    }
}

// Create 2D output texture for raytracing.
void D3D12RaytracingSimpleLighting::CreateRaytracingOutputResource()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto backbufferFormat = m_deviceResources->GetBackBufferFormat();
    {
      // Create the output resource. The dimensions and format should match the swap-chain.
      auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(backbufferFormat, m_width, m_height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

      auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
      ThrowIfFailed(device->CreateCommittedResource(
          &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_raytracingOutput)));
      NAME_D3D12_OBJECT(m_raytracingOutput);

      D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
      m_raytracingOutputResourceUAVDescriptorHeapIndex = AllocateDescriptor(&uavDescriptorHandle, m_raytracingOutputResourceUAVDescriptorHeapIndex);
      D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
      UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
      device->CreateUnorderedAccessView(m_raytracingOutput.Get(), nullptr, &UAVDesc, uavDescriptorHandle);
	  m_raytracingOutputResourceUAVGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), m_raytracingOutputResourceUAVDescriptorHeapIndex, m_descriptorSize);
    }

    {
      // Create the output resource. The dimensions and format should match the swap-chain.
      auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, m_width, m_height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

      auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
      ThrowIfFailed(device->CreateCommittedResource(
          &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&pathtracing_accumulation_resource)));
      NAME_D3D12_OBJECT(pathtracing_accumulation_resource);

      D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
      AllocateDescriptor(&uavDescriptorHandle, m_raytracingOutputResourceUAVDescriptorHeapIndex + 1);
      D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
      UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
      device->CreateUnorderedAccessView(pathtracing_accumulation_resource.Get(), nullptr, &UAVDesc, uavDescriptorHandle);
    }
}

void D3D12RaytracingSimpleLighting::CreateDescriptorHeap()
{
    auto device = m_deviceResources->GetD3DDevice();

    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    // Allocate a heap for 5 descriptors:
    // 2 - vertex and index buffer SRVs
	// 1 - tex
	// 1 - norm tex
    // 1 - raytracing output texture SRV
    // 2 - bottom and top level acceleration structure fallback wrapped pointer UAVs
    descriptorHeapDesc.NumDescriptors = HEAP_DESCRIPTOR_SIZE; 
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    descriptorHeapDesc.NodeMask = 0;
    device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_descriptorHeap));
    NAME_D3D12_OBJECT(m_descriptorHeap);

    m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

// Build geometry used in the sample.
void D3D12RaytracingSimpleLighting::BuildGeometry()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Cube indices.
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

   // AllocateUploadBuffer(device, indices, sizeof(indices), &m_indexBufferCube.resource);
   // AllocateUploadBuffer(device, vertices, sizeof(vertices), &m_vertexBufferCube.resource);

    // Vertex buffer is passed to the shader along with index buffer as a descriptor table.
    // Vertex buffer descriptor must follow index buffer descriptor in the descriptor heap.

	//UINT descriptorIndexIB = CreateBufferSRV(&m_indexBufferCube, 36 * sizeof(Index) / 4, 0);
	//UINT descriptorIndexVB = CreateBufferSRV(&m_vertexBufferCube, 6 , sizeof(vertices[0]));
    //ThrowIfFalse(descriptorIndexVB == descriptorIndexIB + 1, L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index!");
    //ThrowIfFalse(descriptorIndexVB == descriptorIndexIB + 1, L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index!");
}

// Build acceleration structures needed for raytracing.
void D3D12RaytracingSimpleLighting::BuildAccelerationStructures()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();
    auto commandQueue = m_deviceResources->GetCommandQueue();
    auto commandAllocator = m_deviceResources->GetCommandAllocator();

    // Reset the command list for the acceleration structure construction.
    commandList->Reset(commandAllocator, nullptr);

    // What API is used
    bool is_fallback = m_raytracingAPI == RaytracingAPI::FallbackLayer;

    // Build bottom levels
    for (auto& model_pair : m_sceneLoaded->modelMap)
    {
       //int id = model_pair.first;
       ModelLoading::Model& model = model_pair.second;
       model.GetGeomDesc();
       model.GetBottomLevelBuildDesc();
       model.GetPreBuild(is_fallback, m_fallbackDevice, m_dxrDevice);
       model.GetBottomLevelScratchAS(is_fallback, device, m_fallbackDevice,m_dxrDevice);
       model.GetBottomAS(is_fallback, device, m_fallbackDevice,m_dxrDevice);

       //m_sceneLoaded->modelMap[id] = model;
    }

    // Build top level
    m_sceneLoaded->GetTopLevelDesc();
    m_sceneLoaded->GetTopLevelPrebuildInfo(is_fallback, m_fallbackDevice, m_dxrDevice);
    m_sceneLoaded->GetTopLevelScratchAS(is_fallback, device, m_fallbackDevice, m_dxrDevice);
    m_sceneLoaded->GetTopAS(is_fallback, device, m_fallbackDevice, m_dxrDevice);
    m_sceneLoaded->GetInstanceDescriptors(is_fallback, m_fallbackDevice, m_dxrDevice);

    // Finalize the allocation
    m_fallbackTopLevelAccelerationStructurePointer = m_sceneLoaded->GetWrappedGPUPointer(is_fallback, m_fallbackDevice, m_dxrDevice);
    m_sceneLoaded->FinalizeAS();

    // Actually build the AS (executing command lists)
    m_sceneLoaded->BuildAllAS(is_fallback, m_fallbackDevice, m_dxrDevice, m_fallbackCommandList, m_dxrCommandList);
}

// Build shader tables.
// This encapsulates all shader records - shaders and the arguments for their local root signatures.
void D3D12RaytracingSimpleLighting::BuildShaderTables()
{
    auto device = m_deviceResources->GetD3DDevice();

    void* rayGenShaderIdentifier;
    void* missShaderIdentifier;
    void* hitGroupShaderIdentifier;

    auto GetShaderIdentifiers = [&](auto* stateObjectProperties)
    {
        rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_raygenShaderName);
        missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_missShaderName);
        hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_hitGroupName);
    };

    // Get shader identifiers.
    UINT shaderIdentifierSize;
    if (m_raytracingAPI == RaytracingAPI::FallbackLayer)
    {
        GetShaderIdentifiers(m_fallbackStateObject.Get());
        shaderIdentifierSize = m_fallbackDevice->GetShaderIdentifierSize();
    }
    else // DirectX Raytracing
    {
        ComPtr<ID3D12StateObjectPropertiesPrototype> stateObjectProperties;
        ThrowIfFailed(m_dxrStateObject.As(&stateObjectProperties));
        GetShaderIdentifiers(stateObjectProperties.Get());
        shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    }

    // Ray gen shader table
    {
        UINT numShaderRecords = 1;
        UINT shaderRecordSize = shaderIdentifierSize;
        ShaderTable rayGenShaderTable(device, numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
        rayGenShaderTable.push_back(ShaderRecord(rayGenShaderIdentifier, shaderIdentifierSize));
        m_rayGenShaderTable = rayGenShaderTable.GetResource();
    }

    // Miss shader table
    {
        UINT numShaderRecords = 1;
        UINT shaderRecordSize = shaderIdentifierSize;
        ShaderTable missShaderTable(device, numShaderRecords, shaderRecordSize, L"MissShaderTable");
        missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderIdentifierSize));
        m_missShaderTable = missShaderTable.GetResource();
    }

    // Hit group shader table
    {
        struct RootArguments {
            CubeConstantBuffer cb;
        } rootArguments;
        rootArguments.cb = m_cubeCB;

        UINT numShaderRecords = 1;
        UINT shaderRecordSize = shaderIdentifierSize + sizeof(rootArguments);
        ShaderTable hitGroupShaderTable(device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");
        hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize, &rootArguments, sizeof(rootArguments)));
        m_hitGroupShaderTable = hitGroupShaderTable.GetResource();
    }
}

void D3D12RaytracingSimpleLighting::SelectRaytracingAPI(RaytracingAPI type)
{
    if (type == RaytracingAPI::FallbackLayer)
    {
        m_raytracingAPI = type;
    }
    else // DirectX Raytracing
    {
        if (m_isDxrSupported)
        {
            m_raytracingAPI = type;
        }
        else
        {
            OutputDebugString(L"Invalid selection - DXR is not available.\n");
        }
    }
}

// Update frame-based values.
void D3D12RaytracingSimpleLighting::OnUpdate()
{ 
  m_timer.Tick();
  CalculateFrameStats();
  float elapsedTime = static_cast<float>(m_timer.GetElapsedSeconds());
  auto frameIndex = m_deviceResources->GetCurrentFrameIndex();
  auto prevFrameIndex = m_deviceResources->GetPreviousFrameIndex();

  UpdateCameraMatrices();

  // Rotate the second light around Y axis.
  {
    float secondsToRotateAround = 8.0f;
    float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
    XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
    const XMVECTOR& prevLightPosition = m_sceneCB[prevFrameIndex].lightPosition;
    m_sceneCB[frameIndex].lightPosition = XMVector3Transform(prevLightPosition, rotate);
  }

  {
    m_sceneCB[frameIndex].iteration += 1;
  }
}


// Parse supplied command line args.
void D3D12RaytracingSimpleLighting::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
    DXSample::ParseCommandLineArgs(argv, argc);

    if (argc > 1)
    {
        if (_wcsnicmp(argv[1], L"-FL", wcslen(argv[1])) == 0 )
        {
            m_forceComputeFallback = true;
            m_raytracingAPI = RaytracingAPI::FallbackLayer;
        }
        else if (_wcsnicmp(argv[1], L"-DXR", wcslen(argv[1])) == 0)
        {
            m_raytracingAPI = RaytracingAPI::DirectXRaytracing;
        }
    }
}

void D3D12RaytracingSimpleLighting::DoRaytracing()
{
    auto commandList = m_deviceResources->GetCommandList();
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();
    
    auto DispatchRays = [&](auto* commandList, auto* stateObject, auto* dispatchDesc)
    {
        // Since each shader table has only one shader record, the stride is same as the size.
        dispatchDesc->HitGroupTable.StartAddress = m_hitGroupShaderTable->GetGPUVirtualAddress();
        dispatchDesc->HitGroupTable.SizeInBytes = m_hitGroupShaderTable->GetDesc().Width;
        dispatchDesc->HitGroupTable.StrideInBytes = dispatchDesc->HitGroupTable.SizeInBytes;
        dispatchDesc->MissShaderTable.StartAddress = m_missShaderTable->GetGPUVirtualAddress();
        dispatchDesc->MissShaderTable.SizeInBytes = m_missShaderTable->GetDesc().Width;
        dispatchDesc->MissShaderTable.StrideInBytes = dispatchDesc->MissShaderTable.SizeInBytes;
        dispatchDesc->RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
        dispatchDesc->RayGenerationShaderRecord.SizeInBytes = m_rayGenShaderTable->GetDesc().Width;
        dispatchDesc->Width = m_width;
        dispatchDesc->Height = m_height;
        dispatchDesc->Depth = 1;
        commandList->SetPipelineState1(stateObject);
        commandList->DispatchRays(dispatchDesc);
    };


    auto SetCommonPipelineState = [&](auto* descriptorSetCommandList)
    {
      ModelLoading::SceneObject& objectInScene = m_sceneLoaded->objects[0];
      ModelLoading::Texture& diffuse_texture = m_sceneLoaded->diffuseTextureMap[0];
      ModelLoading::Texture& normal_texture = m_sceneLoaded->normalTextureMap[0];
      ModelLoading::MaterialResource& material = m_sceneLoaded->materialMap[0];
      ModelLoading::Model& model = m_sceneLoaded->modelMap[0];
      descriptorSetCommandList->SetDescriptorHeaps(1, m_descriptorHeap.GetAddressOf());
      // Set index and successive vertex buffer decriptor tables
      commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::VertexBuffersSlot, model.vertices.gpuDescriptorHandle);
      commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::IndexBuffersSlot, model.indices.gpuDescriptorHandle);
      commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::InfoBuffersSlot, objectInScene.info_resource.d3d12_resource.gpuDescriptorHandle);
      commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputViewSlot, m_raytracingOutputResourceUAVGpuDescriptor);
      commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::TextureSlot, diffuse_texture.texBuffer.gpuDescriptorHandle);
      commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::NormalTextureSlot, normal_texture.texBuffer.gpuDescriptorHandle);
      commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::MaterialBuffersSlot, material.d3d12_material_resource.gpuDescriptorHandle);
    };

    commandList->SetComputeRootSignature(m_raytracingGlobalRootSignature.Get());

    // Copy the updated scene constant buffer to GPU.
    memcpy(&m_mappedConstantData[frameIndex].constants, &m_sceneCB[frameIndex], sizeof(m_sceneCB[frameIndex]));
    auto cbGpuAddress = m_perFrameConstants->GetGPUVirtualAddress() + frameIndex * sizeof(m_mappedConstantData[0]);
    commandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams::SceneConstantSlot, cbGpuAddress);
   
    // Bind the heaps, acceleration structure and dispatch rays.
    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
    if (m_raytracingAPI == RaytracingAPI::FallbackLayer)
    {
        SetCommonPipelineState(m_fallbackCommandList.Get());
        m_fallbackCommandList->SetTopLevelAccelerationStructure(GlobalRootSignatureParams::AccelerationStructureSlot, m_fallbackTopLevelAccelerationStructurePointer);
        DispatchRays(m_fallbackCommandList.Get(), m_fallbackStateObject.Get(), &dispatchDesc);
    }
    else // DirectX Raytracing
    {
        SetCommonPipelineState(commandList);
        commandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::AccelerationStructureSlot, m_topLevelAccelerationStructure->GetGPUVirtualAddress());
        DispatchRays(m_dxrCommandList.Get(), m_dxrStateObject.Get(), &dispatchDesc);
    }
}

// Update the application state with the new resolution.
void D3D12RaytracingSimpleLighting::UpdateForSizeChange(UINT width, UINT height)
{
    DXSample::UpdateForSizeChange(width, height);
}

// Copy the raytracing output to the backbuffer.
void D3D12RaytracingSimpleLighting::CopyRaytracingOutputToBackbuffer()
{
    auto commandList = m_deviceResources->GetCommandList();
    auto renderTarget = m_deviceResources->GetRenderTarget();
    
    D3D12_RESOURCE_BARRIER preCopyBarriers[2];
    preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
    preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    commandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

    commandList->CopyResource(renderTarget, m_raytracingOutput.Get());

    D3D12_RESOURCE_BARRIER postCopyBarriers[2];
    postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
    postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    commandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);

    LoadSplitImage();
    PreSaveImage();

    //Render IMGUI to screen
    RenderImGUI();

    m_deviceResources->ExecuteCommandList();

    m_deviceResources->WaitForGpu();

    commandList->Reset(m_deviceResources->GetCommandAllocator(), nullptr);

    D3D12_RESOURCE_BARRIER ImGUIBarriers[1];
    ImGUIBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    commandList->ResourceBarrier(ARRAYSIZE(ImGUIBarriers), ImGUIBarriers);
}

// Create resources that are dependent on the size of the main window.
void D3D12RaytracingSimpleLighting::CreateWindowSizeDependentResources()
{
    CreateRaytracingOutputResource(); 
    UpdateCameraMatrices();
}

// Release resources that are dependent on the size of the main window.
void D3D12RaytracingSimpleLighting::ReleaseWindowSizeDependentResources()
{
    m_raytracingOutput.Reset();
}

// Release all resources that depend on the device.
void D3D12RaytracingSimpleLighting::ReleaseDeviceDependentResources()
{
    m_fallbackDevice.Reset();
    m_fallbackCommandList.Reset();
    m_fallbackStateObject.Reset();
    m_raytracingGlobalRootSignature.Reset();
    m_raytracingLocalRootSignature.Reset();

    m_dxrDevice.Reset();
    m_dxrCommandList.Reset();
    m_dxrStateObject.Reset();

    m_descriptorHeap.Reset();
    m_descriptorsAllocated = 0;
    m_raytracingOutputResourceUAVDescriptorHeapIndex = UINT_MAX;
    m_indexBuffer.resource.Reset();
    m_vertexBuffer.resource.Reset();
    m_textureBuffer.resource.Reset();
    m_normalTextureBuffer.resource.Reset();
    m_perFrameConstants.Reset();
    m_rayGenShaderTable.Reset();
    m_missShaderTable.Reset();
    m_hitGroupShaderTable.Reset();

    m_bottomLevelAccelerationStructure.Reset();
    m_topLevelAccelerationStructure.Reset();

    ShutdownImGUI();
}

void D3D12RaytracingSimpleLighting::RecreateD3D()
{
    // Give GPU a chance to finish its execution in progress.
    try
    {
        m_deviceResources->WaitForGpu();
    }
    catch (HrException&)
    {
        // Do nothing, currently attached adapter is unresponsive.
    }
    m_deviceResources->HandleDeviceLost();
}

// Render the scene.
void D3D12RaytracingSimpleLighting::OnRender()
{
    if (!m_deviceResources->IsWindowVisible())
    {
        return;
    }

    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();

    //Draw ImGUI
    StartFrameImGUI();

    //if rebuild scene
    if (rebuild_scene)
    {
      m_deviceResources->WaitForGpu();
      RebuildScene();
      rebuild_scene = false;
    }

    m_deviceResources->Prepare();

    commandList->RSSetViewports(1, &m_deviceResources->GetScreenViewport());
    commandList->RSSetScissorRects(1, &m_deviceResources->GetScissorRect());
    commandList->OMSetRenderTargets(1, &m_deviceResources->GetRenderTargetView(), FALSE, nullptr);

    DoRaytracing();
    CopyRaytracingOutputToBackbuffer();
    PostSaveImage();

    if (m_camChanged)
    {
      //reset iterations
      for (int i = 0; i < FrameCount; i++)
      {
        m_sceneCB[i].iteration = 1;
      }

      static bool give_epilepsy = true;
      if (give_epilepsy)
      {
        auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, m_width, m_height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        
        auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(device->CreateCommittedResource(
            &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&cure_epilepsy)));
        NAME_D3D12_OBJECT(cure_epilepsy);
        give_epilepsy = false;
      }

      D3D12_RESOURCE_BARRIER preCopyBarriers[2];
      preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(pathtracing_accumulation_resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
      preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(cure_epilepsy.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
      commandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

      commandList->CopyResource(pathtracing_accumulation_resource.Get(), cure_epilepsy.Get());

      D3D12_RESOURCE_BARRIER postCopyBarriers[2];
      postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(pathtracing_accumulation_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(cure_epilepsy.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

      commandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
      m_camChanged = false;

      m_deviceResources->WaitForGpu();
    }

    m_deviceResources->Present(D3D12_RESOURCE_STATE_PRESENT);
}

void D3D12RaytracingSimpleLighting::OnDestroy()
{
    // Let GPU finish before releasing D3D resources.
    m_deviceResources->WaitForGpu();
    OnDeviceLost();
}

// Release all device dependent resouces when a device is lost.
void D3D12RaytracingSimpleLighting::OnDeviceLost()
{
    ReleaseWindowSizeDependentResources();
    ReleaseDeviceDependentResources();
}

// Create all device dependent resources when a device is restored.
void D3D12RaytracingSimpleLighting::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

// Compute the average frames per second and million rays per second.
void D3D12RaytracingSimpleLighting::CalculateFrameStats()
{
    static int frameCnt = 0;
    static double elapsedTime = 0.0f;
    double totalTime = m_timer.GetTotalSeconds();
    frameCnt++;

    // Compute averages over one second period.
    if ((totalTime - elapsedTime) >= 1.0f)
    {
        float diff = static_cast<float>(totalTime - elapsedTime);
        float fps = static_cast<float>(frameCnt) / diff; // Normalize to an exact second.

        frameCnt = 0;
        elapsedTime = totalTime;

        float MRaysPerSecond = (m_width * m_height * fps) / static_cast<float>(1e6);

        wstringstream windowText;

        if (m_raytracingAPI == RaytracingAPI::FallbackLayer)
        {
            if (m_fallbackDevice->UsingRaytracingDriver())
            {
                windowText << L"(FL-DXR)";
            }
            else
            {
                windowText << L"(FL)";
            }
        }
        else
        {
            windowText << L"(DXR)";
        }
        windowText << setprecision(2) << fixed
            << L"    fps: " << fps << L"     ~Million Primary Rays/s: " << MRaysPerSecond
            << L"    GPU[" << m_deviceResources->GetAdapterID() << L"]: " << m_deviceResources->GetAdapterDescription();
        SetCustomWindowText(windowText.str().c_str());
    }
}

// Handle OnSizeChanged message event.
void D3D12RaytracingSimpleLighting::OnSizeChanged(UINT width, UINT height, bool minimized)
{
    if (!m_deviceResources->WindowSizeChanged(width, height, minimized))
    {
        return;
    }

    UpdateForSizeChange(width, height);

    ReleaseWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

// Create a wrapped pointer for the Fallback Layer path.
WRAPPED_GPU_POINTER D3D12RaytracingSimpleLighting::CreateFallbackWrappedPointer(ID3D12Resource* resource, UINT bufferNumElements)
{
    auto device = m_deviceResources->GetD3DDevice();

    D3D12_UNORDERED_ACCESS_VIEW_DESC rawBufferUavDesc = {};
    rawBufferUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    rawBufferUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    rawBufferUavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    rawBufferUavDesc.Buffer.NumElements = bufferNumElements;

    D3D12_CPU_DESCRIPTOR_HANDLE bottomLevelDescriptor;
   
    // Only compute fallback requires a valid descriptor index when creating a wrapped pointer.
    UINT descriptorHeapIndex = 0;
    if (!m_fallbackDevice->UsingRaytracingDriver())
    {
        descriptorHeapIndex = AllocateDescriptor(&bottomLevelDescriptor);
        device->CreateUnorderedAccessView(resource, nullptr, &rawBufferUavDesc, bottomLevelDescriptor);
    }
    return m_fallbackDevice->GetWrappedPointerSimple(descriptorHeapIndex, resource->GetGPUVirtualAddress());
}

// Allocate a descriptor and return its index. 
// If the passed descriptorIndexToUse is valid, it will be used instead of allocating a new one.
UINT D3D12RaytracingSimpleLighting::AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse)
{
    auto descriptorHeapCpuBase = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    if (descriptorIndexToUse >= m_descriptorHeap->GetDesc().NumDescriptors)
    {
        descriptorIndexToUse = m_descriptorsAllocated++;
    }
    *cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, m_descriptorSize);
    return descriptorIndexToUse;
}

// Create SRV for a buffer.
UINT D3D12RaytracingSimpleLighting::CreateBufferSRV(D3DBuffer* buffer, UINT numElements, UINT elementSize)
{
    auto device = m_deviceResources->GetD3DDevice();

    // SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.NumElements = numElements;
    if (elementSize == 0)
    {
        srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        srvDesc.Buffer.StructureByteStride = 0;
    }
    else
    {
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        srvDesc.Buffer.StructureByteStride = elementSize;
    }
    UINT descriptorIndex = AllocateDescriptor(&buffer->cpuDescriptorHandle);
    device->CreateShaderResourceView(buffer->resource.Get(), &srvDesc, buffer->cpuDescriptorHandle);
    buffer->gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, m_descriptorSize);
    return descriptorIndex;
}

void D3D12RaytracingSimpleLighting::OnKeyDown(UINT8 key)
{
  if(ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantTextInput)
  {
    return;
  }
	// Store previous values.
	RaytracingAPI previousRaytracingAPI = m_raytracingAPI;
	bool previousForceComputeFallback = m_forceComputeFallback;

	switch (key)
	{
	case VK_NUMPAD1:
	case '1': // Fallback Layer
		m_forceComputeFallback = false;
		SelectRaytracingAPI(RaytracingAPI::FallbackLayer);
		break;
	case VK_NUMPAD2:
	case '2': // Fallback Layer + force compute path
		m_forceComputeFallback = true;
		SelectRaytracingAPI(RaytracingAPI::FallbackLayer);
		break;
	case VK_NUMPAD3:
	case '3': // DirectX Raytracing
		SelectRaytracingAPI(RaytracingAPI::DirectXRaytracing);
		break;
	case 0x44: // D
	{
		XMFLOAT4 right;
		XMVector4Normalize(m_right);
		XMStoreFloat4(&right, m_right);
		XMMATRIX translate = XMMatrixTranslation(-right.x * c_movementAmountFactor, -right.y * c_movementAmountFactor, -right.z * c_movementAmountFactor);

		m_eye = XMVector3Transform(m_eye, translate);
		m_at = XMVector3Transform(m_at, translate);
		m_camChanged = true;
		break;
	}

	case 0x41: // A
	{
		XMFLOAT4 right;
		XMVector4Normalize(m_right);
		XMStoreFloat4(&right, m_right);
		XMMATRIX translate = XMMatrixTranslation(right.x * c_movementAmountFactor, right.y * c_movementAmountFactor, right.z * c_movementAmountFactor);

		m_eye = XMVector3Transform(m_eye, translate);
		m_at = XMVector3Transform(m_at, translate);
		m_camChanged = true;
		break;
	}

	case 0x53: // S
	{
		XMFLOAT4 up;
		XMVector4Normalize(m_up);
		XMStoreFloat4(&up, m_up);
		XMMATRIX translate = XMMatrixTranslation(up.x * c_movementAmountFactor, up.y * c_movementAmountFactor, up.z * c_movementAmountFactor);

		m_eye = XMVector3Transform(m_eye, translate);
		m_at = XMVector3Transform(m_at, translate);
		m_camChanged = true;
		break;
	}

	case 0x57: // W
	{
		XMFLOAT4 up;
		XMVector4Normalize(m_up);
		XMStoreFloat4(&up, m_up);
		XMMATRIX translate = XMMatrixTranslation(-up.x * c_movementAmountFactor, -up.y * c_movementAmountFactor, -up.z * c_movementAmountFactor);

		m_eye = XMVector3Transform(m_eye, translate);
		m_at = XMVector3Transform(m_at, translate);
		m_camChanged = true;
		break;
	}

	case 0x45: // E
	{
		XMFLOAT4 forward;
		XMVector4Normalize(m_forward);
		XMStoreFloat4(&forward, m_forward);
		XMMATRIX translate = XMMatrixTranslation(forward.x * c_movementAmountFactor, forward.y * c_movementAmountFactor, forward.z * c_movementAmountFactor);

		m_eye = XMVector3Transform(m_eye, translate);
		m_at = XMVector3Transform(m_at, translate);
		m_camChanged = true;
		break;
	}

	case 0x51: // Q
	{
		XMFLOAT4 forward;
		XMVector4Normalize(m_forward);
		XMStoreFloat4(&forward, m_forward);
		XMMATRIX translate = XMMatrixTranslation(-forward.x * c_movementAmountFactor, -forward.y * c_movementAmountFactor, -forward.z * c_movementAmountFactor);

		m_eye = XMVector3Transform(m_eye, translate);
		m_at = XMVector3Transform(m_at, translate);
		m_camChanged = true;
		break;
	}

	case VK_UP: // Up arrow
	{
		XMMATRIX rotate = XMMatrixRotationAxis(m_right, XMConvertToRadians(-c_rotateDegrees));
		m_eye = XMVector3Transform(m_eye, rotate);
		m_up = XMVector3Transform(m_up, rotate);
		m_forward = XMVector3Transform(m_forward, rotate);
		m_at = XMVector3Transform(m_at, rotate);
		m_camChanged = true;
		break;
	}

	case VK_DOWN: // Down arrow
	{
		XMMATRIX rotate = XMMatrixRotationAxis(m_right, XMConvertToRadians(c_rotateDegrees));
		m_eye = XMVector3Transform(m_eye, rotate);
		m_up = XMVector3Transform(m_up, rotate);
		m_forward = XMVector3Transform(m_forward, rotate);
		m_at = XMVector3Transform(m_at, rotate);
		m_camChanged = true;
		break;
	}

	case VK_RIGHT: // Right arrow
	{
		XMMATRIX rotate = XMMatrixRotationAxis(m_up, XMConvertToRadians(c_rotateDegrees));
		m_eye = XMVector3Transform(m_eye, rotate);
		m_right = XMVector3Transform(m_right, rotate);
		m_forward = XMVector3Transform(m_forward, rotate);
		m_at = XMVector3Transform(m_at, rotate);
		m_camChanged = true;
		break;
	}

	case VK_LEFT: // Left arrow
	{
		XMMATRIX rotate = XMMatrixRotationAxis(m_up, XMConvertToRadians(-c_rotateDegrees));
		m_eye = XMVector3Transform(m_eye, rotate);
		m_right = XMVector3Transform(m_right, rotate);
		m_forward = XMVector3Transform(m_forward, rotate);
		m_at = XMVector3Transform(m_at, rotate);
		m_camChanged = true;
		break;
	}
	}

	if (m_raytracingAPI != previousRaytracingAPI ||
		m_forceComputeFallback != previousForceComputeFallback)
	{
		// Raytracing API selection changed, recreate everything.
		RecreateD3D();
	}
}

void D3D12RaytracingSimpleLighting::StartFrameImGUI()
{
  auto FormatIdAndName = [&](std::string type, auto& object)
  {
    return std::string(utilityCore::stringAndId(type, object.id) + " | " + object.name);
  };

  auto FindIdFromFormat = [&](std::string format)
  {
    auto found_first_space = format.find(" ");
    found_first_space++;
    auto found_second_space = format.find(" ", found_first_space);
    int id = 0;
    try
    {
      id = std::stol(format.substr(found_first_space, found_second_space - found_first_space));
    }
    catch(std::exception& e)
    {
      return 0;
    }
    return id;
  };

  auto ShowHelpMarker = [&](const char* desc)
  {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
      ImGui::BeginTooltip();
      ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
      ImGui::TextUnformatted(desc);
      ImGui::PopTextWrapPos();
      ImGui::EndTooltip();
    }
  };

  auto ResetPathTracing = [&]()
  {
    m_camChanged = true;
  };

  auto GetNewCPUGPUHandles = [&]
  {
    current_imgui_heap_descriptor++;
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(), current_imgui_heap_descriptor, m_descriptorSize);
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpu_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart(), current_imgui_heap_descriptor, m_descriptorSize);
    return std::make_pair(cpu_handle, gpu_handle);
  };

  auto UpdateObject = [&](const ModelLoading::SceneObject& object)
  {
    //update the resource info
    void* mapped_data;
    object.info_resource.d3d12_resource.resource->Map(0, nullptr, &mapped_data);
    memcpy(mapped_data, &object.info_resource.info, sizeof(Info));
    object.info_resource.d3d12_resource.resource->Unmap(0, nullptr);

    ResetPathTracing();
  };

  static ImGuiFs::Dialog dlg; // one per dialog (and must be static)

#define NAME_LIMIT (256)

  auto ShowModel = [&](ModelLoading::Model& model)
  {
    if (ImGui::TreeNode(FormatIdAndName("Model", model).c_str()))
    {
      ImGui::Text("Model name", model.name, NAME_LIMIT);

      if (ImGui::TreeNode("Vertices"))
      {
        int line_offset = 10;
        ImGui::PushItemWidth(100);
        ImGui::InputInt("##Line", &model.vertex_line, 0, 0, 0);
        ImGui::SameLine();
        ImGui::SliderInt("##Line2", &model.vertex_line, 0, model.vertices_vec.size());
        ImGui::PopItemWidth();

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
        ImGui::BeginChild("Child2", ImVec2(600, 0), true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar);
        ImGui::Columns(9);

        int begin = model.vertex_line - line_offset;
        begin = begin >= 0 ? begin : 0;

        int end = model.vertex_line + line_offset;
        end = end >= model.vertices_vec.size() ? model.vertices_vec.size() - 1 : end;

        //header
        ImGui::Text("Index");
        ImGui::NextColumn();
        ImGui::Text("Vertex");
        ImGui::NextColumn();
        ImGui::NextColumn();
        ImGui::NextColumn();
        ImGui::Text("Normal");
        ImGui::NextColumn();
        ImGui::NextColumn();
        ImGui::NextColumn();
        ImGui::Text("UV");
        ImGui::NextColumn();
        ImGui::NextColumn();
        ImGui::Separator();

        //contents
        for (std::size_t i = begin; i < end; i++)
        {
          ImGui::Text("%d", i);
          ImGui::NextColumn();
          const auto& vertex = model.vertices_vec[i];
          ImGui::Text("%.2f", vertex.position.x);
          ImGui::NextColumn();
          ImGui::Text("%.2f", vertex.position.y);
          ImGui::NextColumn();
          ImGui::Text("%.2f", vertex.position.z);
          ImGui::NextColumn();
          ImGui::Text("%.2f", vertex.normal.x);
          ImGui::NextColumn();
          ImGui::Text("%.2f", vertex.normal.y);
          ImGui::NextColumn();
          ImGui::Text("%.2f", vertex.normal.z);
          ImGui::NextColumn();
          ImGui::Text("%.2f", vertex.texCoord.x);
          ImGui::NextColumn();
          ImGui::Text("%.2f", vertex.texCoord.y);
          ImGui::NextColumn();
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::TreePop();
      }

      if (ImGui::TreeNode("Indices"))
      {
        int line_offset = 10;
        ImGui::PushItemWidth(100);
        ImGui::InputInt("##Line", &model.indices_line, 0, 0, 0);
        ImGui::SameLine();
        ImGui::SliderInt("##Line2", &model.indices_line, 0, model.indices_vec.size());
        ImGui::PopItemWidth();

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
        ImGui::BeginChild("Child2", ImVec2(600, 0), true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar);
        ImGui::Columns(2);

        int begin = model.indices_line - line_offset;
        begin = begin >= 0 ? begin : 0;

        int end = model.indices_line + line_offset;
        end = end >= model.indices_vec.size() ? model.indices_vec.size() - 1 : end;

        //header
        ImGui::Text("Index");
        ImGui::NextColumn();
        ImGui::Text("Index Value");
        ImGui::NextColumn();
        ImGui::Separator();

        //contents
        for (std::size_t i = begin; i < end; i++)
        {
          ImGui::Text("%d", i);
          ImGui::NextColumn();
          const auto& index = model.indices_vec[i];
          ImGui::Text("%d", index);
          ImGui::NextColumn();
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::TreePop();
      }

      if (ImGui::TreeNode("Resource"))
      {
        ImGui::Text("Resource Pointer: %p", model.m_bottomLevelAccelerationStructure.GetAddressOf());
        ImGui::TreePop();
      }

      ImGui::TreePop();
    }
  };

  auto ShowMaterial = [&](ModelLoading::MaterialResource& material_resource)
  {
    if (ImGui::TreeNode(FormatIdAndName("Material", material_resource).c_str()))
    {
      std::vector<char> material_name(NAME_LIMIT, '\0');
      std::copy(std::begin(material_resource.name), std::end(material_resource.name), std::begin(material_name));

      bool input_text_update = ImGui::InputText("Material name", &material_name[0], NAME_LIMIT, ImGuiInputTextFlags_EnterReturnsTrue);

      ImGui::ColorEdit3("Diffuse", &material_resource.material.diffuse.x);
      ImGui::SameLine(); ShowHelpMarker("Click on the colored square to open a color picker.\nRight-click on the colored square to show options.\nCTRL+click on individual component to input value.\n");
      ImGui::ColorEdit3("Specular", &material_resource.material.specular.x);
      ImGui::SliderFloat("Specular Exponent", &material_resource.material.specularExp, 0.0f, 10.0f);
      ImGui::SliderFloat("Reflectiveness", &material_resource.material.reflectiveness, 0.0f, 10.0f);
      ImGui::SliderFloat("Refractiveness", &material_resource.material.refractiveness, 0.0f, 10.0f);
      ImGui::SliderFloat("Index of Refraction", &material_resource.material.eta, 0.0f, 10.0f);
      ImGui::SliderFloat("Emittance", &material_resource.material.emittance, 0.0f, 10.0f);
      if(ImGui::TreeNode("Resource"))
      {
        ImGui::Text("Resource Pointer: %p", material_resource.d3d12_material_resource.resource.GetAddressOf());
        ImGui::Text("CPU handle: %p", material_resource.d3d12_material_resource.cpuDescriptorHandle.ptr);
        ImGui::Text("GPU handle: %p", material_resource.d3d12_material_resource.gpuDescriptorHandle.ptr);
        ImGui::TreePop();
      }

      if(ImGui::Button("Update") || input_text_update)
      {
        material_resource.name = std::string(std::begin(material_name), std::begin(material_name) + ::strlen(&material_name[0]));

        void* data;
        material_resource.d3d12_material_resource.resource->Map(0, nullptr, &data);
        memcpy(data, &material_resource.material, sizeof(Material));
        material_resource.d3d12_material_resource.resource->Unmap(0, nullptr);

        ResetPathTracing();
      }

      ImGui::TreePop();
    }
  };

  auto ShowDiffuseTexture = [&](ModelLoading::Texture& diffuse_texture, ModelLoading::SceneObject* object = nullptr)
  {
    if (ImGui::TreeNode(FormatIdAndName("Diffuse Texture", diffuse_texture).c_str()))
    {
      ImGui::Text("Texture name %s", diffuse_texture.name.c_str());

      if (object != nullptr)
      {
        std::vector<std::string> samplers(2);
        samplers[0] = "Wrap";
        samplers[1] = "Mirror";

        if (ImGui::Button("Select sampler"))
          ImGui::OpenPopup("sampler_select");

        if (ImGui::BeginPopup("sampler_select"))
        {
          ImGui::Text("Sampler");
          ImGui::Separator();

          for (std::size_t i = 0; i < samplers.size(); i++)
          {
            if (ImGui::Selectable(samplers[i].data()))
            {
              diffuse_texture.sampler_offset = i;
              object->info_resource.info.diffuse_sampler_offset = diffuse_texture.sampler_offset;
              UpdateObject(*object);
            }
          }

          ImGui::EndPopup();
        }

        ImGui::SameLine();
        ImGui::Text("Sampler: %s", samplers[diffuse_texture.sampler_offset].c_str());
      }

      if (ImGui::TreeNode("Raw Texture"))
      {
        ImGuiIO& io = ImGui::GetIO();

        auto device = m_deviceResources->GetD3DDevice();

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = diffuse_texture.textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        const auto cpu_gpu_handles = GetNewCPUGPUHandles();
        device->CreateShaderResourceView(diffuse_texture.texBuffer.resource.Get(), &srvDesc, cpu_gpu_handles.first);
        ImTextureID my_tex_id = (ImTextureID)cpu_gpu_handles.second.ptr;
        float my_tex_w = (float)diffuse_texture.textureDesc.Width;
        float my_tex_h = (float)diffuse_texture.textureDesc.Height;

        ImGui::Text("%.0fx%.0f", my_tex_w, my_tex_h);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::Image(my_tex_id, ImVec2(my_tex_w, my_tex_h), ImVec2(0, 0), ImVec2(1, 1), ImColor(255, 255, 255, 255), ImColor(255, 255, 255, 128));
        if (ImGui::IsItemHovered())
        {
          ImGui::BeginTooltip();
          float region_sz = 32.0f;
          float region_x = io.MousePos.x - pos.x - region_sz * 0.5f; if (region_x < 0.0f) region_x = 0.0f; else if (region_x > my_tex_w - region_sz) region_x = my_tex_w - region_sz;
          float region_y = io.MousePos.y - pos.y - region_sz * 0.5f; if (region_y < 0.0f) region_y = 0.0f; else if (region_y > my_tex_h - region_sz) region_y = my_tex_h - region_sz;
          float zoom = 4.0f;
          ImGui::Text("Min: (%.2f, %.2f)", region_x, region_y);
          ImGui::Text("Max: (%.2f, %.2f)", region_x + region_sz, region_y + region_sz);
          ImVec2 uv0 = ImVec2((region_x) / my_tex_w, (region_y) / my_tex_h);
          ImVec2 uv1 = ImVec2((region_x + region_sz) / my_tex_w, (region_y + region_sz) / my_tex_h);
          ImGui::Image(my_tex_id, ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1, ImColor(255, 255, 255, 255), ImColor(255, 255, 255, 128));
          ImGui::EndTooltip();
        }

        ImGui::TreePop();
      }

      if (ImGui::TreeNode("Resource"))
      {
        ImGui::Text("Resource Pointer: %p", diffuse_texture.texBuffer.resource.GetAddressOf());
        ImGui::Text("CPU handle: %p", diffuse_texture.texBuffer.cpuDescriptorHandle.ptr);
        ImGui::Text("GPU handle: %p", diffuse_texture.texBuffer.gpuDescriptorHandle.ptr);
        ImGui::TreePop();
      }

      ImGui::TreePop();
    }
  };

  auto ShowNormalTexture = [&](ModelLoading::Texture& normal_texture, ModelLoading::SceneObject* object = nullptr)
  {
    if (ImGui::TreeNode(FormatIdAndName("Normal Texture", normal_texture).c_str()))
    {
      ImGui::Text("Texture name %s", normal_texture.name.c_str());

      if (object != nullptr)
      {
        std::vector<std::string> samplers(2);
        samplers[0] = "Wrap";
        samplers[1] = "Mirror";

        if (ImGui::Button("Select sampler"))
          ImGui::OpenPopup("sampler_select");

        if (ImGui::BeginPopup("sampler_select"))
        {
          ImGui::Text("Sampler");
          ImGui::Separator();
          for (std::size_t i = 0; i < samplers.size(); i++)
          {
            if (ImGui::Selectable(samplers[i].data()))
            {
              normal_texture.sampler_offset = i;
              object->info_resource.info.texture_normal_offset = normal_texture.sampler_offset;
              UpdateObject(*object);
            }
          }

          ImGui::EndPopup();
        }

        ImGui::SameLine();
        ImGui::Text("Sampler: %s", samplers[normal_texture.sampler_offset].c_str());
      }

      if (ImGui::TreeNode("Raw Texture"))
      {
        ImGuiIO& io = ImGui::GetIO();

        auto device = m_deviceResources->GetD3DDevice();

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = normal_texture.textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        const auto cpu_gpu_handles = GetNewCPUGPUHandles();
        device->CreateShaderResourceView(normal_texture.texBuffer.resource.Get(), &srvDesc, cpu_gpu_handles.first);
        ImTextureID my_tex_id = (ImTextureID)cpu_gpu_handles.second.ptr;
        float my_tex_w = (float)normal_texture.textureDesc.Width;
        float my_tex_h = (float)normal_texture.textureDesc.Height;

        ImGui::Text("%.0fx%.0f", my_tex_w, my_tex_h);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::Image(my_tex_id, ImVec2(my_tex_w, my_tex_h), ImVec2(0, 0), ImVec2(1, 1), ImColor(255, 255, 255, 255), ImColor(255, 255, 255, 128));
        if (ImGui::IsItemHovered())
        {
          ImGui::BeginTooltip();
          float region_sz = 32.0f;
          float region_x = io.MousePos.x - pos.x - region_sz * 0.5f; if (region_x < 0.0f) region_x = 0.0f; else if (region_x > my_tex_w - region_sz) region_x = my_tex_w - region_sz;
          float region_y = io.MousePos.y - pos.y - region_sz * 0.5f; if (region_y < 0.0f) region_y = 0.0f; else if (region_y > my_tex_h - region_sz) region_y = my_tex_h - region_sz;
          float zoom = 4.0f;
          ImGui::Text("Min: (%.2f, %.2f)", region_x, region_y);
          ImGui::Text("Max: (%.2f, %.2f)", region_x + region_sz, region_y + region_sz);
          ImVec2 uv0 = ImVec2((region_x) / my_tex_w, (region_y) / my_tex_h);
          ImVec2 uv1 = ImVec2((region_x + region_sz) / my_tex_w, (region_y + region_sz) / my_tex_h);
          ImGui::Image(my_tex_id, ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1, ImColor(255, 255, 255, 255), ImColor(255, 255, 255, 128));
          ImGui::EndTooltip();
        }

        ImGui::TreePop();
      }
      
      if (ImGui::TreeNode("Resource"))
      {
        ImGui::Text("Resource Pointer: %p", normal_texture.texBuffer.resource.GetAddressOf());
        ImGui::Text("CPU handle: %p", normal_texture.texBuffer.cpuDescriptorHandle.ptr);
        ImGui::Text("GPU handle: %p", normal_texture.texBuffer.gpuDescriptorHandle.ptr);
        ImGui::TreePop();
      }

      ImGui::TreePop();
    }
  };

  auto ShowObjectHeader = [&]()
  {
    if (ImGui::CollapsingHeader("Objects"))
    {
      for(auto& object : m_sceneLoaded->objects)
      {
        if (ImGui::TreeNode(FormatIdAndName("Object", object).c_str()))
        {
          ImGui::DragFloat3("Translation", &object.translation.x, 0.01f, 0, 0, "%.3f", 20.0f);
          ImGui::DragFloat3("Rotation", &object.rotation.x, 0.05f, 0, 0, "%.3f", 200.0f);
          ImGui::DragFloat3("Scale", &object.scale.x, 0.01f, 0, 0, "%.3f", 20.0f);

          if (object.model != nullptr)
          {
            ShowModel(*object.model);
          }

          if (object.material != nullptr)
          {
            ShowMaterial(*object.material);
          }

          if (object.textures.albedoTex != nullptr)
          {
            ShowDiffuseTexture(*object.textures.albedoTex, &object);
          }

          if (object.textures.normalTex != nullptr)
          {
            ShowNormalTexture(*object.textures.normalTex, &object);
          }

          //model
          std::vector<std::pair<std::string, ModelLoading::Model*>> model_names;
          model_names.reserve(m_sceneLoaded->modelMap.size() + 1);
          model_names.emplace_back(std::make_pair("None", nullptr));
          for (auto& material_pair : m_sceneLoaded->modelMap)
          {
            auto& model = material_pair.second;
            model_names.emplace_back(FormatIdAndName("Model", model), &model);
          }

          if (ImGui::Button("Select model"))
            ImGui::OpenPopup("model_select");

          if (ImGui::BeginPopup("model_select"))
          {
            ImGui::Text("Model");
            ImGui::Separator();
            for (std::size_t i = 0; i < model_names.size(); i++)
            {
              if (ImGui::Selectable(model_names[i].first.data()))
              {
                if (i != 0)
                {
                  object.model = model_names[i].second;
                  object.info_resource.info.model_offset = object.model->id;
                }
                else
                {
                  object.model = nullptr;
                  object.info_resource.info.model_offset = -1;
                }

                //update the model
                rebuild_scene = true;
              }
            }

            ImGui::EndPopup();
          }

          //materials
          std::vector<std::pair<std::string, ModelLoading::MaterialResource*>> material_names;
          material_names.reserve(m_sceneLoaded->materialMap.size() + 1);
          material_names.emplace_back(std::make_pair("None", nullptr));
          for (auto& material_pair : m_sceneLoaded->materialMap)
          {
            auto& material = material_pair.second;
            material_names.emplace_back(FormatIdAndName("Material", material), &material);
          }

          if (ImGui::Button("Select material"))
            ImGui::OpenPopup("material_select");

          if (ImGui::BeginPopup("material_select"))
          {
            ImGui::Text("Material");
            ImGui::Separator();
            for (std::size_t i = 0; i < material_names.size(); i++)
            {
              if (ImGui::Selectable(material_names[i].first.data()))
              {
                if (i != 0)
                {
                  if (!m_sceneLoaded->materialMap[i - 1].was_loaded_from_gltf)
                  {
                    object.material = material_names[i].second;
                    object.info_resource.info.material_offset = object.material->id;
                  }
                  else
                  {
                    ImGui::BeginPopup("Cannot set to this item since this item is from a GLTF file");
                    ImGui::EndPopup();
                  }
                }
                else
                {
                  object.material = nullptr;
                  object.info_resource.info.material_offset = -1;
                }

                //update the resource info
                void* mapped_data;
                object.info_resource.d3d12_resource.resource->Map(0, nullptr, &mapped_data);
                memcpy(mapped_data, &object.info_resource.info, sizeof(Info));
                object.info_resource.d3d12_resource.resource->Unmap(0, nullptr);

                ResetPathTracing();
              }
            }

            ImGui::EndPopup();
          }

          //diffuse textures
          std::vector<std::pair<std::string, ModelLoading::Texture*>> diffuse_texture_names;
          diffuse_texture_names.reserve(m_sceneLoaded->diffuseTextureMap.size() + 1);
          diffuse_texture_names.emplace_back(std::make_pair("None", nullptr));
          for (auto& diffuse_texture_pair : m_sceneLoaded->diffuseTextureMap)
          {
            auto& diffuse_texture = diffuse_texture_pair.second;
            diffuse_texture_names.emplace_back(FormatIdAndName("Diffuse Texture", diffuse_texture), &diffuse_texture);
          }

          if (ImGui::Button("Select diffuse texture"))
            ImGui::OpenPopup("diffuse_texture_select");

          if (ImGui::BeginPopup("diffuse_texture_select"))
          {
            ImGui::Text("Diffuse");
            ImGui::Separator();
            for (std::size_t i = 0; i < diffuse_texture_names.size(); i++)
            {
              if (ImGui::Selectable(diffuse_texture_names[i].first.data()))
              {
                if (i != 0)
                {
                  if (!m_sceneLoaded->diffuseTextureMap[i - 1].was_loaded_from_gltf)
                  {
                    object.textures.albedoTex = diffuse_texture_names[i].second;
                    object.info_resource.info.texture_offset = object.textures.albedoTex->id;
                  }
                  else
                  {
                    ImGui::BeginPopup("Cannot set to this item since this item is from a GLTF file");
                    ImGui::EndPopup();
                  }
                }
                else
                {
                  object.textures.albedoTex = nullptr;
                  object.info_resource.info.texture_offset = -1;
                }

                //update the resource info
                void* mapped_data;
                object.info_resource.d3d12_resource.resource->Map(0, nullptr, &mapped_data);
                memcpy(mapped_data, &object.info_resource.info, sizeof(Info));
                object.info_resource.d3d12_resource.resource->Unmap(0, nullptr);

                ResetPathTracing();
              }
            }

            ImGui::EndPopup();
          }

          //normal textures
          std::vector<std::pair<std::string, ModelLoading::Texture*>> normal_texture_names;
          normal_texture_names.reserve(m_sceneLoaded->normalTextureMap.size() + 1);
          normal_texture_names.emplace_back(std::make_pair("None", nullptr));
          for (auto& normal_texture_pair : m_sceneLoaded->normalTextureMap)
          {
            auto& normal_texture = normal_texture_pair.second;
            normal_texture_names.emplace_back(FormatIdAndName("Normal Texture", normal_texture), &normal_texture);
          }

          if (ImGui::Button("Select normal texture"))
            ImGui::OpenPopup("normal_texture_select");

          if (ImGui::BeginPopup("normal_texture_select"))
          {
            ImGui::Text("Normal");
            ImGui::Separator();
            for (std::size_t i = 0; i < normal_texture_names.size(); i++)
            {
              if (ImGui::Selectable(normal_texture_names[i].first.data()))
              {
                if (i != 0)
                {
                  if (!m_sceneLoaded->normalTextureMap[i - 1].was_loaded_from_gltf)
                  {
                    object.textures.normalTex = normal_texture_names[i].second;
                    object.info_resource.info.texture_normal_offset = object.textures.normalTex->id;
                  }
                  else
                  {
                    ImGui::BeginPopup("Cannot set to this item since this item is from a GLTF file");
                    ImGui::EndPopup();
                  }
                }
                else
                {
                  object.textures.normalTex = nullptr;
                  object.info_resource.info.texture_normal_offset = -1;
                }
              }

              //update the resource info
              void* mapped_data;
              object.info_resource.d3d12_resource.resource->Map(0, nullptr, &mapped_data);
              memcpy(mapped_data, &object.info_resource.info, sizeof(Info));
              object.info_resource.d3d12_resource.resource->Unmap(0, nullptr);

              ResetPathTracing();
            }

            ImGui::EndPopup();
          }

          if (ImGui::Button("Update"))
          {
            rebuild_scene = true;
          }

          ImGui::TreePop();
        }
      }

      //add object
      if (ImGui::Button("Add Empty Object"))
      {
        MakeEmptyObject();
      }
    }
  };

  auto ShowHelpHeader = [&]()
  {
    if (ImGui::CollapsingHeader("Help"))
    {
      ImGui::BulletText("Double-click on title bar to collapse window.");
      ImGui::BulletText("Click and drag on lower right corner to resize window\n(double-click to auto fit window to its contents).");
      ImGui::BulletText("Click and drag on any empty space to move window.");
      ImGui::BulletText("TAB/SHIFT+TAB to cycle through keyboard editable fields.");
      ImGui::BulletText("CTRL+Click on a slider or drag box to input value as text.");
      if (ImGui::GetIO().FontAllowUserScaling)
          ImGui::BulletText("CTRL+Mouse Wheel to zoom window contents.");
      ImGui::BulletText("Mouse Wheel to scroll.");
      ImGui::BulletText("While editing text:\n");
      ImGui::Indent();
      ImGui::BulletText("Hold SHIFT or use mouse to select text.");
      ImGui::BulletText("CTRL+Left/Right to word jump.");
      ImGui::BulletText("CTRL+A or double-click to select all.");
      ImGui::BulletText("CTRL+X,CTRL+C,CTRL+V to use clipboard.");
      ImGui::BulletText("CTRL+Z,CTRL+Y to undo/redo.");
      ImGui::BulletText("ESCAPE to revert.");
      ImGui::BulletText("You can apply arithmetic operators +,*,/ on numerical values.\nUse +- to subtract.");
      ImGui::Unindent();
    }
  };

  auto ShowFeaturesHeader = [&]()
  {
    if (ImGui::CollapsingHeader("Features"))
    {  
      ImGui::Checkbox("Anti-Aliasing", &enable_anti_aliasing);
      ImGui::Checkbox("Depth Of Field", &enable_depth_of_field);

      ImGui::DragInt("Iteration depth", reinterpret_cast<int*>(&feature_depth));

      if (ImGui::Button("Update"))
      {
        const auto frame_index = m_deviceResources->GetCurrentFrameIndex();
        auto current_scene = m_sceneCB[frame_index];
        current_scene.features = 0;
        current_scene.features |= enable_anti_aliasing ? AntiAliasing : 0;
        current_scene.features |= enable_depth_of_field ? DepthOfField : 0;
        current_scene.iteration = feature_depth;

        for (std::size_t  i = 0; i < FrameCount; i++)
        {
          m_sceneCB[i] = current_scene;
        }

        ResetPathTracing();
      }
    }
  };

  auto ShowModelHeader = [&]()
  {
    if (ImGui::CollapsingHeader("Models"))
    {
      for (auto& model_pair : m_sceneLoaded->modelMap)
      {
        auto& model = model_pair.second;
        ShowModel(model);
      }

      const bool browseButtonPressed = ImGui::Button("Add model");
      const char* chosen_path = dlg.chooseFileDialog(browseButtonPressed, nullptr, ".obj");

      if (strlen(chosen_path) > 0)
      {
        LoadModel(chosen_path);
      }
      else if(browseButtonPressed)
      {
        ImGui::Text("Invalid path");
      }
    }
  };

  auto ShowMaterialHeader = [&]()
  {
    if (ImGui::CollapsingHeader("Materials"))
    {
      for (auto& material_pair : m_sceneLoaded->materialMap)
      {
        auto& material = material_pair.second;
        ShowMaterial(material);
      }

      if(ImGui::Button("Make Empty Material"))
      {
        MakeEmptyMaterial();
      }
    }
  };

  std::string possible_image_extensions = ".tiff;.gif;.bmp;.png;.jpg;.jpeg;.tga";

  auto ShowDiffuseTextureHeader = [&]()
  {
    if (ImGui::CollapsingHeader("Diffuse Textures"))
    {
      for (auto& diffuse_texture_pair : m_sceneLoaded->diffuseTextureMap)
      {
        auto& diffuse_texture = diffuse_texture_pair.second;
        ShowDiffuseTexture(diffuse_texture);
      }

      const bool browseButtonPressed = ImGui::Button("Add Texture");
      const char* chosen_path = dlg.chooseFileDialog(browseButtonPressed, nullptr, possible_image_extensions.c_str());

      if (strlen(chosen_path) > 0)
      {
        LoadDiffuseTexture(chosen_path);
      }
      else if (browseButtonPressed)
      {
        ImGui::Text("Invalid path");
      }
    }
  };

  auto ShowNormalTextureHeader = [&]()
  {
    if (ImGui::CollapsingHeader("Normal Textures"))
    {
      for (auto& normal_texture_pair : m_sceneLoaded->normalTextureMap)
      {
        auto& normal_texture = normal_texture_pair.second;
        ShowNormalTexture(normal_texture);
      }

      const bool browseButtonPressed = ImGui::Button("Add Texture");
      const char* chosen_path = dlg.chooseFileDialog(browseButtonPressed, nullptr, possible_image_extensions.c_str());

      if (strlen(chosen_path) > 0)
      {
        LoadNormalTexture(chosen_path);
      }
      else if (browseButtonPressed)
      {
        ImGui::Text("Invalid path");
      }
    }
  };

  auto ShowGLTFHeader = [&]()
  {
    if (ImGui::CollapsingHeader("GLTF"))
    {
      const bool browseButtonPressed = ImGui::Button("Upload GLTF file");
      static ImGuiFs::Dialog dlg; // one per dialog (and must be static)
      const char* chosen_path = dlg.chooseFileDialog(browseButtonPressed, nullptr, ".gltf");

      if (strlen(chosen_path) > 0)
      {
        m_sceneLoaded->ParseGLTF(chosen_path, false);
        rebuild_scene = true;
      }
      else if (browseButtonPressed)
      {
        ImGui::Text("Invalid path");
      }
    }
  };

  auto SaveSceneToDiskHeader = [&]()
  {
    if (ImGui::CollapsingHeader("Save/Load Scene File"))
    {
      {
        bool save_button_pressed = ImGui::Button("Save scene");
        static ImGuiFs::Dialog dlg; // one per dialog (and must be static)
        const char* save_path = dlg.saveFileDialog(save_button_pressed, nullptr, "scene.txt");
        if (strlen(save_path) > 0)
        {
          SerializeToTxt(save_path);
        }
        else if (save_button_pressed)
        {
          ImGui::Text("Invalid path");
        }
      }

      {
        bool load_button_pressed = ImGui::Button("Load scene");
        static ImGuiFs::Dialog dlg; // one per dialog (and must be static)
        const char* load_path = dlg.chooseFileDialog(load_button_pressed, nullptr, ".txt");
        if (strlen(load_path) > 0)
        {
          p_sceneFileName = load_path;
          //load scene, basically restart
          rebuild_all_resources = true;
          rebuild_scene = true;
        }
        else if (load_button_pressed)
        {
          ImGui::Text("Invalid path");
        }
      }
    }
  };

  auto ImageFunctionsHeader = [&]()
  {
    if (ImGui::CollapsingHeader("Kewl RTX Image Comparison Thing"))
    {
      {
        bool save_button_pressed = ImGui::Button("Save image");
        static ImGuiFs::Dialog dlg1; // one per dialog (and must be static)
        const char* save_path = dlg1.saveFileDialog(save_button_pressed, nullptr, "scene.bmp", ".bmp");
        if (strlen(save_path) > 0)
        {
          save_image = true;
          save_image_path = save_path;
        }
        else if (save_button_pressed)
        {
          ImGui::Text("Invalid path");
        }
      }

      {
        const bool load_button_pressed = ImGui::Button("Load image for comparison");
        static ImGuiFs::Dialog dlg; // one per dialog (and must be static)
        const char* load_path = dlg.chooseFileDialog(load_button_pressed, nullptr, ".bmp");
        if (strlen(load_path) > 0)
        {
          load_split_image = true;
          image_splitter_position_updated = true;
          split_image_path = load_path;
        }
        else if (load_button_pressed)
        {
          ImGui::Text("Invalid path");
        }

        if (load_split_image)
        {
          ImGui::Checkbox("Auto Move Split Image Slider", &image_splitter_auto_move);

          if(image_splitter_auto_move)
          {
            ImGui::DragInt("Auto Move Split Image Slider Speed", &image_splitter_auto_move_speed, 1, 0, 10);
          }
          else
          {
            ImGui::SliderFloat("Split Image bar position", &image_position_split_percentage, 0.0f, 100.0f);
            image_position_split_percentage /= 100.0f;
            image_splitter_x = static_cast<int>(image_position_split_percentage * static_cast<float>(m_width));
            image_position_split_percentage *= 100.0f;
          }

          if(ImGui::Button("Set slider to be the middle"))
          {
            image_position_split_percentage = 50.0f;
            image_splitter_x = m_width / 2;
          }

          //clamp, cuz copytextureregion will be mad :(
          if(image_splitter_x <= 0)
          {
            image_splitter_x = 1;
          }
          else if(image_splitter_x >= m_width)
          {
            image_splitter_x = m_width;
          }

          if (ImGui::Button("Stop image comparison"))
          {
            load_split_image = false;
          }
        }
      }
    }
  };

  auto ShowHeaders = [&]()
  {
    ShowHelpHeader();
    ShowFeaturesHeader();
    ShowModelHeader();
    ShowMaterialHeader();
    ShowDiffuseTextureHeader();
    ShowNormalTextureHeader();
    ShowObjectHeader();
    ShowGLTFHeader();
    SaveSceneToDiskHeader();
    ImageFunctionsHeader();
  };

  auto commandList = m_deviceResources->GetCommandList();
  ImGui_ImplDX12_NewFrame(commandList);
  
  //make sure to reset the heap descriptor
  current_imgui_heap_descriptor = 0;

  bool resize = true;
  ImGui::Begin("DXR Path Tracer", &resize, ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::Text("ImGUI version: (%s)", IMGUI_VERSION);

  ShowHeaders();

  ImGui::End();
  //bool a = true;
  //ImGui::ShowDemoWindow(&a);
}

void D3D12RaytracingSimpleLighting::InitImGUI()
{
  auto device = m_deviceResources->GetD3DDevice();
  // #IMGUI Setup ImGui binding
  {
    {
      D3D12_DESCRIPTOR_HEAP_DESC desc = {};
      desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
      desc.NumDescriptors = HEAP_DESCRIPTOR_SIZE;
      desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
      ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)));
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    ImGui_ImplDX12_Init(Win32Application::GetHwnd(), FrameCount, device, DXGI_FORMAT_R8G8B8A8_UNORM,
      g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
      g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

    // Setup style
    ImGui::StyleColorsDark();
    ImGui_ImplDX12_CreateDeviceObjects();
  }
}


void D3D12RaytracingSimpleLighting::RenderImGUI()
{
  auto commandList = m_deviceResources->GetCommandList();

  std::vector<ID3D12DescriptorHeap*> heaps = { g_pd3dSrvDescHeap.Get() };
  commandList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData());
}

void D3D12RaytracingSimpleLighting::ShutdownImGUI()
{
  g_pd3dSrvDescHeap.Reset();
  ImGui_ImplDX12_Shutdown();
  ImGui::DestroyContext();
}

void D3D12RaytracingSimpleLighting::RebuildScene()
{
  // Create raytracing interfaces: raytracing device and commandlist.
  m_raytracingGlobalRootSignature.Reset();
  m_raytracingLocalRootSignature.Reset();

  m_descriptorsAllocated = 0;
  m_raytracingOutputResourceUAVDescriptorHeapIndex = UINT_MAX;
  m_indexBuffer.resource.Reset();
  m_vertexBuffer.resource.Reset();
  m_textureBuffer.resource.Reset();
  m_normalTextureBuffer.resource.Reset();
  m_perFrameConstants.Reset();
  m_rayGenShaderTable.Reset();
  m_missShaderTable.Reset();
  m_hitGroupShaderTable.Reset();

  m_bottomLevelAccelerationStructure.Reset();
  m_topLevelAccelerationStructure.Reset();

  m_camChanged = true;

  if (rebuild_all_resources)
  {
    free(m_sceneLoaded);
    m_sceneLoaded = new Scene(p_sceneFileName, this); // this will load everything in the argument text file
    rebuild_all_resources = false;
  }

  for (auto& model : m_sceneLoaded->modelMap)
  {
    model.second.is_gpu_ptr_allocated = false;
    model.second.is_m_bottomLevelAccelerationStructure_allocated = false;
    model.second.is_scratchResource_allocated = false;
    model.second.bottom_level_build_desc_allocated = false;
    model.second.m_bottomLevelAccelerationStructure.Reset();
    model.second.scratchResource.Reset();
  }

  m_sceneLoaded->top_level_build_desc_allocated = false;
  m_sceneLoaded->top_level_prebuild_info_allocated = false;
  m_sceneLoaded->scratchResource.Reset();
  m_sceneLoaded->m_topLevelAccelerationStructure.Reset();
  m_sceneLoaded->instanceDescs.Reset();

  for (auto& object : m_sceneLoaded->objects)
  {
    object.transformBuilt = false;
  }

  // Create root signatures for the shaders.
  CreateRootSignatures();

  // Create a raytracing pipeline state object which defines the binding of shaders, state and resources to be used during raytracing.
  CreateRaytracingPipelineStateObject();

  // Create a heap for descriptors.
  CreateDescriptorHeap();

  // Build geometry to be used in the sample.
  m_sceneLoaded->AllocateResourcesInDescriptorHeap();

  // Build raytracing acceleration structures from the generated geometry.
  BuildAccelerationStructures();

  // Create constant buffers for the geometry and the scene.
  CreateConstantBuffers();

  // Build shader tables, which define shaders and their local root arguments.
  BuildShaderTables();

  // Create an output 2D texture to store the raytracing result to.
  CreateRaytracingOutputResource();
}

bool D3D12RaytracingSimpleLighting::LoadModel(std::string model_path) {
  std::fstream model_file(model_path);
  if (model_file)
  {
    ModelLoading::Model model;
    model.name = model_path;
    int new_id = (--std::end(m_sceneLoaded->modelMap))->first + 1;
    m_sceneLoaded->LoadModelHelper(model_path, new_id, model);
    rebuild_scene = true;
  }
  return false;
}

bool D3D12RaytracingSimpleLighting::LoadDiffuseTexture(std::string diffuse_texture_path)
{
  std::fstream file(diffuse_texture_path);
  if (file)
  {
    file.close();
    ModelLoading::Texture new_texture;
    new_texture.name = diffuse_texture_path;
    int new_id = (--std::end(m_sceneLoaded->diffuseTextureMap))->first + 1;
    m_sceneLoaded->LoadDiffuseTextureHelper(diffuse_texture_path, new_id, new_texture);
    rebuild_scene = true;
  }
  return false;
}

bool D3D12RaytracingSimpleLighting::LoadNormalTexture(std::string normal_texture_path)
{
  std::fstream file(normal_texture_path);
  if (file)
  {
    file.close();
    ModelLoading::Texture new_texture;
    new_texture.name = normal_texture_path;
    int new_id = (--std::end(m_sceneLoaded->normalTextureMap))->first + 1;
    m_sceneLoaded->LoadNormalTextureHelper(normal_texture_path, new_id, new_texture);
    rebuild_scene = true;
  }
  return false;
}

bool D3D12RaytracingSimpleLighting::MakeEmptyMaterial()
{
  ModelLoading::MaterialResource material_resource{};
  int new_id = (--std::end(m_sceneLoaded->materialMap))->first + 1;
  material_resource.id = new_id;
  material_resource.name = "Empty Material";
  m_sceneLoaded->materialMap.insert({ new_id, std::move(material_resource)});
  rebuild_scene = true;
  return true;
}

bool D3D12RaytracingSimpleLighting::MakeEmptyObject()
{
  ModelLoading::SceneObject object{};
  object.id = m_sceneLoaded->objects.size();
  object.name = "Empty Object";
  object.scale = glm::vec3(1.0f);
  m_sceneLoaded->objects.emplace_back(std::move(object));
  rebuild_scene = true;
  return true;
}

void D3D12RaytracingSimpleLighting::SerializeToTxt(std::string path)
{
  std::string ma_boi_pat =
"################################################################+++++++++++++###+#################################################\r\n"
"################################################################++++++++++++++####################################################\r\n"
"#################################################################++++++++++#+#####################################################\r\n"
"#################################################################+++++#+##########################################################\r\n"
"################################################################+#++++############################################################\r\n"
"###############################@##################################################################################################\r\n"
"############################@@@##@@@@###########################################################################################@#\r\n"
"#####################@@@@@@@@@@@@@@@@@#########################################################################################@@#\r\n"
"###################@@@@@@@@@@@@@@@@###############################################################################################\r\n"
"################@@@@@@@@@@@@@@@@@@@###############################################################################################\r\n"
"##############@@@#@@@@@@@@@@@@@@@@@################################################@@@@##################+#+++####################\r\n"
"########@##@@@@@@@@@@@@@@@@@@@@@@@@@##########################################@#####@##################+#++++#####################\r\n"
"##@##@#####@@@@@@@@@@@@@@@@@@@@@@########################################@@##@@####@@################+++++++######################\r\n"
"@@@@#@@#@#@@@@@@@@@@@@@@@@@@@@@@############################################@#@@########@#############+++++++#####################\r\n"
"#####@@@@@@@@@@@@@@@@@@@@@@@##@###########################################@##@######################+++++++++#####################\r\n"
"######@@@@@@@@@@@@@@@@@@@@@#@@##################################@#########@########################++++++++++#####################\r\n"
"#######@@@@@@@@@@@@######################################@@@########@@#########################+#++++++++++#+#####################\r\n"
"########@@@@@@@@@@#########################################@@@#@##@#@####################+#####++++++++++++++#####################\r\n"
"#######@@@@@@@@@@@#@##################################@#@@##+##########################++###++++++++++++++++++####################\r\n"
"######@@@@@@@@@@@#@#############################++'+;''.::,'';'''#'++#############+++++++++++++++++++++++++++#####################\r\n"
"#####@@@@@@@@#@@################################;:..;:.'.,:+:'';'''+'+'++####+##+++++++++++++++++++++++++++++#####################\r\n"
"######@@@@@#@#@##############################+'+.:,.:,,::,;;`:::++:;'',';#++#+##++++++++++++++++++++++++++++######################\r\n"
"##@###@@@@@#@@#@#########################+##+;::..::::'+;::',':''++''+;;;,':++#++++++++++++++++++++++++++++++#####################\r\n"
"######@@@##@#@##@@######################+#+,:,;':,',+:;':''';;''':';#+;';,,,;:+++++++++++++++++++++++++++++++#####################\r\n"
"#@##@#@@@@@#@@#@#@#####################+#',,';,',;':;.;;'':+;'';'++++++';;;,,,;'+'++++++++++++++++++++++++++######################\r\n"
"#####@@@#@#@#@@@@####################++;:.,;:'',::;;:::;;;'+::;:;;;''++++;;:'.,:+++++++++++++++++++++++++++#######################\r\n"
"###@@@@#@@@#@@#@@####################+;:.:,.;+',,,:,.::,,:::::;:::;:;''''++::;:.::+++++###+++++++++++++++#########################\r\n"
"###@#@###########@#@@##############+#+,,:.:,::.,':;;':;;;;,,:,:,,:,,::;'++;'';:,,;:#+#+#+#+++++++++++++###########################\r\n"
"##@####@@@@#####@@@@@@#############+;,.,,+,;',':;;';;;':::::,,,..,,,,,,:'++';;:,.,:;++++#####+####++++############################\r\n"
"##@##########@##@###@####@#########+;,,''+;::+:,;+':':;;;,:,,,.,.,,.,,,,:;'++'::.:,:++++###########+++############################\r\n"
"########@####@###########@@########+,:::;::;';'+''''';'::,,.,:..,,,.,,,,,:+++';;+';;;#+###########################################\r\n"
"#######################@##@######+,.:';;+';;';;;;+''';;,,..,,,,.,...,.,,,,,'+++'';:,;+############################################\r\n"
"################################+;,;'';;:'';'+';''''';:,,......,,,,,,,,,,,,:'++''':':'############################################\r\n"
"################@###@#######@##'+:.;;':;;:;'+;;;;;:;;:,,........,,,,,,,,,,,,:'##+#++++'###########################################\r\n"
"################@###@@########@;,,,++'';;;;'';::;:::::,.........,,,,,,,,,,,,:;+###+#+'+###########################################\r\n"
"#####################@#@#@##@##;::.'';;;;::;;:::,:::,,,,.....,,.,,,,,,,,,,,,:;'#######+###########################################\r\n"
"#####################@@##@@@##;.:;''':::::::::,,:,:,,,,,.....,,,,,,,,,,::::::;'+##@###+#############################@#############\r\n"
"###########################@#+:`;:'#:.,,,:::::,,,,:,,,,,,...,,,,,,,,,,::::::::;++#@@@@'+#############################@############\r\n"
"#####################@@@###@##..,:',:,:,:::,::,::,::,,,,,,,,,,,,,,,:,::::::::;;'+#@@@################################@############\r\n"
"#####################@@@@@@@@:,,:++;,:,::::::,::,:::,,,,,,,,,,,,,,,::::::::::;;'+##@@@@+##########################################\r\n"
"######################@###@#+``:+++;::::::::::,::,::,,,,,,,,,,::::::::::::::::;'+##@@@@+##########################################\r\n"
"#########################@@@+`,;+++;;;;;;:::::::,::::,,,,,,,,::::::::::::::::;;;'+#@@@@###########################@#@#############\r\n"
"#########################@@#',:'##'';;;;:;:::::::::::,,,,,::::::::::::::::::::;;'+##@@############################@@@@############\r\n"
"########################@###.:''+@';';;;;;:::::::::::,,::,::::;:;;:::::::::::::;;'+##@@#+###++##################@##@#@@#@@########\r\n"
"###########################+@:;+@#'''';;;;;:;;::;::::::::::::::;:;:::::::::::::;;;'#####++#+,+###################@#@##@@###@@#####\r\n"
"############################+:'+##''''';;;;;;:;:;::::::::::::::;:::::::,:::::::;;;;+##@@#+ ::,:###############@####@@@##@##@#@####\r\n"
"###########################;:;'##+'''''';;;;;;;;::;:;::;;::::;;:::::,,::::::::::;:;'+####;:;;+;+###################@@##@#@@@@#####\r\n"
"############################+'#@@#''''''';;;;;;;;;;;;;::::;::;::::,,::,:::::::;;:::;+####;:;+'+:#@#########@@@########@@#@@@#@@@@#\r\n"
"############################++#@##'''+''''';;;;;;;;;:;:::;;::;:::,,,:,:,:,:::::;;::;+@#+';'++'''#########@####@##@#@###@@@@@@@@@@@\r\n"
"###########################;''+#@#+'''''''''';''';';;;;:;;;;:::::,::,:,::::::::;;;:;;+@';;++''';###################@#######@@#@@@@\r\n"
"###########################@@'##@#+++'+'''''';''';;;;:;;;;:;:::::,,,::;;;;;:;;;;:;:;#@++''+'''';'#@##@@@################@##@@@@@##\r\n"
"###########################+'+##@#+++''''''''';';';;;;;;;:;:::::,,,::;';'++'''';;;':@;'++++;;';;:@@####@#@###################@@@##\r\n"
"###########################+;+#@@@#+++''''''';';;;;;:;:::;;::,::,:;;''+++'++''++';++#;:+++';;;;;;@@#@##@####################@@@@##\r\n"
"###########################@@;@#@@#++++''''''';;;;;;:::::::::::::;'+++#+''''':#+#:##';:'+#+;;;;;;+##@@####################@##@####\r\n"
"###########################;++#@@@##++++'+''';;;;:;:::::::::;::;';++#+#'+#'+##+#'#@#';;;'#+'';;''@#@#@############################\r\n"
"##########################@#+#+@@@###++''''';;;;:;::::::::::;;:;''++#+#'+#@####++@'''';;;'+''';';@@###############################\r\n"
"##############################+@@@@###++''''';;;;;;::;:;::;:;;;''+++#;+#####@##+++''';;;;';;;';;;@################################\r\n"
"#############################@@#@@@##+++'''''';;';;;;;;;;;;;;;'''+##++@@@#+#++''';'';;';;;;;;;;;;#################################\r\n"
"############################@###@@@@##+++''';;''';;'''';;;;;;'''+#+##+#+##+'++';';;;;;;;;;;;;;;:'#################################\r\n"
"#################################@@##+++++'''''+##+++++'+''''';+####++'+';+''+;;::;;;;';';;;':;:+#################################\r\n"
"#################################@@@#+#++++'++####@####+'+''';;++####+++''''++;::+';;;;'';;;:;::;+################################\r\n"
"############################@@##@+@@@#++++++##++#++####+;#''+;#+#+'''''++++++;:+:;;;;;;''';'::,::#################################\r\n"
"###################@###@@#@##@@###@@@#+++++#+#+++#@###@###@#':::;#@++++++++;:,'::;:;;;;''';';:,:+#################################\r\n"
"################@#@@@@@@@#@#@@@####@@#++++##+++##+@@@@+####+;;::;+#'+'+''';+':::::::;;';'';'',,;+#################################\r\n"
"################@#@@#@@@@@###@@#@''#@#++###@##+#@#@@@#++#+##;;::;'+#';';;#:':::::::;;;';'';'',:'##################################\r\n"
"############@###@#@@@#@@#@######'';+##@++++###@#@+#+##+''+##'::::;;''+';;;;;::::::;:;;'';;';;;;'##################################\r\n"
"##############@@@#@@@@@@####@###,#++#@#@+######@#++#+'+++++#+;:,:;;;;';;;::::::::;;;;;';';;'''+###################################\r\n"
"###########@@@@@@@@@@#@#########'@++##@@'+######++++#'+++++++;:,:::;;;;;;;;;:::;;;;;;;;';';;'+####################################\r\n"
"#########@#@@@@@@@@@@############'##+@@#@#####++#+++++++''@++':,,:,::;''';;;;;;;;;;'''';'';'++####################################\r\n"
"#######@@#@@@@@@@@@@@#@@#########+#####+####++++#++++++''##+';::,::::;;'''''''';'''''''''';;''@###################################\r\n"
"#####@#@@@@@@@@@@@@@@@##########@#++##@###+#'+''++++'+;@'++'';;::,:::;;;'''++'++''''''''+;';''####################################\r\n"
"######@@@@@@@@@@@@@@@#############'+++#####+''''++''#++'++++';;;:,:;';''';;''+#++'+'++'''''''':``````'####+#+++###################\r\n"
"#####@@@@@@@@@@@@@@@##############'''++####+#+++#+;+''''''++'';;::;'+++#;;;''+++#+++'''+''''''``,,,``,.``   ``..,:;+'+':#+########\r\n"
"####@#@@@@@@@@@@@@@@##############+''+++###++++'''';'';';'++++'''''+###'';;''++++#++''''''''+':.,,,:;;,,..,`,,,`````` ``.`,'######\r\n"
"######@@@@@@@@@@@@@@@###############@#+##++++++'''';';;;''+++++++++##++'';'''++++++''''''+''''::.:::;',,,.:':,;,,....,,...`````.;#\r\n"
"#######@@@@@@@@@@@@@################''+++#++++++''''';'''++###@#+++++''''''''++++++''''''''++';;:.:::;';.::;,`..;,:;,::':,```..`  \r\n"
"###@@@@@@@@@@@@@@@@###############+##+++#+##+++''''';;''+''#######+++'+';;''+++#+++''''''''++;';;..:,:;:`.,`,:::::,,,.:;;:``,;::':\r\n"
"######@@@@@@@@@@@@@@#@############++++++++++#+'+'''''''+''++++++##+++''''''''#@#''';'''++++++;:;;:,::,::;,.:::::::,,:::::,,,,,,:,:\r\n"
"#######@@@@@@@#@@@@#@###############++'+#++#+++''''''''+++++++##+++'+''';;'+#@#'''';''''+'++#'';;;::::,:::,::::;::,:::;;:::,,,,,::\r\n"
"######@#@##@@@#@@#@@@@###############++++#+++++++'+''++++++++++++++''';'++####''''''''''+++++''';;::;;;:';::;;::::,:::;:::,:,,:::,\r\n"
"########@#@@@@@@@@@@@###############++#++#++#+++++''++#++++++#+++++'''++##+#+'''''''''+++++#'++'';;:;:;;;';:;;::::::::::;,,:,,..` \r\n"
"###########@@@#@@###@###########+####++;'#++++#+++++++#++#++++++'++++++#++;++';'''''+'++++++';+';';:::;;;';:;;;:::::;:;:;:::`:::,:\r\n"
"############@#####@###################+;'+++++++++++'+++++++++++#+++###'''+++';'''''+''+++++''+;';;::;::;:;::;:;::::;::::`.`::::,,\r\n"
"#############@########################++'#+++#+++++++++++++#########+';++''+';;''''''+++++#++'+''';;;;:;:;;::;;;:::.`.``,:::;::::,\r\n"
"########################################,@###+++++++++++++#@@@#+##+''#''+++'''''';'''+++##+++'@'''';;:;;;;:';;;;;:`::::,::::::;;::\r\n"
"#########################################+'####++#+++''++++######++++'';'+'''''';;;'''+#+##+++@''';;;;:::;;;:';.`,;:;;:':::::;::::\r\n"
"##############################################++++#++'''++'+++#####+''''''''''';;;;''+++##+++''''';;;:;:::;:`,,';;;::;:;:;::;;;;::\r\n"
"############################################+###++#++''';'''++++##++++''++++''';;;;''++####+++'+';;;;::;:::;::;;;;;:;;:;:::;::;:;:\r\n"
"##############@###############################+#++#+++'''''''+'+''++++++++++'';;;'''+++###+++''#';;;;::;::;;,;'+';;;:;;:;';:;::;;;\r\n"
"###########################################+`,+##++#+++''''''+++++++++#+++++';;;;;''++####+++''#';;::;;:;:;;;;;'';:;::::';;:;;:::.\r\n"
"@@##########@#############################;.,;:#+#+#++++''''++++++++++++'''';;;;;''++#####++++'#';;:;;:'::;:;,;'';;;;;'::;:;;;,.`:\r\n"
"@########################################`,.:.;###++#++++++'+++++++++++';'';;;;;'''+#####+++++'+'';;:;;:;:;:;;;;';;;;;;;;::;.`:;;;\r\n"
"@#######################++#############@`,,;#.+#####++#+++++++'++++++''''':;'''''+++#####+++'++++';;;:::;::;;:;';;';;;;;;:.`:;:;;'\r\n"
"@@####################++###+##########+`.:;++:+##@#####+#++++++++++'''';;;;;';;''++#####++++++++#;;;;;:;:;:;:;:;;''':;;:,,;;;;;;;;\r\n"
"#####################++++++++########+`.,;;'';###@@@######+++''++++''';;;;'';'+'++######++++''+++'';;:;;:;:::;:;'''.....;;;;;;;';;\r\n"
"#####################++++++++#######;`,:;';'';+###@@########+++'++'++''''++''++++######+#+++''+++';;;';';;:;;:':;'.;;;:''':';;;'';\r\n"
"#################+##++++++++#+####+:`,.,+'';''+####@@#########++++'++++''++++++#######+#++++'''++;':;;;;;:;;;:;:;,;;;';;;;';';;;;:\r\n"
"#################++#+++++++++#####,.,::;'++':'+####@@@#########+++++++++++++############+++''++++'';;:;;;;:::::::';'';;';'';+;';;;\r\n"
"###############++++++++++++++++##.`.,:.;;+,:;++####@@@###########+++++'++++############+++''+++++';;;;:;;,;:::;';';'+';;';;;'''';'\r\n"
"#############+#+++++++++++++++#+ ``.:'.;;,'+''+#+###@@@###########++++################++++'+'++++''';:':';:::;;;'''';;::;';';;'';;\r\n"
"########+++++++++++++++++++++++` `,;+;,'''++'+++####@@@##############################+#+++''+++++''+';;;;;::;;;''''';';;;;;'''';';\r\n"
"###+#+++++++++++++++++++'++'+'``..:':.;'''++++++#+###@@@#############################++++'''++++;''';;;,,;::;;';''';;';;;;';;;'';.\r\n"
"#++++++++++++++++++++++++++''`...::,,;';'''+++++++####@#@#############################++''''++++++;;::';:;;:';;;'+';;''';;'''',..;\r\n"
"+++++++++++++++++++++++''+';`..,:.,;::'''''++'++++####@@@@###########################++++'+++++++'';;;;::;;;;''''''';'''''''..,'''\r\n"
"+++++++++++++++++++++''+'''```.:.;;:;;''';'++'+++++###@##@##########################+++++++++'++'+''::::::;:;'''++';''';';`.;''';'\r\n"
"++++++++'+++++++'+'+''''''`` .':,:::;;;''''++'++++++#####@#########################++++'++++++++'+''':;;:;';;'''#'';'';,.,';;'''''\r\n"
"+++++++'+'''+'+'''''''''+```.;::;:;;';;+''''+'++++++#####@@#####+###############+#++++++++++++++''''';::;';;'''''''':,.:;''''';'';\r\n"
"+++++++'''''''''''''''''.``.;::::;;;';'';''+'+++++++######@#####################++++++++++++++++'+';;;;;;;;:;+'++',,,;;;;;;;''+';'\r\n"
"+++++''''''''''''''''';..`,;,;;:;;;;;'';';'',:++++++++#####@###++#+#+#######+##++++++++++++++++'++'':;;':'';'''',:'''''';;'''''+;'\r\n"
"++++'''''''''''''''''':``,;::;::;;';''',,'':+++++'+'++#########+#+++#+####+++++++++++++++++++++''''''';;;''''''+++';:';';';+';';''\r\n"
"+++++'''''''''''''''';```:,::;;;;;;;;',:'+;+++++'++++#++####@####++++++++#+++++#+++++++++''+++++#+'';;;;';'''##+''';'''''''+''''''\r\n"
"+++''''''''''''''''';```:,,::::;;;;;,.;'''+'+'#+'++++++++#########+++++++#++++++++++++++++''+++';''';;:';''#@##+''+'';''''''''''''\r\n"
"+'+''''''''''''''''': `.;::::;;;;:..;;''''+'++++++++++++++#########+##+#+#++++++++++++++'+'+'+#';''';''''#@@@#+'++'';''''''';';'+;\r\n"
"'+'''''''''''''''';;`.,;::::::;..:';;'++'+'''++++++'+'++#++#++#######++++#++#++++++++++++'++'++';''';'+@######+++'+'''''''++''''+;\r\n"
"+''''''''''''''''';.` `::::''.`:;;'''''''''+'++#++++++#++++#++######+++++#++++++++++++++++++'+'+':'++@@##++###++'''''';'''''';'''.\r\n"
"'''''''''''''''''';` ,:::;;..;:';;;'''''+'''''++++++++++++#+++++#####+++#++++++++++++++++'+++++';;'#@@@#++++++'++''''''''''''',,,'\r\n"
"''''''''''''''''''``,;;:;:.::':;;;;;';''''+++++++++++'+++++++++++++#####++++++++#++++++'++++++'''+;@@@###+++++++'''''''''';',,:'''\r\n"
"'''''''''''''''';:`.',::.,;:;;;;:;;;;;;'''+'''++#++'+'++'+++++++++++++#+#++++++++++++++'+#+'+:++':;@###+++++++++'''+''''',..;'''''\r\n"
"''''''''''''''''; `,,::`:;::;;:;';;;;''';'++'':':'+'+''++'+++#++'++++++++++++++++++++++++++++,++';;@###+++++++++''++'',,,'++''+''+\r\n"
"'''''''''''''''',..:::`:;;;;;;;;;;;;'''';'';,:'++++++++'+'++++++++'++++++++++++++++++'+++'++';;''+#####++++''++''';,,.'''''''''''+\r\n"
"'''''''''''''';;`,;::`::;:;;:;;;;';''';';,.;+''+#+#++''+'+++++++++'+'+'++++++++++++++''++'+''','+#@####+++++'''::,:;''+;';+;'''++'\r\n"
"'''''''''''''';.`:,,.:::;;;;;;;;';';;;.,:';+''++++++++'+'+++++++++++++++++++++++++++++++'+''+':#+@@##+#+''+:,,:''''''''''''';+'++'\r\n"
"'''''''''''''''`,;,.::;;::;;;:;;;;;..,''''''''+++++#+++++++++++++++++++++++++++++#+++++++''';',+@@@###+':,:;'+''''+'+''''+++'''+++\r\n"
"''''''''''''';..':,:::;;;;;;;:;:;`.;;'''+;+''+''+++#++++'+''++++'+++++++++++++++++#+++++++'+'':+@@##++#,;+''''+'''+'''''''';'''''+\r\n"
"''''''''''''';.,:.::;;;;;;;;;;;..';+;';;'''+'+'''+++#+++++++++++'++++'+++++'+++++#+++++++++';''#####':'++'++''+''+'+'+'''''+'+'++'\r\n"
"'''''''''''';`.;.:;;;;:;'';';..;';';;'''''''+'''++++#+++''+'+++++++'++'+++++++####++++++++'++@#@#';:+#+'+'+'''''+'+'+''''++''+''+'\r\n"
"'''''''''''';.,.;:;;;;;'';;:,;;''+'';'''';'+''+++++++#++++++++''+'++@#+++++++##++###'#++''++@@#+'+#++#+++'+++''+'+'+''''+'''''''''\r\n"
"'''''''''''; ,.:::;:;;;';'..;''';+';''''+'''++'''++++##+++++''+##@@@###++#++++''+###+###++'@######++++++''+++++'+'+++''''++''''',,\r\n"
"'''''''''';`.::::;;;;;;;'.;'';'''''''';''''''''+#'''::+#++++#@@#@@@@@#####++++++''+##++++'++@@###++++++''++'+'''+++++''++++'+':,;+\r\n";
  auto ma_boi_shehzan =
",,,:,:;;:::::::+';+;;;;;;;;';;;;;;';;;;;';;;;;;;;;;;;;;;;;;;;;;;;;;;;':'+++++#+++##############+++++++++'''++';'';;:,,:::  `  `   .` .,,;'::``,,,..```:;;;\r\n"
",:::,:;;:;;;:::+';+;;;:;;;'';;;;;;;;'';';';';;;;;;;;;;;;;;;;;;;;;;;;:;:'++++#+++##################+++'+++++++'+''';;;;;:,`  ` .  `,  `,,;:::,::;:,,```;';;\r\n"
",,,,,:'';;;;:::#':+;;;;;;;'';;;;;;';;;';;;;;';;;;;;;;;;;;;;;;;;;;;;;;;:'+++++'+#############++########+++++++++++'''';;';': ` .   `  ..:,';;::,:,:,`.,;';'\r\n"
",,,,,::::;;;:::#;:';;;;;;;'';;;;;;;;;;;'';''';';;;;;;;;;;;:;;;;;;;;;;':;+'+'+#+#############+###++#####+#+++++++++'''''';;''':.   .` ..:,;'':;,;:;,:;:;'';\r\n"
",,,:,::::::::::';:';;;;;;;'';;;;;;;;;;;';;';';;';;;;;;;;;;;;;:;;;;;;:` ``.:.,`` `:'+#+#################+#####+++++++'''''''';;;':..` ..:;,';,;.;'':;:;;;':\r\n"
",,,:,,::::::::::::';;';;;;'';;;;;;;;;''';''';;;';;;;;;;;;;;;;;;;;:` `....`...,......`'#######++#######++#+########+''''''''''''';;;;:..:;,;;,;':';:;:;;;':\r\n"
":,,::::::::::;;;;:';;;;;;;;';;;';;;;''';;''';;;;;;;;;;;;;:;::;;: ``.,:,,.,,,,.,.,,,,.. +++++##########+##########+#+++''''''''''''';;';;;',:,;;,;;:;;;;;+:\r\n"
":,,:::;::::::,:;;:';;;;;;;'';;;;;;;;;;;';';;';;;;;;;;;;;;;;;:;``..,:,,:,.,;,,,,,:,,,,:..`++++++#####+###########+++++++++''''''''''''';;;;;;,;'',':'';;;''\r\n"
"::::,::::::::::;;:';;;;;;;'';;;;;'';;;'';;;;;;;;;;;;;;;;;;;;: .,,,:,::,,:.:,.,;,,,:,,,,,..+++++######+#########+++++++++++++++'''''''''''';'';;;:;:'';;;;:\r\n"
":::::::,:::::,:;;;';;;;;;;'';';;;;;;;;;;''';;;;;;;:;;;;;;;;,..,,,:,,,:,,,,;;;:.,,,,:.,.:.,.+++++#++++++++########+++++++#+++++++''''''''''''';;'';:'''';''\r\n"
":::::::::::;:::;;:';;;;;;;''';;;;;;;;;;;;;;;;;;;;;;;;;;;;;:`,,,:,::::,,::,:;;:::,,:,.::,::,.++++#++++++++########+++++++++++++++++++'''''''''''';;;';;''+'\r\n"
"::::::,::::::::;::';;;;'''++;;;;;;;;;;;;;'''';;;;;;;;;;;;;`.,:,::,,,,,,,.:,,:;;:,:,:,,:::::.;+++++++++++++##+++##+++++++++++++++++++++++'''''++''''';';;;;\r\n"
":::::::::::;::::;:';;;;;;;;';;;;;;;;;;;';;;;;;;;;;;;;;;:;.,,,,,,,,,..,.,.,;;;;::::.,,:.::,:.,++###++++++++++++++++++++++++++++++++++++++++++''''''''''';;;\r\n"
"::::::::::::;::;;:';;;;;;;;';;;;;;;;;;;;;';;;;;;;;;';:;;:...,,,,,`,,..,.,`:::::';;:,...,:,:,:,++#+++#+++++++++++++++++++++++++++++++++++++++++++''''''''''\r\n"
":::::::::::::,;;;;';;;;;;;;;;;;;;;;;;;;;'';;;;;;;;;;;;;;...,....`...``..,,;';;:::'::,,....,,,.';'+'''+++#+#+++++++++++++++++++++++++++++++++++++++++'''+++\r\n"
"::::::::::;;;,:;;;';;;;;;;;';;;;;;;;;;;'';;;:;;;;;;;;;;;.....`..`.`..`.,,:::;::;;:::,:......,,:';;;'''+''#++##+++++++++++++++++++++++++++++++++#++++#++'''\r\n"
";::::::::::;;:;;;;';;;;;;;;;;;;;;;;'';;';'::;;;;';;;;;::..``....```.,.,.::;:;:;;,:::;:,........+'++'';;;'+'++''++++++++++++++++++++++++++++++++++++++###++\r\n"
":::::::::::;;:;;;;';;;;;';;;;;;;;;;;'''';;:;;:,,,::,,,,,..`..``.``..`:.;;:';;,:'::::;.;:,.,...,+++'++''''';;;'''''+++++++++++++++++++++++++++++++++++##++#\r\n"
";::::::;::::;:;;;;';;;;;;;;;;;;;;;;;::,,,,,:,,,:;;;:;:;`...``. ..``.`.;,:;:,:,,:;'::,::';.,`,.,.''''''''++++';;;;;'''''+++++++++++++++++++++++++++++###+++\r\n"
":::::::;::;;;,;;;;';;;;;;;;;::::,,,:::::;::::::::;;;;;;.....``.`.,`,,,.:;;';;':';:,:::';;.:.,.,.';;'+';;++''''''';;;;;''''++++++++++++++++++++++++++#+++++\r\n"
":::::::;;::::,:;;;';;;;;:::::,,::::;:;;:;;;;;;;;;;;;;;:...``.`````.:.,::,.,.,;''++++;';:;::,;,,.';;;';;;''';'''''+'''';;;;;'''''++++++++++++++++++##++++##\r\n"
"::::::;;::;;:::;;;'::;:;:::::::::;;;;;;;;;;;:;;;;;;;;;..`..``...,..,:'+#@####@@@@@########:::,,,,;;;';;;''';;+';;'''++++'';';;;;''++++++++++++++++++++++##\r\n"
":::::;;;::;;:,::;;':;;;;;;;;;;;;;;;;;;;;;:::;:;;;;;;;;.```.`......'##########################:,,,;;;';;;''';;'';;;'''+++++++'''';;;;;;''++++++++++++++++++\r\n"
";:::;;;;;;;::::;;;';;;;;;;;;;;';;;;;;;;;;;;;;;;;;;;;;;`.....`.,.+###########@@@@@@@@@@@@######+,,;;;;';;;;;;;;;;:;'';''''+''''++++''';;;;'+'++++++++++++++\r\n"
";:;;:;;';::;;::;;;';;;;;;;;;;;;;;;;;;;;;;;;;;;;;::;;;;....,,..+########@@@@########@@@@@@@@####@,:;;:;;;;;;;;;;;;;';;'''+'++''++'+''''''';;;''''++++++++++\r\n"
";::::;;;;;;;;::;;;';;;;;;;;;;;;;;';;;;;;;;;;;;;;;;:;;:...,,.+++####@##########@@#@#@##@@#@@@@####,++':;;;;;;;;;;;:;:;''''+++++'+'''''''''++'';;;;'''++++++\r\n"
";::;::;;;;:;;:;;;;';;;';;;;;;;;;;;;;;;;;;;;;;:;:::;;::...,'#++##########@########@@@@@@@@@#######+++#:';;;;;';:;;:;:;''';:;+++'';'';;''''+'''''';;;;';''++\r\n"
":::;:;';;;:;;;:;;'';;;';;;;;;;;;;;;;;;;;;;;;;:;:::;;;:...'#+###+###@#######################@@####++++::;;;;;++:;::;:;''';;;;+''';''';;';;'''''''+++'';;;;;\r\n"
";::;:;;;;;;;:;:;;'';;';;;;;;'';;;;;;;;;;;;;;;:;;;;;:;..:+++##+##@####@###+###+'''''++#######@#####+++::;;';;++;:;:;:;''';;':++'';;;;;;;;;'';;'';'''''+'';;\r\n"
":::;::;;;;;;:::;''';;;;;;;'';;;''';;;;;;;;;;;;;;;;;;:.,#+#@+########++. ````` ``````` ````;++######++::;;;;;+'',;;;;;'';:;':'++';;;;;;';;';;;';;;''''''+++\r\n"
";::::;;;::;;:;:;;'';;;;';;;';;;;;;;;;;;:;;;;:;;::::::`++##+###+#''` ```` `````````````````` ``#+####+::;;;;:+'',;;';;'';:;;:''''';;;;;;;;';;;';;'+';'';'''\r\n"
":::::;;;:;;:;:;;;'';;;;;;;;;;;;;;;;;;;;;;;;;:;:::::,:;+#++###+'``   ```` `    ````````````````` '###+::;;;;;+'',;;;;;'';:;;:''''';;;;;:;;';;;';;;';;';;'''\r\n"
":::::;;;;:;:;;:;;'';;;;;';';;;;;;;;;;;;;;;;::;:::::::+#+##++;  ` `       `````..,,,:::::::,,.`````.'':,::;;;''',;:;;;'';:;;:'''+';;;;':;;';;;;';;;;;';;'+'\r\n"
":::::;;;::;:;;:;;';;;;;;';;;;;;;;;;;;;;:;;;::;;::,:::+###+;  `  `  `.,:;;';'+##################+'';;;;;;;;;:,'',;:;;;'';:::,;'''';;;;;::;';;;;;;;';;;';;';\r\n"
";:::::;;;::::;;;'';;';'';;;;;;;;;;;;;;;;;;;:::;;::::;##+'     .,:'+##+####++#####################@#'';;;;;;;::',;:;;;'';;;;:;'''';::;;;:;';;:':;;';;;;;;';\r\n"
"::::::;;:::::;:;;';;;;;;;.`.;;;;;;;;;;;;;;;;;:::::::;;,`.,,;+#@##########+++#+#######+###########@##+';;;;;;;';,;:;:;'';;;;:;'''';;:;;;:;';;:+:;;';;':';';\r\n"
"::;::;;;;;;::;:;;';;;;';`````:;;;;;;;;;;;;;;:.`.,,::::;'#@@@@@##############++++++++++++##########@@+'';;;;:;':,;:;:;'';;;':;'''';;:;;;,;;;;:':;;;;'+:';';\r\n"
"::::::;;;;;;;;:;;';,,:;.....,.;;;;;;;;;;;;;;`:::::;;;;'#@#@@@##################++++++++####@@@@@##@@#+'';;;:;':,;:;:;'''';':;'''';;:;;;,;;;;:':;;;;'':;;';\r\n"
"::::::;;;;;;;::;;';,,,:.,...`.;;;;;;;;;;;;;::::::;;;;'+@@@@@##++####@#@@@@@#####++++++##@@@@@@#####@#+'';;;:;',,;:;:;'''''':;'''';;;;;;:;;;;:':';;;'':;'';\r\n"
",,,,::;;;;;;:''';';,,,,,...,...;;;;;;;;;;;;;:::::;;;''@@@@@##+++##@@###@@#@#@###++++++##@@@@#####+#@#+'';;;:;':,;;;:;;''::;:;'';';;;;;;:;;;::':;;;;;':;;';\r\n"
",,,,::;;;;;;;;'''+;,,,,,.,,,...;;;;;;;;;;;::;::::;;;'+@@@@@#++++#############@###++++##@@@@@###++++@#+';;;;:;':,;:;:;;'';::;;';;';;;;'::;;;::::;;;;'':;;';\r\n"
",,,,:::;:;;:;;+;;+:,,,:,,,..,,:;;;;;;;;;;;;:;:::;;;;'@@@@@@#+++++++++####@#@####++++###@@@@@#@##++'@@+';;;;:;';,;:;:;;'''''';;''';;;;'::;;;;'':;;;;;':;;';\r\n"
",,,,,,::::;;;;++;;:,,,:;,.,,,,;;;;::::::::;;'+;::;;'+#@@@@@#+++++++###@@@######++++#####@@@@##@##+'##+:;:;;:;':,;:;:;''''''''';'';;;;':,;;;:'':;;;;+':;;';\r\n"
"....,,::::::;;'++;:,,,:;:;:,,:+'++'';';;:::::;;::;;+##@@@@##++++++##@+##@#######+++###@@#@@@#####+'#+#+;:;;:;;:,;:;:;'''++';''''';;;;':,;;;:;':;;;;'':;;';\r\n"
"''''';;:,..,,,:;++''+++';,.;'++++;:::;;;;;';;'';:;'''++#@@@++++++######@#########+++##############'+++#;;;;:,,,,;;;:;;''+';'''+'';;;;':,;;;;;':;;;;'':;;';\r\n"
"::;;;;;;;''''''';:,,,,;;:,,;:+#'+;::::::::;:;;;:;:'+'''+#@@+++++++++++++#######++++++#############'+++#';';;::,:;;;::';'++++++''';;;;':,;;;;;':;;';;'::;';\r\n"
":::::::;::::::;;;;''++'',,;:''+''::::;:,:::;;;;;;:++''''+##+++++++++++++######++++++++###########+'+#++';';;';;;;;;;;;''''+'''''';;;;':,;;;;;':;;;;'':;;;;\r\n"
",,:,,:::::::,::::::::::::.,``:+'';;:::::;..::::::;++'''''+@++++++++++++######++++++++++##########+'+++#';';;;;';';';;:;;:;;;'''';;;;:;:,;;;;;':;;';'':;;;;\r\n"
",,,:,,,,,,::,,,:::,:,,::,;;,``'+';:::::::;''.;;:,;++''''+'#++++++++++++#####++++++++++++#######+++''+##;';;;;;;;;;';;:::::;;;:;;;;';:,,,;;;;;'::;;;;'::;;;\r\n"
",,,:::,,,,,,,,,:,:,,,,,,:```. +++;:;:;;:;:'';;:;;:'++''+++@#++++'++++++++++++++++++++++++###++++++''+#+'''''''''';;;;;;;::;;;::::;';';;;;;;;,,::;;;;'::;';\r\n"
"::,,.,.,,,,,,,,,,,,:,,,,:...`;,'';;;::;::;;':::;;:'++'++++##+''''''++++++++++++++''+++++###+++++++';##'++'''''''''''';;;;;;;;;;;;;';''''';''';;;;';::::;';\r\n"
";;';;;;;;;;:,,,.,,,,,,,,:;``,;:'';;;:::;;;;;:;;;;':'+'+++'##++'''''''++++++++++++'''++++#####+++++';+#'''''''''''''''''''''''';;;;;;;;;;;;';'''''''';;;;';\r\n"
";;;;;;;;;;;;;;;;;;;';;:,,.,. `.''';;:'';;:;';;';;;;;+'+#++##++''''''''+++++++++'''''+++++###++++++''#+;'''''''''''''''''''''''''''''''';';';;;;;;'''''''''\r\n"
":::::;::::;;:;;;;;;;;;;;;;'':`,';:,:'++;;;;;;;;;;;;;'+'##+##++''''''''++++++++'''''''++++###++++++';#';;;;;;;;;;'''''';''''''''''''''''''''''''''''';;;;''\r\n"
":::::::::::::::;:::::::::;;';:;;'''';;;;:;;'';;;:;;;;+''+++##+'''''''''+++++++'''++'''+######+++++';+;;;;;;;;;'';'';;;;;;;'''''''''''''''+++++++++++''''''\r\n"
",,,:::::::::::::::,:::::::;;;:::;;'';;;;;''''''''''';'''''+#++'''''''''++++++++++##+''#######+++++';+::;;;;;;;;;;;;;;;;;;;;;;;;;'''''''''''''+''++++++++++\r\n"
":,,::,,,,,,:,::::::::::::::;::::::'';:;;;:;';'''++'+';'''''''''''''''''++++++++++++++##########+++''+:::::::::::;;;;;;;;;;;;''''''';;;;;'''''''''''''+++++\r\n"
"::,::,,:,,,:,:::,:::::,:::::::::::'':,,,,:;::::;;'''';''''''''''''''''+++++++++++++############+++'';::::::::::::::::::;;;;;;;;;;;'''''''''';';'''''+++'''\r\n"
";;:::::::,,,,::::::,::,,::::::::,,;'';;:;;';;'+++'';;:;'''''''''''''''++++++++++++#############+++';:;:::::::::::::::::::::;:;;;;;;;;;;';'''''''''''';''''\r\n"
"'''''''''''';;;;;;;::::::::::::::,;''::::,'''++++++'++''''''''''''''''+++++++++++++########+##++++'::'';:::::::::::::::::;;;;;;::::::::::;;;;;';;'''''''''\r\n"
"'''''''''''''''''''''''''';;;::::::;;;;;+++++#+++++++#+';;''+''''''''''''++++++''++++++++++####+++';;+';::::::::::::::::::;;'';;;:::::::::::::;;'+;;;;;'''\r\n"
"'''''';''''''''''''''''''''''''';;';;'':'+'+++++++++#++++'''''''''''''''+++++'+++++++++####@###++'';'''':::::::::::::::::;';''':::::::::::::::;;';:;;;;;;;\r\n"
";;;;;;;;'';''''''''''''''''''''''''';'''''''+'''''''++++++''''''''''''''++#@######+#++#@#######+++:''''';:::::::::::::::;;'+++'':::::,,,,,,,:;:'';:;::;;;;\r\n"
";;;';';';;;;;;;;;'';;;;;;;;;;;';;;;;;;'''''''''''''+++++++'';'''''''''''+#####################+++':;';''::::::::::::::::''''++'''::,,,,,,,,,:;'+;::::;;;;;\r\n"
";;;;;;;;;;;;;;;';;;;;;;';;;;;;;;;;'';;;;''''''''''''++++'''';'''''''''''++##+++++++#####+#####+++;::+;''::::::::::::::::;;'+++'''',,,:;'.,,,;;,:;;:::;;;;;\r\n"
"::::::::::::;;;;';;;;;;;;;;;;;;;;;'';;;''''''''''''''++''''';''''''''''++++++''+'++++#++++####+++::;+:''::::::::::::::::;''++'+'''',:;;;;..;;,,,:;;:;;;;;;\r\n"
"::::::::::::::::;;::::::::::::;;;;'';;;;;+''''''+++'++++'+'';''''''''''++++++''''+++++++++####++;:::':''::::::::::::::::;''++'+,,;'::';';.,:;,,::;::;;:;;;\r\n"
"::::::::::::::::::::::::::::::::::;':;;;;''++''''''';;++'++';'''''''''++++++++''+++++++++#####++::::':''::::::::::::::::;;;++;',,,:;;.';;,:;`,.:;:;;;;;;;;\r\n"
"::::::::::::::::::::::::::::::::::;':;;;;;'++;'''';;;;'+'+'';''''++'++++++++++++++######+####++:::::':;'::::::::::::::::';:++;,,,,,;':.;;,;;,,,;';:;;:;;;;\r\n"
"::::::::::::::::::::::::::::::::::;':;;;;''+''''';';;;''++'';''''++++'++++++++++++++++#######+;:::::':;';:::::::::::::::;;:'+;,,,,,,';:,.;;.,,;;':::;;;;;;\r\n"
";;;;;;::::::::::::::::::::::,:::::;':;:;:+'+';'';;;;+++'+++';''''+++++++++++++++++++++######+'::::::':;';:::::::::::::::;;:++;,,,,,,,';::;;,,,''';::;;:;;;\r\n"
"::::::::::::::::;;;;;:::::::::::::;':;;;:'';;+;';;'+++#;+++:;''''+++++++++++++++++++++#####++:::::::':;';:::::::::::::::;;:++',,,,,,,;;;';,,,'';';:::::;;;\r\n"
"::::::::::::::::::::::::::::::::;;'':;:::''''''';'+'''#+#',:;''''++++++++++++++++++++++####+;;;::::;;::';:::::::::::::::'::++',,,,,,,,:;;;,,;';;';::::::::\r\n"
"::::::::::::::::::::::::::::::::::;':::;:';;';'''+';####+:::;''''++++++++++++++++++++++++#+;;;:::::;'::'':::::::::::::::'::++',,,,,,,,,';,,;';:;';::::::::\r\n"
"'''''';;:;;;;;;:::::::::::::::::::;':::;::;;;';;+''###+#.,::;'''''+++++++++++++++++++++++++;;;:::::;;:;';:::::::::::::::'::'+;',,,,,,,,,::';:;::';::::::::\r\n"
":::::';;::;;;;;;;;;;;;;';:::::::::;;::::,:;;;;;'''++: ..,:;;;''''++++++++++++++++++++++++++;:::::::;'''';;::::::::::::::';:++;'+',,,,,:;';::::::';::::::::\r\n"
":::::;;;;::::::::::::::;;:::::::::;'::;;;;;;;;;''#  ;..,:;;;;'''''+++++++++++++###+++++++++,::::::;'+++++;::::::::::::::';''+'+''''++';::::::;;:';::::::::\r\n"
"::;::;;;;;::::::;::::::;;:::::::::;':::;;;;;;;';;``...,:;;;;;'''+++++++++++++++++++++++++++` .:::::;''''':::::::::::::::+++++++';;;;;::::::::;;;'';:::::::\r\n"
";:::::;;;:::::::::;;;;:;;:::::::::;':::;;;;;:;:```..,,::;';;''''+++++++++++++++++++++++++++,```.,;;;;;;;;:::::::::::::::;;;'+;::;;;::::::::::;+++'::::::::\r\n"
"::::::;;;:::::::::;;::::;:::::::::;',::;;;:;,```...,::;;''';''''++++++++++++++++++++++++++',,..`` :;;;;;;::::::::::::::::;;;;;;;;:;:::::::::::;;;;;;::::::\r\n"
":::::;;';::::::::;:;;:::;::,,,,,,,:',::;:;.``````.,,::;;'''''''+'+++++++++++++++++#+++++++',:.:.``.:::;:;::::::::::::::::;;;'';;;;::::::::::::;;;;;:::::::\r\n"
":::::;;;:;;::,:::::;;:::;:,,,,,,,,:;::::.`````..,,:::;;'''''''''++++++++++++++++++++++++++':::,:...`..:::;:;::::::::::::::;;'';;;;:::;::;::::::;;;;;;:;;::\r\n"
":::::::::::;;::::::;;;::;:,,,,,,,,:;:,:  ````.,,,:::;;'''''''''''+++++++++++++++#++##+'+++':::,;:...```.``:::::::::::::::::;;';;;;;::;:::;:::::;;:::::::::\r\n"
"::::,:::::,::;::::::;;::;::,,,,,,::;:. ``````,::,::;;;''''''''''+'++++++++++++++++++++'+++';,:::::,,...``.` ,::::::::::::::;;';;;;;;;;::::;::::;:;;;:;;:::\r\n"
":,,,,::::,,,,:;:,:::;'::;::,,,,,,:::` `````..:::::;;:;;''''''''''+++++++++++++++++++++++++;+:::::::,:,...``.```:::::::::::;;;;;;;;;::::::;;;:;;:;;;:;;::;:\r\n"
",,,,,::::,,,,,;:,:::;;::;::,,,,,,:. ````....:;;::;;:;;;''''''''''+++++++++++++++++++++++++'#;::::;:,:,,...`...`` ::::::::::;;;;;;;;:::::::;;::;;;;;;;;:;;:\r\n"
",,,,:::::,,,,,:;::::;;::;::,,,,,,` ````....,:;;;;;;;';;;''''''''++++++++++++++++++++++++++'+':::::::,:,.,,.....````,::::::;;'';;;;;:::::::;;:::';;;;;;;;;;\r\n"
",::::;;;::,,,,,::::::;::;::,,,,,` ````....,:;'';;;;;;;;;;'''''''+++++++++++++++++++++++++'++';::::::,:,,,,,,,:,...`` ::;:::;'';;;;;;:;::::;;::;';:;;;;::;;\r\n"
"::::;;;:;;::,,,,;::::;::;:,,,,, `````...,,:;''';;';;;;;'''''''''+++++++++++++++++++++++++++''';:::;::,,,,:::::::,...``,::;;;;'';;;;;::::;:;;::;'+;::;;:::;\r\n"
"::;::;;:::;;:,,,:::::;::;:,,,` ``````.,,,::;'+';;;;;;;;'''''''''++++++++++++++++++++++++++#'''';:;:;::,,,,:::;;::,,,..`.:;;;'''';;;;:::::;;;::;'+;;;::::;:\r\n"
":;::::;::::;;,,,,;::::;::,,.` ``````..::::;'''';'';;;;;''';'++''+++++++++++++++++++++#++++#;''';;:;;:,:,:::;::;;;:::,,,`:;''''''';:::::::::;::';+'::;;;;;;\r\n"
":;::::;;:::;:;,,::::::;:;.  ``````...,:::;'+++';;;;;;;';'''''++++++++++++++++++++++++++++#+'''';;;:;:,:::::::::;;::::,,,`;''''+''';;:;;:;:;;::;;';;:;;;;;;\r\n"
";:::::;;:::::;::,:;:::::`  ```````.,,:;;;''+'''';;;;:;;';'''''++++++++++++++++++++++++++##''''';;;;,::,:::::::;;;;::;:::,:++++''''';;;;:;::;;;''';::::;;;;\r\n"
";:::::;;:::::::,,:;::::`````.````.,,,;;;'''++''';';:;;;;;''''''++++++++++++#+++++++++++##++''''';'';::::::,,::;;;;;;;;;:,.'+''+++''';;;::::;;;''';;;;;;;;;\r\n"
";:::::;;::::::::::;::`` ````..`...,::;;''''''''';;;:;:;:;+'''''''#+#+++++###++++++++#####''''''';';';:;::;;;;;;;;:;;;;;;:,'++++'+'';'';;;;:;;;;'';;;;;:;;;\r\n"
";:::::;;;:::::;:::::```````.....,:::;;;''''''';;;;;;;:;;''''''''''+####++######++#######+'''''';;;;;;;;;;;;';;;;;:;;;;;;;,,'+++''''';';:::;;;;:'';;;;;:;;;\r\n"
";:::::;:::::::;:::`   ````.....,,:;;;;''''';;;;;;:;:;';;''''''''''';+##################+''''';'';''';'':;;'';;;;;:;;;'';;:.'+'+++'''';;::::;:::;;;;;;;;;;;\r\n"
";:::::;:::::::;:. ```````...,,,,::;;;''''';;;;;;;;:;;;;;;''''''''''''''+##############'+''''''''''''';'':''';;;;;;;;;;'';;,+'+++++''''';:::;::::;;;;;;;;;;\r\n"
";::::::::::::::` ```.`.....,,,:::;;''''''';;;;;;;::;:;;;;;;'''''''''''''''+++#####+++'''''''''''''''';''';';;;;;;;;''''';;,''++++''++++';:;;:;;;;;;;;:;;;;\r\n"
";:::::;::::::,````.......,,,,::::;;'''''''';;;;;;;;;'';::;'';'''+'''''''''++'''''++++'+++'''''''''''';;''';;;;;;'';''''';;:;+++++''++++;;:::;::::::;:;;;;;\r\n"
";::;;;;;:,````````......,,:,:,::;;;''''''''''';;:;';;;;;;;;;;;;'''''''''++++++++++'+++++''''''''''''''''';'';;;;;;;''''';;::'++'+++''+'';:::;:::;;;;;;;;:;\r\n"
";;;;;;;;,.`............,,,::::::;;'''''''''''''';';'';;;;;'+';;;'''''+'''+++++++++++++'+++'''''''''''';;''';';'';''''''';;:,''+'''+''++';;::::::;;;;;;;:::\r\n"
";;;;;;;:.,`....,,..,,,,,,,,::::;;;''''''''''''';;;;''';;;;'''';;'';'''''++''++++++'+++''+'';;'''';;'';;;;';;;''''''''''':;:,'++'++++'''';;::;::;;:;;:,,;;;\r\n"
";;;;;;;,:,`...,,,,,,,:,:::::::;;;;''''''''+'''';';;;;;;;;;''''''''''''''+++'''++++''''''+'''''''''''';;;';;';;;''''''''':;:,'++''''''''''':::;:::;:,;;;;;;\r\n"
";:::;::.,``...,,,,,,,,::::::::;;;''''''''''''':';;;';;;;;;;''''''''''''''++++''+''''+++'+''''''''''''';;;''';;;;'''''''':;:,++++++++++++++::;;;:,:;:;:;;;;\r\n"
":;;;;:;````.,,,,,,:::::::::;;;;;''''''''''+''';;'';;;;;;;''''''''''''''+'+++++++++++++'+++'''''''''';;;;;;;;';;;;'''''';:;:,+++++++;+'+++':::::;;;;;;;;;;;\r\n"
";;;;::,```..,,,,,,,:::::::;;;;;;''''+'+'++++''''';;;;;;;;;'''''''''''''''''++''+'''++'+++'''''''''';;'';;;;;;';;'''''''';;::'+++++++++#+;;::;;;;;;;;;;;;;;\r\n"
":::::,````..,,,,,,::::::;;;;;;;''''''++''+++'''''';'';;;'''''''''++''''+'+++++'+'''+++'+++'''''''''';';;;';'';;;;'';''';;;;:++++++++++#+;;:;;;:;;;;;;;;;;;\r\n"
";:::` ``..,,,,,::::::;;;;;;;;;'''''+''++++++'+'+'';'''''''''''''''''''''++'''++'''''''+++'''''''''''''';;;;;;;;;;''''''';;::''+'''''++++;;::;;;;;;;;;;;;;;\r\n"
";'+```...,,,,,:::::::;;;;;;;;;;''+++''++++++;'''''';''''''''''''''''''''''++++++''+'+'++''''''''''''';''';';;;;;;;''''''':;;+++'+++'++++;;::;;;;;;;;::;;;;\r\n"
":;,``....,,,:::::;:;;:;;;;;;';;''+++++'++++++'+''''''''''''';'''''''''''''''+++++'''''++'''''''''''''''''';;;;';;;''''';;:;;+++++++#+#+';;::;;;;;;;:;;:;;;\r\n"
";;` `...,,,:::::::;;;;;;;;;'''''''+++++++++'''+'';;''''''''';;;;'''''''+'++++''++''''++'''''''''+''''';;;;;;;'';;;''''';;:';+++'++##+#++;;;:,..,....,,;:;;\r\n"
":, `...,,,:::::;;;;;;;';;;;'''''''++++++++'+''+''''''''''';;''''''''''''''''''+++++''+'++''''''+''';''''''''';;;;;''''';;:';++++++#++++';;::,,,:,,,,,,;;;;\r\n"
", `...,,,,:::::;;;;;;;;';;;'''''''++++++++'''+''''''''';'';;';''''''''''''+'''''+++'+++++'+''''+''''''';;''''''';;''''':;;';++++#++#+++';;::,,,,,,.,.,:::;\r\n"
"``...,,,:::::::;;;;;;;;;;;''''''''++++'+++'''++'''''''';';;;';'''''''''''''+''''++'+++++'''''''+'''''''';;;;;'';;;'';'';;;';'++++++++++';:::,,,.,....,:;:;\r\n"
"```..,,::::::;;;;;;;;;;;;;''''''''+++++++''''''''''''';;;;;'''''''+++''''+''''''+'''+++'+'''''''''''''';';''';;;;;''';;':'';'+++++++++'';;::,,,,;:...,:::;\r\n"
"``...,,:::::;;;;;;;;;;;;;''+''''''+++++++'''''''';;;;;;;;'''''''''''+''''+''''''''++++'''''''''''''''''''''';;;;;;'';;;':++';:''+++++++';;:::,,:,.,..:::::\r\n"
"`..,,,,:::;;;;;;;';;;;;;'''++'';''+++++++''''''''';;;;;;;;''''''''''''''+''''''''''+++'''''''''''''';'''';';;;;';;;;;;;':+'';''+'++++++';;:::,,:,,:..:;:::\r\n"
"`.,,,:,:::;;;;;;'';;;;;';''++';;'''+++++''''''''''';;;;;;'''';'''''''''+'+'''''''''+++''''''''''''''''''';;;';;';;';;;:':++',...:;'++++';;:::,,,';:,.:;:::\r\n"
"..,,::::;;;;;;;;'';;;;'';''++';;'''+++++';;'''';;:;;;;;;;;'''''''''''''''''''''''++++++'''+'''''''''';';''';;;;;;;;;;;:;;++:       `  .:;;:,:,,.;,;,,:;:::\r\n"
"..,,:::;;;;;;;;'''';;;;''''++';;'''++++++''''';:;;;;;:;;;;'''''''''''''+'''''''''''+'''+'''''''''''''''';;';;;;;;;;;;;;;'+'.````````````,::,,,..::;:,:::;:\r\n"
".,,:::::;;;;'';'''';;;'''''++';;'''++++'''''';;;';;;;;;;;'''''''''''''+'''''''''''++'''++''''''''''';';''''''';';;;';;;;++',.........,,,:;:;;::::;;;;;:,,:\r\n"
",,,::;;;;;;;;'''''';;;'''''+++';'''++++'''''''';;;;;'''''''''''''''''''''''''''''''+'''''+''''''''''''';';';;;;;;;;';;:;++'::,,,,,,,,,,,:':';;';''''';;;;;\r\n"
",,::;:;;;;;;;;''''';;;'''''+++';;'''++++';''''';'''''''''''''''''''''''+'''''''''+'''++'+'+''''''''''';';''';;;;;;;';;;;++':::,:::::::,,,;:,`::;::,:,::;;;\r\n"
":::::;;;;;;';;''''';:;;''''+++';;''++++''''''''''''''''''''''''''''''''''''''''''''''++''++++'''''''''''';'';;';;;;';:;;''''.,,,,,,,,,,:;:,,..,,:....,::::\r\n"
"::::;;;;;;;';;'''';;;;''''''++';;''+++'''''''';''''';'''''''''''''''''''''''''''''''''+'''+'+'''''''''''';+;;;';;;';;:;;+''',,;;;;::;;;;;:;,;,.,:,,,.;;;;;\r\n"
":,::;;;;;;;;;;;''';;;;'''''++++;;'++++''''''''''''';;;'''''''''''''''''''''''''''''''''+'''''''''''''''''''+'';;'';;;:';+''':,''++'++++''';,;,.;;::,,:::::\r\n"
"::::;;;''';;;;;'+';;;''';'''++';''++'''''';;''';;;;;;''''''''''''''''''''''''''''''+'+''+'''''''''''''''''''+';;;;;;;:';+';':,''++++++++'';,;,.,,,,,:::;''\r\n"
"::::;';;'''';;''+'';;''+';''++';''+''''+'';;;;;;';;;;''''''''''''''''''''''''';'''''+++'+''+'''''''''''''''''+';';;;;:';'';'::;'++++++++++;,;.:,,,,,,;''''\r\n"
":::;''';;'''';;'++'';''+';''++';;'+'''''''';;;;;;;;;;''''''''''''''''''''''';;;''''++++''''''''''''''''''''''+';';;;;:''++'':::'++'+++'+++;,',.:;,,,,;''''\r\n"
":::;;''';;'';;;''+''''++';'+++';;'''''''';;;;;;;;';'''''''';;''''''''''+''''';''''++''+'+''+'''''''''''''''''''';;;;;;'++''';;:'++'++++++'':'';'';;'''''''\r\n"
";::;''''';;'';;''''''''+''''++';''''';;;;;;;;''';;''''''''''''''''''''''''''';'''''+'''++++'''''''''''''''''';'';;;;:';++;';;;;'++'++''++'+:'::,;.,,:'''''\r\n"
";::;'''';';;';;''''''''+';'+++';''';;;'''';;'''';;''''''''''''';'''''''''''';'''''++''''+++''''''''''''''+''''';;;;;:''+''';'';''+'+++++++':;+`,,,,,''''''\r\n"
";::;''';''';;'';'+';''++';'++';;'';;;''''''''''';';''''''''''''''''''''''''''''''+'++'+++''''''''''''''''''''';';;;;:'++'';+;''''''++'++++',;+.,,.:,+'''''\r\n"
";;::''';''';;'''''''''++';'++';;;';';;;''''''';'''''''';'';;'';''''''''''''''''''+''''++++''''''''''''''''''''';';;;:++'''++';''''+'''+'+;':;+''';''+'''''\r\n"
";;:;''+'''';;;'''''''+++'''+''';''';;;''''''''';''''''''''''''''''''''''''''''''+'''+'++++'''''''''''''''''''''';;;;:+'''+#''''''+'+';''+':,;+++'++'''';';\r\n"
";;:;'++'''';;;'''''''+++''++''';;'';;';''''''';''''''''''''''''''''''''''''''''+'++++'''+'''+'''+'''''''''''''''';;:;''+'##':''''''+++;'''',:'''';:;++';';\r\n"
";;;:;'++''';;;'''''''##'''++''''''';;;''''';;;;;'''''''''''''''''''''''''''''''+++'+++''''''++++'''''+'''''''''';;;:;'++##+;,''''''++'+'';'::'''''''++;'''\r\n"
";;;;''++'';';;'''''''##'''++'';'';;;;'''';;;;;;;''''''';'''''''''''''''''''+'''''''''++++'''+''''''''''++'''''''';;:'#++#+';:''''+'+++'';:::;;;;;'''''';''\r\n"
";;;;;'+++'';;'''''''+#+''+++'';''';;;;;;;;;;;;;;'''';;;''''''''''''';'''''''''+++''''++''+++++''''++''+'''''''''';;:+#+++';;;;'''''+,,'+;::,;;::;'''''';;'\r\n"
";;;;''+++'';;;'';'''+#''+++''''''';;;;;;;;;;;;'';;;'''''''''''''''''+''''''''''''++''+'+''++''+'''++'''''''''''';;;:++++;;'';;;''+'+';'';::,;;:::'''''''''\r\n"
";;:;;'+++''';;;'''''##''++''''+'';;;;;;;;;;;;;''''''''''''''''''''''''''''''''++''++''+++''+''+'''''+++++'''''''';;;'+'+'''''';:'';;'''';::,:;::;;''';'';;\r\n"
";;;;;''+++'';;''''''#'''+'''''+';;;;;;;;';;;;;;'''''''''''''''''''''''''''''''+++'++'++'''''''++''+'+++''''''''';;;''++'''''++';:;;'';;;:::,::::;'+';;''''\r\n"
";;;;;'''++'';;''''''#''''''''+'';'';;'';;;;;;;;;;;'''''''''''''''''''''''+'+''''+'''+++''+'''++'''++''+''+'''''';;:''''''+++##+';:;'''+';::,::::;'''''''''\r\n"
"'';;;''+++'';;'''''++''+''''+''''''''';;;;;;;;;;'''''''''''''''''''''''''''''''+++++++++++''''''''''''+'++'''''';;:'''''+++###++';;:'''';::,,::::''++'''''\r\n"
"'';;;''+'+''';;''''#'''''''+++'''''';;;;;;;;'';;';;''''''';;''''''''''''+'++'''+'''''''''+''+'+''''+''''''''''''';:''''+++#####++'';:''';:::,;::;'''''''''\r\n"
"'';;;'++''''';;'''++'''''''+++''''';;;;;;;;;;;;;;;;;''''''''';''''''+'''''+++'+++++'''''''''++'''''''+'''''''''';;:'''+'+#######+++';:;;;::::;:;:;;'''';''\r\n"
"+'';;'++''''';;''+++''';'+'++++'';;;;;;;;;;;;;;;;;;;;;';;'''''''''''''''+'++'++'+''''+'''+'''+''''++'''''''''''';;:''+'++#######++++';:;';:::;::;'';''+'';\r\n"
"#'';;'+++'''';;''+++''+++++''++'';;;;;;;;;;;;;;;';''';;;;;''''''''''''''''''''''++'''++'''''''''''''''''''''''''';;'''++#########++++';:''';:++++'''+'';;;\r\n"
"+#+';''++';''';;'+''+#+++++'''''';;;;;;;;;;;;;;;;;;''';;;;'''''''''''''''''++++++''''+++'++''''++++++''''''''''';;;'++++##########++++';:;;:,++++''';;;;;:\r\n"
"'+#+''''''';''''';+++++++++'';'''';;;;;;;;;;;;;;;;';;;;''''''''''''''''''''++++'+'''++'''++++'''+'+'''''''''''''';''++++############+++';:;:,+++'';;;;:::;\r\n";
  
  std::fstream file(path, std::ios::out | std::ios::trunc);
  if(file)
  {
#define LINE_ENDINGS "\r\n"
    auto LINE_END = [](const std::string& string)
    {
      return string + LINE_ENDINGS;
    };

    file << LINE_END("+++++ IT'S MA BOIS +++++");
    file << ma_boi_pat;
    file << ma_boi_shehzan;

    std::map<int, int> model_id_map;
    std::map<int, int> diffuse_texture_id_map;
    std::map<int, int> normal_texture_id_map;
    std::map<int, int> material_id_map;

    file << LINE_END("+++++ MODELS +++++");

    //models
    int model_id = 0;
    for(const auto& pair : m_sceneLoaded->modelMap)
    {
      int original_model_id = pair.first;
      auto& model = pair.second;

      model_id_map.insert({ original_model_id, model_id });

      if(model.name.find(".gltf") == std::string::npos)
      {
        file << "MODEL " << model_id++ << LINE_ENDINGS;
        file << "path " << model.name << LINE_ENDINGS;
        file << LINE_ENDINGS;
      }
    }

    file << LINE_END("+++++ DIFFUSE TEXTURES +++++");

    //diffuse textures
    int diffuse_texture_id = 0;
    for (const auto& pair : m_sceneLoaded->diffuseTextureMap)
    {
      int original_diffuse_texture_id = pair.first;
      auto& diffuse_texture = pair.second;

      diffuse_texture_id_map.insert({ original_diffuse_texture_id, diffuse_texture_id });

      if(!diffuse_texture.was_loaded_from_gltf)
      {
        file << "DIFFUSE_TEXTURE " << diffuse_texture_id++ << LINE_ENDINGS;
        file << "path " << diffuse_texture.name << LINE_ENDINGS;
        file << LINE_ENDINGS;
      }
    }

    file << LINE_END("+++++ NORMAL TEXTURES +++++");

    //normal textures
    int normal_texture_id = 0;
    for (const auto& pair : m_sceneLoaded->diffuseTextureMap)
    {
      int original_normal_texture_id = pair.first;
      auto& normal_texture = pair.second;

      normal_texture_id_map.insert({ original_normal_texture_id, normal_texture_id });

      if (!normal_texture.was_loaded_from_gltf)
      {
        file << "NORMAL_TEXTURE " << normal_texture_id++ << LINE_ENDINGS;
        file << "path " << normal_texture.name << LINE_ENDINGS;
        file << LINE_ENDINGS;
      }
    }

    file << LINE_END("+++++ MATERIALS +++++");

#define STREAM_WIDTH (16)
#define FORMAT_LEFT std::left << std::setfill(' ') << std::setw(STREAM_WIDTH)

    //material
    int material_id = 0;
    for (const auto& pair : m_sceneLoaded->materialMap)
    {
      int original_material_id = pair.first;
      auto& material = pair.second;

      material_id_map.insert({ original_material_id, material_id });

      if (!material.was_loaded_from_gltf)
      {
        file << "MATERIAL " << material_id++ << " " << material.name << LINE_ENDINGS;
        file << FORMAT_LEFT << "RGB" << material.material.diffuse.x << " " << material.material.diffuse.y << " " << material.material.diffuse.z << LINE_ENDINGS;
        file << FORMAT_LEFT << "SPECRGB" << material.material.specular.x << " " << material.material.specular.y << " " << material.material.specular.z << LINE_ENDINGS;
        file << FORMAT_LEFT << "SPECEX" << material.material.specularExp << LINE_ENDINGS;
        file << FORMAT_LEFT << "REFL" << material.material.reflectiveness << LINE_ENDINGS;
        file << FORMAT_LEFT << "REFR" << material.material.refractiveness << LINE_ENDINGS;
        file << FORMAT_LEFT << "ETA" << material.material.eta << LINE_ENDINGS;
        file << FORMAT_LEFT << "EMITTANCE" << material.material.emittance << LINE_ENDINGS;
        file << LINE_ENDINGS;
      }
    }

    file << LINE_END("+++++ OBJECTS +++++");

    int object_id = 0;
    for (const auto& object : m_sceneLoaded->objects)
    {
      if (object.name.find(".gltf") == std::string::npos)
      {
        file << FORMAT_LEFT << "OBJECT" << object_id++ << " " << object.name << LINE_ENDINGS;

        if (object.model == nullptr)
        {
          file << FORMAT_LEFT << "MODEL" << " -1" << LINE_ENDINGS;
        }
        else
        {
          file << FORMAT_LEFT << "MODEL" << model_id_map[object.model->id] << LINE_ENDINGS;
        }

        if (object.textures.albedoTex == nullptr)
        {
          file << FORMAT_LEFT << "DIFFUSE_TEX" << " -1" << LINE_ENDINGS;
        }
        else
        {
          file << FORMAT_LEFT << "DIFFUSE_TEX" << model_id_map[object.textures.albedoTex->id] << LINE_ENDINGS;
        }

        if (object.textures.normalTex == nullptr)
        {
          file << FORMAT_LEFT << "NORMAL_TEX" << " -1" << LINE_ENDINGS;
        }
        else
        {
          file << FORMAT_LEFT << "NORMAL_TEX" << model_id_map[object.textures.normalTex->id] << LINE_ENDINGS;
        }

        if (object.material == nullptr)
        {
          file << FORMAT_LEFT << "MATERIAL" << " -1" << LINE_ENDINGS;
        }
        else
        {
          file << FORMAT_LEFT << "MATERIAL" << model_id_map[object.material->id] << LINE_ENDINGS;
        }

        //transformations
        file << FORMAT_LEFT << "trans" << object.translation.x << " " << object.translation.y << " " << object.translation.z << LINE_ENDINGS;
        file << FORMAT_LEFT << "rotat" << object.rotation.x << " " << object.rotation.y << " " << object.rotation.z << LINE_ENDINGS;
        file << FORMAT_LEFT << "scale" << object.scale.x << " " << object.scale.y << " " << object.scale.z << LINE_ENDINGS;

        file << LINE_ENDINGS;
      }
    }

    file << LINE_END("+++++ GLTFS +++++");

    for (const auto& pair : m_sceneLoaded->modelMap)
    {
      auto& model = pair.second;

      if (model.name.find(".gltf") != std::string::npos)
      {
        file << "GLTF " << model.name << LINE_ENDINGS;
        file << LINE_ENDINGS;
      }
    }
  }
}

void D3D12RaytracingSimpleLighting::PreSaveImage()
{
  if(save_image)
  {
    if (save_image_resource != nullptr)
    {
      save_image_resource.Reset();
    }

    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();
    auto commandAllocator = m_deviceResources->GetCommandAllocator();
    auto renderTarget = m_deviceResources->GetRenderTarget();

    D3D12_RESOURCE_DESC desc = renderTarget->GetDesc();

    UINT64 totalResourceSize = 0;
    UINT64 fpRowPitch = 0;
    UINT fpRowCount = 0;

    // Get the rowcount, pitch and size of the top mip
    device->GetCopyableFootprints(
      &desc,
      0,
      1,
      0,
      nullptr,
      &fpRowCount,
      &fpRowPitch,
      &totalResourceSize);

    // Round up the srcPitch to multiples of 256
    UINT64 dstRowPitch = (fpRowPitch + 255) & ~0xFF;
    save_image_row_pitch = dstRowPitch;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Alignment = desc.Alignment;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.Height = 1;
    bufferDesc.Width = dstRowPitch * desc.Height;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.SampleDesc.Quality = 0;

    ThrowIfFailed(device->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
      D3D12_HEAP_FLAG_NONE,
      &bufferDesc,
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr,
      IID_PPV_ARGS(&save_image_resource)));

    // Get the copy target location
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT bufferFootprint = {};
    bufferFootprint.Footprint.Width = static_cast<UINT>(desc.Width);
    bufferFootprint.Footprint.Height = desc.Height;
    bufferFootprint.Footprint.Depth = 1;
    bufferFootprint.Footprint.RowPitch = static_cast<UINT>(dstRowPitch);
    bufferFootprint.Footprint.Format = desc.Format;

    CD3DX12_TEXTURE_COPY_LOCATION copyDest(save_image_resource.Get(), bufferFootprint);
    CD3DX12_TEXTURE_COPY_LOCATION copySrc(renderTarget, 0);

    D3D12_RESOURCE_BARRIER preCopyBarriers[1];
    preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    commandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

    commandList->CopyTextureRegion(&copyDest, 0, 0, 0, &copySrc, nullptr);

    D3D12_RESOURCE_BARRIER postCopyBarriers[1];
    postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);

    UINT64 imageSize = dstRowPitch * UINT64(desc.Height);
    readRange = { 0, static_cast<SIZE_T>(imageSize) };
    writeRange = { 0, 0 };
  }
}

void D3D12RaytracingSimpleLighting::PostSaveImage()
{
  if (save_image)
  {
    if(!save_image_path.empty())
    {
      auto device = m_deviceResources->GetD3DDevice();
      auto commandList = m_deviceResources->GetCommandList();
      auto commandAllocator = m_deviceResources->GetCommandAllocator();
      auto renderTarget = m_deviceResources->GetRenderTarget();

      D3D12_RESOURCE_DESC desc = renderTarget->GetDesc();

      void* mapped_data;
      save_image_resource->Map(0, &readRange, &mapped_data);

      //https://stackoverflow.com/questions/30021274/capture-screen-using-directx/30125325
#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define __WFILE__ WIDEN(__FILE__)
#define HRCHECK(__expr) {hr=(__expr);if(FAILED(hr)){wprintf(L"FAILURE 0x%08X (%i)\n\tline: %u file: '%s'\n\texpr: '" WIDEN(#__expr) L"'\n",hr, hr, __LINE__,__WFILE__);goto cleanup;}}
#define RELEASE(__p) {if(__p!=nullptr){__p->Release();__p=nullptr;}}
      auto SavePixelsToFile32bppPBGRA = [](UINT width, UINT height, UINT stride, LPBYTE pixels, LPWSTR filePath, const GUID &format)
      {
        if (!filePath || !pixels)
          return E_INVALIDARG;

        HRESULT hr = S_OK;
        IWICImagingFactory *factory = nullptr;
        IWICBitmapEncoder *encoder = nullptr;
        IWICBitmapFrameEncode *frame = nullptr;
        IWICStream *stream = nullptr;
        GUID pf = GUID_WICPixelFormat32bppPBGRA;
        BOOL coInit = CoInitialize(nullptr);

        HRCHECK(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
        HRCHECK(factory->CreateStream(&stream));
        HRCHECK(stream->InitializeFromFilename(filePath, GENERIC_WRITE));
        HRCHECK(factory->CreateEncoder(format, nullptr, &encoder));
        HRCHECK(encoder->Initialize(stream, WICBitmapEncoderNoCache));
        HRCHECK(encoder->CreateNewFrame(&frame, nullptr)); // we don't use options here
        HRCHECK(frame->Initialize(nullptr)); // we dont' use any options here
        HRCHECK(frame->SetSize(width, height));
        HRCHECK(frame->SetPixelFormat(&pf));
        HRCHECK(frame->WritePixels(height, stride, stride * height, pixels));
        HRCHECK(stream->Commit(STGC_OVERWRITE));
        HRCHECK(frame->Commit());
        HRCHECK(encoder->Commit());

      cleanup:
        RELEASE(stream);
        RELEASE(frame);
        RELEASE(encoder);
        RELEASE(factory);
        if (coInit) CoUninitialize();
        return hr;
      };

      //flip r and b? (dunno why they are flipped)
      for (int i = 0; i < desc.Width * desc.Height; i++)
      {
        char* data = (char*)mapped_data;
        std::swap(data[i * sizeof(float)], data[i * sizeof(float) + 2]);
      }

      SavePixelsToFile32bppPBGRA((UINT)desc.Width, (UINT)desc.Height, (UINT)save_image_row_pitch, (LPBYTE)mapped_data, (LPWSTR)utilityCore::string2wstring(save_image_path).c_str(), GUID_ContainerFormatBmp);

      save_image_resource->Unmap(0, &writeRange);

      save_image = false;
      save_image_path = std::string{};
    }
  }
}

void D3D12RaytracingSimpleLighting::LoadSplitImage()
{
  //Place loaded image on screen
  if (load_split_image)
  {
    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();
    auto commandAllocator = m_deviceResources->GetCommandAllocator();
    auto renderTarget = m_deviceResources->GetRenderTarget();

    D3D12_RESOURCE_DESC desc = renderTarget->GetDesc();

    UINT64 totalResourceSize = 0;
    UINT64 fpRowPitch = 0;
    UINT fpRowCount = 0;
    // Get the rowcount, pitch and size of the top mip
    device->GetCopyableFootprints(
      &desc,
      0,
      1,
      0,
      nullptr,
      &fpRowCount,
      &fpRowPitch,
      &totalResourceSize);

    // Round up the srcPitch to multiples of 256
    UINT64 dstRowPitch = (fpRowPitch + 255) & ~0xFF;

    static ComPtr<ID3D12Resource> split_image_resource;

    if (image_splitter_position_updated)
    {
      if(split_image_resource != nullptr)
      {
        split_image_resource.Reset();
      }

      // Load the image from file
      D3D12_RESOURCE_DESC textureDesc{};
      int imageBytesPerRow;
      BYTE* imageData;

      wstring wpath = utilityCore::string2wstring(split_image_path);
      int imageSize = TextureLoader::LoadImageDataFromFile(&imageData, textureDesc, wpath.c_str(), imageBytesPerRow);

      if(imageSize <= 0)
      {
        load_split_image = false;
      }
      else
      {
        //flip r and b? (dunno why they are flipped)
        for (int i = 0; i < imageSize / 4; i++)
        {
          std::swap(imageData[i * sizeof(float)], imageData[i * sizeof(float) + 2]);
        }

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Alignment = desc.Alignment;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferDesc.Height = 1;
        bufferDesc.Width = imageSize;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.SampleDesc.Quality = 0;

        ThrowIfFailed(device->CreateCommittedResource(
          &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
          D3D12_HEAP_FLAG_NONE,
          &bufferDesc,
          D3D12_RESOURCE_STATE_GENERIC_READ,
          nullptr,
          IID_PPV_ARGS(&split_image_resource)));

        void* mapped_data;
        split_image_resource->Map(0, nullptr, &mapped_data);

        memcpy(mapped_data, imageData, imageSize);

        split_image_resource->Unmap(0, nullptr);

        image_splitter_position_updated = false;
      }

      ::free(imageData);
    }
    if(!image_splitter_position_updated)
    {

      // Get the copy target location
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT bufferFootprint = {};
      bufferFootprint.Footprint.Width = static_cast<UINT>(desc.Width);
      bufferFootprint.Footprint.Height = desc.Height;
      bufferFootprint.Footprint.Depth = 1;
      bufferFootprint.Footprint.RowPitch = static_cast<UINT>(dstRowPitch);
      bufferFootprint.Footprint.Format = desc.Format;

      //start the transition
      CD3DX12_TEXTURE_COPY_LOCATION copySrc(split_image_resource.Get(), bufferFootprint);
      CD3DX12_TEXTURE_COPY_LOCATION copyDest(renderTarget, 0);

      D3D12_RESOURCE_BARRIER preCopyBarriers[1];
      preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
      commandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

      D3D12_BOX box;
      box.left = 0;
      box.top = 0;
      box.right = image_splitter_x;
      //box.right = desc.Width;
      box.bottom = desc.Height;
      box.front = 0;
      box.back = 1;

      commandList->CopyTextureRegion(&copyDest, 0, 0, 0, &copySrc, &box);

      D3D12_RESOURCE_BARRIER postCopyBarriers[1];
      postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
      commandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);

      //update if automove
      static bool image_splitter_auto_move_direction = false;
      if (image_splitter_auto_move_direction)
      {
        image_splitter_x += image_splitter_auto_move_speed;
      }
      else
      {
        image_splitter_x -= image_splitter_auto_move_speed;
      }

      //clamp, cuz copytextureregion will be mad :(
      if (image_splitter_x <= 0)
      {
        image_splitter_x = 1;
        image_splitter_auto_move_direction = !image_splitter_auto_move_direction;
      }
      else if (image_splitter_x >= m_width)
      {
        image_splitter_x = m_width;
        image_splitter_auto_move_direction = !image_splitter_auto_move_direction;
      }
    }
  }
}

int D3D12RaytracingSimpleLighting::GetHeapOffsetForVertices()
{
  return vertex_offset++;
}

int D3D12RaytracingSimpleLighting::GetHeapOffsetForIndices()
{
  return indices_offset++;
}

int D3D12RaytracingSimpleLighting::GetHeapOffsetForObjects()
{
  return objects_offset++;
}

int D3D12RaytracingSimpleLighting::GetHeapOffsetForMaterials()
{
  return materials_offset++;
}

int D3D12RaytracingSimpleLighting::GetHeapOffsetForDiffuseTextures()
{
  return diffuse_textures_offset++;
}

int D3D12RaytracingSimpleLighting::GetHeapOffsetForNormalTextures()
{
  return normal_textures_offset++;
}

void D3D12RaytracingSimpleLighting::ResetHeapOffsets()
{
  vertex_offset = VERTEX_HEAP_OFFSET;
  indices_offset = INDICIES_HEAP_OFFSET;
  objects_offset = OBJECTS_HEAP_OFFSET;
  materials_offset = MATERIALS_HEAP_OFFSET;
  diffuse_textures_offset = DIFFUSE_TEXTURES_HEAP_OFFSET;
  normal_textures_offset = NORMAL_TEXTURES_HEAP_OFFSET;
}

int D3D12RaytracingSimpleLighting::AllocateHeapDescriptorType(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse, HeapDescriptorOffsetType offset_type)
{
  switch (offset_type)
  {
  case HeapDescriptorOffsetType::NONE:
    {
      return AllocateDescriptor(cpuDescriptor, descriptorIndexToUse);
    }
  case HeapDescriptorOffsetType::VERTEX:
    {
      return AllocateDescriptor(cpuDescriptor, GetHeapOffsetForVertices());
    }
  case HeapDescriptorOffsetType::INDICES:
    {
      return AllocateDescriptor(cpuDescriptor, GetHeapOffsetForIndices());
    }
  case HeapDescriptorOffsetType::OBJECTS:
    {
      return AllocateDescriptor(cpuDescriptor, GetHeapOffsetForObjects());
    }
  case HeapDescriptorOffsetType::MATERIALS:
    {
      return AllocateDescriptor(cpuDescriptor, GetHeapOffsetForMaterials());
    }
  case HeapDescriptorOffsetType::DIFFUSE_TEXTURES:
    {
      return AllocateDescriptor(cpuDescriptor, GetHeapOffsetForDiffuseTextures());
    }
  case HeapDescriptorOffsetType::NORMAL_TEXTURES:
    {
      return AllocateDescriptor(cpuDescriptor, GetHeapOffsetForNormalTextures());
    }
  default: return AllocateDescriptor(cpuDescriptor, descriptorIndexToUse);
  }
}

