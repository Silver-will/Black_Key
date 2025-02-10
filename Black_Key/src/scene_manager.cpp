#include "scene_manager.h"


struct MeshObject {
	Mesh* mesh{ nullptr };

	vkutil::MaterialInstance* material;
	uint32_t customSortKey;
	glm::mat4 transformMatrix;
	Bounds bounds;

	uint32_t bDrawForwardPass : 1;
	uint32_t bDrawShadowPass : 1;
};

void SceneManager::init()
{
	_forwardPass.type = MeshPassType::Forward;
	_shadowPass.type = MeshPassType::DirectionalShadow;
	_transparentForwardPass.type = MeshPassType::Transparency;
}

Handle<RenderObject> SceneManager::register_object(MeshObject* object)
{
	RenderObject newObj;
	newObj.bounds = object->bounds;
	newObj.transformMatrix = object->transformMatrix;
	newObj.material = getMaterialHandle(object->material);
	newObj.meshID = getMeshHandle(object->mesh);
	newObj.updateIndex = (uint32_t)-1;
	newObj.customSortKey = object->customSortKey;
	newObj.passIndices.clear(-1);
	Handle<RenderObject> handle;
	handle.handle = static_cast<uint32_t>(renderables.size());

	renderables.push_back(newObj);

	if (object->bDrawForwardPass)
	{
		if (object->material->original->passShaders[MeshPassType::Transparency])
		{
			_transparentForwardPass.unbatchedObjects.push_back(handle);
		}
		if (object->material->original->passShaders[MeshPassType::Forward])
		{
			_forwardPass.unbatchedObjects.push_back(handle);
		}
	}
	if (object->bDrawShadowPass)
	{
		if (object->material->original->passShaders[MeshPassType::DirectionalShadow])
		{
			_shadowPass.unbatchedObjects.push_back(handle);
		}
	}

	update_object(handle);
	return handle;
}


void SceneManager::register_object_batch(MeshObject* first, uint32_t count)
{
	renderables.reserve(count);

	for (uint32_t i = 0; i < count; i++) {
		register_object(&(first[i]));
	}
}

void SceneManager::update_transform(Handle<RenderObject> objectID, const glm::mat4& localToWorld)
{
	get_object(objectID)->transformMatrix = localToWorld;
	update_object(objectID);
}

void SceneManager::update_object(Handle<RenderObject> objectID)
{
	auto& passIndices = get_object(objectID)->passIndices;
	if (passIndices[MeshPassType::Forward] != -1)
	{
		Handle<PassObject> obj;
		obj.handle = passIndices[MeshPassType::Forward];

		_forwardPass.objectsToDelete.push_back(obj);
		_forwardPass.unbatchedObjects.push_back(objectID);

		passIndices[MeshPassType::Forward] = -1;
	}


	if (passIndices[MeshPassType::DirectionalShadow] != -1)
	{
		Handle<PassObject> obj;
		obj.handle = passIndices[MeshPassType::DirectionalShadow];

		_shadowPass.objectsToDelete.push_back(obj);
		_shadowPass.unbatchedObjects.push_back(objectID);

		passIndices[MeshPassType::DirectionalShadow] = -1;
	}


	if (passIndices[MeshPassType::Transparency] != -1)
	{
		Handle<PassObject> obj;
		obj.handle = passIndices[MeshPassType::Transparency];

		_transparentForwardPass.unbatchedObjects.push_back(objectID);
		_transparentForwardPass.objectsToDelete.push_back(obj);

		passIndices[MeshPassType::Transparency] = -1;
	}

	if (passIndices[MeshPassType::EarlyDepth] != -1)
	{
		Handle<PassObject> obj;
		obj.handle = passIndices[MeshPassType::EarlyDepth];

		_transparentForwardPass.unbatchedObjects.push_back(objectID);
		_transparentForwardPass.objectsToDelete.push_back(obj);

		passIndices[MeshPassType::EarlyDepth] = -1;
	}

	if (get_object(objectID)->updateIndex == (uint32_t)-1)
	{

		get_object(objectID)->updateIndex = static_cast<uint32_t>(dirtyObjects.size());

		dirtyObjects.push_back(objectID);
	}
}

void SceneManager::fill_objectData(GPUObjectData* data)
{
	
}
void SceneManager::fill_indirectArray(GPUIndirectObject* data, MeshPass& pass)
{

}
void SceneManager::fill_instancesArray(GPUInstance* data, MeshPass& pass)
{

}

void SceneManager::write_object(GPUObjectData* target, Handle<RenderObject> objectID)
{
	RenderObject* renderable = get_object(objectID);
	GPUObjectData object;

	object.modelMatrix = renderable->transformMatrix;
	object.origin_rad = glm::vec4(renderable->bounds.origin, renderable->bounds.radius);
	object.extents = glm::vec4(renderable->bounds.extents, renderable->bounds.valid ? 1.f : 0.f);

	memcpy(target, &object, sizeof(GPUObjectData));
}

void SceneManager::clear_dirty_objects()
{

}

void SceneManager::build_batches()
{

}