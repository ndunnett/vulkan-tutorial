# setup project
cmake_minimum_required(VERSION 3.24)
project("vulkan-tutorial")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(MAIN_DIR ${CMAKE_CURRENT_LIST_DIR})

# load dependencies
include(${MAIN_DIR}/cmake/dependencies.cmake)

# load tests
include(CTest)
include(${MAIN_DIR}/cmake/tests.cmake)

# list of tests (each test must have corresponding test.cpp)
test_sources(
    environment
)

# build executable
add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE
    ${MAIN_DIR}/source/main.cpp
    ${MAIN_DIR}/source/renderer/core.cpp
    ${MAIN_DIR}/source/renderer/helpers.cpp
    ${MAIN_DIR}/source/renderer/window.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE dependencies)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
