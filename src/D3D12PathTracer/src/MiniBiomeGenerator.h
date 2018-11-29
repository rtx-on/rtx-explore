#pragma once

class MiniBiomeGenerator {
public:
    static glm::vec2 random(glm::vec2 p) {
        glm::vec2 toSin = glm::vec2(glm::dot(p, glm::vec2(127.1,311.7)), glm::dot(p,glm::vec2(269.5,183.3)));
        glm::vec2 result = glm::sin(toSin) * 43758.5453f;
        return result - glm::floor(result);
    }

    static BlockType determineBiome(glm::vec2 gridCorner, glm::vec2 blockPos, float& p1, float& p2, float& p3, BlockType& b2, BlockType& b3);

    static glm::vec2 blockToGrid(int x, int z);
    static float divLength;
};
