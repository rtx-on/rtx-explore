#pragma once

struct RootParameter
{
  CD3DX12_ROOT_PARAMETER root_parameter;
};

struct RootConstant : RootParameter
{
  //example
  /*
   * struct DrawConstants
    {
        uint foo;
        float2 bar;
        int moo;
    };
    ConstantBuffer<DrawConstants> myDrawConstants : register(b1, space0);
   */
  CD3DX12_ROOT_CONSTANTS root_constant;
};

struct RootDesciptor : RootParameter
{
  //inlined
  //no out of bounds
  //limited to CBV, SRV, UAV
  CD3DX12_ROOT_DESCRIPTOR root_descriptor;
};

struct RootDescriptorTable : RootParameter
{
  D3D12_DESCRIPTOR_RANGE range;

};

struct RootSignatureDesc
{
  D3D12_ROOT_SIGNATURE_DESC desc = {};
  std::map<CD3DX12_ROOT_PARAMETER*, CD3DX12_DESCRIPTOR_RANGE> ranges;
  std::vector<CD3DX12_ROOT_PARAMETER> root_parameters;

  void AllocateRootConstant(UINT number_of_32bytes, UINT shader_register, UINT register_space = 0, D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL)
  { 
    CD3DX12_ROOT_PARAMETER root_parameter;
    root_parameter.InitAsConstants(number_of_32bytes, shader_register, register_space, shader_visibility);
    root_parameters.emplace_back(root_parameter);
  }

  void AllocateRootDescriptorConstantBufferView(UINT shader_register, UINT register_space = 0, D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL)
  {
    CD3DX12_ROOT_PARAMETER root_parameter;
    root_parameter.InitAsConstantBufferView(shader_register, register_space, shader_visibility);
    root_parameters.emplace_back(root_parameter);
  }

  void AllocateRootDescriptorShaderResourceView(UINT shader_register, UINT register_space = 0, D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL)
  {
    CD3DX12_ROOT_PARAMETER root_parameter;
    root_parameter.InitAsShaderResourceView(shader_register, register_space, shader_visibility);
    root_parameters.emplace_back(root_parameter);
  }

  void AllocateRootDescriptorUnorderedAccessView(UINT shader_register, UINT register_space = 0, D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL)
  {
    CD3DX12_ROOT_PARAMETER root_parameter;
    root_parameter.InitAsUnorderedAccessView(shader_register, register_space, shader_visibility);
    root_parameters.emplace_back(root_parameter);
  }
  
  void AllocateRootDescriptorTable(D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL)
  {
    //TODO update this
    CD3DX12_ROOT_PARAMETER root_parameter;
    CD3DX12_DESCRIPTOR_RANGE r[1];

    root_parameter.InitAsDescriptorTable(1, &r[0], shader_visibility);
    root_parameters.emplace_back(root_parameter);
  }
  void SerializeAndCreateRaytracingRootSignature(ComPtr<ID3D12Device> device, ComPtr<ID3D12RootSignature>* rootSig)
  {
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    //TODO add llvm expected
        ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
        ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
  }

  void SerializeAndCreateFallbackRaytracingRootSignature(ComPtr<ID3D12RaytracingFallbackDevice> device, ComPtr<ID3D12RootSignature>* rootSig)
  {
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    ThrowIfFailed(device->D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
    ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
  }
};
class RootAllocator {
public:

private:

};
