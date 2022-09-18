#pragma once

#include "renderer/core.h"
#include "renderer/window.h"

namespace tutorial {
    class Renderer {
    public:
        Renderer()
            : m_glfw(std::make_shared<GlfwInstance>()), m_vulkan(std::make_shared<VulkanCore>(m_glfw)) {}

        ~Renderer() {}

        inline auto create_window(std::string_view title, std::pair<int, int> size,
                                  std::vector<std::pair<int, int>> hints) {
            return std::make_unique<Window>(m_vulkan, title, size, hints);
        }

    private:
        std::shared_ptr<GlfwInstance> m_glfw;
        std::shared_ptr<VulkanCore> m_vulkan;
    };
}