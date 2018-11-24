#include "stdafx.h"
#include "CommandQueue.h"

CommandQueue::CommandQueue(ComPtr<ID3D12Device5> device, D3D12_COMMAND_LIST_TYPE command_list_type)
    : device(device),
      command_list_type(command_list_type)
{
}

llvm::Expected<bool> CommandQueue::Initialize()
{
  D3D12_COMMAND_QUEUE_DESC desc{};
  desc.Type = command_list_type;
  desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
  desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  desc.NodeMask = 0;

  LLVMThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue)),
                      "Failed to create command queue");

  LLVMThrowIfFailed(device->CreateFence(start_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)),
                      "Failed to create fence");

  fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (fence_event == INVALID_HANDLE_VALUE)
  {
    return llvm::createStringError("Failed to create fence event");
  }

  return true;
}

llvm::Expected<ComPtr<ID3D12GraphicsCommandList>>
CommandQueue::GetCommandList() {
  ComPtr<ID3D12CommandAllocator> command_allocator;
  ComPtr<ID3D12GraphicsCommandList> command_list;

  //check if the front allocator is done and if so, reset it
  //else create a new allocator
  if (!command_allocators.empty() && !IsFenceComplete(command_allocators.front().fence_value))
  {
    command_allocator = command_allocators.front().command_allocator;
    command_allocators.pop();

    LLVMThrowIfFailed(command_allocator->Reset(),
                     "Failed to reset command allocator");
  } 
  else
  {
    if(auto command_allocator_ = CreateCommandAllocator())
    {
      command_allocator = *command_allocator_;
    } 
    else 
    {
      return command_allocator_.takeError();
    }
  }

  //if command list isn't empty, reset the front command list as well
  if (!command_lists.empty()) {
    command_list = command_lists.front();
    command_lists.pop();

    command_list->Reset(command_allocator.Get(), nullptr);
  } 
  else
  {
    if(auto command_list_ = CreateCommandList(command_allocator))
    {
      command_list = *command_list_;
    }
    else
    {
      return command_list_.takeError();
    }
  }

  //associate command list with command allocator
  LLVMThrowIfFailed(command_list->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), command_allocator.Get()),
                   "Failed to set command list private data interface with command allocator");

  return command_list;
}

ComPtr<ID3D12CommandQueue> CommandQueue::GetCommandQueue() const {
  return command_queue;
}

ComPtr<ID3D12Device5> CommandQueue::GetDevice() const {
  return device;
}

llvm::Expected<ComPtr<ID3D12CommandAllocator>> CommandQueue::CreateCommandAllocator() const
{
  ComPtr<ID3D12CommandAllocator> command_allocator;

  LLVMThrowIfFailed(device->CreateCommandAllocator(command_list_type, IID_PPV_ARGS(&command_allocator)),
                   "Failed to make command allocator");

  return command_allocator;
}

llvm::Expected<ComPtr<ID3D12GraphicsCommandList>>
CommandQueue::CreateCommandList(ComPtr<ID3D12CommandAllocator> allocator) const
{
  ComPtr<ID3D12GraphicsCommandList> command_list;

  LLVMThrowIfFailed(device->CreateCommandList(0, command_list_type, allocator.Get(), nullptr, IID_PPV_ARGS(&command_list)), 
                   "Failed to create command list");

  return command_list;
}

llvm::Expected<uint64_t>
CommandQueue::ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList> command_list) {

  //finish recording the command list
  command_list->Close();

  ID3D12CommandAllocator* command_allocator = nullptr;
  UINT command_allocator_size = sizeof(ID3D12CommandAllocator);

  //fetch the command allocator from the command's private data
  LLVMThrowIfFailed(command_list->GetPrivateData(__uuidof(ID3D12CommandAllocator), &command_allocator_size, &command_allocator),
                      "Failed to get command allocator from command list's private data");
  
  ID3D12CommandList* const all_command_lists[] = { command_list.Get() };

  //execute the command list in the command queue (GPU)
  command_queue->ExecuteCommandLists(1, all_command_lists);

  //tell the command queue to send a signal
  if (auto fence_value = Signal())
  {
    //insert signal into command allocator queue and insert command list into command lists queue
    command_allocators.emplace(CommandAllocatorEntry{*fence_value, command_allocator});
    command_lists.push(command_list);

    //release the command allocator pointer because the pointer has ownership transferred to the CommandAllocatorEntry struct
    command_allocator->Release(); 

    //return the fence value associated with the signal
    return fence_value;
  }
  else
  {
     return fence_value.takeError();
  }
}

llvm::Expected<uint64_t> CommandQueue::Signal()
{
  //increment start fence value
  start_fence_value++;

  //tell command queue to signal back the fence value when done
  LLVMThrowIfFailed(command_queue->Signal(fence.Get(), start_fence_value), 
                    "Failed to signal command queue with fence values");

  return start_fence_value;
}

bool CommandQueue::IsFenceComplete(uint64_t fence_value) const
{
  //check if the value from the fence has been updated, fence_value here is the previous value
  return fence->GetCompletedValue() >= fence_value;
}

void CommandQueue::WaitForFence(uint64_t fence_value) const
{
  //check if fence has already arrived
  if(!IsFenceComplete(fence_value))
  {
    //if not, make the event wait for when the value gets updated
    fence->SetEventOnCompletion(fence_value, fence_event);
    WaitForSingleObject(fence_event, INFINITE);
  }
}

void CommandQueue::Flush()
{
  //signal and wait for fence to complete
  if (auto fence_value = Signal())
  {
    WaitForFence(*fence_value);
  }
  else
  {
    //unreachable
    llvm::handleAllErrors(fence_value.takeError());
  }
}
