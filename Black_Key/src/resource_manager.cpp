#include "resource_manager.h"
#include "stb_image.h"
#include "vk_engine.h"

#define USE_BINDLESS

void ResourceManager::init(VulkanEngine* engine_ptr) {
    engine = engine_ptr;

    std::vector<DescriptorAllocator::PoolSizeRatio> bindless_sizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
    };
    bindless_material_descriptor.init_pool(engine->_device, 65536, bindless_sizes, true);
    
}

VkFilter ResourceManager::extract_filter(fastgltf::Filter filter)
{
    switch (filter) {
        // nearest samplers
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
        return VK_FILTER_NEAREST;

        // linear samplers
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode ResourceManager::extract_mipmap_mode(fastgltf::Filter filter)
{
    switch (filter) {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}



std::optional<std::shared_ptr<LoadedGLTF>> ResourceManager::loadGltf(VulkanEngine* engine, std::string_view filePath, bool isPBRMaterial)
{
    std::string rootPath(filePath.begin(), filePath.end());
    rootPath = rootPath.substr(0, rootPath.find_last_of('/') + 1);
    auto name = filePath.substr(filePath.find_last_of('/') + 1, filePath.size() - (filePath.find_last_of('.') -1 ));
   

    //> load_1
    fmt::println("Loading GLTF: {}", std::string(name) + ".gltf");

    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    scene->creator = engine;
    LoadedGLTF& file = *scene.get();

    fastgltf::Parser parser{};

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
    // fastgltf::Options::LoadExternalImages;

    fastgltf::GltfDataBuffer data;
    data.loadFromFile(filePath);

    fastgltf::Asset gltf;

    std::filesystem::path path = filePath;

    auto type = fastgltf::determineGltfFileType(&data);
    if (type == fastgltf::GltfType::glTF) {
        auto load = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
        if (load) {
            gltf = std::move(load.get());
        }
        else {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    }
    else if (type == fastgltf::GltfType::GLB) {
        auto load = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
        if (load) {
            gltf = std::move(load.get());
        }
        else {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    }
    else {
        std::cerr << "Failed to determine glTF container" << std::endl;
        return {};
    }
    //< load_1
    //> load_2
        // we can estimate the descriptors we will need accurately
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } };

    file.descriptorPool.init(engine->_device, gltf.materials.size(), sizes);
    //< load_2
    //> load_samplers

        // load samplers
    for (fastgltf::Sampler& sampler : gltf.samplers) {

        VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr };
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;

        sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        sampl.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(engine->_device, &sampl, nullptr, &newSampler);

        file.samplers.push_back(newSampler);
    }
    //< load_samplers
    //> load_arrays
        // temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;
    //< load_arrays

    int count = 0;
    // load all textures
    for (fastgltf::Image& image : gltf.images) {
        std::optional<AllocatedImage> img = load_image(engine, gltf, image, rootPath);

        if (img.has_value()) {
            images.push_back(*img);
            file.images["image" + std::to_string(count)] = *img;
            count++;
        }
        else {
            // we failed to load, so lets give the slot a default white texture to not
            // completely break loading
            images.push_back(engine->errorCheckerboardImage);
            std::cout << "gltf failed to load texture " << image.name << std::endl;
        }
    }

    //> load_buffer
        // create buffer to hold the material data
    file.materialDataBuffer = engine->create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    int material_index = last_material_index;
    int data_index = 0;

    GLTFMetallic_Roughness::MaterialConstants* sceneMaterialConstants = (GLTFMetallic_Roughness::MaterialConstants*)file.materialDataBuffer.info.pMappedData;
    //< load_buffer
        //
    //> load_material
    //std::vector< GLTFMetallic_Roughness::MaterialResources> bindless_resources;
    //bindless_resources.reserve(gltf.materials.size());

    for (fastgltf::Material& mat : gltf.materials) {
        std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
        materials.push_back(newMat);
        file.materials[mat.name.c_str()] = newMat;


        GLTFMetallic_Roughness::MaterialConstants constants;
        constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
        constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
        constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
        constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

        constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
        constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;
        // write material parameters to buffer
        sceneMaterialConstants[data_index] = constants;

        vkutil::MaterialPass passType = vkutil::MaterialPass::forward;
        if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
            passType = vkutil::MaterialPass::transparency;
        }

        GLTFMetallic_Roughness::MaterialResources materialResources;
        // default the material textures
        materialResources.colorImage = engine->_whiteImage;
        materialResources.colorSampler = engine->defaultSamplerLinear;
        materialResources.metalRoughImage = engine->_whiteImage;
        materialResources.metalRoughSampler = engine->defaultSamplerLinear;
        materialResources.normalImage = engine->_whiteImage;
        materialResources.normalSampler = engine->defaultSamplerLinear;
        materialResources.occlusionImage = engine->_whiteImage;
        materialResources.occlusionSampler = engine->defaultSamplerLinear;


        // set the uniform buffer for the material data
        materialResources.dataBuffer = file.materialDataBuffer.buffer;
        materialResources.dataBufferOffset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants);
        // grab textures from gltf file
        if (mat.pbrData.baseColorTexture.has_value()) {
            size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

            materialResources.colorImage = images[img];
            materialResources.colorSampler = file.samplers[sampler];
        }

        if (mat.pbrData.metallicRoughnessTexture.has_value())
        {
            size_t img = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex.value();

            materialResources.metalRoughImage = images[img];
            materialResources.metalRoughSampler = file.samplers[sampler];
            materialResources.rough_metallic_texture_found = true;
        }

        if (mat.normalTexture.has_value())
        {
            size_t img = gltf.textures[mat.normalTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.normalTexture.value().textureIndex].samplerIndex.value();

            materialResources.normalImage = images[img];
            materialResources.normalSampler = file.samplers[sampler];
            materialResources.normal_texture_found = true;
        }

        if (mat.occlusionTexture.has_value())
        {
            size_t img = gltf.textures[mat.occlusionTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.occlusionTexture.value().textureIndex].samplerIndex.value();

            materialResources.occlusionImage = images[img];
            materialResources.occlusionSampler = file.samplers[sampler];

            materialResources.separate_occ_texture = true;
        }

        // build material
#if USE_BINDLESS 1
        //Store each textures Materials
        bindless_resources.push_back(materialResources);
        newMat->material_index = material_index * 4;
        newMat->data = engine->metalRoughMaterial.SetMaterialProperties(passType, newMat->material_index);
#else
        newMat->data = engine->metalRoughMaterial.write_material(engine->_device, passType, materialResources, file.descriptorPool);
#endif
        data_index++;
        material_index++;
    }
    last_material_index += material_index;

    //< load_material

        // use the same vectors for all meshes so that the memory doesnt reallocate as
        // often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (fastgltf::Mesh& mesh : gltf.meshes) {
        std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
        meshes.push_back(newmesh);
        file.meshes[mesh.name.c_str()] = newmesh;
        newmesh->name = mesh.name;

        // clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) {
            GeoSurface newSurface;
            newSurface.startIndex = (uint32_t)indices.size();
            newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;


            size_t initial_vtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx) {
                        indices.push_back(idx + initial_vtx);
                    });
            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index) {
                        Vertex newvtx;
                        newvtx.position = v;
                        newvtx.normal = { 1, 0, 0 };
                        newvtx.color = glm::vec4{ 1.f };
                        newvtx.uv_x = 0;
                        newvtx.uv_y = 0;
                        vertices[initial_vtx + index] = newvtx;
                    });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initial_vtx + index].normal = v;
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                    [&](glm::vec2 v, size_t index) {
                        vertices[initial_vtx + index].uv_x = v.x;
                        vertices[initial_vtx + index].uv_y = v.y;
                    });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initial_vtx + index].color = v;
                    });
            }

            auto tangent = p.findAttribute("TANGENT");
            if (tangent != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*tangent).second],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initial_vtx + index].tangents = glm::vec4(v);
                    });
            }


            if (p.materialIndex.has_value()) {
                newSurface.material = materials[p.materialIndex.value()];
            }
            else {
                newSurface.material = materials[0];
            }

            glm::vec3 minpos = vertices[initial_vtx].position;
            glm::vec3 maxpos = vertices[initial_vtx].position;
            for (int i = initial_vtx; i < vertices.size(); i++) {
                minpos = glm::min(minpos, vertices[i].position);
                maxpos = glm::max(maxpos, vertices[i].position);
            }

            newSurface.bounds.origin = (maxpos + minpos) / 2.f;
            newSurface.bounds.extents = (maxpos - minpos) / 2.f;
            newSurface.bounds.sphereRadius = glm::length(newSurface.bounds.extents);
            newSurface.vertex_count = vertices.size() - initial_vtx; 
            newmesh->surfaces.push_back(newSurface);
        }

        newmesh->meshBuffers = engine->uploadMesh(indices, vertices);
    }
    //> load_nodes
        // load all nodes and their meshes
    for (fastgltf::Node& node : gltf.nodes) {
        std::shared_ptr<Node> newNode;

        // find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
        if (node.meshIndex.has_value()) {
            newNode = std::make_shared<MeshNode>();
            static_cast<MeshNode*>(newNode.get())->mesh = meshes[*node.meshIndex];
        }
        else {
            newNode = std::make_shared<Node>();
        }

        nodes.push_back(newNode);
        file.nodes[node.name.c_str()];

        std::visit(fastgltf::visitor{ [&](fastgltf::Node::TransformMatrix matrix) {
                                          memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
                                      },
                       [&](fastgltf::Node::TRS transform) {
                           glm::vec3 tl(transform.translation[0], transform.translation[1],
                               transform.translation[2]);
                           glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                               transform.rotation[2]);
                           glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                           glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                           glm::mat4 rm = glm::toMat4(rot);
                           glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                           newNode->localTransform = tm * rm * sm;
                       } },
            node.transform);
    }
    //< load_nodes
    //> load_graph
        // run loop again to setup transform hierarchy
    for (int i = 0; i < gltf.nodes.size(); i++) {
        fastgltf::Node& node = gltf.nodes[i];
        std::shared_ptr<Node>& sceneNode = nodes[i];

        for (auto& c : node.children) {
            sceneNode->children.push_back(nodes[c]);
            nodes[c]->parent = sceneNode;
        }
    }

    // find the top nodes, with no parents
    for (auto& node : nodes) {
        if (node->parent.lock() == nullptr) {
            file.topNodes.push_back(node);
            node->refreshTransform(glm::mat4{ 1.f });
        }
    }
    //loadedScenes[name] = scene;
    return scene;
}

std::optional<AllocatedImage> ResourceManager::load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image, const std::string& rootPath)
{
    AllocatedImage newImage{};

    int width, height, nrChannels;

    std::visit(
        fastgltf::visitor{
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath) {
                assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(filePath.uri.isLocalPath()); // We're only capable of loading
                // local files.

                std::string path(filePath.uri.path().begin(),
                filePath.uri.path().end()); // Thanks C++.
                std::cout << filePath.uri.string() << std::endl;
                path = rootPath + path;
                unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = vkutil::create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, engine, true);

                    stbi_image_free(data);
                }
},
[&](fastgltf::sources::Vector& vector) {
    unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
        &width, &height, &nrChannels, 4);
    if (data) {
        VkExtent3D imagesize;
        imagesize.width = width;
        imagesize.height = height;
        imagesize.depth = 1;

        newImage = vkutil::create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, engine, true);

        stbi_image_free(data);
    }
},
[&](fastgltf::sources::BufferView& view) {
    auto& bufferView = asset.bufferViews[view.bufferViewIndex];
    auto& buffer = asset.buffers[bufferView.bufferIndex];

    std::visit(fastgltf::visitor { // We only care about VectorWithMime here, because we
        // specify LoadExternalBuffers, meaning all buffers
        // are already loaded into a vector.
[](auto& arg) {},
[&](fastgltf::sources::Vector& vector) {
    unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset,
        static_cast<int>(bufferView.byteLength),
        &width, &height, &nrChannels, 4);
    if (data) {
        VkExtent3D imagesize;
        imagesize.width = width;
        imagesize.height = height;
        imagesize.depth = 1;

        newImage = vkutil::create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, engine, true);

        stbi_image_free(data);
    }
} },
buffer.data);
},
        },
        image.data);

    // if any of the attempts to load the data failed, we havent written the image
    // so handle is null
    if (newImage.image == VK_NULL_HANDLE) {
        return {};
    }
    else {
        return newImage;
    }
}

void ResourceManager::cleanup()
{
    vkDestroyDescriptorPool(engine->_device, bindless_material_descriptor.pool, nullptr);
}



void ResourceManager::write_material_array()
{
    writer.clear();
    for (int i = 0; i < bindless_resources.size(); i++)
    {
        int offset = i * 4;
        writer.write_buffer(0, bindless_resources[i].dataBuffer, sizeof(GLTFMetallic_Roughness::MaterialConstants), bindless_resources[i].dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2);
        writer.write_image(1, bindless_resources[i].colorImage.imageView, bindless_resources[i].colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offset);
        writer.write_image(1, bindless_resources[i].metalRoughImage.imageView, bindless_resources[i].metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offset + 1);
        writer.write_image(1, bindless_resources[i].normalImage.imageView, bindless_resources[i].normalSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offset + 2);
        writer.write_image(1, bindless_resources[i].occlusionImage.imageView, bindless_resources[i].occlusionSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offset + 3);
        writer.write_image(2, engine->storageImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, i);
    }

    bindless_set = bindless_material_descriptor.allocate(engine->_device, engine->bindless_descriptor_layout);
    writer.update_set(engine->_device, bindless_set);
}

VkDescriptorSet* ResourceManager::GetBindlessSet()
{
    VkDescriptorSet* desc = &bindless_set;
    return desc;
}

void ResourceManager::ReadBackBufferData(VkCommandBuffer cmd, AllocatedBuffer* buffer)
{
    if (readBackBufferInitialized == true)
    {
        engine->destroy_buffer(readableBuffer);
    }

    readableBuffer = engine->create_buffer(buffer->info.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    readBackBufferInitialized = true;

    VkBufferCopy dataCopy{ 0 };
    dataCopy.dstOffset = 0;
    dataCopy.srcOffset = 0;
    dataCopy.size = buffer->info.size -64;

    vkCmdCopyBuffer(cmd, buffer->buffer, readableBuffer.buffer, 1, &dataCopy);

}

AllocatedBuffer* ResourceManager::GetReadBackBuffer()
{
    return &readableBuffer;
}