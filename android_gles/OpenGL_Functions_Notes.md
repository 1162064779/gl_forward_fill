# OpenGL ES 函数使用笔记

本文档记录了 `android_gles/main.cpp` 中使用的所有 OpenGL ES 函数的详细说明。

## 目录
- [着色器相关函数](#着色器相关函数)
- [纹理相关函数](#纹理相关函数)
- [计算着色器函数](#计算着色器函数)
- [状态查询函数](#状态查询函数)
- [帧缓冲相关函数](#帧缓冲相关函数)
- [资源管理函数](#资源管理函数)

---

## 着色器相关函数

### `glCreateShader(GLenum type)`
**功能**: 创建一个着色器对象
- **参数**: 
  - `type`: 着色器类型 (`GL_VERTEX_SHADER`, `GL_FRAGMENT_SHADER`, `GL_COMPUTE_SHADER`)
- **返回**: 着色器对象ID，失败返回0
- **用途**: 创建顶点、片段或计算着色器

### `glShaderSource(GLuint shader, GLsizei count, const GLchar **string, const GLint *length)`
**功能**: 为着色器对象提供源码
- **参数**:
  - `shader`: 着色器对象ID
  - `count`: 源码字符串数量
  - `string`: 源码字符串数组
  - `length`: 各字符串长度（NULL表示以null结尾）
- **用途**: 加载GLSL源码到着色器对象

### `glCompileShader(GLuint shader)`
**功能**: 编译着色器源码
- **参数**: `shader` - 着色器对象ID
- **用途**: 将GLSL源码编译为GPU可执行代码

### `glGetShaderiv(GLuint shader, GLenum pname, GLint *params)`
**功能**: 查询着色器对象参数
- **参数**:
  - `shader`: 着色器对象ID
  - `pname`: 查询参数类型 (`GL_COMPILE_STATUS`, `GL_INFO_LOG_LENGTH` 等)
  - `params`: 返回值指针
- **用途**: 检查编译状态、获取信息日志长度等

### `glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog)`
**功能**: 获取着色器编译日志
- **参数**:
  - `shader`: 着色器对象ID
  - `bufSize`: 缓冲区大小
  - `length`: 实际写入长度
  - `infoLog`: 日志缓冲区
- **用途**: 调试编译错误

### `glCreateProgram()`
**功能**: 创建一个程序对象
- **返回**: 程序对象ID，失败返回0
- **用途**: 创建着色器程序容器

### `glAttachShader(GLuint program, GLuint shader)`
**功能**: 将着色器附加到程序对象
- **参数**:
  - `program`: 程序对象ID
  - `shader`: 着色器对象ID
- **用途**: 组装完整的渲染管线

### `glLinkProgram(GLuint program)`
**功能**: 链接程序对象
- **参数**: `program` - 程序对象ID
- **用途**: 将附加的着色器链接成可执行程序

### `glGetProgramiv(GLuint program, GLenum pname, GLint *params)`
**功能**: 查询程序对象参数
- **参数**:
  - `program`: 程序对象ID
  - `pname`: 查询参数类型 (`GL_LINK_STATUS`, `GL_INFO_LOG_LENGTH` 等)
  - `params`: 返回值指针
- **用途**: 检查链接状态

### `glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog)`
**功能**: 获取程序链接日志
- **用途**: 调试链接错误

### `glUseProgram(GLuint program)`
**功能**: 激活指定的着色器程序
- **参数**: `program` - 程序对象ID（0表示取消绑定）
- **用途**: 切换当前使用的着色器程序

### `glGetUniformLocation(GLuint program, const GLchar *name)`
**功能**: 获取uniform变量的位置
- **参数**:
  - `program`: 程序对象ID
  - `name`: uniform变量名
- **返回**: uniform位置，-1表示未找到
- **用途**: 获取uniform变量索引用于设置值

### `glUniform1i(GLint location, GLint x)`
**功能**: 设置整型uniform变量值
- **参数**:
  - `location`: uniform位置
  - `x`: 整型值
- **用途**: 传递整型参数到着色器

### `glUniform1f(GLint location, GLfloat x)`
**功能**: 设置浮点型uniform变量值
- **参数**:
  - `location`: uniform位置
  - `x`: 浮点值
- **用途**: 传递浮点参数到着色器

---

## 纹理相关函数

### `glGenTextures(GLsizei n, GLuint *textures)`
**功能**: 生成纹理对象名称
- **参数**:
  - `n`: 生成数量
  - `textures`: 存储纹理ID的数组
- **用途**: 创建纹理对象标识符

### `glBindTexture(GLenum target, GLuint texture)`
**功能**: 绑定纹理对象
- **参数**:
  - `target`: 纹理目标 (`GL_TEXTURE_2D`, `GL_TEXTURE_CUBE_MAP` 等)
  - `texture`: 纹理对象ID（0表示解绑）
- **用途**: 激活纹理对象进行操作

### `glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)`
**功能**: 为2D纹理分配不可变存储空间 (OpenGL ES 3.0+)
- **参数**:
  - `target`: 纹理目标
  - `levels`: mipmap级数
  - `internalformat`: 内部格式 (`GL_RGBA8`, `GL_R32UI` 等)
  - `width`, `height`: 纹理尺寸
- **用途**: 现代纹理存储分配方式，性能更好

### `glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels)`
**功能**: 指定2D纹理图像数据
- **参数**:
  - `target`: 纹理目标
  - `level`: mipmap级别
  - `internalformat`: 内部存储格式
  - `width`, `height`: 图像尺寸
  - `border`: 边框宽度（必须为0）
  - `format`: 像素数据格式 (`GL_RGB`, `GL_RGBA`, `GL_RED` 等)
  - `type`: 像素数据类型 (`GL_UNSIGNED_BYTE`, `GL_FLOAT` 等)
  - `pixels`: 图像数据指针
- **用途**: 上传纹理数据

### `glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)`
**功能**: 更新2D纹理的子区域
- **参数**:
  - `target`: 纹理目标
  - `level`: mipmap级别
  - `xoffset`, `yoffset`: 起始偏移
  - `width`, `height`: 更新区域尺寸
  - `format`: 像素格式
  - `type`: 像素类型
  - `pixels`: 像素数据
- **用途**: 部分更新纹理内容，清零纹理

### `glTexParameteri(GLenum target, GLenum pname, GLint param)`
**功能**: 设置纹理参数
- **参数**:
  - `target`: 纹理目标
  - `pname`: 参数名 (`GL_TEXTURE_MIN_FILTER`, `GL_TEXTURE_MAG_FILTER` 等)
  - `param`: 参数值 (`GL_LINEAR`, `GL_NEAREST` 等)
- **用途**: 配置纹理过滤、包装模式等

### `glActiveTexture(GLenum texture)`
**功能**: 选择活动纹理单元
- **参数**: `texture` - 纹理单元 (`GL_TEXTURE0`, `GL_TEXTURE1` 等)
- **用途**: 切换当前操作的纹理单元

### `glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format)`
**功能**: 绑定纹理到图像单元 (计算着色器用)
- **参数**:
  - `unit`: 图像单元索引
  - `texture`: 纹理对象ID
  - `level`: mipmap级别
  - `layered`: 是否为分层纹理
  - `layer`: 层索引
  - `access`: 访问模式 (`GL_READ_ONLY`, `GL_WRITE_ONLY`, `GL_READ_WRITE`)
  - `format`: 图像格式
- **用途**: 为计算着色器提供图像读写访问

---

## 计算着色器函数

### `glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)`
**功能**: 启动计算着色器执行
- **参数**:
  - `num_groups_x`, `num_groups_y`, `num_groups_z`: 各维度的工作组数量
- **用途**: 执行并行计算任务

### `glMemoryBarrier(GLbitfield barriers)`
**功能**: 插入内存屏障
- **参数**: `barriers` - 屏障类型位掩码
  - `GL_SHADER_IMAGE_ACCESS_BARRIER_BIT`: 着色器图像访问
  - `GL_ALL_BARRIER_BITS`: 所有类型屏障
- **用途**: 确保内存操作完成，保证数据一致性

---

## 状态查询函数

### `glGetString(GLenum name)`
**功能**: 查询OpenGL状态字符串
- **参数**: `name` - 查询类型
  - `GL_VERSION`: OpenGL版本
  - `GL_EXTENSIONS`: 支持的扩展
  - `GL_VENDOR`: 厂商信息
  - `GL_RENDERER`: 渲染器信息
- **返回**: 字符串指针
- **用途**: 获取系统信息

### `glGetIntegerv(GLenum pname, GLint *data)`
**功能**: 查询整型状态值
- **参数**:
  - `pname`: 状态名称 (`GL_MAJOR_VERSION`, `GL_MINOR_VERSION` 等)
  - `data`: 返回值指针
- **用途**: 获取版本号等整型状态

### `glGetIntegeri_v(GLenum pname, GLuint index, GLint *data)`
**功能**: 查询索引化的整型状态值
- **参数**:
  - `pname`: 状态名称
  - `index`: 索引
  - `data`: 返回值指针
- **用途**: 查询计算着色器工作组限制等

---

## 帧缓冲相关函数

### `glGenFramebuffers(GLsizei n, GLuint *framebuffers)`
**功能**: 生成帧缓冲对象名称
- **参数**:
  - `n`: 生成数量
  - `framebuffers`: 存储FBO ID的数组
- **用途**: 创建帧缓冲对象

### `glBindFramebuffer(GLenum target, GLuint framebuffer)`
**功能**: 绑定帧缓冲对象
- **参数**:
  - `target`: 目标类型 (`GL_FRAMEBUFFER`, `GL_READ_FRAMEBUFFER` 等)
  - `framebuffer`: FBO ID（0表示默认帧缓冲）
- **用途**: 切换渲染目标

### `glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)`
**功能**: 将2D纹理附加到帧缓冲
- **参数**:
  - `target`: 帧缓冲目标
  - `attachment`: 附加点 (`GL_COLOR_ATTACHMENT0`, `GL_DEPTH_ATTACHMENT` 等)
  - `textarget`: 纹理目标
  - `texture`: 纹理对象ID
  - `level`: mipmap级别
- **用途**: 设置渲染目标纹理

### `glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)`
**功能**: 从帧缓冲读取像素数据
- **参数**:
  - `x`, `y`: 起始坐标
  - `width`, `height`: 读取区域尺寸
  - `format`: 像素格式
  - `type`: 像素数据类型
  - `pixels`: 存储缓冲区
- **用途**: 从GPU读取渲染结果

---

## 资源管理函数

### `glDeleteShader(GLuint shader)`
**功能**: 删除着色器对象
- **参数**: `shader` - 着色器对象ID
- **用途**: 释放着色器资源

### `glDeleteProgram(GLuint program)`
**功能**: 删除程序对象
- **参数**: `program` - 程序对象ID
- **用途**: 释放程序资源

### `glDeleteTextures(GLsizei n, const GLuint *textures)`
**功能**: 删除纹理对象
- **参数**:
  - `n`: 删除数量
  - `textures`: 纹理ID数组
- **用途**: 释放纹理资源

### `glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)`
**功能**: 删除帧缓冲对象
- **参数**:
  - `n`: 删除数量
  - `framebuffers`: FBO ID数组
- **用途**: 释放帧缓冲资源

---

## 使用注意事项

### 资源管理
- 所有创建的GL对象都需要手动删除
- 使用RAII模式管理资源生命周期
- 在程序退出前清理所有GL资源

### 错误检查
- 重要操作后应调用 `glGetError()` 检查错误
- 着色器编译和程序链接必须检查状态
- 使用 `glGetShaderInfoLog()` 和 `glGetProgramInfoLog()` 调试

### 性能优化
- 尽量减少状态切换
- 批量处理相同状态的操作
- 合理使用纹理缓存和uniform缓存
- 避免频繁的CPU-GPU数据传输

### OpenGL ES版本兼容性
- ES 3.0: 基础计算着色器支持
- ES 3.1: 增强的计算着色器功能
- ES 3.2: 更多高级特性
- 检查扩展支持情况

---

*本文档基于 android_gles/main.cpp 中的实际使用情况编写，持续更新中...* 