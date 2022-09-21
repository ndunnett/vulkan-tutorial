#pragma once

#include "core.h"
#include "shaders.h"

namespace tutorial {
    class Window;

    class Swapchain {
    public:
        Swapchain(Window* window);
        void recreate();

    private:
        void create_object(const vk::SwapchainKHR& old_swapchain = VK_NULL_HANDLE);
        void create_image_views();

        Window* parent = nullptr;
        vk::UniqueSwapchainKHR m_object{};
        std::vector<vk::Image> m_images;
        std::vector<vk::UniqueImageView> m_image_views;
    };

    class GraphicsPipeline {
    public:
        GraphicsPipeline(Window* window)
            : parent(window), m_pipeline_layout(create_pipeline_layout()),
              m_render_pass(create_render_pass()), m_graphics_pipeline(create_graphics_pipeline()) {}
        // ~GraphicsPipeline(){}

    private:
        vk::UniqueShaderModule
        create_shader_module(std::pair<shaderc_shader_kind, std::string_view> shader_source);
        vk::UniqueRenderPass create_render_pass();
        vk::UniquePipelineLayout create_pipeline_layout();
        vk::UniquePipeline create_graphics_pipeline();

        Window* parent = nullptr;
        vk::UniqueRenderPass m_render_pass;
        vk::UniquePipelineLayout m_pipeline_layout;
        vk::UniquePipeline m_graphics_pipeline;
    };

    class Window {
    public:
        Window(VulkanCore* vulkan, std::string_view title, std::pair<int, int> size,
               const std::vector<std::pair<int, int>>& hints);
        ~Window();

        void set_extent(std::pair<uint32_t, uint32_t> size);

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

        inline bool should_close() const {
            return glfwWindowShouldClose(m_window);
        }

        inline static void poll() {
            glfwPollEvents();
        }

        inline void recreate_swapchain() {
            set_extent(get_window_size<uint32_t>());
            m_swapchain->recreate();
        }

        friend Swapchain;
        friend GraphicsPipeline;

    private:
        VulkanCore* vulkan;
        GLFWwindow* m_window = nullptr;
        vk::UniqueSurfaceKHR m_surface;
        SwapchainSupportDetails m_support{};
        vk::SampleCountFlagBits m_msaa_samples = vk::SampleCountFlagBits::e1;
        vk::Queue m_graphics_queue{};
        vk::Queue m_present_queue{};
        vk::SurfaceFormatKHR m_surface_format{};
        vk::PresentModeKHR m_present_mode{};
        vk::Extent2D m_extent{};
        std::unique_ptr<Swapchain> m_swapchain{};
        std::unique_ptr<GraphicsPipeline> m_pipeline{};
    };
}