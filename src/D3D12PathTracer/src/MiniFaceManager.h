#pragma once

class MiniFaceManager {
public:
  explicit MiniFaceManager() = default;

  Scene* GetScene() const
  {
    return scene;
  }

  void SetScene(Scene* scene)
  {
    this->scene = scene;
    
    mini_blocks.clear();
    for (ModelLoading::SceneObject& object : scene->objects)
    {
      mini_blocks.emplace_back(MiniFace(object));
    }
  }

  MiniFace *AllocateMiniFace() {
    if (current_mini_block >= mini_blocks.size()) {
      return nullptr;
    }

    return &mini_blocks[current_mini_block++];
  }

  void Reset() { current_mini_block = 0; }

private:
  Scene *scene = nullptr;

  uint64_t current_mini_block = 0;
  std::vector<MiniFace> mini_blocks;
};
