#pragma once

#ifndef RENDER_GRAPH_H
#define RENDER_GRAPH_H
#include "vk_types.h"
#include "render_pass.h"
#include "resource_manager.h"

namespace black_key{
	enum class RenderGraphResourceType {
		invalid_resource = -1,
		texture_resource = 0,
		buffer_resource = 1,
		attachment_resource = 2,
		reference_resource = 3
	};

	using RenderGraphHandle = uint32_t;

	struct RenderGraphResourceHandle
	{	
		RenderGraphHandle index;
	};

	struct RenderGraphNodeHandle
	{
		RenderGraphHandle index;
	};

	struct RenderGraphResourceInfo
	{
		bool external = false;
		union {
			struct {
				size_t size;
				VkBufferUsageFlags flags;

				BufferHandle buffer;
			} buffer;
			struct{
				uint32_t width;
				uint32_t height;
				uint32_t depth;

				VkFormat format;
				VkImageUsageFlags flags;
				RenderPassEnum op = RenderPassEnum::Load;

				TextureHandle texture;
			} texture;
		};
	};

	struct RenderGraphResource {
		RenderGraphResourceType type;
		RenderGraphResourceInfo resource_info;

		RenderGraphNodeHandle producer;
		RenderGraphResourceHandle output_handle;

		uint32_t ref_count = 0;

		std::string name;
	};


	struct RenderGraphResourceCreation {
		RenderGraphResourceType                  type;
		RenderGraphResourceInfo                  resource_info;

		std::string name;
	};

	struct RenderGraphNodeCreation {
		std::vector<RenderGraphResourceCreation>  inputs;
		std::vector<RenderGraphResourceCreation> outputs;

		bool                                    enabled;

		std::string name;
	};

	struct RenderGraphRenderPass
	{
		virtual void                            add_ui() {}
		virtual void                            pre_render(VkCommandBuffer* gpu_commands/*RenderScene render_scene*/ ) {}
		virtual void                            render(VkCommandBuffer* gpu_commands/*RenderScene render_scene*/) {}
		virtual void                            on_resize(VkDevice& gpu, uint32_t new_width, uint32_t new_height) {}
	};

	struct RenderGraphNode {
		uint32_t                                     ref_count = 0;

		RenderPassHandle                        render_pass;
		RenderTargetHandle                       render_target;

		RenderGraphRenderPass* graph_render_pass;

		std::vector<RenderGraphResourceHandle>         inputs;
		std::vector<RenderGraphResourceHandle>         outputs;

		std::vector<RenderGraphNodeHandle>             edges;

		bool                                    enabled = true;

		const char* name = nullptr;
	};

	struct RenderGraphRenderPassCache {
		void                                    init();
		void                                    shutdown();

		std::unordered_map<uint32_t, RenderGraphRenderPass> render_pass_map;
		//FlatHashMap<u64, FrameGraphRenderPass*> render_pass_map;
	};

	struct RenderGraphResourceCache {
		void                                        init(VkDevice* device);
		void                                        shutdown();

		VkDevice* device;

		std::unordered_map<uint32_t, uint32_t> resource_map;
		//ResourcePoolTyped<FrameGraphResource>       resources;
	};

	struct RenderGraphNodeCache {
		void                                    init(VkDevice* device);
		void                                    shutdown();

		VkDevice* device;

		std::unordered_map<uint32_t, uint32_t> resource_map;
		//ResourcePool                            nodes;
	};

	//
	//
	struct RenderGraphBuilder {
		void                            init(VkDevice* device);
		void                            shutdown();

		void                            register_render_pass(std::string name, RenderGraph* render_pass);

		RenderGraphResourceHandle        create_node_output(const RenderGraphResourceCreation& creation, RenderGraphNodeHandle producer);
		RenderGraphResourceHandle        create_node_input(const RenderGraphResourceCreation& creation);
		RenderGraphNodeHandle            create_node(const RenderGraphNodeCreation& creation);

		RenderGraphNode* get_node(std::string name);
		RenderGraphNode* access_node(RenderGraphNodeHandle handle);

		RenderGraphResource* get_resource(std::string name);
		RenderGraphResource* access_resource(RenderGraphResourceHandle handle);

		RenderGraphResourceCache         resource_cache;
		RenderGraphNodeCache             node_cache;
		RenderGraphRenderPassCache       render_pass_cache;

		//Allocator* allocator;

		VkDevice* device;

		static constexpr uint32_t k_max_render_pass_count = 256;
		static constexpr uint32_t k_max_resources_count = 1024;
		static constexpr uint32_t k_max_nodes_count = 1024;

		const std::string k_name =  "raptor_frame_graph_builder_service";
	};

	struct RenderGraph {
		void                            init(RenderGraphBuilder* builder, ResourceManager* resource_manager = nullptr);
		void                            shutdown();

		void                            parse(std::string_view file_path);

		// NOTE(marco): each frame we rebuild the graph so that we can enable only
		// the nodes we are interested in
		void                            reset();
		void add_render_pass();
		void                            enable_render_pass(std::string_view render_pass_name);
		void                            disable_render_pass(std::string_view render_pass_name);
		void                            compile();
		void                            add_ui();
		void                            render(VkCommandBuffer* gpu_commands/*RenderScene render_scene*/);
		void                            on_resize(VkDevice& gpu, uint32_t new_width, uint32_t new_height);

		RenderGraphNode* get_node(std::string_view name);
		RenderGraphNode* access_node(RenderGraphNodeHandle handle);

		RenderGraphResource* get_resource(std::string_view name);
		RenderGraphResource* access_resource(RenderGraphResourceHandle handle);

		// TODO(marco): in case we want to add a pass in code
		void                            add_node(RenderGraphNodeCreation& node);

		// NOTE(marco): nodes sorted in topological order
		std::vector<RenderGraphNodeHandle>     nodes;
		std::vector<Task> tasks;

		RenderGraphBuilder* builder;
		ResourceManager* resource_manager;

		std::string name = nullptr;
	};
};
#endif