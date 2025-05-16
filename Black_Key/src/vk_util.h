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
		T& operator[](vk_util::MeshPassType pass)
		{
			switch (pass)
			{
			case vk_util::MeshPassType::Forward:
				return data[0];
			case vk_util::MeshPassType::Transparency:
				return data[1];
			case vk_util::MeshPassType::DirectionalShadow:
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
