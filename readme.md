## TRobot (TI MSPM0G3507)

**T**GU **R**obot: The next robot embedded development framework.

> [!NOTE]
> - 由于电赛会有限制 `mspm0` 系列芯片的题，所以搓了一个 trobot 的分支项目 :yum:。
> - 本项目使用 CMake + GCC + OpenOCD 工具链，支持各类烧录器。
> - 该项目基于立创天猛星开发，参考：[【立创·天猛星MSPM0G3507开发板】入门手册](https://wiki.lckfb.com/zh-hans/tmx-mspm0g3507/keil-beginner/)。
> - 若要使用 `mspm0g3519`，需要改一改 `core` 里的 `CMakeLists.txt`。
> - 项目随缘更新，不保证功能完善，Welcome PRs。
> - 当前为裸机版本，因此有许多组件不可用，后续视情况添加 FreeRTOS 支持。

### 快速开始

#### 1. 替换你的 OpenOCD

由于目前主线 OpenOCD 还没有支持 MSPM0，所以需要替换为 TI 提供的 OpenOCD 分支。

下载链接：<https://software-dl.ti.com/ccs/esd/vscode/ti-embedded-debug/ti-openocd.html>

使用这个版本替换你原来的 OpenOCD 即可。

若你未曾使用过 OpenOCD，可以直接下载这个版本，主线支持的芯片和烧录器它都支持。

#### 2. 克隆项目

```bash
git clone https://github.com/lym12321/trobot_m0.git --recursive
```

#### 3. 构建项目

该项目提供了完整的 CMake 构建系统，支持使用 VSCode 或 CLion 等你喜欢的 IDE 来构建项目。

结合 OpenOCD，可实现对 MSPM0 系列芯片的烧录和调试。

#### 4. SYSCONFIG

你可以使用 [TI SYSCONFIG](https://www.ti.com.cn/tool/cn/SYSCONFIG) 打开项目下 `core/trobot.syscfg` 文件。

修改配置后需点击保存按钮生成相应文件：

![](assets/sysconfig.png)

其生成的 `device_linker.lds`、`ti_msp_dl_config.c` 和 `ti_msp_dl_config.h` 文件同样需保存在 `core` 路径下，只有这样才能被 CMake 正确包含。