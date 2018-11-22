#pragma once
class RayTracingShaderTable {
public:
  void BuildFallbackShaderTable(ComPtr<ID3D12RaytracingFallbackDevice> device);
};
