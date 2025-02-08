#include "base_renderer.h"
#include "../asset_manager.h"

class PBRClusteredRenderer : public BaseRenderer{
public:
	void init(VulkanEngine* engine) override;

	//shuts down the engine
	void cleanup() override;

	//draw loop
	void draw() override;
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

	//run main loop
	void run() override;

	void update_scene() override;

	void load_assets();
	void pre_process_pass();
	void cull_lights(VkCommandBuffer cmd);
	void draw_shadows(VkCommandBuffer cmd);
	void draw_main(VkCommandBuffer cmd);
	void draw_post_process(VkCommandBuffer cmd);
	void draw_background(VkCommandBuffer cmd);
	void draw_geometry(VkCommandBuffer cmd);
	void draw_hdr(VkCommandBuffer cmd);
	void draw_early_depth(VkCommandBuffer cmd);
	void init_default_data();


	Camera mainCamera;
	VulkanEngine* engine;

	MaterialInstance defaultData;
	GLTFMetallic_Roughness metalRoughMaterial;
	ShadowPipelineResources cascadedShadows;
	SkyBoxPipelineResources skyBoxPSO;
	BloomBlurPipelineObject postProcessPSO;
	RenderImagePipelineObject HdrPSO;
	EarlyDepthPipelineObject depthPrePassPSO;

	bool resize_requested = false;
	bool _isInitialized{ false };
	int _frameNumber{ 0 };
	bool render_shadowMap{ true };
	bool stop_rendering{ false };
	bool debugShadowMap = true;

	struct {
		float lastFrame;
	} delta;


	FrameData _frames[FRAME_OVERLAP];
	std::vector<glm::mat4> lightMatrices;
	std::vector<float>cascades;

	FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	AllocatedImage _resolveImage;
	AllocatedImage _hdrImage;
	AllocatedImage _shadowDepthImage;

	struct {
		AllocatedImage _lutBRDF;
		AllocatedImage _irradianceCube;
		AllocatedImage _preFilteredCube;
		VkSampler _irradianceCubeSampler;
		VkSampler _lutBRDFSampler;
	}IBL;

	DirectionalLight directLight;
	uint32_t maxLights = 100;

	AssetManager asset_manager;
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


	struct PointLightData {

		uint32_t numOfLights = 6;
		//PointLight pointLights[100];
		std::vector<PointLight> pointLights;

	}pointData;


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
};