#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <iostream>
#include <vector>


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>

int windowWidth = 1920;
int windowHeight = 1080;

#include <fstream>
#include <sstream>

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
    std::cerr << "Shader Compilation Failed:\n" << infoLog << std::endl;
  }

  return shader;
}

std::string loadFile(const char *path) {
  std::ifstream file(path);
  std::stringstream ss;
  ss << file.rdbuf();
  std::string code = ss.str();
  std::cout << "[DEBUG] Loaded shader " << path << ", length: " << code.size()
            << std::endl;
  return code;
}

GLuint createShaderProgram(const char *vertexPath, const char *fragmentPath) {
  std::string vertexCode = loadFile(vertexPath);
  std::string fragmentCode = loadFile(fragmentPath);

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
    std::cerr << "Shader Linking Failed:\n" << infoLog << std::endl;
  }

  glDeleteShader(vertex);
  glDeleteShader(fragment);

  return program;
}

/************  util: 创建 compute shader ************/
GLuint createComputeProgram(const char *path) {
  std::string code = loadFile(path);
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
    std::cerr << "Compute Shader Linking Failed:\n" << infoLog << std::endl;
  }

  glDeleteShader(cs);
  return prog;
}

// Utility: Render fullscreen quad
void renderFullScreenQuad() {
  static GLuint quadVAO = 0;
  static GLuint quadVBO;

  if (quadVAO == 0) {
    float quadVertices[] = {// 位置        // UV
                            -1.0f, 1.0f, 0.0f, 1.0f,  -1.0f, -1.0f,
                            0.0f,  0.0f, 1.0f, -1.0f, 1.0f,  0.0f,

                            -1.0f, 1.0f, 0.0f, 1.0f,  1.0f,  -1.0f,
                            1.0f,  0.0f, 1.0f, 1.0f,  1.0f,  1.0f};

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices,
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void *)(2 * sizeof(float)));
  }

  glBindVertexArray(quadVAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

// Load image.png
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

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB,
               GL_UNSIGNED_BYTE, data);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  stbi_image_free(data);
  return texID;
}

// Load depth.exr
GLuint loadDepthFromEXR(const char *path, int &width, int &height) {
  EXRVersion exr_version;
  int ret = ParseEXRVersionFromFile(&exr_version, path);
  if (ret != 0) {
    std::cerr << "❌ Invalid EXR file: " << path << std::endl;
    return 0;
  }

  if (exr_version.multipart) {
    std::cerr << "❌ Multipart EXR not supported.\n";
    return 0;
  }

  EXRHeader exr_header;
  InitEXRHeader(&exr_header);
  const char *err = nullptr;

  ret = ParseEXRHeaderFromFile(&exr_header, &exr_version, path, &err);
  if (ret != 0) {
    std::cerr << "❌ Parse EXR err: " << (err ? err : "unknown") << std::endl;
    FreeEXRErrorMessage(err);
    return 0;
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
    std::cerr << "❌ Load EXR err: " << (err ? err : "unknown") << std::endl;
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
      break;
    }
  }

  if (z_channel == -1) {
    std::cerr << " No 'Z' or 'R' channel found.\n";
    FreeEXRImage(&exr_image);
    FreeEXRHeader(&exr_header);
    return 0;
  }

  float *src = reinterpret_cast<float *>(exr_image.images[z_channel]);

  // 上传为 OpenGL 纹理
  GLuint texID;
  glGenTextures(1, &texID);
  glBindTexture(GL_TEXTURE_2D, texID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT,
               src);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  FreeEXRImage(&exr_image);
  FreeEXRHeader(&exr_header);

  return texID;
}

void saveTexturePNG(GLuint tex, int w, int h, const char *name) {
  std::vector<unsigned char> buf(w * h * 4);
  glBindTexture(GL_TEXTURE_2D, tex);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());

  // 直接保存原始数据，不做Y轴翻转
  stbi_write_png(name, w, h, 4, buf.data(), w * 4);
}

void saveDepthGrayPNG(GLuint texR32F, int w, int h, const char *filename) {
  std::vector<float> buf(w * h);
  glBindTexture(GL_TEXTURE_2D, texR32F);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, buf.data());

  // 统计最小/最大深度
  float dMin = 1e30f, dMax = -1e30f;
  for (float v : buf) {
    if (!std::isfinite(v))
      continue;
    dMin = std::min(dMin, v);
    dMax = std::max(dMax, v);
  }
  if (dMax - dMin < 1e-6f)
    dMax = dMin + 1e-6f;
  std::cout << "depth range : [" << dMin << ", " << dMax << "]\n";

  // 映射到灰度图
  std::vector<uint8_t> gray(w * h);
  for (size_t i = 0; i < buf.size(); ++i) {
    float norm = (buf[i] - dMin) / (dMax - dMin);
    gray[i] = uint8_t(std::clamp(norm, 0.0f, 1.0f) * 255.0f + 0.5f);
  }

  // 保存
  stbi_write_png(filename, w, h, 1, gray.data(), w);
}

/************  创建一张 rgba32f / r32f 空纹理 ************/
GLuint makeFloatTex(int w, int h, GLenum internal, GLenum fmt) {
  GLuint id;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
  glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, fmt, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return id;
}

int main() {
  if (!glfwInit()) {
    std::cerr << "GLFW Init Failed\n";
    return -1;
  }
  int imageW, imageH, channels;
  unsigned char *info_data =
      stbi_load("image.png", &imageW, &imageH, &channels, 0);
  if (info_data) {
    windowWidth = imageW;
    windowHeight = imageH;
    stbi_image_free(info_data);
    std::cout << "Image dimensions: " << imageW << "x" << imageH << std::endl;
  } else {
    std::cout << "Failed to get image info, using default size: " << windowWidth
              << "x" << windowHeight << std::endl;
  }

  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  GLFWwindow *window =
      glfwCreateWindow(windowWidth, windowHeight, "OpenGL", nullptr, nullptr);
  if (!window) {
    std::cerr << "Window creation failed\n";
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "GLAD Load Failed\n";
    return -1;
  }

  // Load image.png
  GLuint imageTex = loadTextureFromPNG("image.png", imageW, imageH);
  std::cout << "Loaded image.png: " << imageW << "x" << imageH << std::endl;

  // Load depth.exr
  int depthW, depthH;
  GLuint depthTex = loadDepthFromEXR("depth.exr", depthW, depthH);
  if (depthTex == 0) {
    std::cerr << "Failed to load depth texture.\n";
  } else {
    std::cout << "Loaded depth.exr: " << depthW << "x" << depthH << std::endl;
  }
  saveDepthGrayPNG(depthTex, depthW, depthH, "depth.png");
  const float divergence = 2.0f;
  const float convergence = 0.0f;

  int padSize = int(imageW * divergence * 0.01f + 2);
  int paddedW = imageW + padSize * 2; // 线程宽度

//--------------------------------------------------------------------
// 全局尺寸
//--------------------------------------------------------------------
const int TILE_W = 256;                       // 与 fill_tile.comp 保持一致
int numTile = (imageW + TILE_W - 1) / TILE_W; // ⌈W/256⌉
int edgeW   = numTile * 2;                    // 每 tile 2 像素

//--------------------------------------------------------------------
// 统一创建 4 张贴图：RGBA8 / R32UI(depth) / R32UI(index) / RGBA32UI(edge)
//--------------------------------------------------------------------
GLuint leftColor , leftDepth , leftIndex , leftEdge ;
GLuint rightColor, rightDepth, rightIndex, rightEdge;

auto makeTarget4 = [&](GLuint &color, GLuint &depth,
                       GLuint &index, GLuint &edge)
{
    /* ---------- 1. 颜色 (RGBA8) ---------- */
    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, imageW, imageH);

    /* ---------- 2. 深度 (R32UI) ---------- */
    glGenTextures(1, &depth);
    glBindTexture(GL_TEXTURE_2D, depth);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, imageW, imageH);

    /* ---------- 3. index (R32UI) ---------- */
    glGenTextures(1, &index);
    glBindTexture(GL_TEXTURE_2D, index);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, imageW, imageH);

    /* ---------- 4. edge (RGBA32UI) ---------- */
    glGenTextures(1, &edge);
    glBindTexture(GL_TEXTURE_2D, edge);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32UI, edgeW, imageH);

    /* ---------- 统一清零 / 置未定义 ---------- */
    std::vector<uint8_t > clrInit (imageW * imageH * 4,           0);          // (0,0,0,0)
    std::vector<uint32_t> u32Zero (imageW * imageH,               0);          // 深度 0
    std::vector<uint32_t> u32Undef(imageW * imageH, 0xFFFFFFFFu);              // index 未定义
    std::vector<uint32_t> edgeZero(edgeW * imageH * 4,            0);          // edge 清零

    glBindTexture(GL_TEXTURE_2D, color);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0,0, imageW, imageH,
                    GL_RGBA, GL_UNSIGNED_BYTE, clrInit.data());

    glBindTexture(GL_TEXTURE_2D, depth);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0,0, imageW, imageH,
                    GL_RED_INTEGER, GL_UNSIGNED_INT, u32Zero.data());

    glBindTexture(GL_TEXTURE_2D, index);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0,0, imageW, imageH,
                    GL_RED_INTEGER, GL_UNSIGNED_INT, u32Undef.data());

    glBindTexture(GL_TEXTURE_2D, edge);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0,0, edgeW, imageH,
                    GL_RGBA_INTEGER, GL_UNSIGNED_INT, edgeZero.data());
};

/*--------------------------------------------------------------------
   调用：左右眼各来一次
  ------------------------------------------------------------------*/
makeTarget4(leftColor , leftDepth , leftIndex , leftEdge );
makeTarget4(rightColor, rightDepth, rightIndex, rightEdge);

  //--------------------------------------------------------------------
  // ❷ 编译两个 compute shader
  //--------------------------------------------------------------------
  GLuint warpProg = createComputeProgram("warp.comp");   // Pass-A

  //--------------------------------------------------------------------
  // ❸ Pass-A : 前向 warp & 记录 index
  //--------------------------------------------------------------------
  auto warpEye = [&](GLuint dstC, GLuint dstD, GLuint dstI, int eyeSign) {
    glUseProgram(warpProg);

    /* --- 输入源纹理 --- */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, imageTex); // 原图颜色
    glUniform1i(glGetUniformLocation(warpProg, "srcColor"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthTex); // 原图深度
    glUniform1i(glGetUniformLocation(warpProg, "srcDepth"), 1);

    /* --- 输出绑定 --- */
    glBindImageTexture(2, dstC, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    glBindImageTexture(3, dstD, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    glBindImageTexture(4, dstI, 0, GL_FALSE, 0, GL_READ_WRITE,
                       GL_R32UI); // ★ index

    /* --- 参数 --- */
    float shiftScale = divergence * 0.01f * imageW * 0.5f * eyeSign;
    float shiftBias = -convergence * shiftScale;

    glUniform1i(glGetUniformLocation(warpProg, "orgWidth"), imageW);
    glUniform1i(glGetUniformLocation(warpProg, "orgHeight"), imageH);
    glUniform1i(glGetUniformLocation(warpProg, "padSize"), padSize);
    glUniform1i(glGetUniformLocation(warpProg, "paddedWidth"), paddedW);
    glUniform1f(glGetUniformLocation(warpProg, "shiftScale"), shiftScale);
    glUniform1f(glGetUniformLocation(warpProg, "shiftBias"), shiftBias);

    /* --- Dispatch --- */
    GLuint gx = (paddedW + 15) / 16;
    GLuint gy = (imageH + 15) / 16;
    glDispatchCompute(gx, gy, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  };

  //--------------------------------------------------------------------
  // ❺ 执行 warp
  //--------------------------------------------------------------------
  warpEye(leftColor, leftDepth, leftIndex, +1);    // 左眼 warp
  warpEye(rightColor, rightDepth, rightIndex, -1); // 右眼 warp
    // ---------- 4. 保存结果 ----------
  saveTexturePNG(leftColor, imageW, imageH, "left_eye_warped.png");
  saveTexturePNG(rightColor, imageW, imageH, "right_eye_warped.png");

  //--------------------------------------------------------------------
  // ❺ 执行 fill
  //--------------------------------------------------------------------
  GLuint tileProg   = createComputeProgram("fill_tile.comp");    // Pass B-1
  GLuint prefixProg = createComputeProgram("fill_prefix.comp");  // Pass B-2

  auto fillEye = [&](GLuint color, GLuint index, GLuint edge,
                   int eyeSign)
{
    /* ---------- Pass-B-1 : tile 内 shift_fill / fix / shift_fill ---------- */
    glUseProgram(tileProg);

    glBindImageTexture(2, color, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    glBindImageTexture(4, index, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    glBindImageTexture(5, edge , 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32UI);

    int numTile = (imageW + 255) / 256;

    glUniform1i(glGetUniformLocation(tileProg,"orgWidth"),  imageW);
    glUniform1i(glGetUniformLocation(tileProg,"orgHeight"), imageH);
    glUniform1i(glGetUniformLocation(tileProg,"eyeSign"),   eyeSign);

    /* dispatch:  X = tile 数,  Y = 行数 */
    glDispatchCompute(numTile, imageH, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    /* ---------- Pass-B-2 : tile 间前缀传播 ---------- */
    glUseProgram(prefixProg);

    glBindImageTexture(2, color, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    glBindImageTexture(4, index, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    glBindImageTexture(5, edge , 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32UI);

    glUniform1i(glGetUniformLocation(prefixProg,"orgWidth"),  imageW);
    glUniform1i(glGetUniformLocation(prefixProg,"orgHeight"), imageH);
    glUniform1i(glGetUniformLocation(prefixProg,"numTile"),   numTile);

    /* prefix pass 让 X=1、Y=行数 即可 */
    glDispatchCompute(1, imageH, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
};

    fillEye(leftColor , leftIndex , leftEdge , +1);
    fillEye(rightColor, rightIndex, rightEdge, -1);

  // ---------- 4. 保存结果 ----------
  saveTexturePNG(leftColor, imageW, imageH, "left_eye_filled.png");
  saveTexturePNG(rightColor, imageW, imageH, "right_eye_filled.png");

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}