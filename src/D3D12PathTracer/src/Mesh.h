#pragma once

#include "model_loading/tiny_obj_loader.h"
#ifndef OBJ
#include "model_loading/OBJ_Loader.h"
#define OBJ
#endif
#include <DirectXMath.h>

#include "TextureLoader.h"

#include <locale>
#include <codecvt>

using namespace DirectX;
namespace Model
{
	inline std::wstring convert_to_wide(std::string s)
	{
		//convert to wide
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		return converter.from_bytes(s);
	}

	struct Texture
	{
		explicit Texture() = default;
		explicit Texture(const std::string texture_name, const std::string texture_path)
			: texture_name(texture_path + " " + texture_name), wtexture_name(convert_to_wide(this->texture_name))
		{
			auto diffuse_texture_path = convert_to_wide(texture_path);
			if (!texture_path.empty())
			{
				image_size = TextureLoader::LoadImageDataFromFile(&image_data, texture_desc, diffuse_texture_path.c_str(), bytes_per_row);
				if (image_size <= 0)
				{
					throw std::runtime_error(texture_path + " texture not loaded");
				}
			}
		}
		
		void setup_srv(DX::DeviceResources* device_resources, ID3D12DescriptorHeap* descriptor_heap, UINT& descriptors_allocated, UINT descriptor_size)
		{
			if(image_data == nullptr)
			{
				return;
			}

			auto device = device_resources->GetD3DDevice();
			auto command_list = device_resources->GetCommandList();
			auto command_allocator = device_resources->GetCommandAllocator();

			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&texture_desc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&texture_buffer_resource)));

			texture_buffer_resource->SetName(std::wstring(L"Texture buffer resource " + wtexture_name).c_str());

			UINT64 textureUploadBufferSize;
			// this function gets the size an upload buffer needs to be to upload a texture to the gpu.
			// each row must be 256 byte aligned except for the last row, which can just be the size in bytes of the row
			// eg. textureUploadBufferSize = ((((width * numBytesPerPixel) + 255) & ~255) * (height - 1)) + (width * numBytesPerPixel);
			//textureUploadBufferSize = (((imageBytesPerRow + 255) & ~255) * (textureDesc.Height - 1)) + imageBytesPerRow;

			//this basically gets the total size of the upload buffer
			device->GetCopyableFootprints(&texture_desc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

			// now we create an upload heap to upload our texture to the GPU
			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
				D3D12_HEAP_FLAG_NONE, // no flags
				&CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize), // resource description for a buffer (storing the image data in this heap just to copy to the default heap)
				D3D12_RESOURCE_STATE_GENERIC_READ, // We will copy the contents from this heap to the default heap above
				nullptr,
				IID_PPV_ARGS(&texture_upload_heap)));

			texture_upload_heap->SetName(std::wstring(L"Texture upload heap " + wtexture_name).c_str());

			// store vertex buffer in upload heap
			D3D12_SUBRESOURCE_DATA textureData = {};
			textureData.pData = image_data; // pointer to our image data
			textureData.RowPitch = bytes_per_row; // size of all our triangle vertex data
			textureData.SlicePitch = bytes_per_row * texture_desc.Height; // also the size of our triangle vertex data

			command_list->Reset(command_allocator, nullptr);

			// Reset the command list for the acceleration structure construction.
			UpdateSubresources(command_list, texture_buffer_resource.Get(), texture_upload_heap, 0, 0, 1, &textureData);

			// transition the texture default heap to a pixel shader resource (we will be sampling from this heap in the pixel shader to get the color of pixels)
			command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture_buffer_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			UINT descriptor_index_to_use = UINT_MAX;

			auto descriptor_heap_cpu_base = descriptor_heap->GetCPUDescriptorHandleForHeapStart();
			if (descriptor_index_to_use >= descriptor_heap->GetDesc().NumDescriptors)
			{
				descriptor_index_to_use = descriptors_allocated++;
			}

			cpu_descriptor_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptor_heap_cpu_base, descriptor_index_to_use, descriptor_size);

			// create SRV descriptor
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = texture_desc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(texture_buffer_resource.Get(), &srvDesc, cpu_descriptor_handle);

			// used to bind to the root signature
			gpu_descriptor_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptor_heap->GetGPUDescriptorHandleForHeapStart(), descriptor_index_to_use, descriptor_size);

			// Kick off texture uploading
			device_resources->ExecuteCommandList();

			// Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
			device_resources->WaitForGpu();

			//if (image_data != nullptr)
			//{
			//	free(image_data);
			//}
		}

		std::string texture_name;
		std::wstring wtexture_name;

		BYTE* image_data = nullptr;
		int image_size = 0;
		int bytes_per_row = 0;

		D3D12_RESOURCE_DESC texture_desc = {};

		ComPtr<ID3D12Resource> texture_buffer_resource;
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle;
		D3D12_GPU_DESCRIPTOR_HANDLE gpu_descriptor_handle;

		ID3D12Resource* texture_upload_heap;
	};
	struct Material
	{
		explicit Material() = default;
		explicit Material(int material_id, objl::Material obj_material)
			:
			material_id(material_id),
			diffuse("Diffuse", obj_material.map_Kd),
			specular("Specular", obj_material.map_Ks),
			normal("Normal", obj_material.map_bump),
			obj_material(std::move(obj_material))
		{

		}

		void setup_srv(DX::DeviceResources* device_resources, ID3D12DescriptorHeap* descriptor_heap, UINT& descriptors_allocated, UINT descriptor_size)
		{
			diffuse.setup_srv(device_resources, descriptor_heap, descriptors_allocated, descriptor_size);
			specular.setup_srv(device_resources, descriptor_heap, descriptors_allocated, descriptor_size);
			normal.setup_srv(device_resources, descriptor_heap, descriptors_allocated, descriptor_size);
		}

		int material_id;
		Texture diffuse;
		Texture specular;
		Texture normal;
		objl::Material obj_material;
	};

	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT2 texCoord;
	};

	using Index = UINT16;

	struct Mesh
	{
		std::string name;

		std::vector<Vertex> vertices;

		std::vector<Index> vertex_indices;
		Material material;
	};
}
