#pragma once

#include "helpers.h"

namespace tutorial {
    class VulkanCore {
    public:
        VulkanCore(std::shared_ptr<GlfwInstance> glfw, std::string_view name = "Vulkan")
            : m_glfw(glfw), m_instance(create_instance(name, glfw->required_extensions())) {}

        ~VulkanCore() {}

        vk::UniqueInstance create_instance(std::string_view name, std::vector<const char*> extensions) const;
        std::unique_ptr<vk::PhysicalDevice> pick_physical_device(const vk::SurfaceKHR& surface) const;
        vk::UniqueDevice create_logical_device() const;
        void initialise_devices(const vk::SurfaceKHR& surface);
        std::pair<vk::Queue, vk::Queue> get_queues() const;
        auto get_max_msaa_samples() const;
        uint32_t find_memory_type(uint32_t filter, vk::MemoryPropertyFlags flags);
        vk::Format find_supported_format(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
                                         vk::FormatFeatureFlags features);

        inline const vk::Instance& get_instance() {
            return m_instance.get();
        }

        inline auto get_deleter() {
            return vk::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(m_instance.get());
        }

        inline const vk::PhysicalDevice& get_physical_device() {
            return *m_physical_device;
        }

        inline const vk::Device& get_logical_device() {
            return *m_logical_device;
        }

        inline vk::Format find_depth_format() {
            return find_supported_format(
                { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
                vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
        }

    private:
        bool devices_initialised = false;
        std::shared_ptr<GlfwInstance> m_glfw;
        vk::UniqueInstance m_instance;
        std::unique_ptr<vk::PhysicalDevice> m_physical_device = nullptr;
        QueueFamilyIndices m_queue_family_indices{};
        vk::UniqueDevice m_logical_device{};
    };
}