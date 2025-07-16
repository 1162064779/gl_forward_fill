# Android OpenGL ES Stereo Generator

这是一个基于OpenGL ES 3.1的立体图像生成器，专门为Android平台优化。项目使用EGL进行离屏渲染，支持GPU加速的立体图像处理。

## 项目结构

```
android_gles/
├── CMakeLists.txt          # CMake构建配置
├── main.cpp               # 主程序入口
├── build_android.bat      # Android编译脚本
├── copy_assets.bat        # 资源文件复制脚本
├── README.md              # 项目文档
├── include/               # 头文件目录
│   ├── stb_image.h        # STB图像库
│   ├── stb_image_write.h  # STB图像写入库
│   └── tinyexr.h          # EXR格式支持
├── shaders/               # OpenGL ES着色器
│   ├── warp_es.comp       # 图像扭曲计算着色器
│   ├── fill_tile_es.comp  # 瓦片填充计算着色器
│   └── fill_prefix_es.comp # 前缀填充计算着色器
└── assets/                # 资源文件目录
    ├── image.png          # 输入图像
    └── depth.exr          # 深度图
```

## 工作原理

程序从单张输入图像(`image.png`)和对应的深度图(`depth.exr`)生成左右眼立体图像：

1. **输入**：一张`image.png`和一张`depth.exr`
2. **处理**：根据深度图生成左右眼的立体图像
3. **输出**：左眼和右眼的扭曲图像

### 处理流程

1. **扭曲阶段**：根据深度信息对图像进行视差扭曲
2. **填充阶段**：填充扭曲后产生的空白区域
3. **边缘处理**：处理图像边缘的填充

## 系统要求

- Android NDK r25c 或更高版本
- CMake 3.18.1 或更高版本
- MinGW-w64 (Windows)
- Android设备支持OpenGL ES 3.1

## 编译步骤

### 1. 准备环境

确保已安装以下工具：
- Android NDK
- CMake
- MinGW-w64

### 2. 修改NDK路径

编辑 `build_android.bat` 文件，将 `NDK_PATH` 变量设置为你的NDK安装路径：

```batch
set NDK_PATH=D:\ndk\android-ndk-r25c
```

### 3. 准备资源文件

将输入图像文件复制到 `assets/` 目录：
- `image.png` - 输入图像
- `depth.exr` - 深度图

运行复制脚本：
```batch
copy_assets.bat
```

### 4. 编译项目

运行编译脚本：

```batch
build_android.bat
```

编译成功后，可执行文件将生成在 `build/` 目录中。

## 部署和运行

### 1. 连接Android设备

确保设备已启用USB调试并连接到电脑：

```bash
adb devices
```

### 2. 推送可执行文件

```bash
adb push build/OpenGLStereoGenerator /data/local/tmp/
```

### 3. 设置执行权限

```bash
adb shell chmod +x /data/local/tmp/OpenGLStereoGenerator
```

### 4. 运行程序

```bash
adb shell /data/local/tmp/OpenGLStereoGenerator
```

### 5. 查看日志

```bash
adb logcat -s OpenGLStereo
```

## 输出文件

程序运行完成后，将在当前目录生成以下文件：
- `depth.png` - 深度图可视化（用于调试）
- `left_eye_warped.png` - 左眼扭曲结果
- `right_eye_warped.png` - 右眼扭曲结果
- `left_eye_result.png` - 左眼最终结果（包含填充）
- `right_eye_result.png` - 右眼最终结果（包含填充）

## 技术特性

### OpenGL ES 3.1支持
- 使用计算着色器进行GPU加速处理
- 支持图像存储对象(Image Storage Objects)
- 离屏渲染，无需显示窗口

### EGL集成
- 自动初始化EGL上下文
- 创建离屏渲染Surface
- 支持多种Android设备

### 图像处理
- 支持PNG和EXR格式
- 基于深度的立体图像生成
- 自动填充空白区域
- 瓦片化处理提高性能

## 参数调整

可以在`main.cpp`中调整以下参数：

```cpp
const float divergence = 2.0f;    // 发散度
const float convergence = 0.0f;   // 汇聚度
```

## 故障排除

### 编译错误
1. 检查NDK路径是否正确
2. 确保CMake版本满足要求
3. 检查MinGW-w64是否正确安装

### 运行时错误
1. 检查设备是否支持OpenGL ES 3.1
2. 确保资源文件存在且格式正确
3. 查看adb logcat输出获取详细错误信息

### 性能优化
1. 调整计算着色器的工作组大小
2. 优化纹理格式和内存布局
3. 使用适当的同步原语

## 许可证

本项目基于MIT许可证开源。

## 贡献

欢迎提交Issue和Pull Request来改进项目。 