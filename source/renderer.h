#pragma once

#include "renderer/core.h"
#include "renderer/window.h"

namespace tutorial {
    class Renderer {
    public:
        Renderer() : glfw(std::make_unique<GlfwInstance>()), vulkan(std::make_unique<VulkanCore>()) {}

        ~Renderer() {}

        inline void wait_idle() {
            vulkan->get_logical_device().waitIdle();
        }

        inline auto create_window(std::string_view title, std::pair<int, int> size,
                                  std::vector<std::pair<int, int>> hints) {
            return std::make_unique<Window>(vulkan.get(), title, size, hints);
        }

    private:
        std::unique_ptr<GlfwInstance> glfw;
        std::unique_ptr<VulkanCore> vulkan;
    };
}