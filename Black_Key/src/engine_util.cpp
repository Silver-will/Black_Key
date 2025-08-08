#include "engine_util.h"

std::string GetAssetPath()
{
    //ENGINE_ASSET_PATH is predefined by cmake
#if defined (ENGINE_ASSET_PATH)
    return ENGINE_ASSET_PATH;
#else
    return "../..";
#endif
    return "assets/";
}

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

glm::vec4 BlackKey::roundVec4(glm::vec4 v)
{
    for (size_t i = 0; i < 4; i++)
    {
        auto val = v[i];
        auto lower_bound = floorf(val);
        auto decimal_val = val - lower_bound;
        val = decimal_val < 0.5 ? lower_bound : lower_bound + 1;
        v[i] = val;
    }
    return v;
}