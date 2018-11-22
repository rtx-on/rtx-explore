#pragma once

using Microsoft::WRL::ComPtr;

class CommandQueue  : public llvm::ErrorInfo<CommandQueue>
{
public:
  explicit CommandQueue(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE command_list_type);

  llvm::Expected<bool> Initialize();

  llvm::Expected<ComPtr<ID3D12GraphicsCommandList>> GetCommandList();
  ComPtr<ID3D12CommandQueue> GetCommandQueue() const;

  llvm::Expected<ComPtr<ID3D12CommandAllocator>> CreateCommandAllocator() const;
  llvm::Expected<ComPtr<ID3D12GraphicsCommandList>> CreateCommandList(ComPtr<ID3D12CommandAllocator> allocator) const;

  llvm::Expected<uint64_t> ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList> command_list);

  llvm::Expected<uint64_t> Signal();
  bool IsFenceComplete(uint64_t fence_value) const;
  void WaitForFence(uint64_t fence_value) const;
  void Flush();

private:
  //keep track of command allocators that are executing
  struct CommandAllocatorEntry
  {
    uint64_t fence_value;
    ComPtr<ID3D12CommandAllocator> command_allocator;
  };

  std::queue<CommandAllocatorEntry> command_allocators;
  std::queue<ComPtr<ID3D12GraphicsCommandList>> command_lists;

  ComPtr<ID3D12Device> device = nullptr;
  ComPtr<ID3D12CommandQueue> command_queue = nullptr;
  ComPtr<ID3D12Fence> fence = nullptr;

  D3D12_COMMAND_LIST_TYPE command_list_type;

  HANDLE fence_event = INVALID_HANDLE_VALUE;
  uint64_t start_fence_value = 0;
};
