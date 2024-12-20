#include "graphics.h"
#include "camera.h"
#include "vk_initializers.h"
#include "vk_images.h"
#include "vk_engine.h"
#include "vk_pipelines.h"

bool black_key::is_visible(const RenderObject& obj, const glm::mat4& viewproj) {
	std::array<glm::vec3, 8> corners{
		glm::vec3 { 1, 1, 1 },
		glm::vec3 { 1, 1, -1 },
		glm::vec3 { 1, -1, 1 },
		glm::vec3 { 1, -1, -1 },
		glm::vec3 { -1, 1, 1 },
		glm::vec3 { -1, 1, -1 },
		glm::vec3 { -1, -1, 1 },
		glm::vec3 { -1, -1, -1 },
	};

	glm::mat4 matrix = viewproj * obj.transform;

	glm::vec3 min = { 1.5, 1.5, 1.5 };
	glm::vec3 max = { -1.5, -1.5, -1.5 };

	for (int c = 0; c < 8; c++) {
		// project each corner into clip space
		glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + (corners[c] * obj.bounds.extents), 1.f);

		// perspective correction
		v.x = v.x / v.w;
		v.y = v.y / v.w;
		v.z = v.z / v.w;

		min = glm::min(glm::vec3{ v.x, v.y, v.z }, min);
		max = glm::max(glm::vec3{ v.x, v.y, v.z }, max);
	}

	// check the clip space box is within the view
	if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f) {
		return false;
	}
	else {
		return true;
	}
}

void black_key::generate_irradiance_cube(VulkanEngine* engine)
{
	//Created irradiance cubemap mage
	VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
	uint32_t dim = 64;
	uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;
	engine->IBL._irradianceCube = vkutil::create_cubemap_image(VkExtent3D{ dim,dim,1 }, engine, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, true);

	//Create image sampler
	VkSamplerCreateInfo cubeSampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	cubeSampl.magFilter = VK_FILTER_LINEAR;
	cubeSampl.minFilter = VK_FILTER_LINEAR;
	cubeSampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	cubeSampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	cubeSampl.addressModeV = cubeSampl.addressModeU;
	cubeSampl.addressModeW = cubeSampl.addressModeU;
	cubeSampl.mipLodBias = 0.0f;
	cubeSampl.minLod = 0.0f;
	cubeSampl.maxLod = numMips;
	cubeSampl.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	vkCreateSampler(engine->_device, &cubeSampl, nullptr, &engine->IBL._irradianceCubeSampler);

	//Draw target Image
	AllocatedImage drawImage;
	VkExtent3D drawImageExtent{
		dim,
		dim,
		1
	};
	drawImage.imageExtent = drawImageExtent;
	drawImage.imageFormat = format;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VkImageCreateInfo rimg_info = vkinit::image_create_info(drawImage.imageFormat, drawImageUsages, drawImageExtent, 1);

	//for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//allocate and create the image
	vmaCreateImage(engine->_allocator, &rimg_info, &rimg_allocinfo, &drawImage.image, &drawImage.allocation, nullptr);
	vmaSetAllocationName(engine->_allocator, drawImage.allocation, "irradiance pass image");

	VkDescriptorSetLayout irradianceSetLayout;
	DescriptorLayoutBuilder builder;
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	irradianceSetLayout = builder.build(engine->_device, VK_SHADER_STAGE_FRAGMENT_BIT);

	#define M_PI       3.14159265358979323846
	struct PushBlock {
		glm::mat4 mvp;
		// Sampling deltas
		float deltaPhi = (2.0f * float(M_PI)) / 180.0f;
		float deltaTheta = (0.5f * float(M_PI)) / 64.0f;
	} pushBlock;

	//Pipeline setup
	VkShaderModule irradianceVertexShader;
	if (!vkutil::load_shader_module("shaders/filter_cube.vert.spv", engine->_device, &irradianceVertexShader)) {
		fmt::print("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule irradianceFragmentShader;
	if (!vkutil::load_shader_module("shaders/irradiance_cube.frag.spv", engine->_device, &irradianceFragmentShader)) {
		fmt::print("Error when building the shadow fragment shader module\n");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(PushBlock);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayout layouts[] = {irradianceSetLayout};

	VkPipelineLayoutCreateInfo irradiance_layout_info = vkinit::pipeline_layout_create_info();
	irradiance_layout_info.setLayoutCount = 1;
	irradiance_layout_info.pSetLayouts = layouts;
	irradiance_layout_info.pPushConstantRanges = &matrixRange;
	irradiance_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout irradianceLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &irradiance_layout_info, nullptr, &irradianceLayout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(irradianceVertexShader, irradianceFragmentShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(false, false, VK_COMPARE_OP_LESS_OR_EQUAL);
	pipelineBuilder._pipelineLayout = irradianceLayout;

	pipelineBuilder.set_color_attachment_format(drawImage.imageFormat);

	auto irradiancePipeline = pipelineBuilder.build_pipeline(engine->_device);

	std::vector<glm::mat4> matrices = {
		// POSITIVE_X
		glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_X
		glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// POSITIVE_Y
		glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_Y
		glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// POSITIVE_Z
		glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_Z
		glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
	};
}

void black_key::generate_prefiltered_cubemap(VulkanEngine* engine)
{
	VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
	uint32_t dim = 512;
	uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;
	engine->IBL._preFilteredCube = vkutil::create_cubemap_image(VkExtent3D{ dim,dim,1 }, engine, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, true);
	//Draw target Image
	AllocatedImage drawImage;
	VkExtent3D drawImageExtent{
		dim,
		dim,
		1
	};
	drawImage.imageExtent = drawImageExtent;
	drawImage.imageFormat = format;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VkImageCreateInfo rimg_info = vkinit::image_create_info(drawImage.imageFormat, drawImageUsages, drawImageExtent, 1);

	//for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//allocate and create the image
	vmaCreateImage(engine->_allocator, &rimg_info, &rimg_allocinfo, &drawImage.image, &drawImage.allocation, nullptr);
	vmaSetAllocationName(engine->_allocator, drawImage.allocation, "irradiance pass image");

	VkDescriptorSetLayout preFilterSetLayout;
	DescriptorLayoutBuilder builder;
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	preFilterSetLayout = builder.build(engine->_device, VK_SHADER_STAGE_FRAGMENT_BIT);


#define M_PI       3.14159265358979323846
	struct PushBlock {
		glm::mat4 mvp;
		float roughness;
		uint32_t numSamples = 32u;
	} pushBlock;


	//Pipeline setup
	VkShaderModule preFilterVertexShader;
	if (!vkutil::load_shader_module("shaders/filter_cube.vert.spv", engine->_device, &preFilterVertexShader)) {
		fmt::print("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule preFilterFragmentShader;
	if (!vkutil::load_shader_module("shaders/irradiance_cube.frag.spv", engine->_device, &preFilterFragmentShader)) {
		fmt::print("Error when building the shadow fragment shader module\n");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(PushBlock);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayout layouts[] = { preFilterSetLayout };

	VkPipelineLayoutCreateInfo prefilter_layout_info = vkinit::pipeline_layout_create_info();
	prefilter_layout_info.setLayoutCount = 1;
	prefilter_layout_info.pSetLayouts = layouts;
	prefilter_layout_info.pPushConstantRanges = &matrixRange;
	prefilter_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout preFilterLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &prefilter_layout_info, nullptr, &preFilterLayout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(preFilterVertexShader, preFilterFragmentShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(false, false, VK_COMPARE_OP_LESS_OR_EQUAL);
	pipelineBuilder._pipelineLayout = preFilterLayout;

	auto preFilterPipeline = pipelineBuilder.build_pipeline(engine->_device);
}