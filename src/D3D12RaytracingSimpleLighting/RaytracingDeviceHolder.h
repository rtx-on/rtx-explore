#pragma once
class RaytracingDeviceHolder {
public:
  void Init(std::shared_ptr<DX::DeviceResources> device_resources, ComPtr<ID3D12RaytracingFallbackDevice> fallback_device, ComPtr<ID3D12RaytracingFallbackCommandList> fallback_command_list, ComPtr<ID3D12Device5> dxr_device, ComPtr<ID3D12GraphicsCommandList5> dxr_command_list, std::shared_ptr<RaytracingApi> ray_tracing_api)
  {
    this->device_resources = device_resources;
    this->fallback_device = fallback_device;
    this->fallback_command_list = fallback_command_list;
    this->dxr_device = dxr_device;
    this->dxr_command_list = dxr_command_list;
    this->ray_tracing_api = ray_tracing_api;
  }

  std::shared_ptr<DX::DeviceResources> device_resources;
  ComPtr<ID3D12RaytracingFallbackDevice> fallback_device;
  ComPtr<ID3D12RaytracingFallbackCommandList> fallback_command_list;
  ComPtr<ID3D12Device5> dxr_device;
  ComPtr<ID3D12GraphicsCommandList5> dxr_command_list;
  std::shared_ptr<RaytracingApi> ray_tracing_api;
};
