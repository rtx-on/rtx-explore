#pragma once

#include <iostream>
#include "stdafx.h"
#include "Scene.h"
#include "Utilities.h"
#include <cstring>
#include <glm/glm/gtc/matrix_inverse.hpp>
#include <glm/glm/gtx/string_cast.hpp>
#include "model_loading/tiny_obj_loader.h"
#include "DirectXRaytracingHelper.h"
#include "D3D12RaytracingSimpleLighting.h"
#include "TextureLoader.h"

using namespace tinyobj;
using namespace std;

void OuputAndReset(std::wstringstream& stream)
{
	OutputDebugStringW(stream.str().c_str());
	const static std::wstringstream initial;

	stream.str(std::wstring());
	stream.clear();
	stream.copyfmt(initial);
}


Scene::Scene(string filename, D3D12RaytracingSimpleLighting* programState) : programState(programState) {
	std::wstringstream wstr;
	wstr << L"\n";
	wstr << L"------------------------------------------------------------------------------\n";
	wstr << L"Reading scene from " << filename.c_str() << L"\n";
	wstr << L"------------------------------------------------------------------------------\n";
	OuputAndReset(wstr);

	char* fname = (char*)filename.c_str();
	fp_in.open(fname);
	if (!fp_in.is_open()) {
		wstr << L"Error reading from file - aborting!\n";
		wstr << L"------------------------------------------------------------------------------\n";
		OuputAndReset(wstr);
		throw;
	}
	while (fp_in.good()) {
		string line;
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty()) {
			vector<string> tokens = utilityCore::tokenizeString(line);
			if (strcmp(tokens[0].c_str(), "MATERIAL") == 0) {
				loadMaterial(tokens[1]);
				std::cout << " " << endl;
			}
			else if (strcmp(tokens[0].c_str(), "MODEL") == 0) {
				loadModel(tokens[1]);
				std::cout << " " << endl;
			}
			else if (strcmp(tokens[0].c_str(), "TEXTURE") == 0) {
				loadTexture(tokens[1]);
				std::cout << " " << endl;
			}
			else if (strcmp(tokens[0].c_str(), "OBJECT") == 0) {
				loadObject(tokens[1]);
				std::cout << " " << endl;
			}
		}
	}

	wstr << L"Done loading the scene file!\n";
	wstr << L"------------------------------------------------------------------------------\n";
	OuputAndReset(wstr);
}

int Scene::loadObject(string objectid) {
	int id = atoi(objectid.c_str());

	std::wstringstream wstr;
	wstr << L"Loading OBJECT " << id << L"\n";
	OuputAndReset(wstr);

	ModelLoading::SceneObject newObject;

	string line;

	// LOAD MODEL (MUST EXIST)
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			wstr << L"----------------------------------------\n";
			wstr << L"Linking model...\n";
			OuputAndReset(wstr);

			vector<string> tokens = utilityCore::tokenizeString(line);
			int modelId = atoi(tokens[1].c_str());
			newObject.model = &(modelMap.find(modelId)->second);
		}
	}

	// LOAD TEXTURES IF EXIST
	{
		ModelLoading::TextureBundle texUsed;
		texUsed.albedoTex = nullptr;
		texUsed.normalTex = nullptr;
		newObject.textures = texUsed;

		// albedo tex
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);
			int texId = atoi(tokens[1].c_str());
			if (texId != -1) {
				wstr << L"----------------------------------------\n";
				wstr << L"Linking albedo texture...\n";
				OuputAndReset(wstr);
				newObject.textures.albedoTex = &(textureMap.find(texId)->second);
			}
		}

		// normal tex
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);
			int texId = atoi(tokens[1].c_str());
			if (texId != -1) {
				wstr << L"----------------------------------------\n";
				wstr << L"Linking normal texture...\n";
				OuputAndReset(wstr);
				newObject.textures.normalTex = &(textureMap.find(texId)->second);
			}
		}
	}
	
	// LOAD MATERIAL IF EXISTS
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);
			int matId = atoi(tokens[1].c_str());
			if (matId != -1) {
				wstr << L"----------------------------------------\n";
				wstr << L"Linking material...\n";
				OuputAndReset(wstr);
				newObject.material = &(materialMap.find(matId)->second);
			}
		}
	}

	// LOAD TRANSFORM (MUST EXIST)
	{
		wstr << L"----------------------------------------\n";
		wstr << L"Loading transform...\n";
		OuputAndReset(wstr);
		utilityCore::safeGetline(fp_in, line);
		while (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			if (strcmp(tokens[0].c_str(), "trans") == 0) {
				glm::vec3 t(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
				newObject.translation = t;
			}
			else if (strcmp(tokens[0].c_str(), "rotat") == 0) {
				glm::vec3 r(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
				newObject.rotation = r;
			}
			else if (strcmp(tokens[0].c_str(), "scale") == 0) {
				glm::vec3 s(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
				newObject.scale = s;
			}

			utilityCore::safeGetline(fp_in, line);
		}
	}

	newObject.id = id;
	objects.push_back(newObject);

	wstr << L"----------------------------------------\n";
	wstr << L"Done loading OBJECT " << id << L" !\n";
	wstr << L"------------------------------------------------------------------------------\n";
	OuputAndReset(wstr);

	return 1;
}


int Scene::loadModel(string modelid) {
	int id = atoi(modelid.c_str());

	std::wstringstream wstr;
	wstr << L"Loading MODEL " << id << L"\n";
	wstr << L"----------------------------------------\n";
	OuputAndReset(wstr);

	ModelLoading::Model newModel;

	string line;

	//load model type
	utilityCore::safeGetline(fp_in, line);
	if (!line.empty() && fp_in.good()) {
		vector<string> tokens = utilityCore::tokenizeString(line);

		// load mesh here
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;

		tinyobj::attrib_t attrib;
		std::string err;
		bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, tokens[1].c_str());

		std::vector<Index> indices;
		std::vector<Vertex> vertices;

		if (!ret)
		{
			throw std::runtime_error("failed to load Object!");
		}
		else
		{
			// loop over shapes
			int finalIdx = 0;
			for (unsigned int s = 0; s < shapes.size(); s++)
			{
				size_t index_offset = 0;
				// loop over  faces
				for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
					int fv = shapes[s].mesh.num_face_vertices[f];
					std::vector<float> &positions = attrib.vertices;
					std::vector<float> &normals = attrib.normals;
					std::vector<float> &texcoords = attrib.texcoords;

					// loop over vertices in face, might be > 3
					for (size_t v = 0; v < fv; v++) {
						tinyobj::index_t index = shapes[s].mesh.indices[v + index_offset];

						Vertex vert;
						vert.position = XMFLOAT3(positions[index.vertex_index * 3], positions[index.vertex_index * 3 + 1], positions[index.vertex_index * 3 + 2]);
						if (index.normal_index != -1)
						{
							vert.normal = XMFLOAT3(normals[index.normal_index * 3], normals[index.normal_index * 3 + 1], normals[index.normal_index * 3 + 2]);
						}

						if (index.texcoord_index != -1)
						{
							vert.texCoord = XMFLOAT2(texcoords[index.texcoord_index * 2], 1 - texcoords[index.texcoord_index * 2 + 1]);
						}
						vertices.push_back(vert);
						indices.push_back((Index)(finalIdx + index_offset + v));
					}

					index_offset += fv;
				}
				finalIdx += index_offset;
			}

			Vertex* vPtr = vertices.data();
			Index* iPtr = indices.data();
			auto device = programState->GetDeviceResources()->GetD3DDevice();
			AllocateUploadBuffer(device, iPtr, indices.size() * sizeof(Index), &newModel.indices.resource);
			AllocateUploadBuffer(device, vPtr, vertices.size() * sizeof(Vertex), &newModel.vertices.resource);

			// Vertex buffer is passed to the shader along with index buffer as a descriptor table.
			// Vertex buffer descriptor must follow index buffer descriptor in the descriptor heap.
			//UINT descriptorIndexIB = programState->CreateBufferSRV(&newModel.indices, indices.size() * sizeof(Index) / 4, 0);
			//UINT descriptorIndexVB = programState->CreateBufferSRV(&newModel.vertices, vertices.size(), sizeof(Vertex));
                        newModel.verticesCount = vertices.size();
                        newModel.indicesCount = indices.size();
			//ThrowIfFalse(descriptorIndexVB == descriptorIndexIB + 1, L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index!");
		}
	}

	newModel.id = id;
        std::pair<int, ModelLoading::Model> pair(id, newModel);
	modelMap.insert(pair);

	wstr << L"Done loading MODEL " << id << L" !\n";
	wstr << L"------------------------------------------------------------------------------\n";
	OuputAndReset(wstr);

	return 1;
}

int Scene::loadTexture(string texid) {
	int id = atoi(texid.c_str());

	std::wstringstream wstr;
	wstr << L"Loading TEXTURE " << id << L"\n";
	wstr << L"----------------------------------------\n";
	OuputAndReset(wstr);

	ModelLoading::Texture newTexture;

	string line;

	//load texture
	utilityCore::safeGetline(fp_in, line);
	if (!line.empty() && fp_in.good()) {
		vector<string> tokens = utilityCore::tokenizeString(line);

		// Load the image from file
		D3D12_RESOURCE_DESC& textureDesc = newTexture.textureDesc;
		int imageBytesPerRow;
		BYTE* imageData;

		wstring tempStr = utilityCore::string2wstring(tokens[1]);
		int imageSize = TextureLoader::LoadImageDataFromFile(&imageData, textureDesc, tempStr.c_str(), imageBytesPerRow);

		// make sure we have data
		if (imageSize <= 0)
		{
			return false;
		}

		auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		auto device = programState->GetDeviceResources()->GetD3DDevice();
		ThrowIfFailed(device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&newTexture.texBuffer.resource)));

		UINT64 textureUploadBufferSize;
		// this function gets the size an upload buffer needs to be to upload a texture to the gpu.
		// each row must be 256 byte aligned except for the last row, which can just be the size in bytes of the row
		// eg. textureUploadBufferSize = ((((width * numBytesPerPixel) + 255) & ~255) * (height - 1)) + (width * numBytesPerPixel);
		//textureUploadBufferSize = (((imageBytesPerRow + 255) & ~255) * (textureDesc.Height - 1)) + imageBytesPerRow;
		device->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

		ID3D12Resource* textureBufferUploadHeap = programState->GetTextureBufferUploadHeap();
		// now we create an upload heap to upload our texture to the GPU
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize), // resource description for a buffer (storing the image data in this heap just to copy to the default heap)
			D3D12_RESOURCE_STATE_GENERIC_READ, // We will copy the contents from this heap to the default heap above
			nullptr,
			IID_PPV_ARGS(&textureBufferUploadHeap)));

		textureBufferUploadHeap->SetName(L"Texture Buffer Upload Resource Heap");

		// store vertex buffer in upload heap
		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = &imageData[0]; // pointer to our image data
		textureData.RowPitch = imageBytesPerRow; // size of all our triangle vertex data
		textureData.SlicePitch = imageBytesPerRow * textureDesc.Height; // also the size of our triangle vertex data


		auto commandList = programState->GetDeviceResources()->GetCommandList();
		auto commandAllocator = programState->GetDeviceResources()->GetCommandAllocator();

		commandList->Reset(commandAllocator, nullptr);

		// Reset the command list for the acceleration structure construction.
		UpdateSubresources(commandList, newTexture.texBuffer.resource.Get(), textureBufferUploadHeap, 0, 0, 1, &textureData);

		// transition the texture default heap to a pixel shader resource (we will be sampling from this heap in the pixel shader to get the color of pixels)
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(newTexture.texBuffer.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		// Kick off texture uploading
		programState->GetDeviceResources()->ExecuteCommandList();

		// Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
		programState->GetDeviceResources()->WaitForGpu();
	}

	newTexture.id = id;
        std::pair<int, ModelLoading::Texture> pair(id, newTexture);
	textureMap.insert(pair);

	wstr << L"Done loading TEXTURE " << id << L" !\n";
	wstr << L"------------------------------------------------------------------------------\n";
	OuputAndReset(wstr);
	return 1;
}

int Scene::loadMaterial(string matid) {
	int id = atoi(matid.c_str());

	std::wstringstream wstr;
	wstr << L"Loading MATERIAL " << id << L"\n";
	wstr << L"----------------------------------------\n";
	OuputAndReset(wstr);

	ModelLoading::MaterialResource newMat;
	string line;

	// LOAD RGB
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			newMat.material.diffuse = XMFLOAT3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
		}
	}

	// LOAD SPEC
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			newMat.material.specular = XMFLOAT3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
		}
	}

	// LOAD SPECEX
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			float exp(atof(tokens[1].c_str()));
			newMat.material.specularExp = exp;
		}
	}

	// LOAD REFL
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			float refl(atof(tokens[1].c_str()));
			newMat.material.reflectiveness = refl;
		}
	}

	// LOAD REFR
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			float refr(atof(tokens[1].c_str()));
			newMat.material.refractiveness = refr;
		}
	}

        // LOAD ETA
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			float eta(atof(tokens[1].c_str()));
			newMat.material.eta = eta;
		}
	}


	// LOAD EMITTANCE
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			float emit(atof(tokens[1].c_str()));
			newMat.material.emittance = emit;
		}
	}

	newMat.id = id;
        std::pair<int, ModelLoading::MaterialResource> pair(id, newMat);
	materialMap.insert(pair);

	wstr << L"Done loading MATERIAL " << id << L" !\n";
	wstr << L"------------------------------------------------------------------------------\n";
	OuputAndReset(wstr);
	return 1;
}

int Scene::loadCamera() {
	std::wstringstream wstr;
	wstr << L"Loading CAMERA\n";
	wstr << L"----------------------------------------\n";
	OuputAndReset(wstr);

	ModelLoading::Camera newCam;
	float fovy;

	// load static properties
	for (int i = 0; i < 4; i++) {
		string line;
		utilityCore::safeGetline(fp_in, line);
		vector<string> tokens = utilityCore::tokenizeString(line);
		if (strcmp(tokens[0].c_str(), "resolution") == 0) {
			newCam.width = atoi(tokens[1].c_str());
			newCam.height = atoi(tokens[2].c_str());
		}
		else if (strcmp(tokens[0].c_str(), "fovy") == 0) {
			fovy = atof(tokens[1].c_str());
		}
		else if (strcmp(tokens[0].c_str(), "iterations") == 0) {
			newCam.maxIterations = atoi(tokens[1].c_str());
		}
		else if (strcmp(tokens[0].c_str(), "depth") == 0) {
			newCam.maxDepth = atoi(tokens[1].c_str());
		}
	}

	string line;
	utilityCore::safeGetline(fp_in, line);
	while (!line.empty() && fp_in.good()) {
		vector<string> tokens = utilityCore::tokenizeString(line);
		if (strcmp(tokens[0].c_str(), "eye") == 0) {
			newCam.eye = glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
		}
		else if (strcmp(tokens[0].c_str(), "lookat") == 0) {
			newCam.lookat = glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
		}
		else if (strcmp(tokens[0].c_str(), "up") == 0) {
			newCam.up = glm::vec3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
		}
		else if (strcmp(tokens[0].c_str(), "focal") == 0) {
			newCam.focalDist = atof(tokens[1].c_str());
		}
		else if (strcmp(tokens[0].c_str(), "lensr") == 0) {
			newCam.lensRadius = atof(tokens[1].c_str());
		}

		utilityCore::safeGetline(fp_in, line);
	}

	// calculate fov based on resolution
	float yscaled = tan(fovy * (PI / 180));
	float xscaled = (yscaled * camera.width) / camera.height;
	float fovx = (atan(xscaled) * 180) / PI;
	newCam.fov = glm::vec2(fovx, fovy);

	// camera axes
	newCam.forward = glm::normalize(newCam.lookat - newCam.eye);
	newCam.right = glm::normalize(glm::cross(camera.forward, camera.up));	

	wstr << L"Done loading CAMERA !n";
	wstr << L"----------------------------------------\n";
	OuputAndReset(wstr);
	return 1;
}

D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& Scene::GetTopLevelDesc()
{
  if (!top_level_build_desc_allocated)
  {
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS &topLevelInputs =
        top_level_build_desc.Inputs;
    topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    topLevelInputs.Flags =
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    topLevelInputs.NumDescs = objects.size();
    topLevelInputs.pGeometryDescs = nullptr;
    topLevelInputs.Type =
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    top_level_build_desc_allocated = true;
  }

  return top_level_build_desc;
}

D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO& Scene::GetTopLevelPrebuildInfo(bool is_fallback, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice, ComPtr<ID3D12Device5> m_dxrDevice)
{
  if (!top_level_prebuild_info_allocated)
    {
      D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& desc = GetTopLevelDesc();
      top_level_prebuild_info = {};
      if (is_fallback) {
        m_fallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(
            &desc.Inputs, &top_level_prebuild_info);
      } else // DirectX Raytracing
      {
        m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(
            &desc.Inputs, &top_level_prebuild_info);
      }
      ThrowIfFalse(top_level_prebuild_info.ResultDataMaxSizeInBytes > 0);

      top_level_prebuild_info_allocated = true;
    }
    return top_level_prebuild_info;
}

ComPtr<ID3D12Resource> Scene::GetTopLevelScratchAS(bool is_fallback, ComPtr<ID3D12Device> device, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice, ComPtr<ID3D12Device5> m_dxrDevice)
{
  
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO&
    topLevelPrebuildInfo =
        GetTopLevelPrebuildInfo(is_fallback, m_fallbackDevice, m_dxrDevice);
    AllocateUAVBuffer(device.Get(), topLevelPrebuildInfo.ScratchDataSizeInBytes,
                      &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                      L"TopLevelScratchResourceScene");
    return scratchResource;
}

ComPtr<ID3D12Resource> Scene::GetTopAS(bool is_fallback, ComPtr<ID3D12Device> device, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice, ComPtr<ID3D12Device5> m_dxrDevice)
{
  
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO& topLevelPrebuildInfo = GetTopLevelPrebuildInfo(is_fallback, m_fallbackDevice, m_dxrDevice);
    D3D12_RESOURCE_STATES initialResourceState;
    if (is_fallback) {
      initialResourceState =
          m_fallbackDevice->GetAccelerationStructureResourceState();
    } else // DirectX Raytracing
    {
      initialResourceState =
          D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    }

    AllocateUAVBuffer(device.Get(),
                      topLevelPrebuildInfo.ResultDataMaxSizeInBytes,
                      &m_topLevelAccelerationStructure, initialResourceState,
                      L"TopLevelASScene");
    return m_topLevelAccelerationStructure;
}

ComPtr<ID3D12Resource> Scene::GetInstanceDescriptors(
    bool is_fallback, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice,
    ComPtr<ID3D12Device5> m_dxrDevice) {
  
    auto device = programState->GetDeviceResources()->GetD3DDevice();
    if (is_fallback)
    {
      std::vector<D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC> instanceDescArray;
      for (int i = 0; i < objects.size(); i++) {
        D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC instanceDesc = {};

        ModelLoading::SceneObject& obj = objects[i];
        ModelLoading::Model* model = obj.model;

        memcpy(instanceDesc.Transform, obj.getTransform3x4(), 12 * sizeof(FLOAT));
		
        instanceDesc.InstanceMask = 0xFF;
        instanceDesc.InstanceID = i; // TODO MAKE SURE THIS MATCHES THE ONE BELOW (FOR ACTUAL RAYTRACING)
        //instanceDesc.InstanceContributionToHitGroupIndex = 0;


        UINT numBufferElements = static_cast<UINT>(model->GetPreBuild(is_fallback, m_fallbackDevice, m_dxrDevice).ResultDataMaxSizeInBytes) / sizeof(UINT32);
     
        instanceDesc.AccelerationStructure = model->GetFallBackWrappedPoint(programState, is_fallback, m_fallbackDevice, m_dxrDevice, numBufferElements);

      
        // figure out a way to do textures & materials TODO
        instanceDescArray.push_back(instanceDesc);
      }

      AllocateUploadBuffer(device, instanceDescArray.data(),
                           sizeof(D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC) * instanceDescArray.size(),
                           &instanceDescs, L"InstanceDescs");

      //programState->GetDeviceResources()->ExecuteCommandList();
      //programState->GetDeviceResources()->GetCommandList()->Reset(programState->GetDeviceResources()->GetCommandAllocator(), nullptr);
    }
    else
    {
      std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescArray;
      for (int i = 0; i < objects.size(); i++) {
        D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};

        ModelLoading::SceneObject obj = objects[i];
        ModelLoading::Model* model = obj.model;

        memcpy(instanceDesc.Transform, obj.getTransform3x4(), 12 * sizeof(FLOAT));
        instanceDesc.InstanceMask = 0xFF;
        instanceDesc.InstanceID = i; // TODO MAKE SURE THIS MATCHES THE ONE BELOW (FOR ACTUAL RAYTRACING)
        //instanceDesc.InstanceContributionToHitGroupIndex = 0;

        UINT numBufferElements =
            static_cast<UINT>(model->GetPreBuild(is_fallback, m_fallbackDevice, m_dxrDevice).ResultDataMaxSizeInBytes) / sizeof(UINT32);
     
        instanceDesc.AccelerationStructure =
            model->GetBottomAS(is_fallback, device, m_fallbackDevice, m_dxrDevice)->GetGPUVirtualAddress();

      
        // figure out a way to do textures & materials TODO
        instanceDescArray.push_back(instanceDesc);
      }

      AllocateUploadBuffer(device, instanceDescArray.data(),
                           sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instanceDescArray.size(),
                           &instanceDescs, L"InstanceDescs");
    }
    return instanceDescs;
}

WRAPPED_GPU_POINTER Scene::GetWrappedGPUPointer(bool is_fallback, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice, ComPtr<ID3D12Device5> m_dxrDevice)
{
  
    UINT numBufferElements =
        static_cast<UINT>(GetTopLevelPrebuildInfo(is_fallback, m_fallbackDevice, m_dxrDevice).ResultDataMaxSizeInBytes) / sizeof(UINT32);
        return programState->CreateFallbackWrappedPointer(m_topLevelAccelerationStructure.Get(), numBufferElements); 
}

void Scene::FinalizeAS()
{
  
    for (auto& model_pair : modelMap)
    {
      ModelLoading::Model &model = model_pair.second;
      model.FinalizeAS();
    }

    auto& topLevelBuildDesc = GetTopLevelDesc();
    topLevelBuildDesc.DestAccelerationStructureData =
        m_topLevelAccelerationStructure->GetGPUVirtualAddress();
        topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
        topLevelBuildDesc.Inputs.InstanceDescs = instanceDescs->GetGPUVirtualAddress();
  }

  void Scene::BuildAllAS(bool is_fallback, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice,
      ComPtr<ID3D12Device5> m_dxrDevice, ComPtr<ID3D12RaytracingFallbackCommandList> fbCmdLst, ComPtr<ID3D12GraphicsCommandList5> rtxCmdList) {
    auto commandList =
        programState->GetDeviceResources()->GetCommandList();
    auto device = programState->GetDeviceResources()->GetD3DDevice();
    if (is_fallback) {
      // Set the descriptor heaps to be used during acceleration structure build
      // for the Fallback Layer.
        ID3D12DescriptorHeap *pDescriptorHeaps[] = { programState->GetDescriptorHeap().Get() };
        fbCmdLst->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);

        for (auto& model_pair : modelMap)
        {
          ModelLoading::Model &model = model_pair.second;
          fbCmdLst.Get()->BuildRaytracingAccelerationStructure(&model.GetBottomLevelBuildDesc(), 0, nullptr);
          commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(model.GetBottomAS(is_fallback, device, m_fallbackDevice, m_dxrDevice).Get()));
        }
        fbCmdLst->BuildRaytracingAccelerationStructure(&GetTopLevelDesc(), 0, nullptr);
    } else {
        for (auto& model_pair : modelMap)
        {
           ModelLoading::Model &model = model_pair.second;
          rtxCmdList.Get()->BuildRaytracingAccelerationStructure(&model.GetBottomLevelBuildDesc(), 0, nullptr);
          commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(model.GetBottomAS(is_fallback, device, m_fallbackDevice, m_dxrDevice).Get()));
        }
       rtxCmdList->BuildRaytracingAccelerationStructure(&GetTopLevelDesc(), 0, nullptr);
    }
    // Kick off acceleration structure construction.
    programState->GetDeviceResources()->ExecuteCommandList();

    // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
    programState->GetDeviceResources()->WaitForGpu();
}

void Scene::AllocateResourcesInDescriptorHeap()
{
  auto device = programState->GetDeviceResources()->GetD3DDevice();

  for (auto& model_pair : modelMap)
  {
    auto& newModel = model_pair.second;
    programState->CreateBufferSRV(&newModel.vertices, newModel.verticesCount, sizeof(Vertex));
  }

  for (auto& model_pair : modelMap)
  {
    auto& newModel = model_pair.second;
    programState->CreateBufferSRV(&newModel.indices, newModel.indicesCount * sizeof(Index) / 4, 0);
  }

  for (auto& object : objects)
  {
    ModelLoading::InfoResource& info_resource = object.info_resource;

    //DEFAULT TO NEGATIVE
    memset(&info_resource.info, -1, sizeof(info_resource.info));

    info_resource.info.model_offset = object.model->id;

    if (object.textures.albedoTex != nullptr)
    {
      info_resource.info.texture_offset = object.textures.albedoTex->id;
    }
    
    if (object.textures.normalTex != nullptr)
    {
      info_resource.info.texture_normal_offset = object.textures.normalTex->id;
    }

    if (object.material != nullptr)
    {
      info_resource.info.material_offset = object.material->id;
    }

	XMVECTOR v = XMLoadFloat3(&XMFLOAT3((object.rotation.z), (object.rotation.y), (object.rotation.x)));
	info_resource.info.rotation_matrix = XMMatrixRotationRollPitchYawFromVector(v);

    // Create the constant buffer memory and map the CPU and GPU addresses
    const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // Allocate one constant buffer per frame, since it gets updated every frame.
    size_t cbSize = (sizeof(Info) + 255 ) & ~255; //align to 256 for CBV
    const D3D12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &constantBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&info_resource.d3d12_resource.resource)));

    info_resource.d3d12_resource.resource->SetName(
        utilityCore::stringAndId(L"InfoResourceObject ", object.id).c_str());

    UINT descriptorIndex = programState->AllocateDescriptor(&info_resource.d3d12_resource.cpuDescriptorHandle);
    info_resource.d3d12_resource.gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(programState->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, *programState->GetDescriptorSize());

    // create SRV descriptor
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = info_resource.d3d12_resource.resource->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = cbSize;

    device->CreateConstantBufferView(&cbvDesc, info_resource.d3d12_resource.cpuDescriptorHandle);

    // Map the constant buffer and cache its heap pointers.
    // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    Info *mapped_data;
    ThrowIfFailed(info_resource.d3d12_resource.resource->Map(0, &readRange, reinterpret_cast<void**>(&mapped_data)));
    memcpy(mapped_data, &info_resource.info, sizeof(Info));
    info_resource.d3d12_resource.resource->Unmap(0, &readRange);
  }

  for (auto& material_pair : materialMap)
  {
    int material_id = material_pair.first;
    ModelLoading::MaterialResource& material = material_pair.second;

    // Create the constant buffer memory and map the CPU and GPU addresses
    const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // Allocate one constant buffer per frame, since it gets updated every frame.
    size_t cbSize = (sizeof(Material) + 255 ) & ~255; //align to 256 for CBV
    const D3D12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &constantBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&material.d3d12_material_resource.resource)));
    material.d3d12_material_resource.resource->SetName(
        utilityCore::stringAndId(L"Material ", material_id).c_str());

    UINT descriptorIndex = programState->AllocateDescriptor(&material.d3d12_material_resource.cpuDescriptorHandle);
    material.d3d12_material_resource.gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(programState->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, *programState->GetDescriptorSize());

    // create SRV descriptor
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = material.d3d12_material_resource.resource->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = cbSize;

    device->CreateConstantBufferView(&cbvDesc, material.d3d12_material_resource.cpuDescriptorHandle);

    // Map the constant buffer and cache its heap pointers.
    // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    Material *material_mapped_data;
    ThrowIfFailed(material.d3d12_material_resource.resource->Map(0, &readRange, reinterpret_cast<void**>(&material_mapped_data)));
    memcpy(material_mapped_data, &material.material, sizeof(Material));
    material.d3d12_material_resource.resource->Unmap(0, &readRange);

  }

  for (auto& texture_pair : textureMap)
  {
    auto &newTexture = texture_pair.second;
    UINT descriptorIndex = programState->AllocateDescriptor(&newTexture.texBuffer.cpuDescriptorHandle);

    // create SRV descriptor
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = newTexture.textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(newTexture.texBuffer.resource.Get(), &srvDesc, newTexture.texBuffer.cpuDescriptorHandle);

    // used to bind to the root signature
    newTexture.texBuffer.gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(programState->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, *programState->GetDescriptorSize());
  }
}
