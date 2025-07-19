#pragma once
#ifndef VK_MATERIAL_SYSTEM
#define VK_MATERIAL_SYSTEM
#include "vk_types.h"
#include "vk_util.h"
#include "vk_pipelines.h"


enum class VertexAttributeTemplate {
	DefaultVertex,
	DefaultVertexPosOnly
};

struct EffectBuilder {
	VertexAttributeTemplate vertexAttrib;
	struct ShaderEffect* effect{ nullptr };

	VkPrimitiveTopology topology;
	VkPipelineRasterizationStateCreateInfo rasterizerInfo;
	VkPipelineColorBlendAttachmentState colorBlendAttachmentInfo;
	VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
};

class MaterialSystem
{
};

namespace vkutil {
	struct ShaderPass {
		ShaderEffect* effect{ nullptr };
		VkPipeline pipeline{ VK_NULL_HANDLE };
		VkPipelineLayout layout{ VK_NULL_HANDLE };
	};

	struct SampledTexture {
		VkSampler sampler;
		VkImageView view;
	};
	struct ShaderParameters
	{

	};

	enum TransaparencyMode : uint8_t {
		transparecny
	};

	struct EffectTemplate {
		PerPassData<ShaderPass*> passShaders;

		ShaderParameters* defaultParameters;
		TransaparencyMode transparency;
	};

	struct MaterialData {
		std::vector<SampledTexture> textures;
		ShaderParameters* parameters;
		std::string baseTemplate;

		bool operator==(const MaterialData& other) const;

		size_t hash() const;
	};
}

#endif