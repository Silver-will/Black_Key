#include "scene_manager.h"
#include "vk_engine.h"
#include "resource_manager.h"
#include <algorithm>
#include <future>
/*

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

*/

void SceneManager::Init(ResourceManager* rm, VulkanEngine* engine_ptr)
{
	resource_manager = rm;
	engine = engine_ptr;

	forward_pass.type = vk_util::MeshPassType::Forward;
	shadow_pass.type = vk_util::MeshPassType::Shadow;
	early_depth_pass.type = vk_util::MeshPassType::EarlyDepth;
	transparency_pass.type = vk_util::MeshPassType::Transparent;

	//mesh render passes with no texture reads
	early_depth_pass.needs_materials = false;
	shadow_pass.needs_materials = false;
}

void SceneManager::MergeMeshes()
{
	size_t total_vertices = 0;
	size_t total_indices = 0;

	//merge opaque and transparent render objects into one 
	std::vector<RenderObject> scene_meshes(engine->drawCommands.OpaqueSurfaces.size() + engine->drawCommands.TransparentSurfaces.size());
	std::merge(engine->drawCommands.OpaqueSurfaces.begin(), engine->drawCommands.OpaqueSurfaces.end(),
		engine->drawCommands.TransparentSurfaces.begin(), engine->drawCommands.TransparentSurfaces.end(), scene_meshes.begin());

	mesh_count = scene_meshes.size();
	for (auto& m : scene_meshes)
	{
		m.firstIndex = static_cast<uint32_t>(total_indices);
		m.firstVertex = static_cast<uint32_t>(total_vertices);

		total_vertices += m.vertexCount;
		total_indices += m.indexCount;
	}
	assert(total_vertices && total_indices);

	merged_vertex_buffer = engine->create_buffer(total_vertices * sizeof(Vertex), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);


	merged_index_buffer = engine->create_buffer(total_indices * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);


	engine->immediate_submit([&](VkCommandBuffer cmd)
		{
			std::vector<GPUModelInformation> scene_indirect_data;
			for (auto& m : scene_meshes)
			{
				VkBufferCopy vertex_copy;
				vertex_copy.dstOffset = sizeof(Vertex) * m.firstVertex;
				vertex_copy.size = sizeof(Vertex) * m.vertexCount;
				vertex_copy.srcOffset = 0;
				vkCmdCopyBuffer(cmd, m.vertexBuffer, merged_vertex_buffer.buffer,1,&vertex_copy);

				VkBufferCopy index_copy;
				index_copy.dstOffset = sizeof(uint32_t) * m.firstIndex;
				index_copy.size = sizeof(uint32_t) * m.indexCount;
				index_copy.srcOffset = 0;
				vkCmdCopyBuffer(cmd, m.indexBuffer, merged_index_buffer.buffer, 1, &index_copy);

				/*GPUModelInformation model_info;
				model_info.origin = m.bounds.origin;
				model_info.sphereRadius = m.bounds.sphereRadius;
				model_info.texture_index = m.material->material_index;
				model_info.local_transform = m.transform;
				model_info.indexCount = m.indexCount;
				scene_indirect_data.emplace_back(model_info);
				*/

				scene_indirect_data.emplace_back(GPUModelInformation
					{.origin = m.bounds.origin,
					.sphereRadius = m.bounds.sphereRadius,
					.texture_index = m.material->material_index,
					.firstIndex = m.firstIndex / ((uint32_t)sizeof(uint32_t)),
					.indexCount = m.indexCount,
					._pad = 0,
					.local_transform = m.transform,
					});
			}
			model_buffer = engine->create_and_upload(scene_indirect_data.size() * sizeof(GPUModelInformation),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, scene_indirect_data.data());
		}
	);

}



void SceneManager::PrepareIndirectBuffers()
{
	
	auto indirect_buffer_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	indirect_command_buffer = engine->create_buffer(sizeof(VkDrawIndexedIndirectCommand) * mesh_count, indirect_buffer_flags, VMA_MEMORY_USAGE_GPU_ONLY);

	VkBufferDeviceAddressInfoKHR address_info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR };
	address_info.buffer = indirect_command_buffer.buffer;
	VkDeviceAddress srcPtr = vkGetBufferDeviceAddressKHR(engine->_device, &address_info);
	
	const size_t address_buffer_size = sizeof(VkDeviceAddress);
	//auto staging_address_buffer = engine->create_buffer(address_buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	address_buffer = engine->create_and_upload(address_buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY, &srcPtr);
	
	/*void* dst = nullptr;
	vmaMapMemory(engine->_allocator, staging_address_buffer.allocation, &dst);
	auto destPtr = (uint64_t*)dst;
	*destPtr = srcPtr;
	vmaUnmapMemory(engine->_allocator, staging_address_buffer.allocation);
	*/
}

void SceneManager::RefreshPass(MeshPass* pass)
{
	pass->needsIndirectRefresh = true;
	pass->needsInstanceRefresh = true;

	std::vector<uint32_t> new_objects;
	if (pass->objectsToDelete.size() > 0)
	{
		std::vector<RenderBatch> deletion_batches;
		deletion_batches.reserve(new_objects.size());

		for (auto i : pass->objectsToDelete)
		{
			pass->reusableObjects.push_back(i);
			RenderBatch new_command;

			auto obj = ;
		}
	}
}

void SceneManager::BuildBatches()
{
	auto fwd = std::async(std::launch::async, [&] {RefreshPass(&forward_pass); });
	auto shadow = std::async(std::launch::async, [&] {RefreshPass(&shadow_pass); });
	auto trans = std::async(std::launch::async, [&] {RefreshPass(&transparency_pass); });
	auto early_z = std::async(std::launch::async, [&] {RefreshPass(&early_depth_pass); });


	fwd.get();
	shadow.get();
	trans.get();
	early_z.get();
}

void SceneManager::RegisterObjectBatch(DrawContext ctx)
{
	
}