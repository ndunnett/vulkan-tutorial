#include "resources.h"

namespace tutorial {
    UboResource::UboResource(VulkanCore* vulkan) : vulkan(vulkan) {
        std::tie(buffer, memory) = vulkan->create_buffer(
            sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    }

    void UboResource::update(const vk::Extent2D& extent) {
        static auto start_time = std::chrono::high_resolution_clock::now();
        auto current_time = std::chrono::high_resolution_clock::now();
        float time =
            std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0F), time * glm::radians(90.0F), glm::vec3(0.0F, 0.0F, 1.0F));
        ubo.view = glm::lookAt(glm::vec3(2.0F, 2.0F, 2.0F), glm::vec3(0.0F, 0.0F, 0.0F),
                               glm::vec3(0.0F, 0.0F, 1.0F));
        ubo.proj = glm::perspective(glm::radians(45.0F),
                                    static_cast<float>(extent.width) / static_cast<float>(extent.height),
                                    0.1F, 10.0F);
        ubo.proj[1][1] *= -1;

        vulkan->copy_to_memory(*memory, &ubo, sizeof(UniformBufferObject));
    }

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

    Object::Object(VulkanCore* vulkan, const vk::Queue& queue, const std::vector<Vertex>& vertices,
                   const std::vector<uint16_t>& indices)
        : vulkan(vulkan), index_count(indices.size()) {
        size_t vertex_buffer_size = sizeof(Vertex) * vertices.size();

        auto [vertex_staging_buffer, vertex_staging_memory] = vulkan->create_buffer(
            vertex_buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        vulkan->copy_to_memory(*vertex_staging_memory, vertices.data(), vertex_buffer_size);

        std::tie(vertex_buffer, vertex_memory) = vulkan->create_buffer(
            vertex_buffer_size,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        vulkan->copy_buffer(queue, *vertex_buffer, *vertex_staging_buffer, vertex_buffer_size);

        size_t index_buffer_size = sizeof(uint16_t) * indices.size();

        auto [index_staging_buffer, index_staging_memory] = vulkan->create_buffer(
            index_buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        vulkan->copy_to_memory(*index_staging_memory, indices.data(), index_buffer_size);

        std::tie(index_buffer, index_memory) = vulkan->create_buffer(
            index_buffer_size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        vulkan->copy_buffer(queue, *index_buffer, *index_staging_buffer, index_buffer_size);
    }
}