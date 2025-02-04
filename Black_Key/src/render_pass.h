#pragma once
#ifndef RENDER_PASS_H
#define RENDER_PASS_H
#include "vk_types.h"
#include <vector>
#include <optional>
struct RenderPass {
	VkRenderingAttachmentInfo depth_attachment;
	std::vector<VkRenderingAttachmentInfo> color_attachments;
	VkClearValue clear_value;
	RenderPass(std::optional<VkRenderingAttachmentInfo> depth_attachment_, std::optional<std::vector<VkRenderingAttachmentInfo>> color_attachments_);
	VkRenderingInfo render_info;
};

struct Task {
	enum struct Types {
		render_pass,
		compute_pass,
		transfer_pass,
		image_transition_pass,
	};
	Types type;
	size_t index;
};
#endif