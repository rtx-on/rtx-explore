//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>

// C RunTime Header Files
#include <iomanip>
#include <sstream>
#include <stdlib.h>

#include <algorithm>
#include <assert.h>
#include <atlbase.h>
#include <list>
#include <map>
#include <memory>
#include <shellapi.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <stack>
#include <thread>
#include <condition_variable>
#include <future>

#include <wrl.h>

#define GLM_FORCE_RADIANS
// Primary GLM library
#include <glm/glm/glm.hpp>
// For glm::to_string.
#include <glm/glm/gtx/string_cast.hpp>
// For glm::value_ptr.
#include <glm/glm/gtc/type_ptr.hpp>

#include "D3D12RaytracingFallback.h"
#include "D3D12RaytracingHelpers.hpp"
#include "d3d12_1.h"
#include "d3dx12.h"
#include <atlbase.h>
#include <dxgi1_6.h>

#include <DirectXMath.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#include "DXSampleHelper.h"
#include "DeviceResources.h"

#include "D3D12RaytracingSimpleLighting.h"

#include "model_loading/OBJ_Loader.h"

#include "Model.h"

#include "Scene.h"

#include "MiniEnumTypes.h"

#include "MiniFace.h"

#include "MiniTurtle.h"
#include "MiniFBM.h"
#include "MiniLSystem.h"
#include "MiniBiomeGenerator.h"
#include "MiniTerrian.h"

#include "MiniFaceManager.h"

