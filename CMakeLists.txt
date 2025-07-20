cmake_minimum_required(VERSION 3.15...4.0)

project(BlackKey VERSION 1.0
                 DESCRIPTION "A modern rendering engine in vulkan"
                 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vulkan SDK
find_package(Vulkan REQUIRED)        

#add submodules
add_subdirectory(third_party/fmt)
add_subdirectory(third_party/glfw)
add_subdirectory(third_party/glm)
add_subdirectory(third_party/tracy)
add_subdirectory(third_party/vk-bootstrap)
add_subdirectory(third_party/fastgltf)
add_subdirectory(third_party/imgui)
add_subdirectory(third_party/vma)
add_subdirectory(third_party/stb)
add_subdirectory(third_party/ktx)

file(GLOB_RECURSE SRC_FILES
    include/*.h 
    include/*.cpp
    src/*.cpp)

add_executable(${PROJECT_NAME} ${SRC_FILES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    include/
    src/
    third_party/vma/include
    third_party/vma/src
    third_party/imgui/include
    third_party/glm
    third_party/fastgltf/include
    third_party/glfw/include
    third_party/vk-bootstrap/src
    third_party/fmt/include
    third_party/fmt/src
    third_party/stb
    third_party/ktx/include
    ${Vulkan_INCLUDE_DIRS}
)
#   third_party/VulkanMemoryAllocator/include

# Link dependencies
target_link_libraries(${PROJECT_NAME}
    stb
    imgui
    fastgltf
    glfw
    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
    ktx
    fmt-header-only
)

# Platform-specific (optional: enable this if needed)
if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS GLM_ENABLE_EXPERIMENTAL)
    target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()