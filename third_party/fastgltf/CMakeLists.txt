cmake_minimum_required(VERSION 3.15...4.0)

# Create an INTERFACE wrapper target
add_library(fastgltf INTERFACE)

# Include headers
target_include_directories(fastgltf INTERFACE "${CMAKE_CURRENT_LIST_DIR}/include")


# Pick the correct libs based on build type
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(fastgltf INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/lib/debug/fastgltf.lib"
        "${CMAKE_CURRENT_LIST_DIR}/lib/debug/fastgltf_simdjson.lib"
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "Release" OR
       CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR
       CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    target_link_libraries(fastgltf INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/lib/release/fastgltf.lib"
        "${CMAKE_CURRENT_LIST_DIR}/lib/release/fastgltf_simdjson.lib"
    )
endif()