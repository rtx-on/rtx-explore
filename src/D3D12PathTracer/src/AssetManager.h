#pragma once

#include <unordered_map>
using namespace std;
class AssetManager
{
public:
	AssetManager();
	~AssetManager();

	bool LoadTexture(ID3D12Device* device, BYTE** imageData, D3D12_RESOURCE_DESC& resourceDescription, LPCWSTR filename, int &bytesPerRow);
private:
	unordered_map<string, ID3D12Resource*> textureResources;
	ID3D12Resource* textureBuffer; // the resource heap containing our texture

	ID3D12DescriptorHeap* mainDescriptorHeap;
	ID3D12Resource* textureBufferUploadHeap;


};

