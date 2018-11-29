#pragma once

using Sprite = int;

const std::map<std::string, Sprite> texture_name_map = {
  { "a", 0 },
};

class MiniBlockManager {
public:
  explicit MiniBlockManager() = default;

  Scene* get_scene() const
  {
    return scene;
  }

  void set_scene(Scene* scene)
  {
    this->scene = scene;
    
    mini_blocks.clear();
    for (ModelLoading::SceneObject& object : scene->objects)
    {
      mini_blocks.emplace_back(MiniBlock(object));
    }
  }

  MiniBlock *AllocateMiniBlock() {
    if (current_mini_block >= mini_blocks.size()) {
      return nullptr;
    }

    return &mini_blocks[current_mini_block++];
  }

  void ResetMiniBlocks() { current_mini_block = 0; }

private:
  Scene *scene = nullptr;

  uint64_t current_mini_block = 0;
  std::vector<MiniBlock> mini_blocks;
};
