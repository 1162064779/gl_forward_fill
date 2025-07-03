# OpenGL 立体图像生成器

这个项目使用OpenGL从单张图像和深度图生成立体图像对（左眼和右眼图像）。

## 功能

- 读取 `image.png` 图像文件
- 读取 `depth.exr` 深度图文件
- 使用OpenGL着色器进行立体图像生成
- 输出 `openglLeft.png` 和 `openglRight.png` 立体图像对

## 依赖项

### 必需库
- **OpenGL**: 图形渲染API
- **GLFW**: 窗口管理和输入处理
- **GLEW**: OpenGL扩展加载库
- **OpenEXR**: 用于读取EXR格式的深度图
- **stb_image**: 用于读取PNG图像

### 系统要求
- Windows 10/11
- Visual Studio 2019 或更高版本（推荐）
- CMake 3.10 或更高版本

## 安装依赖

### 项目私有模式（推荐）

这个项目使用vcpkg的项目私有模式，依赖会安装在项目目录下的`vcpkg_installed/`文件夹中。

#### 自动安装

```bash
# 使用项目私有安装脚本
install_private.bat
```

#### 手动安装

```bash
# 确保vcpkg已安装并添加到PATH
# 在项目根目录运行
vcpkg install --triplet=x64-windows
```

### 全局模式（备选）

如果你想使用全局vcpkg安装：

```bash
# 安装vcpkg（如果还没有安装）
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat

# 安装依赖
vcpkg install glfw3:x64-windows
vcpkg install glew:x64-windows
vcpkg install openexr:x64-windows
vcpkg install stb:x64-windows
```

### 手动安装

1. **GLFW**: 从 https://www.glfw.org/ 下载
2. **GLEW**: 从 http://glew.sourceforge.net/ 下载
3. **OpenEXR**: 从 https://www.openexr.com/ 下载
4. **stb_image**: 从 https://github.com/nothings/stb 下载

## 构建项目

### 使用CMake

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg根目录]/scripts/buildsystems/vcpkg.cmake

# 构建项目
cmake --build . --config Release
```

### 使用Visual Studio

1. 打开CMakeLists.txt文件
2. 配置CMake设置
3. 构建项目

## 运行

1. 确保 `image.png` 和 `depth.exr` 文件在当前目录
2. 运行生成的可执行文件
3. 程序将生成 `openglLeft.png` 和 `openglRight.png` 文件
4. 按ESC键退出程序

## 项目结构

```
opengl/
├── main.cpp                 # 主程序文件
├── CMakeLists.txt           # CMake构建配置
├── vcpkg.json              # vcpkg依赖配置
├── install_private.bat      # 项目私有依赖安装脚本
├── build.bat               # 构建脚本
├── README.md               # 项目说明
├── image.png               # 输入图像
├── depth.exr               # 输入深度图
├── vcpkg_installed/        # 项目私有依赖（安装后生成）
├── openglLeft.png          # 输出左眼图像
└── openglRight.png         # 输出右眼图像
```

## 算法说明

程序使用OpenGL着色器进行立体图像生成：

1. **顶点着色器**: 处理几何变换和纹理坐标
2. **片段着色器**: 实现立体图像生成算法
   - 读取原始图像颜色
   - 读取深度信息
   - 根据深度计算视差偏移
   - 生成左眼和右眼图像

## 注意事项

- 确保输入图像和深度图具有相同的分辨率
- 深度图应该是单通道的浮点数据
- 程序需要支持OpenGL 3.3或更高版本

## 故障排除

### 常见问题

1. **找不到OpenGL**: 确保安装了最新的显卡驱动
2. **GLFW初始化失败**: 检查GLFW库是否正确安装
3. **OpenEXR读取失败**: 确保depth.exr文件格式正确
4. **着色器编译错误**: 检查OpenGL版本支持

### 调试

程序会输出详细的错误信息到控制台，包括：
- 图像加载状态
- 深度图加载状态
- 着色器编译错误
- OpenGL错误信息 