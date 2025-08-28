#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <GLES3/gl32.h>
#include <algorithm>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>

// Android日志宏
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "xptest", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "xptest", __VA_ARGS__)

using namespace std::chrono;

// STB和TINYEXR实现定义
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#define TINYEXR_IMPLEMENTATION

// 包含头文件
#include "stb_image.h"
#include "stb_image_write.h"
#include "tinyexr.h"



// EGL相关变量
EGLDisplay display = EGL_NO_DISPLAY;
EGLContext context = EGL_NO_CONTEXT;
EGLSurface surface = EGL_NO_SURFACE;

// 窗口尺寸
int windowWidth = 1920;
int windowHeight = 1080;

// Uniform位置缓存结构体
struct WarpUniforms {
    GLint orgWidth, orgHeight, padSizeX, padSizeY, paddedWidth, paddedHeight;
    GLint shiftScaleX, shiftBiasX, shiftScaleY, shiftBiasY;
} warpU;

struct TileUniforms {
    GLint orgWidth, orgHeight, eyeSign;
} tileU;

// 着色器编译函数
GLuint compileShader(GLenum type, const std::string &source) {
    GLuint shader = glCreateShader(type);
    const char *src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    // 错误检查
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        LOGE("Shader Compilation Failed:\n%s", infoLog);
    }

    return shader;
}

// 文件加载函数
std::string loadFile(const char *path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOGE("Failed to open file: %s", path);
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    std::string code = ss.str();
    LOGI("Loaded shader %s, length: %zu", path, code.size());
    return code;
}

// 创建着色器程序
GLuint createShaderProgram(const char *vertexPath, const char *fragmentPath) {
    std::string vertexCode = loadFile(vertexPath);
    std::string fragmentCode = loadFile(fragmentPath);

    if (vertexCode.empty() || fragmentCode.empty()) {
        LOGE("Failed to load shader files");
        return 0;
    }

    GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexCode);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentCode);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    // 错误检查
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        LOGE("Shader Linking Failed:\n%s", infoLog);
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return program;
}

// 创建计算着色器程序
GLuint createComputeProgram(const char *path) {
    std::string code = loadFile(path);
    if (code.empty()) {
        LOGE("Failed to load compute shader: %s", path);
        return 0;
    }

    GLuint cs = compileShader(GL_COMPUTE_SHADER, code);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, cs);
    glLinkProgram(prog);

    // 错误检查
    GLint success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(prog, 512, nullptr, infoLog);
        LOGE("Compute Shader Linking Failed:\n%s", infoLog);
    }

    glDeleteShader(cs);
    return prog;
}

// 从PNG加载纹理
GLuint loadTextureFromPNG(const char *path, int &width, int &height) {
    int channels;
    unsigned char *data = stbi_load(path, &width, &height, &channels, 4);
    if (!data) {
        LOGE("Failed to load image: %s", path);
        return 0;
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return texID;
}

// 从EXR加载深度图
GLuint loadDepthFromEXR(const char *path, int &width, int &height) {
    EXRVersion exr_version;
    int ret = ParseEXRVersionFromFile(&exr_version, path);
    if (ret != 0) {
        LOGE("Invalid EXR file: %s", path);
        return 0;
    }

    if (exr_version.multipart) {
        LOGE("Multipart EXR not supported");
        return 0;
    }

    EXRHeader exr_header;
    InitEXRHeader(&exr_header);
    const char *err = nullptr;

    ret = ParseEXRHeaderFromFile(&exr_header, &exr_version, path, &err);
    if (ret != 0) {
        LOGE("Parse EXR err: %s", (err ? err : "unknown"));
        FreeEXRErrorMessage(err);
        return 0;
    }

    LOGI("EXR file has %d channels", exr_header.num_channels);
    for (int i = 0; i < exr_header.num_channels; ++i) {
        LOGI("Channel %d: %s", i, exr_header.channels[i].name);
    }

    // Convert HALF channels to FLOAT
    for (int i = 0; i < exr_header.num_channels; ++i) {
        if (exr_header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF) {
            exr_header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
        }
    }

    EXRImage exr_image;
    InitEXRImage(&exr_image);

    ret = LoadEXRImageFromFile(&exr_image, &exr_header, path, &err);
    if (ret != 0) {
        LOGE("Load EXR err: %s", (err ? err : "unknown"));
        FreeEXRHeader(&exr_header);
        FreeEXRErrorMessage(err);
        return 0;
    }

    width = exr_image.width;
    height = exr_image.height;

    // 查找 Z 通道
    int z_channel = -1;
    for (int i = 0; i < exr_header.num_channels; ++i) {
        std::string cname = exr_header.channels[i].name;
        if (cname == "Z" || cname == "R") { // 如果你用 imageio 写的 RGB 就取 R
            z_channel = i;
            LOGI("Found depth channel: %s at index %d", cname.c_str(), i);
            break;
        }
    }

    if (z_channel == -1) {
        LOGE("No 'Z' or 'R' channel found. Available channels:");
        for (int i = 0; i < exr_header.num_channels; ++i) {
            LOGE("  Channel %d: %s", i, exr_header.channels[i].name);
        }
        FreeEXRImage(&exr_image);
        FreeEXRHeader(&exr_header);
        return 0;
    }

    float *src = reinterpret_cast<float *>(exr_image.images[z_channel]);

    // 上传为 OpenGL 纹理
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, src);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    FreeEXRImage(&exr_image);
    FreeEXRHeader(&exr_header);

    LOGI("Successfully loaded depth channel from EXR: %dx%d", width, height);
    return texID;
}

// 保存纹理为PNG - 使用FBO读取纹理
void saveTexturePNG(GLuint tex, int w, int h, const char *name) {
    // 创建FBO来读取纹理
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("FBO incomplete: 0x%04X", st);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        return;
    }
    
    // 读取RGBA格式，然后转换为RGB
    std::vector<unsigned char> rgba_buf(w * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba_buf.data());
    
    // 转换RGBA到RGB
    std::vector<unsigned char> rgb_buf(w * h * 3);
    for (int i = 0; i < w * h; i++) {
        rgb_buf[i * 3 + 0] = rgba_buf[i * 4 + 0]; // R
        rgb_buf[i * 3 + 1] = rgba_buf[i * 4 + 1]; // G
        rgb_buf[i * 3 + 2] = rgba_buf[i * 4 + 2]; // B
        // 跳过Alpha通道
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    
    stbi_write_png(name, w, h, 3, rgb_buf.data(), w * 3);
    LOGI("Saved texture to: %s", name);
}

// 保存RGBA纹理为PNG - 保留Alpha通道
void saveRGBATexturePNG(GLuint tex, int w, int h, const char *name) {
    // 创建FBO来读取纹理
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("FBO incomplete: 0x%04X", st);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        return;
    }
    
    // 读取RGBA格式
    std::vector<unsigned char> rgba_buf(w * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba_buf.data());
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    
    // 直接保存RGBA数据
    stbi_write_png(name, w, h, 4, rgba_buf.data(), w * 4);
    LOGI("Saved RGBA texture to: %s", name);
}

// 保存深度图为灰度PNG - 使用FBO读取纹理
void saveDepthGrayPNG(GLuint texR32F, int w, int h, const char *filename) {
    // 创建FBO来读取纹理
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texR32F, 0);
    
    std::vector<float> buf(w * h);
    glReadPixels(0, 0, w, h, GL_RED, GL_FLOAT, buf.data());
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);

    // 找到最小和最大值
    float min_val = *std::min_element(buf.begin(), buf.end());
    float max_val = *std::max_element(buf.begin(), buf.end());
    float range = max_val - min_val;

    if (range == 0.0f) range = 1.0f;

    std::vector<uint8_t> gray(w * h);
    for (int i = 0; i < w * h; ++i) {
        float normalized = (buf[i] - min_val) / range;
        gray[i] = static_cast<uint8_t>(normalized * 255.0f);
    }

    stbi_write_png(filename, w, h, 1, gray.data(), w);
    LOGI("Saved depth to: %s", filename);
}

// 初始化EGL
bool initEGL() {
    // 获取EGL显示
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        LOGE("Failed to get EGL display");
        return false;
    }

    // 初始化EGL
    EGLint major, minor;
    if (!eglInitialize(display, &major, &minor)) {
        LOGE("Failed to initialize EGL");
        return false;
    }

    LOGI("EGL initialized: version %d.%d", major, minor);

    // 选择配置
    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs)) {
        LOGE("Failed to choose EGL config");
        return false;
    }

    // 创建上下文
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        LOGE("Failed to create EGL context");
        return false;
    }

    // 创建离屏Surface
    const EGLint surfaceAttribs[] = {
        EGL_WIDTH, windowWidth,
        EGL_HEIGHT, windowHeight,
        EGL_NONE
    };

    surface = eglCreatePbufferSurface(display, config, surfaceAttribs);
    if (surface == EGL_NO_SURFACE) {
        LOGE("Failed to create EGL surface");
        return false;
    }

    // 绑定上下文
    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOGE("Failed to make EGL context current");
        return false;
    }

    LOGI("xptest: EGL initialized successfully");
    return true;
}

// 清理EGL
void cleanupEGL() {
    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (surface != EGL_NO_SURFACE) {
            eglDestroySurface(display, surface);
        }
        if (context != EGL_NO_CONTEXT) {
            eglDestroyContext(display, context);
        }
        eglTerminate(display);
    }
}

// ───────────────────────────────────────────────────────────────
//  loadFileTest() —— 加载文本文件并把前 16 个字节打印出来
// ───────────────────────────────────────────────────────────────
std::string loadFileTest(const char *path)
{
    std::ifstream file(path, std::ios::binary);   // 二进制读取，避免 \r\n 被翻译
    if (!file.is_open()) {
        LOGE("Failed to open file: %s", path);
        return "";
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string code = ss.str();

    /* ---------- 打印前 16 字节 (HEX & 可视字符) ----------------- */
    char hexBuf[128] = {};
    char ascBuf[32]  = {};

    size_t n = std::min<size_t>(16, code.size());
    for (size_t i = 0; i < n; ++i)
    {
        unsigned char c = static_cast<unsigned char>(code[i]);
        sprintf(hexBuf + i * 3, "%02X ", c);                // “EF BB BF 23 …”
        ascBuf[i] = (c >= 32 && c < 127) ? c : '.';         // 不可见字符打印 '.'
    }
    LOGI("Shader %s  len=%zu  head(hex): %s  head(asc): %.16s",
         path, code.size(), hexBuf, ascBuf);

    return code;
}

inline void clearTextureRGBA8 (GLuint tex, uint8_t  r, uint8_t  g,
    uint8_t  b, uint8_t  a = 0)
{
    static GLuint sFBO = 0;
    if (!sFBO) glGenFramebuffers(1, &sFBO);

    const GLfloat v[4] = { r / 255.f, g / 255.f, b / 255.f, a / 255.f };

    glBindFramebuffer(GL_FRAMEBUFFER, sFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
    GL_TEXTURE_2D, tex, 0);

    glClearBufferfv(GL_COLOR, 0, v);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

inline void clearTextureR32UI(GLuint tex, uint32_t value)
{
    static GLuint sFBO = 0;
    if (!sFBO) glGenFramebuffers(1, &sFBO);

    const GLuint v[4] = { value, 0u, 0u, 0u };

    glBindFramebuffer(GL_FRAMEBUFFER, sFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
    GL_TEXTURE_2D, tex, 0);

    glClearBufferuiv(GL_COLOR, 0, v);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

inline void clearTextureRGBA32UI(GLuint tex,
      uint32_t r, uint32_t g,
      uint32_t b, uint32_t a = 0)
{
    static GLuint sFBO = 0;
    if (!sFBO) glGenFramebuffers(1, &sFBO);

    const GLuint v[4] = { r, g, b, a };

    glBindFramebuffer(GL_FRAMEBUFFER, sFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
    GL_TEXTURE_2D, tex, 0);

    glClearBufferuiv(GL_COLOR, 0, v);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// 主函数
int main() {
    auto startTime = steady_clock::now();
    LOGI("xptest: Starting OpenGL ES Stereo Generator");

    // 初始化EGL
    if (!initEGL()) {
        LOGE("Failed to initialize EGL");
        return -1;
    }

    // 检查OpenGL ES版本
    const char* version = (const char*)glGetString(GL_VERSION);
    LOGI("OpenGL ES Version: %s", version);
    
    // 检查支持的扩展
    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
    LOGI("Supported extensions: %s", extensions);
    
    int major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major < 3 || (major == 3 && minor < 1)) {
        LOGE("xptest: Compute shaders not supported (OpenGL ES version too low)");
        cleanupEGL();
        return -1;
    }



    // 查询硬件支持的计算着色器上限
    GLint maxWGCount[3], maxWGSize[3], maxWGInvoc;
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maxWGCount[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &maxWGCount[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &maxWGCount[2]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxWGSize[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxWGSize[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &maxWGSize[2]);
    glGetIntegerv (GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxWGInvoc);

    LOGI("xptest: Max work group  count: %d x %d x %d", maxWGCount[0], maxWGCount[1], maxWGCount[2]);
    LOGI("xptest: Max work group  size : %d x %d x %d", maxWGSize [0], maxWGSize [1], maxWGSize [2]);
    LOGI("xptest: Max work group  invocations (x*y*z) : %d", maxWGInvoc);

    // 获取图像尺寸
    int imageW, imageH, channels;
    unsigned char *info_data = stbi_load("assets/avengers_sbs_d.png", &imageW, &imageH, &channels, 0);
    if (info_data) {
        windowWidth = imageW;
        windowHeight = imageH;
        stbi_image_free(info_data);
        LOGI("Image dimensions: %dx%d", imageW, imageH);
    } else {
        LOGI("Failed to get image info, using default size: %dx%d", windowWidth, windowHeight);
    }

    // 加载图像
    auto loadStart = steady_clock::now();
    GLuint imageTex = loadTextureFromPNG("assets/avengers_sbs_d.png", imageW, imageH);
    if (!imageTex) {
        LOGE("Failed to load avengers_sbs_d.png");
        cleanupEGL();
        return -1;
    }
    LOGI("Loaded avengers_sbs_d.png: %dx%d", imageW, imageH);
    imageW = imageW / 2;

    // // 加载深度图
    // int depthW, depthH;
    // GLuint depthTex = loadDepthFromEXR("assets/depth.exr", depthW, depthH);
    // if (!depthTex) {
    //     LOGE("Failed to load depth.exr");
    //     cleanupEGL();
    //     return -1;
    // }
    // LOGI("Loaded depth.exr: %dx%d", depthW, depthH);
    // auto loadTime = duration_cast<milliseconds>(steady_clock::now() - loadStart).count();
    // LOGI("xptest: Assets loaded in %lldms", loadTime);

    // // 保存深度图为PNG用于调试
    // saveDepthGrayPNG(depthTex, depthW, depthH, "depth.png");

    // 立体参数
    const float divergence = 2.0f;
    const float convergence = 0.0f;
    int padSizeX = int(imageW * divergence * 0.01f + 2);
    int padSizeY = int(imageH * divergence * 0.01f + 2);
    int paddedW = imageW + padSizeX * 2;
    int paddedH = imageH + padSizeY * 2;

    // 全局尺寸
    const int TILE_W = 256;

    // 创建目标纹理
    GLuint leftColor, leftDepth, leftIndex;
    GLuint rightColor, rightDepth, rightIndex;

    auto allocTex2D = [](GLuint &id, GLenum internalFmt, int w, int h) {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexStorage2D(GL_TEXTURE_2D, 1, internalFmt, w, h); // ES3.0 核心
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    };

    auto makeTarget3 = [&](GLuint &color, GLuint &depth, GLuint &index) {
        allocTex2D(color, GL_RGBA8,     imageW, imageH);
        allocTex2D(depth, GL_R32UI,     imageW, imageH);
        allocTex2D(index, GL_R32UI,     imageW, imageH);

        /* --- 清零纹理 -------------------------------------------------- */
        clearTextureRGBA8(color, 255, 255, 255, 255);   // 白色
        clearTextureR32UI(depth, 0);                     // 0
        clearTextureR32UI(index, 0xFFFFFFFFu);           // UNDEF
    };

    // 创建左右眼目标纹理
    auto makeTargetStart = steady_clock::now();
    makeTarget3(leftColor, leftDepth, leftIndex);
    makeTarget3(rightColor, rightDepth, rightIndex);
    auto makeTargetTime = duration_cast<milliseconds>(steady_clock::now() - makeTargetStart).count();
    LOGI("xptest: Target textures created in %lldms", makeTargetTime);

    // 编译着色器
    auto shaderStart = steady_clock::now();
    GLuint warpProg = createComputeProgram("shaders/warp_es.comp");
    loadFileTest("shaders/warp_es.comp");
    GLuint tileProg = createComputeProgram("shaders/fill_tile_es.comp");
    loadFileTest("shaders/fill_tile_es.comp");
    // GLuint prefixProg = createComputeProgram("shaders/fill_prefix_es.comp");
    // loadFileTest("shaders/fill_prefix_es.comp");
    // GLuint resetProg = createComputeProgram("shaders/reset_textures_es.comp");
    // loadFileTest("shaders/reset_textures_es.comp");

    // if (!warpProg || !tileProg || !prefixProg || !resetProg) {
    //     LOGE("xptest: Failed to create compute programs");
    //     cleanupEGL();
    //     return -1;
    // }
    if (!warpProg || !tileProg) {
        LOGE("xptest: Failed to create compute programs");
        cleanupEGL();
        return -1;
    }

    // 缓存 uniform 位置
    warpU.orgWidth = glGetUniformLocation(warpProg, "orgWidth");
    warpU.orgHeight = glGetUniformLocation(warpProg, "orgHeight");
    warpU.padSizeX = glGetUniformLocation(warpProg, "padSizeX");
    warpU.padSizeY = glGetUniformLocation(warpProg, "padSizeY");
    warpU.paddedWidth = glGetUniformLocation(warpProg, "paddedWidth");
    warpU.paddedHeight = glGetUniformLocation(warpProg, "paddedHeight");
    warpU.shiftScaleX = glGetUniformLocation(warpProg, "shiftScaleX");
    warpU.shiftBiasX = glGetUniformLocation(warpProg, "shiftBiasX");
    warpU.shiftScaleY = glGetUniformLocation(warpProg, "shiftScaleY");
    warpU.shiftBiasY = glGetUniformLocation(warpProg, "shiftBiasY");

    tileU.orgWidth = glGetUniformLocation(tileProg, "orgWidth");
    tileU.orgHeight = glGetUniformLocation(tileProg, "orgHeight");
    tileU.eyeSign = glGetUniformLocation(tileProg, "eyeSign");

    // 打印所有uniform位置 (-1表示未找到)
    LOGI("xptest: Warp uniform locations:");
    LOGI("xptest:   orgWidth=%d, orgHeight=%d, padSizeX=%d, padSizeY=%d", warpU.orgWidth, warpU.orgHeight, warpU.padSizeX, warpU.padSizeY);
    LOGI("xptest:   paddedWidth=%d, paddedHeight=%d", warpU.paddedWidth, warpU.paddedHeight);
    LOGI("xptest:   shiftScaleX=%d, shiftBiasX=%d, shiftScaleY=%d, shiftBiasY=%d", warpU.shiftScaleX, warpU.shiftBiasX, warpU.shiftScaleY, warpU.shiftBiasY);
    
    LOGI("xptest: Tile uniform locations:");
    LOGI("xptest:   orgWidth=%d, orgHeight=%d, eyeSign=%d", tileU.orgWidth, tileU.orgHeight, tileU.eyeSign);

    auto shaderTime = duration_cast<milliseconds>(steady_clock::now() - shaderStart).count();
    LOGI("xptest: Shaders compiled and uniforms cached in %lldms", shaderTime);

    // 扭曲处理函数
    auto warpEye = [&](GLuint dstC, GLuint dstD, GLuint dstI, float xScale, float yScale) {
        glUseProgram(warpProg);
        // 输入源纹理
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, imageTex);
        glUniform1i(glGetUniformLocation(warpProg, "srcColor"), 0); 

        // glActiveTexture(GL_TEXTURE1);
        // glBindTexture(GL_TEXTURE_2D, depthTex);
        // glUniform1i(glGetUniformLocation(warpProg, "srcDepth"), 1); 

        // 输出绑定
        glBindImageTexture(2, dstC, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
        glBindImageTexture(3, dstD, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
        glBindImageTexture(4, dstI, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

        // 参数 - 使用缓存的 uniform 位置
        float shiftScaleX = divergence * 0.01f * imageW * 0.5f * xScale;
        float shiftBiasX = -convergence * shiftScaleX;
        float shiftScaleY = divergence * 0.01f * imageH * 0.5f * yScale;
        float shiftBiasY = -convergence * shiftScaleY;

        glUniform1i(warpU.orgWidth, imageW);
        glUniform1i(warpU.orgHeight, imageH);
        glUniform1i(warpU.padSizeX, padSizeX);
        glUniform1i(warpU.padSizeY, padSizeY);
        glUniform1i(warpU.paddedWidth, paddedW);
        glUniform1i(warpU.paddedHeight, paddedH);
        glUniform1f(warpU.shiftScaleX, shiftScaleX);
        glUniform1f(warpU.shiftBiasX, shiftBiasX);
        glUniform1f(warpU.shiftScaleY, shiftScaleY);
        glUniform1f(warpU.shiftBiasY, shiftBiasY);

        // Dispatch - 检查硬件上限
        GLuint gx = (paddedW + 15) / 16;
        GLuint gy = (paddedH + 15) / 16;
        
        if (gx > (GLuint)maxWGCount[0] || gy > (GLuint)maxWGCount[1]) {
            LOGE("xptest: Work group count exceeds hardware limits: %u x %u > %d x %d", 
                 gx, gy, maxWGCount[0], maxWGCount[1]);
            return;
        }
        glDispatchCompute(gx, gy, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    };

    // 填充处理函数
    auto fillEye = [&](GLuint color, GLuint index, int eyeSign) {
        int numTile = (imageW + TILE_W - 1) / TILE_W;
        
        // 检查硬件上限
        if ((GLuint)numTile > (GLuint)maxWGCount[0] || TILE_W > maxWGSize[0]) {
            LOGE("xptest: Fill dispatch exceeds hardware limits: numTile=%d > %d or %d > %d", 
                 numTile, maxWGCount[0], TILE_W, maxWGSize[0]);
            return;
        }

        // Pass-B-1: tile内shift_fill/fix/shift_fill
        glUseProgram(tileProg);

        // 写（binding = 2）
        glBindImageTexture(2, color, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
        // 读（binding = 6）
        glBindImageTexture(6, color, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
        glBindImageTexture(4, index, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

        // 使用缓存的 uniform 位置
        glUniform1i(tileU.orgWidth, imageW);
        glUniform1i(tileU.orgHeight, imageH);
        glUniform1i(tileU.eyeSign, eyeSign);

        glDispatchCompute(numTile, imageH, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    };
    saveTexturePNG(imageTex, imageW, imageH, "image.png");

    // 数据恢复函数
    auto resetTargets = [&]() {
        auto t0 = steady_clock::now();
    
        // ---------------- 左眼 ----------------
        clearTextureRGBA8 (leftColor, 255,0,0,255);   // 白色
        clearTextureR32UI (leftDepth, 0);                 // 0
        clearTextureR32UI (leftIndex, 0xFFFFFFFFu);       // UNDEF
    
        // ---------------- 右眼 ----------------
        clearTextureRGBA8 (rightColor, 255,0,0,255);
        clearTextureR32UI (rightDepth, 0);
        clearTextureR32UI (rightIndex, 0xFFFFFFFFu);
    
        auto us = duration_cast<microseconds>(steady_clock::now() - t0).count();
        static bool first = true;
        if (first) {
            LOGI("xptest: Texture clear method: FBO+glClearBuffer, time: %.2f ms",
                    us / 1000.0);
            first = false;
        }
    };
    float xScale = 10.0f;
    float yScale = 5.0f;
    // 执行扭曲 - 先跑一次正常流程
    LOGI("xptest: Processing left eye...");
    warpEye(leftColor, leftDepth, leftIndex, xScale, yScale);
    LOGI("xptest: Processing right eye...");
    warpEye(rightColor, rightDepth, rightIndex, xScale-2.0f, yScale);
    // 保存扭曲结果
    saveTexturePNG(leftColor, imageW, imageH, "left_eye_warped.png");
    saveTexturePNG(rightColor, imageW, imageH, "right_eye_warped.png");

    // 执行填充 - 先跑一次正常流程
    LOGI("xptest: Filling left eye...");
    fillEye(leftColor, leftIndex, +1);
    LOGI("xptest: Filling right eye...");
    fillEye(rightColor, rightIndex, -1);
    saveTexturePNG(leftColor, imageW, imageH, "left_eye_filled.png");
    saveTexturePNG(rightColor, imageW, imageH, "right_eye_filled.png");
    resetTargets();
    glFinish();

    // 数据恢复函数 - 使用 Compute Shader
    // auto resetTargets = [&]() {
    //     auto t0 = steady_clock::now();
        
    //     glUseProgram(resetProg);

    //     /* ------- 把 8 张纹理按 shader 里写的 binding 号绑上 ------- */
    //     // 左眼
    //     glBindImageTexture(0, leftColor , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8   );
    //     glBindImageTexture(1, leftDepth , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI   );
    //     glBindImageTexture(2, leftIndex , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI   );
    //     glBindImageTexture(3, leftEdge  , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32UI);
    //     // 右眼
    //     glBindImageTexture(4, rightColor, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8   );
    //     glBindImageTexture(5, rightDepth, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI   );
    //     glBindImageTexture(6, rightIndex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI   );
    //     glBindImageTexture(7, rightEdge , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32UI);

    //     /* ------- 计算 dispatch 尺寸（取所有贴图里最大的宽高） ------- */
    //     const int maxW = std::max(imageW, edgeW);      // edgeW 可能比 imageW 大
    //     const int groupX = (maxW  + 31) / 32;
    //     const int groupY = (imageH + 31) / 32;

    //     glDispatchCompute(groupX, groupY, 1);

    //     /* GPU 写 image → 后续 shader / 采样器可见 */
    //     glMemoryBarrier(GL_ALL_BARRIER_BITS);
        
    //     auto us = duration_cast<microseconds>(steady_clock::now() - t0).count();
    //     static bool first = true;
    //     if (first) {
    //         LOGI("xptest: Texture clear method: Compute Shader, time: %.2f ms",
    //              us / 1000.0);
    //         first = false;
    //     }
    // };

    // 性能测试配置
    const int TEST_ITERATIONS = 100;
    
    // 性能测试 - 完整流程(Warp+Fill)
    LOGI("xptest: Starting complete pipeline performance test (%d iterations)...", TEST_ITERATIONS);
    auto pipelineTestStart = steady_clock::now();
    for (int i = 0; i < TEST_ITERATIONS; ++i)
    {
        auto iterStart = steady_clock::now();
        
        // ---------- 1. 扭曲 + 填充 ----------
        warpEye (leftColor , leftDepth , leftIndex , xScale, yScale);
        warpEye (rightColor, rightDepth, rightIndex, xScale-2.0f, yScale);
        fillEye (leftColor , leftIndex, +1);
        fillEye (rightColor, rightIndex, -1);
        
    
        // ---------- 3. 恢复数据 ----------
        if (i < TEST_ITERATIONS-1)
        {
            resetTargets();
        }
        glFinish();
        
        // ---------- 4. 打印耗时 ----------
        auto iterTime = duration_cast<microseconds>(steady_clock::now() - iterStart).count();
        // 打印前10轮、中间10轮、后10轮的耗时，避免输出过多
        bool shouldPrint = (i < 10) || 
                          (i >= TEST_ITERATIONS/2 - 5 && i < TEST_ITERATIONS/2 + 5) || 
                          (i >= TEST_ITERATIONS - 10);
        
        if (shouldPrint) {
            LOGI("xptest: Iteration %d/%d: %.2f ms", i+1, TEST_ITERATIONS, iterTime / 1000.0);
        } else if (i == 10) {
            LOGI("xptest: ... (skipping middle iterations for brevity) ...");
        }
    }
    auto pipelineAvgTime = duration_cast<microseconds>(steady_clock::now() - pipelineTestStart).count() / (double)TEST_ITERATIONS;
    LOGI("xptest: Complete pipeline average time: %.2f ms ", pipelineAvgTime / 1000.0);

    // 保存最终结果
    saveTexturePNG(leftColor, imageW, imageH, "left_eye_result.png");
    saveTexturePNG(rightColor, imageW, imageH, "right_eye_result.png");

    // 清理资源
    glDeleteTextures(1, &imageTex);
    // glDeleteTextures(1, &depthTex);
    glDeleteTextures(1, &leftColor);
    glDeleteTextures(1, &leftDepth);
    glDeleteTextures(1, &leftIndex);
    glDeleteTextures(1, &rightColor);
    glDeleteTextures(1, &rightDepth);
    glDeleteTextures(1, &rightIndex);
    glDeleteProgram(warpProg);
    glDeleteProgram(tileProg);
    // glDeleteProgram(prefixProg);
    // glDeleteProgram(resetProg);

    cleanupEGL();
    auto totalTime = duration_cast<milliseconds>(steady_clock::now() - startTime).count();
    LOGI("xptest: Processing completed successfully in %lldms total!", totalTime);
    return 0;
} 