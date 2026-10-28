#pragma once
#include <QtQml/qqmlregistration.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

#include "Algorithm/Geometry/GimbalModel.h"                     // PanTiltModel（造斗收的目标位形）
#include "Device/PcapForm.h"
#include "Device/GMB/GMBDevice.h"                  // 云台族两层（族基 GMBDeviceBase + 族层 GMBDevice）
#include "Device/GMB/Mczt/McztConfig.h"         // 型号夹宪法
#include "Device/GMB/Mczt/McztRunSide.h"        // 运行侧：两轴意图 + 意图 → 指令

// ============================================================
// McztDevice.h — MCZT46 电机云台型号夹的**官邸**（运行侧住 McztRunSide.h，词典住
// McztVocab.h）。以太/IP/UDP 信封归 PcapHandle，MCZT 自己那层帧（EB90 / 累加和 / 146F）
// 整本归本夹。
//
// **同一套连接机制上的第二类设备**：DAQ/Hfnl / DAQ/LidarQ 是采集卡，本夹是云台，三者共用
// PcapHandle。正因为它，采集卡的共同语言（AcqSaver）不能挂在连接层上——**连接方式与设备族
// 是两根轴**（网口协议由 HandleCore 的 Pcap 模块提供）。
// ============================================================

// ============================================================
// McztDevice — MCZT46 云台设备子类：子夹最高长官（职责对齐 SqafDevice）。四件事：
//   ① makeMoveCollector 按目标位形及启动时传入的速度快照构造定位 Collector。
//      逐轴动作（扇扫 / 循环 / 停止）只走设备页直发，不造斗。
//   ② 常态灶的解析口 decodeStream：上报**不因有没有 run 而中断**；一帧同报两轴，解一次交出实测位形
//      （回显与喂斗共用）。换世界角、画图、喂斗归族层与机制层。
//   ③ **是一台 pan-tilt 云台**：族层写死用 `PanTiltModel` 反解与表达位形；本类不继承、不持有模型。
//      设备的真实行程是协议量程（McztVocab::*AngleMin/Max），成帧时的 *2Code 钳位是最后一道闸。
//   ④ 型号事实按值交给自己的 QML 官邸，页面内容和分区顺序由 QML 决定。
//
// **两轴动作逐轴独立**：协议的七档是逐轴的——告警复位、标零、停止在协议表里都分方位 / 俯仰两个
// 位，上行的 BIT 状态字也分方位告警（10 位）与俯仰告警（11 位）。故动作口都按逐轴意图成帧
// （动一根轴时另一根给 Hold），不设"两轴同档同参"的便捷口。告警复位、标零两档协议有、本机没有客户，词典只留档位。
// ============================================================
class McztDevice final
#ifdef Q_MOC_RUN
    : public GMBDeviceBase // 模板层继承的族级元对象。
#else
    : public GMBDevice<PcapHandle, McztCollector>
#endif
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap connectionParameters READ connectionParameters CONSTANT)
    QML_ELEMENT
    QML_ATTACHED(McztDevice)
    QML_UNCREATABLE("由对应 QML 官邸创建和拥有")
    // 连接只接收网卡下标；线路参数固定在 McztConfig。
    // 速度设置仅在 QML 官邸；命令和任务工厂显式接收本次值。
    // 标定整值从族层读取；固定行程随 QML 模板按值交入，不另存设备属性。

public:
    static McztDevice *qmlAttachedProperties(QObject *owner) { return new McztDevice(owner); }
    [[nodiscard]] QVariantMap connectionParameters() const
    {
        return PcapForm::connectionParameters(McztConfig::hostIPv4, McztConfig::hostPort,
                                                McztConfig::deviceIPv4, McztConfig::devicePort, std::nullopt);
    }
    Q_INVOKABLE QVariantMap positioningParameters() const
    {
        return {{"panMin", McztVocab::PanAngleMin}, {"panMax", McztVocab::PanAngleMax},
                 {"tiltMin", McztVocab::TiltAngleMin}, {"tiltMax", McztVocab::TiltAngleMax}};
    }

    Q_INVOKABLE [[nodiscard]] QVariantMap chartParameters() const
    {
        return {{"configuration", QVariantMap{
                     {"windowSeconds", std::chrono::duration<double>(McztConfig::angleChartWindow).count()},
                     {"historySeconds", std::chrono::duration<double>(McztConfig::angleHistoryWindow).count()},
                     {"intervalSeconds", std::chrono::duration<double>(McztConfig::uplinkInterval).count()}}}};
    }

    ~McztDevice() override { PreDestruction(); }

    // 角一律本机定义（俯仰：水平 = 0、抬头为正）：码↔角的换算在 McztVocab，这里、界面、任务都只见本机角。
    explicit McztDevice(QObject *parent) : GMBDevice(QStringLiteral("铭辰云台"), QStringLiteral("网口 · UDP"), parent)
    {
        setObjectName(QStringLiteral("Mczt")); // ASCII 标识：落盘会话目录名用
    }

    // 族口：**本台方位轴带滑环**——可一直同向转，跨零不必回绕（任务页的轮末那一笔据此画）。
    [[nodiscard]] bool hasSlipRing() const noexcept final { return true; }

    // 预览只查询型号判据，不造 Collector、不发令、不记录操作日志。
    Q_INVOKABLE bool withinTravel(const double pan, const double tilt) const override
    {
        return McztVocab::withinAngles(pan, tilt);
    }

    Q_INVOKABLE QVariantMap controlParameters() const
    {
        return {{"speedStep", McztConfig::speedStep}, {"panSpeedMax", McztVocab::PanSpeedMax},
                {"tiltSpeedMax", McztVocab::TiltSpeedMax}, {"panSpeedDefault", McztConfig::panSpeedDefault},
                {"tiltSpeedDefault", McztConfig::tiltSpeedDefault}};
    }

private:
    void PreDestruction() noexcept final
    {
        static_assert(FinalDevice<McztDevice, decltype(&McztDevice::PreDestruction)>);
        GMBDevice::PreDestruction();
    }

    // 协议量化只作用于本次值，不回写或缓存 QML 设置。
    [[nodiscard]] std::optional<double> commandSpeed(const double value, const double maximum) const
    {
        if (!std::isfinite(value))
        {
            Q_EMIT this->device().logSource().logMessage(QStringLiteral("云台速度不是有限数，未下发"));
            return std::nullopt;
        }
        const double step = McztConfig::speedStep;
        return std::round(std::clamp(value, step, maximum) / step) * step;
    }
    [[nodiscard]] std::optional<GMBMoveParameters::Speeds> commandSpeeds(const GMBMoveParameters &parameters) const
    {
        if (!parameters.speeds)
        {
            Q_EMIT this->device().logSource().logMessage(QStringLiteral("铭辰定位缺少两轴速度，未下发"));
            return std::nullopt;
        }
        const auto pan = commandSpeed(parameters.speeds->pan, McztVocab::PanSpeedMax);
        if (!pan) return std::nullopt;
        const auto tilt = commandSpeed(parameters.speeds->tilt, McztVocab::TiltSpeedMax);
        if (!tilt) return std::nullopt;
        return GMBMoveParameters::Speeds{*pan, *tilt};
    }

    // 任务的速度快照在此编码入指令；循环和重试只复用 Collector，不回读界面。
    [[nodiscard]] std::shared_ptr<const McztCollector>
    makeMoveCollector(const PanTiltModel &target, const std::chrono::milliseconds hold,
                      const GMBMoveParameters &parameters) const override
    {
        const auto speeds = commandSpeeds(parameters);
        if (!speeds) return nullptr;
        const auto pan = McztPanMeta::position(target.pan(), speeds->pan);
        const auto tilt = McztTiltMeta::position(target.tilt(), speeds->tilt);
        if (!McztVocab::withinTravel(pan, tilt)) // 与设备页直发同一道检查
            return nullptr;
        return McztCollector::createPrototype(target, McztConfig::angleTol, hold,
                                              McztVocab::startOf(pan, tilt), McztVocab::stopOf(),
                                              McztConfig::expectedDurationMs);
    }

    // [worker] 族层常态灶的解析口。本云台 100 Hz 自动上报，**不因有没有 run 而中断**，且一帧同报两轴：
    // 解一次（`McztVocab::decodeUplink`）交出实测位形——回显与喂斗共用，**每一口都交**。
    // 上行帧里的两个速度量不用：判到位由族级收集器按"进容差圈后稳住多久"判。
    // **BIT 状态字按协议记报警日志**（用户定）：帧验过之后读状态字，变化时记"报警 / 解除"（见 logStatusChange）。
    // 解不开的帧直接丢弃（不回显、不喂斗、不记日志）。
    [[nodiscard]] std::optional<PanTiltModel> decodeStream(const QByteArray &raw) override
    {
        const auto u = McztVocab::decodeUplink(raw);
        if (!u)
            return std::nullopt; // 长度 / 帧头 / 帧尾 / 累加和任一不对：不回显、不喂斗
        logStatusChange(McztVocab::uplinkStatus(raw));
        return PanTiltModel((*u)[McztVocab::SlotPan], (*u)[McztVocab::SlotTilt]);
    }

    // [worker] BIT 状态字 → 报警日志（协议：故障 / 限位 / 告警各占一位）。**只在变化时记**——上报 100 Hz，
    // 逐帧记会淹没日志：新置位的记"云台报警"，清除的记"报警解除"。出生态全零 = 无报警，故上电即带着的
    // 报警在首帧就记。
    // **协议表没列的位也记**：没名字的按原始值记一条「未列位 0xNNNN」。不认没列的位，就分不清
    // "伺服没置位"与"置了我们不认的位"（实机遇到过：俯仰压到限位却一条报警都没有）——这种"看不见"
    // 本身就是缺陷，设备的异常状态至少要是**响的**。
    void logStatusChange(const quint16 status)
    {
        const auto names = [](const quint16 bits)
        {
            quint16 named = 0;
            for (const auto &[bit, name] : McztVocab::StatusNames)
                named |= bit;
            QStringList list;
            for (const auto &[bit, name] : McztVocab::StatusNames)
                if (bits & bit)
                    list << QString::fromUtf8(name);
            if (const auto rest = static_cast<quint16>(bits & ~named); rest != 0)
                list << QStringLiteral("未列位 0x%1").arg(rest, 4, 16, QLatin1Char('0'));
            return list.join(QStringLiteral("、"));
        };
        const QString raised = names(static_cast<quint16>(status & ~status_));
        const QString cleared = names(static_cast<quint16>(status_ & ~status));
        status_ = status;
        if (!raised.isEmpty())
            Q_EMIT this->device().logSource().logMessage(QStringLiteral("云台报警：") + raised);
        if (!cleared.isEmpty())
            Q_EMIT this->device().logSource().logMessage(QStringLiteral("报警解除：") + cleared);
    }

    quint16 status_ = 0; // [仅 worker] 上一帧的 BIT 状态字（只为判变化）

    // ════════════════════════════════════════════════════════════════════════════
    // 设备页：族层规定分区，本官邸交组件地址和完整创建参数。
    //   · 设备连接 —— 网口参数 + 连接 / 断开（QML 表单，显式调用 open / close）；
    //   · 设备控制 —— 两个速度、水平扇扫 / 俯仰扇扫 / 水平循环、停止。
    //     （前往云台角 / 前往世界角 / 安装标定是族级的，在族层那块"状态与定位"分区里。）
    // **两轴动作按逐轴意图直接成帧**（McztVocab::buildFrame 是自由函数，不造斗），逐轴独立：动一根轴时
    // 另一根给 Hold。成帧前一律查行程（withinTravel，与造斗同一道）。
    // **水平扇扫用起点 / 终点两个独立输入，不用区间输入**（用户定）：10→50 与 50→10 扫的是互补的两段
    // （经过 0° 那段与不经过的那段），区间输入会把它们当成同一段。俯仰扇扫没有绕回问题，用区间输入。
    // **动作一律直发**（用户定）：这里是测试用的控制，不起 run、不拿锁、不等到位——随时能被下一条
    // 命令（包括停止）打断，测试阶段要的正是这个。任务里的自动控制走协程与锁。
    // **但验锁**：验锁统一在 `Device::send`（canEngage）——设备页传空令牌 = "我不持锁"，设备空闲且
    // 已连接才放行，被任务占着一律拒并记日志（连停止也拒：任务的停止走任务页）。
    // BIT 状态字：报警 / 解除写进本台日志（decodeStream 里按变化记），不单独显示。
    // ════════════════════════════════════════════════════════════════════════════
// **动作对 QML 公开**：那两份 .qml 直接按名调这些槽（McztControl.qml 与族层的
// GimbalSection.qml；按名绑定的耦合关在型号夹内部——QML 与官邸同作者、一起改）。
public:
    using GMBDevice::setMounting;
public Q_SLOTS:
    bool open(const int interfaceIndex)
    {
        const auto params = PcapForm::parseParameters(this->device().logSource(), interfaceIndex,
            McztConfig::hostIPv4, McztConfig::hostPort, McztConfig::deviceIPv4, McztConfig::devicePort,
            std::nullopt, GMBDevice::kPcapReadTimeout, GMBDevice::kPcapKernelBufferBytes);
        return params && this->device().open(*params);
    }
    void close() { this->device().close(); }
    // Qt 入口接收页面参数；成功只返回结果，文案由 QML 生成。标定三个角一次提交。
    bool issueGoTo(const double pan, const double tilt, const QVariantMap &parameters)
    {
        return GMBDevice::issueGoTo(pan, tilt, parameters);
    }
    bool setMounting(const double azimuth, const double elevation, const double roll)
    {
        return GMBDeviceBase::setMounting(WorldModel(azimuth, elevation, roll), {});
    }
    bool issueStop() override { return issue(McztPanMeta::stop(), McztTiltMeta::stop()); }

    // 起点 / 终点**不互相约束**：方位是环面，150° → 30° 是合法的反向扫。
    // （俯仰扇扫则要 lo ≤ hi——同是"两个框"，约束却相反，这正是不把它抽成一个控件的理由。）
    bool scanPan(const double from, const double to, const double panSpeed)
    {
        const auto speed = commandSpeed(panSpeed, McztVocab::PanSpeedMax);
        return speed && issue(McztPanMeta::scan(from, to, *speed), McztTiltMeta::hold());
    }
    bool rotatePan(const double panSpeed)
    {
        const auto speed = commandSpeed(panSpeed, McztVocab::PanSpeedMax);
        return speed && issue(McztPanMeta::rotate(*speed), McztTiltMeta::hold());
    }
    bool scanTilt(const double lower, const double upper, const double tiltSpeed)
    {
        const auto speed = commandSpeed(tiltSpeed, McztVocab::TiltSpeedMax);
        return speed && issue(McztPanMeta::hold(), McztTiltMeta::scan(lower, upper, *speed));
    }

private:
    // 两轴定位使用显式快照；组包前的检查与 Collector 工厂相同。
    bool issueGoTo(const PanTiltModel &target, const GMBMoveParameters &parameters) override
    {
        const auto speeds = commandSpeeds(parameters);
        return speeds && issue(McztPanMeta::position(target.pan(), speeds->pan),
                               McztTiltMeta::position(target.tilt(), speeds->tilt));
    }

    // 直发一帧（不造斗、不起 run、不拿锁，见上方分区注）。组包前查行程，与造斗同一道 McztVocab::withinTravel：
    // 越界不发、记一条。验锁与连接由 `send`（canEngage）判，拒发时它自己记日志。
    bool issue(const McztPanMeta &pan, const McztTiltMeta &tilt)
    {
        if (!McztVocab::withinTravel(pan, tilt))
        {
            Q_EMIT this->device().logSource().logMessage(QStringLiteral("云台运动参数超出协议量程（方位 0~360°、俯仰 −2°~+92°），未下发"));
            return false;
        }
        return this->device().send(McztVocab::startOf(pan, tilt), {}); // 空令牌 = 我不持锁；与 Collector 共用成帧
    }

};
