#pragma once

#include "resources.h"

namespace tutorial {
    class Window {
    public:
        Window(VulkanCore* vulkan, std::string_view title, std::pair<int, int> size,
               const std::vector<std::pair<int, int>>& hints);
        ~Window();

        void rebuild_swapchain();
        void draw_frame();

        template<class T>
        std::pair<T, T> get_size() const {
            return { static_cast<T>(m_extent.width), static_cast<T>(m_extent.height) };
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
        vk::UniqueDescriptorSetLayout create_descriptor_set_layout() const;
        vk::UniquePipelineLayout create_pipeline_layout() const;
        vk::UniquePipeline create_graphics_pipeline() const;
        vk::UniqueDescriptorPool create_descriptor_pool() const;
        std::vector<vk::UniqueDescriptorSet> create_descriptor_sets() const;
        vk::UniqueSwapchainKHR create_swapchain(const vk::SwapchainKHR& old_swapchain = VK_NULL_HANDLE) const;
        std::vector<vk::UniqueImageView> create_swapchain_views() const;
        // std::unique_ptr<ImageResource> create_color_image() const;
        std::unique_ptr<ImageResource> create_depth_image() const;
        std::vector<vk::UniqueFramebuffer> create_framebuffers() const;

        void update_size();
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
        vk::UniqueDescriptorSetLayout m_descriptor_set_layout{};
        vk::UniquePipelineLayout m_pipeline_layout{};
        vk::UniquePipeline m_graphics_pipeline{};
        // std::unique_ptr<ImageResource> m_color_image;
        std::unique_ptr<ImageResource> m_depth_image;
        std::unique_ptr<FrameTransients> m_frames;
        vk::UniqueDescriptorPool m_descriptor_pool{};
        std::vector<vk::UniqueDescriptorSet> m_descriptor_sets;
        bool m_framebuffer_resized = false;

        std::unique_ptr<Object> m_object = nullptr;
    };
}