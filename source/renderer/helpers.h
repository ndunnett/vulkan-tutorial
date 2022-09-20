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
        GlfwInstance();
        ~GlfwInstance();
        std::vector<const char*> required_extensions() const;
    };

    struct QueueFamilyIndices {
        QueueFamilyIndices() = default;
        QueueFamilyIndices(const vk::PhysicalDevice& physical_device, const vk::SurfaceKHR& surface);

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
}