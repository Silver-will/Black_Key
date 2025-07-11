#include "scene_manager.h"
#include "vk_engine.h"
#include "resource_manager.h"
#include "engine_util.h"
#include <algorithm>
#include <future>

void SceneManager::Init(std::shared_ptr<ResourceManager> rm, VulkanEngine* engine_ptr)
{
	resource_manager = rm;
	engine = engine_ptr;

	forward_pass.type = vkutil::MeshPassType::Forward;
	shadow_pass.type = vkutil::MeshPassType::Shadow;
	early_depth_pass.type = vkutil::MeshPassType::EarlyDepth;
	transparency_pass.type = vkutil::MeshPassType::Transparent;

	//mesh render passes with no texture reads
	early_depth_pass.needs_materials = false;
	shadow_pass.needs_materials = false;
}

void SceneManager::MergeMeshes()
{
	size_t total_vertices = 0;
	size_t total_indices = 0;

	size_t last_mesh_vert_size = 0;
	size_t last_mesh_indice_size = 0;
	mesh_count = renderables.size();
	GPUMeshBuffers* mesh_buffer = renderables[0].meshBuffer;
	RenderObject* previous_obj = nullptr;
	for (auto& m : renderables)
	{
		m.firstIndex = static_cast<uint32_t>(total_indices);
		m.firstVertex = static_cast<uint32_t>(total_vertices);

		total_vertices += m.vertexCount;
		total_indices += m.indexCount;
		if (mesh_buffer == m.meshBuffer)
		{
			last_mesh_vert_size += m.vertexCount;
			last_mesh_indice_size += m.indexCount;

			m.meshBuffer->mesh_info.mesh_vert_count = last_mesh_vert_size;
			m.meshBuffer->mesh_info.mesh_indice_count = last_mesh_indice_size;
			//m.meshAssetVertexCount += last_mesh_vert_size;
			//m.meshAssetIndexCount += last_mesh_indice_size;
		}
		else
		{
			last_mesh_vert_size = 0;
			last_mesh_indice_size = 0;
		}
		mesh_buffer = m.meshBuffer;
	}
	assert(total_vertices && total_indices);

	merged_vertex_buffer = resource_manager->CreateBuffer(total_vertices * sizeof(Vertex), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT| VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
	merged_index_buffer = resource_manager->CreateBuffer(total_indices * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = merged_vertex_buffer.buffer };
	mergedVertexAddress = vkGetBufferDeviceAddress(engine->_device, &deviceAdressInfo);


	engine->immediate_submit([&](VkCommandBuffer cmd)
		{
			GPUMeshBuffers* last_mesh_buffer = nullptr;
			uint32_t vert_dst_off = 0;
			uint32_t index_dst_off = 0;
			int same_buffer_count = 0;
			
			for (auto& m : renderables)
			{
				
				
				/*VkBufferCopy vertex_copy;
				vertex_copy.dstOffset = sizeof(Vertex) * m.firstVertex;
				vertex_copy.size = sizeof(Vertex) * m.vertexCount;
				vertex_copy.srcOffset = 0;
				vkCmdCopyBuffer(cmd, m.vertexBuffer, merged_vertex_buffer.buffer, 1, &vertex_copy);
				
				
				VkBufferCopy index_copy;
				index_copy.dstOffset = sizeof(uint32_t) * m.firstIndex;
				index_copy.size = sizeof(uint32_t) * m.indexCount;
				index_copy.srcOffset = 0;
				vkCmdCopyBuffer(cmd, m.indexBuffer, merged_index_buffer.buffer, 1, &index_copy);
				*/

				if (last_mesh_buffer != m.meshBuffer)
				{
					//m.meshBuffer->vertexBuffer.info.size;
					/*VkBufferCopy vertex_copy;
					vertex_copy.dstOffset = sizeof(Vertex) * m.firstVertex;
					vertex_copy.size = sizeof(Vertex) * m.vertexCount;
					vertex_copy.srcOffset = 0;
					vkCmdCopyBuffer(cmd, m.vertexBuffer, merged_vertex_buffer.buffer, 1, &vertex_copy);

					VkBufferCopy index_copy;
					index_copy.dstOffset = sizeof(uint32_t) * m.firstIndex;
					index_copy.size = sizeof(uint32_t) * m.indexCount;
					index_copy.srcOffset = 0;
					vkCmdCopyBuffer(cmd, m.indexBuffer, merged_index_buffer.buffer, 1, &index_copy);*/
					//last_vertex_buffer = &m.vertexBuffer;
					

					
					VkBufferCopy vertex_copy;
					vertex_copy.dstOffset = vert_dst_off;
					vertex_copy.size = m.meshBuffer->mesh_info.mesh_vert_count * sizeof(Vertex);
					vertex_copy.srcOffset = 0;
					vkCmdCopyBuffer(cmd, m.vertexBuffer, merged_vertex_buffer.buffer, 1, &vertex_copy);
					
					VkBufferCopy index_copy;
					index_copy.dstOffset = index_dst_off;
					index_copy.size = m.meshBuffer->mesh_info.mesh_indice_count * sizeof(uint32_t);
					index_copy.srcOffset = 0;
					vkCmdCopyBuffer(cmd, m.indexBuffer, merged_index_buffer.buffer, 1, &index_copy);
					

					vert_dst_off += vertex_copy.size;
					index_dst_off += m.meshBuffer->indexBuffer.info.size;
				}
				last_mesh_buffer = m.meshBuffer;
			}
		}
	);

	std::vector<vkutil::GPUModelInformation> scene_indirect_data;
	for (auto& m : renderables)
	{
		scene_indirect_data.emplace_back(vkutil::GPUModelInformation
			{
			.local_transform = m.transform,
			.sphereBounds = BlackKey::Vec3Tovec4(m.bounds.origin, m.bounds.sphereRadius),
			.texture_index = m.material->material_index,
			.firstIndex = m.firstIndex / ((uint32_t)sizeof(uint32_t)),
			.indexCount = m.indexCount,
			.firstVertex = m.firstVertex,
			.vertexCount = m.vertexCount,
			.firstInstance = 0,
			.vertexBuffer = m.vertexBufferAddress,
			.pad = glm::vec4(0)
			});
	}
	object_data_buffer = resource_manager->CreateAndUpload(scene_indirect_data.size() * sizeof(vkutil::GPUModelInformation),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, scene_indirect_data.data());

}

AllocatedBuffer* SceneManager::GetMergedIndexBuffer()
{
	return &merged_index_buffer;
}

AllocatedBuffer* SceneManager::GetMergedVertexBuffer()
{
	return &merged_vertex_buffer;
}

void SceneManager::PrepareIndirectBuffers()
{
	uint32_t index = 0;
	for (const auto& r : renderables)
	{
		GPUIndirectObject indirectCommand;
		indirectCommand.command.indexCount = r.indexCount;
		indirectCommand.batchID = 0;
		indirectCommand.objectID = index;
		indirectCommand.command.instanceCount = 0;
		indirectCommand.command.firstIndex = r.firstIndex;
		indirectCommand.command.vertexOffset = 0;
		indirectCommand.command.firstInstance = index;
		index++;
		object_commands.push_back(indirectCommand);
	}

	auto indirect_buffer_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	indirect_command_buffer = resource_manager->CreateBuffer(sizeof(VkDrawIndexedIndirectCommand) * mesh_count, indirect_buffer_flags, VMA_MEMORY_USAGE_GPU_ONLY);

	clear_indirect_command_buffer = resource_manager->CreateBuffer(sizeof(GPUIndirectObject) * mesh_count, indirect_buffer_flags, VMA_MEMORY_USAGE_GPU_ONLY);
	
	forward_pass.drawIndirectBuffer = resource_manager->CreateAndUpload(sizeof(GPUIndirectObject) * object_commands.size(), indirect_buffer_flags, VMA_MEMORY_USAGE_GPU_ONLY, object_commands.data());
	shadow_pass.drawIndirectBuffer = resource_manager->CreateAndUpload(sizeof(GPUIndirectObject) * object_commands.size(), indirect_buffer_flags, VMA_MEMORY_USAGE_GPU_ONLY, object_commands.data());
	transparency_pass.drawIndirectBuffer = resource_manager->CreateAndUpload(sizeof(GPUIndirectObject) * object_commands.size(), indirect_buffer_flags, VMA_MEMORY_USAGE_GPU_ONLY, object_commands.data());
	early_depth_pass.drawIndirectBuffer = resource_manager->CreateAndUpload(sizeof(GPUIndirectObject) * object_commands.size(), indirect_buffer_flags, VMA_MEMORY_USAGE_GPU_ONLY, object_commands.data());

	VkBufferDeviceAddressInfoKHR address_info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR };
	address_info.buffer = indirect_command_buffer.buffer;
	VkDeviceAddress srcPtr = vkGetBufferDeviceAddress(engine->_device, &address_info);
	
	const size_t address_buffer_size = sizeof(VkDeviceAddress);

	address_buffer = resource_manager->CreateAndUpload(address_buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY, &srcPtr);
	
}

void SceneManager::ClearIndirectBuffers(MeshPass* pass)
{
	engine->immediate_submit([&](VkCommandBuffer cmd)
		{
				VkBufferCopy clear_copy;
				clear_copy.dstOffset = 0;
				clear_copy.srcOffset = 0;
				clear_copy.size = sizeof(GPUIndirectObject) * mesh_count;
				vkCmdCopyBuffer(cmd, clear_indirect_command_buffer.buffer, pass->drawIndirectBuffer.buffer, 1, &clear_copy);
		}
	);
}

void SceneManager::RefreshPass(MeshPass* pass)
{
	//ClearIndirectBuffers(pass);
	if (pass->needs_materials == false)
	{
		pass->multibatches.push_back(Multibatch{
			.first = 0,
			.count = (uint32_t)renderables.size()
			});

		pass->batches.push_back(IndirectBatch{
			.first = 0,
			.count = (uint32_t)renderables.size()
			});
		return;
	}

	MaterialInstance* last_mat = nullptr;
	size_t first = pass->batches.size();
	//size_t count = 0;

	IndirectBatch indirect_draw_call;
	indirect_draw_call.count = 0;
	indirect_draw_call.first = 0;
	size_t last_index = 0;
	for (auto& render_object : renderables)
	{
		if (last_mat != render_object.material)
		{
			indirect_draw_call.material = *render_object.material;
			indirect_draw_call.count = 1;
			indirect_draw_call.first = pass->batches.size();
			last_index = pass->batches.size();

			pass->batches.push_back(indirect_draw_call);
			last_mat = render_object.material;
			
		}
		else
			pass->batches[last_index].count++;
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

SceneManager::MeshPass* SceneManager::GetMeshPass(vkutil::MaterialPass passType)
{
	switch (passType)
	{
		case vkutil::MaterialPass::forward:
			return &forward_pass;
			break;
		case vkutil::MaterialPass::early_depth:
			return &early_depth_pass;
		case vkutil::MaterialPass::shadow_pass:
			return &shadow_pass;
		case vkutil::MaterialPass::transparency:
			return &transparency_pass;

	}
}

void SceneManager::RegisterObjectBatch(DrawContext ctx)
{
	renderables.clear();
	for (const auto& object: ctx.OpaqueSurfaces)
	{
		forward_pass.flat_objects.push_back(object);
	}
	
	for (const auto& object : ctx.TransparentSurfaces)
	{
		transparency_pass.flat_objects.push_back(object);
	}
	std::copy(forward_pass.flat_objects.begin(), forward_pass.flat_objects.end(), std::back_inserter(renderables));
	std::copy(transparency_pass.flat_objects.begin(), transparency_pass.flat_objects.end(), std::back_inserter(renderables));
	early_depth_pass.flat_objects = renderables;
	shadow_pass.flat_objects = forward_pass.flat_objects;
}

void SceneManager::RegisterMeshAssetReference(std::string_view mesh_reference)
{
	mesh_assets.push_back(std::string(mesh_reference));
}
void SceneManager::UpdateObjectDataBuffers()
{

}

AllocatedBuffer* SceneManager::GetObjectDataBuffer()
{
	return &object_data_buffer;
}

AllocatedBuffer* SceneManager::GetIndirectCommandBuffer()
{
	return &indirect_command_buffer;
}
size_t SceneManager::GetModelCount()
{
	return renderables.size();
}

VkDeviceAddress* SceneManager::GetMergedDeviceAddress() {
	return &mergedVertexAddress;
}