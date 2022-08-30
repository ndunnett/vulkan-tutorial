#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr std::array validation_layers{ "VK_LAYER_KHRONOS_validation" };
constexpr std::array required_device_extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
constexpr std::string_view create_debug_msg_ext{ "vkCreateDebugUtilsMessengerEXT" };
constexpr std::string_view destroy_debug_msg_ext{ "vkDestroyDebugUtilsMessengerEXT" };
constexpr std::array instance_portability_extensions{
    std::string_view("VK_KHR_get_physical_device_properties2"),
};
constexpr std::array device_portability_extensions{
    std::string_view("VK_KHR_portability_subset"),
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
    std::optional<uint32_t> present_family;

    bool is_complete() {
        return graphics_family.has_value() && present_family.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
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

        create_surface();
        pick_physical_device();
        create_logical_device();
        create_swap_chain();
        create_image_views();
        create_graphics_pipeline();
    }

    void main_loop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);

        for (auto* image_view : swap_chain_image_views) {
            vkDestroyImageView(device, image_view, nullptr);
        }

        vkDestroySwapchainKHR(device, swap_chain, nullptr);
        vkDestroyDevice(device, nullptr);

        if (enable_validation_layers) {
            destroy_debug_messenger();
        }

        vkDestroySurfaceKHR(instance, surface, nullptr);
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
            extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            instance_ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
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
                throw std::runtime_error("validation layers requested, but not available!");
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

    void create_surface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void pick_physical_device() {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

        if (device_count == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

        std::pair<VkPhysicalDevice, uint32_t> best_device{ VK_NULL_HANDLE, 0 };

        for (const auto& device : devices) {
            uint32_t suitability = rate_device_suitability(device, surface);
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

    void create_logical_device() {
        QueueFamilyIndices indices = find_queue_families(physical_device, surface);
        float queue_priority = 1.0F;
        VkPhysicalDeviceFeatures device_features{};

        std::set<uint32_t> unique_queue_families = { indices.graphics_family.value(),
                                                     indices.present_family.value() };

        std::vector<VkDeviceQueueCreateInfo> queue_cis;

        for (auto queue_family : unique_queue_families) {
            VkDeviceQueueCreateInfo queue_ci{};
            queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_ci.queueFamilyIndex = queue_family;
            queue_ci.queueCount = 1;
            queue_ci.pQueuePriorities = &queue_priority;
            queue_cis.push_back(queue_ci);
        }

        std::vector device_extensions(required_device_extensions.begin(), required_device_extensions.end());
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> available_extensions(count);
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count, available_extensions.data());

        for (auto available_extension : available_extensions) {
            for (auto portability_extension : device_portability_extensions) {
                if (std::string_view(available_extension.extensionName) == portability_extension) {
                    device_extensions.emplace_back(portability_extension.data());
                }
            }
        }

        VkDeviceCreateInfo device_ci{};
        device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_ci.pQueueCreateInfos = queue_cis.data();
        device_ci.queueCreateInfoCount = queue_cis.size();
        device_ci.pEnabledFeatures = &device_features;
        device_ci.enabledExtensionCount = device_extensions.size();
        device_ci.ppEnabledExtensionNames = device_extensions.data();

        if (enable_validation_layers) {
            device_ci.enabledLayerCount = validation_layers.size();
            device_ci.ppEnabledLayerNames = validation_layers.data();
        } else {
            device_ci.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physical_device, &device_ci, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(device, indices.graphics_family.value(), 0, &graphics_queue);
        vkGetDeviceQueue(device, indices.present_family.value(), 0, &present_queue);
    }

    void setup_debug_messenger() {
        auto ci = debug_messenger_create_info();
        auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, create_debug_msg_ext.data()));

        if (func != nullptr && func(instance, &ci, nullptr, &debug_messenger) == VK_SUCCESS) {
            return;
        }

        throw std::runtime_error("failed to set up debug messenger!");
    }

    void destroy_debug_messenger() {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, destroy_debug_msg_ext.data()));

        if (func != nullptr) {
            func(instance, debug_messenger, nullptr);
        }
    }

    void create_swap_chain() {
        auto swap_chain_support = query_swap_chain_support(physical_device, surface);
        auto surface_format = choose_swap_surface_format(swap_chain_support.formats);
        auto present_mode = choose_swap_present_mode(swap_chain_support.present_modes);
        auto extent = choose_swap_extent(swap_chain_support.capabilities, window);

        uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
        if (swap_chain_support.capabilities.maxImageCount > 0 &&
            image_count > swap_chain_support.capabilities.maxImageCount) {
            image_count = swap_chain_support.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR ci{};
        ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ci.surface = surface;
        ci.minImageCount = image_count;
        ci.imageFormat = surface_format.format;
        ci.imageColorSpace = surface_format.colorSpace;
        ci.imageExtent = extent;
        ci.imageArrayLayers = 1;
        ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        auto indices = find_queue_families(physical_device, surface);
        std::array queue_family_indices{ indices.graphics_family.value(), indices.present_family.value() };

        if (indices.graphics_family != indices.present_family) {
            ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            ci.queueFamilyIndexCount = queue_family_indices.size();
            ci.pQueueFamilyIndices = queue_family_indices.data();
        } else {
            ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            ci.queueFamilyIndexCount = 0;     // Optional
            ci.pQueueFamilyIndices = nullptr; // Optional
        }

        ci.preTransform = swap_chain_support.capabilities.currentTransform;
        ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode = present_mode;
        ci.clipped = VK_TRUE;
        ci.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &ci, nullptr, &swap_chain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        swap_chain_image_format = surface_format.format;
        swap_chain_extent = extent;
        vkGetSwapchainImagesKHR(device, swap_chain, &image_count, nullptr);
        swap_chain_images.resize(image_count);
        vkGetSwapchainImagesKHR(device, swap_chain, &image_count, swap_chain_images.data());
    }

    void create_image_views() {
        swap_chain_image_views.resize(swap_chain_images.size());

        for (size_t i = 0; i < swap_chain_images.size(); i++) {
            VkImageViewCreateInfo ci{};
            ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ci.image = swap_chain_images[i];
            ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ci.format = swap_chain_image_format;
            ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ci.subresourceRange.baseMipLevel = 0;
            ci.subresourceRange.levelCount = 1;
            ci.subresourceRange.baseArrayLayer = 0;
            ci.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &ci, nullptr, &swap_chain_image_views[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    void create_graphics_pipeline() {
        auto vert_shader_code = read_file("shaders/vert.spv");
        VkShaderModule vert_shader_module = create_shader_module(vert_shader_code);
        VkPipelineShaderStageCreateInfo vert_shader_ci{};
        vert_shader_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_shader_ci.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vert_shader_ci.module = vert_shader_module;
        vert_shader_ci.pName = "main";

        auto frag_shader_code = read_file("shaders/frag.spv");
        VkShaderModule frag_shader_module = create_shader_module(frag_shader_code);
        VkPipelineShaderStageCreateInfo frag_shader_ci{};
        frag_shader_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_shader_ci.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_shader_ci.module = frag_shader_module;
        frag_shader_ci.pName = "main";

        const std::array shader_stages{ vert_shader_ci, frag_shader_ci };

        VkPipelineVertexInputStateCreateInfo vertex_input_ci{};
        vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_ci.vertexBindingDescriptionCount = 0;
        vertex_input_ci.pVertexBindingDescriptions = nullptr; // Optional
        vertex_input_ci.vertexAttributeDescriptionCount = 0;
        vertex_input_ci.pVertexAttributeDescriptions = nullptr; // Optional

        VkPipelineInputAssemblyStateCreateInfo input_assembly_ci{};
        input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_ci.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0F;
        viewport.y = 0.0F;
        viewport.width = static_cast<float>(swap_chain_extent.width);
        viewport.height = static_cast<float>(swap_chain_extent.height);
        viewport.minDepth = 0.0F;
        viewport.maxDepth = 1.0F;

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swap_chain_extent;

        const std::array dynamic_states{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic_state_ci{};
        dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_ci.dynamicStateCount = dynamic_states.size();
        dynamic_state_ci.pDynamicStates = dynamic_states.data();

        VkPipelineViewportStateCreateInfo viewport_state_ci{};
        viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_ci.viewportCount = 1;
        viewport_state_ci.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer_ci{};
        rasterizer_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer_ci.depthClampEnable = VK_FALSE;
        rasterizer_ci.rasterizerDiscardEnable = VK_FALSE;
        rasterizer_ci.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer_ci.lineWidth = 1.0F;
        rasterizer_ci.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer_ci.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer_ci.depthBiasEnable = VK_FALSE;
        rasterizer_ci.depthBiasConstantFactor = 0.0F; // Optional
        rasterizer_ci.depthBiasClamp = 0.0F;          // Optional
        rasterizer_ci.depthBiasSlopeFactor = 0.0F;    // Optional

        VkPipelineMultisampleStateCreateInfo multisampling_ci{};
        multisampling_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling_ci.sampleShadingEnable = VK_FALSE;
        multisampling_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling_ci.minSampleShading = 1.0F;          // Optional
        multisampling_ci.pSampleMask = nullptr;            // Optional
        multisampling_ci.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling_ci.alphaToOneEnable = VK_FALSE;      // Optional

        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_TRUE;
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;           // Optional
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // Optional
        color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;                            // Optional
        color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;                 // Optional
        color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;                // Optional
        color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;                            // Optional

        VkPipelineColorBlendStateCreateInfo color_blend_ci{};
        color_blend_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_ci.logicOpEnable = VK_FALSE;
        color_blend_ci.logicOp = VK_LOGIC_OP_COPY; // Optional
        color_blend_ci.attachmentCount = 1;
        color_blend_ci.pAttachments = &color_blend_attachment;
        color_blend_ci.blendConstants[0] = 0.0F; // Optional
        color_blend_ci.blendConstants[1] = 0.0F; // Optional
        color_blend_ci.blendConstants[2] = 0.0F; // Optional
        color_blend_ci.blendConstants[3] = 0.0F; // Optional

        VkPipelineLayoutCreateInfo pipeline_layout_ci{};
        pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_ci.setLayoutCount = 0;            // Optional
        pipeline_layout_ci.pSetLayouts = nullptr;         // Optional
        pipeline_layout_ci.pushConstantRangeCount = 0;    // Optional
        pipeline_layout_ci.pPushConstantRanges = nullptr; // Optional

        if (vkCreatePipelineLayout(device, &pipeline_layout_ci, nullptr, &pipeline_layout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        vkDestroyShaderModule(device, frag_shader_module, nullptr);
        vkDestroyShaderModule(device, vert_shader_module, nullptr);
    }

    VkShaderModule create_shader_module(const std::vector<char>& code) {
        VkShaderModuleCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = code.size();
        ci.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shader_module{};
        if (vkCreateShaderModule(device, &ci, nullptr, &shader_module) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shader_module;
    }

    static SwapChainSupportDetails query_swap_chain_support(const VkPhysicalDevice& device,
                                                            const VkSurfaceKHR& surface) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);

        if (format_count > 0) {
            details.formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
        }

        uint32_t present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);

        if (present_mode_count > 0) {
            details.present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count,
                                                      details.present_modes.data());
        }

        return details;
    }

    static VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) {
        for (const auto& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }

        return formats.at(0);
    }

    static VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& modes) {
        for (const auto& mode : modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return mode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    static VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actual_extent{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width,
                                         capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height,
                                          capabilities.maxImageExtent.height);

        return actual_extent;
    }

    static QueueFamilyIndices find_queue_families(const VkPhysicalDevice& device,
                                                  const VkSurfaceKHR& surface) {
        QueueFamilyIndices indices{};

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        int i = 0;
        for (const auto& queue_family : queue_families) {
            if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphics_family = i;
            }

            VkBool32 present_support = 0;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
            if (present_support) {
                indices.present_family = i;
            }

            if (indices.is_complete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    static uint32_t rate_device_suitability(const VkPhysicalDevice& device, const VkSurfaceKHR& surface) {
        if (!device_is_suitable(device, surface)) {
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

    static bool device_is_suitable(const VkPhysicalDevice& device, const VkSurfaceKHR& surface) {
        if (!find_queue_families(device, surface).is_complete()) {
            return false;
        }

        if (!device_supports_extensions(device)) {
            return false;
        }

        auto swap_chain_support = query_swap_chain_support(device, surface);
        if (swap_chain_support.formats.empty()) {
            return false;
        }
        if (swap_chain_support.present_modes.empty()) {
            return false;
        }

        return true;
    }

    static bool device_supports_extensions(const VkPhysicalDevice& device) {
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> available_extensions(count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available_extensions.data());

        std::set<std::string> unsupported_extensions(required_device_extensions.begin(),
                                                     required_device_extensions.end());

        for (const auto& extension : available_extensions) {
            unsupported_extensions.erase(extension.extensionName);
        }

        return unsupported_extensions.empty();
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
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

        for (size_t i = 0; i < glfw_extension_count; i++) {
            extensions.emplace_back(glfw_extensions[i]);
        }

        uint32_t extension_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions.data());

        for (auto available_extension : available_extensions) {
            for (auto portability_extension : instance_portability_extensions) {
                if (std::string_view(available_extension.extensionName) == portability_extension) {
                    extensions.emplace_back(portability_extension.data());
                }
            }
        }

        return extensions;
    }

    static bool check_validation_layer_support() {
        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

        for (const auto* layer_name : validation_layers) {
            bool layer_found = false;
            for (const auto& layer_properties : available_layers) {
                if (layer_name == std::string_view{ layer_properties.layerName }) {
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
    debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
                   const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
        std::cerr << "validation layer: " << callback_data->pMessage << std::endl;
        return VK_FALSE;
    }

    static std::vector<char> read_file(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t file_size = file.tellg();
        std::vector<char> buffer(file_size);

        file.seekg(0);
        file.read(buffer.data(), file_size);
        file.close();

        return buffer;
    }

    VkInstance instance{};
    GLFWwindow* window = nullptr;
    VkDebugUtilsMessengerEXT debug_messenger{};
    VkPhysicalDevice physical_device{ VK_NULL_HANDLE };
    VkDevice device{};
    VkQueue graphics_queue{};
    VkQueue present_queue{};
    VkSurfaceKHR surface{};
    VkSwapchainKHR swap_chain{};
    std::vector<VkImage> swap_chain_images;
    VkFormat swap_chain_image_format{};
    VkExtent2D swap_chain_extent{};
    std::vector<VkImageView> swap_chain_image_views;
    VkPipelineLayout pipeline_layout{};
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
