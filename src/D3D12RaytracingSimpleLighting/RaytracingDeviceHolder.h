#pragma once
class RaytracingDeviceHolder {
public:
  virtual void Init(std::shared_ptr<DX::DeviceResources> device_resources, ComPtr<ID3D12RaytracingFallbackDevice> fallback_device, ComPtr<ID3D12RaytracingFallbackCommandList> fallback_command_list, ComPtr<ID3D12RaytracingFallbackStateObject> fallback_state_object, ComPtr<ID3D12Device5> dxr_device, ComPtr<ID3D12GraphicsCommandList5> dxr_command_list, ComPtr<ID3D12StateObject> dxr_state_object, std::shared_ptr<RaytracingApi> ray_tracing_api)
  {
    this->device_resources = device_resources;
    this->fallback_device = fallback_device;
    this->fallback_command_list = fallback_command_list;
    this->fallback_state_object = fallback_state_object;
    this->dxr_device = dxr_device;
    this->dxr_command_list = dxr_command_list;
    this->dxr_state_object = dxr_state_object;
    this->ray_tracing_api = ray_tracing_api;
  }

protected:
  std::shared_ptr<DX::DeviceResources> device_resources;
  ComPtr<ID3D12RaytracingFallbackDevice> fallback_device;
  ComPtr<ID3D12RaytracingFallbackCommandList> fallback_command_list;
  ComPtr<ID3D12RaytracingFallbackStateObject> fallback_state_object;
  ComPtr<ID3D12Device5> dxr_device;
  ComPtr<ID3D12GraphicsCommandList5> dxr_command_list;
  ComPtr<ID3D12StateObject> dxr_state_object;
  std::shared_ptr<RaytracingApi> ray_tracing_api;
};
