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

uint32_t BlackKey::PreviousPow2(uint32_t v)
{
    uint32_t r = 1;
    while (r * 2 < v)
        r *= 2;

    return r;
}

uint32_t BlackKey::GetImageMipLevels(uint32_t width, uint32_t height)
{
    uint32_t result = 1;

    while (width > 1 || height > 1)
    {
        result++;
        width /= 2;
        height /= 2;
    }

    return result;
}