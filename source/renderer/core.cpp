#include "core.h"

namespace tutorial {
    std::vector<const char*> VulkanCore::required_extensions() const {
        uint32_t count = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions(&count);
        return { extensions, extensions + count };
    }

    vk::UniqueInstance VulkanCore::create_instance(std::string_view name,
                                                   std::vector<const char*> extensions) const {
        vk::ApplicationInfo app_info{};
        app_info.setPApplicationName(name.data());
        app_info.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0));
        app_info.setEngineVersion(VK_MAKE_VERSION(1, 0, 0));
        app_info.setApiVersion(VK_API_VERSION_1_0);

        vk::InstanceCreateInfo instance_ci{};
        instance_ci.setPApplicationInfo(&app_info);

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
            auto available_layers = vk::enumerateInstanceLayerProperties();

            auto layer_is_available = [&](std::string_view layer_name) {
                return std::any_of(available_layers.begin(), available_layers.end(),
                                   [&](const vk::LayerProperties& available_layer) {
                                       return layer_name == available_layer.layerName;
                                   });
            };

            if (!std::all_of(validation_layers.begin(), validation_layers.end(), layer_is_available)) {
                throw std::runtime_error("validation layers requested, but not available!");
            }

            extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

            auto severity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;

            auto message_type = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;

            auto callback = [](VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                               VkDebugUtilsMessageTypeFlagsEXT type,
                               const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
                std::cerr << callback_data->pMessage << std::endl;
                return VK_FALSE;
            };

            vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_ci{ {}, severity, message_type, callback };
            instance_ci.setPNext(&debug_messenger_ci);
            instance_ci.setPEnabledLayerNames(validation_layers);
        }

        instance_ci.setPEnabledExtensionNames(extensions);
        return vk::createInstanceUnique(instance_ci);
    }

    vk::PhysicalDevice VulkanCore::pick_physical_device(const vk::SurfaceKHR& surface) const {
        auto devices = instance->enumeratePhysicalDevices();
        std::pair<vk::PhysicalDevice, uint32_t> best_device{};

        auto device_is_compatible = [this, surface](const vk::PhysicalDevice& device) {
            if (!QueueFamilyIndices(device, surface).is_complete()) {
                return false;
            }

            auto available_extensions = device.enumerateDeviceExtensionProperties();
            std::set<std::string> unsupported_extensions{ required_device_extensions.begin(),
                                                          required_device_extensions.end() };

            for (const auto& extension : available_extensions) {
                unsupported_extensions.erase(extension.extensionName);
            }

            if (!unsupported_extensions.empty()) {
                return false;
            }

            if (device.getFeatures().samplerAnisotropy == 0) {
                return false;
            }

            if (SwapchainSupportDetails(device, surface).is_empty()) {
                return false;
            }

            return true;
        };

        auto calculate_suitability = [](const vk::PhysicalDevice& device) {
            auto properties = device.getProperties();
            uint32_t suitability = 0;

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
                uint32_t suitability = calculate_suitability(device);
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

    vk::UniqueDevice VulkanCore::create_logical_device() const {
        vk::PhysicalDeviceFeatures enabled_features{};
        enabled_features.setSamplerAnisotropy(VK_TRUE);
        enabled_features.setSampleRateShading(VK_TRUE);

        constexpr std::array queue_priorities{ 1.0F };
        std::vector<vk::DeviceQueueCreateInfo> queue_cis;
        for (auto index : queue_family_indices.set()) {
            queue_cis.push_back(vk::DeviceQueueCreateInfo({}, index, queue_priorities));
        }

        std::vector device_extensions(required_device_extensions.begin(), required_device_extensions.end());

        for (auto available_extension : physical_device.enumerateDeviceExtensionProperties()) {
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

        return physical_device.createDeviceUnique(device_ci);
    }

    vk::UniqueCommandPool VulkanCore::create_command_pool() const {
        vk::CommandPoolCreateInfo ci{};
        ci.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
        ci.setQueueFamilyIndex(queue_family_indices.graphics.value());
        return logical_device->createCommandPoolUnique(ci);
    }

    void VulkanCore::initialise_devices(const vk::SurfaceKHR& surface) {
        if (m_devices_initialised) {
            return;
        }

        physical_device = pick_physical_device(surface);
        queue_family_indices = QueueFamilyIndices(physical_device, surface);
        logical_device = create_logical_device();
        command_pool = create_command_pool();
        m_devices_initialised = true;
    }

    vk::SampleCountFlagBits VulkanCore::get_max_msaa_samples() const {
        auto properties = physical_device.getProperties();
        auto counts =
            properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;
        if (counts & vk::SampleCountFlagBits::e64) {
            return vk::SampleCountFlagBits::e64;
        }
        if (counts & vk::SampleCountFlagBits::e32) {
            return vk::SampleCountFlagBits::e32;
        }
        if (counts & vk::SampleCountFlagBits::e16) {
            return vk::SampleCountFlagBits::e16;
        }
        if (counts & vk::SampleCountFlagBits::e8) {
            return vk::SampleCountFlagBits::e8;
        }
        if (counts & vk::SampleCountFlagBits::e4) {
            return vk::SampleCountFlagBits::e4;
        }
        if (counts & vk::SampleCountFlagBits::e2) {
            return vk::SampleCountFlagBits::e2;
        }
        return vk::SampleCountFlagBits::e1;
    }

    uint32_t VulkanCore::find_memory_type(uint32_t filter, vk::MemoryPropertyFlags flags) const {
        auto properties = physical_device.getMemoryProperties();

        for (uint32_t i = 0; i < properties.memoryTypeCount; i++) {
            if ((filter & (1 << i)) > 0 && (properties.memoryTypes[i].propertyFlags & flags) == flags) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    vk::Format VulkanCore::find_supported_format(const std::vector<vk::Format>& candidates,
                                                 vk::ImageTiling tiling,
                                                 vk::FormatFeatureFlags features) const {
        for (auto format : candidates) {
            auto properties = physical_device.getFormatProperties(format);

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

    std::pair<vk::UniqueBuffer, vk::UniqueDeviceMemory>
    VulkanCore::create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                              vk::MemoryPropertyFlags properties) const {
        std::pair<vk::UniqueBuffer, vk::UniqueDeviceMemory> buffer_pair;

        vk::BufferCreateInfo ci{};
        ci.setSize(size);
        ci.setUsage(usage);
        ci.setSharingMode(vk::SharingMode::eExclusive);
        buffer_pair.first = logical_device->createBufferUnique(ci);

        auto mem_req = logical_device->getBufferMemoryRequirements(*buffer_pair.first);
        vk::MemoryAllocateInfo ai{};
        ai.setAllocationSize(mem_req.size);
        ai.setMemoryTypeIndex(find_memory_type(mem_req.memoryTypeBits, properties));
        buffer_pair.second = logical_device->allocateMemoryUnique(ai);

        logical_device->bindBufferMemory(*buffer_pair.first, *buffer_pair.second, 0);
        return std::move(buffer_pair);
    }

    void VulkanCore::copy_to_memory(const vk::DeviceMemory& memory, const void* source, size_t size,
                                    size_t offset) const {
        void* destination = logical_device->mapMemory(memory, offset, size);
        memcpy(destination, source, size);
        logical_device->unmapMemory(memory);
    }

    void VulkanCore::copy_buffer(const vk::Queue& queue, const vk::Buffer& destination,
                                 const vk::Buffer& source, vk::DeviceSize size, vk::DeviceSize dst_offset,
                                 vk::DeviceSize src_offset) const {
        vk::BufferCopy copy_region{};
        copy_region.setSize(size);
        copy_region.setDstOffset(dst_offset);
        copy_region.setSrcOffset(src_offset);

        auto commands = get_single_time_commands(queue);
        commands.buffer->copyBuffer(source, destination, copy_region);
    }
}