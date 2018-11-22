#include "stdafx.h"
#include "ResourceHelper.h"

/*
 * Want to use this for data that doesn't change frequently, ex textures
 */
llvm::Expected<ComPtr<ID3D12Resource>> ResourceHelper::UploadResourceToGPUSync(
    ComPtr<ID3D12Device> device, CommandQueue *command_queue,
    std::size_t num_elements, std::size_t element_size, void *data) {
  if (auto command_list = command_queue->GetCommandList()) {
    // make sure command list is reset to make sure it is able to allocate
    // commands command_list->Reset()

    if (data == nullptr) {
      return llvm::createStringError("Data should not be null");
    }

    if (!num_elements) {
      return llvm::createStringError("Num elements should not be zero");
    }

    if (!element_size) {
      return llvm::createStringError("Element size should not be zero");
    }

    ComPtr<ID3D12Resource> gpu_resource_buffer = nullptr;
    ComPtr<ID3D12Resource> cpu_resource_buffer = nullptr;

    const std::size_t buffer_size = num_elements * element_size;

    // Create default heap on GPU
    LLVMThrowIfFailed(device->CreateCommittedResource(
                          &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                          D3D12_HEAP_FLAG_NONE,
                          &CD3DX12_RESOURCE_DESC::Buffer(buffer_size),
                          D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                          IID_PPV_ARGS(&gpu_resource_buffer)),
                      "Failed to commit resource to the default heap");

    // Create upload heap on CPU
    LLVMThrowIfFailed(device->CreateCommittedResource(
                          &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                          D3D12_HEAP_FLAG_NONE,
                          &CD3DX12_RESOURCE_DESC::Buffer(buffer_size),
                          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                          IID_PPV_ARGS(&cpu_resource_buffer)),
                      "Failed to commit resource to the upload heap");

    D3D12_SUBRESOURCE_DATA subresource_data{};
    subresource_data.pData = data;
    subresource_data.RowPitch = buffer_size;
    subresource_data.SlicePitch = element_size;

    // upload copy data to CPU upload heap and then copy data to GPU default
    // heap
    UpdateSubresources(command_list->Get(), gpu_resource_buffer.Get(),
                       cpu_resource_buffer.Get(), 0, 0, 1, &subresource_data);

    // execute the upload
    if (auto fence_value =
            command_queue->ExecuteCommandList(command_list->Get())) {
      command_queue->WaitForFence(*fence_value);
    } else {
      return fence_value.takeError();
    }

    return gpu_resource_buffer;
  } else {
    return command_list.takeError();
  }
}

void ResourceHelper::TransitionResource(
    ComPtr<ID3D12GraphicsCommandList> command_list,
    ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after) {

  CD3DX12_RESOURCE_BARRIER barrier =
      CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), before, after);

  command_list->ResourceBarrier(1, &barrier);
}

ComPtr<ID3D12Resource> ResourceHelper::CreateBuffer(
    ComPtr<ID3D12Device> device, UINT size,
    D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES &heap_properties, std::wstring resource_name,
  D3D12_RESOURCE_FLAGS flags) {
  CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size, flags);
  
  ComPtr<ID3D12Resource> resource;
  device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &desc,
                                  state, nullptr, IID_PPV_ARGS(&resource));

  if (!resource_name.empty())
  {
    resource->SetName(resource_name.c_str());
  }

  return resource;
}
