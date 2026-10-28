#pragma once
#include <chrono>

#include "Device/GMB/Mczt/McztVocab.h" // 速度上限（默认值直取它，不另抄数）

// ============================================================
// McztConfig — MCZT46 云台常量店（编译期常量；型号夹宪法，对齐 SqafConfig / HfnlConfig）：
// 夹内皆可读、夹外不读（界面要的量由官邸以创建参数转交）。店持**数值事实**，协议事实（帧布局 / 命令
// 位 / 量程 / 分辨率）全在词典 McztVocab，两边不重复。
//
// 与 SqafConfig 的一处差别：**本店不列轴作动范围**。sqaf 要列，是因为 Pelco-D 不编码行程
// 上下限；MCZT 的协议自己就写死了，那是协议事实，住 McztVocab::PanAngleMin/Max、TiltAngleMin/Max。
// 抄一份到这里就是给同一个数开第二本账。
// ============================================================
namespace McztConfig
{
// ── 判到位阈值 ──────────────────────────────────────────────────────────────
// 造斗时交给族级收集器：多近算"进了容差圈"。协议分辨率是 0.01°，取 0.05° 留四个最小刻度的余量。
static constexpr double angleTol = 0.05; // TODO[接机]: 待实测调整

// 驻留时长由任务传入 GMBCollector，型号不另设常量；按首末圈内样本的间隔判齐，出圈不清起点。

// ── 单次尝试时限 ────────────────────────────────────────────────────────────
// **不能照抄 sqaf 的 5000**：本云台俯仰轴上限只有 2°/s（协议 McztVocab::TiltSpeedMax），
// 走完 -2°~+92° 整段行程要 ≈47 s；水平 40°/s 走 360° 约 9 s。取 60 s 给俯仰留够余量——
// 时限短于真实行程，看门狗就会在半路停机重发，云台永远走不到。
static constexpr int expectedDurationMs = 60000; // TODO[接机]: 按实测行程时间收紧

// **扇扫 / 循环没有时限**：它们没有停止条件（协议也报不出"异常退出"），只直发、不造斗，压根
// 不进 run，故本店不为它们备第二个时限数（用户定：不做过度设计）。

// ── 界面旋钮默认值 / 步距（官邸以创建参数转交）────────────────────────────────────────
// 范围直读 McztVocab 的量程常量，此处只补"默认停在哪"与"一格多大"。
// QML 设置默认取协议上限（用户定）；任务在启动时按值固定当前设置。
static constexpr double panSpeedDefault = McztVocab::PanSpeedMax;   // °/s
static constexpr double tiltSpeedDefault = McztVocab::TiltSpeedMax; // °/s
static constexpr double speedStep = 0.1; // 协议分辨率即最小步距

// ── 型号固定网口参数；界面只选择本机网卡 ─────────────────────────
// 出处：协议文档"伺服 192.168.6.6:2000 ← 控制主机 192.168.6.10:1000"（见 McztVocab 头注）。
// 伺服 MAC 协议未给：官邸传空，连上后由 ARP 学到。
static constexpr auto deviceIPv4 = "192.168.6.6";
static constexpr int devicePort = 2000;
static constexpr auto hostIPv4 = "192.168.6.10";
static constexpr int hostPort = 1000;

// 上行回显间隔：**协议事实**。本台 100 Hz 自动上报——实测坐实（路由器 tcpdump 与程序探针逐位
// 吻合、零丢包），**协议文档写的 200 Hz 是错的**。角度图的环长由它与下面的窗宽定。
static constexpr auto uplinkInterval = std::chrono::milliseconds{10};
// 角度图看多长：显示决定（用户定）。环长 = 窗 ÷ 回显间隔，在图里算，这儿不抄。
static constexpr auto angleChartWindow = std::chrono::minutes{2};
static constexpr auto angleHistoryWindow = std::chrono::hours{1}; // 覆盖虚拟卡最长累积窗，显示窗不变。

// 读取政策（唤醒间隔 / 内核缓冲）**不在型号宪法里**——那是族级常量，住 GMBDeviceBase::kPcap*
// （理由见那儿：由流量形状决定，与型号无关）。
} // namespace McztConfig
