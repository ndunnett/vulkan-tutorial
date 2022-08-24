#include "doctest.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

TEST_CASE("Look for Vulkan extensions") {
    uint32_t extensions = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensions, nullptr);
}

TEST_CASE("Initialise GLFW and create window") {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "GLFW", nullptr, nullptr);
    auto should_close = glfwWindowShouldClose(window);
    glfwPollEvents();
    glfwDestroyWindow(window);
    glfwTerminate();
}

TEST_CASE("Calculate something using GLM") {
    glm::mat4 matrix;
    glm::vec4 vec;
    auto result = matrix * vec;
}
