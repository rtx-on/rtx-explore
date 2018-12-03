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

//NULL OFFSET IF INDEX OFFSET IS -1
#define NULL_OFFSET (-1)

#define AA 1
#define DOF 0

RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0);
RWTexture2D<float4> RenderTarget2 : register(u1);
StructuredBuffer<Vertex> Vertices[] : register(t0, space1);
ByteAddressBuffer Indices[] : register(t0, space2);
ConstantBuffer<Info> infos[] : register(b0, space3);
ConstantBuffer<Material> materials[] : register(b0, space4);
Texture2D text[] : register(t0, space5);
Texture2D normal_text[] : register(t0, space6);
SamplerState s1 : register(s0);
SamplerState s2 : register(s1);

ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);
ConstantBuffer<CubeConstantBuffer> g_cubeCB : register(b1);

static const float PI = 3.1415926535897932384626422832795028841971f;
static const float TWO_PI = 6.2831853071795864769252867665590057683943f;
static const float SQRT_OF_ONE_THIRD = 0.5773502691896257645091487805019574556476f;

static const float4 BACKGROUND_COLOR = float4(0,0,0,0);
static const float4 INITIAL_COLOR = float4(1, 1, 1, 0);
static const float4 SPECULAR_COLOR = float4(1, 1, 1, 1);
static const float INV_PI = 0.3183098861837906715377675267450287240689f;
static const float RED_WAVELENGTH_UM = 0.69f;
static const float BLUE_WAVELENGTH_UM = 0.47f;
static const float GREEN_WAVELENGTH_UM = 0.53f;

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

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
        float4 color;
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

// taken from https://en.wikipedia.org/wiki/Sellmeier_equation
// note: wavelength should be in micrometers
float SellmeierEquation(float wavelength)
{
	const float B1 = 1.03961212f;
	const float B2 = 0.231792344f;
	const float B3 = 1.01046945f;
	const float C1 = 0.00600069867f;
	const float C2 = 0.0200179144f;
	const float C3 = 0.0103560653f;

	float wavlenSquared = wavelength * wavelength;

	float term1 = (B1 * wavlenSquared) / (wavlenSquared - C1);
	float term2 = (B2 * wavlenSquared) / (wavlenSquared - C2);
	float term3 = (B3 * wavlenSquared) / (wavlenSquared - C3);

	return sqrt(1.0f + term1 + term2 + term3);
}

void GlassBounce(float3 triangleNormal, float3 hitPosition, float hitType, RayPayload payload)
{
	float wavelength = 0.0f;
	float rand = Uniform01();
	if (rand < 0.33333f)
	{
		// sample the red color and get red wavelength
		wavelength = RED_WAVELENGTH_UM;
		payload.color *= float4(3.0f, 0.0f, 0.0f, 0.0f);
	}
	else if (rand < 0.66666f)
	{
		// sample the green color
		wavelength = GREEN_WAVELENGTH_UM;
		payload.color *= float4(0.0f, 3.0f, 0.0f, 0.0f);
	}
	else
	{
		// sample the blue color
		wavelength = BLUE_WAVELENGTH_UM;
		payload.color *= float4(0.0f, 0.0f, 3.0f, 0.0f);
	}

	float indexOfRefraction = SellmeierEquation(wavelength);

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

void DiffuseBounce(uint texture_offset, uint material_offset, float emittance, float3 triangleNormal, float3 hitPosition, float hitType, float2 triangleUV, RayPayload payload)
{
	float3 newDir = CalculateRandomDirectionInHemisphere(triangleNormal);
	payload.rayDir = newDir;
	payload.rayOrigin = hitPosition + payload.rayDir * 0.01f;

	float3 color = BACKGROUND_COLOR.xyz;
	if (texture_offset != NULL_OFFSET)
	{
		float3 tex = text[texture_offset].SampleLevel(s1, triangleUV, 0);
		color = payload.color.rgb * tex.rgb;
	}
	else if (material_offset != NULL_OFFSET)
	{
		color = payload.color.rgb * materials[material_offset].diffuse;
	}

	payload.color = float4(color.xyz, emittance);
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
		ComputeRngSeed(id, g_sceneCB.iteration, i);


		if (payload.color.w == 0) {
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
	if (g_sceneCB.iteration == 1)
	{
		RenderTarget2[DispatchRaysIndex().xy].xyz = float3(0, 0, 0);
	}

	// TODO: Stream Compact here ?? thrust::remove_if on the payload

	// Write the raytraced color to the output texture.
	float3 color = float3(RenderTarget2[DispatchRaysIndex().xy].xyz);
        color += payload.color.xyz;
        RenderTarget2[DispatchRaysIndex().xy] = float4(color.xyz, 0.0f);

        // Average output color
	float r = clamp(color.r / g_sceneCB.iteration, 0, 1);
	float g = clamp(color.g / g_sceneCB.iteration, 0, 1);
	float b = clamp(color.b / g_sceneCB.iteration, 0, 1);
	float4 avgColor = float4(r, g, b, 0.0f);
	RenderTarget[DispatchRaysIndex().xy] = avgColor;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
	float3 hitPosition = HitWorldPosition();
	uint instanceId = InstanceID(); //Object id

	//use object id to index into info structure
	uint model_offset = infos[instanceId].model_offset;
	uint texture_offset = infos[instanceId].texture_offset;
	uint texture_normal_offset = infos[instanceId].texture_normal_offset;
	uint material_offset = infos[instanceId].material_offset;
	float4x4 rotation_matrix = infos[instanceId].rotation_matrix;

	float eta = 0;
	float reflectiveness = 0;
	float refractiveness = 0;
	float specular_exp = 0;
	float emittance = 0;

	if (material_offset != NULL_OFFSET)
	{
		eta = materials[material_offset].eta;
		reflectiveness = materials[material_offset].reflectiveness;
		refractiveness = materials[material_offset].refractiveness;
		specular_exp = materials[material_offset].specularExp;
		emittance = materials[material_offset].emittance;
	}

	// Get the base index of the triangle's first 16 bit index.
	uint indexSizeInBytes = 4;
	uint indicesPerTriangle = 3;
	uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	uint baseIndex = PrimitiveIndex() * triangleIndexStride;

        float hitType = emittance ? 1 : 0; // 1 is light, 0 is not

	// Load up 3 16 bit indices for the triangle.
	const uint3 indices = Indices[model_offset].Load3(baseIndex);

	// Retrieve corresponding vertex normals for the triangle vertices.
	float3 vertexNormals[3] = {
		Vertices[model_offset][indices[0]].normal,
		Vertices[model_offset][indices[1]].normal,
		Vertices[model_offset][indices[2]].normal
	};

        // Compute the triangle's normal.
	// This is redundant and done for illustration purposes 
	// as all the per-vertex normals are the same and match triangle's normal in this sample. 
        float3 triangleNormal = HitAttribute(vertexNormals, attr);

	float2 vertexUVs[3] = {
		Vertices[model_offset][indices[0]].texCoord,
		Vertices[model_offset][indices[1]].texCoord,
		Vertices[model_offset][indices[2]].texCoord
	};

        float2 triangleUV = HitAttribute2D(vertexUVs, attr);

        //if texture map, then sample that instead
        if (texture_normal_offset != NULL_OFFSET)
        {
          triangleNormal = normal_text[texture_normal_offset].SampleLevel(s1, triangleUV, 0);
        }

        //multiply by rotation/scale matrix to correct the normals 
	triangleNormal = mul(float4(triangleNormal, 1.0f), rotation_matrix).xyz;


	if (reflectiveness > 0.0f && refractiveness > 0.0f) // Do both a R E F L E C C and a R E F R A C C with fresnel effects
	{
		float indexOfRefraction = 1.2f; // TODO: Change this to be more general

		float VdotN = dot(-WorldRayDirection(), triangleNormal);
		bool leaving = VdotN < 0.f;
		float eI = leaving ? indexOfRefraction : 1.f;
		float eT = leaving ? 1.f : indexOfRefraction;

		float fresnel = EvaluateFresnelDielectric(VdotN, eI, eT) / abs(VdotN);

		if (Uniform01() < fresnel) {
			ReflectiveBounce(triangleNormal, hitPosition, hitType, payload);
		}
		else {
			GlassBounce(triangleNormal, hitPosition, hitType, payload);
		}
	}
	else if (reflectiveness > 0.0f) // do a R E F L E C C
	{
		ReflectiveBounce(triangleNormal, hitPosition, hitType, payload);
	}
	else if (refractiveness > 0.0f) // Do a R E F R A C C
	{
		GlassBounce(triangleNormal, hitPosition, hitType, payload);
	}
	else if (emittance > 0.0f) // I think this means its a light
	{

		float3 color = BACKGROUND_COLOR.xyz;
		if (texture_offset != NULL_OFFSET)
		{
			float3 tex = text[texture_offset].SampleLevel(s1, triangleUV, 0);
			color = payload.color.rgb * tex.rgb;
		}
		else if (material_offset != NULL_OFFSET)
		{
			color = payload.color.rgb * materials[material_offset].diffuse;
		}

		payload.color = float4(color.xyz, emittance);
	}
	else // Do a diffuse bounce
	{
		DiffuseBounce(texture_offset, material_offset, emittance, triangleNormal, hitPosition, hitType, triangleUV, payload);
	}
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
	payload.color = float4(BACKGROUND_COLOR.xyz, -1.0f); // -1 to indicate hit nothing
}

#endif // RAYTRACING_HLSL