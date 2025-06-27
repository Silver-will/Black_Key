#include "engine_util.h"


glm::vec4 BlackKey::Vec3Tovec4(glm::vec3 v, float fill) {
	glm::vec4 ret;
	ret.x = v.x;
	ret.y = v.y;
	ret.z = v.z;
	ret.w = fill == FLT_MAX ? 0 : fill;
	return ret;
}

glm::vec4 BlackKey::NormalizePlane(glm::vec4 p)
{
	return p / glm::length(glm::vec3(p));
}