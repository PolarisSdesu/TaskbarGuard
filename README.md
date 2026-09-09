# Taskbar Guard Lite

一个只有约 100 KB 的 Windows 任务栏控制工具。

Windows 开启“自动隐藏任务栏”后，鼠标移动到屏幕边缘或应用收到通知时，任务栏仍可能自动弹出。Taskbar Guard Lite 提供一个“强制隐藏”模式，让任务栏保持隐藏，避免遮挡 Photoshop、Illustrator、Premiere Pro 等应用底部的操作区域。

## 功能

- 使用全局快捷键切换任务栏状态
- 阻止鼠标触碰屏幕边缘时弹出任务栏
- 阻止通知消息唤出任务栏
- 同时控制主屏幕和副屏幕任务栏
- 托盘图标双击切换模式
- 支持设置开机自动启动
- 右键菜单跟随 Windows 深色或浅色主题
- 退出程序时自动恢复任务栏
- 单实例运行，避免重复启动
- 纯 Win32 C++ 实现，无需安装 .NET 或其他运行库

## 下载与运行

根据设备架构运行对应版本：

```text
TaskbarGuard-x64.exe
TaskbarGuard-arm64.exe
```

程序启动后不会显示主窗口，而是常驻系统托盘。

> 程序目前没有数字签名。Windows SmartScreen 可能显示“未知发布者”，这是因为缺少代码签名证书，并不代表程序需要管理员权限。

## 操作方法

| 操作 | 效果 |
| --- | --- |
| `Ctrl + Alt + T` | 切换普通模式与强制隐藏模式 |
| 双击托盘图标 | 切换普通模式与强制隐藏模式 |
| 右键托盘图标 | 打开设置菜单 |
| 右键菜单 → 开机自动启动 | 启用或关闭开机启动 |
| 右键菜单 → 退出并恢复任务栏 | 恢复任务栏并退出程序 |

### 普通模式

保留 Windows 当前的任务栏行为。程序不会更改系统的“自动隐藏任务栏”设置。

### 强制隐藏模式

持续隐藏所有任务栏窗口。鼠标触碰屏幕边缘或收到通知时，任务栏也不会停留在屏幕上。

需要临时使用开始菜单或任务栏时，再按一次 `Ctrl + Alt + T` 即可恢复普通模式。

## 深色模式

托盘右键菜单会跟随 Windows 的“应用模式”设置：

- Windows 使用深色主题时显示深色菜单
- Windows 使用浅色主题时显示浅色菜单
- 运行期间切换系统主题后自动刷新

深色菜单支持依赖 Windows 10 1903 或更高版本。旧版本 Windows 会使用系统默认菜单样式，不影响任务栏控制功能。

## 文件结构

```text
TaskbarGuard/
├── .github/workflows/release.yml
├── TaskbarGuard.cpp       # 完整的 Win32 C++ 源码
├── build.ps1              # 本地 x64 构建脚本
├── LICENSE                # MIT 许可证
└── README.md
```

## 从源码构建

### 环境要求

- Windows 10 或 Windows 11
- MinGW-w64 `g++`
- PowerShell

确认 `g++` 已加入 `PATH`：

```powershell
g++ --version
```

在项目根目录运行：

```powershell
.\build.ps1
```

构建脚本相当于执行：

```powershell
g++ -std=c++17 -Os -s -mwindows -municode -static `
    .\TaskbarGuard.cpp `
    -o .\TaskbarGuard-x64.exe `
    -lshell32 -ladvapi32
```

参数说明：

- `-Os`：优先缩小程序体积
- `-s`：移除调试符号
- `-mwindows`：构建无控制台窗口的 Windows 应用
- `-municode`：使用 Unicode 程序入口
- `-static`：静态链接运行库，生成可独立运行的单文件

如果正式版正在运行，Windows 会阻止构建脚本覆盖 EXE。请先从托盘菜单正常退出，再重新构建。ARM64 正式版由 GitHub Actions 使用 MSVC 交叉编译。

## 工作原理

程序通过 Win32 API 查找以下 Windows Explorer 任务栏窗口：

- `Shell_TrayWnd`：主任务栏
- `Shell_SecondaryTrayWnd`：副屏幕任务栏

进入强制隐藏模式后，程序会隐藏这些窗口，并用短周期计时器处理 Explorer 因鼠标触边或通知而重新显示任务栏的情况。退出强制隐藏模式或正常关闭程序时，会重新显示所有任务栏窗口。

程序不会结束或修改 Windows Explorer，也不会更改系统任务栏的自动隐藏配置。

## 故障恢复

如果程序被任务管理器强制结束、系统崩溃或断电，任务栏可能暂时保持隐藏。可以使用以下任意方法恢复：

1. 重新运行 Taskbar Guard Lite，然后从托盘菜单选择“退出并恢复任务栏”。
2. 按 `Ctrl + Shift + Esc` 打开任务管理器，找到“Windows 资源管理器”，选择“重新启动”。
3. 注销并重新登录 Windows。

## 已知限制

- 快捷键固定为 `Ctrl + Alt + T`，当前版本没有快捷键设置界面。
- 如果其他应用已经注册相同快捷键，程序会显示提示，此时仍可双击托盘图标切换。
- 强制结束进程时无法执行任务栏恢复逻辑；请尽量使用托盘菜单正常退出。
- Windows Explorer 更新后若改变任务栏窗口结构，相关功能可能需要适配。

## 系统支持

- Windows 11：支持
- Windows 10：支持
- Windows 8.1 及更早版本：未测试
- 处理器架构：当前构建为 64 位 x86（x64）

## 许可证

本项目采用 [MIT License](./LICENSE) 开源。

Copyright © 2026 PolarisS
