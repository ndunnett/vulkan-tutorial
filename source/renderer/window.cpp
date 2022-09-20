#include "window.h"

namespace tutorial {
    void Swapchain::set_extent(std::pair<uint32_t, uint32_t> size) {
        m_extent.width = std::clamp(size.first, m_support.capabilities.minImageExtent.width,
                                    m_support.capabilities.maxImageExtent.width);
        m_extent.height = std::clamp(size.second, m_support.capabilities.minImageExtent.height,
                                     m_support.capabilities.maxImageExtent.height);
    }

    void Swapchain::create_object(const vk::PhysicalDevice& physical_device, const vk::Device& logical_device,
                                  const vk::SurfaceKHR& surface, std::pair<uint32_t, uint32_t> size,
                                  const vk::SwapchainKHR& old_swapchain) {
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
        if (m_support.capabilities.maxImageCount > 0 && image_count > m_support.capabilities.maxImageCount) {
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

    void Swapchain::create_image_views(const vk::Device& logical_device) {
        m_images = logical_device.getSwapchainImagesKHR(*m_object);

        for (auto image : m_images) {
            vk::ImageViewCreateInfo ci{};
            ci.setImage(image);
            ci.setViewType(vk::ImageViewType::e2D);
            ci.setFormat(m_surface_format.format);
            ci.setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
            m_image_views.emplace_back(logical_device.createImageViewUnique(ci));
        }
    }

    Window::Window(std::shared_ptr<VulkanCore> vulkan, std::string_view title, std::pair<int, int> size,
                   const std::vector<std::pair<int, int>>& hints)
        : m_vulkan(vulkan) {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        for (auto hint : hints) {
            glfwWindowHint(hint.first, hint.second);
        }

        m_window = glfwCreateWindow(size.first, size.second, title.data(), nullptr, nullptr);

        if (!m_window) {
            throw std::runtime_error("failed to create GLFW window!");
        }

        VkSurfaceKHR raw_surface{};
        if (glfwCreateWindowSurface(m_vulkan->get_instance(), m_window, nullptr, &raw_surface) !=
            VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }

        m_vulkan->initialise_devices(raw_surface);
        std::tie(m_graphics_queue, m_present_queue) = m_vulkan->get_queues();
        m_surface = vk::UniqueSurfaceKHR(raw_surface, m_vulkan->get_deleter());
        m_swapchain = std::make_unique<Swapchain>(m_vulkan->get_physical_device(),
                                                  m_vulkan->get_logical_device(), *m_surface, size);
        m_pipeline = std::make_unique<GraphicsPipeline>();
    }

    Window::~Window() {
        glfwDestroyWindow(m_window);
    }

    std::pair<uint32_t, uint32_t> Window::get_window_size() const {
        int width = 0;
        int height = 0;

        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(m_window, &width, &height);
            glfwWaitEvents();
        }

        return { width, height };
    }
}