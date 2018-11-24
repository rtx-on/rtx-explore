#include <utility>
#pragma once

enum class ResourceType
{
  ConstantBufferView,
  ShaderResourceView,
  UnorderedAccessView,
};

struct IRootResource;
using Serializer = std::function<ComPtr<ID3D12Resource>(std::shared_ptr<DX::DeviceResources> device_resources, IRootResource* root_resource)>;

struct IRootResource
{
  virtual ~IRootResource() = default;

  explicit IRootResource(Serializer serializer)
    : serializer(std::move(serializer))
  {
  }

  virtual ComPtr<ID3D12Resource> Serialize(std::shared_ptr<DX::DeviceResources> device_resources)
  {
    if (resource == nullptr)
    {
      resource = serializer(device_resources, this);
    }
    return resource;
  }

  ComPtr<ID3D12Resource> resource = nullptr;
  ResourceType resource_type;
  Serializer serializer = nullptr;
  CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle;
  CD3DX12_GPU_DESCRIPTOR_HANDLE gpu_handle;
};

struct RootResourceConstantBufferView : IRootResource
{
  explicit RootResourceConstantBufferView(Serializer serializer)
    : IRootResource(serializer)
  {
    resource_type = ResourceType::ConstantBufferView;
    // TODO
    // desc.BufferLocation
  }

  virtual ~RootResourceConstantBufferView() = default;

  // TODO FIGURE OUT A WAY TO FORMAT THE DESC
  D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
};

struct RootResourceShaderResourceView : IRootResource
{
  explicit RootResourceShaderResourceView(Serializer serializer)
    : IRootResource(serializer)
  {
    resource_type = ResourceType::ShaderResourceView;
  }

  virtual ~RootResourceShaderResourceView() = default;
  D3D12_SHADER_RESOURCE_VIEW_DESC desc;
};

struct RootResourceUnorderedAccessView : IRootResource
{
  explicit RootResourceUnorderedAccessView(Serializer serializer)
    : IRootResource(serializer)
  {
    resource_type = ResourceType::ShaderResourceView;
  }

  virtual ~RootResourceUnorderedAccessView() = default;
  D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
};

struct IRootParameter
{
  virtual ~IRootParameter() = default;
  virtual CD3DX12_ROOT_PARAMETER GetRootParameter() = 0;
};

struct RootConstant : IRootParameter
{
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
      register_space(register_space), shader_visibility(shader_visibility)
  {
  }

  CD3DX12_ROOT_PARAMETER GetRootParameter() override
  {
    CD3DX12_ROOT_PARAMETER root_parameter;
    root_parameter.InitAsConstants(number_of_32bytes, shader_register,
                                   register_space, shader_visibility);
    return root_parameter;
  }

  void SetResource(std::shared_ptr<IRootResource> resource)
  {
    root_resource = resource;
  }

  UINT number_of_32bytes = 0;
  UINT shader_register = 0;
  UINT register_space = 0;
  D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL;

  std::shared_ptr<IRootResource> root_resource = nullptr;
};

struct RootDescriptor : IRootParameter
{
  // inlined
  // no out of bounds
  // limited to CBV, SRV, UAV

  virtual ~RootDescriptor() = default;

  explicit RootDescriptor(UINT shader_register, UINT register_space,
                          D3D12_SHADER_VISIBILITY shader_visibility)
    : shader_register(shader_register), register_space(register_space),
      shader_visibility(shader_visibility)
  {
  }

  CD3DX12_ROOT_PARAMETER GetRootParameter() override
  {
    CD3DX12_ROOT_PARAMETER root_parameter;
    // TODO use the resource to figure out what kind of view it is
    // root_parameter.InitAsUnorderedAccessView(number_of_32bytes,
    // shader_register,
    //                               register_space, shader_visibility);
    return root_parameter;
  }

  ResourceType GetResourceType() { return root_resource->resource_type; }

  // only if this is part of a root descriptor table
  D3D12_DESCRIPTOR_RANGE_TYPE GetDescriptorRangeType()
  {
    D3D12_DESCRIPTOR_RANGE_TYPE descriptor_range_type;
    switch (GetResourceType())
    {
    case ResourceType::ConstantBufferView:
      {
        return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
      }
    case ResourceType::ShaderResourceView:
      {
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
      }
    case ResourceType::UnorderedAccessView:
      {
        return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
      }
    }
  }

  void SetResource(std::shared_ptr<IRootResource> resource)
  {
    root_resource = resource;
  }

  UINT shader_register = 0;
  UINT register_space = 0;
  D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL;

  std::shared_ptr<IRootResource> root_resource = nullptr;
};

struct RootDescriptorTable : IRootParameter
{
  virtual ~RootDescriptorTable() = default;

  std::shared_ptr<RootDescriptor> AllocateDescriptor(
    UINT shader_register, UINT register_space = 0,
    D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL)
  {
    auto root_parameter = std::make_shared<RootDescriptor>(
      shader_register, register_space, shader_visibility);
    descriptors.emplace_back(root_parameter);
    return root_parameter;
  }

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

struct StaticSampler
{
  explicit StaticSampler()
  {
    desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.MipLODBias = 0;
    desc.MaxAnisotropy = 0;
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    desc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = 1.0f;
    desc.ShaderRegister = 0;
    desc.RegisterSpace = 0;
    desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  }

  CD3DX12_STATIC_SAMPLER_DESC desc{};
};

// TODO add 64 DWORD limit on allocation on root descriptor table
constexpr UINT ROOT_CONSTANT_DWORD_SIZE = 1;
constexpr UINT ROOT_DESCRIPTOR_DWORD_SIZE = 2;
constexpr UINT ROOT_DESCRIPTOR_TABLE_DWORD_SIZE = 2;
constexpr UINT ROOT_TABLE_DWORD_LIMIT = 64;

class RootAllocator : public RaytracingDeviceHolder
{
public:
  //
  UINT root_table_dword_size = 0;

  ComPtr<ID3D12RootSignature> d3d12_root_signature;
  std::vector<std::shared_ptr<IRootParameter>> root_parameters;
  std::vector<std::shared_ptr<StaticSampler>> static_samplers;

  std::shared_ptr<RootConstant> AllocateRootConstant(
    UINT number_of_32bytes, UINT shader_register, UINT register_space = 0,
    D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL)
  {
    //TODO fix with llvm
    if (root_table_dword_size > ROOT_TABLE_DWORD_LIMIT)
    {
      throw std::exception();
    }

    root_table_dword_size += ROOT_CONSTANT_DWORD_SIZE;

    auto root_parameter = std::make_shared<RootConstant>(
      number_of_32bytes, shader_register, register_space, shader_visibility);
    root_parameters.emplace_back(root_parameter);
    return root_parameter;
  }

  std::shared_ptr<RootDescriptor> AllocateRootDescriptor(
    UINT shader_register, UINT register_space = 0,
    D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL)
  {
    //TODO fix with llvm
    if (root_table_dword_size > ROOT_TABLE_DWORD_LIMIT)
    {
      throw std::exception();
    }

    root_table_dword_size += ROOT_DESCRIPTOR_DWORD_SIZE;

    auto root_parameter = std::make_shared<RootDescriptor>(
      shader_register, register_space, shader_visibility);
    root_parameters.emplace_back(root_parameter);
    return root_parameter;
  }

  std::shared_ptr<RootDescriptorTable> AllocateRootDescriptorTable()
  {
    //TODO fix with llvm
    if (root_table_dword_size > ROOT_TABLE_DWORD_LIMIT)
    {
      throw std::exception();
    }

    root_table_dword_size += ROOT_DESCRIPTOR_TABLE_DWORD_SIZE;

    auto root_parameter = std::make_shared<RootDescriptorTable>();
    root_parameters.emplace_back(root_parameter);
    return root_parameter;
  }

  std::shared_ptr<StaticSampler> AllocateStaticSamplier()
  {
    auto static_sampler = std::make_shared<StaticSampler>();
    root_parameters.emplace_back(static_sampler);
    return static_sampler;
  }

  void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC desc)
  {

    auto device = device_resources->GetD3DDevice();
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    if (*ray_tracing_api == RaytracingApiType::Fallback)
    {
        ThrowIfFailed(fallback_device->D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
        ThrowIfFailed(fallback_device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&d3d12_root_signature)));
    }
    else // DirectX Raytracing
    {
        ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
        ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&d3d12_root_signature)));
    }
  }

  void Serialize()
  {
    auto device = device_resources->GetD3DDevice();
    auto command_list = device_resources->GetCommandList();
    auto command_allocator = device_resources->GetCommandAllocator();

    // reset the command list (YES?) TODO, need this for allocation cuz command
    // list needs to be reset inorder toa llocate stuff to gpu
    command_list->Reset(command_allocator, nullptr);

    // setup the signature
    UINT root_parameter_index = 0;
    std::vector<CD3DX12_ROOT_PARAMETER> d3d12_root_parameter;
    d3d12_root_parameter.resize(root_parameters.size());
    for (std::shared_ptr<IRootParameter> root_parameter : root_parameters)
    {
      d3d12_root_parameter.emplace_back(root_parameter->GetRootParameter());
    }

    std::vector<CD3DX12_STATIC_SAMPLER_DESC> descs;
    descs.reserve(static_samplers.size());
    for (std::shared_ptr<StaticSampler> static_sampler : static_samplers)
    {
      descs.emplace_back(static_sampler->desc);
    }

    // serialize root signature
    CD3DX12_ROOT_SIGNATURE_DESC root_signature_desc(d3d12_root_parameter.size(), d3d12_root_parameter.data(), descs.size(), descs.data());
    SerializeAndCreateRaytracingRootSignature(root_signature_desc);

    // allocate the descriptor heap, descriptors and resources
    for (std::shared_ptr<IRootParameter> root_parameter : root_parameters)
    {
      if (auto root_constant = std::dynamic_pointer_cast<RootConstant>(root_parameter))
      {
        ComPtr<ID3D12Resource> resource = root_constant->root_resource->Serialize(device_resources);
        // command_list->SetComputeRoot32BitConstant(root_parameter_index, )

        // TODO allocator constant
      }
      else if (auto root_descriptor = std::dynamic_pointer_cast<RootDescriptor>(root_parameter))
      {
        ComPtr<ID3D12Resource> resource = root_descriptor->root_resource->Serialize(device_resources);

        // TODO allocator root descriptor, similiar to how its done in root
        // descriptor table
      }
      else if (auto root_descriptor_table = std::dynamic_pointer_cast<RootDescriptorTable>(root_parameter))
      {
        const auto& table_descriptors = root_descriptor_table->descriptors;

        ComPtr<ID3D12DescriptorHeap> descriptor_heap = ResourceHelper::CreateDescriptorHeap(device, table_descriptors.size());
        const UINT descriptor_heap_increment = device->GetDescriptorHandleIncrementSize(descriptor_heap->GetDesc().Type);
        UINT descriptor_heap_index = 0;

        for (std::shared_ptr<RootDescriptor> table_root_desciptor : table_descriptors)
        {
          std::shared_ptr<IRootResource> root_resource = table_root_desciptor->root_resource;
          ComPtr<ID3D12Resource> resource = root_resource->Serialize(device_resources);

          root_resource->cpu_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
            descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
            descriptor_heap_index, descriptor_heap_increment);

          root_resource->gpu_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
            descriptor_heap->GetGPUDescriptorHandleForHeapStart(),
            descriptor_heap_index, descriptor_heap_increment);

          if (auto cbv = std::dynamic_pointer_cast<RootResourceConstantBufferView>(root_resource))
          {
            device->CreateConstantBufferView(&cbv->desc, root_resource->cpu_handle);
          }
          else if (auto srv = std::dynamic_pointer_cast<RootResourceShaderResourceView>(root_resource))
          {
            device->CreateShaderResourceView(resource.Get(), &srv->desc, root_resource->cpu_handle);
          }
          else if (auto uav = std::dynamic_pointer_cast<RootResourceUnorderedAccessView>(root_resource))
          {
            // TODO uav needs another resource
            // device->CreateUnorderedAccessView(resource.Get(), &srv->desc,
            // root_resource->cpu_handle);
          }
          descriptor_heap_index++;
        }
      }
      else
      {
        llvm::ExitOnError("Root type not parsable");
      }
    }
  }

};
