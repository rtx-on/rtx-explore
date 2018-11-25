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
#include "D3D12Raytracingばか.h"
#include "DirectXRaytracingHelper.h"

using namespace std;
using namespace DX;

D3D12PathTracing::D3D12PathTracing(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_curRotationAngleRad(0.0f)
{
    raytracing_manager.GetRayTracingApi()->ForceSelectRaytracingAPI(RaytracingApiType::Fallback);
    UpdateForSizeChange(width, height);
}

void D3D12PathTracing::OnInit()
{
    raytracing_manager.Init(this, 3);

    raytracing_manager.InitializeScene();

    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

// Create constant buffers.
// void D3D12PathTracing::CreateConstantBuffers()
// {
//     auto device = raytracing_manager.GetDeviceResoures()->GetD3DDevice();
//     auto frameCount = raytracing_manager.GetDeviceResoures()->GetBackBufferCount();
//     
//     // Create the constant buffer memory and map the CPU and GPU addresses
//     const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
//
//     // Allocate one constant buffer per frame, since it gets updated every frame.
//     size_t cbSize = frameCount * sizeof(AlignedSceneConstantBuffer);
//     const D3D12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
//
//     ThrowIfFailed(device->CreateCommittedResource(
//         &uploadHeapProperties,
//         D3D12_HEAP_FLAG_NONE,
//         &constantBufferDesc,
//         D3D12_RESOURCE_STATE_GENERIC_READ,
//         nullptr,
//         IID_PPV_ARGS(&m_perFrameConstants)));
//
//     // Map the constant buffer and cache its heap pointers.
//     // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
//     CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
//     ThrowIfFailed(m_perFrameConstants->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedConstantData)));
// }

// Create resources that depend on the device.
void D3D12PathTracing::CreateDeviceDependentResources()
{
    // Initialize raytracing pipeline.

    // Create raytracing interfaces: raytracing device and commandlist.
    raytracing_manager.CreateRaytracingInterfaces();

    raytracing_manager.CreateRayGenSignature();

    raytracing_manager.CreateAcclerationStrutures<Vertex>()
    // Create root signatures for the shaders.
    CreateRootSignatures();

    // Create a raytracing pipeline state object which defines the binding of shaders, state and resources to be used during raytracing.
    CreateRaytracingPipelineStateObject();

    // Create a heap for descriptors.
    CreateDescriptorHeap();

    // Build geometry to be used in the sample.
    BuildGeometry();

    // Build raytracing acceleration structures from the generated geometry.
    BuildAccelerationStructures();

    // Create constant buffers for the geometry and the scene.
    CreateConstantBuffers();

    // Build shader tables, which define shaders and their local root arguments.
    BuildShaderTables();

    // Create an output 2D texture to store the raytracing result to.
    CreateRaytracingOutputResource();
}

void D3D12PathTracing::OnKeyDown(UINT8 key)
{
    // Store previous values.
    RaytracingApiType prev_raytracing_api_type = raytracing_manager.GetRayTracingApi()->GetRayTracingApiType();

    switch (key)
    {
    case VK_NUMPAD1:
    case '1': // Fallback Layer
        raytracing_manager.GetRayTracingApi()->ForceSelectRaytracingAPI(RaytracingApiType::Fallback);
        break;
    case VK_NUMPAD2:
    case '2': // DirectX Raytracing
        raytracing_manager.GetRayTracingApi()->ForceSelectRaytracingAPI(RaytracingApiType::DXR);
        break;
    default:
        break;
    }
    
    if (prev_raytracing_api_type != raytracing_manager.GetRayTracingApi()->GetRayTracingApiType())
    {
        // Raytracing API selection changed, recreate everything.
        RecreateD3D();
    }
}

// Update frame-based values.
void D3D12PathTracing::OnUpdate()
{
    m_timer.Tick();
    CalculateFrameStats();
    float elapsedTime = static_cast<float>(m_timer.GetElapsedSeconds());
    auto frameIndex = raytracing_manager.GetDeviceResoures()->GetCurrentFrameIndex();
    auto prevFrameIndex = raytracing_manager.GetDeviceResoures()->GetPreviousFrameIndex();

    // Rotate the camera around Y axis.
    {
        float secondsToRotateAround = 24.0f;
        float angleToRotateBy = 360.0f * (elapsedTime / secondsToRotateAround);
        XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
        m_eye = XMVector3Transform(m_eye, rotate);
        m_up = XMVector3Transform(m_up, rotate);
        m_at = XMVector3Transform(m_at, rotate);
        raytracing_manager.UpdateCameraMatrices(m_eye, m_up, m_at);
    }

    // Rotate the second light around Y axis.
    {
        float secondsToRotateAround = 8.0f;
        float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
        XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));

      //TODO
      //const XMVECTOR& prevLightPosition = m_sceneCB[prevFrameIndex].lightPosition;
        //m_sceneCB[frameIndex].lightPosition = XMVector3Transform(prevLightPosition, rotate);
    }
}


// Parse supplied command line args.
void D3D12PathTracing::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
    DXSample::ParseCommandLineArgs(argv, argc);

    if (argc > 1)
    {
        if (_wcsnicmp(argv[1], L"-FL", wcslen(argv[1])) == 0 )
        {
          //ignore
        }
        else if (_wcsnicmp(argv[1], L"-DXR", wcslen(argv[1])) == 0)
        {
        }
    }
}

// Update the application state with the new resolution.
void D3D12PathTracing::UpdateForSizeChange(UINT width, UINT height)
{
    DXSample::UpdateForSizeChange(width, height);
}

// Create resources that are dependent on the size of the main window.
void D3D12PathTracing::CreateWindowSizeDependentResources()
{
    CreateRaytracingOutputResource(); 
    
  //TODO
  //UpdateCameraMatrices();
}

// Release resources that are dependent on the size of the main window.
void D3D12PathTracing::ReleaseWindowSizeDependentResources()
{
    raytracing_manager.GetRaytracedUAVOutputResource().Reset();
}

// Release all resources that depend on the device.
void D3D12PathTracing::ReleaseDeviceDependentResources()
{
    raytracing_manager.Release();
}

void D3D12PathTracing::RecreateD3D()
{
    // Give GPU a chance to finish its execution in progress.
    try
    {
        raytracing_manager.GetDeviceResoures()->WaitForGpu();
    }
    catch (HrException&)
    {
        // Do nothing, currently attached adapter is unresponsive.
    }
    raytracing_manager.GetDeviceResoures()->HandleDeviceLost();
}

// Render the scene.
void D3D12PathTracing::OnRender()
{
    if (!raytracing_manager.GetDeviceResoures()->IsWindowVisible())
    {
        return;
    }

    raytracing_manager.GetDeviceResoures()->Prepare();
    DoRaytracing();
    CopyRaytracingOutputToBackbuffer();

    raytracing_manager.GetDeviceResoures()->Present(D3D12_RESOURCE_STATE_PRESENT);
}

void D3D12PathTracing::OnDestroy()
{
    // Let GPU finish before releasing D3D resources.
    raytracing_manager.GetDeviceResoures()->WaitForGpu();
    OnDeviceLost();
}

// Release all device dependent resouces when a device is lost.
void D3D12PathTracing::OnDeviceLost()
{
    ReleaseWindowSizeDependentResources();
    ReleaseDeviceDependentResources();
}

// Create all device dependent resources when a device is restored.
void D3D12PathTracing::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

// Compute the average frames per second and million rays per second.
void D3D12PathTracing::CalculateFrameStats()
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

        if (*raytracing_manager.GetRayTracingApi() == RaytracingApiType::Fallback)
        {
          //TODO
            if (raytracing_manager.GetFallbackDevice()->UsingRaytracingDriver())
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
            << L"    GPU[" << raytracing_manager.GetDeviceResoures()->GetAdapterID() << L"]: " << raytracing_manager.GetDeviceResoures()->GetAdapterDescription();
        SetCustomWindowText(windowText.str().c_str());
    }
}

// Handle OnSizeChanged message event.
void D3D12PathTracing::OnSizeChanged(UINT width, UINT height, bool minimized)
{
    if (!raytracing_manager.GetDeviceResoures()->WindowSizeChanged(width, height, minimized))
    {
        return;
    }

    UpdateForSizeChange(width, height);

    ReleaseWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

// Create a wrapped pointer for the Fallback Layer path.
WRAPPED_GPU_POINTER D3D12PathTracing::CreateFallbackWrappedPointer(ID3D12Resource* resource, UINT bufferNumElements)
{
    auto device = raytracing_manager.GetDeviceResoures()->GetD3DDevice();

    D3D12_UNORDERED_ACCESS_VIEW_DESC rawBufferUavDesc = {};
    rawBufferUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    rawBufferUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    rawBufferUavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    rawBufferUavDesc.Buffer.NumElements = bufferNumElements;

    D3D12_CPU_DESCRIPTOR_HANDLE bottomLevelDescriptor;
   
    // Only compute fallback requires a valid descriptor index when creating a wrapped pointer.
    UINT descriptorHeapIndex = 0;
    if (!raytracing_manager.GetFallbackDevice()->UsingRaytracingDriver())
    {
        descriptorHeapIndex = AllocateDescriptor(&bottomLevelDescriptor);
        device->CreateUnorderedAccessView(resource, nullptr, &rawBufferUavDesc, bottomLevelDescriptor);
    }
    return raytracing_manager.GetFallbackDevice()->GetWrappedPointerSimple(descriptorHeapIndex, resource->GetGPUVirtualAddress());
}

// Allocate a descriptor and return its index. 
// If the passed descriptorIndexToUse is valid, it will be used instead of allocating a new one.
// UINT D3D12PathTracing::AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse)
// {
//     auto descriptorHeapCpuBase = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
//     if (descriptorIndexToUse >= m_descriptorHeap->GetDesc().NumDescriptors)
//     {
//         descriptorIndexToUse = m_descriptorsAllocated++;
//     }
//     *cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, m_descriptorSize);
//     return descriptorIndexToUse;
// }

// Create SRV for a buffer.
// UINT D3D12PathTracing::CreateBufferSRV(D3DBuffer* buffer, UINT numElements, UINT elementSize)
// {
//     auto device = raytracing_manager.GetDeviceResoures()->GetD3DDevice();
//
//     // SRV
//     D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
//     srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
//     srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
//     srvDesc.Buffer.NumElements = numElements;
//     if (elementSize == 0)
//     {
//         srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
//         srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
//         srvDesc.Buffer.StructureByteStride = 0;
//     }
//     else
//     {
//         srvDesc.Format = DXGI_FORMAT_UNKNOWN;
//         srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
//         srvDesc.Buffer.StructureByteStride = elementSize;
//     }
//     UINT descriptorIndex = AllocateDescriptor(&buffer->cpuDescriptorHandle);
//     device->CreateShaderResourceView(buffer->resource.Get(), &srvDesc, buffer->cpuDescriptorHandle);
//     buffer->gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, m_descriptorSize);
//     return descriptorIndex;
// }