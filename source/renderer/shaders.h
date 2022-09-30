#pragma once

#include <shaderc/shaderc.hpp>

namespace tutorial {
    typedef std::pair<shaderc_shader_kind, std::string_view> ShaderSource;

    constexpr ShaderSource fragment_shader_source{ shaderc_glsl_fragment_shader, R"""(
            #version 450

            layout(location = 0) in vec3 fragColor;

            layout(location = 0) out vec4 outColor;

            void main() {
                outColor = vec4(fragColor, 1.0);
            }
        )""" };

    constexpr ShaderSource vertex_shader_source{ shaderc_glsl_vertex_shader, R"""(
            #version 450

            layout(binding = 0) uniform UniformBufferObject {
                mat4 model;
                mat4 view;
                mat4 proj;
            } ubo;

            layout(location = 0) in vec2 inPosition;
            layout(location = 1) in vec3 inColor;

            layout(location = 0) out vec3 fragColor;

            void main() {
                gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0);
                fragColor = inColor;
            }
        )""" };

    inline const std::vector<uint32_t> compile_shader(const ShaderSource& source) {
        static shaderc::Compiler compiler{};
        std::string name{};
        auto module = compiler.CompileGlslToSpv(source.second.data(), source.first, name.data());

        if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
            std::cerr << "[shaderc] " << module.GetErrorMessage() << std::endl;
            throw std::runtime_error("failed to compile shader!");
        }

        return { module.begin(), module.end() };
    }
}