#include "stdafx.h"
#include "MiniBlock.h"
#include <glm/glm/gtc/type_ptr.hpp>

bool MiniBlock::SetTexture(BlockType block_type)
{
  //change resource in object
  object.info_resource.info.texture_offset = static_cast<UINT>(block_type);

  CD3DX12_RANGE cd3_dx12_range(0, 0);

  void* mapped_resource = nullptr;
  object.info_resource.d3d12_resource.resource->Map(0, &cd3_dx12_range, &mapped_resource);

  //change the resource on mapped memory
  memcpy(mapped_resource, &object.info_resource.info, sizeof(Info));

  object.info_resource.d3d12_resource.resource->Unmap(0, &cd3_dx12_range);
  return true;
}
