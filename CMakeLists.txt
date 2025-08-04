# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.15)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()


project ("A-Ray")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vulkan SDK
find_package(Vulkan REQUIRED)    



#set(KTX_FEATURE_LOADTEST_APPS OFF CACHE BOOL "" FORCE)
#set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
#set(KTX_FEATURE_ASTC=OFF)
#set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
#set(KTX_FEATURE_STATIC_LIBRARY OFF CACHE BOOL "" FORCE)
#set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)


# Include sub-projects.
add_subdirectory ("A-Ray")
add_subdirectory ("third_party/fastgltf")
add_subdirectory ("third_party/glm")
add_subdirectory ("third_party/glfw")
add_subdirectory ("third_party/imgui")
add_subdirectory ("third_party/stb")
add_subdirectory ("third_party/vma")
add_subdirectory ("third_party/tracy")
add_subdirectory ("third_party/vk-bootstrap")
#add_subdirectory ("third_party/fmt")
#add_library(fmt::fmt ALIAS fmt)
#find_package(fmt REQUIRED)

option(MATCH_SUPPORT_KTX "Support KTX" ON)
if (MATCH_SUPPORT_KTX)
 option(KTX_FEATURE_TOOLS "Create KTX tools" OFF )
 option(KTX_FEATURE_DOC "Create KTX documentation." OFF )
 option(KTX_FEATURE_JNI "Create Java bindings for libktx." OFF )
 option(KTX_FEATURE_PY "Create Python source distribution." OFF )
 option(KTX_FEATURE_TESTS "Create unit tests." OFF )
 option(KTX_FEATURE_TOOLS_CTS "Enable KTX CLI Tools CTS tests (requires CTS submodule)." OFF )
 option(KTX_FEATURE_KTX1 "Enable KTX 1 support." ON )
 option(KTX_FEATURE_KTX2 "Enable KTX 2 support." ON )
 option(KTX_FEATURE_VK_UPLOAD "Enable Vulkan texture upload." ON )
 option(KTX_FEATURE_GL_UPLOAD "Enable OpenGL texture upload." OFF )
 option(KTX_FEATURE_STATIC_LIBRARY "Create static libraries (shared otherwise)." ON )
  if (WIN32)
        if (NOT DEFINED BASH_EXECUTABLE)
            set(BASH_EXECUTABLE "bash.exe")
        endif()
    endif()

add_subdirectory ("third_party")

endif()
# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    A-Ray/
    A-Ray/include/
    A-Ray/src/
    third_party/vma/include
    third_party/vma
    third_party/vma/src
    third_party/glfw/include
    third_party/glm
    third_party/fastgltf/include
    third_party/imgui/include
    third_party/vk-bootstrap/src
    third_party/stb
    third_party/ktx/include
    third_party/ktx/lib
    third_party/ktx/external
    third_party/tracy/public/tracy
    third_party/fmt/include
    third_party/fmt/src
    ${Vulkan_INCLUDE_DIRS}
)

set(VULKAN_LIBRARY "$ENV{VK_SDK_PATH}/Lib/vulkan-1.lib")
target_compile_definitions(${PROJECT_NAME} PRIVATE
                            ENGINE_ASSET_PATH="${CMAKE_CURRENT_LIST_DIR}/assets")

target_link_libraries(${PROJECT_NAME}
    stb
    imgui
    fastgltf
    glfw
    ${VULKAN_LIBRARY}
    vk-bootstrap::vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
    ktx
)

#add_custom_target(assets COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_LIST_DIR}/assets.cmake)
#add_dependencies(${PROJECT_NAME} assets)