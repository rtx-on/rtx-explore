#pragma once

class MiniFace : public ModelLoading::SceneObject {
public:
  explicit MiniFace(SceneObject& object)
    : object(object)
  {
  }

  bool SetTexture(BlockType block_type);

  void SetFacePos(glm::vec4 world, glm::vec4 normal)
  {
    glm::vec3 new_rotation{};
    glm::vec3 new_translation{};
    if (normal.x == -1.0f)
    {
      new_rotation.y = 90.0f;
    }
    else if (normal.x == 1.0f)
    {
      new_rotation.y = -90.0f;
      new_translation.x = 1.0f;
    }
    else if (normal.y == -1.0f)
    {
      new_rotation.x = -90.0f;
    }
    else if (normal.y == 1.0f)
    {
      new_rotation.x = 90.0f;
      new_translation.y = 1.0f;
    }
    else if (normal.z == -1.0f)
    {

    }
    else if (normal.z == 1.0f)
    {
      new_rotation.x = 180.0f;
      new_translation.z = 1.0f;
    }
    else 
    {
      std::cerr << "MINIFACE ERROR\n";
      exit(-1);
    }

    //scale to actual size
    new_translation *= object.scale.x;

    //update
    object.translation = glm::vec3(world) + new_translation;
    object.rotation = new_rotation;

    //rebuild transformation
    object.transformBuilt = false;
  }

  glm::vec3 GetTranslation() const
  {
    return this->object.translation;
  }

  void SetTranslation(const glm::vec3& translation)
  {
    object.transformBuilt = false;
    this->object.translation = translation;
  }

  glm::vec3 GetRotation() const
  {
    return this->object.rotation;
  }

  void SetRotation(const glm::vec3& rotation)
  {
    object.transformBuilt = false;
    this->object.rotation = rotation;
  }

  glm::vec3 GetScale() const
  {
    return this->object.scale;
  }

  void SetScale(const glm::vec3& scale)
  {
    object.transformBuilt = false;
    this->object.scale = scale;
  }

private:
  SceneObject& object;
};
