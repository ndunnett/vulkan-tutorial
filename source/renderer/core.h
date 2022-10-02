#pragma once

#include "helpers.h"

namespace tutorial {
    class VulkanCore {
    public:
        VulkanCore(std::string_view name = "Vulkan")
            : instance(create_instance(name, required_extensions())) {}

        ~VulkanCore() {}

        void initialise_devices(const vk::SurfaceKHR& surface);
        vk::SampleCountFlagBits get_max_msaa_samples() const;
        uint32_t find_memory_type(uint32_t filter, vk::MemoryPropertyFlags flags) const;
        vk::Format find_supported_format(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
                                         vk::FormatFeatureFlags features) const;
        std::pair<vk::UniqueBuffer, vk::UniqueDeviceMemory>
        create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                      vk::MemoryPropertyFlags properties) const;
        void copy_to_memory(const vk::DeviceMemory& memory, const void* source, size_t size,
                            size_t offset = 0) const;
        void copy_buffer(const vk::Queue& queue, const vk::Buffer& destination, const vk::Buffer& source,
                         vk::DeviceSize size, vk::DeviceSize dst_offset = 0,
                         vk::DeviceSize src_offset = 0) const;

        inline auto get_deleter() const {
            return vk::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(instance.get());
        }

        inline SingleTimeCommands get_single_time_commands(const vk::Queue& queue) const {
            return SingleTimeCommands(*logical_device, *command_pool, queue);
        }

        inline vk::Format find_depth_format() const {
            return find_supported_format(
                { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
                vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
        }

    private:
        std::vector<const char*> required_extensions() const;
        vk::UniqueInstance create_instance(std::string_view name, std::vector<const char*> extensions) const;
        vk::PhysicalDevice pick_physical_device(const vk::SurfaceKHR& surface) const;
        vk::UniqueDevice create_logical_device() const;
        vk::UniqueCommandPool create_command_pool() const;

    public:
        vk::UniqueInstance instance;
        vk::PhysicalDevice physical_device{};
        QueueFamilyIndices queue_family_indices{};
        vk::UniqueDevice logical_device{};
        vk::UniqueCommandPool command_pool{};

    private:
        bool m_devices_initialised = false;
    };
}