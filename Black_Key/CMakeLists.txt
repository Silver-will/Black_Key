# CMakeList.txt : CMake project for A-Ray, include source and define
# project specific logic here.
#

file(GLOB_RECURSE SRC_FILES
    include/*.h 
    include/*.cpp
    src/*.cpp)

# Add source to this project's executable.
add_executable ("Black_Key" "main.cpp" ${SRC_FILES})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Black_Key PROPERTY CXX_STANDARD 23)
endif()

# TODO: Add tests and install targets if needed.
