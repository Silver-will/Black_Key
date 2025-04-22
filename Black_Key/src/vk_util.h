#pragma once
#include"vk_types.h"
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

namespace vk_util {
	enum class MeshPassType {
		Forward,
		Transparent,
		Shadow,
		EarlyDepth
	};

	template<typename T>
	struct PerPassData {

	public:
		T& operator[](MeshpassType pass)
		{
			switch (pass)
			{
			case MeshpassType::Forward:
				return data[0];
			case MeshpassType::Transparency:
				return data[1];
			case MeshpassType::DirectionalShadow:
				return data[2];
			}
			assert(false);
			return data[0];
		};

		void clear(T&& val)
		{
			for (int i = 0; i < 3; i++)
			{
				data[i] = val;
			}
		}

	private:
		std::array<T, 3> data;
	};
};
