#include "vk_scene.h"
#include "vk_engine.h"

void RenderScene::init()
{
	_forwardPass.type = vk_util::MeshPassType::Forward;
	_shadowPass.type = vk_util::MeshPassType::Shadow;
	_transparentForwardPass.type = vk_util::MeshPassType::Transparent;
}

Handle<SceneMeshObject> RenderScene::register_object(SceneMeshObject* object) {
	SceneRenderObject new_obj;
	new_obj.bounds = object->bounds;
	new_obj.transformMatrix = object->transformMatrix;
	new_obj.material = GetMaterialHandle(object->material);
	new_obj.meshID = GetMeshHandle(object->mesh);
	new_obj.updateIndex = (uint32_t)-1;
	new_obj.customSortKey = object->customSortKey;
	new_obj.passIndices.clear(-1);
	Handle<SceneRenderObject> handle;
	handle.handle = static_cast<uint32_t>(renderables.size());

	renderables.push_back(new_obj);

	if (object->bDrawForwardPass)
	{
		//if(object->material->)
	}
}