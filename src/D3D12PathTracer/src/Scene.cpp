#include "stdafx.h"
#include "Scene.h"
#include "Utilities.h"
#include <cstring>
#include <glm/glm/gtc/matrix_inverse.hpp>
#include <glm/glm/gtx/string_cast.hpp>
#include "include/tiny_obj_loader.h"
#include "DirectXRaytracingHelper.h"
#include "D3D12RaytracingSimpleLighting.h"
#include "TextureLoader.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#include "tiny_gltf.h"
#include <glm/glm/gtc/type_ptr.inl>

using namespace tinyobj;
using namespace std;

void Scene::AllocateBufferOnGpu(void *pData, UINT64 width, ID3D12Resource **ppResource, std::wstring resource_name, CD3DX12_RESOURCE_DESC* resource_desc_ptr)
{
  CD3DX12_RESOURCE_DESC resource_desc;
  if (resource_desc_ptr == nullptr)
  {
    resource_desc = CD3DX12_RESOURCE_DESC::Buffer(width);    
  }
  else
  {
    resource_desc = *resource_desc_ptr;
  }

  auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  auto device = programState->GetDeviceResources()->GetD3DDevice();
  ThrowIfFailed(device->CreateCommittedResource(
    &defaultHeapProperties,
    D3D12_HEAP_FLAG_NONE,
    &resource_desc,
    D3D12_RESOURCE_STATE_COPY_DEST,
    nullptr,
    IID_PPV_ARGS(ppResource)));

  (*ppResource)->SetName(std::wstring(L"Default Heap " + resource_name).c_str());

  UINT64 textureUploadBufferSize;
  // this function gets the size an upload buffer needs to be to upload a texture to the gpu.
  // each row must be 256 byte aligned except for the last row, which can just be the size in bytes of the row
  // eg. textureUploadBufferSize = ((((width * numBytesPerPixel) + 255) & ~255) * (height - 1)) + (width * numBytesPerPixel);
  //textureUploadBufferSize = (((imageBytesPerRow + 255) & ~255) * (textureDesc.Height - 1)) + imageBytesPerRow;
  device->GetCopyableFootprints(&resource_desc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

  ID3D12Resource* textureBufferUploadHeap = programState->GetTextureBufferUploadHeap();
  // now we create an upload heap to upload our texture to the GPU
  ThrowIfFailed(device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
    D3D12_HEAP_FLAG_NONE, // no flags
    &CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize),
    // resource description for a buffer (storing the image data in this heap just to copy to the default heap)
    D3D12_RESOURCE_STATE_GENERIC_READ, // We will copy the contents from this heap to the default heap above
    nullptr,
    IID_PPV_ARGS(&textureBufferUploadHeap)));

  textureBufferUploadHeap->SetName(std::wstring(L"Upload Heap " + resource_name).c_str());

  // store vertex buffer in upload heap
  D3D12_SUBRESOURCE_DATA textureData = {};
  textureData.pData = pData; // pointer to our image data
  textureData.RowPitch = width; // size of all our triangle vertex data
  textureData.SlicePitch = width * resource_desc.Height; // also the size of our triangle vertex data    

  auto commandList = programState->GetDeviceResources()->GetCommandList();
  auto commandAllocator = programState->GetDeviceResources()->GetCommandAllocator();

  commandList->Reset(commandAllocator, nullptr);

  // Reset the command list for the acceleration structure construction.
  UpdateSubresources(commandList, *ppResource, textureBufferUploadHeap, 0, 0, 1, &textureData);

  // transition the texture default heap to a pixel shader resource (we will be sampling from this heap in the pixel shader to get the color of pixels)
  commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(*ppResource, D3D12_RESOURCE_STATE_COPY_DEST,
                                                                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                                                                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

  // Kick off texture uploading
  programState->GetDeviceResources()->ExecuteCommandList();

  // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
  programState->GetDeviceResources()->WaitForGpu();
};

void OuputAndReset(std::wstringstream& stream)
{
	OutputDebugStringW(stream.str().c_str());
	const static std::wstringstream initial;

	stream.str(std::wstring());
	stream.clear();
	stream.copyfmt(initial);
}


Scene::Scene(string filename, D3D12RaytracingSimpleLighting* programState) : programState(programState) {

        if (filename.find(".gltf") != std::string::npos)
        {
          ParseGLTF(filename);
        }
        else
        {
          ParseScene(filename);
        }
}

void Scene::ParseScene(std::string filename)
{
  std::wstringstream wstr;
  wstr << L"\n";
  wstr << L"------------------------------------------------------------------------------\n";
  wstr << L"Reading scene from " << filename.c_str() << L"\n";
  wstr << L"------------------------------------------------------------------------------\n";
  OuputAndReset(wstr);

  char* fname = (char*)filename.c_str();
  fp_in.open(fname);
  if (!fp_in.is_open()) {
    wstr << L"Error reading from file - aborting!\n";
    wstr << L"------------------------------------------------------------------------------\n";
    OuputAndReset(wstr);
    throw;
  }
  while (fp_in.good()) {
    string line;
    utilityCore::safeGetline(fp_in, line);
    if (!line.empty()) {
      vector<string> tokens = utilityCore::tokenizeString(line);
      std::string name{};
      if (tokens.size() == 3)
      {
        name = tokens[2];
      }
      if (strcmp(tokens[0].c_str(), "MATERIAL") == 0) {

        loadMaterial(tokens[1], name);
        std::cout << " " << endl;
      }
      else if (strcmp(tokens[0].c_str(), "MODEL") == 0) {
        loadModel(tokens[1]);
        std::cout << " " << endl;
      }
      else if (strcmp(tokens[0].c_str(), "DIFFUSE_TEXTURE") == 0) {
        loadDiffuseTexture(tokens[1]);
        std::cout << " " << endl;
      }
      else if (strcmp(tokens[0].c_str(), "NORMAL_TEXTURE") == 0) {
        loadNormalTexture(tokens[1]);
        std::cout << " " << endl;
      }
      else if (strcmp(tokens[0].c_str(), "OBJECT") == 0) {
        loadObject(tokens[1], name);
        std::cout << " " << endl;
      }
      else if (strcmp(tokens[0].c_str(), "GLTF") == 0) {
        ParseGLTF(tokens[1], false);
        std::cout << " " << endl;
      }
      else if (strcmp(tokens[0].c_str(), "CAMERA") == 0) {
        loadCamera();
        programState->UpdateCameraMatrices();
      }
    }
  }

  wstr << L"Done loading the scene file!\n";
  wstr << L"------------------------------------------------------------------------------\n";
  OuputAndReset(wstr);
}


template <typename Callback>
void Scene::RecurseGLTF(tinygltf::Model& model, tinygltf::Node& node, Callback callback)
{
  if (node.mesh != -1)
  {
    callback(model, node);
  }
  for (size_t i = 0; i < node.children.size(); i++)
  {
    RecurseGLTF(model, model.nodes[node.children[i]], callback);
  }
}


void Scene::ParseGLTF(std::string filename, bool make_light)
{
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
  if (!warn.empty())
  {
    printf("Warn: %s\n", warn.c_str());
  }

  if (!err.empty())
  {
    printf("Err: %s\n", err.c_str());
  }

  if (!ret)
  {
    printf("Failed to parse glTF\n");
    exit(-1);
  }

  int model_id = 0;
  int diffuse_texture_id = 0;
  int normal_texture_id = 0;
  int material_id = 0;
  int object_id = 0;

  if(!modelMap.empty())
  {
    model_id = (--std::end(modelMap))->first + 1;
  }
  if (!diffuseTextureMap.empty())
  {
    diffuse_texture_id = (--std::end(diffuseTextureMap))->first + 1;
  }
  if (!normalTextureMap.empty())
  {
    normal_texture_id = (--std::end(normalTextureMap))->first + 1;
  }
  if (!materialMap.empty())
  {
    material_id = (--std::end(materialMap))->first + 1;
  }
  if (!objects.empty())
  {
    object_id = objects.size();
  }

  const tinygltf::Scene &scene = model.scenes[model.defaultScene];
  for (size_t i = 0; i < scene.nodes.size(); ++i) 
  {
    RecurseGLTF(model, model.nodes[scene.nodes[i]], [&](tinygltf::Model &model, tinygltf::Node& node)
    {
      const tinygltf::Mesh &mesh = model.meshes[node.mesh];
      
      for (size_t i = 0; i < mesh.primitives.size(); ++i)
      {
        const tinygltf::Primitive& primitive = mesh.primitives[i];
        tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];

        std::vector<unsigned char> vertex_data;
        std::vector<unsigned char> indices_data;
        std::vector<unsigned char> texture_data;
        std::vector<unsigned char> normal_data;

        int vertex_stride = 0;
        int indices_stride = 0;
        int texture_stride = 0;
        int normal_stride = 0;

        //parse indices
        const tinygltf::BufferView& buffer_view = model.bufferViews[primitive.indices];
        const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];
        indices_stride = indexAccessor.ByteStride(buffer_view);
        indices_data = std::vector<unsigned char>(buffer.data.begin() + buffer_view.byteOffset, buffer.data.begin() + buffer_view.byteOffset + buffer_view.byteLength);

        for (auto& attrib : primitive.attributes)
        {
          tinygltf::Accessor accessor = model.accessors[attrib.second];
          int byteStride = accessor.ByteStride(model.bufferViews[accessor.bufferView]);

          int size = 1;
          if (accessor.type != TINYGLTF_TYPE_SCALAR)
          {
            size = accessor.type;
          }

          const tinygltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];
          const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

          if (attrib.first == "POSITION")
          {
            vertex_data = std::vector<unsigned char>(buffer.data.begin() + buffer_view.byteOffset, buffer.data.begin() + buffer_view.byteOffset + buffer_view.byteLength);
            vertex_stride = byteStride;
          }
          else if (attrib.first == "NORMAL")
          {
            normal_data = std::vector<unsigned char>(buffer.data.begin() + buffer_view.byteOffset, buffer.data.begin() + buffer_view.byteOffset + buffer_view.byteLength);
            normal_stride = byteStride;
          }
          else if (attrib.first == "TEXCOORD_0")
          {
            texture_data = std::vector<unsigned char>(buffer.data.begin() + buffer_view.byteOffset, buffer.data.begin() + buffer_view.byteOffset + buffer_view.byteLength);
            texture_stride = byteStride;
          }
        }

        //now make vertices
        std::vector<Vertex> vertices;
        for (int i = 0; i < vertex_data.size() / vertex_stride; i++)
        {
          Vertex v;
          float x = *((float *)&vertex_data[i * vertex_stride]);
          float y = *((float *)&vertex_data[i * vertex_stride + 4]);
          float z = *((float *)&vertex_data[i * vertex_stride + 8]);
          v.position = {x, y, z};
          //memcpy(&v.position, &vertex_data[i * vertex_stride], vertex_stride);
          if (texture_stride != 0 && i * texture_stride < texture_data.size())
          {
            memcpy(&v.texCoord, &texture_data[i * texture_stride], texture_stride);
          }
          if (normal_stride != 0)
          {
            memcpy(&v.normal, &normal_data[i * normal_stride], normal_stride);
          }

          vertices.emplace_back(std::move(v));
        }

        std::vector<Index> indices;
        for (int i = 0; i < indices_data.size() / indices_stride; i++)
        {
          Index index{};
          memcpy(&index, &indices_data[i * indices_stride], indices_stride);
          indices.emplace_back(index);
        }

        //allocate model
        ModelLoading::Model new_model;
        new_model.id = model_id;
        new_model.name = filename;
        new_model.indicesCount = indices.size();
        new_model.verticesCount = vertices.size();

        AllocateBufferOnGpu(indices.data(), indices.size() * sizeof(Index), &new_model.indices.resource, utilityCore::stringAndId(L"Vertices", model_id));
        AllocateBufferOnGpu(vertices.data(), vertices.size() * sizeof(Vertex), &new_model.vertices.resource, utilityCore::stringAndId(L"Indices", model_id));
        modelMap.insert({model_id++, std::move(new_model)});

        //allocate object as well
        ModelLoading::SceneObject new_object{};
        new_object.id = object_id++;
        new_object.name = mesh.name + ":" + filename;
        new_object.info_resource.info.model_offset = model_id - 1;
        new_object.info_resource.info.texture_offset = -1;
        new_object.info_resource.info.texture_normal_offset = -1;
        new_object.info_resource.info.material_offset = -1;
        new_object.model = &modelMap[new_object.info_resource.info.model_offset];

        //default scale to 1.0...
        new_object.scale = glm::vec3(1.0f);

        if (!node.translation.empty())
        {
          new_object.translation = { node.translation[0], node.translation[1], node.translation[2] };
        }
        if (!node.rotation.empty())
        {
          new_object.rotation = { node.rotation[0], node.rotation[1], node.rotation[2] };
          new_object.rotation = glm::degrees(new_object.rotation);
        }
        if (!node.scale.empty())
        {
          new_object.scale = { node.scale[0], node.scale[1], node.scale[2] };
        }
        if (!node.matrix.empty())
        {
          //TODO decompose to trans, rot, scale
          //std::vector<float> matrix_data = std::vector<float>(std::begin(node.matrix), std::end(node.matrix));
          //memcpy(new_object.getTransform3x4(), matrix_data.data(), 12 * sizeof(float));
        }

        //parse material TODO parse rest, for now, only get emittance
        tinygltf::Accessor& material_accessor = model.accessors[primitive.material];
        ModelLoading::MaterialResource material_resource{};
        material_resource.was_loaded_from_gltf = true;
        material_resource.id = material_id;
        material_resource.name = material_accessor.name;
        
        const tinygltf::Material material = model.materials[primitive.material];
        for (const auto& value : material.values)
        {
          //diffuse texture
          if (value.first == "baseColorTexture")
          {
            //iterating through gltf...
            const tinygltf::Parameter& parameter = value.second;
            int texture_index = parameter.TextureIndex();
            
            const tinygltf::Texture& texture = model.textures[texture_index];
            const tinygltf::Image& image = model.images[texture.source];

            std::experimental::filesystem::path file_path(filename);
            std::experimental::filesystem::path parent_path = file_path.parent_path();
            auto image_path = parent_path.append(image.uri).string();

            std::wstring wimage_path = utilityCore::string2wstring(image_path);

            //allocate texture
            ModelLoading::Texture new_texture;
            new_texture.id = diffuse_texture_id;
            new_texture.name = image_path;
            new_texture.was_loaded_from_gltf = true;

            LoadDiffuseTextureHelper(image_path, diffuse_texture_id++, new_texture);

            //make sure object points to this
            new_object.textures.albedoTex = &diffuseTextureMap[diffuse_texture_id - 1];
          }
        }

        for (const auto& value : material.additionalValues)
        {
          //diffuse texture
          if (value.first == "normalTexture")
          {
            //iterating through gltf...
            const tinygltf::Parameter& parameter = value.second;
            int texture_index = parameter.TextureIndex();
            
            const tinygltf::Texture& texture = model.textures[texture_index];
            const tinygltf::Image& image = model.images[texture.source];

            //get path to image
            std::experimental::filesystem::path file_path(filename);
            std::experimental::filesystem::path parent_path = file_path.parent_path();
            auto image_path = parent_path.append(image.uri).string();

            //allocate texture
            ModelLoading::Texture new_texture;
            new_texture.id = normal_texture_id;
            new_texture.name = image_path;
            new_texture.was_loaded_from_gltf = true;

            LoadNormalTextureHelper(image_path, normal_texture_id++, new_texture);

            //make sure object points to this
            new_object.textures.normalTex = &normalTextureMap[normal_texture_id - 1];
          } 
          else if (value.first == "emissiveFactor")
          {
            if (!value.second.number_array.empty())
            {
              material_resource.material.emittance = 1.0f;
            }
          }
        }

        //add material to map
        materialMap.insert({material_id++, std::move(material_resource)});

        //make object point to material
        new_object.material = &materialMap[material_id - 1];

        //add object
        objects.emplace_back(std::move(new_object));
      }
    });
  }

  //make sure that textures, normals have at least one entry
  if (diffuseTextureMap.empty())
  {
    ModelLoading::Texture useless_texture;

    std::vector<char> useless(256);

    D3D12_RESOURCE_DESC& texture_desc = useless_texture.textureDesc;
    texture_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, useless.size(), 1);

    AllocateBufferOnGpu(useless.data(), useless.size(), &useless_texture.texBuffer.resource, L"Null Diffuse Texture", &CD3DX12_RESOURCE_DESC(texture_desc));

    diffuseTextureMap.insert({0, std::move(useless_texture)});
  }
  if (normalTextureMap.empty())
  {
    ModelLoading::Texture useless_texture;

    std::vector<char> useless(256);

    D3D12_RESOURCE_DESC& texture_desc = useless_texture.textureDesc;
    texture_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, useless.size(), 1);

    AllocateBufferOnGpu(useless.data(), useless.size(), &useless_texture.texBuffer.resource, L"Null Normal Texture", &CD3DX12_RESOURCE_DESC(texture_desc));

    normalTextureMap.insert({0, std::move(useless_texture)});
  }

  //make a light
  // std::vector<Index> indices =
  // {
  //   0, 1, 2,
  //   0, 2, 3
  // };
  //
  //
  // std::vector<Vertex> vertices =
  // {
  //   {XMFLOAT3(-1.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f)},
  //   {XMFLOAT3(-1.0f, 0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f)},
  //   {XMFLOAT3(1.0f, 0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 0.0f)},
  //   {XMFLOAT3(1.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f)}
  // };

  if (make_light)
  {
    // Cube indices.
    std::vector<Index> indices =
    {
      3, 1, 0,
      2, 1, 3,

      6, 4, 5,
      7, 4, 6,

      11, 9, 8,
      10, 9, 11,

      14, 12, 13,
      15, 12, 14,

      19, 17, 16,
      18, 17, 19,

      22, 20, 21,
      23, 20, 22
    };

    // Cube vertices positions and corresponding triangle normals.
    std::vector<Vertex> vertices =
    {
      {XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},
      {XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},
      {XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},
      {XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},

      {XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},
      {XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},
      {XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},
      {XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},

      {XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},
      {XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},
      {XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},
      {XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},

      {XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},
      {XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},
      {XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},
      {XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},

      {XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},
      {XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},
      {XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},
      {XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},

      {XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
      {XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
      {XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
      {XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
    };

    //allocate model
    ModelLoading::Model new_model;
    new_model.id = model_id;
    new_model.indicesCount = indices.size();
    new_model.verticesCount = vertices.size();

    AllocateBufferOnGpu(indices.data(), indices.size() * sizeof(Index), &new_model.indices.resource,
                        utilityCore::stringAndId(L"Vertices", model_id));
    AllocateBufferOnGpu(vertices.data(), vertices.size() * sizeof(Vertex), &new_model.vertices.resource,
                        utilityCore::stringAndId(L"Indices", model_id));
    modelMap.insert({model_id++, std::move(new_model)});

    //allocate object as well
    ModelLoading::SceneObject new_object{};
    new_object.id = object_id++;
    new_object.info_resource.info.model_offset = model_id - 1;
    new_object.info_resource.info.texture_offset = -1;
    new_object.info_resource.info.texture_normal_offset = -1;
    new_object.info_resource.info.material_offset = -1;
    new_object.model = &modelMap[new_object.info_resource.info.model_offset];

    //default scale to 1.0...
    new_object.translation = glm::vec3(0.0f, -5.0f, 2.0f) + objects[0].translation;
    new_object.scale = glm::vec3(7.5f, 0.25f, 7.5f);

    ModelLoading::MaterialResource material_resource{};
    material_resource.id = material_id;

    material_resource.material.diffuse = XMFLOAT3(1.0f, 1.0f, 1.0f);
    material_resource.material.emittance = 1.0f;

    //add material to map
    materialMap.insert({material_id++, std::move(material_resource)});

    //make object point to material
    new_object.material = &materialMap[material_id - 1];

    //add object
    objects.emplace_back(std::move(new_object));
  }
}

int Scene::loadObject(string objectid, std::string name) {
	int id = atoi(objectid.c_str());

	std::wstringstream wstr;
	wstr << L"Loading OBJECT " << id << L"\n";
	OuputAndReset(wstr);

	ModelLoading::SceneObject newObject;
        newObject.name = name;

	string line;

	// LOAD MODEL (MUST EXIST)
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			wstr << L"----------------------------------------\n";
			wstr << L"Linking model...\n";
			OuputAndReset(wstr);

			vector<string> tokens = utilityCore::tokenizeString(line);
			int modelId = atoi(tokens[1].c_str());
			newObject.model = &(modelMap.find(modelId)->second);
		}
	}

	// LOAD TEXTURES IF EXIST
	{
		ModelLoading::TextureBundle texUsed;
		texUsed.albedoTex = nullptr;
		texUsed.normalTex = nullptr;
		newObject.textures = texUsed;

		// albedo tex
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);
			int texId = atoi(tokens[1].c_str());
			if (texId != -1) {
				wstr << L"----------------------------------------\n";
				wstr << L"Linking albedo texture...\n";
				OuputAndReset(wstr);
				newObject.textures.albedoTex = &(diffuseTextureMap.find(texId)->second);
			}
		}

		// normal tex
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);
			int texId = atoi(tokens[1].c_str());
			if (texId != -1) {
				wstr << L"----------------------------------------\n";
				wstr << L"Linking normal texture...\n";
				OuputAndReset(wstr);
				newObject.textures.normalTex = &(normalTextureMap.find(texId)->second);
			}
		}
	}
	
	// LOAD MATERIAL IF EXISTS
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);
			int matId = atoi(tokens[1].c_str());
			if (matId != -1) {
				wstr << L"----------------------------------------\n";
				wstr << L"Linking material...\n";
				OuputAndReset(wstr);
				newObject.material = &(materialMap.find(matId)->second);
			}
		}
	}

	// LOAD TRANSFORM (MUST EXIST)
	{
		wstr << L"----------------------------------------\n";
		wstr << L"Loading transform...\n";
		OuputAndReset(wstr);
		utilityCore::safeGetline(fp_in, line);
		while (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			if (strcmp(tokens[0].c_str(), "trans") == 0) {
				glm::vec3 t(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
				newObject.translation = t;
			}
			else if (strcmp(tokens[0].c_str(), "rotat") == 0) {
				glm::vec3 r(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
				newObject.rotation = r;
			}
			else if (strcmp(tokens[0].c_str(), "scale") == 0) {
				glm::vec3 s(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
				newObject.scale = s;
			}

			utilityCore::safeGetline(fp_in, line);
		}
	}

	newObject.id = id;
	objects.push_back(newObject);

	wstr << L"----------------------------------------\n";
	wstr << L"Done loading OBJECT " << id << L" !\n";
	wstr << L"------------------------------------------------------------------------------\n";
	OuputAndReset(wstr);

	return 1;
}

void Scene::LoadModelHelper(std::string path, int id, ModelLoading::Model& model)
{
  // load mesh here
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  tinyobj::attrib_t attrib;
  std::string err;
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path.c_str());

  std::vector<Index> indices;
  std::vector<Vertex> vertices;

  if (!ret)
  {
    throw std::runtime_error("failed to load Object!");
  }
  else
  {
    // loop over shapes
    int finalIdx = 0;
    for (unsigned int s = 0; s < shapes.size(); s++)
    {
      size_t index_offset = 0;
      // loop over  faces
      for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
      {
        int fv = shapes[s].mesh.num_face_vertices[f];
        std::vector<float>& positions = attrib.vertices;
        std::vector<float>& normals = attrib.normals;
        std::vector<float>& texcoords = attrib.texcoords;

        // loop over vertices in face, might be > 3
        for (size_t v = 0; v < fv; v++)
        {
          tinyobj::index_t index = shapes[s].mesh.indices[v + index_offset];

          Vertex vert;
          vert.position = XMFLOAT3(positions[index.vertex_index * 3], positions[index.vertex_index * 3 + 1],
                                   positions[index.vertex_index * 3 + 2]);
          if (index.normal_index != -1)
          {
            vert.normal = XMFLOAT3(normals[index.normal_index * 3], normals[index.normal_index * 3 + 1],
                                   normals[index.normal_index * 3 + 2]);
          }

          if (index.texcoord_index != -1)
          {
            vert.texCoord = XMFLOAT2(texcoords[index.texcoord_index * 2], 1 - texcoords[index.texcoord_index * 2 + 1]);
          }
          vertices.push_back(vert);
          indices.push_back((Index)(finalIdx + index_offset + v));
        }

        index_offset += fv;
      }
      finalIdx += index_offset;
    }

    Vertex* vPtr = vertices.data();
    Index* iPtr = indices.data();
    auto device = programState->GetDeviceResources()->GetD3DDevice();

    //now on gpu
    AllocateBufferOnGpu(iPtr, indices.size() * sizeof(Index), &model.indices.resource,
                        utilityCore::stringAndId(L"Vertices", id));
    AllocateBufferOnGpu(vPtr, vertices.size() * sizeof(Vertex), &model.vertices.resource,
                        utilityCore::stringAndId(L"Indices", id));

    model.verticesCount = vertices.size();
    model.indicesCount = indices.size();

    model.vertices_vec = std::move(vertices);
    model.indices_vec = std::move(indices);

    model.id = id;
    std::pair<int, ModelLoading::Model> pair(id, model);
    modelMap.insert(pair);
  }
}

void Scene::LoadDiffuseTextureHelper(std::string path, int id, ModelLoading::Texture& newTexture)
{
  // Load the image from file
  D3D12_RESOURCE_DESC& textureDesc = newTexture.textureDesc;
  int imageBytesPerRow;
  BYTE* imageData;

  wstring wpath = utilityCore::string2wstring(path);
  int imageSize = TextureLoader::LoadImageDataFromFile(&imageData, textureDesc, wpath.c_str(), imageBytesPerRow);

  // make sure we have data
  if (imageSize <= 0)
  {
    return throw std::exception("Image size < 0");
  }

  AllocateBufferOnGpu(imageData, imageBytesPerRow, &(newTexture.texBuffer.resource), utilityCore::stringAndId(L"Diffuse Texture", id), &CD3DX12_RESOURCE_DESC(textureDesc));
  ::free(imageData);

  newTexture.id = id;
  std::pair<int, ModelLoading::Texture> pair(id, newTexture);
  diffuseTextureMap.insert(pair);
}

void Scene::LoadNormalTextureHelper(std::string path, int id, ModelLoading::Texture& newTexture)
{
  // Load the image from file
  D3D12_RESOURCE_DESC& textureDesc = newTexture.textureDesc;
  int imageBytesPerRow;
  BYTE* imageData;

  wstring wpath = utilityCore::string2wstring(path);
  int imageSize = TextureLoader::LoadImageDataFromFile(&imageData, textureDesc, wpath.c_str(), imageBytesPerRow);

  // make sure we have data
  if (imageSize <= 0)
  {
    return throw std::exception("Image size < 0");
  }

  AllocateBufferOnGpu(imageData, imageBytesPerRow, &(newTexture.texBuffer.resource), utilityCore::stringAndId(L"Normal Texture", id), &CD3DX12_RESOURCE_DESC(textureDesc));
  ::free(imageData);

  newTexture.id = id;
  std::pair<int, ModelLoading::Texture> pair(id, newTexture);
  normalTextureMap.insert(pair);
}

int Scene::loadModel(string modelid) {
	int id = atoi(modelid.c_str());

	std::wstringstream wstr;
	wstr << L"Loading MODEL " << id << L"\n";
	wstr << L"----------------------------------------\n";
	OuputAndReset(wstr);

	ModelLoading::Model newModel;

	string line;

	//load model type
	utilityCore::safeGetline(fp_in, line);
	if (!line.empty() && fp_in.good()) {
		vector<string> tokens = utilityCore::tokenizeString(line);
                newModel.name = tokens[1];
                LoadModelHelper(tokens[1], id, newModel);
        }

	wstr << L"Done loading MODEL " << id << L" !\n";
	wstr << L"------------------------------------------------------------------------------\n";
	OuputAndReset(wstr);

	return 1;
}

int Scene::loadDiffuseTexture(string texid) {
	int id = atoi(texid.c_str());

	std::wstringstream wstr;
	wstr << L"Loading TEXTURE " << id << L"\n";
	wstr << L"----------------------------------------\n";
	OuputAndReset(wstr);

	ModelLoading::Texture newTexture;

	string line;

	//load texture
	utilityCore::safeGetline(fp_in, line);
	if (!line.empty() && fp_in.good()) {
		vector<string> tokens = utilityCore::tokenizeString(line);
                newTexture.name = tokens[1];

                LoadDiffuseTextureHelper(tokens[1], id, newTexture);
	}

	wstr << L"Done loading TEXTURE " << id << L" !\n";
	wstr << L"------------------------------------------------------------------------------\n";
	OuputAndReset(wstr);
	return 1;
}

int Scene::loadNormalTexture(string texid) {
	int id = atoi(texid.c_str());

	std::wstringstream wstr;
	wstr << L"Loading NORMAL TEXTURE " << id << L"\n";
	wstr << L"----------------------------------------\n";
	OuputAndReset(wstr);

	ModelLoading::Texture newTexture;

	string line;

	//load texture
	utilityCore::safeGetline(fp_in, line);
	if (!line.empty() && fp_in.good()) {
		vector<string> tokens = utilityCore::tokenizeString(line);
                newTexture.name = tokens[1];

                LoadNormalTextureHelper(tokens[1], id, newTexture);
	}

	wstr << L"Done loading NORMAL TEXTURE " << id << L" !\n";
	wstr << L"------------------------------------------------------------------------------\n";
	OuputAndReset(wstr);
	return 1;
}

int Scene::loadMaterial(string matid, std::string name) {
	int id = atoi(matid.c_str());

	std::wstringstream wstr;
	wstr << L"Loading MATERIAL " << id << L"\n";
	wstr << L"----------------------------------------\n";
	OuputAndReset(wstr);

	ModelLoading::MaterialResource newMat;
        newMat.name = name;
	string line;

	// LOAD RGB
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			newMat.material.diffuse = XMFLOAT3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
		}
	}

	// LOAD SPEC
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			newMat.material.specular = XMFLOAT3(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
		}
	}

	// LOAD SPECEX
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			float exp(atof(tokens[1].c_str()));
			newMat.material.specularExp = exp;
		}
	}

	// LOAD REFL
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			float refl(atof(tokens[1].c_str()));
			newMat.material.reflectiveness = refl;
		}
	}

	// LOAD REFR
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			float refr(atof(tokens[1].c_str()));
			newMat.material.refractiveness = refr;
		}
	}

        // LOAD ETA
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			float eta(atof(tokens[1].c_str()));
			newMat.material.eta = eta;
		}
	}


	// LOAD EMITTANCE
	{
		utilityCore::safeGetline(fp_in, line);
		if (!line.empty() && fp_in.good()) {
			vector<string> tokens = utilityCore::tokenizeString(line);

			float emit(atof(tokens[1].c_str()));
			newMat.material.emittance = emit;
		}
	}

	newMat.id = id;
        std::pair<int, ModelLoading::MaterialResource> pair(id, newMat);
	materialMap.insert(pair);

	wstr << L"Done loading MATERIAL " << id << L" !\n";
	wstr << L"------------------------------------------------------------------------------\n";
	OuputAndReset(wstr);
	return 1;
}

int Scene::loadCamera() {
	std::wstringstream wstr;
	wstr << L"Loading CAMERA\n";
	wstr << L"----------------------------------------\n";
	OuputAndReset(wstr);

	ModelLoading::Camera newCam;

	// load static properties
	for (int i = 0; i < 5; i++) {
		string line;
		utilityCore::safeGetline(fp_in, line);
		vector<string> tokens = utilityCore::tokenizeString(line);
                if (strcmp(tokens[0].c_str(), "fov") == 0) {
                        newCam.fov = atof(tokens[1].c_str());
		}
		else if (strcmp(tokens[0].c_str(), "depth") == 0) {
			newCam.maxDepth = atoi(tokens[1].c_str());
		}
                else if (strcmp(tokens[0].c_str(), "eye") == 0) {
                        glm::vec3 eye(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
                        XMFLOAT3 xm_eye(eye.x, eye.y, eye.z);
                        newCam.eye = XMLoadFloat3(&xm_eye);
                }
                else if (strcmp(tokens[0].c_str(), "lookat") == 0) {
                        glm::vec3 look_at(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
                        XMFLOAT3 xm_look_at(look_at.x, look_at.y, look_at.z);
                        newCam.lookat = XMLoadFloat3(&xm_look_at);
                }
                else if (strcmp(tokens[0].c_str(), "up") == 0) {
                        glm::vec3 up(atof(tokens[1].c_str()), atof(tokens[2].c_str()), atof(tokens[3].c_str()));
                        XMFLOAT3 xm_up(up.x, up.y, up.z);
                        newCam.up = XMLoadFloat3(&xm_up);
                }
	}

        camera = std::move(newCam);

	wstr << L"Done loading CAMERA !n";
	wstr << L"----------------------------------------\n";
	OuputAndReset(wstr);
	return 1;
}

D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& Scene::GetTopLevelDesc()
{
  if (!top_level_build_desc_allocated)
  {
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS &topLevelInputs =
        top_level_build_desc.Inputs;
    topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    topLevelInputs.Flags =
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    topLevelInputs.NumDescs = objects.size();
    topLevelInputs.pGeometryDescs = nullptr;
    topLevelInputs.Type =
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    top_level_build_desc_allocated = true;
  }

  return top_level_build_desc;
}

D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO& Scene::GetTopLevelPrebuildInfo(bool is_fallback, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice, ComPtr<ID3D12Device5> m_dxrDevice)
{
  if (!top_level_prebuild_info_allocated)
    {
      D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& desc = GetTopLevelDesc();
      top_level_prebuild_info = {};
      if (is_fallback) {
        m_fallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(
            &desc.Inputs, &top_level_prebuild_info);
      } else // DirectX Raytracing
      {
        m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(
            &desc.Inputs, &top_level_prebuild_info);
      }
      ThrowIfFalse(top_level_prebuild_info.ResultDataMaxSizeInBytes > 0);

      top_level_prebuild_info_allocated = true;
    }
    return top_level_prebuild_info;
}

ComPtr<ID3D12Resource> Scene::GetTopLevelScratchAS(bool is_fallback, ComPtr<ID3D12Device> device, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice, ComPtr<ID3D12Device5> m_dxrDevice)
{
  
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO&
    topLevelPrebuildInfo =
        GetTopLevelPrebuildInfo(is_fallback, m_fallbackDevice, m_dxrDevice);
    AllocateUAVBuffer(device.Get(), topLevelPrebuildInfo.ScratchDataSizeInBytes,
                      &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                      L"TopLevelScratchResourceScene");
    return scratchResource;
}

ComPtr<ID3D12Resource> Scene::GetTopAS(bool is_fallback, ComPtr<ID3D12Device> device, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice, ComPtr<ID3D12Device5> m_dxrDevice)
{
  
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO& topLevelPrebuildInfo = GetTopLevelPrebuildInfo(is_fallback, m_fallbackDevice, m_dxrDevice);
    D3D12_RESOURCE_STATES initialResourceState;
    if (is_fallback) {
      initialResourceState =
          m_fallbackDevice->GetAccelerationStructureResourceState();
    } else // DirectX Raytracing
    {
      initialResourceState =
          D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    }

    AllocateUAVBuffer(device.Get(),
                      topLevelPrebuildInfo.ResultDataMaxSizeInBytes,
                      &m_topLevelAccelerationStructure, initialResourceState,
                      L"TopLevelASScene");
    return m_topLevelAccelerationStructure;
}

ComPtr<ID3D12Resource> Scene::GetInstanceDescriptors(
    bool is_fallback, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice,
    ComPtr<ID3D12Device5> m_dxrDevice) {
  
    auto device = programState->GetDeviceResources()->GetD3DDevice();
    if (is_fallback)
    {
      std::vector<D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC> instanceDescArray;
      for (int i = 0; i < objects.size(); i++) {
        D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC instanceDesc = {};

        ModelLoading::SceneObject& obj = objects[i];
        ModelLoading::Model* model = obj.model;

        memcpy(instanceDesc.Transform, obj.getTransform3x4(), 12 * sizeof(FLOAT));
		
        instanceDesc.InstanceMask = 0xFF;
        instanceDesc.InstanceID = i; // TODO MAKE SURE THIS MATCHES THE ONE BELOW (FOR ACTUAL RAYTRACING)
        //instanceDesc.InstanceContributionToHitGroupIndex = 0;

        if (model != nullptr)
        {
          UINT numBufferElements = static_cast<UINT>(model->GetPreBuild(is_fallback, m_fallbackDevice, m_dxrDevice).ResultDataMaxSizeInBytes) / sizeof(UINT32);

          instanceDesc.AccelerationStructure = model->GetFallBackWrappedPoint(programState, is_fallback, m_fallbackDevice, m_dxrDevice, numBufferElements);

          instanceDescArray.push_back(instanceDesc);
        }
      }

      AllocateUploadBuffer(device, instanceDescArray.data(),
                           sizeof(D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC) * instanceDescArray.size(),
                           &instanceDescs, L"InstanceDescs");

      //programState->GetDeviceResources()->ExecuteCommandList();
      //programState->GetDeviceResources()->GetCommandList()->Reset(programState->GetDeviceResources()->GetCommandAllocator(), nullptr);
    }
    else
    {
      std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescArray;
      for (int i = 0; i < objects.size(); i++) {
        D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};

        ModelLoading::SceneObject obj = objects[i];
        ModelLoading::Model* model = obj.model;

        memcpy(instanceDesc.Transform, obj.getTransform3x4(), 12 * sizeof(FLOAT));
        instanceDesc.InstanceMask = 0xFF;
        instanceDesc.InstanceID = i; // TODO MAKE SURE THIS MATCHES THE ONE BELOW (FOR ACTUAL RAYTRACING)
        //instanceDesc.InstanceContributionToHitGroupIndex = 0;
        if (model != nullptr)
        {
          UINT numBufferElements =
              static_cast<UINT>(model->GetPreBuild(is_fallback, m_fallbackDevice, m_dxrDevice).ResultDataMaxSizeInBytes) / sizeof(UINT32);
       
          instanceDesc.AccelerationStructure =
              model->GetBottomAS(is_fallback, device, m_fallbackDevice, m_dxrDevice)->GetGPUVirtualAddress();

        
          instanceDescArray.push_back(instanceDesc);
        }
      }

      AllocateUploadBuffer(device, instanceDescArray.data(),
                           sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instanceDescArray.size(),
                           &instanceDescs, L"InstanceDescs");
    }
    return instanceDescs;
}

WRAPPED_GPU_POINTER Scene::GetWrappedGPUPointer(bool is_fallback, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice, ComPtr<ID3D12Device5> m_dxrDevice)
{
  
    UINT numBufferElements =
        static_cast<UINT>(GetTopLevelPrebuildInfo(is_fallback, m_fallbackDevice, m_dxrDevice).ResultDataMaxSizeInBytes) / sizeof(UINT32);
        return programState->CreateFallbackWrappedPointer(m_topLevelAccelerationStructure.Get(), numBufferElements); 
}

void Scene::FinalizeAS()
{
  
    for (auto& model_pair : modelMap)
    {
      ModelLoading::Model &model = model_pair.second;
      model.FinalizeAS();
    }

    auto& topLevelBuildDesc = GetTopLevelDesc();
    topLevelBuildDesc.DestAccelerationStructureData =
        m_topLevelAccelerationStructure->GetGPUVirtualAddress();
        topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
        topLevelBuildDesc.Inputs.InstanceDescs = instanceDescs->GetGPUVirtualAddress();
  }

  void Scene::BuildAllAS(bool is_fallback, ComPtr<ID3D12RaytracingFallbackDevice> m_fallbackDevice,
      ComPtr<ID3D12Device5> m_dxrDevice, ComPtr<ID3D12RaytracingFallbackCommandList> fbCmdLst, ComPtr<ID3D12GraphicsCommandList5> rtxCmdList) {
    auto commandList =
        programState->GetDeviceResources()->GetCommandList();
    auto device = programState->GetDeviceResources()->GetD3DDevice();
    if (is_fallback) {
      // Set the descriptor heaps to be used during acceleration structure build
      // for the Fallback Layer.
        ID3D12DescriptorHeap *pDescriptorHeaps[] = { programState->GetDescriptorHeap().Get() };
        fbCmdLst->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);

        for (auto& model_pair : modelMap)
        {
          ModelLoading::Model &model = model_pair.second;
          fbCmdLst.Get()->BuildRaytracingAccelerationStructure(&model.GetBottomLevelBuildDesc(), 0, nullptr);
          commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(model.GetBottomAS(is_fallback, device, m_fallbackDevice, m_dxrDevice).Get()));
        }
        fbCmdLst->BuildRaytracingAccelerationStructure(&GetTopLevelDesc(), 0, nullptr);
    } else {
        for (auto& model_pair : modelMap)
        {
           ModelLoading::Model &model = model_pair.second;
          rtxCmdList.Get()->BuildRaytracingAccelerationStructure(&model.GetBottomLevelBuildDesc(), 0, nullptr);
          commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(model.GetBottomAS(is_fallback, device, m_fallbackDevice, m_dxrDevice).Get()));
        }
       rtxCmdList->BuildRaytracingAccelerationStructure(&GetTopLevelDesc(), 0, nullptr);
    }
    // Kick off acceleration structure construction.
    programState->GetDeviceResources()->ExecuteCommandList();

    // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
    programState->GetDeviceResources()->WaitForGpu();
}

void Scene::AllocateResourcesInDescriptorHeap()
{
  auto device = programState->GetDeviceResources()->GetD3DDevice();

  for (auto& model_pair : modelMap)
  {
    auto& newModel = model_pair.second;
    programState->CreateBufferSRV(&newModel.vertices, newModel.verticesCount, sizeof(Vertex));
  }

  for (auto& model_pair : modelMap)
  {
    auto& newModel = model_pair.second;
    programState->CreateBufferSRV(&newModel.indices, newModel.indicesCount * sizeof(Index) / 4, 0);
  }

  for (auto& object : objects)
  {
    ModelLoading::InfoResource& info_resource = object.info_resource;

    //DEFAULT TO NEGATIVE
    memset(&info_resource.info, -1, sizeof(info_resource.info));
    info_resource.info.diffuse_sampler_offset = 0;
    info_resource.info.normal_sampler_offset = 0;

    if(object.model != nullptr)
    {
      info_resource.info.model_offset = object.model->id;
    }

    if (object.textures.albedoTex != nullptr)
    {
      info_resource.info.texture_offset = object.textures.albedoTex->id;
      info_resource.info.diffuse_sampler_offset = object.textures.albedoTex->sampler_offset;
    }
    
    if (object.textures.normalTex != nullptr)
    {
      info_resource.info.texture_normal_offset = object.textures.normalTex->id;
      info_resource.info.normal_sampler_offset = object.textures.normalTex->sampler_offset;
    }

    if (object.material != nullptr)
    {
      info_resource.info.material_offset = object.material->id;
    }

    XMVECTOR rotation_vector = XMLoadFloat3(&XMFLOAT3((object.rotation.x), (object.rotation.y), (object.rotation.z)));
    XMMATRIX rotation = XMMatrixRotationRollPitchYawFromVector(rotation_vector);
    XMMATRIX scale = XMMatrixScaling(object.scale.x, object.scale.y, object.scale.z);

	glm::mat4 transformCol = utilityCore::buildTransformationMatrix(glm::vec4(0,0,0,1), object.rotation, object.scale);
	glm::mat4 transformRow = glm::transpose(transformCol);

	float* mat = glm::value_ptr(transformRow);

	info_resource.info.rotation_scale_matrix = XMMATRIX(mat);

    // Create the constant buffer memory and map the CPU and GPU addresses
    const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // Allocate one constant buffer per frame, since it gets updated every frame.
    size_t cbSize = (sizeof(Info) + 255 ) & ~255; //align to 256 for CBV
    const D3D12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &constantBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&info_resource.d3d12_resource.resource)));

    info_resource.d3d12_resource.resource->SetName(
        utilityCore::stringAndId(L"InfoResourceObject ", object.id).c_str());

    UINT descriptorIndex = programState->AllocateDescriptor(&info_resource.d3d12_resource.cpuDescriptorHandle);
    info_resource.d3d12_resource.gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(programState->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, *programState->GetDescriptorSize());

    // create SRV descriptor
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = info_resource.d3d12_resource.resource->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = cbSize;

    device->CreateConstantBufferView(&cbvDesc, info_resource.d3d12_resource.cpuDescriptorHandle);

    // Map the constant buffer and cache its heap pointers.
    // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    Info *mapped_data;
    ThrowIfFailed(info_resource.d3d12_resource.resource->Map(0, &readRange, reinterpret_cast<void**>(&mapped_data)));
    memcpy(mapped_data, &info_resource.info, sizeof(Info));
    info_resource.d3d12_resource.resource->Unmap(0, &readRange);
  }

  for (auto& material_pair : materialMap)
  {
    int material_id = material_pair.first;
    ModelLoading::MaterialResource& material = material_pair.second;

    // Create the constant buffer memory and map the CPU and GPU addresses
    const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // Allocate one constant buffer per frame, since it gets updated every frame.
    size_t cbSize = (sizeof(Material) + 255 ) & ~255; //align to 256 for CBV
    const D3D12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &constantBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&material.d3d12_material_resource.resource)));
    material.d3d12_material_resource.resource->SetName(
        utilityCore::stringAndId(L"Material ", material_id).c_str());

    UINT descriptorIndex = programState->AllocateDescriptor(&material.d3d12_material_resource.cpuDescriptorHandle);
    material.d3d12_material_resource.gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(programState->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, *programState->GetDescriptorSize());

    // create SRV descriptor
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = material.d3d12_material_resource.resource->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = cbSize;

    device->CreateConstantBufferView(&cbvDesc, material.d3d12_material_resource.cpuDescriptorHandle);

    // Map the constant buffer and cache its heap pointers.
    // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    Material *material_mapped_data;
    ThrowIfFailed(material.d3d12_material_resource.resource->Map(0, &readRange, reinterpret_cast<void**>(&material_mapped_data)));
    memcpy(material_mapped_data, &material.material, sizeof(Material));
    material.d3d12_material_resource.resource->Unmap(0, &readRange);

  }

  //allocate GPU memory for textures into diffuse/normals
  for (auto& texture_pair : diffuseTextureMap)
  {
    auto &newTexture = texture_pair.second;
    UINT descriptorIndex = programState->AllocateDescriptor(&newTexture.texBuffer.cpuDescriptorHandle);

    // create SRV descriptor
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = newTexture.textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(newTexture.texBuffer.resource.Get(), &srvDesc, newTexture.texBuffer.cpuDescriptorHandle);

    // used to bind to the root signature
    newTexture.texBuffer.gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(programState->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, *programState->GetDescriptorSize());
  }

  for (auto& texture_pair : normalTextureMap)
  {
    auto &newTexture = texture_pair.second;
    UINT descriptorIndex = programState->AllocateDescriptor(&newTexture.texBuffer.cpuDescriptorHandle);

    // create SRV descriptor
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = newTexture.textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(newTexture.texBuffer.resource.Get(), &srvDesc, newTexture.texBuffer.cpuDescriptorHandle);

    // used to bind to the root signature
    newTexture.texBuffer.gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(programState->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, *programState->GetDescriptorSize());
  }

}
