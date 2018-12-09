#pragma once

#include "include/tiny_obj_loader.h"
#include "Mesh.h"

#include <string>

using namespace tinyobj;

namespace Model
{
	class MeshLoader
	{
	public:
		static std::vector<Mesh> MeshLoader::load_obj(std::string base_path, std::string object_name);
		const MeshLoader& get() const;
	private:
		MeshLoader() = default;
	};
}