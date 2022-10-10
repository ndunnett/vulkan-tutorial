#include "resources.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace tutorial {
    UboResource::UboResource(VulkanCore* vulkan) {
        std::tie(buffer, memory) = vulkan->create_buffer(
            sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    }

    FrameTransient::FrameTransient(VulkanCore* vulkan, vk::UniqueCommandBuffer& command_buffer)
        : command_buffer(std::move(command_buffer)),
          image_available(vulkan->logical_device->createSemaphoreUnique({})),
          render_finished(vulkan->logical_device->createSemaphoreUnique({})),
          in_flight(vulkan->logical_device->createFenceUnique({ vk::FenceCreateFlagBits::eSignaled })),
          ubo(vulkan) {}

    FrameTransients::FrameTransients(VulkanCore* vulkan, size_t frames_in_flight) : vulkan(vulkan) {
        vk::CommandBufferAllocateInfo ai{};
        ai.setCommandPool(*vulkan->command_pool);
        ai.setLevel(vk::CommandBufferLevel::ePrimary);
        ai.setCommandBufferCount(frames_in_flight);

        for (auto& command_buffer : vulkan->logical_device->allocateCommandBuffersUnique(ai)) {
            m_frames.emplace_back(vulkan, command_buffer);
        }
    }

    void FrameTransients::wait_for_fences() {
        if (vulkan->logical_device->waitForFences(*m_frames.at(m_frame_index).in_flight, VK_TRUE,
                                                  UINT64_MAX) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to wait for frame fences!");
        }
    }

    void FrameTransients::reset_fences() {
        vulkan->logical_device->resetFences(*m_frames.at(m_frame_index).in_flight);
    }

    vk::ResultValue<uint32_t> FrameTransients::next_image_index(const vk::SwapchainKHR& swapchain) {
        auto index = vulkan->logical_device->acquireNextImageKHR(swapchain, UINT64_MAX,
                                                                 *m_frames.at(m_frame_index).image_available);

        if (index.result != vk::Result::eSuccess && index.result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("failed to acquire swapchain image!");
        }

        return index;
    }

    void FrameTransients::reset_command_buffer() {
        m_frames.at(m_frame_index).command_buffer->reset();
    }

    void FrameTransients::submit(const vk::Queue& queue, vk::PipelineStageFlags dst_stage_mask) {
        vk::SubmitInfo si{};
        si.setWaitSemaphores(*m_frames.at(m_frame_index).image_available);
        si.setWaitDstStageMask(dst_stage_mask);
        si.setCommandBuffers(*m_frames.at(m_frame_index).command_buffer);
        si.setSignalSemaphores(*m_frames.at(m_frame_index).render_finished);
        queue.submit(si, *m_frames.at(m_frame_index).in_flight);
    }

    vk::Result FrameTransients::present(const vk::Queue& queue, const vk::SwapchainKHR& swapchain,
                                        uint32_t index) {
        vk::PresentInfoKHR pi{};
        pi.setWaitSemaphores(*m_frames.at(m_frame_index).render_finished);
        pi.setSwapchains(swapchain);
        pi.setImageIndices(index);

        auto result = queue.presentKHR(pi);

        if (result != vk::Result::eSuccess && result != vk::Result::eErrorOutOfDateKHR &&
            result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("failed to present swapchain image!");
        }

        return result;
    }

    void FrameTransients::update_ubo(const UniformBufferObject& new_ubo) {
        vulkan->copy_to_memory(*m_frames.at(m_frame_index).ubo.memory, &new_ubo, sizeof(UniformBufferObject));
    }

    ImageResource::ImageResource(VulkanCore* vulkan, const ImageProperties& properties)
        : vulkan(vulkan), properties(properties) {
        vk::Extent3D extent{ properties.size.first, properties.size.second, 1 };

        vk::ImageCreateInfo image_ci{};
        image_ci.setImageType(vk::ImageType::e2D);
        image_ci.setExtent(extent);
        image_ci.setMipLevels(properties.mip_levels);
        image_ci.setArrayLayers(1);
        image_ci.setFormat(properties.format);
        image_ci.setTiling(properties.tiling);
        image_ci.setInitialLayout(vk::ImageLayout::eUndefined);
        image_ci.setUsage(properties.usage);
        image_ci.setSharingMode(vk::SharingMode::eExclusive);
        image_ci.setSamples(properties.samples);

        image = vulkan->logical_device->createImageUnique(image_ci);
        auto mem_req = vulkan->logical_device->getImageMemoryRequirements(*image);

        vk::MemoryAllocateInfo image_ai{};
        image_ai.setAllocationSize(mem_req.size);
        image_ai.setMemoryTypeIndex(vulkan->find_memory_type(mem_req.memoryTypeBits, properties.memory));

        memory = vulkan->logical_device->allocateMemoryUnique(image_ai);
        vulkan->logical_device->bindImageMemory(*image, *memory, 0);

        vk::ImageViewCreateInfo view_ci{};
        view_ci.setImage(*image);
        view_ci.setViewType(vk::ImageViewType::e2D);
        view_ci.setFormat(properties.format);
        view_ci.setComponents({ {}, {}, {}, {} });
        view_ci.setSubresourceRange({ properties.aspect_flags, 0, properties.mip_levels, 0, 1 });

        view = vulkan->logical_device->createImageViewUnique(view_ci);
    }

    void ImageResource::transition_layout(const vk::Queue& queue, vk::ImageLayout old_layout,
                                          vk::ImageLayout new_layout) {
        auto commands = vulkan->get_single_time_commands(queue);

        vk::ImageMemoryBarrier barrier{};
        barrier.setOldLayout(old_layout);
        barrier.setNewLayout(new_layout);
        barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setImage(*image);
        barrier.setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, properties.mip_levels, 0, 1 });

        vk::PipelineStageFlags source_stage{};
        vk::PipelineStageFlags destination_stage{};

        if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
            barrier.setSrcAccessMask({});
            barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
            source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
            destination_stage = vk::PipelineStageFlagBits::eTransfer;
        } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
                   new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
            barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
            barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
            source_stage = vk::PipelineStageFlagBits::eTransfer;
            destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
        } else if (old_layout == vk::ImageLayout::eUndefined &&
                   new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
            barrier.setSrcAccessMask({});
            barrier.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentRead |
                                     vk::AccessFlagBits::eDepthStencilAttachmentWrite);
            source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
            destination_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        if (new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
            barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

            // has stencil component
            if (properties.format == vk::Format::eD32SfloatS8Uint ||
                properties.format == vk::Format::eD24UnormS8Uint) {
                barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
            }
        } else {
            barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        }

        commands.buffer->pipelineBarrier(source_stage, destination_stage, {}, nullptr, nullptr, barrier);
    }

    void ImageResource::copy_buffer(const vk::Queue& queue, const vk::Buffer& buffer,
                                    std::pair<uint32_t, uint32_t> image_size) {
        auto commands = vulkan->get_single_time_commands(queue);

        vk::BufferImageCopy copy_region{};
        copy_region.setBufferOffset(0);
        copy_region.setBufferRowLength(0);
        copy_region.setBufferImageHeight(0);
        copy_region.setImageSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 });
        copy_region.setImageOffset({ 0, 0, 0 });
        copy_region.setImageExtent({ image_size.first, image_size.second, 1 });

        commands.buffer->copyBufferToImage(buffer, *image, vk::ImageLayout::eTransferDstOptimal, copy_region);
    }

    void ImageResource::generate_mipmaps(const vk::Queue& queue) {
        if (properties.mip_levels == 1) {
            return transition_layout(queue, vk::ImageLayout::eTransferDstOptimal,
                                     vk::ImageLayout::eShaderReadOnlyOptimal);
        }

        auto format_properties = vulkan->physical_device.getFormatProperties(properties.format);

        if (!(format_properties.optimalTilingFeatures &
              vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
            throw std::runtime_error("texture image format does not support linear blitting!");
        }

        auto commands = vulkan->get_single_time_commands(queue);

        vk::ImageMemoryBarrier barrier{};
        barrier.setImage(*image);
        barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        std::array barriers{ barrier };

        auto mip_width = static_cast<int32_t>(properties.size.first);
        auto mip_height = static_cast<int32_t>(properties.size.second);

        for (uint32_t i = 1; i < properties.mip_levels; i++) {
            barriers.at(0).subresourceRange.baseMipLevel = i - 1;
            barriers.at(0).setOldLayout(vk::ImageLayout::eTransferDstOptimal);
            barriers.at(0).setNewLayout(vk::ImageLayout::eTransferSrcOptimal);
            barriers.at(0).setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
            barriers.at(0).setDstAccessMask(vk::AccessFlagBits::eTransferRead);

            commands.buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                             vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr,
                                             barriers);

            const std::array src_offsets{ vk::Offset3D{ 0, 0, 0 }, vk::Offset3D{ mip_width, mip_height, 1 } };
            const std::array dst_offsets{ vk::Offset3D{ 0, 0, 0 },
                                          vk::Offset3D{ mip_width > 1 ? mip_width / 2 : 1,
                                                        mip_height > 1 ? mip_height / 2 : 1, 1 } };

            vk::ImageBlit blit{};
            blit.setSrcOffsets(src_offsets);
            blit.setSrcSubresource({ vk::ImageAspectFlagBits::eColor, i - 1, 0, 1 });
            blit.setDstOffsets(dst_offsets);
            blit.setDstSubresource({ vk::ImageAspectFlagBits::eColor, i, 0, 1 });
            const std::array blits{ blit };

            commands.buffer->blitImage(*image, vk::ImageLayout::eTransferSrcOptimal, *image,
                                       vk::ImageLayout::eTransferDstOptimal, blits, vk::Filter::eLinear);

            barriers.at(0).setOldLayout(vk::ImageLayout::eTransferSrcOptimal);
            barriers.at(0).setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            barriers.at(0).setSrcAccessMask(vk::AccessFlagBits::eTransferRead);
            barriers.at(0).setDstAccessMask(vk::AccessFlagBits::eShaderRead);

            commands.buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                             vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr,
                                             barriers);

            if (mip_width > 1) {
                mip_width /= 2;
            }
            if (mip_height > 1) {
                mip_height /= 2;
            }
        }

        barriers.at(0).subresourceRange.baseMipLevel = properties.mip_levels - 1;
        barriers.at(0).setOldLayout(vk::ImageLayout::eTransferDstOptimal);
        barriers.at(0).setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        barriers.at(0).setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
        barriers.at(0).setDstAccessMask(vk::AccessFlagBits::eShaderRead);

        commands.buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                         vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr,
                                         barriers);
    }

    Object::Object(VulkanCore* vulkan, const vk::Queue& queue, std::string_view model_path,
                   std::string_view texture_path)
        : vulkan(vulkan), queue(queue) {
        texture = create_texture(texture_path);
        sampler = create_sampler(*texture);
        load_model(model_path);
    }

    std::unique_ptr<ImageResource> Object::create_texture(std::string_view path) const {
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load(path.data(), &width, &height, &channels, STBI_rgb_alpha);

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        size_t memory_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
        auto [staging_buffer, staging_memory] = vulkan->create_buffer(
            memory_size, vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        vulkan->copy_to_memory(*staging_memory, pixels, memory_size);
        stbi_image_free(pixels);

        std::pair<uint32_t, uint32_t> image_size{ width, height };
        ImageProperties properties{ image_size,
                                    static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1,
                                    vk::SampleCountFlagBits::e1,
                                    vk::Format::eR8G8B8A8Srgb,
                                    vk::ImageTiling::eOptimal,
                                    vk::ImageAspectFlagBits::eColor,
                                    vk::ImageUsageFlagBits::eTransferSrc |
                                        vk::ImageUsageFlagBits::eTransferDst |
                                        vk::ImageUsageFlagBits::eSampled,
                                    vk::MemoryPropertyFlagBits::eDeviceLocal };
        auto image = std::make_unique<ImageResource>(vulkan, properties);

        image->transition_layout(queue, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        image->copy_buffer(queue, *staging_buffer, image_size);
        image->generate_mipmaps(queue);

        return std::move(image);
    }

    void Object::load_model(std::string_view path) {
        tinyobj::attrib_t attrib{};
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn{};
        std::string err{};

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.data())) {
            throw std::runtime_error(warn + err);
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<Vertex, uint32_t> unique_vertices{};

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                Vertex vertex{};

                vertex.pos = { attrib.vertices[3 * index.vertex_index + 0],
                               attrib.vertices[3 * index.vertex_index + 1],
                               attrib.vertices[3 * index.vertex_index + 2] };

                vertex.tex_coord = { attrib.texcoords[2 * index.texcoord_index + 0],
                                     1.0F - attrib.texcoords[2 * index.texcoord_index + 1] };

                vertex.color = { 1.0F, 1.0F, 1.0F };

                if (!unique_vertices.contains(vertex)) {
                    unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(unique_vertices[vertex]);
            }
        }

        index_count = indices.size();
        size_t vertex_buffer_size = sizeof(Vertex) * vertices.size();
        size_t index_buffer_size = sizeof(uint32_t) * indices.size();

        auto [vertex_staging_buffer, vertex_staging_memory] = vulkan->create_buffer(
            vertex_buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        vulkan->copy_to_memory(*vertex_staging_memory, vertices.data(), vertex_buffer_size);

        std::tie(vertex_buffer, vertex_memory) = vulkan->create_buffer(
            vertex_buffer_size,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        vulkan->copy_buffer(queue, *vertex_buffer, *vertex_staging_buffer, vertex_buffer_size);

        auto [index_staging_buffer, index_staging_memory] = vulkan->create_buffer(
            index_buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        vulkan->copy_to_memory(*index_staging_memory, indices.data(), index_buffer_size);

        std::tie(index_buffer, index_memory) = vulkan->create_buffer(
            index_buffer_size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        vulkan->copy_buffer(queue, *index_buffer, *index_staging_buffer, index_buffer_size);
    }

    vk::UniqueSampler Object::create_sampler(const ImageResource& image) const {
        auto properties = vulkan->physical_device.getProperties();

        vk::SamplerCreateInfo ci{};
        ci.setMagFilter(vk::Filter::eLinear);
        ci.setMinFilter(vk::Filter::eLinear);
        ci.setAddressModeU(vk::SamplerAddressMode::eRepeat);
        ci.setAddressModeV(vk::SamplerAddressMode::eRepeat);
        ci.setAddressModeW(vk::SamplerAddressMode::eRepeat);
        ci.setAnisotropyEnable(VK_TRUE);
        ci.setMaxAnisotropy(properties.limits.maxSamplerAnisotropy);
        ci.setBorderColor(vk::BorderColor::eIntOpaqueBlack);
        ci.setUnnormalizedCoordinates(VK_FALSE);
        ci.setCompareEnable(VK_FALSE);
        ci.setCompareOp(vk::CompareOp::eAlways);
        ci.setMipmapMode(vk::SamplerMipmapMode::eLinear);
        ci.setMipLodBias(0.0F);
        ci.setMinLod(0.0F);
        ci.setMaxLod(static_cast<float>(image.properties.mip_levels));

        return vulkan->logical_device->createSamplerUnique(ci);
    }
}