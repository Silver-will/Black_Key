#ifndef VK_SCENE_H
#define VK_SCENE_H
#include "vk_types.h"
#include "vk_util.h"
#include "vk_loader.h"

template <typename T>
struct Handle {
	uint32_t handle;
};

namespace vkutil { struct Material; }
namespace vkutil { struct ShaderPass; }


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


struct SceneRenderObject {

	Handle<DrawMesh> meshID;
	Handle<MaterialInstance> material;

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

	Handle<SceneRenderObject> register_object(MeshObject* object);

	void register_object_batch(MeshObject* first, uint32_t count);

	void update_transform(Handle<SceneRenderObject> objectID, const glm::mat4& localToWorld);
	void update_object(Handle<SceneRenderObject> objectID);

	void fill_objectData(GPUObjectData* data);
	void fill_indirectArray(GPUIndirectObject* data, MeshPass& pass);
	void fill_instancesArray(GPUInstance* data, MeshPass& pass);

	void write_object(GPUObjectData* target, Handle<SceneRenderObject> objectID);

	void clear_dirty_objects();

	void build_batches();

	void merge_meshes(class VulkanEngine* engine);

	void refresh_pass(MeshPass* pass);

	void build_indirect_batches(MeshPass* pass, std::vector<IndirectBatch>& outbatches, std::vector<RenderScene::RenderBatch>& inobjects);
	RenderObject* get_object(Handle<RenderObject> objectID);
	DrawMesh* get_mesh(Handle<DrawMesh> objectID);


	vkutil::Material* get_material(Handle<vkutil::Material> objectID);

	std::vector<RenderObject> renderables;
	std::vector<DrawMesh> meshes;
	std::vector<vkutil::Material*> materials;

	std::vector<Handle<RenderObject>> dirtyObjects;

	MeshPass* get_mesh_pass(vk_util::MeshPassType name);

	MeshPass _forwardPass;
	MeshPass _transparentForwardPass;
	MeshPass _shadowPass;

	std::unordered_map<vkutil::Material*, Handle<vkutil::Material>> materialConvert;
	std::unordered_map<GPUMeshBuffers*, Handle<DrawMesh>> meshConvert;

	Handle<vkutil::Material> getMaterialHandle(vkutil::Material* m);
	Handle<DrawMesh> getMeshHandle(GPUMeshBuffers* m);


	AllocatedBuffer mergedVertexBuffer;
	AllocatedBuffer mergedIndexBuffer;

	AllocatedBuffer objectDataBuffer;
};
#endif