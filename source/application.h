#pragma once

#include "renderer.h"

namespace tutorial {
    constexpr std::string_view window_title{ "Vulkan" };
    constexpr std::pair<int, int> window_size{ 800, 600 };

    class Application {
    public:
        Application()
            : renderer(std::make_unique<Renderer>()),
              window(renderer->create_window(window_title, window_size)) {
            const std::vector<Vertex> vertices{ { { -0.5F, -0.5F }, { 1.0F, 0.0F, 0.0F } },
                                                { { 0.5F, -0.5F }, { 0.0F, 1.0F, 0.0F } },
                                                { { 0.5F, 0.5F }, { 0.0F, 0.0F, 1.0F } },
                                                { { -0.5F, 0.5F }, { 1.0F, 1.0F, 1.0F } } };
            
            const std::vector<uint16_t> indices{ 0, 1, 2, 2, 3, 0 };

            window->add_object(vertices, indices);
        }

        ~Application() {}

        void run() {
            while (!window->should_close()) {
                window->poll();
                window->draw_frame();
            }

            renderer->wait_idle();
        }

    private:
        std::unique_ptr<Renderer> renderer;
        std::unique_ptr<Window> window;
    };
}