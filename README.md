# OpenGL 立体图像生成与分块修补

本项目基于 OpenGL，利用深度图和单张图像自动生成高质量的立体图像对（左眼/右眼），并采用高效的分块（tile-based）修补算法，支持任意分辨率。

## 功能亮点
- 输入单张彩色图像（image.png）和深度图（depth.exr）
- OpenGL Compute Shader 实现高效视差变换与洞填补
- 分块+前缀扫描算法，支持超大分辨率
- 输出左/右眼立体图像

## 依赖环境
- **OpenGL 4.3+**（需支持 Compute Shader）
- **GLFW**：窗口与上下文管理
- **GLAD**：OpenGL 扩展加载
- **tinyexr**：读取 EXR 深度图
- **stb_image/stb_image_write**：PNG 读写
- **CMake 3.10+**，推荐 Windows 10/11 + Visual Studio 2019+

> 推荐用 vcpkg 私有模式自动安装依赖，见下文。

## 快速安装

### 一键安装依赖
```bash
install_private.bat
```

### 手动安装（如需）
- vcpkg 全局模式：
```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat
vcpkg install glfw3:x64-windows glad:x64-windows tinyexr:x64-windows stb:x64-windows
```
- 或手动下载各依赖库

## 构建与运行

### CMake 构建
```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg根目录]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### 运行
1. 确保 `image.png` 和 `depth.exr` 在项目根目录
2. 运行生成的可执行文件
3. 输出：`left_eye_filled.png`、`right_eye_filled.png`（修补后）

## 项目结构
```
main.cpp              # 主程序，OpenGL流程与调度
CMakeLists.txt        # 构建配置
warp.comp             # 视差变换+深度竞争（compute shader）
fill_tile.comp        # 分块修补 Pass-1（tile 内）
fill_prefix.comp      # 分块修补 Pass-2（tile 间前缀传播）
normalize.frag        # 归一化片元着色器
pad_lr.frag           # 边缘复制填充
screen.vert           # 全屏顶点着色器
image.png             # 输入图像
depth.exr             # 输入深度图
xptest/               # 测试输出/中间结果
```

## 算法流程简介

1. **视差变换与洞生成**
   - `warp.comp`：根据深度和视差参数，将像素投射到目标视图，记录最大深度和像素索引，生成初步左右眼图像（含洞）。
2. **分块修补（Tile+Prefix）**
   - `fill_tile.comp`：每 256 像素为一 tile，tile 内用共享内存做 shift_fill + fix，记录 tile 边界像素到 edgeTex。
   - `fill_prefix.comp`：对 edgeTex 做前缀传播，跨 tile 补齐所有洞，支持任意宽度。
3. **输出**
   - 保存修补后的左右眼图像。

## 主要着色器说明
- `warp.comp`：深度竞争与像素投射，生成带洞的左右眼图
- `fill_tile.comp`：tile 内 shift_fill + fix，记录边界
- `fill_prefix.comp`：tile 间前缀传播，补齐所有洞

## 常见问题
- **着色器编译失败**：请确保显卡支持 OpenGL 4.3+ 和 Compute Shader
- **输出全黑/无图像**：检查输入文件路径和格式
- **运行崩溃**：建议用 Release 模式，确保依赖正确安装

## 参考与致谢
- OpenGL 官方文档与 Compute Shader 教程
- vcpkg、GLFW、GLAD、tinyexr、stb_image

---
如有问题欢迎 issue 或联系作者。 