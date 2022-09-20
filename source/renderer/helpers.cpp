#include "helpers.h"

namespace tutorial {
    GlfwInstance::GlfwInstance() {
        if (glfwInit() != GLFW_TRUE) {
            throw std::runtime_error("GLFW failed to initialise!");
        }
    }

    GlfwInstance::~GlfwInstance() {
        glfwTerminate();
    }

    std::vector<const char*> GlfwInstance::required_extensions() const {
        uint32_t count = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions(&count);
        return { extensions, extensions + count };
    }

    QueueFamilyIndices::QueueFamilyIndices(const vk::PhysicalDevice& physical_device,
                                           const vk::SurfaceKHR& surface) {
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
}