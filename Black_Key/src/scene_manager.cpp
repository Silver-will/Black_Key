#include "scene_manager.h"


struct MeshObject {
	Mesh* mesh{ nullptr };

	vkutil::Material* material;
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

Handle<RenderObject> register_object(MeshObject* object)
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
		if (object->material->original->passShaders[MeshpassType::Transparency])
		{
			_transparentForwardPass.unbatchedObjects.push_back(handle);
		}
		if (object->material->original->passShaders[MeshpassType::Forward])
		{
			_forwardPass.unbatchedObjects.push_back(handle);
		}
	}
	if (object->bDrawShadowPass)
	{
		if (object->material->original->passShaders[MeshpassType::DirectionalShadow])
		{
			_shadowPass.unbatchedObjects.push_back(handle);
		}
	}

	update_object(handle);
	return handle;
}