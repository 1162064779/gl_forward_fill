cmake_minimum_required(VERSION 3.10)
project(OpenGLStereoGenerator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



# 设置源文件
set(SOURCES
    offscreen_main.cpp
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 查找并链接依赖（vcpkg会自动处理）
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    glad::glad
)

# 设置编译选项
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/warp.comp
        ${CMAKE_SOURCE_DIR}/fill_tile.comp
        ${CMAKE_SOURCE_DIR}/fill_prefix.comp
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
)