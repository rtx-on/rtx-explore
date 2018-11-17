#pragma once

#include "model_loading/tiny_obj_loader.h"
#include "Mesh.h"

#include <string>

using namespace tinyobj;

namespace Model
{
	class MeshLoader
	{
	public:
		std::vector<Mesh> MeshLoader::load_obj(std::string obj_file_path);
		const MeshLoader& operator->() const;
	private:
		MeshLoader() = default;
	};
}