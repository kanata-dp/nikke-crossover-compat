# NIKKE CrossOver Compatibility

**让《胜利女神：NIKKE》Windows PC 版在 Apple Silicon Mac 上通过 CrossOver 运行。**

[English](README.en.md) · [验证记录](docs/VALIDATION.md) · [技术设计](docs/ARCHITECTURE.md)

这是面向 NIKKE 的实验性兼容补丁。它针对本次测试中遇到的启动异常、部分 Wine 接口缺失和背景视频黑屏问题，并提供固定在 CrossOver 中的启动入口。

**2026-09-26 更新：修复登录时「LEVEL INFINITE CAPTCHA NEW」验证码窗口白屏。** 验证码使用独立的浏览器进程，旧版启动器修复未覆盖它。已实测滑块出现、用户手动完成验证；NIKKE PC 国际服 **152.8.13** 随后进入大厅，页面切换正常。

本次为 **0.2.1 实验性源码更新**，新增验证码浏览器修复的源码、安装及还原工具。详见 [验证码修复与安装方法](docs/UPDATE-2026-09-26.zh-CN.md)。Wine 内核/媒体补丁沿用 0.2.0；此前 152.8.11 的战斗和爬塔记录见 [上次更新](docs/UPDATE-2026-09-22.zh-CN.md)。

## 它解决什么问题？

- **启动兼容性：**处理本机 Rosetta 无法正确执行的少量 NOP 指令形式，修正特权指令异常分类，并补充游戏启动过程中调用的部分 Wine 内核接口。
- **背景动画黑屏：**让视频播放尽早使用应用支持的软件回退路径。
- **登录验证码白屏：**为独立的 INTL WebView 宿主添加软件渲染参数，让验证码正常显示；验证仍由玩家手动完成。
- **后续启动：**把配置和运行时保存在持久目录，直接从 CrossOver 的程序列表打开官方 NIKKE 启动器。
- **画面模糊：**支持在这个独立容器中开启 CrossOver 高分辨率模式；本机窗口配置由 1388×781 提高至 2202×1340。

这些是本机已观察到的效果，不代表兼容所有使用 ACE 的游戏，也不是所有 NIKKE 启动报错的通用修复。

## 使用前需要什么？

本次验证环境为 **Apple M4 Max、macOS 27.0、CrossOver 26.1、NIKKE 152.8.13 国际服**。此前的战斗记录使用 152.8.11；更早记录来自 macOS 26.6.2。其他硬件、CrossOver 版本和后续游戏更新尚未验证。

你需要：

1. 已安装并可使用的 CrossOver 26.1 和 Rosetta。
2. 一个已经安装 NIKKE Windows PC 版的 CrossOver 容器，官方启动器能够打开并登录。本项目默认安装位置为 `C:\NIKKE\Launcher`。
3. Xcode Command Line Tools、Python 3、Bison 3 和 MinGW-w64，用来从源码构建补丁。

本仓库只提供源码，不包含游戏、ACE 文件、CrossOver 二进制或账号数据。测试环境此前已使用 [li-miniloader-wine-fix](https://github.com/Dorin130/li-miniloader-wine-fix) 以及 CEF 启动器修复；这些依赖不由本项目安装。若官方启动器本身打不开，应先解决启动器问题。

## 只遇到验证码白屏？

如果启动器能打开，但登录验证是白色空窗，可单独安装这次的 [验证码修复](docs/UPDATE-2026-09-26.zh-CN.md)，无需重编 Wine 或重新下载游戏。它只需要 Python 3 和 MinGW-w64，安装后继续使用原来的 CrossOver 入口。已经应用本次修复且能正常验证的用户无需重复安装。

## 安装

以下命令在本仓库根目录执行。先完全退出源容器里的游戏、启动器和后台进程。

### 1. 构建兼容层

```sh
make
make test
```

### 2. 构建 Wine 补丁模块

从 CodeWeavers 下载 [CrossOver 26.1 官方源码包](https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.1.0.tar.gz)，然后运行：

```sh
python3 scripts/build_wine_modules.py \
    --archive /absolute/path/to/crossover-sources-26.1.0.tar.gz \
    --output local/wine-modules-0.2.0

python3 scripts/prepare_runtime.py \
    --output local/runtime-0.2.0 \
    --modules local/wine-modules-0.2.0/build
```

构建脚本默认包含本次更新、线程所属进程接口及最小 Wine `lsass.exe` 组件，并验证源码包的固定 SHA-256。Bison 默认路径为 `/opt/homebrew/opt/bison/bin/bison`，不同安装位置可通过 `--bison` 指定。

### 3. 添加 CrossOver 固定入口

把下面的 `YOUR_NIKKE_BOTTLE` 换成你现有 NIKKE 容器的名称：

```sh
python3 scripts/install_crossover_entry.py \
    --source-prefix "$HOME/Library/Application Support/CrossOver/Bottles/YOUR_NIKKE_BOTTLE" \
    --source-runtime local/runtime-0.2.0 \
    --source-app build/NopBridgeLab.app \
    --source-bridge build/libnop_bridge.dylib
```

脚本使用 APFS 克隆创建独立的 **NIKKE-Compatibility** 容器，保留已下载资源。原始容器仍然保留；已有同名目标不会被覆盖。复制的账号状态只留在本机。

运行时存放在 `~/Library/Application Support/NIKKE Compatibility`，依赖现有的 CrossOver 安装。不要删除该目录；不再需要保留临时测试目录才能启动。

## 已安装旧版，如何升级？

GitHub 源码更新不会自动替换本机运行时。退出旧容器后，按上面的步骤重新构建，使用新的输出目录；然后使用下面的安装命令（替换源容器名称）：

**游戏更新、更新仓库、更新固定入口中的兼容运行时，是三件不同的事。** 旧入口仍可能指向旧版运行时；验证码修复也不会升级它。如果已安装 0.2.0 且游戏运行正常，本次只需按上面的独立说明处理验证码白屏。

```sh
python3 scripts/install_crossover_entry.py \
    --source-prefix "$HOME/Library/Application Support/CrossOver/Bottles/YOUR_NIKKE_BOTTLE" \
    --source-runtime local/runtime-0.2.0 \
    --source-app build/NopBridgeLab.app \
    --source-bridge build/libnop_bridge.dylib \
    --bottle-name NIKKE-Compatibility-152 \
    --menu-name "NIKKE Compatibility 152" \
    --support "$HOME/Library/Application Support/NIKKE Compatibility 152"
```

新入口确认可用前，保留原入口和运行时。新安装会在复制的容器中配置 Wine 系统进程组件，不影响 macOS 服务或原容器。

## 以后怎么启动？

重新打开 CrossOver，进入：

**NIKKE-Compatibility → NIKKE Compatibility → 官方启动器的“开始游戏”**

如需更清楚的画面，在这个容器右侧开启 **高分辨率模式**，按提示重启容器。安装时可用 `--menu-name` 自定义入口名称；本机测试使用的是“NIKKE 兼容版”。

当前启动配置使用已测试的 **DXVK**。在 CrossOver 中改选其他图形后端不会自动改写此专用启动配置，其他后端需另行配置和验证。

## 已知限制

- 当前配置已实测可玩，但两个后台 ACE CORE 驱动进程仍有异常退出记录；这不代表所有保护组件或检查均正常，也不是官方支持声明。
- 发布源码已去除临时诊断和内存快照代码。干净构建的接口测试与用户实玩验证分别记录；新安装流程尚未完成从安装到战斗的整体验证，见 [验证记录](docs/VALIDATION.md)。
- 未做长期稳定性、定量 FPS 或所有过场测试。CrossOver 或游戏更新后可能需要重新适配。

本项目修改 Wine 兼容层，不分发或修改游戏、ACE 二进制，也不把失败的接口查询替换为固定成功值。部分接口仍明确返回不支持。

## 开发与贡献

原生回归测试：

```sh
make test
```

Windows/Wine 接口测试需要独立、可丢弃的测试容器：

```sh
python3 scripts/test_windows.py --prefix /absolute/path/to/test-bottle \
    --runtime local/runtime-0.2.0
python3 scripts/test_wine_modules.py \
    --prefix /absolute/path/to/test-bottle \
    --runtime local/runtime-0.2.0
```

上述测试默认包含线程所属进程、真实退出状态、线程上下文及映射生命周期。发布纯源码包：

```sh
python3 scripts/package_source.py
```

反馈问题时请附上 Mac 型号、macOS/CrossOver 版本、使用的图形后端和出现问题的具体步骤。请先从日志里去掉登录信息、令牌和账号标识，再附上必要片段。

## 许可证与致谢

采用 **LGPL-2.1-or-later**，详见 [LICENSE](LICENSE) 与 [第三方来源说明](THIRD_PARTY.md)。

感谢 Wine、CodeWeavers、DW-Proton、Endfield_FineWine 及此前启动器修复项目提供的公开工作。NIKKE、CrossOver 和 Rosetta 均为各自权利人的产品；本项目是独立的社区兼容研究。
