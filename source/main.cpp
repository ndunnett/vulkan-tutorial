#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <chrono>
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
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
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

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static auto get_binding_description() {
        VkVertexInputBindingDescription binding_description{};
        binding_description.binding = 0;
        binding_description.stride = sizeof(Vertex);
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding_description;
    }

    static auto get_attribute_descriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};
        attribute_descriptions.at(0).binding = 0;
        attribute_descriptions.at(0).location = 0;
        attribute_descriptions.at(0).format = VK_FORMAT_R32G32_SFLOAT;
        attribute_descriptions.at(0).offset = offsetof(Vertex, pos);
        attribute_descriptions.at(1).binding = 0;
        attribute_descriptions.at(1).location = 1;
        attribute_descriptions.at(1).format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions.at(1).offset = offsetof(Vertex, color);
        return attribute_descriptions;
    }
};

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
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
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
        create_render_pass();
        create_descriptor_set_layout();
        create_graphics_pipeline();
        create_framebuffers();
        create_command_pool();
        create_texture_image();
        create_vertex_buffer();
        create_index_buffer();
        create_uniform_buffers();
        create_descriptor_pool();
        create_descriptor_sets();
        create_command_buffers();
        create_sync_objects();
    }

    void main_loop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            draw_frame();
        }
        vkDeviceWaitIdle(device);
    }

    void cleanup() {
        cleanup_swap_chain();
        vkDestroyImage(device, texture_image, nullptr);
        vkFreeMemory(device, texture_image_memory, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(device, uniform_buffers.at(i), nullptr);
            vkFreeMemory(device, uniform_buffers_memory.at(i), nullptr);
        }

        vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
        vkDestroyBuffer(device, index_buffer, nullptr);
        vkFreeMemory(device, index_buffer_memory, nullptr);
        vkDestroyBuffer(device, vertex_buffer, nullptr);
        vkFreeMemory(device, vertex_buffer_memory, nullptr);
        vkDestroyPipeline(device, graphics_pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        vkDestroyRenderPass(device, render_pass, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, image_available_semaphores.at(i), nullptr);
            vkDestroySemaphore(device, render_finished_semaphores.at(i), nullptr);
            vkDestroyFence(device, in_flight_fences.at(i), nullptr);
        }

        vkDestroyCommandPool(device, command_pool, nullptr);
        vkDestroyDevice(device, nullptr);

        if (enable_validation_layers) {
            destroy_debug_messenger();
        }

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void draw_frame() {
        vkWaitForFences(device, 1, &in_flight_fences.at(current_frame), VK_TRUE, UINT64_MAX);

        uint32_t image_index = 0;
        VkResult result =
            vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX,
                                  image_available_semaphores.at(current_frame), VK_NULL_HANDLE, &image_index);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return recreate_swap_chain();
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        vkResetFences(device, 1, &in_flight_fences.at(current_frame));
        vkResetCommandBuffer(command_buffers.at(current_frame), 0);
        record_command_buffer(command_buffers.at(current_frame), image_index);
        update_uniform_buffer(current_frame);

        const std::array signal_semaphores{ render_finished_semaphores.at(current_frame) };
        const std::array wait_semaphores{ image_available_semaphores.at(current_frame) };
        const std::array wait_stages{ VkPipelineStageFlags(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) };

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = wait_semaphores.size();
        submit_info.pWaitSemaphores = wait_semaphores.data();
        submit_info.pWaitDstStageMask = wait_stages.data();
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffers.at(current_frame);
        submit_info.signalSemaphoreCount = signal_semaphores.size();
        submit_info.pSignalSemaphores = signal_semaphores.data();

        if (vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fences.at(current_frame)) !=
            VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        const std::array swap_chains{ swap_chain };
        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = signal_semaphores.size();
        present_info.pWaitSemaphores = signal_semaphores.data();
        present_info.swapchainCount = swap_chains.size();
        present_info.pSwapchains = swap_chains.data();
        present_info.pImageIndices = &image_index;
        present_info.pResults = nullptr; // Optional

        result = vkQueuePresentKHR(present_queue, &present_info);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized) {
            framebuffer_resized = false;
            recreate_swap_chain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
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

    void cleanup_swap_chain() {
        for (auto* framebuffer : swap_chain_framebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        for (auto* image_view : swap_chain_image_views) {
            vkDestroyImageView(device, image_view, nullptr);
        }

        vkDestroySwapchainKHR(device, swap_chain, nullptr);
    }

    void recreate_swap_chain() {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);

        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(device);
        cleanup_swap_chain();
        create_swap_chain();
        create_image_views();
        create_framebuffers();
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

    void create_render_pass() {
        VkAttachmentDescription color_attachment{};
        color_attachment.format = swap_chain_image_format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_ref{};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo render_pass_ci{};
        render_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_ci.attachmentCount = 1;
        render_pass_ci.pAttachments = &color_attachment;
        render_pass_ci.subpassCount = 1;
        render_pass_ci.pSubpasses = &subpass;
        render_pass_ci.dependencyCount = 1;
        render_pass_ci.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &render_pass_ci, nullptr, &render_pass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void create_descriptor_set_layout() {
        VkDescriptorSetLayoutBinding ubo_layout_binding{};
        ubo_layout_binding.binding = 0;
        ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo_layout_binding.descriptorCount = 1;
        ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        ubo_layout_binding.pImmutableSamplers = nullptr; // Optional

        VkDescriptorSetLayoutCreateInfo layout_ci{};
        layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_ci.bindingCount = 1;
        layout_ci.pBindings = &ubo_layout_binding;

        if (vkCreateDescriptorSetLayout(device, &layout_ci, nullptr, &descriptor_set_layout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    void create_descriptor_pool() {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_size.descriptorCount = MAX_FRAMES_IN_FLIGHT;

        VkDescriptorPoolCreateInfo pool_ci{};
        pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_ci.poolSizeCount = 1;
        pool_ci.pPoolSizes = &pool_size;
        pool_ci.maxSets = MAX_FRAMES_IN_FLIGHT;

        if (vkCreateDescriptorPool(device, &pool_ci, nullptr, &descriptor_pool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void create_descriptor_sets() {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptor_set_layout);
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = descriptor_pool;
        alloc_info.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        alloc_info.pSetLayouts = layouts.data();

        descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo buffer_info{};
            buffer_info.buffer = uniform_buffers.at(i);
            buffer_info.offset = 0;
            buffer_info.range = sizeof(UniformBufferObject);

            VkWriteDescriptorSet write_descriptor{};
            write_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_descriptor.dstSet = descriptor_sets.at(i);
            write_descriptor.dstBinding = 0;
            write_descriptor.dstArrayElement = 0;
            write_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write_descriptor.descriptorCount = 1;
            write_descriptor.pBufferInfo = &buffer_info;
            write_descriptor.pImageInfo = nullptr;       // Optional
            write_descriptor.pTexelBufferView = nullptr; // Optional
            vkUpdateDescriptorSets(device, 1, &write_descriptor, 0, nullptr);
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
        auto binding_description = Vertex::get_binding_description();
        auto attribute_descriptions = Vertex::get_attribute_descriptions();

        VkPipelineVertexInputStateCreateInfo vertex_input_ci{};
        vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_ci.vertexBindingDescriptionCount = 1;
        vertex_input_ci.pVertexBindingDescriptions = &binding_description;
        vertex_input_ci.vertexAttributeDescriptionCount = attribute_descriptions.size();
        vertex_input_ci.pVertexAttributeDescriptions = attribute_descriptions.data();

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
        rasterizer_ci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
        pipeline_layout_ci.setLayoutCount = 1;                   // Optional
        pipeline_layout_ci.pSetLayouts = &descriptor_set_layout; // Optional
        pipeline_layout_ci.pushConstantRangeCount = 0;           // Optional
        pipeline_layout_ci.pPushConstantRanges = nullptr;        // Optional

        if (vkCreatePipelineLayout(device, &pipeline_layout_ci, nullptr, &pipeline_layout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = shader_stages.size();
        pipeline_info.pStages = shader_stages.data();
        pipeline_info.pVertexInputState = &vertex_input_ci;
        pipeline_info.pInputAssemblyState = &input_assembly_ci;
        pipeline_info.pViewportState = &viewport_state_ci;
        pipeline_info.pRasterizationState = &rasterizer_ci;
        pipeline_info.pMultisampleState = &multisampling_ci;
        pipeline_info.pDepthStencilState = nullptr; // Optional
        pipeline_info.pColorBlendState = &color_blend_ci;
        pipeline_info.pDynamicState = &dynamic_state_ci;
        pipeline_info.layout = pipeline_layout;
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipeline_info.basePipelineIndex = -1;              // Optional

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                      &graphics_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(device, frag_shader_module, nullptr);
        vkDestroyShaderModule(device, vert_shader_module, nullptr);
    }

    void create_framebuffers() {
        swap_chain_framebuffers.resize(swap_chain_image_views.size());

        for (size_t i = 0; i < swap_chain_image_views.size(); i++) {
            const std::array attachments{ swap_chain_image_views[i] };

            VkFramebufferCreateInfo framebuffer_ci{};
            framebuffer_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_ci.renderPass = render_pass;
            framebuffer_ci.attachmentCount = attachments.size();
            framebuffer_ci.pAttachments = attachments.data();
            framebuffer_ci.width = swap_chain_extent.width;
            framebuffer_ci.height = swap_chain_extent.height;
            framebuffer_ci.layers = 1;

            if (vkCreateFramebuffer(device, &framebuffer_ci, nullptr, &swap_chain_framebuffers[i]) !=
                VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void create_command_pool() {
        QueueFamilyIndices indices = find_queue_families(physical_device, surface);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = indices.graphics_family.value();

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &command_pool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void create_texture_image() {
        int width = 0;
        int height = 0;
        int channels = 0;

        stbi_uc* pixels = stbi_load("textures/texture.jpeg", &width, &height, &channels, STBI_rgb_alpha);
        auto image_size = 4 * static_cast<VkDeviceSize>(width * height);

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        VkBuffer staging_buffer{};
        VkDeviceMemory staging_buffer_memory{};

        create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      staging_buffer, staging_buffer_memory);

        void* data = nullptr;
        vkMapMemory(device, staging_buffer_memory, 0, image_size, 0, &data);
        memcpy(data, pixels, image_size);
        vkUnmapMemory(device, staging_buffer_memory);
        stbi_image_free(pixels);

        create_image(width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture_image, texture_image_memory);

        transition_image_layout(texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copy_buffer_to_image(staging_buffer, texture_image, static_cast<uint32_t>(width),
                             static_cast<uint32_t>(height));
        transition_image_layout(texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_buffer_memory, nullptr);
    }

    void create_image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                      VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image,
                      VkDeviceMemory& image_memory) {
        VkImageCreateInfo image_ci{};
        image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_ci.imageType = VK_IMAGE_TYPE_2D;
        image_ci.extent.width = width;
        image_ci.extent.height = height;
        image_ci.extent.depth = 1;
        image_ci.mipLevels = 1;
        image_ci.arrayLayers = 1;
        image_ci.format = format;
        image_ci.tiling = tiling;
        image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_ci.usage = usage;
        image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
        image_ci.flags = 0; // Optional

        if (vkCreateImage(device, &image_ci, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements mem_req{};
        vkGetImageMemoryRequirements(device, image, &mem_req);

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex = find_memory_type(mem_req.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &alloc_info, nullptr, &image_memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(device, image, image_memory, 0);
    }

    void transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout,
                                 VkImageLayout new_layout) {
        auto* command_buffer = begin_single_time_commands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0; // TODO
        barrier.dstAccessMask = 0; // TODO

        VkPipelineStageFlags source_stage{};
        VkPipelineStageFlags destination_stage{};

        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                   new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0, 0, nullptr, 0, nullptr, 1,
                             &barrier);

        end_single_time_commands(command_buffer);
    }

    void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer command_buffer = begin_single_time_commands();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { width, height, 1 };

        vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &region);

        end_single_time_commands(command_buffer);
    }

    VkCommandBuffer begin_single_time_commands() {
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = command_pool;
        alloc_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer{};
        vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(command_buffer, &begin_info);
        return command_buffer;
    }

    void end_single_time_commands(VkCommandBuffer command_buffer) {
        vkEndCommandBuffer(command_buffer);

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;

        vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphics_queue);
        vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    }

    void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                       VkBuffer& buffer, VkDeviceMemory& buffer_memory) {
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements mem_req{};
        vkGetBufferMemoryRequirements(device, buffer, &mem_req);

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex = find_memory_type(mem_req.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &alloc_info, nullptr, &buffer_memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        vkBindBufferMemory(device, buffer, buffer_memory, 0);
    }

    void copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) {
        auto* command_buffer = begin_single_time_commands();
        VkBufferCopy copy_region{};
        copy_region.srcOffset = 0; // Optional
        copy_region.dstOffset = 0; // Optional
        copy_region.size = size;
        vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);
        end_single_time_commands(command_buffer);
    }

    void create_vertex_buffer() {
        VkDeviceSize buffer_size = sizeof(vertices.at(0)) * vertices.size();
        VkBuffer staging_buffer{};
        VkDeviceMemory staging_buffer_memory{};
        create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      staging_buffer, staging_buffer_memory);

        void* data = nullptr;
        vkMapMemory(device, staging_buffer_memory, 0, buffer_size, 0, &data);
        memcpy(data, vertices.data(), buffer_size);
        vkUnmapMemory(device, staging_buffer_memory);

        create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertex_buffer, vertex_buffer_memory);
        copy_buffer(staging_buffer, vertex_buffer, buffer_size);
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_buffer_memory, nullptr);
    }

    void create_index_buffer() {
        VkDeviceSize buffer_size = sizeof(indices.at(0)) * indices.size();
        VkBuffer staging_buffer{};
        VkDeviceMemory staging_buffer_memory{};
        create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      staging_buffer, staging_buffer_memory);

        void* data = nullptr;
        vkMapMemory(device, staging_buffer_memory, 0, buffer_size, 0, &data);
        memcpy(data, indices.data(), buffer_size);
        vkUnmapMemory(device, staging_buffer_memory);

        create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, index_buffer, index_buffer_memory);
        copy_buffer(staging_buffer, index_buffer, buffer_size);
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_buffer_memory, nullptr);
    }

    void create_uniform_buffers() {
        VkDeviceSize buffer_size = sizeof(UniformBufferObject);

        uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniform_buffers_memory.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            create_buffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          uniform_buffers.at(i), uniform_buffers_memory.at(i));
        }
    }

    void update_uniform_buffer(uint32_t current_image) {
        static auto start_time = std::chrono::high_resolution_clock::now();
        auto current_time = std::chrono::high_resolution_clock::now();
        float time =
            std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0F), time * glm::radians(90.0F), glm::vec3(0.0F, 0.0F, 1.0F));
        ubo.view = glm::lookAt(glm::vec3(2.0F, 2.0F, 2.0F), glm::vec3(0.0F, 0.0F, 0.0F),
                               glm::vec3(0.0F, 0.0F, 1.0F));
        ubo.proj = glm::perspective(glm::radians(45.0F),
                                    static_cast<float>(swap_chain_extent.width) /
                                        static_cast<float>(swap_chain_extent.height),
                                    0.1F, 10.0F);
        ubo.proj[1][1] *= -1;

        void* data = nullptr;
        vkMapMemory(device, uniform_buffers_memory.at(current_image), 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(device, uniform_buffers_memory.at(current_image));
    }

    void create_command_buffers() {
        command_buffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = command_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = command_buffers.size();

        if (vkAllocateCommandBuffers(device, &allocate_info, command_buffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index) {
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;                  // Optional
        begin_info.pInheritanceInfo = nullptr; // Optional

        if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = render_pass;
        render_pass_info.framebuffer = swap_chain_framebuffers[image_index];
        render_pass_info.renderArea.offset = { 0, 0 };
        render_pass_info.renderArea.extent = swap_chain_extent;
        VkClearValue clear_color = { { { 0.0F, 0.0F, 0.0F, 1.0F } } };
        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues = &clear_color;

        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

        VkViewport viewport{};
        viewport.x = 0.0F;
        viewport.y = 0.0F;
        viewport.width = static_cast<float>(swap_chain_extent.width);
        viewport.height = static_cast<float>(swap_chain_extent.height);
        viewport.minDepth = 0.0F;
        viewport.maxDepth = 1.0F;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swap_chain_extent;
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        const std::array vertex_buffers{ vertex_buffer };
        const std::array offsets{ VkDeviceSize(0) };
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers.data(), offsets.data());
        vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT16);

        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1,
                                &descriptor_sets.at(current_frame), 0, nullptr);

        vkCmdDrawIndexed(command_buffer, indices.size(), 1, 0, 0, 0);
        vkCmdEndRenderPass(command_buffer);

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void create_sync_objects() {
        image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphore_ci{};
        semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_ci{};
        fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphore_ci, nullptr, &image_available_semaphores.at(i)) !=
                    VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphore_ci, nullptr, &render_finished_semaphores.at(i)) !=
                    VK_SUCCESS ||
                vkCreateFence(device, &fence_ci, nullptr, &in_flight_fences.at(i)) != VK_SUCCESS) {
                throw std::runtime_error("failed to create semaphores!");
            }
        }
    }

    uint32_t find_memory_type(uint32_t filter, VkMemoryPropertyFlags flags) {
        VkPhysicalDeviceMemoryProperties properties{};
        vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);

        for (uint32_t i = 0; i < properties.memoryTypeCount; i++) {
            if ((filter & (1 << i)) && (properties.memoryTypes[i].propertyFlags & flags) == flags) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
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

    static void framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
        auto* app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->framebuffer_resized = true;
    }

    GLFWwindow* window = nullptr;
    VkInstance instance{};
    VkDebugUtilsMessengerEXT debug_messenger{};
    VkPhysicalDevice physical_device{ VK_NULL_HANDLE };
    VkDevice device{};
    VkQueue graphics_queue{};
    VkQueue present_queue{};
    VkSurfaceKHR surface{};
    VkSwapchainKHR swap_chain{};
    VkFormat swap_chain_image_format{};
    VkExtent2D swap_chain_extent{};
    VkRenderPass render_pass{};
    VkDescriptorSetLayout descriptor_set_layout{};
    VkPipelineLayout pipeline_layout{};
    VkPipeline graphics_pipeline{};
    VkCommandPool command_pool{};
    VkDescriptorPool descriptor_pool{};
    uint32_t current_frame = 0;

    VkImage texture_image{};
    VkDeviceMemory texture_image_memory{};
    VkBuffer vertex_buffer{};
    VkDeviceMemory vertex_buffer_memory{};
    VkBuffer index_buffer{};
    VkDeviceMemory index_buffer_memory{};
    std::vector<VkBuffer> uniform_buffers;
    std::vector<VkDeviceMemory> uniform_buffers_memory;

    std::vector<VkImage> swap_chain_images;
    std::vector<VkImageView> swap_chain_image_views;
    std::vector<VkFramebuffer> swap_chain_framebuffers;
    std::vector<VkCommandBuffer> command_buffers;
    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_finished_semaphores;
    std::vector<VkFence> in_flight_fences;
    std::vector<VkDescriptorSet> descriptor_sets;

    const std::vector<Vertex> vertices{ { { -0.5F, -0.5F }, { 1.0F, 0.0F, 0.0F } },
                                        { { 0.5F, -0.5F }, { 0.0F, 1.0F, 0.0F } },
                                        { { 0.5F, 0.5F }, { 0.0F, 0.0F, 1.0F } },
                                        { { -0.5F, 0.5F }, { 1.0F, 1.0F, 1.0F } } };
    const std::vector<uint16_t> indices = { 0, 1, 2, 2, 3, 0 };

public:
    bool framebuffer_resized = false;
};

int main() {
    try {
        HelloTriangleApplication app{};
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
