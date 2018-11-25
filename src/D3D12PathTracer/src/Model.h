#pragma once

#include "shaders/RayTracingHlslCompat.h"
#include <glm/glm/glm.hpp>

namespace ModelLoading {

	// Holds the buffer that contains the texture data
	struct Texture {
		int id;

		D3DBuffer texBuffer;
		ID3D12Resource* textureBufferUploadHeap;
	};

	// Holds a pointer to each type of texture
	struct TextureBundle {
		Texture* albedoTex;
		Texture* normalTex;
	};

	// Holds data for a specific material
	struct Material {
		int id;

		glm::vec3 diffuse;
		glm::vec3 specular;
		float specularExp;
		float eta;
		float reflectiveness;
		float refractiveness;
		float emittance;
	};

	// Holds the vertex and index buffer (triangulated) for a loaded model
	struct Model {
		int id;

		D3DBuffer indices;
		D3DBuffer vertices;
	};

	// Holds pointers to a model, textures, and a material
	class SceneObject {
	public:
		FLOAT* getTransform3x4();

		int id;

		Model* model;
		TextureBundle textures;
		Material* material;

		glm::vec3 translation; // parsed transform values
		glm::vec3 rotation;
		glm::vec3 scale;

	private:
		FLOAT transform[3][4]; // instance desc transform
		bool transformBuilt = false;
	};

	struct Camera {
		int width;
		int height;
		glm::vec2 fov;

		glm::vec3 eye;
		glm::vec3 lookat;
		glm::vec3 up;
		glm::vec3 right;
		glm::vec3 forward;

		float focalDist;
		float lensRadius;

		int maxIterations;
		int maxDepth;
	};
}