#pragma once

#include "renderer.h"

namespace tutorial {
    constexpr std::string_view window_title{ "Vulkan" };
    constexpr std::pair<int, int> window_size{ 800, 600 };
    constexpr std::pair<int, int> window_not_resizable{ GLFW_RESIZABLE, GLFW_FALSE };
    const std::vector window_flags{ window_not_resizable };

    class Application {
    public:
        Application()
            : renderer(std::make_unique<Renderer>()),
              window(renderer->create_window(window_title, window_size, window_flags)) {}

        ~Application() {}

        void run() {
            while (!window->should_close()) {
                window->poll();
            }
        }

    private:
        std::unique_ptr<Renderer> renderer;
        std::unique_ptr<Window> window;
    };
}