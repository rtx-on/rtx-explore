#pragma once

using Microsoft::WRL::ComPtr;

class ResourceHelper {
public:
  static llvm::Expected<ComPtr<ID3D12Resource>>
  UploadResourceToGPUSync(ComPtr<ID3D12Device> device, CommandQueue* command_queue,
                          std::size_t num_elements, std::size_t element_size,
                          void* data);
  
  static void TransitionResource(ComPtr<ID3D12GraphicsCommandList> command_list,
                                 ComPtr<ID3D12Resource> resource,
                                 D3D12_RESOURCE_STATES before,
                                 D3D12_RESOURCE_STATES after);

  static ComPtr<ID3D12Resource>
  CreateBuffer(ComPtr<ID3D12Device> device, UINT size, D3D12_RESOURCE_STATES state,
               const D3D12_HEAP_PROPERTIES &heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), std::wstring resource_name = L"",
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

  static ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device> device, UINT num_descriptors, D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
  {
    ComPtr<ID3D12DescriptorHeap> descriptor_heap;
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    // Allocate a heap for 3 descriptors:
    // 2 - bottom and top level acceleration structure fallback wrapped pointers
    // 1 - raytracing output texture SRV
    descriptorHeapDesc.NumDescriptors = num_descriptors; 
    descriptorHeapDesc.Type = type;
    descriptorHeapDesc.Flags = flags;
    descriptorHeapDesc.NodeMask = 0;
    device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptor_heap));
    NAME_D3D12_OBJECT(descriptor_heap);

    //descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    return descriptor_heap;
  }

};


