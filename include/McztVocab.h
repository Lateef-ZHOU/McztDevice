#pragma once
#include <algorithm> // std::clamp（量程钳位——发给电机的最后一道闸）
#include <array>     // decodeUplink：四个量
#include <cmath>     // std::lround（角/速度 → 设备码）
#include <optional>  // decodeUplink：空 = 帧不对
#include <utility>   // StatusNames：位 → 名称
#include <QByteArray>
#include <QtEndian> // qToBigEndian / qFromBigEndian（本协议全字段大端）
#include <QtGlobal>

// ============================================================
// McztVocab.h — MCZT46 电机云台的**协议实现层**（收发词汇典）。
// 依据：《MCZT46通讯协议》v1.0（2025-12-15，李鹏）。以太网 UDP：
//   伺服 192.168.6.6:2000 ← 控制主机 192.168.6.10:1000。
//
// **本文件只管"字节长什么样"**：帧常量 / 命令位 / 量程 / 角速度↔设备码 / 累加和 / 上行解帧。
// 成帧（8 个槽位怎么填）在邻居 McztRunSide.h——那一步要认识两份 AxisMeta，是 run 侧的事。
//
// **方位与俯仰共用一个命令枚举 AxisCmd**：协议表里两轴的位号完全对齐，唯一差别是俯仰的 00 位
// 留空（没有"连续转动"）。为这一个差别在**字节层**立两个类型不值当——两轴的槽位本就该同形，
// 正对着线上那 8 个槽位。"俯仰不能 Rotate"由 run 侧的类型承担：`McztTiltMeta` 没有 `rotate()`
// 这一口，写错在编译期就挡下了，字节层可以安心只有一个枚举。
// ============================================================
namespace McztVocab
{
// ── 帧常量 ──────────────────────────────────────────────────────────────────
// 每个字段"先发高字节，再发低字节"——全协议大端，含帧头帧尾自身。
inline constexpr quint8 HeadHi = 0xEB, HeadLo = 0x90;
inline constexpr quint8 TailHi = 0x14, TailLo = 0x6F;

// 下行（主机 → 伺服）30 字节：
//   [0..1] 帧头 | [2] 方位命令 | [3] 俯仰命令 | [4..5] 方位速度 | [6..9] 方位定位角/扇扫起始
//   [10..13] 方位扇扫终止 | [14..15] 俯仰速度 | [16..19] 俯仰定位角/扇扫起始
//   [20..23] 俯仰扇扫终止 | [24..26] 预留 | [27] 累加和 | [28..29] 帧尾
inline constexpr int DownSize = 30;
inline constexpr int DownPanCmdOff = 2;
inline constexpr int DownTiltCmdOff = 3;
inline constexpr int DownPanSpeedOff = 4;
inline constexpr int DownPanAngle1Off = 6;
inline constexpr int DownPanAngle2Off = 10;
inline constexpr int DownTiltSpeedOff = 14;
inline constexpr int DownTiltAngle1Off = 16;
inline constexpr int DownTiltAngle2Off = 20;
inline constexpr int DownChecksumOff = 27;
inline constexpr int DownTailOff = 28;

// 上行（伺服 → 主机）20 字节。协议写 200 Hz 自动上报，**实测是 100 Hz**（见 McztConfig::uplinkInterval）：
//   [0..1] 帧头 | [2..3] BIT 状态 | [4..5] 方位速度 | [6..9] 方位角度
//   [10..11] 俯仰速度 | [12..15] 俯仰角度 | [16] 预留 | [17] 累加和 | [18..19] 帧尾
// （解析见本文件的 decodeUplink；协议数字唯一住协议层。）
inline constexpr int UpSize = 20;
inline constexpr int UpStatusOff = 2;
inline constexpr int UpPanSpeedOff = 4;
inline constexpr int UpPanAngleOff = 6;
inline constexpr int UpTiltSpeedOff = 10;
inline constexpr int UpTiltAngleOff = 12;
inline constexpr int UpChecksumOff = 17;
// **上行累加和的实测区间是 [2, 15)，不是协议写的 [2, 17)**（实机两帧反推，用户抓包）：
//   EB90 0000 0000 000062B9 0000 00000009 00 [1B] 146F —— 62+B9 = 0x11B → 1B（协议算法得 24）
//   EB90 0000 0000 00002EE0 0000 00001770 00 [25] 146F —— 2E+E0+17 = 0x125 → 25（协议算法得 95；再去掉 17 得 0E，不符）
// 即伺服**没把字节 15（俯仰角最低字节）算进去**；字节 16 是预留位，两帧都为 0，算不算进去无从分辨——
// 取 [2,15)，恰是"校验字节前两个字节不算"。**残余风险**：若伺服哪天在预留位填了非零值、而它其实
// 参与求和，这里会误拒（解不开的帧不记日志，表现是角度回显断流）。协议文档（v1.0）的"不含帧头
// 帧尾自身"一句与实机不符，待厂家确认。
inline constexpr int UpChecksumEnd = 15;
inline constexpr int UpTailOff = 18;

// 上行 BIT 状态位（大端 quint16，位号即协议表的"NN 位"；表里留空的位不列）。
enum Status : quint16
{
    PanFault = 1u << 1,       // 方位故障
    TiltFault = 1u << 2,      // 俯仰故障
    TiltLimit0 = 1u << 5,     // 俯仰 0° 限位
    TiltLimit90 = 1u << 6,    // 俯仰 90° 限位
    PanDriverFault = 1u << 7, // 方位驱动器故障
    TiltDriverFault = 1u << 8,// 俯仰驱动故障
    PanAlarm = 1u << 10,      // 方位告警
    TiltAlarm = 1u << 11,     // 俯仰告警
};

// ── 命令位（方位 / 俯仰各一字节：**要么恰一位置 1，要么全零**）───────────────────
// 协议表按位列举，两轴位号对齐，故合成一个枚举（差异只在俯仰无 00 位，由 McztTiltMeta 的类型把守）。
//
// **"维持现状"是整字节全零，不是某一位**（用户核过真表）：位 4 在表里是空格——那是**保留位、
// 不许写**，不是"这一位就是空操作"；协议的空操作是命令字节**一位都不置**，本轴自然不受本帧干预。
// ⚠ 这个误读很容易犯：协议表里的空格既可能是"保留"也可能是"未使用"，而两者对写入方的要求恰好
// 相反——前者禁写，后者随意。拿不准时按禁写办，代价只是少一个可用位。
// 于是 Hold 恒为 0，且**保留位全栈无一处被写**（成帧只写 static_cast<quint8>(cmd)）。
enum class AxisCmd : quint8
{
    Hold = 0,             // 维持现状 = 整字节全零：本帧不干预本轴（本协议的空操作）
    Rotate = 1u << 0,     // 连续转动（循环）——**协议表里只有方位有**，故只有 McztPanMeta 有这一口
    Position = 1u << 1,   // 定位
    ClearAlarm = 1u << 2, // 告警复位
    SetZero = 1u << 3,    // 标零
    // 位 4 —— **保留，不许写**（见上）
    Scan = 1u << 5,       // 扇扫
    Stop = 1u << 6,       // 停止
};

// ── 各命令用得到哪几个槽位（协议表；四个不带数据的命令三槽全不用）────────────────
// 成帧时据此把用不上的槽位写零：**同一个模式恒出同一串字节**，meta 里那些与本模式无关的
// 残留值不会漏到线上。
[[nodiscard]] inline bool usesSpeed(const AxisCmd c) noexcept
{
    return c == AxisCmd::Rotate || c == AxisCmd::Position || c == AxisCmd::Scan;
}
[[nodiscard]] inline bool usesStart(const AxisCmd c) noexcept
{
    return c == AxisCmd::Position || c == AxisCmd::Scan; // 定位角与扇扫起始角共用 angle1 槽
}
[[nodiscard]] inline bool usesEnd(const AxisCmd c) noexcept { return c == AxisCmd::Scan; }

// ── 物理量程（协议表写死；两轴各不相同，不共用常量）──────────────────────────
inline constexpr double PanSpeedMax = 40.0;  // °/s（码 0~400，分辨率 0.1°/s）
inline constexpr double PanAngleMin = 0.0;   // °（码 0~36000，分辨率 0.01°）
inline constexpr double PanAngleMax = 360.0;
inline constexpr double TiltSpeedMax = 2.0;  // °/s（码 0~20，分辨率 0.1°/s）
inline constexpr double TiltAngleMin = -2.0; // °，本机定义（码域 -200~9200 经换算正好也是 −2~92；分辨率 0.01°，负数补码）
inline constexpr double TiltAngleMax = 92.0;

// ── 角/速度 ↔ 设备码（TX 说 / RX 听在此对望，形制承自 SqafVocab）────────────────
// **一律先钳位再取整**：这是发给电机的最后一道闸。UI 侧的 validator 是第一道，但直发口
// （官邸 on* 槽）与协程都可能绕开 UI，越界角度打到伺服上不是"无反应"那么客气。
// **角 ↔ 码在此一步到位，角一律是本机定义**（俯仰：水平 = 0、抬头为正，窗 [−180, 180)；方位 [0, 360)；行程常量
// 也是本机定义下的）：码里没有"角"的意思，码域怎么对应本机角只写在这四个函数的算式里（用户定）。
// 俯仰的算式是 本机角 = 90° − 码/100（实测：上报 9000 时平视、0 时仰视），方位是 码/100。
[[nodiscard]] inline quint16 PanSpeed2Code(const double dps)
{
    return static_cast<quint16>(std::lround(std::clamp(dps, 0.0, PanSpeedMax) * 10.0));
}
[[nodiscard]] inline quint32 PanAngle2Code(const double deg)
{
    return static_cast<quint32>(std::lround(std::clamp(deg, PanAngleMin, PanAngleMax) * 100.0));
}
[[nodiscard]] inline quint16 TiltSpeed2Code(const double dps)
{
    return static_cast<quint16>(std::lround(std::clamp(dps, 0.0, TiltSpeedMax) * 10.0));
}
// 俯仰码可负：留在有符号域，大端写出即补码（协议原话"负数采用补码形式"）。码 = (90° − 本机角)×100。
[[nodiscard]] inline qint32 TiltAngle2Code(const double deg)
{
    return static_cast<qint32>(std::lround((90.0 - std::clamp(deg, TiltAngleMin, TiltAngleMax)) * 100.0));
}
[[nodiscard]] inline double Code2PanAngle(const quint32 code) { return code / 100.0; }
[[nodiscard]] inline double Code2TiltAngle(const qint32 code) { return 90.0 - code / 100.0; } // 本机角 = 90° − 码/100（绕一圈折回归模型构造）

// ── 累加和：逐字节求和取低 8 位，收 [first, last) 半开区间。
//    下行按协议取 [2,27)；**上行按实机取 [2,15)**（见 UpChecksumEnd 的注）。下行未改：若伺服校验下行也是
//    "校验字节前两个字节不算"，那两个字节（25、26）是恒零的预留位，结果与协议一致——但伺服是否真这么校验，要发命令实测。
[[nodiscard]] inline quint8 checksum(const quint8 *first, const quint8 *last)
{
    quint32 sum = 0;
    for (const quint8 *p = first; p != last; ++p)
        sum += *p;
    return static_cast<quint8>(sum & 0xFF);
}

// ── 上行解帧：官邸调本口解一次，取两角摆成位形交给族层（常态灶与斗不各解一遍）。
// **只解两个角**（帧里还有两个速度量：没有消费者，不解；它们的偏移常量留着，是帧布局的事实），按下标排：
// [SlotPan] 方位角（°）/ [SlotTilt] 俯仰角（°）。BIT 状态字不在此，由 `uplinkStatus` 另读（官邸记报警日志）。
// **不立结构体**（用户定）：两个同型定长的量就是一只数组，布局靠这两个下标说话。
inline constexpr int SlotPan = 0;
inline constexpr int SlotTilt = 1;

// 一帧 20 字节上行 → 两个角。空 = 长度 / 帧头 / 帧尾 / 累加和任一不对。
[[nodiscard]] inline std::optional<std::array<double, 2>> decodeUplink(const QByteArray &raw)
{
    if (raw.size() != UpSize)
        return std::nullopt; // 长度不对
    const auto *p = reinterpret_cast<const quint8 *>(raw.constData());
    if (p[0] != HeadHi || p[1] != HeadLo)
        return std::nullopt; // 帧头不对
    if (p[UpTailOff] != TailHi || p[UpTailOff + 1] != TailLo)
        return std::nullopt; // 帧尾不对
    if (p[UpChecksumOff] != checksum(p + UpStatusOff, p + UpChecksumEnd))
        return std::nullopt; // 累加和不符（实测区间 [2,15)，见 UpChecksumEnd）
    std::array<double, 2> u{};
    u[SlotPan] = Code2PanAngle(qFromBigEndian<quint32>(p + UpPanAngleOff));
    u[SlotTilt] = Code2TiltAngle(qFromBigEndian<qint32>(p + UpTiltAngleOff));
    return u;
}

// 上行 BIT 状态字（大端 quint16，位定义见 Status）。**只在 decodeUplink 验过帧之后调**——本口不再验长度。
[[nodiscard]] inline quint16 uplinkStatus(const QByteArray &raw)
{
    return qFromBigEndian<quint16>(reinterpret_cast<const quint8 *>(raw.constData()) + UpStatusOff);
}

// 状态位 → 协议表名称（按位号升序；表里留空的位不列）。官邸记报警日志用。
inline constexpr std::array<std::pair<Status, const char *>, 8> StatusNames{{
    {PanFault, "方位故障"},
    {TiltFault, "俯仰故障"},
    {TiltLimit0, "俯仰 0° 限位"},
    {TiltLimit90, "俯仰 90° 限位"},
    {PanDriverFault, "方位驱动器故障"},
    {TiltDriverFault, "俯仰驱动故障"},
    {PanAlarm, "方位告警"},
    {TiltAlarm, "俯仰告警"},
}};
} // namespace McztVocab
