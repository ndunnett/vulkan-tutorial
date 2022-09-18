#pragma once

#include "core.h"

namespace tutorial {
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

    private:
        GLFWwindow* m_window = nullptr;
        std::shared_ptr<VulkanCore> m_vulkan;
        vk::UniqueSurfaceKHR m_surface;
    };
}