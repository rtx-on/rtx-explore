#include "stdafx.h"
#include "MiniFBM.h"

//noise fnct
float random(glm::vec2 p) {
    float result = sin(glm::dot(p, glm::vec2(12.9898, 4.1414))) * 43758.5453;
    return result - glm::floor(result);
}

//noise interpolation
float noiseInterp(glm::vec2 p) {
    glm::vec2 i = glm::vec2(glm::floor(p));
    glm::vec2 f = p - i;
//    /std::cout << glm::to_string(f) << std::endl;

    float c1 = random(i);
    float c2 = random(i + glm::vec2(1.0, 0.0));
    float c3 = random(i + glm::vec2(0.0, 1.0));
    float c4 = random(i + glm::vec2(1.0, 1.0));

    float v1 = glm::mix(c1, c2, f.x);
    float v2 = glm::mix(c3, c4, f.x);

    return glm::mix(v1, v2, f.y);
}

//fbm height generation
float MiniFBM::fbm(const glm::vec2 &p) {
    float total = 0;
    float persistence = 0.3f;
    int samples = 8;
    float denominator = 0;
    for (int i = 0; i < samples; i++) {
        float freq = pow(2.f, i);
        float amp = pow(persistence, i);
        denominator += amp;
        total += noiseInterp(p * freq) * amp; //max is amp
    }

    return total / denominator;
}

