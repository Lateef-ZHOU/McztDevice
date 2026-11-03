# McztDevice

2026-11-03 用户定：网口连接区仅保留网卡选择和连接/断开，删除 IP、端口等固定参数展示及对应 QML 参数字段；实际连接配置、校验、ARP 和收发关闭行为保持。

Windows 验证：通过主程序本地源码覆盖增量构建，五个网口官邸 qmllint 目标成功；仍有 QObject 属性类型的既有静态提示。八秒离屏冒烟通过，仅既有字体提示；未进行硬件操作或 Linux 验证。证据：LiDAR_DataACQ/build/build-network-ui.log、smoke-network-ui.json。

2026-11-03 上传接口：官邸仍拥有唯一具体 Core；Core 继承 `Device<HandleT, CollectorT>`，Device 直接继承 Handle 并重写 onData。Core 只提供 Qt 属性、锁信号与末端预析构；不增设上传回调、状态缓存或逐包信号转发。协议、Collector 与 UI 逻辑保持。

2026-10-28 连接显示整理（用户定）：型号固定连接参数：设备 192.168.6.6:2000，本机 192.168.6.10:1000；不指定 MAC，继续 ARP 学习。QML 仅保存网卡下标，open(interfaceIndex) 使用 McztConfig 构参；连接区只提供网卡选择和连接/断开。

LiDAR 的独立官邸：具体 C++ 后端、Collector/协议/处理逻辑及其 QML 页面在同一仓库维护。来自 LiDAR_DataACQ master `5cc6d65`，本次分仓不改变运行行为。

## 边界与接入

- QML 模块：`Lidar.Mczt`；页面：`McztModule`。
- CMake 目标：`Lidar::McztDevice`；静态插件：`LidarMcztDeviceplugin`，最终可执行文件显式 `Q_IMPORT_QML_PLUGIN(LidarMcztDevicePlugin)`，并导入 TaskUI/UI/Charts 的静态插件。
- 页面以 attached 对象拥有唯一具体后端，后端不能在 QML 直接创建。设备通过只读 `family` 借出族引用；任务页面只接收族列表，不依赖设备官邸。
- 依赖由 `cmake/Dependencies.cmake` 固定到提交；可用标准 FetchContent 源码覆盖做联调。多个官邸并用时，先开启所有需要的 HandleCore/TaskCore 可选项，再配置共享库，避免已创建目标后补开模块。
- 需要 libpcap >= 1.10.0；Windows 使用 Npcap SDK 与已安装的 Npcap 运行时，Linux 使用 libpcap。驱动不随仓库分发。

## 构建与验证

C++20、CMake >= 3.28、Qt 6.11.2（Quick/Qml/GuiPrivate/ShaderTools）。Windows 使用 MSVC x64；Linux 使用 GCC，Qt 与原生依赖由环境提供。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<Qt前缀;需要的原生依赖前缀>"
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target LidarMcztDevice_qmllint
```

独立测试逐头编译、创建真实页面并销毁后端；不连接实物设备。硬件收发仍需对应设备实测。主程序负责跨官邸仿真、任务及取消链回归。

## 许可

MIT，Copyright (c) 2026 周涛（ZHOU TAO）。Qt、libserialport、libpcap/Npcap 等依赖各自授权，MIT 不替代这些许可；详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。本仓库不包含第三方 SDK、运行库或驱动。

CI：托管 Linux 自动验证；Windows 原生依赖使用预装 SDK/运行时的自托管入口，默认跳过不代表通过。本机 Windows 验证记录与主程序分仓记录一起保存。

本轮本机验证（2026-10-28）：Windows Qt 6.11.2 Release 构建、CMake qmllint 和既有页面冒烟 1/1 通过，无编译或 QML 警告。日志：build/build-connection-summary.log、build/test-connection-summary.log。未打开实物连接，未重复 Linux 验证。

## 资源阶段通知（2026-11-02）

连接采用 HandleCore 的 `connectionChanged(ResourcePhase)`；主动关闭前在所属线程同步发 `AboutToRelease`，完成事件用于回读现状。页面借出已有 `deviceState` 机制对象，不再保存 isOpen/isLocked/holder 的 QML 转交属性；标签通过连接/锁完成阶段回读，不恢复通用 statusChanged。固定依赖已同步；本轮提交尚未推送，验证使用显式本地源码覆盖。


2026-11-03 验证：Windows Qt 6.11.2 主程序 Release 编译、15 组 QML 检查、跨官邸生命周期和三任务回归 7/7 通过。接口联调使用相邻源码覆盖，新依赖固定为本地提交，尚未推送；未连接实物设备或更新板卡。

## 源码目录（2026-11-03）

公开文件直接放在 `include/`，引用时使用文件名，不再携带原单仓库的业务目录前缀。CMake 目标名称、C++ 类型和 QML 模块名保持不变；使用方须同步更新头文件路径及固定依赖版本。

目录整理验证（2026-11-03）：Windows：公开头独立编译、页面测试 1/1 及主程序构建通过。本轮通过本地源码覆盖验证，固定依赖同步更新到本地提交；尚未推送，不代表已验证远端拉取。

## 2026-11-04：设备日志直接接线（用户定）

删除具体设备的 logSource 自引用属性，QML 直接监听现有设备对象的 logMessage/connectionChanged；设备失败日志直接调用继承自 Handle 的信号。协议、收发、重连和关闭顺序不变。 本轮仅本地整理，未推送；Windows 集成验证记录见主程序维护操作。
