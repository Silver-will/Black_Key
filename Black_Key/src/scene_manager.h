#ifndef VK_SCENE_H
#define VK_SCENE_H
#include "vk_types.h"
#include "vk_util.h"
#include "vk_loader.h"

class VulkanEngine;
class ResourceManager;
/*
template <typename T>
struct Handle {
	uint32_t handle;
};

namespace vk_util { struct Material; }
namespace vk_util { struct ShaderPass; }


struct GPUIndirectObject {
	VkDrawIndexedIndirectCommand command;
	uint32_t objectID;
	uint32_t batchID;
};

struct DrawMesh {
	uint32_t firstVertex;
	uint32_t firstIndex;
	uint32_t indexCount;
	uint32_t vertexCount;
	bool isMerged;

	GPUMeshBuffers* original;
};


struct SceneMeshObject {

	Handle<DrawMesh> meshID;
	IndirectMesh* mesh{nullptr};
	vk_util::Material* material;

	uint32_t updateIndex;
	uint32_t customSortKey{ 0 };

	vk_util::PerPassData<int32_t> passIndices;

	glm::mat4 transformMatrix;
	uint32_t bDrawForwardPass : 1;
	uint32_t bDrawShadowPass : 1;

	Bounds bounds;
};

struct SceneRenderObject {
	Handle<DrawMesh> meshID;
	Handle<vk_util::Material> material;

	uint32_t updateIndex;
	uint32_t customSortKey{ 0 };

	
	vk_util::PerPassData<int32_t> passIndices;

	glm::mat4 transformMatrix;

	Bounds bounds;
};



struct GPUInstance {
	uint32_t objectID;
	uint32_t batchID;
};

struct RenderScene {

	struct PassMaterial {
		VkDescriptorSet materialSet;
		MaterialInstance* shaderPass;

		bool operator==(const PassMaterial& other) const
		{
			return materialSet == other.materialSet && shaderPass == other.shaderPass;
		}
	};

	struct PassObject {
		PassMaterial material;
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
	struct IndirectBatch {
		Handle<DrawMesh> meshID;
		PassMaterial material;
		uint32_t first;
		uint32_t count;
	};

	struct Multibatch {
		uint32_t first;
		uint32_t count;
	};


	struct MeshPass {
		std::vector<RenderScene::Multibatch> multibatches;

		std::vector<RenderScene::IndirectBatch> batches;

		std::vector<Handle<RenderObject>> unbatchedObjects;

		std::vector<RenderScene::RenderBatch> flat_batches;

		std::vector<PassObject> objects;

		std::vector<Handle<PassObject>> reusableObjects;

		std::vector<Handle<PassObject>> objectsToDelete;


		AllocatedBuffer compactedInstanceBuffer;
		AllocatedBuffer passObjectsBuffer;

		AllocatedBuffer drawIndirectBuffer;
		AllocatedBuffer clearIndirectBuffer;

		PassObject* get(Handle<PassObject> handle);

		vk_util::MeshPassType type;

		bool needsIndirectRefresh = true;
		bool needsInstanceRefresh = true;
	};

	void init();

	Handle<SceneMeshObject> register_object(SceneMeshObject* object);

	void register_object_batch(SceneMeshObject* first, uint32_t count);

	void update_transform(Handle<SceneMeshObject> objectID, const glm::mat4& localToWorld);
	void update_object(Handle<SceneMeshObject> objectID);

	void fill_objectData(GPUObjectData* data);
	void fill_indirectArray(GPUIndirectObject* data, MeshPass& pass);
	void fill_instancesArray(GPUInstance* data, MeshPass& pass);

	void write_object(GPUObjectData* target, Handle<SceneMeshObject> objectID);

	void clear_dirty_objects();

	void build_batches();

	void merge_meshes(class VulkanEngine* engine);

	void refresh_pass(MeshPass* pass);

	void build_indirect_batches(MeshPass* pass, std::vector<IndirectBatch>& outbatches, std::vector<RenderScene::RenderBatch>& inobjects);
	RenderObject* get_object(Handle<RenderObject> objectID);
	DrawMesh* get_mesh(Handle<DrawMesh> objectID);


	vk_util::Material get_material(vk_util::Material objectID);

	std::vector<SceneRenderObject> renderables;
	std::vector<DrawMesh> meshes;
	std::vector<vk_util::Material*> materials;

	std::vector<Handle<RenderObject>> dirtyObjects;

	MeshPass* get_mesh_pass(vk_util::MeshPassType name);

	MeshPass _forwardPass;
	MeshPass _transparentForwardPass;
	MeshPass _shadowPass;

	std::unordered_map<vk_util::Material*, Handle<vk_util::Material>> materialConvert;
	std::unordered_map<GPUMeshBuffers*, Handle<DrawMesh>> meshConvert;

	Handle<vk_util::Material> GetMaterialHandle(vk_util::Material* m);
	Handle<DrawMesh> GetMeshHandle(IndirectMesh* m);


	AllocatedBuffer mergedVertexBuffer;
	AllocatedBuffer mergedIndexBuffer;

	AllocatedBuffer objectDataBuffer;
};
*/


struct SceneManager {
	SceneManager();
	~SceneManager();
	void Init(ResourceManager* rm,VulkanEngine* engine_ptr);
	void MergeMeshes();
	void PrepareIndirectBuffers();

	struct GPUModelInformation {
		glm::vec3 origin;
		float sphereRadius;
		uint32_t  texture_index = 0;
		uint32_t  firstIndex = 0;
		uint32_t  indexCount = 0;
		uint32_t  _pad = 0;
		glm::mat4 local_transform;
	};
private:
	int mesh_count = 0;
	bool is_initialized = false;
	DeletionQueue queue;
	AllocatedBuffer merged_vertex_buffer;
	AllocatedBuffer merged_index_buffer;
	AllocatedBuffer model_buffer;
	AllocatedBuffer staging_address_buffer;
	AllocatedBuffer address_buffer;
	AllocatedBuffer indirect_command_buffer;
	VulkanEngine* engine;
	ResourceManager* resource_manager;
};
#endif
