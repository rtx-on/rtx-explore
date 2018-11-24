// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>

// C RunTime Header Files
#include <cassert>

// C++
#include <functional>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <list>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <map>
#include <cstdint>  // For uint64_t
#include <queue>    // For std::queue

// WINDOWS SPECIFIC
#include <wrl.h>
#include <shellapi.h>
#include <atlbase.h>

///EXTERNAL
//GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


//DIRECTX
#include <dxgi1_6.h>
#include "d3d12_1.h"
#include <atlbase.h>
#include "D3D12RaytracingFallback.h"
#include "D3D12RaytracingHelpers.hpp"
#include "d3dx12.h"

#include "DXSample.h"

#include <DirectXMath.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#include "DXSampleHelper.h"
#include "DeviceResources.h"
#include "DirectXRaytracingHelper.h"

#include "llvm-expected.h"
#include "CommandQueue.h"
#include "ResourceHelper.h"
#include "RootAllocator.h"

#include "RaytracingPipeline.h"
#include "RaytracingAcclerationStructure.h"
#include "RaytracingApi.h"

#include "RaytracingHlslCompat.h"
#include "RaytracingManager.h"