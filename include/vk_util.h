#pragma once
#include"vk_types.h"
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

namespace vkutil {
	void InsertDebugLabel(VkCommandBuffer cmd, std::string_view label, glm::vec4 color);
	enum class MeshPassType {
		Forward,
		Transparent,
		Shadow,
		EarlyDepth
	};

	template<typename T>
	struct PerPassData {

	public:
		T& operator[](vkutil::MeshPassType pass)
		{
			switch (pass)
			{
			case vkutil::MeshPassType::Forward:
				return data[0];
			case vkutil::MeshPassType::Transparency:
				return data[1];
			case vkutil::MeshPassType::DirectionalShadow:
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
