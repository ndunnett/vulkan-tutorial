#include "window.h"

namespace tutorial {
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
        m_surface = vk::UniqueSurfaceKHR(raw_surface, m_vulkan->get_deleter());
    }

    Window::~Window() {
        glfwDestroyWindow(m_window);
    }
}