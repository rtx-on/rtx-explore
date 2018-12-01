#pragma once

class MiniFace : public ModelLoading::SceneObject {
public:
  explicit MiniFace(SceneObject& object)
    : object(object)
  {
  }

  bool SetTexture(BlockType block_type);

  void SetFacePos(glm::vec4 world, glm::vec4 a, glm::vec4 b, glm::vec4 c, glm::vec4 d)
  {
    glm::vec3 new_rotation{};
    glm::vec3 new_translation{};
    if (a.x == b.x == c.x == d.x)
    {
      //left face
      if (a.x == 0.0f)
      {
        new_rotation.y = -90.0f; 
      }
      //right face
      else
      {
        new_rotation.y = 90.0f; 
        new_translation.x = 1.0f;
      }
    }
    else if (a.y == b.y == c.y == d.y)
    {
      //bottom face
      if (a.y == 0.0f)
      {
        new_rotation.z = -90.0f; 
      }
      //top face
      else
      {
        new_rotation.z = 90.0f; 
        new_translation.y = 1.0f;
      }
    }
    else if (a.z == b.z == c.z == d.z)
    {
      //front face
      if (a.x == 0.0f)
      {
      }
      //back face
      else
      {
        new_rotation.x = 180.0f; 
        new_translation.z = 1.0f;
      }
    } 
    else 
    {
      std::cerr << "MINIFACE ERROR\n";
      exit(-1);
    }
    world = glm::vec4(1.0f);
    object.translation = glm::vec3(world) + new_translation;
    object.rotation = new_rotation;
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
