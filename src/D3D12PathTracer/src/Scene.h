#pragma once

#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>
#include <glm/glm/glm.hpp>

#include "Model.h"

using namespace std;

//static glm::vec3 calculate_geometric_normals(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2);

class D3D12RaytracingSimpleLighting;

using namespace std;
class Scene {
private:
	ifstream fp_in;
	int loadMaterial(string materialid);
	int loadTexture(string texid);
	int loadModel(string modelid);
	int loadObject(string objectid);
	int loadCamera();

public:
	Scene(string filename, D3D12RaytracingSimpleLighting* programState);
	~Scene();

	D3D12RaytracingSimpleLighting* programState;

	unordered_map<int, ModelLoading::Model> modelMap;
	unordered_map<int, ModelLoading::Texture> textureMap;
	unordered_map<int, ModelLoading::Material> materialMap;

	ModelLoading::Camera camera;

	vector<ModelLoading::SceneObject> objects;
};