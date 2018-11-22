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

#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL
#include "RayTracingHlslCompat.h"

RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0);
ByteAddressBuffer Indices : register(t1, space0);
StructuredBuffer<Vertex> Vertices : register(t2, space0);
Texture2D text : register(t3, space0);
Texture2D norm_text : register(t4, space0);
SamplerState s1 : register(s0);
SamplerState s2 : register(s1);

ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);
ConstantBuffer<CubeConstantBuffer> g_cubeCB : register(b1);

static const float PI = 3.14159265f;
static const float TWO_PI = 6.283185f;
static const float SQRT_OF_ONE_THIRD = 0.577350f;

// Functions for PRNG
static uint rng_state;
static const float png_01_convert = (1.0f / 4294967296.0f);
uint rand_xorshift()
{
	// Xorshift algorithm from George Marsaglia's paper
	rng_state ^= uint(rng_state << 13);
	rng_state ^= uint(rng_state >> 17);
	rng_state ^= uint(rng_state << 5);
	return rng_state;
}

// Wang hash
uint wang_hash(uint seed)
{
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);
	return seed;
}

SamplerState MeshTextureSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

// Load three 16 bit indices from a byte addressed buffer.
uint3 Load3x16BitIndices(uint offsetBytes)
{
    uint3 indices;

    // ByteAdressBuffer loads must be aligned at a 4 byte boundary.
    // Since we need to read three 16 bit indices: { 0, 1, 2 } 
    // aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
    // we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
    // based on first index's offsetBytes being aligned at the 4 byte boundary or not:
    //  Aligned:     { 0 1 | 2 - }
    //  Not aligned: { - 0 | 1 2 }
    const uint dwordAlignedOffset = offsetBytes & ~7;    
    const uint2 four16BitIndices = Indices.Load2(dwordAlignedOffset);
 
    // Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
    if (dwordAlignedOffset == offsetBytes)
    {
        indices.x = four16BitIndices.x;
		indices.y = four16BitIndices.y;
//        indices.z = four16BitIndices.z;
    }
    else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
    {
        indices.x = four16BitIndices.y;
        indices.y = four16BitIndices.y;
//        indices.z = (four16BitIndices.y >> 32) & 0xffffffff;
    }

    return indices;
}

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
    float4 color; // w component stores info if hit or not: 1 == hit, 0 == light hit, -1 == miss
	float3 rayOrigin;
	float3 rayDir;
};

// Retrieve hit world position.
float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float2 HitAttribute2D(float2 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
	return vertexAttribute[0] +
		attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
		attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
    float2 xy = index + 0.5f; // center in the middle of the pixel.
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
    screenPos.y = -screenPos.y;

    // Unproject the pixel coordinate into a ray.
    float4 world = mul(float4(screenPos, 0, 1), g_sceneCB.projectionToWorld);

    world.xyz /= world.w;
    origin = g_sceneCB.cameraPosition.xyz;
    direction = normalize(world.xyz - origin);
}

// Diffuse lighting calculation.
float4 CalculateDiffuseLighting(float3 hitPosition, float3 normal)
{
    float3 pixelToLight = normalize(g_sceneCB.lightPosition.xyz - hitPosition);

    // Diffuse contribution.
    float fNDotL = max(0.0f, dot(pixelToLight, normal)); 
	float4 col = float4(0, 1, 1, 0);
    return col * g_sceneCB.lightDiffuseColor * fNDotL;
}

/**
 * Computes a cosine-weighted random direction in a hemisphere.
 * Used for diffuse lighting.
 */
float3 CalculateRandomDirectionInHemisphere(float3 normal) {

	float up = sqrt(float(rand_xorshift() * png_01_convert)); // cos(theta)
	float over = sqrt(1 - up * up); // sin(theta)
	float around = float(rand_xorshift() * png_01_convert) * TWO_PI;

	// Find a direction that is not the normal based off of whether or not the
	// normal's components are all equal to sqrt(1/3) or whether or not at
	// least one component is less than sqrt(1/3). Learned this trick from
	// Peter Kutz.

	float3 directionNotNormal;
	if (abs(normal.x) < SQRT_OF_ONE_THIRD) {
		directionNotNormal = float3(1, 0, 0);
	}
	else if (abs(normal.y) < SQRT_OF_ONE_THIRD) {
		directionNotNormal = float3(0, 1, 0);
	}
	else {
		directionNotNormal = float3(0, 0, 1);
	}

	// Use not-normal direction to generate two perpendicular directions
	float3 perpendicularDirection1 =
		normalize(cross(normal, directionNotNormal));
	float3 perpendicularDirection2 =
		normalize(cross(normal, perpendicularDirection1));

	return up * normal
		+ cos(around) * over * perpendicularDirection1
		+ sin(around) * over * perpendicularDirection2;
}

[shader("raygeneration")]
void MyRaygenShader()
{
    float3 rayDir;
    float3 origin;
    
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    GenerateCameraRay(DispatchRaysIndex().xy, origin, rayDir);

    // Trace the ray.
    // Set the ray's extents.
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = rayDir;

    // Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
    // TMin should be kept small to prevent missing geometry at close contact areas.
    ray.TMin = 0.001;
    ray.TMax = 10000.0;
    RayPayload payload = { float4(1, 1, 1, -1.0f), float3(0, 0, 0), float3(0, 0, 0)};
	// for loop over path tracing depth
	// given pay load data (missed, hit something), do something
	// case 1: hit nothing - stop here
	// case 2: hit light source - output accumulated color
	// case 3: hit something else, keep going with new direction
	int depth = 16; // MAKE THIS PART OF THE SEED

	// Use PRNG to generate ray direction
	uint2 samplePt = DispatchRaysIndex().xy;
	uint2 sampleDim = DispatchRaysDimensions().xy;
	for (int i = 0; i < depth; i++) {
		uint id = samplePt.x + sampleDim.x * samplePt.y;
		rng_state = uint(wang_hash((1 << 31) | (depth << 22) | g_sceneCB.iteration) ^ wang_hash(id));

		TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

		if (payload.color.w == 1.0f) {
			// new ray has to be edited here
			ray.Origin = payload.rayOrigin;
			ray.Direction = payload.rayDir;
			payload.color = float4(payload.color.rgb, -1.0f);
			if (i == depth - 1) {
				payload.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
			}
		}
		else {
			// hit light or nothing, either way stop here
			payload.color = float4(payload.color.rgb, 0.0f);
			break;
		}
	}

	// Write the raytraced color to the output texture.
	float3 oldColor = RenderTarget[DispatchRaysIndex().xy].xyz;
	float3 newColor = oldColor.rgb * (g_sceneCB.iteration - 1) + payload.color.xyz;

	float r = clamp(newColor.r / g_sceneCB.iteration, 0, 1);
	float g = clamp(newColor.g / g_sceneCB.iteration, 0, 1);
	float b = clamp(newColor.b / g_sceneCB.iteration, 0, 1);
	float4 color = float4(r, g, b, 0.0f);

	RenderTarget[DispatchRaysIndex().xy] = color;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
	float3 hitPosition = HitWorldPosition();
	uint instanceId = InstanceID();
	float hitType;
	if (instanceId == 1) {
		hitType = 1; // object
	}
	else {
		hitType = 0; // light
	}

	// Get the base index of the triangle's first 16 bit index.
	uint indexSizeInBytes = 4;
	uint indicesPerTriangle = 3;
	uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	uint baseIndex = PrimitiveIndex() * triangleIndexStride;

	// Load up 3 16 bit indices for the triangle.
	const uint3 indices = Indices.Load3(baseIndex);

	// Retrieve corresponding vertex normals for the triangle vertices.
	float3 vertexNormals[3] = {
		Vertices[indices[0]].normal,
		Vertices[indices[1]].normal,
		Vertices[indices[2]].normal
	};

	float2 vertexUVs[3] = {
		Vertices[indices[0]].texCoord,
		Vertices[indices[1]].texCoord,
		Vertices[indices[2]].texCoord
	};

	// Compute the triangle's normal.
	// This is redundant and done for illustration purposes 
	// as all the per-vertex normals are the same and match triangle's normal in this sample. 
	float3 triangleNormal = HitAttribute(vertexNormals, attr);
	float2 triangleUV = HitAttribute2D(vertexUVs, attr);

	// get diffuse direction
	float3 newDir = CalculateRandomDirectionInHemisphere(triangleNormal);
	payload.rayDir = newDir;
	payload.rayOrigin = hitPosition + payload.rayDir * 0.01f;;

	// get the color
	float3 tex = text.SampleLevel(s1, triangleUV, 0);
	float3 color = payload.color.rgb * tex.rgb;
	payload.color = float4(color.xyz, hitType);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
	float3 background = float3(0.0f, 0.0f, 0.0f);
	payload.color = float4(background.xyz, -1.0f);
}

#endif // RAYTRACING_HLSL