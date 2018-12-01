#include "stdafx.h"
#include "MiniFace.h"
#include <glm/glm/gtc/type_ptr.hpp>

bool MiniFace::SetTexture(BlockType block_type)
{
  if (object.info_resource.info.texture_offset != block_type)
  {
     //change resource in object
    object.info_resource.info.texture_offset = static_cast<UINT>(block_type);

    void* mapped_resource = nullptr;
    object.info_resource.d3d12_resource.resource->Map(0, nullptr, &mapped_resource);

    //change the resource on mapped memory
    memcpy(mapped_resource, &object.info_resource.info, sizeof(Info));

    object.info_resource.d3d12_resource.resource->Unmap(0, nullptr); 
  }
  return true;
}
