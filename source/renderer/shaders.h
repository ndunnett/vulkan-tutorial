#pragma once

#include <shaderc/shaderc.hpp>

namespace tutorial {
    constexpr std::pair<shaderc_shader_kind, std::string_view> fragment_shader_source{
        shaderc_glsl_fragment_shader,
        R"""(
            #version 450

            layout(location = 0) in vec3 fragColor;

            layout(location = 0) out vec4 outColor;

            void main() {
                outColor = vec4(fragColor, 1.0);
            }
        )"""
    };

    constexpr std::pair<shaderc_shader_kind, std::string_view> vertex_shader_source{
        shaderc_glsl_vertex_shader,
        R"""(
            #version 450

            layout(location = 0) out vec3 fragColor;

            vec2 positions[3] = vec2[](
                vec2(0.0, -0.5),
                vec2(0.5, 0.5),
                vec2(-0.5, 0.5)
            );

            vec3 colors[3] = vec3[](
                vec3(1.0, 0.0, 0.0),
                vec3(0.0, 1.0, 0.0),
                vec3(0.0, 0.0, 1.0)
            );

            void main() {
                gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
                fragColor = colors[gl_VertexIndex];
            }
        )"""
    };

    inline const std::vector<uint32_t>
    compile_shader(std::pair<shaderc_shader_kind, std::string_view> shader_source) {
        static shaderc::Compiler compiler{};
        auto module = compiler.CompileGlslToSpv(shader_source.second.data(), shader_source.first, nullptr);

        if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
            std::cerr << module.GetErrorMessage();
            throw std::runtime_error("failed to compile shader");
        }

        return { module.begin(), module.end() };
    }
}