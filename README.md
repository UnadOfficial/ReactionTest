# ReactionTest

(也许是) 精度最高的反应测试软件  
(Maybe) the most accurate reaction test software.

## 功能特点 / Features

- **高精度计时**: 使用 `QueryPerformanceCounter` 实现微秒级精度
- **低延迟渲染**: 基于 Direct3D 9，使用 `D3DPRESENT_INTERVAL_IMMEDIATE` 禁用垂直同步
- **精确触发**: 使用多媒体定时器 (`timeSetEvent`) 实现 1ms 精度的随机延迟
- **高优先级**: 进程和线程优先级提升，减少系统调度延迟
- **全屏显示**: 无边框全屏模式，消除窗口管理器延迟

## 技术实现 / Technical Implementation

- **高精度计时器**: `QueryPerformanceCounter` / `QueryPerformanceFrequency`
- **图形渲染**: Direct3D 9 (DirectX)
- **精确事件触发**: Windows Multimedia Timer API (`mmsystem.h`)
- **优先级优化**: 
  - 进程优先级：`HIGH_PRIORITY_CLASS`
  - 线程优先级：`THREAD_PRIORITY_TIME_CRITICAL` (测试期间)

## 编译方法 / Build Instructions

使用 MinGW 编译:

```bash
gcc -o ReactionTest.exe ReactionTest.c -ld3d9 -lwinmm -mwindows
```

或使用 MSVC 编译:

```bash
cl ReactionTest.c d3d9.lib winmm.lib user32.lib gdi32.lib
```

## 使用方法 / Usage

1. 运行程序，进入全屏模式
2. 屏幕显示白色，提示 "Click to start"
3. 点击鼠标左键开始测试
4. 等待屏幕变为绿色
5. 看到绿色后立即点击鼠标左键
6. 显示反应时间（毫秒），点击重新开始

**退出**: 按 `ESC` 键退出程序

## 状态说明 / States

| 状态 | 显示 | 操作 |
|------|------|------|
| 等待开始 | 白色背景，"Click to start" | 点击开始 |
| 等待绿色 | 白色背景 | 等待变绿（提前点击会显示 "Too early!"） |
| 等待反应 | 绿色背景 | 立即点击 |
| 显示结果 | 绿色背景，显示反应时间 | 点击重新开始 |
| 过早点击 | 白色背景，"Too early! Click to retry" | 点击重试 |

## 系统要求 / Requirements

- Windows 操作系统
- DirectX 9.0 或更高版本
- 支持硬件顶点处理的显卡

## 许可证 / License

详见 [LICENSE](LICENSE) 文件

## 注意事项 / Notes

- 程序需要全屏运行以获得最佳精度
- 关闭其他应用程序以减少系统干扰
- 结果可能受显示器刷新率和输入设备影响
