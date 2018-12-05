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

#pragma once

#include "DXSample.h"
#include "StepTimer.h"
#include "shaders/RaytracingHlslCompat.h"
#include "Scene.h"


namespace GlobalRootSignatureParams {
    enum Value {
        OutputViewSlot = 0,
        AccelerationStructureSlot,
	TextureSlot,
	NormalTextureSlot,
	SceneConstantSlot,
        VertexBuffersSlot,
        IndexBuffersSlot,
        MaterialBuffersSlot,
        InfoBuffersSlot,
        Count 
    };
}

namespace LocalRootSignatureParams {
    enum Value {
        CubeConstantSlot = 0,
        Count 
    };
}

// The sample supports both Raytracing Fallback Layer and DirectX Raytracing APIs. 
// This is purely for demonstration purposes to show where the API differences are. 
// Real-world applications will implement only one or the other. 
// Fallback Layer uses DirectX Raytracing if a driver and OS supports it. 
// Otherwise, it falls back to compute pipeline to emulate raytracing.
// Developers aiming for a wider HW support should target Fallback Layer.
class D3D12RaytracingSimpleLighting : public DXSample
{
    enum class RaytracingAPI {
        FallbackLayer,
        DirectXRaytracing,
    };

public:
    D3D12RaytracingSimpleLighting(UINT width, UINT height, std::wstring name);

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
    virtual void OnInit();
    virtual void OnKeyDown(UINT8 key);
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnSizeChanged(UINT width, UINT height, bool minimized);
    virtual void OnDestroy();
    virtual IDXGISwapChain* GetSwapchain() { return m_deviceResources->GetSwapChain(); }

	// Public variables
	std::string p_sceneFileName;

	UINT CreateBufferSRV(D3DBuffer* buffer, UINT numElements, UINT elementSize);

	UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX);

	ID3D12Resource* GetTextureBufferUploadHeap() {
		return textureBufferUploadHeap;
	}

	ComPtr<ID3D12DescriptorHeap> GetDescriptorHeap() {
		return m_descriptorHeap;
	}

	UINT* GetDescriptorSize() {
		return &m_descriptorSize;
	}

      WRAPPED_GPU_POINTER CreateFallbackWrappedPointer(ID3D12Resource* resource, UINT bufferNumElements);

private:
	static const UINT FrameCount = 3;

    // We'll allocate space for several of these and they will need to be padded for alignment.
    static_assert(sizeof(SceneConstantBuffer) < D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, "Checking the size here.");

    union AlignedSceneConstantBuffer
    {
        SceneConstantBuffer constants;
        uint8_t alignmentPadding[D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
    };
    AlignedSceneConstantBuffer*  m_mappedConstantData;
    ComPtr<ID3D12Resource>       m_perFrameConstants;
        
    // Raytracing Fallback Layer (FL) attributes
    ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice;
    ComPtr<ID3D12RaytracingFallbackCommandList> m_fallbackCommandList;
    ComPtr<ID3D12RaytracingFallbackStateObject> m_fallbackStateObject;
    WRAPPED_GPU_POINTER m_fallbackTopLevelAccelerationStructurePointer;

    // DirectX Raytracing (DXR) attributes
    ComPtr<ID3D12Device5> m_dxrDevice;
    ComPtr<ID3D12GraphicsCommandList5> m_dxrCommandList;
    ComPtr<ID3D12StateObject> m_dxrStateObject;
    bool m_isDxrSupported;

    // Root signatures
    ComPtr<ID3D12RootSignature> m_raytracingGlobalRootSignature;
    ComPtr<ID3D12RootSignature> m_raytracingLocalRootSignature;

    // Descriptors
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
    UINT m_descriptorsAllocated;
    UINT m_descriptorSize;
    
    // Raytracing scene
	Scene* m_sceneLoaded;
    SceneConstantBuffer m_sceneCB[FrameCount];
    CubeConstantBuffer m_cubeCB;


    D3DBuffer m_indexBuffer;
    D3DBuffer m_vertexBuffer;

	D3DBuffer m_textureBuffer;
	D3DBuffer m_normalTextureBuffer;
	ID3D12Resource* textureBufferUploadHeap;

    // Acceleration structure
    ComPtr<ID3D12Resource> m_bottomLevelAccelerationStructure;
    ComPtr<ID3D12Resource> m_topLevelAccelerationStructure;

    // Raytracing output
    ComPtr<ID3D12Resource> m_raytracingOutput;
    D3D12_GPU_DESCRIPTOR_HANDLE m_raytracingOutputResourceUAVGpuDescriptor;
    UINT m_raytracingOutputResourceUAVDescriptorHeapIndex;

    //pathtracing accumulator
    ComPtr<ID3D12Resource> pathtracing_accumulation_resource;
    ComPtr<ID3D12Resource> cure_epilepsy;

    // Shader tables
    static const wchar_t* c_hitGroupName;
    static const wchar_t* c_raygenShaderName;
    static const wchar_t* c_closestHitShaderName;
    static const wchar_t* c_missShaderName;
    ComPtr<ID3D12Resource> m_missShaderTable;
    ComPtr<ID3D12Resource> m_hitGroupShaderTable;
    ComPtr<ID3D12Resource> m_rayGenShaderTable;
    
    // Application state
    RaytracingAPI m_raytracingAPI;
    bool m_forceComputeFallback;
    StepTimer m_timer;

	// Camera state
    XMVECTOR m_eye;
    XMVECTOR m_at;
    XMVECTOR m_up;
	XMVECTOR m_right;
	XMVECTOR m_forward;
	bool m_camChanged;
	static const float c_rotateDegrees;
	static const float c_movementAmountFactor;

	static const unsigned int c_maxIteration;

    void EnableDirectXRaytracing(IDXGIAdapter1* adapter);
    void ParseCommandLineArgs(WCHAR* argv[], int argc);
    void UpdateCameraMatrices();
    void InitializeScene();
    void RecreateD3D();
    void DoRaytracing();
    void CreateConstantBuffers();
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    void ReleaseDeviceDependentResources();
    void ReleaseWindowSizeDependentResources();
    void CreateRaytracingInterfaces();
    void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig);
    void CreateRootSignatures();
    void CreateLocalRootSignatureSubobjects(CD3D12_STATE_OBJECT_DESC* raytracingPipeline);
    void CreateRaytracingPipelineStateObject();
    void CreateDescriptorHeap();
    void CreateRaytracingOutputResource();
    void BuildGeometry();
    void BuildAccelerationStructures();
    void BuildShaderTables();
    void SelectRaytracingAPI(RaytracingAPI type);
    void UpdateForSizeChange(UINT clientWidth, UINT clientHeight);
    void CopyRaytracingOutputToBackbuffer();
    void CalculateFrameStats();

    //IMGUI stuff
#define HEAP_DESCRIPTOR_SIZE (10000)
    ComPtr<ID3D12DescriptorHeap> g_pd3dSrvDescHeap;
    void InitImGUI();
    void StartFrameImGUI();
    void RenderImGUI();
    void ShutdownImGUI();

    int current_imgui_heap_descriptor = 0;
    bool rebuild_scene = false;

    void RebuildScene();
    bool LoadModel(std::string model_path);
    bool LoadDiffuseTexture(std::string diffuse_texture_path);
    bool LoadNormalTexture(std::string normal_texture_path);
    bool MakeEmptyMaterial();
    bool MakeEmptyObject();
    void SerializeToObj();


#define VERTEX_HEAP_OFFSET (1000)
#define INDICIES_HEAP_OFFSET (2000)
#define OBJECTS_HEAP_OFFSET (3000)
#define MATERIALS_HEAP_OFFSET (4000)
#define DIFFUSE_TEXTURES_HEAP_OFFSET (5000)
#define NORMAL_TEXTURES_HEAP_OFFSET (6000)

    int vertex_offset = VERTEX_HEAP_OFFSET;
    int indices_offset = INDICIES_HEAP_OFFSET;
    int objects_offset = OBJECTS_HEAP_OFFSET;
    int materials_offset = MATERIALS_HEAP_OFFSET;
    int diffuse_textures_offset = DIFFUSE_TEXTURES_HEAP_OFFSET;
    int normal_textures_offset = NORMAL_TEXTURES_HEAP_OFFSET;

public:
    int GetHeapOffsetForVertices();
    int GetHeapOffsetForIndices();
    int GetHeapOffsetForObjects();
    int GetHeapOffsetForMaterials();
    int GetHeapOffsetForDiffuseTextures();
    int GetHeapOffsetForNormalTextures();
    void ResetHeapOffsets();

    enum class HeapDescriptorOffsetType
    {
      NONE,
      VERTEX,
      INDICES,
      OBJECTS,
      MATERIALS,
      DIFFUSE_TEXTURES,
      NORMAL_TEXTURES
    };

    int AllocateHeapDescriptorType(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse, HeapDescriptorOffsetType offset_type);
};
