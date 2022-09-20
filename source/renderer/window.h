#pragma once

#include "core.h"

namespace tutorial {
    class Swapchain {
    public:
        Swapchain(const vk::PhysicalDevice& physical_device, const vk::Device& logical_device,
                  const vk::SurfaceKHR& surface, std::pair<uint32_t, uint32_t> size) {
            create_object(physical_device, logical_device, surface, size);
            create_resources();
        }

        ~Swapchain() {
            cleanup_resources();
        }

        inline void recreate(const vk::PhysicalDevice& physical_device, const vk::Device& logical_device,
                             const vk::SurfaceKHR& surface, std::pair<uint32_t, uint32_t> size) {
            cleanup_resources();
            create_object(physical_device, logical_device, surface, size, m_object.release());
            create_resources();
        }

    private:
        void set_extent(std::pair<uint32_t, uint32_t> size);
        void create_object(const vk::PhysicalDevice& physical_device, const vk::Device& logical_device,
                           const vk::SurfaceKHR& surface, std::pair<uint32_t, uint32_t> size,
                           const vk::SwapchainKHR& old_swapchain = VK_NULL_HANDLE);

        void create_resources() {
            // m_images
            // m_image_views
            // m_framebuffers
        }

        void cleanup_resources() {
            // m_images
            // m_image_views
            // m_framebuffers
        }

        SwapchainSupportDetails m_support{};
        vk::UniqueSwapchainKHR m_object{};
        vk::SurfaceFormatKHR m_surface_format{};
        vk::PresentModeKHR m_present_mode{};
        vk::Extent2D m_extent{};
        std::vector<vk::Image> m_images;
        std::vector<vk::ImageView> m_image_views;
        std::vector<vk::Framebuffer> m_framebuffers;
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
    };
}