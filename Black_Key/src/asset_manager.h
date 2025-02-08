#pragma once
#include "vk_engine.h"

struct AssetManager {
	AssetManager(VulkanEngine* engine);
	VulkanEngine* engine;
	void LoadGLTF(std::string_view filePath);
	VkFilter extract_filter(fastgltf::Filter filter);
	VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter);
	std::optional<AllocatedImage> load_image(fastgltf::Asset& asset, fastgltf::Image& image, const std::string& rootPath);
	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	std::vector<uint32_t> draws;
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;
};