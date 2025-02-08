#pragma once
#include "vk_types.h"

template <typename T>
struct Handle {
	uint32_t handle;
};

struct MeshObject;
struct Mesh;
struct GPUObjectData;

namespace vkutil { struct Material; }
namespace vkutil { struct ShaderPass; }

struct GPUIndirectObject {
	VkDrawIndexedIndirectCommand command;
	uint32_t object_id;
	uint32_t batch_id;
};


struct DrawMesh
{
	uint32_t first_vertex;
	uint32_t first_index;
	uint32_t index_count;
	uint32_t vertex_count;
	bool is_merged;

	Mesh* original;
};

enum MeshPassType {
	Forward,
	Transparency,
	DirectionalShadow,
	EarlyDepth
};

template<typename T>
struct PerPassData {

public:
	T& operator[](MeshpassType pass)
	{
		switch (pass)
		{
		case MeshpassType::Forward:
			return data[0];
		case MeshpassType::Transparency:
			return data[1];
		case MeshpassType::DirectionalShadow:
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

struct RenderObject {

	Handle<DrawMesh> meshID;
	Handle<vkutil::Material> material;

	uint32_t updateIndex;
	uint32_t customSortKey{ 0 };

	PerPassData<int32_t> passIndices;

	glm::mat4 transformMatrix;

	Bounds bounds;
};

struct GPUInstance {
	uint32_t objectID;
	uint32_t batchID;
};


class SceneManager
{
	struct PassMaterial {
		VkDescriptorSet materialSet;
		vkutil::ShaderPass* shaderPass;

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

		std::vector<SceneManager::Multibatch> multibatches;

		std::vector<SceneManager::IndirectBatch> batches;

		std::vector<Handle<RenderObject>> unbatchedObjects;

		std::vector<SceneManager::RenderBatch> flat_batches;

		std::vector<PassObject> objects;

		std::vector<Handle<PassObject>> reusableObjects;

		std::vector<Handle<PassObject>> objectsToDelete;


		AllocatedBuffer compactedInstanceBuffer;
		AllocatedBuffer passObjectsBuffer;

		AllocatedBuffer drawIndirectBuffer;
		AllocatedBuffer clearIndirectBuffer;

		PassObject* get(Handle<PassObject> handle);

		MeshPassType type;

		bool needsIndirectRefresh = true;
		bool needsInstanceRefresh = true;
	};

	void init();

	Handle<RenderObject> register_object(MeshObject* object);

	void register_object_batch(MeshObject* first, uint32_t count);

	void update_transform(Handle<RenderObject> objectID, const glm::mat4& localToWorld);
	void update_object(Handle<RenderObject> objectID);

	void fill_objectData(GPUObjectData* data);
	void fill_indirectArray(GPUIndirectObject* data, MeshPass& pass);
	void fill_instancesArray(GPUInstance* data, MeshPass& pass);

	void write_object(GPUObjectData* target, Handle<RenderObject> objectID);

	void clear_dirty_objects();

	void build_batches();

	void merge_meshes(class VulkanEngine* engine);

	void refresh_pass(MeshPass* pass);

	void build_indirect_batches(MeshPass* pass, std::vector<IndirectBatch>& outbatches, std::vector<SceneManager::RenderBatch>& inobjects);
	RenderObject* get_object(Handle<RenderObject> objectID);
	DrawMesh* get_mesh(Handle<DrawMesh> objectID);

	vkutil::Material* get_material(Handle<vkutil::Material> objectID);

	std::vector<RenderObject> renderables;
	std::vector<DrawMesh> meshes;
	std::vector<vkutil::Material*> materials;

	std::vector<Handle<RenderObject>> dirtyObjects;

	MeshPass* get_mesh_pass(MeshPassType name);

	MeshPass _forwardPass;
	MeshPass _transparentForwardPass;
	MeshPass _shadowPass;
	MeshPass _earlyDepthPass;

	std::unordered_map<vkutil::Material*, Handle<vkutil::Material>> materialConvert;
	std::unordered_map<Mesh*, Handle<DrawMesh>> meshConvert;

	Handle<vkutil::Material> getMaterialHandle(vkutil::Material* m);
	Handle<DrawMesh> getMeshHandle(Mesh* m);


	AllocatedBuffer mergedVertexBuffer;
	AllocatedBuffer mergedIndexBuffer;

	AllocatedBuffer objectDataBuffer;
};

