#include "stdafx.h"

#include "Model.h"
#include "DXSample.h"
#include "Utilities.h"
#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/type_ptr.hpp>

using namespace ModelLoading;

FLOAT* SceneObject::getTransform3x4() {
	if (!transformBuilt) {
		transformBuilt = true;
		const float *matrix = (const float*)glm::value_ptr(utilityCore::buildTransformationMatrix(translation, rotation, scale));

		transform[0][0] = matrix[0];
		transform[0][1] = matrix[4];
		transform[0][2] = matrix[8];
		transform[0][3] = matrix[12];

		transform[1][0] = matrix[1];
		transform[1][1] = matrix[5];
		transform[1][2] = matrix[9];
		transform[1][3] = matrix[13];

		transform[2][0] = matrix[2];
		transform[2][1] = matrix[6];
		transform[2][2] = matrix[10];
		transform[2][3] = matrix[14];
	}
	return &transform[0][0];
}