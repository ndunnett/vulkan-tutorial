cmake_minimum_required(VERSION 3.24)
include(FetchContent)

# fetch repos
FetchContent_Declare(
    glfw
    GIT_REPOSITORY "https://github.com/glfw/glfw"
    GIT_TAG 7482de6071d21db77a7236155da44c172a7f6c9e # main 3.3.8
)
FetchContent_Declare(
    glm
    GIT_REPOSITORY "https://github.com/g-truc/glm"
    GIT_TAG bf71a834948186f4097caa076cd2663c69a10e1e # main 0.9.9.8
)
FetchContent_Declare(
    stb
    GIT_REPOSITORY "https://github.com/nothings/stb"
    GIT_TAG 8b5f1f37b5b75829fc72d38e7b5d4bcbf8a26d55 # main 9/09/2022
)
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY "https://github.com/tinyobjloader/tinyobjloader"
    GIT_TAG f48bd0bfb9b00ed1f5b13dbcdbd7909ca8bd49b5 # main v2.0.0rc10
)
FetchContent_MakeAvailable(glfw glm stb tinyobjloader)

# Add interface library for dependencies
add_library(dependencies INTERFACE)

# Add Vulkan to interface library
find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)
target_link_libraries(dependencies INTERFACE
    Vulkan::Vulkan
    Vulkan::shaderc_combined
)
target_include_directories(dependencies SYSTEM INTERFACE
    ${Vulkan_INCLUDE_DIR}
)

# Build GLFW library and link to interface library
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
target_link_libraries(dependencies INTERFACE
    glfw
)

# Add GLM includes to interface library
target_include_directories(dependencies SYSTEM INTERFACE
    ${glm_SOURCE_DIR}
)

# Add stb includes to interface library
target_include_directories(dependencies SYSTEM INTERFACE
    ${stb_SOURCE_DIR}
)

# Add tinyobjloader includes to interface library
target_include_directories(dependencies SYSTEM INTERFACE
    ${tinyobjloader_SOURCE_DIR}
)
