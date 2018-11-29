#pragma once

class MiniBlock : public ModelLoading::SceneObject {
public:
  explicit MiniBlock(SceneObject& object)
    : object(object)
  {
  }

  void SetTranformation();

  bool SetTexture(std::string texture_name);

  glm::vec3 get_translation() const
  {
    return this->object.translation;
  }

  void set_translation(const glm::vec3& translation)
  {
    this->object.translation = translation;
  }

  glm::vec3 get_rotation() const
  {
    return this->object.rotation;
  }

  void set_rotation(const glm::vec3& rotation)
  {
    this->object.rotation = rotation;
  }

  glm::vec3 get_scale() const
  {
    return this->object.scale;
  }

  void set_scale(const glm::vec3& scale)
  {
    this->object.scale = scale;
  }

private:
  SceneObject& object;
};
