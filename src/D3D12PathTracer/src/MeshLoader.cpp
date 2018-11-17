#include "stdafx.h"
#include "MeshLoader.h"

#define TINYOBJLOADER_IMPLEMENTATION 
#include "model_loading/tiny_obj_loader.h"

#include <iostream>

using namespace Model;

std::vector<Mesh> MeshLoader::load_obj(std::string obj_file_path)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string err;
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, obj_file_path.c_str());

	if (!err.empty()) { // `err` may contain warning message.
		std::cerr << err << std::endl;
	}

	if (!ret) {
		throw std::runtime_error("Tiny obj file could not be loaded\n");
		exit(1);
	}

	std::vector<Mesh> meshes;

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++) {

		Mesh mesh;

		//put name
		mesh.name = shapes[s].name;

		// Loop over vertices(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			int fv = shapes[s].mesh.num_face_vertices[f];

			Vertex face;

			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++) {
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
				tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
				tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
				tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
				tinyobj::real_t tx = attrib.texcoords[2 * idx.texcoord_index + 0];
				tinyobj::real_t ty = attrib.texcoords[2 * idx.texcoord_index + 1];
				// Optional: vertex colors
				// tinyobj::real_t red = attrib.colors[3*idx.vertex_index+0];
				// tinyobj::real_t green = attrib.colors[3*idx.vertex_index+1];
				// tinyobj::real_t blue = attrib.colors[3*idx.vertex_index+2];

				face.position = { vx, vy, vz };
				face.normal = { nx, ny, nz };
				face.texCoord = { tx, ty };

				// put in indices
				mesh.vertex_indices.emplace_back(idx.vertex_index);
				mesh.normal_indices.emplace_back(idx.normal_index);
				mesh.texture_indices.emplace_back(idx.texcoord_index);
			}
			index_offset += fv;

			// put in face
			mesh.vertices.emplace_back(face);

			// per-face material
			const auto material_id = shapes[s].mesh.material_ids[f];
			const auto& tiny_material = materials[material_id];

			Material material { tiny_material };

			mesh.materials.emplace_back(material);
		}
	}
	return {};
}

const MeshLoader& Model::MeshLoader::operator->() const
{
	const MeshLoader mesh_loader;
	return mesh_loader;
}
