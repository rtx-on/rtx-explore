#pragma once
class RaytracingShaderTable {
public:
  template<typename Callback>
  void BuildFallbackShaderTable(ComPtr<ID3D12RaytracingFallbackDevice> device, ComPtr<ID3D12RaytracingFallbackStateObject> state_object, UINT num_table_entries, Callback callback)
  {
    UINT shader_identified_size = device->GetShaderIdentifierSize();
    shader_identified_size = Align(shader_identified_size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
    ComPtr<ID3D12Resource> shader_table =
        ResourceHelper::CreateUploadBuffer(device, shader_identified_size * num_table_entries, D3D12_RESOURCE_FLAG_NONE,
                     D3D12_RESOURCE_STATE_GENERIC_READ,
                     CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT));

    void *data;
    shader_table->Map(0, nullptr, &data);
    callback(state_object, data);
    shader_table->Unmap(0, nullptr);
  }
};
