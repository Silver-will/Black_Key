#include "render_graph.h"

namespace black_key {
	void RenderGraph::init(RenderGraphBuilder* builder, ResourceManager* resource_manager)
	{
		this->builder = builder;
		this->resource_manager = resource_manager;
		nodes = std::vector<RenderGraphNodeHandle>(RenderGraphBuilder::k_max_nodes_count);
	}

	void RenderGraph::shutdown()
	{
		for (size_t i = 0; i < nodes.size(); i++)
		{
			RenderGraphNodeHandle handle = nodes[i];

			/*Come back here*/
		}
		nodes.clear();
	}
};