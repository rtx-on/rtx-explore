#include "stdafx.h"
#include "MiniBiomeGenerator.h"

float MiniBiomeGenerator::divLength = 200.f;//noise fnct

float heightRandom(glm::vec2 p) {
    float result = sin(glm::dot(p, glm::vec2(12.9898, 4.1414))) * 43758.5453;
    return result - glm::floor(result);
}

BlockType selectBlock(glm::vec2 gridPoint) {
    float biomeFloat = heightRandom(gridPoint);
    if (biomeFloat < 0.5f) {
        return GRASS;
    } else if (biomeFloat < 0.8f){
        return SAND;
    } else if (biomeFloat < 0.9f){
        return SNOW;
    } else {
        return WATER;
    }
}

BlockType MiniBiomeGenerator::determineBiome(glm::vec2 gridCorner, glm::vec2 blockPos, float& p1, float& p2, float& p3, BlockType& b2, BlockType &b3) {
    float firstMinDist = INFINITY;
    glm::vec2 firstGridPoint = glm::vec2(0, 0); //worley point associated with block

    float secondMinDist = INFINITY;
    glm::vec2 secondGridPoint = glm::vec2(0, 0); //worley point associated with block

    float thirdMinDist = INFINITY;
    glm::vec2 thirdGridPoint = glm::vec2(0, 0); //worley point associated with block

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            glm::vec2 gridPoint = gridCorner + glm::vec2(x, y); //which cell grid
            glm::vec2 offset = MiniBiomeGenerator::random(gridPoint) * MiniBiomeGenerator::divLength; //offset from corner
            gridPoint *= MiniBiomeGenerator::divLength; //corner in world space
            gridPoint += offset; //vornoi cell point

            float distance = glm::distance(gridPoint, blockPos);

            if (distance < firstMinDist) {
                thirdMinDist = secondMinDist;
                thirdGridPoint = secondGridPoint;

                secondMinDist = firstMinDist;
                secondGridPoint = firstGridPoint;

                firstMinDist = distance;
                firstGridPoint = gridPoint;
            } else if (distance < secondMinDist) {
                thirdMinDist = secondMinDist;
                thirdGridPoint = secondGridPoint;

                secondMinDist = distance;
                secondGridPoint = gridPoint;
            } else if (distance < thirdMinDist) {
                thirdMinDist = distance;
                thirdGridPoint = gridPoint;
            }

        }
    }
    BlockType b1 = selectBlock(firstGridPoint);
    b2 = selectBlock(secondGridPoint);
    b3 = selectBlock(thirdGridPoint);

    float totalDist = firstMinDist + secondMinDist + thirdMinDist;
    p1 = firstMinDist / totalDist;
    p2 = secondMinDist / totalDist;
    p3 = thirdMinDist / totalDist;
    return b1;
}

glm::vec2 MiniBiomeGenerator::blockToGrid(int x, int z) {
    int gridX = (x / divLength) - (((x < 0) && (x % (int) divLength)) ? 1 : 0);
    int gridZ = (z / divLength) - (((z < 0) && (z % (int) divLength)) ? 1 : 0);
    return glm::vec2(gridX, gridZ);
}
