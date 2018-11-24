#include <utility>
#pragma once

enum class ResourceType {
  ConstantBufferView,
  ShaderResourceView,
  UnorderedAccessView,
};

using Serializer = std::function<ComPtr<ID3D12Resource>()>;

struct IRootResource {
  virtual ~IRootResource() = default;

  explicit IRootResource(Serializer serializer)
      : serializer(std::move(serializer)) {}

  virtual ComPtr<ID3D12Resource> Serialize() {
    if (resource == nullptr) {
      resource = serializer();
    }
    return resource;
  }
  ComPtr<ID3D12Resource> resource = nullptr;
  ResourceType resource_type;
  Serializer serializer = nullptr;
  CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle;
  CD3DX12_GPU_DESCRIPTOR_HANDLE gpu_handle;
};

struct RootResourceConstantBufferView : IRootResource {
  explicit RootResourceConstantBufferView(Serializer serializer)
      : IRootResource(serializer)
  {
    resource_type = ResourceType::ConstantBufferView;
    //TODO
    //desc.BufferLocation
  }
  virtual ~RootResourceConstantBufferView() = default;
  
  //TODO FIGURE OUT A WAY TO FORMAT THE DESC
  D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
};

struct RootResourceShaderResourceView : IRootResource {
  explicit RootResourceShaderResourceView(Serializer serializer)
      : IRootResource(serializer)
  {
    resource_type = ResourceType::ShaderResourceView;
  }
  virtual ~RootResourceShaderResourceView() = default;
  D3D12_SHADER_RESOURCE_VIEW_DESC desc;
};

struct RootResourceUnorderedAccessView : IRootResource {
  explicit RootResourceUnorderedAccessView(Serializer serializer)
      : IRootResource(serializer)
  {
    resource_type = ResourceType::ShaderResourceView;
  }
  virtual ~RootResourceUnorderedAccessView() = default;
  D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
};

struct IRootParameter {
  virtual ~IRootParameter() = default;
  virtual CD3DX12_ROOT_PARAMETER GetRootParameter() = 0;
};

struct RootConstant : IRootParameter {
  // example
  /*
   * struct DrawConstants
    {
        uint foo;
        float2 bar;
        int moo;
    };
    ConstantBuffer<DrawConstants> myDrawConstants : register(b1, space0);
   */
  virtual ~RootConstant() = default;

  explicit RootConstant(UINT number_of_32_bytes, UINT shader_register,
                        UINT register_space,
                        D3D12_SHADER_VISIBILITY shader_visibility)
      : number_of_32bytes(number_of_32_bytes), shader_register(shader_register),
        register_space(register_space), shader_visibility(shader_visibility) {}

  CD3DX12_ROOT_PARAMETER GetRootParameter() override
  {
    CD3DX12_ROOT_PARAMETER root_parameter;
    root_parameter.InitAsConstants(number_of_32bytes, shader_register,
                                   register_space, shader_visibility);
    return root_parameter;   
  }

  UINT number_of_32bytes;
  UINT shader_register;
  UINT register_space = 0;
  D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL;

  std::shared_ptr<IRootResource> root_resource = nullptr;
};

struct RootDescriptor : IRootParameter {
  // inlined
  // no out of bounds
  // limited to CBV, SRV, UAV

  virtual ~RootDescriptor() = default;

  explicit RootDescriptor(UINT shader_register, UINT register_space,
                         D3D12_SHADER_VISIBILITY shader_visibility)
      : shader_register(shader_register), register_space(register_space),
        shader_visibility(shader_visibility) {}

  CD3DX12_ROOT_PARAMETER GetRootParameter() override
  {
    CD3DX12_ROOT_PARAMETER root_parameter;
    //TODO use the resource to figure out what kind of view it is
    //root_parameter.InitAsUnorderedAccessView(number_of_32bytes, shader_register,
    //                               register_space, shader_visibility);
    return root_parameter;   
  }

  ResourceType GetResourceType()
  {
    return root_resource->resource_type;
  }

  //only if this is part of a root descriptor table
  D3D12_DESCRIPTOR_RANGE_TYPE GetDescriptorRangeType()
  {
    D3D12_DESCRIPTOR_RANGE_TYPE descriptor_range_type;
    switch (GetResourceType()) { 
      case ResourceType::ConstantBufferView: 
      {
        return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
      }
      case ResourceType::ShaderResourceView:
      {
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
      }
      case ResourceType::UnorderedAccessView: {
        return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
      }
    }
  }

  UINT shader_register;
  UINT register_space = 0;
  D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL;

  std::shared_ptr<IRootResource> root_resource;
};
struct RootDescriptorTable : IRootParameter {
  virtual ~RootDescriptorTable() = default;

  CD3DX12_ROOT_PARAMETER GetRootParameter() override 
  {
    std::vector<CD3DX12_DESCRIPTOR_RANGE> ranges;
    ranges.reserve(descriptors.size());

    for (std::shared_ptr<RootDescriptor> descriptor : descriptors)
    {
      CD3DX12_DESCRIPTOR_RANGE range;
      range.Init(descriptor->GetDescriptorRangeType(), 1,
                 descriptor->shader_register, descriptor->register_space);
      ranges.emplace_back(std::move(range));
    }

    CD3DX12_ROOT_PARAMETER root_parameter;
    root_parameter.InitAsDescriptorTable(ranges.size(), ranges.data());

    return root_parameter;
  }

  std::vector<std::shared_ptr<RootDescriptor>> descriptors;
};

struct RootSignatureDesc {
  D3D12_ROOT_SIGNATURE_DESC desc = {};
  std::map<CD3DX12_ROOT_PARAMETER *, CD3DX12_DESCRIPTOR_RANGE> ranges;
  std::vector<CD3DX12_ROOT_PARAMETER> root_parameters;

  void AllocateRootDescriptorTable(
      D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL) {
    // TODO update this
    CD3DX12_ROOT_PARAMETER root_parameter;
    CD3DX12_DESCRIPTOR_RANGE r[1];

    root_parameter.InitAsDescriptorTable(1, &r[0], shader_visibility);
    root_parameters.emplace_back(root_parameter);
  }
  void SerializeAndCreateRaytracingRootSignature(
      ComPtr<ID3D12Device> device, ComPtr<ID3D12RootSignature> *rootSig) {
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    // TODO add llvm expected
    ThrowIfFailed(
        D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob,
                                    &error),
        error ? static_cast<wchar_t *>(error->GetBufferPointer()) : nullptr);
    ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(),
                                              blob->GetBufferSize(),
                                              IID_PPV_ARGS(&(*rootSig))));
  }

  void SerializeAndCreateFallbackRaytracingRootSignature(
      ComPtr<ID3D12RaytracingFallbackDevice> device,
      ComPtr<ID3D12RootSignature> *rootSig) {
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    ThrowIfFailed(
        device->D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                            &blob, &error),
        error ? static_cast<wchar_t *>(error->GetBufferPointer()) : nullptr);
    ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(),
                                              blob->GetBufferSize(),
                                              IID_PPV_ARGS(&(*rootSig))));
  }
};
class RootAllocator {
public:
  std::vector<std::shared_ptr<IRootParameter>> root_parameters;

  std::shared_ptr<RootConstant> AllocateRootConstant(
      UINT number_of_32bytes, UINT shader_register, UINT register_space = 0,
      D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL) {
    auto root_parameter = std::make_shared<RootConstant>(
        number_of_32bytes, shader_register, register_space, shader_visibility);
    root_parameters.emplace_back(root_parameter);
    return root_parameter;
  }

  std::shared_ptr<RootDescriptor> AllocateRootDescriptor(
      UINT shader_register, UINT register_space = 0,
      D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL) {
    auto root_parameter = std::make_shared<RootDescriptor>(
        shader_register, register_space, shader_visibility);
    root_parameters.emplace_back(root_parameter);
    return root_parameter;
  }

  std::shared_ptr<RootDescriptorTable> AllocateRootDescriptorTable() {
    auto root_parameter = std::make_shared<RootDescriptorTable>();
    root_parameters.emplace_back(root_parameter);
    return root_parameter;
  }

  void Serialize(std::shared_ptr<DX::DeviceResources> device_resources) {
    auto device = device_resources->GetD3DDevice();
    auto command_list = device_resources->GetCommandList();
    auto command_allocator = device_resources->GetCommandAllocator();

    // reset the command list
    command_list->Reset(command_allocator, nullptr);

    UINT root_parameter_index = 0;
    std::vector<CD3DX12_ROOT_PARAMETER> d3d12_root_parameter;
    d3d12_root_parameter.resize(root_parameters.size());
    for (std::shared_ptr<IRootParameter> root_parameter : root_parameters) {
      if (auto root_constant =
              std::dynamic_pointer_cast<RootConstant>(root_parameter)) {

      } else if (auto root_desciptor =
                     std::dynamic_pointer_cast<RootDescriptor>(root_parameter)) {
        ComPtr<ID3D12Resource> resource =
            root_desciptor->root_resource->Serialize();

        // TODO
      } else if (auto root_descriptor_table = std::dynamic_pointer_cast<RootDescriptorTable>(root_parameter)) {
        auto table_descriptor = root_descriptor_table->descriptors;

        ComPtr<ID3D12DescriptorHeap> descriptor_heap = ResourceHelper::CreateDescriptorHeap(device, table_descriptor.size());
        UINT descriptor_heap_increment = device->GetDescriptorHandleIncrementSize(descriptor_heap->GetDesc().Type);
        UINT descriptor_heap_index = 0;

        for (std::shared_ptr<IRootParameter> table_parameter : table_descriptor) {
          if (auto table_root_desciptor = std::dynamic_pointer_cast<RootDescriptor>(root_parameter)) {
            std::shared_ptr<IRootResource> root_resource = table_root_desciptor->root_resource;
            ComPtr<ID3D12Resource> resource = root_resource->Serialize();

            root_resource->cpu_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
                descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
                descriptor_heap_index, descriptor_heap_increment);

            root_resource->gpu_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
                descriptor_heap->GetGPUDescriptorHandleForHeapStart(),
                descriptor_heap_index, descriptor_heap_increment);

            if (auto cbv =
                    std::dynamic_pointer_cast<RootResourceConstantBufferView>(
                        root_resource)) {
              device->CreateConstantBufferView(&cbv->desc,
                                               root_resource->cpu_handle);
            } else if (auto srv = std::dynamic_pointer_cast<
                           RootResourceShaderResourceView>(root_resource)) {
              device->CreateShaderResourceView(resource.Get(), &srv->desc,
                                               root_resource->cpu_handle);
            } else if (auto uav = std::dynamic_pointer_cast<
                           RootResourceUnorderedAccessView>(root_resource)) {
              // TODO
              // device->CreateUnorderedAccessView(resource.Get(), &srv->desc,
              // root_resource->cpu_handle);
            }
            descriptor_heap_index++;
          } else {
            llvm::ExitOnError(
                "Other types should be be in the descriptor table");
          }
        }
      } else {
        llvm::ExitOnError("Root type not parsable");
      }
    }

    for (std::shared_ptr<IRootParameter> root_parameter : root_parameters) {
      if (auto root_constant =
              std::dynamic_pointer_cast<RootConstant>(root_parameter)) {
        ComPtr<ID3D12Resource> resource =
            root_constant->root_resource->Serialize();
        // command_list->SetComputeRoot32BitConstant(root_parameter_index, )

        // TODO
      } else if (auto root_desciptor =
                     std::dynamic_pointer_cast<RootDescriptor>(root_parameter)) {
        ComPtr<ID3D12Resource> resource =
            root_desciptor->root_resource->Serialize();

        // TODO
      } else if (auto root_descriptor_table =
                     std::dynamic_pointer_cast<RootDescriptorTable>(
                         root_parameter)) {
        auto table_root_parameters = root_descriptor_table->root_parameters;

        ComPtr<ID3D12DescriptorHeap> descriptor_heap =
            ResourceHelper::CreateDescriptorHeap(device,
                                                 table_root_parameters.size());
        UINT descriptor_heap_increment =
            device->GetDescriptorHandleIncrementSize(
                descriptor_heap->GetDesc().Type);
        UINT descriptor_heap_index = 0;

        for (std::shared_ptr<IRootParameter> table_parameter :
             table_root_parameters) {
          if (auto table_root_constant =
                  std::dynamic_pointer_cast<RootConstant>(root_parameter)) {
            // TODO
            std::shared_ptr<IRootResource> root_resource =
                table_root_constant->root_resource;
            ComPtr<ID3D12Resource> resource = root_resource->Serialize();
            if (auto cbv =
                    std::dynamic_pointer_cast<RootResourceConstantBufferView>(
                        root_resource)) {

            } else if (auto srv = std::dynamic_pointer_cast<
                           RootResourceShaderResourceView>(root_resource)) {

            } else if (auto uav = std::dynamic_pointer_cast<
                           RootResourceUnorderedAccessView>(root_resource)) {
            }
          } else if (auto table_root_desciptor =
                         std::dynamic_pointer_cast<RootDescriptor>(
                             root_parameter)) {
            std::shared_ptr<IRootResource> root_resource =
                table_root_desciptor->root_resource;
            ComPtr<ID3D12Resource> resource = root_resource->Serialize();

            root_resource->cpu_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
                descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
                descriptor_heap_index, descriptor_heap_increment);

            root_resource->gpu_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
                descriptor_heap->GetGPUDescriptorHandleForHeapStart(),
                descriptor_heap_index, descriptor_heap_increment);

            if (auto cbv =
                    std::dynamic_pointer_cast<RootResourceConstantBufferView>(
                        root_resource)) {
              device->CreateConstantBufferView(&cbv->desc,
                                               root_resource->cpu_handle);
            } else if (auto srv = std::dynamic_pointer_cast<
                           RootResourceShaderResourceView>(root_resource)) {
              device->CreateShaderResourceView(resource.Get(), &srv->desc,
                                               root_resource->cpu_handle);
            } else if (auto uav = std::dynamic_pointer_cast<
                           RootResourceUnorderedAccessView>(root_resource)) {
              // TODO
              // device->CreateUnorderedAccessView(resource.Get(), &srv->desc,
              // root_resource->cpu_handle);
            }
            descriptor_heap_index++;
          } else {
            llvm::ExitOnError(
                "Other types should be be in the descriptor table");
          }
        }
      } else {
        llvm::ExitOnError("Root type not parsable");
      }
    }
  }
  /*
  void AllocateRootConstant(
      UINT number_of_32bytes, UINT shader_register, UINT register_space = 0,
      D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL) {
    CD3DX12_ROOT_PARAMETER root_parameter;
    root_parameter.InitAsConstants(number_of_32bytes, shader_register,
                                   register_space, shader_visibility);
    root_parameters.emplace_back(root_parameter);
  }

  void AllocateRootDescriptorConstantBufferView(
      UINT shader_register, UINT register_space = 0,
      D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL) {
    CD3DX12_ROOT_PARAMETER root_parameter;
    root_parameter.InitAsConstantBufferView(shader_register, register_space,
                                            shader_visibility);
    root_parameters.emplace_back(root_parameter);
  }

  void AllocateRootDescriptorShaderResourceView(
      UINT shader_register, UINT register_space = 0,
      D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL) {
    CD3DX12_ROOT_PARAMETER root_parameter;
    root_parameter.InitAsShaderResourceView(shader_register, register_space,
                                            shader_visibility);
    root_parameters.emplace_back(root_parameter);
  }

  void AllocateRootDescriptorUnorderedAccessView(
      UINT shader_register, UINT register_space = 0,
      D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL) {
    CD3DX12_ROOT_PARAMETER root_parameter;
    root_parameter.InitAsUnorderedAccessView(shader_register, register_space,
                                             shader_visibility);
    root_parameters.emplace_back(root_parameter);
  }
  */
};
