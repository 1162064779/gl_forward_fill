#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>

// 性能测试工具
class PerformanceProfiler {
private:
    std::vector<std::pair<std::string, double>> timings;
    std::chrono::high_resolution_clock::time_point startTime;

public:
    void start() {
        startTime = std::chrono::high_resolution_clock::now();
    }
    
    void record(const std::string& name) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        timings.push_back({name, duration.count() / 1000.0}); // 转换为毫秒
        startTime = endTime;
    }
    
    void printReport() {
        std::cout << "=== Performance Analysis Report ===" << std::endl;
        double totalTime = 0;
        for (const auto& timing : timings) {
            std::cout << timing.first << ": " << timing.second << " ms" << std::endl;
            totalTime += timing.second;
        }
        std::cout << "Total Time: " << totalTime << " ms" << std::endl;
        std::cout << "Processing Speed: " << (1000.0 / totalTime) << " fps" << std::endl;
    }
};

// 着色器编译工具
GLuint compileShader(GLenum type, const std::string &source) {
    GLuint shader = glCreateShader(type);
    const char *src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader Compilation Failed:" << infoLog << std::endl;
    }

    return shader;
}

std::string loadFile(const char *path) {
    std::ifstream file(path);
    std::stringstream ss;
    ss << file.rdbuf();
    std::string code = ss.str();
    std::cout << "[DEBUG] Loaded shader " << path << ", length: " << code.size() << std::endl;
    return code;
}

GLuint createComputeProgram(const char *path) {
    std::string code = loadFile(path);
    GLuint cs = compileShader(GL_COMPUTE_SHADER, code);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, cs);
    glLinkProgram(prog);

    GLint success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(prog, 512, nullptr, infoLog);
        std::cerr << "Compute Shader Linking Failed:" << infoLog << std::endl;
    }

    glDeleteShader(cs);
    return prog;
}

// 纹理加载和保存工具
GLuint loadTextureFromPNG(const char *path, int &width, int &height) {
    int channels;
    unsigned char *data = stbi_load(path, &width, &height, &channels, 3);
    if (!data) {
        std::cerr << "Failed to load image: " << path << std::endl;
        return 0;
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return texID;
}

GLuint loadDepthFromEXR(const char *path, int &width, int &height) {
    EXRVersion exr_version;
    int ret = ParseEXRVersionFromFile(&exr_version, path);
    if (ret != 0) {
        std::cerr << "Invalid EXR file: " << path << std::endl;
        return 0;
    }

    if (exr_version.multipart) {
        std::cerr << "Multipart EXR not supported.";
        return 0;
    }

    EXRHeader exr_header;
    InitEXRHeader(&exr_header);
    const char *err = nullptr;

    ret = ParseEXRHeaderFromFile(&exr_header, &exr_version, path, &err);
    if (ret != 0) {
        std::cerr << "Parse EXR err: " << (err ? err : "unknown") << std::endl;
        FreeEXRErrorMessage(err);
        return 0;
    }

    for (int i = 0; i < exr_header.num_channels; ++i) {
        if (exr_header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF) {
            exr_header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
        }
    }

    EXRImage exr_image;
    InitEXRImage(&exr_image);

    ret = LoadEXRImageFromFile(&exr_image, &exr_header, path, &err);
    if (ret != 0) {
        std::cerr << "Load EXR err: " << (err ? err : "unknown") << std::endl;
        FreeEXRHeader(&exr_header);
        FreeEXRErrorMessage(err);
        return 0;
    }

    width = exr_image.width;
    height = exr_image.height;

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, exr_image.images[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    FreeEXRImage(&exr_image);
    FreeEXRHeader(&exr_header);
    return texID;
}

void saveTexturePNG(GLuint tex, int w, int h, const char *name) {
    std::vector<unsigned char> buf(w * h * 3);
    glBindTexture(GL_TEXTURE_2D, tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, buf.data());
    stbi_write_png(name, w, h, 3, buf.data(), w * 3);
    std::cout << "Saved: " << name << std::endl;
}

// 创建离屏渲染上下文
bool createOffscreenContext() {
    if (!glfwInit()) {
        std::cerr << "GLFW Init Failed";
        return false;
    }

    // 设置离屏渲染提示
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 创建1x1的隐藏窗口（最小化资源使用）
    GLFWwindow *window = glfwCreateWindow(1, 1, "Offscreen Renderer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create offscreen context";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD Load Failed";
        glfwDestroyWindow(window);
        glfwTerminate();
        return false;
    }

    // 打印系统信息
    std::cout << "=== System Information ===" << std::endl;
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GPU: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    // 查询计算着色器限制
    GLint maxComputeWorkGroupSize[3];
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxComputeWorkGroupSize[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxComputeWorkGroupSize[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &maxComputeWorkGroupSize[2]);
    
    std::cout << "Max Work Group Size: " << maxComputeWorkGroupSize[0] << "x" 
              << maxComputeWorkGroupSize[1] << "x" << maxComputeWorkGroupSize[2] << std::endl;

    return true;
}

int main() {
    PerformanceProfiler profiler;
    profiler.start();

    // 创建离屏渲染上下文
    if (!createOffscreenContext()) {
        return -1;
    }
    profiler.record("Context Creation");

    // 获取图像尺寸
    int imageW, imageH, channels;
    unsigned char *info_data = stbi_load("image.png", &imageW, &imageH, &channels, 0);
    if (info_data) {
        stbi_image_free(info_data);
        std::cout << "Image Size: " << imageW << "x" << imageH << std::endl;
    } else {
        std::cout << "Cannot get image info, using default size: 1920x1080" << std::endl;
        imageW = 1920;
        imageH = 1080;
    }
    profiler.record("Image Info Loading");

    // 加载纹理
    GLuint imageTex = loadTextureFromPNG("image.png", imageW, imageH);
    std::cout << "Loaded image.png: " << imageW << "x" << imageH << std::endl;

    int depthW, depthH;
    GLuint depthTex = loadDepthFromEXR("depth.exr", depthW, depthH);
    if (depthTex == 0) {
        std::cerr << "Depth texture loading failed";
        return -1;
    } else {
        std::cout << "Loaded depth.exr: " << depthW << "x" << depthH << std::endl;
    }
    profiler.record("Texture Loading");

    // 参数设置
    const float divergence = 2.0f;
    const float convergence = 0.0f;
    int padSize = int(imageW * divergence * 0.01f + 2);
    int paddedW = imageW + padSize * 2;

    // 全局尺寸
    const int TILE_W = 256;
    int numTile = (imageW + TILE_W - 1) / TILE_W;
    int edgeW = numTile * 2;

    // 创建目标纹理
    GLuint leftColor, leftDepth, leftIndex, leftEdge;
    GLuint rightColor, rightDepth, rightIndex, rightEdge;

    auto makeTarget4 = [&](GLuint &color, GLuint &depth, GLuint &index, GLuint &edge) {
        glGenTextures(1, &color);
        glBindTexture(GL_TEXTURE_2D, color);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, imageW, imageH);

        glGenTextures(1, &depth);
        glBindTexture(GL_TEXTURE_2D, depth);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, imageW, imageH);

        glGenTextures(1, &index);
        glBindTexture(GL_TEXTURE_2D, index);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, imageW, imageH);

        glGenTextures(1, &edge);
        glBindTexture(GL_TEXTURE_2D, edge);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32UI, edgeW, imageH);

        // 初始化纹理
        std::vector<uint8_t> clrInit(imageW * imageH * 4, 0);
        std::vector<uint32_t> u32Zero(imageW * imageH, 0);
        std::vector<uint32_t> u32Undef(imageW * imageH, 0xFFFFFFFFu);
        std::vector<uint32_t> edgeZero(edgeW * imageH * 4, 0);

        glBindTexture(GL_TEXTURE_2D, color);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imageW, imageH, GL_RGBA, GL_UNSIGNED_BYTE, clrInit.data());

        glBindTexture(GL_TEXTURE_2D, depth);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imageW, imageH, GL_RED_INTEGER, GL_UNSIGNED_INT, u32Zero.data());

        glBindTexture(GL_TEXTURE_2D, index);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imageW, imageH, GL_RED_INTEGER, GL_UNSIGNED_INT, u32Undef.data());

        glBindTexture(GL_TEXTURE_2D, edge);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, edgeW, imageH, GL_RGBA_INTEGER, GL_UNSIGNED_INT, edgeZero.data());
    };

    makeTarget4(leftColor, leftDepth, leftIndex, leftEdge);
    makeTarget4(rightColor, rightDepth, rightIndex, rightEdge);
    profiler.record("Target Texture Creation");

    // 编译着色器
    GLuint warpProg = createComputeProgram("warp.comp");
    GLuint tileProg = createComputeProgram("fill_tile.comp");
    GLuint prefixProg = createComputeProgram("fill_prefix.comp");
    profiler.record("Shader Compilation");

    // Warp阶段
    auto warpEye = [&](GLuint dstC, GLuint dstD, GLuint dstI, int eyeSign) {
        glUseProgram(warpProg);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, imageTex);
        glUniform1i(glGetUniformLocation(warpProg, "srcColor"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glUniform1i(glGetUniformLocation(warpProg, "srcDepth"), 1);

        glBindImageTexture(2, dstC, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
        glBindImageTexture(3, dstD, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
        glBindImageTexture(4, dstI, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

        float shiftScale = divergence * 0.01f * imageW * 0.5f * eyeSign;
        float shiftBias = -convergence * shiftScale;

        glUniform1i(glGetUniformLocation(warpProg, "orgWidth"), imageW);
        glUniform1i(glGetUniformLocation(warpProg, "orgHeight"), imageH);
        glUniform1i(glGetUniformLocation(warpProg, "padSize"), padSize);
        glUniform1i(glGetUniformLocation(warpProg, "paddedWidth"), paddedW);
        glUniform1f(glGetUniformLocation(warpProg, "shiftScale"), shiftScale);
        glUniform1f(glGetUniformLocation(warpProg, "shiftBias"), shiftBias);

        GLuint gx = (paddedW + 15) / 16;
        GLuint gy = (imageH + 15) / 16;
        glDispatchCompute(gx, gy, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    };

    warpEye(leftColor, leftDepth, leftIndex, +1);
    warpEye(rightColor, rightDepth, rightIndex, -1);
    profiler.record("Warp Stage");

    // Fill阶段
    auto fillEye = [&](GLuint color, GLuint index, GLuint edge, int eyeSign) {
        glUseProgram(tileProg);
        glBindImageTexture(2, color, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(4, index, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
        glBindImageTexture(5, edge, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32UI);

        glUniform1i(glGetUniformLocation(tileProg, "orgWidth"), imageW);
        glUniform1i(glGetUniformLocation(tileProg, "orgHeight"), imageH);
        glUniform1i(glGetUniformLocation(tileProg, "eyeSign"), eyeSign);

        glDispatchCompute(numTile, imageH, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glUseProgram(prefixProg);
        glBindImageTexture(2, color, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(4, index, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
        glBindImageTexture(5, edge, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32UI);

        glUniform1i(glGetUniformLocation(prefixProg, "orgWidth"), imageW);
        glUniform1i(glGetUniformLocation(prefixProg, "orgHeight"), imageH);
        glUniform1i(glGetUniformLocation(prefixProg, "numTile"), numTile);

        glDispatchCompute(1, imageH, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    };

    fillEye(leftColor, leftIndex, leftEdge, +1);
    fillEye(rightColor, rightIndex, rightEdge, -1);
    profiler.record("Fill Stage");

    // 保存结果
    saveTexturePNG(leftColor, imageW, imageH, "left_eye_filled.png");
    saveTexturePNG(rightColor, imageW, imageH, "right_eye_filled.png");
    profiler.record("Result Saving");

    // 清理资源
    glDeleteTextures(1, &imageTex);
    glDeleteTextures(1, &depthTex);
    glDeleteTextures(1, &leftColor);
    glDeleteTextures(1, &leftDepth);
    glDeleteTextures(1, &leftIndex);
    glDeleteTextures(1, &leftEdge);
    glDeleteTextures(1, &rightColor);
    glDeleteTextures(1, &rightDepth);
    glDeleteTextures(1, &rightIndex);
    glDeleteTextures(1, &rightEdge);
    glDeleteProgram(warpProg);
    glDeleteProgram(tileProg);
    glDeleteProgram(prefixProg);

    glfwTerminate();
    profiler.record("Resource Cleanup");

    // 打印性能报告
    profiler.printReport();
    
    std::cout << "Stereo image generation completed!" << std::endl;
    return 0;
} 