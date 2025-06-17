#include "vk_descriptors.h"

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type, uint32_t count)
{
    VkDescriptorSetLayoutBinding newbind{};
    newbind.binding = binding;
    newbind.descriptorCount = count;
    newbind.descriptorType = type;
    

    bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear()
{
    bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
{
    for (auto& b : bindings) {
        b.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutCreateInfo info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.pNext = pNext;
    
    info.pBindings = bindings.data();
    info.bindingCount = (uint32_t)bindings.size();
    info.flags = flags;

    if ((flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT) == VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT)
    {
        VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT };
        extended_info.pNext = nullptr;
        extended_info.bindingCount = (uint32_t)bindings.size();
        VkDescriptorBindingFlagsEXT bindless_flags =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT |
            VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT;
        
        VkDescriptorBindingFlagsEXT binding_flags[3];
        binding_flags[0] = bindless_flags;
        binding_flags[1] = bindless_flags;
        binding_flags[2] = bindless_flags;

        extended_info.pBindingFlags = binding_flags;
        info.pNext = &extended_info;
    }
    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}

VkDescriptorSet DescriptorAllocatorBindless::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

    return ds;
}

void DescriptorAllocatorBindless::init_bindless_pool(VkDevice device, uint32_t MAX_BINDLESS_RESOURCES)
{
    std::vector<VkDescriptorPoolSize> pool_sizes_bindless = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_BINDLESS_RESOURCES},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_BINDLESS_RESOURCES},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_BINDLESS_RESOURCES}
    };

    VkDescriptorPoolCreateInfo pool_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    pool_info.maxSets = MAX_BINDLESS_RESOURCES * pool_sizes_bindless.size();
    pool_info.poolSizeCount = (uint32_t)pool_sizes_bindless.size();
    pool_info.pPoolSizes = pool_sizes_bindless.data();

    vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
}

void DescriptorAllocatorBindless::clear_descriptors(VkDevice device)
{
    vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocatorBindless::destroy_pool(VkDevice device)
{
    vkDestroyDescriptorPool(device, pool, nullptr);
}

void DescriptorAllocator::init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios, bool bindless)
{
    VkDescriptorPoolCreateInfo pool_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };

    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio.type,
            .descriptorCount = uint32_t(ratio.ratio * maxSets)
            });
    }

    pool_info.flags = 0;
    if (bindless)
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    pool_info.maxSets = maxSets;
    pool_info.poolSizeCount = (uint32_t)poolSizes.size();
    pool_info.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
}

void DescriptorAllocator::clear_descriptors(VkDevice device)
{
    vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device)
{
    vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

    return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device)
{
    VkDescriptorPool newPool;
    if (readyPools.size() != 0) {
        newPool = readyPools.back();
        readyPools.pop_back();
    }
    else {
        //need to create a new pool
        newPool = create_pool(device, setsPerPool, ratios);

        setsPerPool = setsPerPool * 1.5;
        if (setsPerPool > 4092) {
            setsPerPool = 4092;
        }
    }

    return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio.type,
            .descriptorCount = uint32_t(ratio.ratio * setCount)
            });
    }

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0;
    pool_info.maxSets = setCount;
    pool_info.poolSizeCount = (uint32_t)poolSizes.size();
    pool_info.pPoolSizes = poolSizes.data();

    VkDescriptorPool newPool;
    vkCreateDescriptorPool(device, &pool_info, nullptr, &newPool);
    return newPool;
}

void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
    ratios.clear();

    for (auto r : poolRatios) {
        ratios.push_back(r);
    }

    VkDescriptorPool newPool = create_pool(device, maxSets, poolRatios);

    setsPerPool = maxSets * 1.5; //grow it next allocation

    readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::clear_pools(VkDevice device)
{
    for (auto p : readyPools) {
        vkResetDescriptorPool(device, p, 0);
    }
    for (auto p : fullPools) {
        vkResetDescriptorPool(device, p, 0);
        readyPools.push_back(p);
    }
    fullPools.clear();
}

void DescriptorAllocatorGrowable::destroy_pools(VkDevice device)
{
    for (auto p : readyPools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    readyPools.clear();
    for (auto p : fullPools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    fullPools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext)
{
    //get or create a pool to allocate from
    VkDescriptorPool poolToUse = get_pool(device);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = pNext;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = poolToUse;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

    //allocation failed. Try again
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {

        fullPools.push_back(poolToUse);

        poolToUse = get_pool(device);
        allocInfo.descriptorPool = poolToUse;

        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
    }

    readyPools.push_back(poolToUse);
    return ds;
}


void DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type, int arr_index)
{
    VkDescriptorBufferInfo& info = bufferInfos.emplace_back(VkDescriptorBufferInfo{
        .buffer = buffer,
        .offset = offset,
        .range = size
        });

    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &info;
    if (arr_index >= 0)
        write.dstArrayElement = arr_index;

    writes.push_back(write);
}

void DescriptorWriter::write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type, int arr_index)
{
    VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{
        .sampler = sampler,
        .imageView = image,
        .imageLayout = layout
        });

    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &info;
    if (arr_index >= 0)
        write.dstArrayElement = arr_index;
    
   
    writes.push_back(write);
}

void DescriptorWriter::clear()
{
    imageInfos.clear();
    writes.clear();
    bufferInfos.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set)
{
    for (VkWriteDescriptorSet& write : writes) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}