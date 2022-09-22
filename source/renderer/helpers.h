#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

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
}