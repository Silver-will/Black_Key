#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_buffer.h"
#include "vk_engine.h"

#include <stb_image.h>
#include <ktx.h>
#include <ktxvulkan.h>


void vkutil::transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
{
    VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    imageBarrier.pNext = nullptr;

    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;

    VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL || currentLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask);
    imageBarrier.image = image;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

void vkutil::copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize, VkImageBlit2* region)
{
	VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

	blitRegion.srcOffsets[1].x = srcSize.width;
	blitRegion.srcOffsets[1].y = srcSize.height;
	blitRegion.srcOffsets[1].z = 1;

	blitRegion.dstOffsets[1].x = dstSize.width;
	blitRegion.dstOffsets[1].y = dstSize.height;
	blitRegion.dstOffsets[1].z = 1;

	blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.srcSubresource.baseArrayLayer = 0;
	blitRegion.srcSubresource.layerCount = 1;
	blitRegion.srcSubresource.mipLevel = 0;

	blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.dstSubresource.baseArrayLayer = 0;
	blitRegion.dstSubresource.layerCount = 1;
	blitRegion.dstSubresource.mipLevel = 0;

    if (region != nullptr)
        blitRegion = *region;

	VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
	blitInfo.dstImage = destination;
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.srcImage = source;
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitInfo.filter = VK_FILTER_LINEAR;
	blitInfo.regionCount = 1;
	blitInfo.pRegions = &blitRegion;

	vkCmdBlitImage2(cmd, &blitInfo);
}


void vkutil::generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent3D imageSize, int faces)
{
    int mipLevels = int(std::floor(std::log2(std::max(imageSize.depth,std::max(imageSize.width, imageSize.height))))) + 1;
    for (int mip = 0; mip < mipLevels; mip++) {

        VkExtent3D halfSize = imageSize;
        halfSize.width /= 2;
        halfSize.height /= 2;
        halfSize.depth = halfSize.depth > 1 ? halfSize.depth / 2 : 1;

        VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr };

        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask);
        imageBarrier.subresourceRange.levelCount = 1;
        imageBarrier.subresourceRange.baseMipLevel = mip;
        imageBarrier.image = image;

        VkDependencyInfo depInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr };
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imageBarrier;

        vkCmdPipelineBarrier2(cmd, &depInfo);

        if (mip < mipLevels - 1) {
            VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };
           // blitRegion.srcOffsets[0] = { 0, 0, 0 };
            blitRegion.srcOffsets[1].x = imageSize.width;
            blitRegion.srcOffsets[1].y = imageSize.height;
            blitRegion.srcOffsets[1].z = imageSize.depth;
            
           // blitRegion.dstOffsets[0] = { 0, 0, 0 };
            blitRegion.dstOffsets[1].x = halfSize.width;
            blitRegion.dstOffsets[1].y = halfSize.height;
            blitRegion.dstOffsets[1].z = halfSize.depth;

            blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.srcSubresource.baseArrayLayer = 0;
            blitRegion.srcSubresource.layerCount = faces;
            blitRegion.srcSubresource.mipLevel = mip;

            blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.dstSubresource.baseArrayLayer = 0;
            blitRegion.dstSubresource.layerCount = faces;
            blitRegion.dstSubresource.mipLevel = mip + 1;

            VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
            blitInfo.dstImage = image;
            blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            blitInfo.srcImage = image;
            blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            blitInfo.filter = VK_FILTER_LINEAR;
            blitInfo.regionCount = 1;
            blitInfo.pRegions = &blitRegion;

            vkCmdBlitImage2(cmd, &blitInfo);

            imageSize = halfSize;
        }
    }

    // transition all mip levels into the final read_only layout
    transition_image(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

AllocatedImage vkutil::create_image_empty(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VulkanEngine* engine, VkImageViewType viewType, bool mipmapped, int layers, VkSampleCountFlagBits msaaSamples, int mipLevels)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size, layers, msaaSamples);
    if (mipmapped) {
        img_info.mipLevels = mipLevels != -1 ? mipLevels : static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(engine->_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // build a image-view for the image
    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag, viewType,layers);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(engine->_device, &view_info, nullptr, &newImage.imageView));
   
    return newImage;
}

AllocatedImage vkutil::create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VulkanEngine* engine, bool mipmapped)
{
    size_t data_size = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadbuffer = create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,engine);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    AllocatedImage new_image = create_image_empty(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,engine,VK_IMAGE_VIEW_TYPE_2D, mipmapped);
    

    engine->immediate_submit([&](VkCommandBuffer cmd) {
        vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = size;

    
    // copy the buffer into the image
    vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
        &copyRegion);

    if (mipmapped) {
        vkutil::generate_mipmaps(cmd, new_image.image, VkExtent3D(new_image.imageExtent.width,new_image.imageExtent.height, new_image.imageExtent.depth));
    }
    else {
        if(format == VK_IMAGE_USAGE_SAMPLED_BIT)
            vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        else if (format == VK_IMAGE_USAGE_STORAGE_BIT)
            vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        else if (format == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        
        else
           vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
        });
    destroy_buffer(uploadbuffer, engine);
    return new_image;
}

AllocatedImage vkutil::create_cubemap_image(VkExtent3D size, VulkanEngine* engine, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;


    uint32_t mipCount = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    VkImageCreateInfo img_info = vkinit::image_cubemap_create_info(format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, size, mipCount);

    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(engine->_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    // build a image-view for the image

    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag, VK_IMAGE_VIEW_TYPE_CUBE, 6);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(engine->_device, &view_info, nullptr, &newImage.imageView));
    return newImage;
}

AllocatedImage vkutil::load_cubemap_image(std::string_view path, VkExtent3D size,VulkanEngine* engine, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    ktxResult result;
    ktxTexture* texture;
    
    result = ktxTexture_CreateFromNamedFile(std::string(path).c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
    assert(result == KTX_SUCCESS);

    size.width = texture->baseWidth;
    size.height = texture->baseHeight;
    uint32_t mipCount = texture->numLevels;


    ktx_uint8_t* ktxTextureData = ktxTexture_GetData(texture);
    auto elementSize = ktxTexture_GetElementSize(texture);
    ktx_size_t ktxTextureSize = ktxTexture_GetDataSizeUncompressed(texture);
   
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

 
    //uint32_t mipCount = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    VkImageCreateInfo img_info = vkinit::image_cubemap_create_info(format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, size, mipCount);

    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(engine->_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    // build a image-view for the image
    
    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag, VK_IMAGE_VIEW_TYPE_CUBE,6);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(engine->_device, &view_info, nullptr, &newImage.imageView));

    AllocatedBuffer uploadbuffer = vkutil::create_buffer(ktxTextureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,engine);
    memcpy(uploadbuffer.info.pMappedData, ktxTextureData, ktxTextureSize);

    engine->immediate_submit([&](VkCommandBuffer cmd)
        {
            vkutil::transition_image(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            std::vector<VkBufferImageCopy> bufferCopyRegions;
            
                size_t offset;
                for (uint32_t face = 0; face < 6; face++)
                {
                    for (uint32_t level = 0; level < mipCount; level++)
                    {
                        ktx_size_t offset;
                        KTX_error_code ret = ktxTexture_GetImageOffset(texture, level, 0, face, &offset);
                        assert(ret == KTX_SUCCESS);
                        VkBufferImageCopy bufferCopyRegion = {};
                        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        bufferCopyRegion.imageSubresource.mipLevel = level;
                        bufferCopyRegion.imageSubresource.baseArrayLayer = face;
                        bufferCopyRegion.imageSubresource.layerCount = 1;
                        bufferCopyRegion.imageExtent.width = texture->baseWidth >> level;
                        bufferCopyRegion.imageExtent.height = texture->baseHeight >> level;
                        bufferCopyRegion.imageExtent.depth = 1;
                        bufferCopyRegion.bufferOffset = offset;
                        bufferCopyRegions.push_back(bufferCopyRegion);
                    }
                }

                vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, bufferCopyRegions.size(),
                    bufferCopyRegions.data());

                if (mipmapped) {
                    vkutil::generate_mipmaps(cmd, newImage.image, VkExtent3D{ newImage.imageExtent.width,newImage.imageExtent.height, newImage.imageExtent.depth });
                }
                else {
                    vkutil::transition_image(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                }

        });
    destroy_buffer(uploadbuffer, engine);
    
    return newImage;
}


void vkutil::destroy_image(const AllocatedImage& img, VulkanEngine* engine)
{
    vkDestroyImageView(engine->_device, img.imageView, nullptr);
    vmaDestroyImage(engine->_allocator, img.image, img.allocation);
}

AllocatedImage vkutil::create_array_image(VkExtent3D size, VulkanEngine* engine, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage image;
    return image;
}