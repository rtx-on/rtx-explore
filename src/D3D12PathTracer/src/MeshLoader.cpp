#include "stdafx.h"
#include "MeshLoader.h"

#define TINYOBJLOADER_IMPLEMENTATION 
#include "model_loading/tiny_obj_loader.h"

using namespace Model;

//くそ
//Kuso

std::vector<Mesh> MeshLoader::load_obj(std::string base_path, std::string object_name)
{
	std::string object_path = base_path + object_name;

	std::vector<Mesh> meshes;

	objl::Loader loader;
	bool ret = loader.LoadFile(object_path);

	if(!ret)
	{
		throw std::runtime_error("obj file could not be loaded\n");
	}

	for(int i = 0; i < loader.LoadedMeshes.size(); i++)
	{
		objl::Mesh& obj_mesh = loader.LoadedMeshes[i];

		Mesh mesh;
		mesh.name = obj_mesh.MeshName;

		for(auto j = 0; j < obj_mesh.Vertices.size(); j++)
		{
			objl::Vertex& obj_vertex = obj_mesh.Vertices[j];

			Vertex vertex{};
			vertex.position = XMFLOAT3(obj_vertex.Position.X, obj_vertex.Position.Y, obj_vertex.Position.Z);
			vertex.normal = XMFLOAT3(obj_vertex.Normal.X, obj_vertex.Normal.Y, obj_vertex.Normal.Z);
			vertex.texCoord = XMFLOAT2(obj_vertex.TextureCoordinate.X, obj_vertex.TextureCoordinate.Y);
			
			mesh.vertices.emplace_back(vertex);
		}

		for (auto j = 0; j < obj_mesh.Indices.size(); j++)
		{
			mesh.vertex_indices.emplace_back(j);
		}
		if (!obj_mesh.MeshMaterial.name.empty())
		{
			Material material{ 0, obj_mesh.MeshMaterial };
			mesh.material = std::move(material);
		}
		meshes.emplace_back(std::move(mesh));
	}

	/*
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string err;
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, object_path.c_str(), base_path.c_str());

	if (!err.empty()) { // `err` may contain warning message.
		std::cerr << err << std::endl;
	}

	if (!ret) {
		throw std::runtime_error("Tiny obj file could not be loaded\n");
	}

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++) {

		Mesh mesh;

		//put name
		mesh.name = shapes[s].name;

		// Loop over vertices(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			int fv = shapes[s].mesh.num_face_vertices[f];

			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++) {
				// access to vertex
				int i = index_offset + v;
				tinyobj::index_t idx = shapes[s].mesh.indices[i];
				tinyobj::real_t vx = attrib.vertices[3 * i + 0];
				tinyobj::real_t vy = attrib.vertices[3 * i + 1];
				tinyobj::real_t vz = attrib.vertices[3 * i + 2];
				tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
				tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
				tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
				//tinyobj::real_t tx = attrib.texcoords[2 * idx.texcoord_index + 0];
				//tinyobj::real_t ty = attrib.texcoords[2 * idx.texcoord_index + 1];
				// Optional: vertex colors
				// tinyobj::real_t red = attrib.colors[3*idx.vertex_index+0];
				// tinyobj::real_t green = attrib.colors[3*idx.vertex_index+1];
				// tinyobj::real_t blue = attrib.colors[3*idx.vertex_index+2];

				Vertex vertex;
				vertex.position = XMFLOAT3(vx, vy, vz);
				vertex.normal = XMFLOAT3(nx, ny, nz);
				//vertex.texCoord = XMFLOAT2(tx, ty);
				
				// put in face
				mesh.vertices.emplace_back(vertex);

				// put in indices
				mesh.vertex_indices.emplace_back(idx.vertex_index);
				mesh.normal_indices.emplace_back(idx.normal_index);
				mesh.texture_indices.emplace_back(idx.texcoord_index);
			}
			index_offset += fv;

			// per-face material
			const auto material_id = shapes[s].mesh.material_ids[f];
			if(material_id >= 0 && static_cast<decltype(materials.size())>(material_id) < materials.size())
			{
				const auto& tiny_material = materials[material_id];

				bool already_exists = false;
				for(const auto& mat : mesh.materials)
				{
					if(mat.material_id == material_id)
					{
						already_exists = true;
						break;
					}
				}

				if(!already_exists)
				{
					//Material material{ material_id, tiny_material };

					//mesh.materials.emplace_back(std::move(material));
				}
			}
		}
		// for (unsigned int j = 0; j < attrib.vertices.size() / 3; j++)
		// {
		// 	//Vertex vertex{};
		// 	mesh.vertices[j].position = XMFLOAT3(attrib.vertices[j * 3], attrib.vertices[j * 3 + 1], attrib.vertices[j * 3 + 2]);
		//
		// 	// put in face
		// 	//mesh.vertices.emplace_back(vertex);
		// }
		// for (unsigned int j = 0; j < attrib.normals.size() / 3; j++)
		// {
		// 	mesh.vertices[j].normal = XMFLOAT3(attrib.normals[j * 3], attrib.normals[j * 3 + 1], attrib.normals[j * 3 + 2]);
		// }
		//
		// for (unsigned int j = 0; j < attrib.texcoords.size() / 2; j++)
		// {
		// 	try
		// 	{
		// 		mesh.vertices.at(j).texCoord = XMFLOAT2(attrib.texcoords[j * 2], 1.0 - attrib.texcoords[j * 2 + 1]);
		// 	}
		// 	catch(std::exception& e)
		// 	{
		// 		break;
		// 	}
		// }

		for (unsigned int j = 0; j < mesh.vertices.size(); j++)
		{
			// Vertex tempVertexInfo;
			// tempVertexInfo.positions = this->Vpositions[j];
			// tempVertexInfo.colors = glm::vec3(0.0f);
			//
			// if (normals.size() == 0)
			// 	tempVertexInfo.normals = glm::vec3(0.0f, 0.0f, 1.0f);
			// else
			// 	tempVertexInfo.normals = this->Vnormals[j];
			//
			// if (uvs.size() == 0)
			// 	tempVertexInfo.texcoords = glm::vec2(0.0f, 0.0f);
			// else
			// 	tempVertexInfo.texcoords = this->Vuvs[j];
			//
			// tempVertexInfo.tangents = glm::vec3(0.0f);
			// tempVertexInfo.bitangents = glm::vec3(0.0f);
			//
			// this->vertices.push_back(tempVertexInfo);
			//
			// maxCorner = glm::max(maxCorner, tempVertexInfo.positions);
			// minCorner = glm::min(minCorner, tempVertexInfo.positions);
		}

		meshes.emplace_back(mesh);
	}
	*/
	return meshes;
}

const MeshLoader& Model::MeshLoader::get() const
{
	static MeshLoader mesh_loader;
	return mesh_loader;
}
