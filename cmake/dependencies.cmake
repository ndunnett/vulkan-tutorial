cmake_minimum_required(VERSION 3.24)
include(FetchContent)

# fetch repos
FetchContent_Declare(
    glfw_repo
    GIT_REPOSITORY "https://github.com/glfw/glfw"
    GIT_TAG 7482de6071d21db77a7236155da44c172a7f6c9e # main 3.3.8
)
FetchContent_Declare(
    glm_repo
    GIT_REPOSITORY "https://github.com/g-truc/glm"
    GIT_TAG bf71a834948186f4097caa076cd2663c69a10e1e # main 0.9.9.8
)
FetchContent_Declare(
    stb_repo
    GIT_REPOSITORY "https://github.com/nothings/stb"
    GIT_TAG af1a5bc352164740c1cc1354942b1c6b72eacb8a # main 10/09/2021
)
FetchContent_Declare(
    tinyobjloader_repo
    GIT_REPOSITORY "https://github.com/tinyobjloader/tinyobjloader"
    GIT_TAG 2636244e73fa424bb68da6c0ae38f22d15d2f7c6 # main v2.0.0-rc4
)
FetchContent_Declare(
    doctest_repo
    GIT_REPOSITORY "https://github.com/doctest/doctest"
    GIT_TAG b7c21ec5ceeadb4951b00396fc1e4642dd347e5f # main v2.4.9
)

# add interface library for dependencies
add_library(dependencies INTERFACE)
target_compile_options(dependencies INTERFACE -Wno-deprecated-volatile)

# Add Vulkan to interface library
find_package(Vulkan REQUIRED)
target_link_libraries(dependencies INTERFACE
    Vulkan::Vulkan
)
target_include_directories(dependencies SYSTEM INTERFACE
    ${Vulkan_INCLUDE_DIR}
)

# Build GLM library and link to interface library
FetchContent_MakeAvailable(glm_repo)
include_directories(${glm_repo_SOURCE_DIR})
target_link_libraries(dependencies INTERFACE
    glm
)

# Add stb includes to interface library
FetchContent_MakeAvailable(stb_repo)
target_include_directories(dependencies SYSTEM INTERFACE
    ${stb_repo_SOURCE_DIR}
)

# Add tinyobjloader includes to interface library
FetchContent_MakeAvailable(tinyobjloader_repo)
target_include_directories(dependencies SYSTEM INTERFACE
    ${tinyobjloader_repo_SOURCE_DIR}
)

# Build GLFW library and link to interface library
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw_repo)
include_directories(${glfw_repo_SOURCE_DIR})
target_link_libraries(dependencies INTERFACE
    glfw
)
