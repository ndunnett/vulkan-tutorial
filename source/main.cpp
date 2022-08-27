#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr std::array validation_layers{ "VK_LAYER_KHRONOS_validation" };
constexpr auto create_debug_utils_messenger{ "vkCreateDebugUtilsMessengerEXT" };
constexpr auto destroy_debug_utils_messenger{
    "vkDestroyDebugUtilsMessengerEXT"
};

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

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;

    bool is_complete() {
        return graphics_family.has_value();
    }
};

class HelloTriangleApplication {
public:
    void run() {
        init_window();
        init_vulkan();
        main_loop();
        cleanup();
    }

private:
    void init_window() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void init_vulkan() {
        create_instance();

        if (enable_validation_layers) {
            setup_debug_messenger();
        }

        pick_physical_device();
    }

    void main_loop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        if (enable_validation_layers) {
            destroy_debug_messenger();
        }

        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void create_instance() {
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Hello Triangle";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "No Engine";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo instance_ci{};
        instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_ci.pApplicationInfo = &app_info;

        auto extensions = get_required_extensions();
        if (using_molten_vk) {
            extensions.emplace_back(
                VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            instance_ci.flags |=
                VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
        if (enable_validation_layers) {
            auto debug_ci = debug_messenger_create_info();
            instance_ci.pNext = &debug_ci;
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        instance_ci.enabledExtensionCount = extensions.size();
        instance_ci.ppEnabledExtensionNames = extensions.data();

        if (enable_validation_layers) {
            if (!check_validation_layer_support()) {
                throw std::runtime_error(
                    "validation layers requested, but not available!");
            }
            instance_ci.enabledLayerCount = validation_layers.size();
            instance_ci.ppEnabledLayerNames = validation_layers.data();
        } else {
            instance_ci.enabledLayerCount = 0;
        }

        if (vkCreateInstance(&instance_ci, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

    void pick_physical_device() {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

        if (device_count == 0) {
            throw std::runtime_error(
                "failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

        std::pair<VkPhysicalDevice, uint32_t> best_device{ VK_NULL_HANDLE, 0 };

        for (const auto& device : devices) {
            uint32_t suitability = rate_device_suitability(device);
            if (suitability > best_device.second) {
                best_device = { device, suitability };
                break;
            }
        }

        if (best_device.first == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }

        physical_device = best_device.first;
    }

    void setup_debug_messenger() {
        auto ci = debug_messenger_create_info();
        auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, create_debug_utils_messenger));

        if (func != nullptr &&
            func(instance, &ci, nullptr, &debug_messenger) == VK_SUCCESS) {
            return;
        }

        throw std::runtime_error("failed to set up debug messenger!");
    }

    void destroy_debug_messenger() {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, destroy_debug_utils_messenger));

        if (func != nullptr) {
            func(instance, debug_messenger, nullptr);
        }
    }

    static QueueFamilyIndices
    find_queue_families(const VkPhysicalDevice& device) {
        QueueFamilyIndices indices{};

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                                 nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                                 queue_families.data());

        int i = 0;
        for (const auto& queue_family : queue_families) {
            if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphics_family = i;
            }

            if (indices.is_complete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    static uint32_t rate_device_suitability(const VkPhysicalDevice& device) {
        if (!find_queue_families(device).is_complete()) {
            return 0;
        }

        VkPhysicalDeviceProperties properties{};
        VkPhysicalDeviceFeatures features{};
        vkGetPhysicalDeviceProperties(device, &properties);
        vkGetPhysicalDeviceFeatures(device, &features);

        uint32_t suitability = 0;

        switch (properties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: suitability += 10000;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: suitability += 5000;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: suitability += 2000;
            case VK_PHYSICAL_DEVICE_TYPE_CPU: suitability += 1000;
            case VK_PHYSICAL_DEVICE_TYPE_OTHER: suitability += 1;
            default: suitability += 0;
        };

        suitability += properties.limits.maxImageDimension2D;
        return suitability;
    }

    static VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info() {
        VkDebugUtilsMessengerCreateInfoEXT ci{};
        ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        ci.pfnUserCallback = debug_callback;
        return ci;
    }

    static std::vector<const char*> get_required_extensions() {

        std::vector<const char*> extensions{};
        uint32_t glfw_extension_count = 0;
        const char** glfw_extensions =
            glfwGetRequiredInstanceExtensions(&glfw_extension_count);

        for (size_t i = 0; i < glfw_extension_count; i++) {
            extensions.emplace_back(glfw_extensions[i]);
        }

        return extensions;
    }

    static bool check_validation_layer_support() {
        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count,
                                           available_layers.data());

        for (const auto* layer_name : validation_layers) {
            bool layer_found = false;
            for (const auto& layer_properties : available_layers) {
                if (layer_name ==
                    std::string_view{ layer_properties.layerName }) {
                    layer_found = true;
                    break;
                }
            }

            if (!layer_found) {
                return false;
            }
        }

        return true;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                   VkDebugUtilsMessageTypeFlagsEXT type,
                   const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
                   void* user_data) {
        std::cerr << "validation layer: " << callback_data->pMessage
                  << std::endl;
        return VK_FALSE;
    }

    VkInstance instance{};
    GLFWwindow* window = nullptr;
    VkDebugUtilsMessengerEXT debug_messenger{};
    VkPhysicalDevice physical_device{ VK_NULL_HANDLE };
};

int main() {
    HelloTriangleApplication app{};

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
