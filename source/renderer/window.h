#pragma once

#include "core.h"
#include "shaders.h"

namespace tutorial {
    constexpr size_t MAX_FRAMES_IN_FLIGHT = 3;

    struct FrameTransient {
        vk::UniqueCommandBuffer command_buffer{};
        vk::UniqueSemaphore image_available{};
        vk::UniqueSemaphore render_finished{};
        vk::UniqueFence in_flight{};
    };

    struct FrameTransients {
        FrameTransients(VulkanCore* vulkan, size_t frames_in_flight);

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

    private:
        VulkanCore* vulkan = nullptr;
        std::vector<FrameTransient> m_frames;
        size_t m_frame_index = 0;
    };

    struct ImageProperties {
        ImageProperties() = default;
        ImageProperties(std::pair<uint32_t, uint32_t> size, uint32_t mip_levels,
                        vk::SampleCountFlagBits samples, vk::Format format, vk::ImageTiling tiling,
                        vk::ImageAspectFlags aspect_flags, vk::ImageUsageFlags usage,
                        vk::MemoryPropertyFlags memory)
            : size(size), mip_levels(mip_levels), samples(samples), format(format), tiling(tiling),
              aspect_flags(aspect_flags), usage(usage), memory(memory) {}

        std::pair<uint32_t, uint32_t> size;
        uint32_t mip_levels;
        vk::SampleCountFlagBits samples;
        vk::Format format;
        vk::ImageTiling tiling;
        vk::ImageAspectFlags aspect_flags;
        vk::ImageUsageFlags usage;
        vk::MemoryPropertyFlags memory;
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
               const std::vector<uint16_t>& indices)
            : vulkan(vulkan), index_count(indices.size()) {
            vk::DeviceSize vertex_buffer_size = sizeof(Vertex) * vertices.size();

            auto [vertex_staging_buffer, vertex_staging_memory] = vulkan->create_buffer(
                vertex_buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            vulkan->copy_to_memory(*vertex_staging_memory, vertices.data(), vertex_buffer_size);

            std::tie(vertex_buffer, vertex_memory) = vulkan->create_buffer(
                vertex_buffer_size,
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                vk::MemoryPropertyFlagBits::eDeviceLocal);
            vulkan->copy_buffer(queue, *vertex_buffer, *vertex_staging_buffer, vertex_buffer_size);

            vk::DeviceSize index_buffer_size = sizeof(uint16_t) * indices.size();

            auto [index_staging_buffer, index_staging_memory] = vulkan->create_buffer(
                index_buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            vulkan->copy_to_memory(*index_staging_memory, indices.data(), index_buffer_size);

            std::tie(index_buffer, index_memory) = vulkan->create_buffer(
                index_buffer_size,
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                vk::MemoryPropertyFlagBits::eDeviceLocal);
            vulkan->copy_buffer(queue, *index_buffer, *index_staging_buffer, index_buffer_size);
        }

        VulkanCore* vulkan = nullptr;
        vk::UniqueBuffer vertex_buffer;
        vk::UniqueDeviceMemory vertex_memory;
        vk::UniqueBuffer index_buffer;
        vk::UniqueDeviceMemory index_memory;
        uint32_t index_count = 0;
    };

    class Window {
    public:
        Window(VulkanCore* vulkan, std::string_view title, std::pair<int, int> size,
               const std::vector<std::pair<int, int>>& hints);
        ~Window();

        void set_extent(std::pair<uint32_t, uint32_t> size);
        void rebuild_swapchain();
        void draw_frame();

        template<class T>
        std::pair<T, T> get_window_size() const {
            int width = 0;
            int height = 0;

            while (width == 0 || height == 0) {
                glfwGetFramebufferSize(m_window, &width, &height);
                glfwWaitEvents();
            }

            return { static_cast<T>(width), static_cast<T>(height) };
        }

        inline void add_object(const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices) {
            m_objects.emplace_back(vulkan, m_graphics_queue, vertices, indices);
        }

        inline bool should_close() const {
            return glfwWindowShouldClose(m_window);
        }

        inline static void poll() {
            glfwPollEvents();
        }

        inline void set_framebuffer_resized() {
            m_framebuffer_resized = true;
        }

        static void framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
            auto parent = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
            parent->set_framebuffer_resized();
        }

    private:
        vk::UniqueSurfaceKHR create_surface() const;
        vk::UniqueShaderModule create_shader_module(const ShaderSource& shader_source) const;
        vk::UniqueRenderPass create_render_pass() const;
        vk::UniquePipelineLayout create_pipeline_layout() const;
        vk::UniquePipeline create_graphics_pipeline() const;
        vk::UniqueSwapchainKHR create_swapchain(const vk::SwapchainKHR& old_swapchain = VK_NULL_HANDLE) const;
        std::vector<vk::UniqueImageView> create_swapchain_views() const;
        std::unique_ptr<ImageResource> create_color_image() const;
        std::unique_ptr<ImageResource> create_depth_image() const;
        std::vector<vk::UniqueFramebuffer> create_framebuffers() const;

        void get_swapchain_details();
        void record_command_buffer(uint32_t index);

        VulkanCore* vulkan = nullptr;
        GLFWwindow* m_window = nullptr;
        vk::UniqueSurfaceKHR m_surface{};
        SwapchainSupportDetails m_support{};
        vk::SampleCountFlagBits m_msaa_samples = vk::SampleCountFlagBits::e1;
        vk::Queue m_graphics_queue{};
        vk::Queue m_present_queue{};
        vk::SurfaceFormatKHR m_surface_format{};
        vk::PresentModeKHR m_present_mode{};
        vk::Extent2D m_extent{};
        vk::UniqueSwapchainKHR m_swapchain{};
        std::vector<vk::Image> m_swapchain_images;
        std::vector<vk::UniqueImageView> m_swapchain_views;
        vk::UniqueRenderPass m_render_pass{};
        std::vector<vk::UniqueFramebuffer> m_framebuffers;
        vk::UniquePipelineLayout m_pipeline_layout{};
        vk::UniquePipeline m_graphics_pipeline{};
        std::unique_ptr<ImageResource> m_color_image;
        std::unique_ptr<ImageResource> m_depth_image;
        std::unique_ptr<FrameTransients> m_frames;
        bool m_framebuffer_resized = false;

        std::vector<Object> m_objects;
    };
}