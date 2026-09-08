# NIKKE CrossOver Compatibility

**让《胜利女神：NIKKE》Windows PC 版在 Apple Silicon Mac 上通过 CrossOver 运行。**

[English](README.en.md) · [验证记录](docs/VALIDATION.md) · [技术设计](docs/ARCHITECTURE.md)

这是面向 NIKKE 的实验性兼容补丁。它针对本次测试中遇到的启动异常、部分 Wine 接口缺失和背景视频黑屏问题，并提供固定在 CrossOver 中的启动入口。

**已实测：进入大厅和战斗、背景动画正常；通过固定入口重新启动并开启高清后，用户确认超过 10 分钟没有 ACE 弹窗，画面明显更清楚。** 帧率体感正常，尚未做定量 FPS 和长时间稳定性测试。

## 它解决什么问题？

- **启动兼容性：**处理本机 Rosetta 无法正确执行的少量 NOP 指令形式，修正特权指令异常分类，并补充游戏启动过程中调用的部分 Wine 内核接口。
- **背景动画黑屏：**让视频播放尽早使用应用支持的软件回退路径。
- **后续启动：**把配置和运行时保存在持久目录，直接从 CrossOver 的程序列表打开官方 NIKKE 启动器。
- **画面模糊：**支持在这个独立容器中开启 CrossOver 高分辨率模式；本机窗口配置由 1388×781 提高至 2202×1340。

这些是本机已观察到的效果，不代表兼容所有使用 ACE 的游戏，也不是所有 NIKKE 启动报错的通用修复。

## 使用前需要什么？

目前验证环境为 **Apple M4 Max、macOS 26.6.2、CrossOver 26.1**。其他硬件、CrossOver 版本和后续游戏更新尚未验证。

你需要：

1. 已安装并可使用的 CrossOver 26.1 和 Rosetta。
2. 一个已经安装 NIKKE Windows PC 版的 CrossOver 容器，官方启动器能够打开并登录。本项目默认安装位置为 `C:\NIKKE\Launcher`。
3. Xcode Command Line Tools、Python 3、Bison 3 和 MinGW-w64，用来从源码构建补丁。

本仓库只提供源码，不包含游戏、ACE 文件、CrossOver 二进制或账号数据。测试环境此前已使用 [li-miniloader-wine-fix](https://github.com/Dorin130/li-miniloader-wine-fix) 以及 CEF 启动器修复；这些依赖不由本项目安装。若官方启动器本身打不开，应先解决启动器问题。

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
    --archive /absolute/path/to/crossover-sources-26.1.0.tar.gz

python3 scripts/prepare_runtime.py \
    --output local/runtime-modules \
    --modules local/wine-modules/build
```

构建脚本会验证源码包的固定 SHA-256。Bison 默认路径为 `/opt/homebrew/opt/bison/bin/bison`，不同安装位置可通过 `--bison` 指定。

### 3. 添加 CrossOver 固定入口

把下面的 `YOUR_NIKKE_BOTTLE` 换成你现有 NIKKE 容器的名称：

```sh
python3 scripts/install_crossover_entry.py \
    --source-prefix "$HOME/Library/Application Support/CrossOver/Bottles/YOUR_NIKKE_BOTTLE" \
    --source-runtime local/runtime-modules \
    --source-app build/NopBridgeLab.app \
    --source-bridge build/libnop_bridge.dylib
```

脚本使用 APFS 克隆创建独立的 **NIKKE-Compatibility** 容器，保留已下载资源。原始容器仍然保留；已有同名目标不会被覆盖。复制的账号状态只留在本机。

运行时存放在 `~/Library/Application Support/NIKKE Compatibility`，依赖现有的 CrossOver 安装。不要删除该目录；不再需要保留临时测试目录才能启动。

## 以后怎么启动？

重新打开 CrossOver，进入：

**NIKKE-Compatibility → NIKKE Compatibility → 官方启动器的“开始游戏”**

如需更清楚的画面，在这个容器右侧开启 **高分辨率模式**，按提示重启容器。安装时可用 `--menu-name` 自定义入口名称；本机测试使用的是“NIKKE 兼容版”。

当前启动配置使用已测试的 **DXVK**。在 CrossOver 中改选其他图形后端不会自动改写此专用启动配置，其他后端需另行配置和验证。

## 已知限制

- 验证范围是本机进入大厅、战斗及重启后超过 10 分钟无 ACE 弹窗，不是长期稳定性承诺。
- 已验证能玩的运行时仍会记录缺失 `PsGetThreadProcess` 的驱动进程异常。短时可玩不等于每个 ACE 组件或检查都成功。
- 对应候选补丁已通过独立接口测试，但尚未做游戏验证，默认不启用。开发者可用 `build_wine_modules.py --with-thread-process` 构建该候选。
- 部分 Wine 内核行为仍不完整；背景恢复依赖应用支持软件解码回退，尚未验证所有过场。
- 每次处理无法原生执行的 NOP 都有信号处理成本，未测定量性能影响。
- CrossOver 或游戏更新后可能需要重新适配。

技术实验、失败路径和测试细节见 [验证记录](docs/VALIDATION.md)。本项目通过运行时/API 兼容处理工作，不修改游戏或 ACE 二进制，也不伪造反作弊成功结果。

## 开发与贡献

原生回归测试：

```sh
make test
```

Windows/Wine 接口测试需要独立、可丢弃的测试容器：

```sh
python3 scripts/test_windows.py --prefix /absolute/path/to/test-bottle
python3 scripts/test_wine_modules.py \
    --prefix /absolute/path/to/test-bottle \
    --runtime local/runtime-modules
```

候选线程接口测试需额外传入 `--with-thread-process`。发布纯源码包：

```sh
python3 scripts/package_source.py
```

反馈问题时请附上 Mac 型号、macOS/CrossOver 版本、使用的图形后端和出现问题的具体步骤。请先从日志里去掉登录信息、令牌和账号标识，再附上必要片段。

## 许可证与致谢

采用 **LGPL-2.1-or-later**，详见 [LICENSE](LICENSE) 与 [第三方来源说明](THIRD_PARTY.md)。

感谢 Wine、CodeWeavers、Endfield_FineWine 及此前启动器修复项目提供的公开工作。NIKKE、CrossOver 和 Rosetta 均为各自权利人的产品；本项目是独立的社区兼容研究。
