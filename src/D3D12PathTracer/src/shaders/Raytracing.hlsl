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

#define AA 1
#define DOF 0

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

static const float4 BACKGROUND_COLOR = float4(0,0,0,0);
static const float4 INITIAL_COLOR = float4(1, 1, 1, 0);
static const float4 SPECULAR_COLOR = float4(1, 1, 1, 1);

static uint rng_state; // the current seed
static const float png_01_convert = (1.0f / 4294967296.0f); // to convert into a 01 distribution

// Magic bit shifting algorithm from George Marsaglia's paper
uint rand_xorshift()
{
	rng_state ^= uint(rng_state << 13);
	rng_state ^= uint(rng_state >> 17);
	rng_state ^= uint(rng_state << 5);
	return rng_state;
}

// Wang hash for randomizing
uint wang_hash(uint seed)
{
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);
	return seed;
}

// Sets the seed of the pseudo-rng calls using the index of the pixel, the iteration number, and the current depth
void ComputeRngSeed(uint index, uint iteration, uint depth) {
	rng_state = uint(wang_hash((1 << 31) | (depth << 22) | iteration) ^ wang_hash(index));
}

// Returns a pseudo-rng float between 0 and 1. Must call ComputeRngSeed at least once.
float Uniform01() {
	return float(rand_xorshift() * png_01_convert);
}

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

// Taken from https://github.com/emily-vo/Project3-CUDA-Path-Tracer
float EvaluateFresnelDielectric(float cosThetaI, float etaI, float etaT)
{
	cosThetaI = clamp(cosThetaI, -1.0f, 1.0f);

	bool entering = cosThetaI > 0.0f;
	float etaIb = etaI;
	float etaTb = etaT;
	if (!entering) {
		etaIb = etaT;
		etaTb = etaI;
		cosThetaI = abs(cosThetaI);
	}

	float sinThetaI = sqrt(max(0.0f, 1 - cosThetaI * cosThetaI));
	float sinThetaT = etaIb / etaTb * sinThetaI;

	if (sinThetaT >= 1) {
		return 1.0f;
	}

	float cosThetaT = sqrt(max(0.0f, 1 - sinThetaT * sinThetaT));

	float Rparl = ((etaTb * cosThetaI) - (etaIb * cosThetaT)) /
		((etaTb * cosThetaI) + (etaIb * cosThetaT));
	float Rperp = ((etaIb * cosThetaI) - (etaTb * cosThetaT)) /
		((etaIb * cosThetaI) + (etaTb * cosThetaT));
	return (Rparl * Rparl + Rperp * Rperp) / 2;
}

/**
 * Maps a (u,v) in [0, 1)^2 to a 2D unit disk centered at (0,0). Based on PBRT
 */
float2 CalculateConcentricSampleDisk(float u, float v) {
	float2 uOffset = 2.0f * float2(u, v) - float2(1, 1);
	if (uOffset.x == 0 && uOffset.y == 0) {
		return float2(0.0f, 0.0f);
	}

	float theta, r;
	if (abs(uOffset.x) > abs(uOffset.y)) {
		r = uOffset.x;
		theta = PI / 4 * (uOffset.y / uOffset.x);
	}
	else {
		r = uOffset.y;
		theta = (PI / 2) - (PI / 4 * (uOffset.x / uOffset.y));
	}
	return r * float2(cos(theta), sin(theta));
}

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
    float2 xy = index + 0.5f; // center in the middle of the pixel.

#if AA
	// Anti - aliasing
	float epsilonX = 0;
	float epsilonY = 0;
	epsilonX = Uniform01();
	epsilonY = Uniform01();
	xy.x += epsilonX;
	xy.y += epsilonY;
#endif

    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
    screenPos.y = -screenPos.y;

    // Unproject the pixel coordinate into a ray.
    float4 world = mul(float4(screenPos, 0, 1), g_sceneCB.projectionToWorld);

    world.xyz /= world.w;
    origin = g_sceneCB.cameraPosition.xyz;
    direction = normalize(world.xyz - origin);

#if DOF
	// Depth of Field
	float lensRad = 0.5f;
	float focalDist = 20.0f;
	float3 pLens = float3(lensRad * CalculateConcentricSampleDisk(Uniform01(), Uniform01()), 0.0f);
	float ft = focalDist / abs(direction.z);
	float3 pFocus = direction * ft;
	origin += pLens;
	direction = normalize(pFocus - pLens);
#endif
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

	float up = sqrt(Uniform01()); // cos(theta)
	float over = sqrt(1 - up * up); // sin(theta)
	float around = Uniform01() * TWO_PI;

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

void ReflectiveBounce(float3 triangleNormal, float3 hitPosition, float hitType, RayPayload payload)
{
	float3 newDir = reflect(WorldRayDirection(), triangleNormal);
	payload.rayDir = newDir;
	payload.rayOrigin = hitPosition + payload.rayDir * 0.01f;

	float3 color = payload.color.rgb * SPECULAR_COLOR;
	payload.color = float4(color, hitType);
}

void RefractiveBounce(float3 triangleNormal, float3 hitPosition, float hitType, RayPayload payload)
{
	float indexOfRefraction = 1.2f; // TODO: Change this to be more general

	// adjust eta & normal according to direction of ray (inside or outside mat)
	bool inside = dot(WorldRayDirection(), triangleNormal) > 0.f;
	float3 tempNormal = triangleNormal * (inside ? -1.0f : 1.0f);
	float eta = inside ? indexOfRefraction : (1.0f / indexOfRefraction);

	// normal refraction
	float3 newDir = refract(WorldRayDirection(), tempNormal, eta);

	// internal total reflection
	if (length(newDir) < 0.01f) {
		payload.color *= 0;
		newDir = reflect(WorldRayDirection(), triangleNormal);
	}

	// use schlick's approx
	float schlick_0 = pow((inside ? indexOfRefraction - 1.0f : 1.0f - indexOfRefraction) /
		(1.0f + indexOfRefraction), 2.0f);
	float schlick_coef = schlick_0 +
		(1 - schlick_0) * pow(1 - max(0.0f, dot(WorldRayDirection(), triangleNormal)), 5);

	// based on coef, pick either a refraction or reflection
	newDir = schlick_coef < Uniform01() ? reflect(WorldRayDirection(), triangleNormal) : newDir;
	payload.rayDir = newDir;
	payload.rayOrigin = hitPosition + payload.rayDir * 0.01f;

	float3 color = payload.color.rgb * SPECULAR_COLOR;
	payload.color = float4(color, hitType);
}

void DiffuseBounce(float3 triangleNormal, float3 hitPosition, float hitType, float2 triangleUV, RayPayload payload)
{
	float3 newDir = CalculateRandomDirectionInHemisphere(triangleNormal);
	payload.rayDir = newDir;
	payload.rayOrigin = hitPosition + payload.rayDir * 0.01f;

	float3 tex = text.SampleLevel(s1, triangleUV, 0);
	float3 color = payload.color.rgb * tex.rgb * 3.0f;
	payload.color = float4(color.xyz, hitType);
}

[shader("raygeneration")]
void MyRaygenShader()
{
	// Path tracing depth
	int depth = g_sceneCB.depth;

	// Set the seed of the prng factory
	uint2 samplePt = DispatchRaysIndex().xy;
	uint2 sampleDim = DispatchRaysDimensions().xy;
	uint id = samplePt.x + sampleDim.x * samplePt.y;
	ComputeRngSeed(id, g_sceneCB.iteration, depth);

	// Ray state to be filled before the first TraceRay()
	float3 rayDir;
	float3 origin;
    
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    GenerateCameraRay(DispatchRaysIndex().xy, origin, rayDir);

	// Ray to be traced
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = rayDir;
    ray.TMin = 0.001;
    ray.TMax = 10000.0;

	// Payload: color with w coord indicating type of hit, origin of the new ray, direction of new ray
    RayPayload payload = { float4(INITIAL_COLOR.rgb, -1.0f), float3(0, 0, 0), float3(0, 0, 0)};


	// for loop over path tracing depth
	// given pay load data (missed, hit something), do something
	// case 1: hit nothing - stop here
	// case 2: hit light source - output accumulated color
	// case 3: hit something else, keep going with new direction
	// case 4: no more tracing and didnt hit light: stop here
	for (int i = 0; i < depth; i++) {
		TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);
		ComputeRngSeed(id, g_sceneCB.iteration, depth);
		if (payload.color.w == 1.0f) {
			// new ray has to be edited here
			ray.Origin = payload.rayOrigin;
			ray.Direction = payload.rayDir;
			payload.color = float4(payload.color.rgb, -1.0f);
			if (i == depth - 1) {
				// if this is the final depth, then stop here with a black color
				payload.color = BACKGROUND_COLOR;
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

	// TODO: needs to be edited with type of hit object later
	if (instanceId == 1) {
		hitType = 1; // object
	}
	else
	{
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

	if (hitType == 1) // Do both a R E F L E C C and a R E F R A C C
	{
		float indexOfRefraction = 1.2f; // TODO: Change this to be more general

		float VdotN = dot(-WorldRayDirection(), triangleNormal);
		bool leaving = VdotN < 0.f;
		float eI = leaving ? indexOfRefraction : 1.f;
		float eT = leaving ? 1.f : indexOfRefraction;

		float fresnel = EvaluateFresnelDielectric(VdotN, eI, eT) / abs(VdotN);

		if (Uniform01() < fresnel) {
			ReflectiveBounce(triangleNormal, hitPosition, hitType, payload);
			//SpecularReflection_BxDF(pathSegment, intersect, normal, m);
		}
		else {
			RefractiveBounce(triangleNormal, hitPosition, hitType, payload);
			//SpecularRefraction_BxDF(pathSegment, intersect, normal, m, rng);
		}
	}
	else if (hitType == 3) // Do a R E F R A C C
	{
		RefractiveBounce(triangleNormal, hitPosition, hitType, payload);
	}
	else if (hitType == 2) // Do a diffuse bounce
	{
		DiffuseBounce(triangleNormal, hitPosition, hitType, triangleUV, payload);
	}
	else if (hitType == 0) // I think this means its a light
	{
		float3 tex = text.SampleLevel(s1, triangleUV, 0);
		float3 color = payload.color.rgb * tex.rgb * 3.0f;
		payload.color = float4(color.xyz, hitType);
	}
	else // do a R E F L E C C
	{
		ReflectiveBounce(triangleNormal, hitPosition, hitType, payload);
	}
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
	payload.color = float4(BACKGROUND_COLOR.xyz, -1.0f); // -1 to indicate hit nothing
}

#endif // RAYTRACING_HLSL