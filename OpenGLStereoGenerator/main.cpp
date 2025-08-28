#include <glad/glad.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
  #include <windows.h>
#endif

// OpenGL loader and window/context
#include <GLFW/glfw3.h>

#define LOGI(...) do { std::printf(__VA_ARGS__); std::printf("\n"); } while(0)
#define LOGE(...) do { std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); } while(0)


/* ---------------- STB 实现定义（仅此处） ------------- */
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

/* ------------------------- 视口尺寸 --------------------------- */
int windowWidth = 1920;
int windowHeight = 1080;

/* ---------------------- Uniform 位置缓存 ---------------------- */
struct WarpUniforms {
    GLint orgWidth, orgHeight, padSizeX, padSizeY, paddedWidth, paddedHeight;
    GLint shiftScaleX, shiftBiasX, shiftScaleY, shiftBiasY;
} warpU;

struct TileUniforms {
    GLint orgWidth, orgHeight, eyeSign;
} tileU;

/* ----------------------- 工具：读文件 ------------------------- */
static std::string loadFile(const char *path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        LOGE("Failed to open file: %s", path);
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string code = ss.str();
    LOGI("Loaded file %s, length: %zu", path, code.size());
    return code;
}

/* ----------------------- 编译/链接着色器 ---------------------- */
static GLuint compileShader(GLenum type, const std::string &source) {
    GLuint shader = glCreateShader(type);
    const char *src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        LOGE("Shader compile failed: %s", log);
    }
    return shader;
}

static GLuint createComputeProgram(const char *path) {
    std::string code = loadFile(path);
    if (code.empty()) return 0;
    GLuint cs = compileShader(GL_COMPUTE_SHADER, code);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, cs);
    glLinkProgram(prog);
    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        LOGE("Program link failed: %s", log);
    }
    glDeleteShader(cs);
    return prog;
}

/* ----------------------- 纹理加载/保存 ------------------------ */
static GLuint loadTextureFromPNG(const char *path, int &width, int &height) {
    int channels = 0;
    unsigned char *data = stbi_load(path, &width, &height, &channels, 4);
    if (!data) {
        LOGE("stbi_load failed: %s", path);
        return 0;
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    return tex;
}

static void saveTexturePNG(GLuint tex, int w, int h, const char *name) {
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex, 0);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("FBO incomplete: 0x%04X", st);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        return;
    }
    std::vector<unsigned char> rgba(w * h * 4);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);

    // 转成 RGB 存
    std::vector<unsigned char> rgb(w * h * 3);
    for (int i = 0; i < w * h; ++i) {
        rgb[i*3+0] = rgba[i*4+0];
        rgb[i*3+1] = rgba[i*4+1];
        rgb[i*3+2] = rgba[i*4+2];
    }
    stbi_write_png(name, w, h, 3, rgb.data(), w * 3);
    LOGI("Saved: %s", name);
}

static void saveRGBATexturePNG(GLuint tex, int w, int h, const char *name) {
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex, 0);
    std::vector<unsigned char> rgba(w*h*4);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    stbi_write_png(name, w, h, 4, rgba.data(), w*4);
    LOGI("Saved RGBA: %s", name);
}

/* ------------------------- 纹理清空 --------------------------- */
static void clearTextureRGBA8(GLuint tex, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0) {
    static GLuint sFBO = 0;
    if (!sFBO) glGenFramebuffers(1, &sFBO);
    const GLfloat v[4] = { r/255.f, g/255.f, b/255.f, a/255.f };
    glBindFramebuffer(GL_FRAMEBUFFER, sFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glClearBufferfv(GL_COLOR, 0, v);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void clearTextureR32UI(GLuint tex, uint32_t v0) {
    static GLuint sFBO = 0;
    if (!sFBO) glGenFramebuffers(1, &sFBO);
    const GLuint v[4] = { v0, 0u, 0u, 0u };
    glBindFramebuffer(GL_FRAMEBUFFER, sFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glClearBufferuiv(GL_COLOR, 0, v);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/* ----------------------- OpenGL 上下文初始化 ------------------ */
static GLFWwindow* gWindow = nullptr;

static bool initOpenGL43Core(bool visibleWindow = false) {
    if (!glfwInit()) {
        LOGE("glfwInit failed");
        return false;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, visibleWindow ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 0);
#ifdef _DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    gWindow = glfwCreateWindow(64, 64, "GL43ComputeHidden", nullptr, nullptr);
    if (!gWindow) {
        LOGE("glfwCreateWindow failed (need OpenGL 4.3 core).");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(gWindow);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOGE("gladLoadGLLoader failed");
        glfwDestroyWindow(gWindow);
        glfwTerminate();
        return false;
    }

    const char* glVer = (const char*)glGetString(GL_VERSION);
    const char* glslVer = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    LOGI("OpenGL Version: %s", glVer ? glVer : "(null)");
    LOGI("GLSL Version:   %s", glslVer ? glslVer : "(null)");
    LOGI("Renderer:       %s", renderer ? renderer : "(null)");

    // 基本版本检查：4.3+ 才有 compute 和 imageAtomicMax（核心）
    GLint maj = 0, min = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &maj);
    glGetIntegerv(GL_MINOR_VERSION, &min);
    if (maj < 4 || (maj == 4 && min < 3)) {
        LOGE("Need OpenGL 4.3+ (got %d.%d)", maj, min);
        return false;
    }
    return true;
}

static void cleanupOpenGL() {
    if (gWindow) {
        glfwDestroyWindow(gWindow);
        gWindow = nullptr;
    }
    glfwTerminate();
}

/* ============================== 主函数 ============================== */
int main() {
    using namespace std::chrono;
    auto startTime = steady_clock::now();
    LOGI("xptest: Starting OpenGL 4.3 Stereo Generator (Desktop GL)");

    if (!initOpenGL43Core(false)) {
        LOGE("Failed to initialize OpenGL 4.3 core context.");
        return -1;
    }

    // 查询 compute 限制
    GLint maxWGCount[3] = {0}, maxWGSize[3] = {0}, maxWGInvoc = 0;
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maxWGCount[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &maxWGCount[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &maxWGCount[2]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE , 0, &maxWGSize[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE , 1, &maxWGSize[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE , 2, &maxWGSize[2]);
    glGetIntegerv (GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxWGInvoc);
    LOGI("Max WG count: %d x %d x %d", maxWGCount[0], maxWGCount[1], maxWGCount[2]);
    LOGI("Max WG size : %d x %d x %d", maxWGSize [0], maxWGSize [1], maxWGSize [2]);
    LOGI("Max WG invocations: %d", maxWGInvoc);

    // 读取图片尺寸（用于日志/可选）
    int imageW=0, imageH=0, channels=0;
    unsigned char *info_data = stbi_load("assets/output.png", &imageW, &imageH, &channels, 0);
    if (info_data) {
        windowWidth = imageW; windowHeight = imageH;
        stbi_image_free(info_data);
        LOGI("Image dimensions: %dx%d", imageW, imageH);
    } else {
        LOGI("Failed to get image info, use default: %dx%d", windowWidth, windowHeight);
    }

    // 加载图像（SBS：左幅在 [0..W-1]，右幅/深度编码在 [W..2W-1]）
    GLuint imageTex = loadTextureFromPNG("assets/output.png", imageW, imageH);
    if (!imageTex) {
        LOGE("Failed to load assets/output.png");
        cleanupOpenGL();
        return -1;
    }
    LOGI("Loaded output.png: %dx%d", imageW, imageH);
    imageW = imageW / 2;

    // 立体参数
    const float divergence = 2.0f;
    const float convergence = 0.0f;
    int padSizeX = int(imageW * divergence * 0.01f + 2);
    int padSizeY = int(imageH * divergence * 0.01f + 2);
    int paddedW  = imageW + padSizeX * 2;
    int paddedH  = imageH + padSizeY * 2;

    const int TILE_W = 256;

    // 目标纹理
    auto allocTex2D = [](GLuint &id, GLenum internalFmt, int w, int h) {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexStorage2D(GL_TEXTURE_2D, 1, internalFmt, w, h);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    };

    GLuint leftColor=0, leftDepth=0, leftIndex=0;
    GLuint rightColor=0, rightDepth=0, rightIndex=0;

    auto makeTarget3 = [&](GLuint &color, GLuint &depth, GLuint &index) {
        allocTex2D(color, GL_RGBA8, imageW, imageH);
        allocTex2D(depth, GL_R32UI, imageW, imageH);
        allocTex2D(index, GL_R32UI, imageW, imageH);
        clearTextureRGBA8(color, 255,255,255,255);
        clearTextureR32UI (depth, 0u);
        clearTextureR32UI (index, 0xFFFFFFFFu);
    };

    auto tMake = steady_clock::now();
    makeTarget3(leftColor, leftDepth, leftIndex);
    makeTarget3(rightColor, rightDepth, rightIndex);
    LOGI("Targets created in %.2f ms",
         duration_cast<milliseconds>(steady_clock::now()-tMake).count()/1.0);

    // 编译 compute shader（请确保你的 .comp 文件是 GLSL 430，而不是 ESSL）
    auto tShader = steady_clock::now();
    GLuint warpProg = createComputeProgram("shaders/warp_gl.comp");      // 建议改名并用 #version 430
    GLuint tileProg = createComputeProgram("shaders/fill_tile_gl.comp"); // 建议改名并用 #version 430
    if (!warpProg || !tileProg) {
        LOGE("Failed to create compute programs");
        cleanupOpenGL();
        return -1;
    }

    // 缓存 uniform 位置
    warpU.orgWidth     = glGetUniformLocation(warpProg, "orgWidth");
    warpU.orgHeight    = glGetUniformLocation(warpProg, "orgHeight");
    warpU.padSizeX     = glGetUniformLocation(warpProg, "padSizeX");
    warpU.padSizeY     = glGetUniformLocation(warpProg, "padSizeY");
    warpU.paddedWidth  = glGetUniformLocation(warpProg, "paddedWidth");
    warpU.paddedHeight = glGetUniformLocation(warpProg, "paddedHeight");
    warpU.shiftScaleX  = glGetUniformLocation(warpProg, "shiftScaleX");
    warpU.shiftBiasX   = glGetUniformLocation(warpProg, "shiftBiasX");
    warpU.shiftScaleY  = glGetUniformLocation(warpProg, "shiftScaleY");
    warpU.shiftBiasY   = glGetUniformLocation(warpProg, "shiftBiasY");

    tileU.orgWidth  = glGetUniformLocation(tileProg, "orgWidth");
    tileU.orgHeight = glGetUniformLocation(tileProg, "orgHeight");
    tileU.eyeSign   = glGetUniformLocation(tileProg, "eyeSign");

    LOGI("Shaders ready in %.2f ms",
         duration_cast<milliseconds>(steady_clock::now()-tShader).count()/1.0);

    // 扭曲（warp）函数
    auto warpEye = [&](GLuint dstC, GLuint dstD, GLuint dstI, float xScale, float yScale) {
        glUseProgram(warpProg);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, imageTex);
        glUniform1i(glGetUniformLocation(warpProg, "srcColor"), 0);

        glBindImageTexture(2, dstC, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
        glBindImageTexture(3, dstD, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
        glBindImageTexture(4, dstI, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

        float shiftScaleX = divergence * 0.01f * imageW * 0.5f * xScale;
        float shiftBiasX  = -convergence * shiftScaleX;
        float shiftScaleY = divergence * 0.01f * imageH * 0.5f * yScale;
        float shiftBiasY  = -convergence * shiftScaleY;

        glUniform1i(warpU.orgWidth    , imageW);
        glUniform1i(warpU.orgHeight   , imageH);
        glUniform1i(warpU.padSizeX    , padSizeX);
        glUniform1i(warpU.padSizeY    , padSizeY);
        glUniform1i(warpU.paddedWidth , paddedW);
        glUniform1i(warpU.paddedHeight, paddedH);
        glUniform1f(warpU.shiftScaleX , shiftScaleX);
        glUniform1f(warpU.shiftBiasX  , shiftBiasX);
        glUniform1f(warpU.shiftScaleY , shiftScaleY);
        glUniform1f(warpU.shiftBiasY  , shiftBiasY);

        GLuint gx = (GLuint)((paddedW + 15) / 16);
        GLuint gy = (GLuint)((paddedH + 15) / 16);
        glDispatchCompute(gx, gy, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    };

    // 填充（fill）函数
    auto fillEye = [&](GLuint color, GLuint index, int eyeSign) {
        int numTile = (imageW + TILE_W - 1) / TILE_W;
        glUseProgram(tileProg);
        glBindImageTexture(6, color, 0, GL_FALSE, 0, GL_READ_ONLY , GL_RGBA8);
        glBindImageTexture(2, color, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
        glBindImageTexture(4, index, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
        glUniform1i(tileU.orgWidth , imageW);
        glUniform1i(tileU.orgHeight, imageH);
        glUniform1i(tileU.eyeSign  , eyeSign);
        glDispatchCompute((GLuint)numTile, (GLuint)imageH, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    };

    // 保存原图中左幅（验证）
    saveTexturePNG(imageTex, imageW, imageH, "image_left.png");

    // 清空函数
    auto resetTargets = [&]() {
        auto t0 = steady_clock::now();
        clearTextureRGBA8(leftColor , 255,0,0,255);
        clearTextureR32UI(leftDepth , 0u);
        clearTextureR32UI(leftIndex , 0xFFFFFFFFu);
        clearTextureRGBA8(rightColor, 255,0,0,255);
        clearTextureR32UI(rightDepth, 0u);
        clearTextureR32UI(rightIndex, 0xFFFFFFFFu);
        auto us = duration_cast<std::chrono::microseconds>(steady_clock::now() - t0).count();
        static bool first = true;
        if (first) {
            LOGI("Texture clear via FBO+glClearBuffer: %.2f ms", us/1000.0);
            first = false;
        }
    };

    float xScale = -8.0f, yScale = 0.0f;
    LOGI("Warping left...");
    warpEye(leftColor , leftDepth , leftIndex , xScale, yScale);
    LOGI("Warping right...");
    warpEye(rightColor, rightDepth, rightIndex, xScale-2.0f, yScale);
    saveTexturePNG(leftColor , imageW, imageH, "left_eye_warped.png");
    saveTexturePNG(rightColor, imageW, imageH, "right_eye_warped.png");

    LOGI("Filling left...");
    fillEye(leftColor , leftIndex, +1);
    LOGI("Filling right...");
    fillEye(rightColor, rightIndex, -1);
    saveTexturePNG(leftColor , imageW, imageH, "left_eye_filled.png");
    saveTexturePNG(rightColor, imageW, imageH, "right_eye_filled.png");

    resetTargets();
    glFinish();

    // 性能测试
    const int TEST_ITERATIONS = 100;
    LOGI("Pipeline perf test: %d iterations", TEST_ITERATIONS);
    auto tPerf = steady_clock::now();
    for (int i=0;i<TEST_ITERATIONS;++i) {
        auto t0 = steady_clock::now();
        warpEye(leftColor , leftDepth , leftIndex , xScale, yScale);
        warpEye(rightColor, rightDepth, rightIndex, xScale-2.0f, yScale);
        std::string name = "left_eye_warped_" + std::to_string(i) + ".png";
        saveTexturePNG(leftColor , imageW, imageH, name.c_str());
        name = "right_eye_warped_" + std::to_string(i) + ".png";
        saveTexturePNG(rightColor, imageW, imageH, name.c_str());
        fillEye(leftColor , leftIndex, +1);
        fillEye(rightColor, rightIndex, -1);
        name = "left_eye_filled_" + std::to_string(i) + ".png";
        saveTexturePNG(leftColor , imageW, imageH, name.c_str());
        name = "right_eye_filled_" + std::to_string(i) + ".png";
        saveTexturePNG(rightColor, imageW, imageH, name.c_str());
        if (i < TEST_ITERATIONS-1) resetTargets();
        glFinish();
        auto us = duration_cast<std::chrono::microseconds>(steady_clock::now() - t0).count();
        bool print = (i < 10) || (i >= TEST_ITERATIONS/2 - 5 && i < TEST_ITERATIONS/2 + 5) || (i >= TEST_ITERATIONS - 10);
        if (print) LOGI("Iter %d/%d: %.2f ms", i+1, TEST_ITERATIONS, us/1000.0);
        else if (i == 10) LOGI("... (skipping) ...");
    }
    double avgUs = duration_cast<std::chrono::microseconds>(steady_clock::now()-tPerf).count() / double(TEST_ITERATIONS);
    LOGI("Average: %.2f ms", avgUs/1000.0);

    // 保存最终结果
    saveTexturePNG(leftColor , imageW, imageH, "left_eye_result.png");
    saveTexturePNG(rightColor, imageW, imageH, "right_eye_result.png");

    // 清理
    glDeleteTextures(1, &imageTex);
    glDeleteTextures(1, &leftColor);
    glDeleteTextures(1, &leftDepth);
    glDeleteTextures(1, &leftIndex);
    glDeleteTextures(1, &rightColor);
    glDeleteTextures(1, &rightDepth);
    glDeleteTextures(1, &rightIndex);
    glDeleteProgram(warpProg);
    glDeleteProgram(tileProg);

    cleanupOpenGL();
    auto totalMs = duration_cast<std::chrono::milliseconds>(steady_clock::now() - startTime).count();
    LOGI("Done. Total time: %lld ms", (long long)totalMs);
    return 0;
}