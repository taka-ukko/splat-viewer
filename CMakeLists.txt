cmake_minimum_required(VERSION 3.12)
project(splatRenderer VERSION 0.1.0 LANGUAGES C CXX)


find_package(OpenGL REQUIRED)

# vcpkg dependencies
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Stb REQUIRED)

# Add GLAD source and include paths
add_library(glad STATIC src/glad.c)
target_include_directories(glad PUBLIC include)

add_executable(splatRenderer src/main.cpp)

target_link_libraries(splatRenderer PRIVATE glfw glm::glm-header-only imgui::imgui glad)

target_include_directories(splatRenderer PRIVATE ${Stb_INCLUDE_DIR})


