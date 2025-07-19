#include "vk_util.h"

void vkutil::InsertDebugLabel(VkCommandBuffer cmd, std::string_view label, glm::vec4 color)
{
	/*	std::string label_s(label);
	
	const VkDebugUtilsLabelEXT utilsLabel = {
	.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
	.pNext = nullptr,
	.pLabelName = label_s.c_str(),
	.color = {color.r, color.g, color.b, color.a}
	};
	vkCmdInsertDebugUtilsLabelEXT(cmd, &utilsLabel);
	*/
}