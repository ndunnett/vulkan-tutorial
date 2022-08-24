cmake_minimum_required(VERSION 3.24)
include(CTest)
include(FetchContent)

# interface library
add_library(test_interface INTERFACE)
FetchContent_MakeAvailable(doctest_repo)
target_include_directories(test_interface INTERFACE ${doctest_repo_SOURCE_DIR}/doctest)
target_link_libraries(test_interface INTERFACE dependencies)
target_compile_features(test_interface INTERFACE cxx_std_17)

# main object
add_library(test_main OBJECT ${MAIN_DIR}/tests/main.cpp)
target_link_libraries(test_main PUBLIC test_interface)

# add executable, build on main object, link to interface, add test
function(test_sources)
    foreach(arg IN LISTS ARGN)
        add_executable(test_${arg} ${MAIN_DIR}/tests/${arg}.cpp $<TARGET_OBJECTS:test_main>)
        target_link_libraries(test_${arg} PUBLIC test_interface)
        add_test(NAME ${arg} COMMAND test_${arg})
    endforeach()
endfunction()
