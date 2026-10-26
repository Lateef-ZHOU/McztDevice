#pragma once
#include "Device/GMB/GMBCollector.h"
#include <QByteArray>
#include <QList>
#include <QtEndian>
#include <QtGlobal>

#include "Device/GMB/Mczt/McztVocab.h" // 协议实现层：帧常量 + AxisCmd + 角速度↔码 + 累加和


// ── AxisMeta / McztPanMeta / McztTiltMeta：**一轴的意图**，两轴各一份 ────────────────
//
// 两轴各立一个类型，收益有三，前两条是结构性的：
//   ① **俯仰没有循环档这件事是编译期禁止**：`McztTiltMeta` 压根没有 `rotate()` 这一口，写错当场
//      编不过——而不是成帧时把 Rotate 悄悄折成 Hold（"你可以写错，我替你改对"）。
//   ② **两个轴是两个类型**，`startOf(pan, tilt)` 传反了编不过（两者是兄弟，没有互相转换的路；
//      同 GMBCollector / DAQCollector 那一对族标签的判例）。
//   ③ 两轴的对称是类型本身的对称，不靠 `pan*` / `tilt*` 字段前缀。
//
// **为何是继承，不是 `AxisMeta<轴枚举>`**（用户定）：**轴数在本层是板上钉钉**——协议就这两个
// 轴，既不会扩也不会参数化。拿一个两值枚举做非类型模板参数，那个枚举除了"把两个类型除开"没有
// 第二个用处，是为分型而造的分型器。分两个类，名字直接就是两个类型，共享那四个字段与四个读口
// 正是继承的本职；`rotate()` 只写在方位那一边，也就不必拿 `requires` 去抓一个本不存在的通用形状。
// （基类无虚函数、不经基类指针删除，故不需虚析构；它纯是**共享实现**的基类，对外的唯一用途
//  是 `withinTravel` 里逐轴检查的形参类型——那一步只读 mode / start / end / speed，两轴同法。）
//
// **裸构造不公开、公开只具名口**（MAC / SO3 / ARPPacket 同判）。这里不是偏好，是**被迫**：
// Hold / Stop / 告警复位 / 标零 四口全是零参，重载构造根本区分不了它们；而 `McztPanMeta{5.0}`
// 这种拼写也说不出"这是循环还是定位"。具名口是唯一写得出来的形状。
// 附带好处：**"只填该填的槽"是构造期的事实**——造出来的对象里，与本模式无关的字段恒是 0，
// 不可能留着上一轮的残值。
//
// **槽位谓词仍然要用**（`usesSpeed/usesStart/usesEnd`）：成帧写零的依据是**协议表**，不是
// "0.0 恰好编成 0"（为什么见 buildFrame 里写零那句）。
class AxisMeta
{
public:
    [[nodiscard]] McztVocab::AxisCmd mode() const noexcept { return mode_; }
    [[nodiscard]] double start() const noexcept { return start_; } // 定位角 / 扇扫起始角
    [[nodiscard]] double end() const noexcept { return end_; }     // 扇扫终止角
    [[nodiscard]] double speed() const noexcept { return speed_; } // °/s

protected:
    // **唯一构造口，且不公开**：没有默认构造（"一个没说要干什么的轴意图"不该存在，要表达
    // 不干预请写 `hold()`）；protected 又令本类只能作为子对象存在。
    explicit AxisMeta(const McztVocab::AxisCmd m, const double s = 0.0, const double e = 0.0,
                      const double v = 0.0)
        : mode_(m), start_(s), end_(e), speed_(v)
    {
    }

private:
    McztVocab::AxisCmd mode_;
    double start_;
    double end_;
    double speed_;
};

// 方位轴：协议七档全有。
class McztPanMeta final : public AxisMeta
{
public:
    [[nodiscard]] static McztPanMeta hold() { return McztPanMeta{McztVocab::AxisCmd::Hold}; }
    [[nodiscard]] static McztPanMeta stop() { return McztPanMeta{McztVocab::AxisCmd::Stop}; }
    [[nodiscard]] static McztPanMeta position(const double deg, const double dps)
    {
        return McztPanMeta{McztVocab::AxisCmd::Position, deg, 0.0, dps};
    }
    [[nodiscard]] static McztPanMeta scan(const double from, const double to, const double dps)
    {
        return McztPanMeta{McztVocab::AxisCmd::Scan, from, to, dps};
    }
    // **只有方位轴有**：协议表的俯仰 00 位留空，物理上也讲得通（俯仰只有 -2°~+92° 一段行程）。
    // 口就写在这个类里，`McztTiltMeta::rotate(...)` 遂是一句编译错误，而不是一次运行期降格。
    [[nodiscard]] static McztPanMeta rotate(const double dps)
    {
        return McztPanMeta{McztVocab::AxisCmd::Rotate, 0.0, 0.0, dps};
    }

protected:
    using AxisMeta::AxisMeta; // 继承构造：访问级随基类（仍是 protected），具名口在类内用得到
};

// 俯仰轴：比方位少一档循环。**少的那一档就是字面上少一个函数**，不靠约束也不靠降格。
class McztTiltMeta final : public AxisMeta
{
public:
    [[nodiscard]] static McztTiltMeta hold() { return McztTiltMeta{McztVocab::AxisCmd::Hold}; }
    [[nodiscard]] static McztTiltMeta stop() { return McztTiltMeta{McztVocab::AxisCmd::Stop}; }
    [[nodiscard]] static McztTiltMeta position(const double deg, const double dps)
    {
        return McztTiltMeta{McztVocab::AxisCmd::Position, deg, 0.0, dps};
    }
    [[nodiscard]] static McztTiltMeta scan(const double from, const double to, const double dps)
    {
        return McztTiltMeta{McztVocab::AxisCmd::Scan, from, to, dps};
    }

protected:
    using AxisMeta::AxisMeta;
};

// ── 意图 → 指令：McztVocab 的运行侧延伸（自由函数）──────────
// 词汇表（McztVocab.h）只认协议常量与码、不认 meta；本段认 meta，故住运行侧、在 meta 之后重开同一命名空间。
// 两处共用（设备页直发、官邸造斗时交给族级收集器），都经 startOf。
namespace McztVocab
{
// 两份意图 → 30 字节 UDP 载荷。
// 载荷即整帧：帧头 / 累加和 / 帧尾都是 MCZT 自己的成帧，不是 UDP 的——以太/IP/UDP 那层
// 信封归 PcapHandle::onTX，两层信封各司其层。
// 量程钳位在各 *2Code 里（发给电机的最后一道闸），此处只管把码放进槽位。
[[nodiscard]] inline QByteArray buildFrame(const McztPanMeta &pm, const McztTiltMeta &tm)
{
    QByteArray frame(DownSize, '\0'); // 三字节预留区靠这一次零填到位，下面不再碰
    auto *p = reinterpret_cast<quint8 *>(frame.data());

    p[0] = HeadHi;
    p[1] = HeadLo;

    // 俯仰没有 Rotate：由 `McztTiltMeta` 没有 `rotate()` 在编译期挡下，此处不必降格。
    const AxisCmd pan = pm.mode();
    const AxisCmd tilt = tm.mode();
    p[DownPanCmdOff] = static_cast<quint8>(pan);
    p[DownTiltCmdOff] = static_cast<quint8>(tilt);

    // 用不到的槽位写零：判据来自**协议表**（哪个命令带哪几个数据），不看 meta 里存了什么。
    // 具名口已保证残值不存在，但担保写零的仍是协议表——规范优先于巧合（今天两者结论一致：各 *2Code 都是无偏移线性）。
    qToBigEndian<quint16>(usesSpeed(pan) ? PanSpeed2Code(pm.speed()) : quint16{0}, p + DownPanSpeedOff);
    qToBigEndian<quint32>(usesStart(pan) ? PanAngle2Code(pm.start()) : quint32{0}, p + DownPanAngle1Off);
    qToBigEndian<quint32>(usesEnd(pan) ? PanAngle2Code(pm.end()) : quint32{0}, p + DownPanAngle2Off);
    // 俯仰起止角各自换算、不调换顺序：协议只写"起始 / 终止"，不规定大小关系（换算后两个码的大小关系与角相反，
    // 伺服挑不挑顺序待实机证实）。
    qToBigEndian<quint16>(usesSpeed(tilt) ? TiltSpeed2Code(tm.speed()) : quint16{0}, p + DownTiltSpeedOff);
    qToBigEndian<qint32>(usesStart(tilt) ? TiltAngle2Code(tm.start()) : qint32{0}, p + DownTiltAngle1Off);
    qToBigEndian<qint32>(usesEnd(tilt) ? TiltAngle2Code(tm.end()) : qint32{0}, p + DownTiltAngle2Off);

    // [2,27)：不含帧头、不含自身
    p[DownChecksumOff] = checksum(p + DownPanCmdOff, p + DownChecksumOff);
    p[DownTailOff] = TailHi;
    p[DownTailOff + 1] = TailLo;
    return frame;
}

// 预览与实际命令共用角度范围判断；预览不构造运动意图或读取速度。
[[nodiscard]] inline bool angleWithin(const double value, const double minimum, const double maximum)
{
    return value >= minimum && value <= maximum;
}
[[nodiscard]] inline bool withinAngles(const double pan, const double tilt)
{
    return angleWithin(pan, PanAngleMin, PanAngleMax) && angleWithin(tilt, TiltAngleMin, TiltAngleMax);
}

// 两轴意图在不在本云台行程内（协议量程；只查本档用得到的槽位，依据协议表 usesStart / usesEnd / usesSpeed）。
// 行程常量是本机定义下的角，直接比。
// 官邸造斗与设备页直发都先过这一道——组包程序有义务检查数据是否超限。
[[nodiscard]] inline bool withinTravel(const McztPanMeta &pan, const McztTiltMeta &tilt)
{
    const auto axisOk = [](const AxisMeta &m, const double angleMin, const double angleMax, const double speedMax)
    {
        const auto angleOk = [&](const double v) { return angleWithin(v, angleMin, angleMax); };
        return (!usesStart(m.mode()) || angleOk(m.start())) && (!usesEnd(m.mode()) || angleOk(m.end())) &&
               (!usesSpeed(m.mode()) || (m.speed() >= 0.0 && m.speed() <= speedMax));
    };
    return axisOk(pan, PanAngleMin, PanAngleMax, PanSpeedMax) &&
           axisOk(tilt, TiltAngleMin, TiltAngleMax, TiltSpeedMax);
}

// 意图 → 启动指令（恰一帧）。
[[nodiscard]] inline QList<QByteArray> startOf(const McztPanMeta &pan, const McztTiltMeta &tilt)
{
    return {buildFrame(pan, tilt)};
}

// 停机指令：两轴都停（打断路发它）。停一个本就没动的轴无害。
[[nodiscard]] inline QList<QByteArray> stopOf()
{
    return startOf(McztPanMeta::stop(), McztTiltMeta::stop());
}
} // namespace McztVocab

struct McztCollectorTag;
using McztCollector = TypedGMBCollector<McztCollectorTag>;
