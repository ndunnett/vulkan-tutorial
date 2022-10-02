#pragma once

#include "core.h"
#include "shaders.h"

namespace tutorial {
    struct UboResource {
        UboResource(VulkanCore* vulkan);
        void update(const vk::Extent2D& extent);

        VulkanCore* vulkan = nullptr;
        vk::UniqueBuffer buffer{};
        vk::UniqueDeviceMemory memory{};
    };

    struct FrameTransient {
        FrameTransient(VulkanCore* vulkan, vk::UniqueCommandBuffer& command_buffer)
            : command_buffer(std::move(command_buffer)),
              image_available(vulkan->logical_device->createSemaphoreUnique({})),
              render_finished(vulkan->logical_device->createSemaphoreUnique({})),
              in_flight(vulkan->logical_device->createFenceUnique({ vk::FenceCreateFlagBits::eSignaled })),
              ubo(vulkan) {}

        vk::UniqueCommandBuffer command_buffer;
        vk::UniqueSemaphore image_available;
        vk::UniqueSemaphore render_finished;
        vk::UniqueFence in_flight;
        UboResource ubo;
    };

    struct FrameTransients {
        FrameTransients(VulkanCore* vulkan, size_t frames_in_flight = 3);

        void wait_for_fences();
        void reset_fences();
        vk::ResultValue<uint32_t> next_image_index(const vk::SwapchainKHR& swapchain);
        void reset_command_buffer();
        void submit(const vk::Queue& queue, vk::PipelineStageFlags dst_stage_mask);
        vk::Result present(const vk::Queue& queue, const vk::SwapchainKHR& swapchain, uint32_t index);

        inline void next_frame() {
            m_frame_index = (m_frame_index + 1) % m_frames.size();
        }

        inline FrameTransient& current() {
            return m_frames.at(m_frame_index);
        }

        inline size_t size() const {
            return m_frames.size();
        }

        inline const vk::Buffer& get_ubo_buffer(size_t index) {
            return *m_frames.at(index).ubo.buffer;
        }

        inline size_t current_index() const {
            return m_frame_index;
        }

    private:
        VulkanCore* vulkan = nullptr;
        std::vector<FrameTransient> m_frames;
        size_t m_frame_index = 0;
    };

    struct ImageResource {
        ImageResource(VulkanCore* vulkan, const ImageProperties& properties);
        void transition_layout(const vk::Queue& queue, vk::ImageLayout old_layout,
                               vk::ImageLayout new_layout);

        VulkanCore* vulkan = nullptr;
        ImageProperties properties;
        vk::UniqueImage image;
        vk::UniqueDeviceMemory memory;
        vk::UniqueImageView view;
    };

    struct Object {
        Object(VulkanCore* vulkan, const vk::Queue& queue, const std::vector<Vertex>& vertices,
               const std::vector<uint16_t>& indices);

        VulkanCore* vulkan = nullptr;
        vk::UniqueBuffer vertex_buffer;
        vk::UniqueDeviceMemory vertex_memory;
        vk::UniqueBuffer index_buffer;
        vk::UniqueDeviceMemory index_memory;
        uint32_t index_count = 0;
    };
}