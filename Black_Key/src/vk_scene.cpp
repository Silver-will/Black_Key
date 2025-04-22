#include "vk_scene.h"
#include "vk_engine.h"

void RenderScene::init()
{
	_forwardPass.type = vk_util::MeshPassType::Forward;
	_shadowPass.type = vk_util::MeshPassType::Shadow;
	_transparentForwardPass.type = vk_util::MeshPassType::Transparent;
}

Handle<SceneRenderObject> RenderScene::register_object(MeshObject* object) {

}