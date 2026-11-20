## 铭辰任务到位容差

以任务目标水平 180.00°、俯仰 20.00° 为例，9 秒只读抓包中水平回报为 179.92°～179.93°，俯仰为 20.00°。水平偏差超过原 0.05°，因此将型号水平容差调整为 0.12°，覆盖历史实测的 0.11° 并留一个编码步距。俯仰容差 0.05° 和原停稳等待保持。

## 铭辰实测俯仰行程与世界角回显（用户定）

本机俯仰定位和扇扫使用 0° 至 180°，定位草稿默认 0°。按 `(角度−90°)×100` 编码，端点分别为 −9000 和 +9000，回报按 `90°+码/100` 解码。实时读数和趋势图采用横滚绝对值不大于 90° 的世界角分支，零安装标定时保留越过天顶后的俯仰和方位。任务与落盘仍用标准方向分解。

## 2026-11-20：放宽铭辰俯仰测试范围（用户定）

本机俯仰定位与扇扫输入接受 −90° 至 270°，下发按原公式 `(90°−角度)×100` 编码。−90°、270° 在线上分别为 +18000、−18000。定位草稿仍从 −2° 起步。默认俯仰速度为 2°/s，定位看门狗为 200 秒。机械行程与限位由现场测试确认。

## 2026-11-17：速度草稿不按占用状态拦截（用户定）

水平、俯仰速度编辑只更新 QML 草稿并记录设置日志，删除占用检查；不立即发令，不影响已提交的运动和任务快照。之后手动定位、扇扫及循环读取当时的草稿，分别经 Device::start 或 Device::send 统一验锁，型号速度校验、量化及命令编码保持。被任务占用时可以编辑下次用的值，实际手动命令仍被拒绝。

仅修改 McztControl 的两处判断及注释，不新增状态或转发。验证见主程序 docs/maintenance/validation.md；使用相邻本地源码覆盖，暂不推送。

## 2026-11-17：命令集合使用 vector（用户定）

startOf/stopOf 与 Collector 标签统一返回 std::vector<std::vector<unsigned char>>，官邸直接交 Device::send，删除原批次转换。单条 buildFrame 编码保持，命令集合工厂负责转为自有字节；两轴位置/速度、停机命令、驻留及重试保持。

不新增命令缓存或兼容包装。验证见主程序 docs/maintenance/validation.md；使用相邻源码覆盖，暂不推送。

## 2026-11-14：UTF-8 日志接口（用户定）

日志直接使用 Handle 的 UTF-8 std::string；报警位列表、未命名位十六进制和变更文字均用标准字符串及 std::format 生成。报警变化判据、控制、QML 接线与设备生命周期不变。

需要支持 C++20 std::format 的标准库。本轮仅本地修改与验证，不推送。

验证：主程序全部官邸增量构建及设备/任务生命周期 8/8、8 秒离屏冒烟通过；无实物发令。本轮未单独执行铭辰硬件测试。

## 2026-11-14：驻留说明纠正（用户定）

纠正 McztConfig 的旧注释：出圈清空驻留区间，重新进圈后按新的首末圈内样本计时；不是“出圈不清起点”。仅修改说明，型号参数、坐标换算、重试和取消链不变。集成验证见主程序维护操作，本轮不推送。

## 2026-11-14：接入 FamilyCore（用户定）

依赖声明与开关改用 FamilyCore / FAMILYCORE_WITH_*，协程头由 DeviceCore 提供。当前仅本地整理，GitHub 仍为 TaskCore，旧固定提交暂不更新；联调使用 `FETCHCONTENT_SOURCE_DIR_LIDAR_FAMILYCORE` 指向相邻 FamilyCore，并覆盖配套 DeviceCore/Algorithm 等源码。Windows 主程序或 simonly 的实际消费目标构建通过，业务与数据格式不变；未推送、未更新交付包。

# McztDevice

2026-11-09 用户定：官邸提供 `paramsFor(target, hold, speeds)`（型号校验与量化，任务预验、造斗与直发共用），makeMoveCollector 与手动 issueGoTo 都收 `GMBMoveParameters` 并复用该判据；删除 collectorFor 与只读原型，Collector 由 begin 直接造出完整对象。

2026-11-09 用户定：设备与云台族合为一个对象：末端官邸直接继承 `GMBDevice<HandleT, CollectorT>`（它同时继承 `Device<HandleT, CollectorT>` 与纯 C++ 的 `GMBDeviceBase`），删除中间 Core 类与 `device_` 指针；唯一 QObject 来自 Handle。末端声明 displayName/subtitle/isOpen/holder/mounting/slipRing 属性及 lockChanged/mountingChanged/anglesReady 信号，析构首句预析构只走 Device 链。页面 `family` 改为 `QtObject`，`deviceState`/日志直接指向设备本身，删除 `device.device` 自引用。页面测试新增 family 即后端本身、可交叉转换到族基的断言。

2026-11-03 用户定：网口连接区仅保留网卡选择和连接/断开，删除 IP、端口等固定参数展示及对应 QML 参数字段；实际连接配置、校验、ARP 和收发关闭行为保持。

Windows 验证：通过主程序本地源码覆盖增量构建，五个网口官邸 qmllint 目标成功；仍有 QObject 属性类型的既有静态提示。八秒离屏冒烟通过，仅既有字体提示；未进行硬件操作或 Linux 验证。证据：LiDAR_DataACQ/build/build-network-ui.log、smoke-network-ui.json。

2026-11-03 上传接口：官邸仍拥有唯一具体 Core；Core 继承 `Device<HandleT, CollectorT>`，Device 直接继承 Handle 并重写 onData。Core 只提供 Qt 属性、锁信号与末端预析构；不增设上传回调、状态缓存或逐包信号转发。协议、Collector 与 UI 逻辑保持。

2026-10-28 连接显示整理（用户定）：型号固定连接参数：设备 192.168.6.6:2000，本机 192.168.6.10:1000；不指定 MAC，继续 ARP 学习。QML 仅保存网卡下标，open(interfaceIndex) 使用 McztConfig 构参；连接区只提供网卡选择和连接/断开。

LiDAR 的独立官邸：具体 C++ 后端、Collector/协议/处理逻辑及其 QML 页面在同一仓库维护。来自 LiDAR_DataACQ master `5cc6d65`，本次分仓不改变运行行为。

## 边界与接入

- QML 模块：`Lidar.Mczt`；页面：`McztModule`。
- CMake 目标：`Lidar::McztDevice`；静态插件：`LidarMcztDeviceplugin`，最终可执行文件显式 `Q_IMPORT_QML_PLUGIN(LidarMcztDevicePlugin)`，并导入 TaskUI/UI/Charts 的静态插件。
- 页面以 attached 对象拥有唯一具体后端，后端不能在 QML 直接创建。设备通过只读 `family` 借出族引用；任务页面只接收族列表，不依赖设备官邸。
- 依赖由 `cmake/Dependencies.cmake` 固定到提交；可用标准 FetchContent 源码覆盖做联调。多个官邸并用时，先开启所有需要的 HandleCore/FamilyCore 可选项，再配置共享库，避免已创建目标后补开模块。
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

## 2026-11-04：设备身份与占用查询（用户定）

设备名称/副标题由构造传入 DeviceBase 的公开 const 成员，Qt 通过 MEMBER CONSTANT 直接暴露；占用查询只保留 holder，Qt 用无效 QVariant 表示空闲、字符串表示占用，删除 isLocked 属性。既有连接、重连、收发和取消逻辑保持。

本轮仅本地整理；集成验证记录见主程序 docs/maintenance/validation.md，不推送。

2026-11-05 表单统一（用户定）：扇扫及循环操作接入 DoubleButtonRow，复合输入列采用 FieldStrip 权重；保留既定组间距与协议不支持的俯仰循环占位。 公共视觉规格由 UICore 提供，不增加业务状态。Windows 主程序增量构建、八秒离屏冒烟通过；固定依赖在本地更新，暂不推送。


### 2026-11-05 公共视觉与连接布局

连接区使用 UICore ConnectionSection，小标题/说明使用 Subheading/HintText；速度、定位和连接动作仍由官邸管理。 本轮仅本地整理，未推送；Windows 集成验证记录见主程序维护操作。


### 2026-11-05 按钮气泡与布局复用

扇扫和循环两排 DoubleButtonRow 直接交给 Section 排列，删除仅重复设置公共行距的 ColumnLayout。动作、禁用的俯仰循环格、停止按钮前间距不变。

主程序Windows Release增量构建、八秒桌面冒烟与四窗口装配检查通过，运行stderr为空。未连接硬件，仅本地提交、未推送。
