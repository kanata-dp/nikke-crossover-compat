# nikke-crossover-compat

**实验性兼容层：NIKKE 已在本机进入大厅和战斗，用户报告帧率体感正常、背景动画正常。通过 CrossOver 固定入口重启并开启高清后，用户确认超过 10 分钟无 ACE 弹窗。长时间稳定性仍待更多测试。**

This is a small, experimental register-NOP compatibility bridge for macOS
x86_64 processes running under Rosetta and Wine. It is not a complete emulator.
On the tested machine, the user entered the lobby and combat and reported
normal perceived frame rate and working background animation. After a
restart through the persistent CrossOver entry with Retina mode enabled, the
user confirmed more than ten minutes without an ACE popup. Long-session
stability remains unverified, and
Wine still logs a missing API in a driver process; see the limitations below.

## 做了什么

在 macOS 的 SIGILL 处理链中识别两个经过复现的指令形式，按照 NOP 语义推进 RIP，保留其余上下文：

| 编码 | 支持范围 | 长度 |
| --- | --- | --- |
| `0f 1f c0` … `0f 1f c7` | 无前缀的寄存器 NOP | 3 字节 |
| `41 0f 1f c0` … `41 0f 1f c7` | 带 REX.B 前缀的寄存器 NOP | 4 字节 |

不匹配的指令继续进入原有异常处理器。实现不改写游戏指令、磁盘上的游戏二进制或系统保护设置。

项目包含动态库、自编 Wine 引导程序、子进程运行时视图生成器、Wine 内核与媒体接口实验补丁，以及独立的原生、Windows 和视频诊断程序。
运行时仍依赖用户自己安装的 CrossOver 和 Rosetta；没有集成 ARM64/FEX。

## 本机验证

2026-09-08，Apple M4 Max，macOS 26.6.2（25G83），CrossOver 26.1。

| 测试 | 不加载兼容层 | 加载兼容层 |
| --- | --- | --- |
| 原生四次 NOP 执行 | 四次进入诊断异常处理器 | 零次；兼容层计数为四 |
| Windows 四次 NOP 执行 | 四次进入 VEH | 零次 |
| DLL 在主程序之前初始化 | 两次进入 VEH | 零次；进入 main |
| Windows 子进程 | 初版传递路径曾失败 | 运行时视图修复后通过 |
| 多线程 | — | 八线程合计 12,800 次处理通过 |
| UD2、LOCK NOP、页边界、单次处理器 | — | 保持预期异常/处理器语义 |

独立解码测试枚举了 33,554,432 个候选序列，只接受上述 16 个编码。
基本上下文测试实际检查了 RDX、RFLAGS、errno；并未逐个验证所有 CPU 扩展状态。

早期测试曾反复遇到 ACE 弹窗；后续组合已进入实际大厅和战斗。
用户初次反馈约 3 分钟无弹窗、帧率体感正常；固定入口重启后的第二轮又确认超过 10 分钟无弹窗。未做定量 FPS 测试。
详细证据与限制见 [验证记录](docs/VALIDATION.md)。

补丁逐步补充了 guarded mutex、进程短名称缓存、崩溃回调注册生命周期、物理内存范围和逻辑地址转换；独立接口测试通过。线程状态快照只明确返回未支持，并没有生成快照。已验证能玩的运行时仍记录了缺失 `PsGetThreadProcess` 的驱动进程异常，所以短时可玩不代表所有 ACE 组件或检查都已成功。对应候选修复已通过独立接口测试，作为默认关闭的额外补丁保留，尚未用游戏验证。

高清测试中，游戏保存的窗口分辨率从 1388×781 变为 2202×1340；CrossOver 设置从普通模式、96 DPI 改为 Retina、192 DPI，用户确认明显更清楚。这里记录的是该窗口的配置值，不代表每个渲染阶段的内部像素尺寸。

## 构建与测试

需要 Xcode Command Line Tools、Python 3、可运行 x86_64 程序的 macOS。
Windows 测试还需要 MinGW-w64 的 `x86_64-w64-mingw32-gcc`；图形探针需要对应的 `g++`。

```sh
make
make test
make windows
python3 scripts/prepare_runtime.py
python3 scripts/test_windows.py --prefix /absolute/path/to/a/dedicated/test-bottle
```

`prepare_runtime.py` 在 `local/runtime` 创建仅供本机使用的目录：大部分内容是指向现有 CrossOver 的符号链接，`ntdll.so` 是未改动的本地副本，邻接的 `wine` 指向自编引导程序。
这是为了让 Wine 创建子进程时继续走兼容层。直接设置 `WINELOADER` 在本次运行时里不足以保证这一点。
已有输出目录不会被覆盖；如需重建，使用新的 `--output` 路径。

### 重建实验 Wine 模块

需要 Bison 3、Xcode 编译工具及 MinGW-w64。下载 CodeWeavers 官方的
[CrossOver 26.1 源码包](https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.1.0.tar.gz)，
脚本会校验固定 SHA-256，然后只构建 `ntoskrnl.exe`、`mfplat.dll` 和 `mfreadwrite.dll`：

```sh
python3 scripts/build_wine_modules.py --archive /absolute/path/crossover-sources-26.1.0.tar.gz
python3 scripts/prepare_runtime.py --output local/runtime-modules \
    --modules local/wine-modules/build
python3 scripts/test_wine_modules.py --prefix /absolute/path/to/test-bottle \
    --runtime local/runtime-modules
```

构建不会安装到 CrossOver 或原始容器。使用该运行时启动时，额外传入
`--runtime local/runtime-modules`。补丁以 CrossOver 26.1 的公开 Wine 源码为基线，
不能直接用于其他版本。

默认构建对应已做游戏测试的补丁组合。`--with-thread-process` 额外应用
`PsGetThreadProcess` 候选；测试该候选时也向 `test_wine_modules.py` 传入同名开关。
它尚未加入已验证的游戏配置。

### 运行自编 Windows 探针

```sh
python3 scripts/run_wine.py --prefix /absolute/path/to/test-bottle \
    build/windows_probe.exe --expect-bridge
python3 scripts/run_wine.py --prefix /absolute/path/to/test-bottle \
    build/windows_probe.exe --spawn-child
```

`run_wine.py` 直接进入 Wine，主要用于诊断程序。`--without-bridge` 做同路径对照。

### 保留 CrossOver 容器配置的启动方式

```sh
python3 scripts/launch_crossover.py --prefix /absolute/path/to/test-bottle \
    --workdir 'C:\NIKKE\Launcher' --dll version=n,b \
    'C:\NIKKE\Launcher\nikke_launcher.exe'
```

该命令要求测试容器里已安装官方启动器及其所需的启动器修复；本项目不会下载游戏、安装启动器补丁或复制登录状态。`--dll version=n,b` 仅适用于已经使用相应启动器补丁的容器。
脚本使用 CrossOver 的包装程序，保留容器原有图形设置。不要把原始容器当成临时实验目录。
构建产物中的 `.app` 是引导程序的封装。以下安装脚本为它写入本机路径和 NIKKE 启动配置。

### 固定在 CrossOver 里启动

先完全退出测试容器中的游戏、启动器及后台进程，再运行：

```sh
python3 scripts/install_crossover_entry.py \
    --source-prefix /absolute/path/to/test-bottle \
    --source-runtime /absolute/path/to/runtime-modules \
    --source-app build/NopBridgeLab.app \
    --source-bridge build/libnop_bridge.dylib
```

脚本用 APFS 文件克隆复制现有容器和已下载资源，在 CrossOver 的标准容器目录创建
`NIKKE-Compatibility`，把本机运行时保存在 `~/Library/Application Support/NIKKE Compatibility`。
它通过 CrossOver 自带的 `cxmenu --type raw` 注册入口，启动官方 NIKKE 启动器。
不会覆盖现有同名目录。原始容器保留，复制的登录状态仅留在本机。

重新打开 CrossOver，选择 **NIKKE-Compatibility → NIKKE Compatibility**。
在右侧开启 **高分辨率模式**，按提示重启这个新容器；之后双击该入口，再点官方启动器的开始游戏。
本机测试时入口命名为“NIKKE 兼容版”。游戏仍需正确的官方启动器及之前的启动器修复；
本脚本不会安装这些依赖，也不需要重新下载已有资源。

CrossOver 图形后端在该启动配置中固定为已测试的 DXVK。更换后端、更新 CrossOver 或删除依赖运行时后需重新验证；高清开关则直接读取该容器的注册表设置。

### 背景视频诊断

```sh
make build/shared_texture.exe
python3 scripts/launch_crossover.py --prefix /absolute/path/to/test-bottle \
    --graphics dxvk build/shared_texture.exe
```

它只创建自有的 RGBA8/NV12 纹理，测试 `GetSharedHandle` 和 `OpenSharedResource`，不读取游戏资源。
本机 DXVK 返回 `0x80070057`，与游戏背景视频日志一致。

媒体补丁提供两个独立、默认关闭的实验开关：

- `--software-video`：Source Reader 不接收 D3D manager，改用 CPU 帧。独立解码通过，但 Unity 仍要求 `IMFDXGIBuffer`，实测没有解决游戏黑屏。
- `--disable-dxgi-video`：在创建 DXGI manager 时返回不支持，让调用者尽早选择软件路径。独立探针能回退并解码；当前组合下用户确认资源下载界面的背景正常。尚未做仅切换该开关的游戏 A/B 对照。

`tests/video_reader.cpp` 用自制动态测试片段检查帧数、内容变化和校验和。
两个开关都没有实现跨设备共享纹理；不要把独立探针的回退能力当作所有应用都支持回退。

## 当前边界

- 目前验证的是 x86_64 指令路径；没有完整验证 32 位指令语义、所有游戏、其他 Wine 版本和系统版本。
- 每次无法原生执行的 NOP 都会触发信号，存在处理成本；用户报告战斗帧率体感正常，未测定量 FPS、长时间性能或不同场景表现。
- SIGILL 注册使用不可回收的固定槽位，每进程最多 32 次注册尝试（包含初始化；系统拒绝的注册也消耗槽位）。耗尽返回 `ENOMEM`，保留现有处理器。
- 动态库必须随进程存活，不支持运行中卸载；直接使用底层系统调用替换处理器不在覆盖范围内。
- 读取指令使用 Darwin Mach API。已测页边界失败路径，但这不是跨平台的 POSIX 信号安全保证。
- `--privileged-faults` 只把有效的 MOV CR0/2/3/4/8 指令在用户态产生的错误分类修正为特权指令异常；保留 RIP 并交给 Wine/程序处理，不执行控制寄存器访问。
- guarded mutex 补丁沿用 Wine 上游仍不完整的 guarded-region/APC 语义；只验证了互斥行为。
- 崩溃回调实现登记、重复拒绝和移除，不提供 Windows 内核崩溃转储和回调执行管线。
- 物理内存范围来自 Wine 实际报告的内存信息，返回可释放的终止数组；它不开放 Mac 的原始物理内存。
- `MmGetVirtualForPhysical` 对应 Wine 现有的逻辑地址模型，只接受已提交且可访问的映射；它不是 Windows 页表或 Mac 物理地址转换。
- `KeCapturePersistentThreadState` 沿用 Wine 衍生项目的未支持返回路径，保持输出缓冲区不变；完整内核快照没有实现，该未公开接口的全部 Windows 行为没有验证。
- ACE 错误码本身的完整含义尚未查到可靠映射。当前实现修复具体 Wine API 和异常语义，不禁用或伪造反作弊检查。
- 本机三个图形后端的共享纹理对照都未通过；恢复背景依赖软件回退，尚未验证全部过场和长时间播放。

## 开源与交付

源码采用 **LGPL-2.1-or-later**，参见 [LICENSE](LICENSE) 和 [第三方说明](THIRD_PARTY.md)。
源码包只包含显式列出的源文件、文档和测试：

```sh
python3 scripts/package_source.py
```

不要把 `local/`、容器、日志、游戏、第三方动态库或登录数据上传到仓库。
当前版本已有本机短时游戏验证，适合以实验性源码发布；不宣称所有 ACE 检查通过或支持所有使用 ACE 的游戏。
源码包不包含可直接分发的完整游戏环境。发布前应检查 [验证记录](docs/VALIDATION.md) 中的已知边界。

## 技术依据

- [Intel 指令手册，NOP](https://cdrdv2-public.intel.com/868141/253667-089-sdm-vol-2b.pdf)：多字节 NOP 不改变寄存器或标志位，只推进指令位置。
- [Wine loader/main.c](https://github.com/wine-mirror/wine/blob/36b6a2cf679fb395f668a917b76537190e212d9c/loader/main.c)：引导接口与 macOS 地址空间预留。
- [Microsoft GetSharedHandle](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiresource-getsharedhandle)：图形资源共享接口。
- [DXVK 共享资源实现](https://github.com/doitsujin/dxvk/blob/master/src/d3d11/d3d11_resource.cpp)：用于分析共享句柄错误；不代表本机二进制与主分支版本完全相同。
