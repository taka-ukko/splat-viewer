cmake_minimum_required(VERSION 3.12)
project(splatRenderer VERSION 0.1.0 LANGUAGES C CXX)


find_package(OpenGL REQUIRED)

# vcpkg dependencies
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Stb REQUIRED)

# Add GLAD source and include paths
add_library(glad STATIC src/third_party/glad.c)
target_include_directories(glad PUBLIC third_party)


add_executable(splatRenderer 
    src/main.cpp
    src/third_party/miniply.cpp
)

target_link_libraries(splatRenderer PRIVATE 
    glfw 
    glm::glm-header-only 
    imgui::imgui 
    glad
)

target_include_directories(splatRenderer PRIVATE 
    ${Stb_INCLUDE_DIR} 
    ${CMAKE_CURRENT_SOURCE_DIR}/src 
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# copy all shader files to the build directory
# --------------------------------------------
file(GLOB SHADER_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/resources/shaders/*")

# Iterate over each shader file and configure it
foreach(SHADER_FILE ${SHADER_FILES})
    # Get the filename without the directory
    get_filename_component(SHADER_FILENAME ${SHADER_FILE} NAME)

    # Configure the file to the build directory
    configure_file(${SHADER_FILE} ${CMAKE_BINARY_DIR}/resources/shaders/${SHADER_FILENAME} COPYONLY)
endforeach()

# copy all ply models to the build directory
# ------------------------------------------
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources/models/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/resources/models)


