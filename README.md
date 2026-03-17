# QtFeatureHub

QtFeatureHub 是一个基于 Qt5 和 CMake 的桌面示例项目，用来集中演示常见桌面功能模块的实现方式。当前仓库包含主窗口表格管理、SQLite 数据持久化、无边框窗口、自定义标题栏、PDF 预览、OpenGL 示例窗口、对话框交互和若干测试控件页面。

## 功能概览

- 主窗口表格增删改操作
- 使用 SQLite 保存和加载人员数据
- 无边框窗口与自定义标题栏
- PDF 文件打开、缩放、缩略图和打印
- OpenGL 测试窗口
- 自定义对话框示例
- 测试控件窗口

## 技术栈

- C++17
- CMake 3.16+
- Qt 5
  - Core
  - Gui
  - Widgets
  - Sql
  - PrintSupport
  - Xml
  - Concurrent
- Poppler C++

## 目录结构

```text
.
|-- CMakeLists.txt
|-- resources/
|   `-- resources.qrc
`-- src/
    |-- CMakeLists.txt
    |-- main.cpp
    |-- customize/      # 无边框窗口、自定义标题栏
    |-- database/       # SQLite 数据访问
    |-- dialogs/        # 对话框示例
    `-- widgets/        # 主窗口、PDF、OpenGL、测试页面等
```

## 环境要求

在当前仓库配置下，Windows 开发环境至少需要以下依赖：

- Qt 5 开发包
- CMake
- 支持 Qt 的 MinGW 编译器
- Poppler C++ 库

项目中的 CMake 默认按以下方式查找 Poppler：

```cmake
set(POPPLER_ROOT "C:/msys64/mingw64" CACHE PATH "Poppler install root")
```

如果你的 Poppler 不在这个位置，需要在配置阶段显式传入 `POPPLER_ROOT`。

## 构建

### 方式一：使用 VS Code 现有任务

工作区已经提供了构建任务：

- `CMake: build`
- `Run QtFeatureHub executable`

如果本地环境变量已经配置好 Qt 和 Poppler 运行时路径，可以直接使用这些任务完成构建和启动。

### 方式二：命令行构建

下面示例适用于 Windows + MinGW + Qt5：

```powershell
cmake -S . -B build `
  -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_PREFIX_PATH="E:/Softwore/Qt5.14.2/5.14.2/mingw73_64" `
  -DPOPPLER_ROOT="C:/msys64/mingw64"

cmake --build build --config Debug
```

## 运行

构建完成后，可执行文件位于下面位置：

```text
build/src/QtFeatureHub.exe
```

运行前请确保以下目录已经加入 `PATH`，否则程序可能因为缺少 Qt 或 Poppler DLL 无法启动：

- Qt 的 `bin` 目录
- MinGW 的 `bin` 目录
- Poppler 对应的运行时目录

当前工作区任务里使用的是以下路径思路，可作为参考：

```text
C:/msys64/mingw64/bin
E:/Softwore/Qt5.14.2/5.14.2/mingw73_64/bin
```

## 数据说明

- 主窗口启动时会在程序目录下创建或打开 `QtFeatureHub.db`
- 当前表格数据会优先从 SQLite 加载
- 如果数据库打开失败，程序仍可作为内存表格示例继续运行

## 适合的用途

- 学习 Qt Widgets 项目组织方式
- 参考 CMake + Qt5 工程配置
- 演示本地数据库、PDF 预览和窗口定制能力
- 作为后续功能实验的基础工程

## 后续可补充内容

- 添加界面截图
- 补充各模块的详细说明
- 增加依赖安装脚本
- 增加打包与发布说明