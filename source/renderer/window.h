#pragma once

#include "core.h"
#include "shaders.h"

namespace tutorial {
    class Swapchain {
    public:
        Swapchain() = default;
        Swapchain(const vk::PhysicalDevice& physical_device, const vk::Device& logical_device,
                  const vk::SurfaceKHR& surface, std::pair<uint32_t, uint32_t> size) {
            create_object(physical_device, logical_device, surface, size);
            create_image_views(logical_device);
        }

        ~Swapchain() {}

        inline void recreate(const vk::PhysicalDevice& physical_device, const vk::Device& logical_device,
                             const vk::SurfaceKHR& surface, std::pair<uint32_t, uint32_t> size) {
            logical_device.waitIdle();
            m_image_views.clear();
            create_object(physical_device, logical_device, surface, size, m_object.release());
            create_image_views(logical_device);
        }

    private:
        void set_extent(std::pair<uint32_t, uint32_t> size);
        void create_object(const vk::PhysicalDevice& physical_device, const vk::Device& logical_device,
                           const vk::SurfaceKHR& surface, std::pair<uint32_t, uint32_t> size,
                           const vk::SwapchainKHR& old_swapchain = VK_NULL_HANDLE);
        void create_image_views(const vk::Device& logical_device);

        SwapchainSupportDetails m_support{};
        vk::UniqueSwapchainKHR m_object{};
        vk::SurfaceFormatKHR m_surface_format{};
        vk::PresentModeKHR m_present_mode{};
        vk::Extent2D m_extent{};
        std::vector<vk::Image> m_images;
        std::vector<vk::UniqueImageView> m_image_views;
    };

    class GraphicsPipeline {
    public:
        GraphicsPipeline() = default;
        GraphicsPipeline(const vk::Device& logical_device) {
            fragment_shader = create_shader_module(logical_device, fragment_shader_source);
            vertex_shader = create_shader_module(logical_device, vertex_shader_source);
        }

        ~GraphicsPipeline() {}

    private:
        vk::UniqueShaderModule
        create_shader_module(const vk::Device& logical_device,
                             std::pair<shaderc_shader_kind, std::string_view> shader_source) {
            auto compiled_shader = compile_shader(shader_source);
            return logical_device.createShaderModuleUnique({ {}, compiled_shader });
        }

        vk::UniqueShaderModule fragment_shader;
        vk::UniqueShaderModule vertex_shader;
    };

    class Window {
    public:
        Window(std::shared_ptr<VulkanCore> vulkan, std::string_view title, std::pair<int, int> size,
               const std::vector<std::pair<int, int>>& hints);
        ~Window();

        std::pair<uint32_t, uint32_t> get_window_size() const;

        inline bool should_close() const {
            return glfwWindowShouldClose(m_window);
        }

        inline static void poll() {
            glfwPollEvents();
        }

        inline void recreate_swapchain() {
            m_swapchain->recreate(m_vulkan->get_physical_device(), m_vulkan->get_logical_device(), *m_surface,
                                  get_window_size());
        }

    private:
        GLFWwindow* m_window = nullptr;
        std::shared_ptr<VulkanCore> m_vulkan;
        vk::UniqueSurfaceKHR m_surface;
        vk::Queue m_graphics_queue{};
        vk::Queue m_present_queue{};
        std::unique_ptr<Swapchain> m_swapchain{};
        std::unique_ptr<GraphicsPipeline> m_pipeline{};
    };
}