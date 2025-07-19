#ifndef VK_SCENE_H
#define VK_SCENE_H
#include "vk_types.h"
#include "vk_util.h"
#include "vk_loader.h"
#include "engine_util.h"
#include <memory>
#include <string_view>

class VulkanEngine;
class ResourceManager;

struct DrawMesh {
	uint32_t firstVertex;
	uint32_t firstIndex;
	uint32_t indexCount;
	uint32_t vertexCount;
	bool isMerged;

	GPUMeshBuffers* original;
};


struct SceneManager {

	struct IndirectRenderObject {
		uint32_t indexCount;
		uint32_t firstIndex;
		uint32_t vertexCount;
		uint32_t firstVertex;
		VkBuffer indexBuffer;
		VkBuffer vertexBuffer;
		Handle<RenderObject> handle;

		MaterialInstance* material;

		glm::mat4 transform;
		Bounds bounds;
		VkDeviceAddress vertexBufferAddress;
	};

	struct PassObject {
		Handle<DrawMesh> meshID;
		Handle<RenderObject> original;
		int32_t builtbatch;
		uint32_t customKey;
	};
	
	struct RenderBatch {
		Handle<PassObject> object;
		uint64_t sortKey;

		bool operator==(const RenderBatch& other) const
		{
			return object.handle == other.object.handle && sortKey == other.sortKey;
		}
	};
	struct GPUIndirectObject {
		VkDrawIndexedIndirectCommand command;
		uint32_t objectID;
		uint32_t batchID;
	};

	struct IndirectBatch {
		MaterialInstance material;
		uint32_t first;
		uint32_t count;
	};

	struct Multibatch {
		uint32_t first;
		uint32_t count;
	};


	struct MeshPass {
		std::vector<SceneManager::Multibatch> multibatches;

		std::vector<SceneManager::IndirectBatch> batches;

		std::vector<Handle<RenderObject>> unbatchedObjects;

		std::vector<SceneManager::RenderBatch> flat_batches;
		std::vector<RenderObject>flat_objects;

		std::vector<PassObject> objects;

		std::vector<Handle<PassObject>> reusableObjects;

		std::vector<Handle<PassObject>> objectsToDelete;


		AllocatedBuffer compactedInstanceBuffer;
		AllocatedBuffer passObjectsBuffer;

		AllocatedBuffer drawIndirectBuffer;
		AllocatedBuffer clearIndirectBuffer;

		PassObject* get(Handle<PassObject> handle);

		vkutil::MeshPassType type;

		bool needsIndirectRefresh = true;
		bool needsInstanceRefresh = true;
		bool needs_materials = true;
	};

	SceneManager() {}
	~SceneManager() {}
	void Init(std::shared_ptr<ResourceManager> rm,VulkanEngine* engine_ptr);
	void MergeMeshes();
	void BuildBatches();
	void RefreshPass(MeshPass* pass);
	void PrepareIndirectBuffers();
	void RegisterObjectBatch(DrawContext ctx);
	void RegisterMeshAssetReference(std::string_view mesh_reference);
	void UpdateObjectDataBuffers();
	size_t GetModelCount();
	MeshPass* GetMeshPass(vkutil::MaterialPass passType);
	void ClearIndirectBuffers(MeshPass* pass);
	AllocatedBuffer* GetObjectDataBuffer();
	AllocatedBuffer* GetIndirectCommandBuffer();
	AllocatedBuffer* GetMergedVertexBuffer();
	AllocatedBuffer* GetMergedIndexBuffer();
	VkDeviceAddress* GetMergedDeviceAddress();

private:
	MeshPass early_depth_pass;
	MeshPass shadow_pass;
	MeshPass forward_pass;
	MeshPass transparency_pass;
	MeshPass voxelization_pass;
	MeshPass cone_tracing_pass;

	int mesh_count = 0;
	bool is_initialized = false;
	AllocatedBuffer merged_vertex_buffer;
	AllocatedBuffer merged_index_buffer;
	AllocatedBuffer object_data_buffer;
	AllocatedBuffer staging_address_buffer;
	AllocatedBuffer address_buffer;
	AllocatedBuffer clear_indirect_command_buffer;
	AllocatedBuffer indirect_command_buffer;
	std::vector<std::string> mesh_assets;

	VulkanEngine* engine;
	std::shared_ptr<ResourceManager> resource_manager;
	VkDeviceAddress mergedVertexAddress;
	
	std::vector<RenderObject> renderables;
	std::vector<GPUIndirectObject> object_commands;
};
#endif
