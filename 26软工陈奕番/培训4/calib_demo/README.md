# Workbuddy 相机标定练习

## 项目结构

```text
pratice/
├── CMakeLists.txt
├── README.md
├── workbuddy.cpp
└── cam_mix_img/
    ├── README.md
    └── 放置标定图片（至少 6 张 JPG 或 PNG）
```

`build/` 是 CMake 生成的构建目录，不需要手动创建或修改。

## 环境要求

- CMake 3.16 或更新版本
- 支持 C++17 的编译器
- OpenCV 开发库（包含 CMake 可查找的 `OpenCVConfig.cmake`）

如果 CMake 找不到 OpenCV，请安装 OpenCV 开发包，或设置 `OpenCV_DIR` 指向包含 `OpenCVConfig.cmake` 的目录。

## 编译

在 `pratice` 目录下执行：

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## 准备图片并运行

将至少 6 张棋盘格照片放进 `cam_mix_img/`。棋盘格应有 11 列、8 行方格（即 10 列、7 行内角点），图片尺寸应一致。

程序使用相对目录 `cam_mix_img`，所以请从 `pratice` 目录启动：

```powershell
.\build\workbuddy.exe
```

若使用 Visual Studio 生成器，程序通常位于 `build\Release\workbuddy.exe`；CMake 已将 Visual Studio 的调试工作目录设为 `pratice`。

运行后会在当前工作目录输出 `undistort_before.jpg` 和 `undistort_after.jpg`。没有图片时，程序会提示补充标定图片。
