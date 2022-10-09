# setup project
cmake_minimum_required(VERSION 3.24)
project("vulkan-tutorial")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(MAIN_DIR ${CMAKE_CURRENT_LIST_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)

# set output directory per config
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
endforeach()

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
    ${MAIN_DIR}/source/renderer/resources.cpp
    ${MAIN_DIR}/source/renderer/window.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE dependencies)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# copy assets
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)