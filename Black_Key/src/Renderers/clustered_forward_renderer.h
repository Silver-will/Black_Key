#pragma once
#include "base_renderer.h"
struct ClusteredForwardRenderer : BaseRenderer
{
	void Init(VulkanEngine* engine) override;

	void Cleanup() override;

	void Draw() override;
	void DrawUI() override;

	void Run() override;

	void InitImgui() override;

	void LoadAssets() override;
	void UpdateScene() override;

	void PreProcessPass();
	void CullLights(VkCommandBuffer cmd);
	void DrawShadows(VkCommandBuffer cmd);
	void DrawMain(VkCommandBuffer cmd);
	void DrawPostProcess(VkCommandBuffer cmd);
	void DrawBackground(VkCommandBuffer cmd);
	void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void DrawGeometry(VkCommandBuffer cmd);
	void DrawHdr(VkCommandBuffer cmd);
	void DrawEarlyDepth(VkCommandBuffer cmd);
	void InitCommands();
	void InitRenderTargets();
	void InitSwapchain();
	void InitComputePipelines();

	
	void InitDefaultData();
	void InitSyncStructures();
	void InitDescriptors();
	void InitBuffers();
	void InitPipelines();
	void CreateSwapchain(uint32_t width, uint32_t height);
	void DestroySwapchain();
	void ResizeSwapchain();
	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void CursorCallback(GLFWwindow* window, double xpos, double ypos);
	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

private:

	Camera mainCamera;

	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;

	MaterialInstance defaultData;
	GLTFMetallic_Roughness metalRoughMaterial;
	ShadowPipelineResources cascadedShadows;
	SkyBoxPipelineResources skyBoxPSO;
	BloomBlurPipelineObject postProcessPSO;
	RenderImagePipelineObject HdrPSO;
	EarlyDepthPipelineObject depthPrePassPSO;

	DrawContext mainDrawContext;
	//r
	std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;

	DescriptorAllocator globalDescriptorAllocator;
	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _shadowSceneDescriptorLayout;

	bool resize_requested = false;
	bool _isInitialized{ false };
	int _frameNumber{ 0 };
	bool render_shadowMap{ true };
	bool stop_rendering{ false };
	bool debugShadowMap = true;

	struct {
		float lastFrame;
	} delta;
	VkExtent2D _windowExtent{ 1920,1080 };
	float _aspect_width = 1920;
	float _aspect_height = 1080;

	static VulkanEngine& Get();

	FrameData _frames[FRAME_OVERLAP];
	std::vector<glm::mat4> lightMatrices;
	std::vector<float>cascades;

	FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };
	
	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	AllocatedImage _resolveImage;
	AllocatedImage _hdrImage;
	AllocatedImage _shadowDepthImage;
	AllocatedImage _presentImage;

	struct {
		AllocatedImage _lutBRDF;
		AllocatedImage _irradianceCube;
		AllocatedImage _preFilteredCube;
		VkSampler _irradianceCubeSampler;
		VkSampler _lutBRDFSampler;

	}IBL;
	VkExtent2D _drawExtent;
	float renderScale = 1.f;

	VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;
	
	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };

	VkPipelineLayout _trianglePipelineLayout;
	VkPipeline _trianglePipeline;
	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;
	VkPipeline _cullLightsPipeline;
	VkPipelineLayout _cullLightsPipelineLayout;

	GPUMeshBuffers rectangle;
	std::vector<std::shared_ptr<MeshAsset>> testMeshes;

	GPUSceneData sceneData;
	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
	VkDescriptorSetLayout _singleImageDescriptorLayout;
	VkDescriptorSetLayout _skyboxDescriptorLayout;
	VkDescriptorSetLayout _drawImageDescriptorLayout;
	VkDescriptorSetLayout _cullLightsDescriptorLayout;
	VkDescriptorSetLayout _buildClustersDescriptorLayout;
	VkDescriptorSetLayout _bindlessDescriptorLayout;
	//VkDescriptorSetLayout _

	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;

	AllocatedImage _skyImage;
	ktxVulkanTexture _skyBoxImage;

	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;
	VkSampler _cubeMapSampler;
	VkSampler _depthSampler;
	DrawContext drawCommands;
	DrawContext skyDrawCommands;
	DrawContext imageDrawCommands;
	ShadowCascades shadows;

	EngineStats stats;
	VkSampleCountFlagBits msaa_samples;

	//Clustered culling  values
	struct {
		//Configuration values
		const uint32_t gridSizeX = 16;
		const uint32_t gridSizeY = 9;
		const uint32_t gridSizeZ = 24;
		const uint32_t numClusters = gridSizeX * gridSizeY * gridSizeZ;
		const uint32_t maxLightsPerTile = 50;
		uint32_t sizeX, sizeY;

		//Storage Buffers
		AllocatedBuffer AABBVolumeGridSSBO;
		AllocatedBuffer screenToViewSSBO;
		AllocatedBuffer lightSSBO;
		AllocatedBuffer lightIndexListSSBO;
		AllocatedBuffer lightGridSSBO;
		AllocatedBuffer lightGlobalIndex[2];
	} ClusterValues;

	std::vector<uint32_t> draws;
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

	//lights
	DirectionalLight directLight;
	uint32_t maxLights = 100;

	struct PointLightData {

		uint32_t numOfLights = 6;
		//PointLight pointLights[100];
		std::vector<PointLight> pointLights;

	}pointData;
};

