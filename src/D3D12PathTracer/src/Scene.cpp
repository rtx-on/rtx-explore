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
			UINT descriptorIndexIB = programState->CreateBufferSRV(&newModel.indices, indices.size() * sizeof(Index) / 4, 0);
			UINT descriptorIndexVB = programState->CreateBufferSRV(&newModel.vertices, vertices.size(), sizeof(Vertex));
			ThrowIfFalse(descriptorIndexVB == descriptorIndexIB + 1, L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index!");
		}
	}

	newModel.id = id;
	std::pair<int, ModelLoading::Model> modelPair(id, newModel);
	modelMap.insert(modelPair);

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
		D3D12_RESOURCE_DESC textureDesc;
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

		UINT descriptorIndex = programState->AllocateDescriptor(&newTexture.texBuffer.cpuDescriptorHandle);

		// create SRV descriptor
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(newTexture.texBuffer.resource.Get(), &srvDesc, newTexture.texBuffer.cpuDescriptorHandle);

		// used to bind to the root signature
		newTexture.texBuffer.gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(programState->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, *programState->GetDescriptorSize());

		// Kick off texture uploading
		programState->GetDeviceResources()->ExecuteCommandList();

		// Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
		programState->GetDeviceResources()->WaitForGpu();
	}

	newTexture.id = id;
	std::pair<int, ModelLoading::Texture> texPair(id, newTexture);
	textureMap.insert(texPair);

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

	ModelLoading::Material newMat;
	string line;

	// LOAD RGB
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			glm::vec3 diffuse(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
			newMat.diffuse = diffuse;
		}
	}

	// LOAD SPEC
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			glm::vec3 spec(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
			newMat.specular = spec;
		}
	}

	// LOAD SPECEX
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			float exp(atof(tokens[1].c_str()));
			newMat.specularExp = exp;
		}
	}

	// LOAD ETA
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			float eta(atof(tokens[1].c_str()));
			newMat.eta = eta;
		}
	}

	// LOAD REFL
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			float refl(atof(tokens[1].c_str()));
			newMat.reflectiveness = refl;
		}
	}

	// LOAD REFR
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			float refr(atof(tokens[1].c_str()));
			newMat.refractiveness = refr;
		}
	}

	// LOAD EMITTANCE
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			float emit(atof(tokens[1].c_str()));
			newMat.emittance = emit;
		}
	}

	newMat.id = id;
	std::pair<int, ModelLoading::Material> matPair(id, newMat);
	materialMap.insert(matPair);

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