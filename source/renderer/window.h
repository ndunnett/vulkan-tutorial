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
        void set_extent(std::pair<uint32_t, uint32_t> size) {
            m_extent.width = std::clamp(size.first, m_support.capabilities.minImageExtent.width,
                                        m_support.capabilities.maxImageExtent.width);
            m_extent.height = std::clamp(size.second, m_support.capabilities.minImageExtent.height,
                                         m_support.capabilities.maxImageExtent.height);
        }

        void create_object(const vk::PhysicalDevice& physical_device, const vk::Device& logical_device,
                           const vk::SurfaceKHR& surface, std::pair<uint32_t, uint32_t> size,
                           const vk::SwapchainKHR& old_swapchain = VK_NULL_HANDLE) {
            m_support = { physical_device, surface };

            for (auto format : m_support.formats) {
                if (format.format == vk::Format::eB8G8R8A8Srgb &&
                    format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                    m_surface_format = format;
                }
            }

            for (auto mode : m_support.present_modes) {
                if (mode == vk::PresentModeKHR::eMailbox) {
                    m_present_mode = mode;
                }
            }

            if (m_support.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
                m_extent = m_support.capabilities.currentExtent;
            } else {
                set_extent(size);
            }

            uint32_t image_count = m_support.capabilities.minImageCount + 1;
            if (m_support.capabilities.maxImageCount > 0 &&
                image_count > m_support.capabilities.maxImageCount) {
                image_count = m_support.capabilities.maxImageCount;
            }

            vk::SwapchainCreateInfoKHR ci{};
            ci.setSurface(surface);
            ci.setMinImageCount(image_count);
            ci.setImageFormat(m_surface_format.format);
            ci.setImageColorSpace(m_surface_format.colorSpace);
            ci.setImageExtent(m_extent);
            ci.setImageArrayLayers(1);
            ci.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

            auto indices = QueueFamilyIndices(physical_device, surface);
            auto index_values = indices.values();
            if (indices.set().size() > 1) {
                ci.setImageSharingMode(vk::SharingMode::eConcurrent);
                ci.setQueueFamilyIndices(index_values);
            } else {
                ci.setImageSharingMode(vk::SharingMode::eExclusive);
            }

            ci.setPreTransform(m_support.capabilities.currentTransform);
            ci.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
            ci.setPresentMode(m_present_mode);
            ci.setClipped(VK_TRUE);
            ci.setOldSwapchain(old_swapchain);

            m_object = logical_device.createSwapchainKHRUnique(ci);
        }

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

        inline bool should_close() const {
            return glfwWindowShouldClose(m_window);
        }

        inline static void poll() {
            glfwPollEvents();
        }

        inline std::pair<uint32_t, uint32_t> get_window_size() const {
            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(m_window, &width, &height);
            return { width, height };
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