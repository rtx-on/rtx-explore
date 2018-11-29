#include "stdafx.h"
#include "MiniBlock.h"
#include <glm/glm/gtc/type_ptr.hpp>

void MiniBlock::SetTranformation()
{
  object.transformBuilt = false;
  FLOAT* a = object.getTransform3x4();
  //transform3x4 = (FLOAT*)glm::value_ptr(
    //glm::transpose(utilityCore::buildTransformationMatrix(translation, rotation, scale)));

}

bool MiniBlock::SetTexture(std::string texture_name)
{
  auto found_texture = texture_name_map.find(texture_name);
  if (found_texture != std::end(texture_name_map))
  {
    const Sprite& sprite = found_texture->second;

    //change resource in object
    object.info_resource.info.texture_offset = sprite;

    CD3DX12_RANGE cd3_dx12_range(0, 0);

    void* mapped_resource = nullptr;
    object.info_resource.d3d12_resource.resource->Map(0, &cd3_dx12_range, &mapped_resource);

    //change the resource on mapped memory
    memcpy(mapped_resource, &object.info_resource.info, sizeof(Info));

    object.info_resource.d3d12_resource.resource->Unmap(0, &cd3_dx12_range);
    return true;
  }
  return false;
}
