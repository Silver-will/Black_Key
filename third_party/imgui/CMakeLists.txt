cmake_minimum_required(VERSION 3.15...4.0)

file(GLOB MYLIB_SOURCES CONFIGURE_DEPENDS
    "include/imgui/*.cpp"
    "include/imgui/*.h"
)

add_library(imgui STATIC ${MYLIB_SOURCES})

target_link_libraries(imgui PRIVATE 
                            glfw
                            Vulkan::Vulkan)
target_include_directories(imgui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
${Vulkan_INCLUDE_DIRS})