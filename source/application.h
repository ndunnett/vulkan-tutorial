#pragma once

#include "render.h"

namespace tutorial {
    constexpr std::string_view window_title{ "Vulkan" };
    constexpr std::pair<int, int> window_size{ 800, 600 };
    constexpr std::pair<int, int> window_not_resizable{ GLFW_RESIZABLE, GLFW_FALSE };

    class Application {
    public:
        Application()
            : render(std::make_unique<RenderTarget>(window_title, window_size,
                                                    std::vector{ window_not_resizable })) {}

        ~Application() {}

        void run() {
            while (!render->window->should_close()) {
                render->window->poll_events();
            }
        }

    private:
        std::unique_ptr<RenderTarget> render;
    };
}