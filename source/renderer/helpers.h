#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <algorithm>
#include <iostream>
#include <memory>
#include <ranges>
#include <set>
#include <stdexcept>
#include <tuple>

namespace tutorial {
#ifdef NDEBUG
    constexpr bool enable_validation_layers = false;
#else
    constexpr bool enable_validation_layers = true;
#endif

#ifdef __APPLE__
    constexpr bool using_molten_vk = true;
#else
    constexpr bool using_molten_vk = false;
#endif

    constexpr std::array validation_layers{ "VK_LAYER_KHRONOS_validation" };
    constexpr std::array required_device_extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    constexpr std::array instance_portability_extensions{
        std::string_view("VK_KHR_get_physical_device_properties2"),
    };
    constexpr std::array device_portability_extensions{
        std::string_view("VK_KHR_portability_subset"),
    };

    struct GlfwInstance {
        GlfwInstance() {
            if (glfwInit() != GLFW_TRUE) {
                throw std::runtime_error("GLFW failed to initialise!");
            }
        }

        ~GlfwInstance() {
            glfwTerminate();
        }
    };

    struct QueueFamilyIndices {
        QueueFamilyIndices() = default;
        QueueFamilyIndices(const vk::PhysicalDevice& physical_device, const vk::SurfaceKHR& surface) {
            auto queue_families = physical_device.getQueueFamilyProperties();

            for (int i = 0; i < queue_families.size(); i++) {
                if (queue_families.at(i).queueFlags & vk::QueueFlagBits::eGraphics) {
                    graphics = i;
                }

                if (physical_device.getSurfaceSupportKHR(i, surface)) {
                    present = i;
                }

                if (is_complete()) {
                    break;
                }
            }
        }

        inline bool is_complete() const {
            return graphics.has_value() && present.has_value();
        }

        inline auto set() const {
            return std::set<uint32_t>{ graphics.value(), present.value() };
        }

        inline auto values() const {
            return std::array{ graphics.value(), present.value() };
        }

        std::optional<uint32_t> graphics{};
        std::optional<uint32_t> present{};
    };

    struct SwapchainSupportDetails {
        SwapchainSupportDetails() = default;
        SwapchainSupportDetails(const vk::PhysicalDevice& physical_device, const vk::SurfaceKHR& surface)
            : capabilities(physical_device.getSurfaceCapabilitiesKHR(surface)),
              formats(physical_device.getSurfaceFormatsKHR(surface)),
              present_modes(physical_device.getSurfacePresentModesKHR(surface)) {}

        inline bool is_empty() const {
            return formats.empty() | present_modes.empty();
        }

        vk::SurfaceCapabilitiesKHR capabilities{};
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> present_modes;
    };

    struct SingleTimeCommands {
    public:
        SingleTimeCommands(const vk::Device& device, const vk::CommandPool& command_pool,
                           const vk::Queue& queue)
            : m_queue(queue) {
            vk::CommandBufferAllocateInfo ai{};
            ai.setLevel(vk::CommandBufferLevel::ePrimary);
            ai.setCommandPool(command_pool);
            ai.setCommandBufferCount(1);
            m_command_buffer = std::move(device.allocateCommandBuffersUnique(ai).at(0));
            m_command_buffer->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        }

        ~SingleTimeCommands() {
            m_command_buffer->end();
            vk::SubmitInfo si{};
            si.setCommandBuffers(*m_command_buffer);
            m_queue.submit(si);
            m_queue.waitIdle();
        }

        inline const vk::CommandBuffer& get_buffer() {
            return *m_command_buffer;
        }

    private:
        const vk::Queue& m_queue;
        vk::UniqueCommandBuffer m_command_buffer;
    };

    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    struct Vertex {
        glm::vec2 pos;
        glm::vec3 color;
        // glm::vec2 tex_coord;

        bool operator==(const Vertex& other) const {
            return pos == other.pos && color == other.color; // && tex_coord == other.tex_coord;
        }

        static auto get_binding_description() {
            vk::VertexInputBindingDescription binding_description{};
            binding_description.binding = 0;
            binding_description.stride = sizeof(Vertex);
            binding_description.inputRate = vk::VertexInputRate::eVertex;
            return binding_description;
        }

        static auto get_attribute_descriptions() {
            vk::VertexInputAttributeDescription pos_description{};
            pos_description.binding = 0;
            pos_description.location = 0;
            pos_description.format = vk::Format::eR32G32B32Sfloat;
            pos_description.offset = offsetof(Vertex, pos);

            vk::VertexInputAttributeDescription color_description{};
            color_description.binding = 0;
            color_description.location = 1;
            color_description.format = vk::Format::eR32G32B32Sfloat;
            color_description.offset = offsetof(Vertex, color);

            // vk::VertexInputAttributeDescription tex_coord_description{};
            // tex_coord_description.binding = 0;
            // tex_coord_description.location = 2;
            // tex_coord_description.format = vk::Format::eR32G32Sfloat;
            // tex_coord_description.offset = offsetof(Vertex, tex_coord);

            // return std::array{ pos_description, color_description, tex_coord_description };
            return std::array{ pos_description, color_description };
        }
    };
}

namespace std {
    template<>
    struct hash<tutorial::Vertex> {
        size_t operator()(tutorial::Vertex const& vertex) const {
            return ((hash<glm::vec2>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1);
        }
        // size_t operator()(tutorial::Vertex const& vertex) const {
        //     return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
        //            (hash<glm::vec2>()(vertex.tex_coord) << 1);
        // }
    };
}