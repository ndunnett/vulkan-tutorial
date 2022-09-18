#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>

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

namespace tutorial {
    constexpr std::array validation_layers{ "VK_LAYER_KHRONOS_validation" };
    constexpr std::array required_device_extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    constexpr std::array instance_portability_extensions{
        std::string_view("VK_KHR_get_physical_device_properties2"),
    };
    constexpr std::array device_portability_extensions{
        std::string_view("VK_KHR_portability_subset"),
    };

    static int glfw_windows = 0;
    static bool glfw_initialised = false;

    struct QueueFamilyIndices {
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

        std::optional<uint32_t> graphics{};
        std::optional<uint32_t> present{};

        bool is_complete() {
            return graphics.has_value() && present.has_value();
        }

        auto set() {
            return std::set<uint32_t>{ graphics.value(), present.value() };
        };
    };

    struct SwapChainSupportDetails {
        SwapChainSupportDetails(const vk::PhysicalDevice& physical_device, const vk::SurfaceKHR& surface)
            : capabilities(physical_device.getSurfaceCapabilitiesKHR(surface)),
              formats(physical_device.getSurfaceFormatsKHR(surface)),
              present_modes(physical_device.getSurfacePresentModesKHR(surface)) {}

        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> present_modes;

        inline bool is_empty() const {
            return formats.empty() | present_modes.empty();
        }
    };

    class Renderer {
    public:
        Renderer(std::string_view name = "Vulkan")
            : m_instance(create_instance(name)), m_physical_device(pick_physical_device()) {}

        ~Renderer() {}

        const vk::Instance& get_instance() {
            return *m_instance;
        }

        const vk::PhysicalDevice& get_physical_device() {
            return m_physical_device;
        }

        vk::UniqueInstance create_instance(std::string_view name) {
            vk::ApplicationInfo app_info{};
            app_info.setPApplicationName(name.data());
            app_info.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0));
            app_info.setEngineVersion(VK_MAKE_VERSION(1, 0, 0));
            app_info.setApiVersion(VK_API_VERSION_1_0);

            vk::InstanceCreateInfo instance_ci{};
            instance_ci.setPApplicationInfo(&app_info);

            if (!glfw_initialised && glfwInit() == GLFW_TRUE) {
                glfw_initialised = true;
            }

            uint32_t glfw_extensions_count = 0;
            const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extensions_count);

            std::vector<const char*> extensions{};

            for (size_t i = 0; i < glfw_extensions_count; i++) {
                extensions.emplace_back(glfw_extensions[i]);
            }

            for (auto available_extension : vk::enumerateInstanceExtensionProperties()) {
                for (auto portability_extension : instance_portability_extensions) {
                    if (std::string_view(available_extension.extensionName) == portability_extension) {
                        extensions.emplace_back(portability_extension.data());
                    }
                }
            }

            if (using_molten_vk) {
                extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                instance_ci.flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
            }

            if (enable_validation_layers) {
                auto layer_is_available = [](std::string_view layer_name) {
                    static auto available_layers = vk::enumerateInstanceLayerProperties();
                    for (auto available_layer : available_layers) {
                        if (layer_name == std::string_view{ available_layer.layerName }) {
                            return true;
                        }
                    }
                    return false;
                };

                for (auto validation_layer : validation_layers) {
                    if (!layer_is_available(validation_layer)) {
                        throw std::runtime_error("validation layers requested, but not available!");
                    }
                }

                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

                auto severity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;

                auto message_type = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;

                auto callback =
                    [](VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
                       const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
                        std::cerr << callback_data->pMessage << std::endl;
                        return VK_FALSE;
                    };

                vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_ci{
                    {}, severity, message_type, callback
                };
                instance_ci.setPNext(&debug_messenger_ci);
                instance_ci.setPEnabledLayerNames(validation_layers);
            }

            instance_ci.setPEnabledExtensionNames(extensions);
            return vk::createInstanceUnique(instance_ci);
        }

        vk::PhysicalDevice pick_physical_device() {
            auto devices = m_instance->enumeratePhysicalDevices();
            std::pair<vk::PhysicalDevice, int> best_device{};

            auto device_is_compatible = [this](const vk::PhysicalDevice& device) {
                auto available_extensions = device.enumerateDeviceExtensionProperties();
                std::set<std::string> unsupported_extensions(required_device_extensions.begin(),
                                                             required_device_extensions.end());

                for (const auto& extension : available_extensions) {
                    unsupported_extensions.erase(extension.extensionName);
                }

                if (!unsupported_extensions.empty()) {
                    return false;
                }

                if (!device.getFeatures().samplerAnisotropy) {
                    return false;
                }

                return true;
            };

            auto calculate_suitability = [](const vk::PhysicalDevice& device) {
                auto properties = device.getProperties();
                int suitability = 0;

                switch (properties.deviceType) {
                    case vk::PhysicalDeviceType::eDiscreteGpu: suitability += 10000;
                    case vk::PhysicalDeviceType::eIntegratedGpu: suitability += 5000;
                    case vk::PhysicalDeviceType::eVirtualGpu: suitability += 2000;
                    case vk::PhysicalDeviceType::eCpu: suitability += 1000;
                    case vk::PhysicalDeviceType::eOther: suitability += 1;
                    default: suitability += 0;
                };

                suitability += properties.limits.maxImageDimension2D;
                return suitability;
            };

            for (const auto& device : devices) {
                if (device_is_compatible(device)) {
                    int suitability = calculate_suitability(device);
                    if (suitability > best_device.second) {
                        best_device = { device, suitability };
                    }
                }
            }

            if (!best_device.first) {
                throw std::runtime_error("failed to find a suitable GPU!");
            }

            return best_device.first;
        }

        uint32_t find_memory_type(uint32_t filter, vk::MemoryPropertyFlags flags) {
            auto properties = m_physical_device.getMemoryProperties();

            for (uint32_t i = 0; i < properties.memoryTypeCount; i++) {
                if ((filter & (1 << i)) && (properties.memoryTypes[i].propertyFlags & flags) == flags) {
                    return i;
                }
            }

            throw std::runtime_error("failed to find suitable memory type!");
        }

        vk::Format find_supported_format(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
                                         vk::FormatFeatureFlags features) {
            for (auto format : candidates) {
                auto properties = m_physical_device.getFormatProperties(format);

                if (tiling == vk::ImageTiling::eLinear &&
                    (properties.linearTilingFeatures & features) == features) {
                    return format;
                }
                if (tiling == vk::ImageTiling::eOptimal &&
                    (properties.optimalTilingFeatures & features) == features) {
                    return format;
                }
            }

            throw std::runtime_error("failed to find supported format!");
        }

        inline vk::Format find_depth_format() {
            return find_supported_format(
                { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
                vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
        }

        inline auto get_available_extensions() {
            return m_physical_device.enumerateDeviceExtensionProperties();
        }

        inline auto create_unique_logical_device(const vk::DeviceCreateInfo& device_ci) {
            return m_physical_device.createDeviceUnique(device_ci);
        }

        inline auto get_queue_family_indices(const vk::SurfaceKHR& surface) {
            return QueueFamilyIndices(m_physical_device, surface);
        }

        inline auto get_swap_chain_support(const vk::SurfaceKHR& surface) {
            return SwapChainSupportDetails(m_physical_device, surface);
        }

    private:
        vk::UniqueInstance m_instance;
        vk::PhysicalDevice m_physical_device;
    };

    static std::unique_ptr<Renderer> renderer = std::make_unique<Renderer>();

    class Window {
    public:
        Window(std::string_view title, std::pair<int, int> size, std::vector<std::pair<int, int>>& hints) {
            if (!glfw_initialised && glfwInit() == GLFW_TRUE) {
                glfw_initialised = true;
            }

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            for (auto hint : hints) {
                glfwWindowHint(hint.first, hint.second);
            }

            m_window = glfwCreateWindow(size.first, size.second, title.data(), nullptr, nullptr);

            if (m_window) {
                glfw_windows += 1;
            } else {
                throw std::runtime_error("failed to create GLFW window!");
            }
        }

        ~Window() {
            glfwDestroyWindow(m_window);
            glfw_windows -= 1;

            if (glfw_windows < 1) {
                glfwTerminate();
                glfw_initialised = false;
            }
        }

        const bool should_close() const {
            return glfwWindowShouldClose(m_window);
        }

        static void poll() {
            glfwPollEvents();
        }

        GLFWwindow* get_handle() {
            return m_window;
        }

    private:
        GLFWwindow* m_window = nullptr;
    };

    class RenderTarget {
    public:
        RenderTarget(std::string_view title, std::pair<int, int> size,
                     std::vector<std::pair<int, int>> hints = {})
            : window(std::make_unique<Window>(title, size, hints)), m_surface(create_surface()),
              m_indices(renderer->get_queue_family_indices(*m_surface)), m_device(create_logical_device()),
              m_graphics_queue(m_device->getQueue(m_indices.graphics.value(), 0)),
              m_present_queue(m_device->getQueue(m_indices.present.value(), 0)),
              m_swap_chain_support(renderer->get_swap_chain_support(*m_surface)) {}

        ~RenderTarget() {}

        std::unique_ptr<Window> window;

    private:
        vk::UniqueSurfaceKHR create_surface() {
            VkSurfaceKHR raw_surface{};

            if (glfwCreateWindowSurface(renderer->get_instance(), window->get_handle(), nullptr,
                                        &raw_surface) != VK_SUCCESS) {
                throw std::runtime_error("failed to create window surface!");
            }

            vk::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE> deleter(
                renderer->get_instance());
            return vk::UniqueSurfaceKHR(raw_surface, deleter);
        }

        vk::UniqueDevice create_logical_device() {
            vk::PhysicalDeviceFeatures enabled_features{};
            enabled_features.setSamplerAnisotropy(VK_TRUE);
            enabled_features.setSampleRateShading(VK_TRUE);

            constexpr std::array queue_priorities{ 1.0F };
            std::vector<vk::DeviceQueueCreateInfo> queue_cis;
            for (auto index : m_indices.set()) {
                queue_cis.push_back(vk::DeviceQueueCreateInfo({}, index, queue_priorities));
            }

            std::vector device_extensions(required_device_extensions.begin(),
                                          required_device_extensions.end());

            for (auto available_extension : renderer->get_available_extensions()) {
                for (auto portability_extension : device_portability_extensions) {
                    if (std::string_view(available_extension.extensionName) == portability_extension) {
                        device_extensions.emplace_back(portability_extension.data());
                    }
                }
            }

            vk::DeviceCreateInfo device_ci{};
            device_ci.setQueueCreateInfos(queue_cis);
            device_ci.setPEnabledFeatures(&enabled_features);
            device_ci.setPEnabledExtensionNames(device_extensions);

            if (enable_validation_layers) {
                device_ci.setPEnabledLayerNames(validation_layers);
            }

            return renderer->create_unique_logical_device(device_ci);
        }

        vk::UniqueSurfaceKHR m_surface;
        QueueFamilyIndices m_indices;
        vk::UniqueDevice m_device;
        vk::Queue m_graphics_queue;
        vk::Queue m_present_queue;
        SwapChainSupportDetails m_swap_chain_support;
    };
}