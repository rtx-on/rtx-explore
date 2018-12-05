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

#ifndef RAYTRACINGHLSLCOMPAT_H
#define RAYTRACINGHLSLCOMPAT_H

#ifdef HLSL
#include "util/HlslCompat.h"
#else
using namespace DirectX;

// Shader will use byte encoding to access indices.
typedef UINT32 Index;
#endif

enum Feature
{
  AntiAliasing = 1,
  DepthOfField = 2,
};

struct SceneConstantBuffer
{
  XMMATRIX projectionToWorld;
  XMMATRIX view_matrix;
  XMVECTOR cameraPosition;
  XMVECTOR lightPosition;
  XMVECTOR lightAmbientColor;
  XMVECTOR lightDiffuseColor;
  UINT iteration;
  UINT depth;
  UINT features;
};

struct CubeConstantBuffer
{
    XMFLOAT4 albedo;
};

struct Vertex {
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 texCoord;
};

// Holds data for a specific material
struct Material 
{
  float specularExp;
  float reflectiveness;
  float refractiveness;
  float eta;
  float emittance;
  XMFLOAT3 diffuse;
  XMFLOAT3 specular;
};

struct Info
{
  UINT model_offset;
  UINT texture_offset;
  UINT texture_normal_offset;
  UINT material_offset;
  UINT diffuse_sampler_offset;
  UINT normal_sampler_offset;
  XMMATRIX rotation_scale_matrix;
};

#endif // RAYTRACINGHLSLCOMPAT_H