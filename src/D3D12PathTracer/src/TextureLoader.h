#pragma once

#include "wincodec.h"

class TextureLoader {
public:
	// load and decode image from file
	static int LoadImageDataFromFile(BYTE** imageData, D3D12_RESOURCE_DESC& resourceDescription, LPCWSTR filename, int &bytesPerRow);
};

