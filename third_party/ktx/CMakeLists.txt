cmake_minimum_required(VERSION 3.15...4.0)

# Create an INTERFACE wrapper target
add_library(ktx INTERFACE)

# Include headers
target_include_directories(ktx INTERFACE "${CMAKE_CURRENT_LIST_DIR}/include")

target_link_libraries(ktx INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/lib/ktx.lib"
)