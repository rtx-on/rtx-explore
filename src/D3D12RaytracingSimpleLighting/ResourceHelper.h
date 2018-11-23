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
               const D3D12_HEAP_PROPERTIES &heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),std::wstring resource_name = L"",
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
};


