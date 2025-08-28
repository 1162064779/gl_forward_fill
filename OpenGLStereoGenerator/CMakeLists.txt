cmake_minimum_required(VERSION 3.20)
project(OpenGLStereoGenerator LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1) 依赖（通过 vcpkg）
#    - glfw3: 窗口/上下文（隐藏窗口也可以）
#    - glad:  OpenGL 加载器
#    - opengl: 桌面 OpenGL（Windows 下映射到 opengl32）
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# 2) stb（header-only）
#    vcpkg 的 stb 是头文件包，这里仅定位头目录
find_path(STB_INCLUDE_DIRS "stb_image.h")
if (NOT STB_INCLUDE_DIRS)
    message(FATAL_ERROR "Could not find stb headers (stb_image.h). Make sure vcpkg installed 'stb'.")
endif()

add_executable(${PROJECT_NAME}
    main.cpp
)

# 3) MSVC 常见宏
if (MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# 4) 头文件包含
target_include_directories(${PROJECT_NAME} PRIVATE
    ${STB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 5) 编译定义
#    使用 GLAD 加载 OpenGL，避免 GLFW 自带包含 gl.h
target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)

# 6) 链接库：glad、glfw、OpenGL
#    vcpkg 下 glad 的目标一般是 glad::glad；glfw3 的目标为 glfw
target_link_libraries(${PROJECT_NAME} PRIVATE
    glad::glad
    glfw
    OpenGL::GL
)

# 7) 资源与着色器拷贝（只在文件变化时拷贝）
file(GLOB_RECURSE SHADER_FILES "${CMAKE_SOURCE_DIR}/shaders/*")
file(GLOB_RECURSE ASSET_FILES "${CMAKE_SOURCE_DIR}/assets/*")

add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
    DEPENDS ${PROJECT_NAME} ${SHADER_FILES}
    COMMENT "Copying shaders to build directory"
)

add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    DEPENDS ${PROJECT_NAME} ${ASSET_FILES}
    COMMENT "Copying assets to build directory"
)

# 8) 运行时拷贝 DLL（如果用 vcpkg 的二进制包，拷贝 bin 到执行目录，简单粗暴但有效）
if (DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()