#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlListReference>
#include <QtQml/QQmlExtensionPlugin>
#include <QDebug>
#include <array>
#include <chrono>
#include <thread>
#include <cmath>
#include <utility>
#include <memory>
#include <QPointer>
#include <QtEndian>
#include "McztDevice.h"
#include "DeviceBase.h"
#include "GMBFamily/GMBDevice.h"
Q_IMPORT_QML_PLUGIN(LidarUIPlugin)
Q_IMPORT_QML_PLUGIN(LidarChartsPlugin)
Q_IMPORT_QML_PLUGIN(LidarTaskUIPlugin)
Q_IMPORT_QML_PLUGIN(LidarMcztDevicePlugin)
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QQmlEngine engine;
    bool warned = false;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine, [&](const auto &) { warned = true; });
    {
        QQmlComponent component(&engine);
        component.setData("import Lidar.Mczt\nMcztModule {  }", QUrl());
        std::unique_ptr<QObject> page(component.create());
        if (!page) { qCritical() << component.errors(); return 1; }
        const QQmlListReference charts(page.get(), "mainPages");
        const QQmlListReference sections(page.get(), "sections");
        if (charts.count() < 1 || sections.count() < 1) return 2;
        if (page->property("name").toString().isEmpty()) return 3;
        QObject *backend = nullptr;
        int backends = 0;
        for (auto *child : page->children()) {
            if (QByteArray(child->metaObject()->className()) == "McztDevice") {
                backend = child;
                ++backends;
            }
        }
        if (backends != 1 || backend->parent() != page.get()) return 5;
        // 设备与云台族是同一个对象：family 即后端本身，可交叉转换到纯 C++ 族基。
        if (page->property("family").value<QObject *>() != backend || !dynamic_cast<GMBFamily *>(backend)) return 7;
        auto *device = qobject_cast<McztDevice *>(backend);
        const auto form = device->positioningParameters();
        if (form.value("tiltMin").toDouble() != 0.0 || form.value("tiltMax").toDouble() != 180.0)
            return 8;
        for (int index : {1, 2}) {
            if (sections.at(index)->property("tiltMin").toDouble() != 0.0
                || sections.at(index)->property("tiltMax").toDouble() != 180.0) return 9;
        }
        for (const double angle : {0.0, 45.0, 90.0, 150.0, 180.0}) {
            const auto params = device->paramsFor(PanTiltModel(0, angle), {}, 40, 2);
            if (!device->withinTravel(0, angle) || !params || params->target.tilt() != angle) return 10;
            const auto packets = McztCollector(*params).buildStart();
            if (packets.size() != 1 || packets.front().size() != McztVocab::DownSize
                || qFromBigEndian<qint32>(packets.front().data() + McztVocab::DownTiltAngle1Off)
                    != static_cast<qint32>((angle - 90) * 100)) return 11;
        }
        const auto scan = McztVocab::buildFrame(McztPanMeta::hold(), McztTiltMeta::scan(0, 180, 2));
        if (qFromBigEndian<qint32>(scan.constData() + McztVocab::DownTiltAngle1Off) != -9000
            || qFromBigEndian<qint32>(scan.constData() + McztVocab::DownTiltAngle2Off) != 9000
            || device->withinTravel(0, -0.01) || device->withinTravel(0, 180.01)
            || McztVocab::withinTravel(McztPanMeta::hold(), McztTiltMeta::scan(-0.01, 180, 2))
            || McztVocab::withinTravel(McztPanMeta::hold(), McztTiltMeta::scan(0, 180.01, 2))) return 12;
        if (sections.at(1)->property("initialTilt").toDouble() != 0.0
            || device->paramsFor(PanTiltModel(0, -0.01), {}, 40, 2)
            || device->paramsFor(PanTiltModel(0, 180.01), {}, 40, 2)) return 13;
        for (const auto &[code, angle] : std::array<std::pair<qint32, double>, 3>{{{-9000, 0}, {0, 90}, {9000, 180}}}) {
            std::array<unsigned char, McztVocab::UpSize> report{};
            report[0] = McztVocab::HeadHi;
            report[1] = McztVocab::HeadLo;
            report[McztVocab::UpTailOff] = McztVocab::TailHi;
            report[McztVocab::UpTailOff + 1] = McztVocab::TailLo;
            qToBigEndian(code, report.data() + McztVocab::UpTiltAngleOff);
            report[McztVocab::UpChecksumOff] = McztVocab::checksum(
                report.data() + McztVocab::UpStatusOff, report.data() + McztVocab::UpChecksumEnd);
            const auto decoded = McztVocab::decodeUplink(report);
            if (!decoded || (*decoded)[McztVocab::SlotTilt] != angle) return 18;
        }
        constexpr double epsilon = 1e-6;
        for (const double tilt : {0.0, 89.99, 90.0, 90.01, 150.0, 180.0}) {
            const PanTiltModel pose(180.01, tilt);
            const auto display = device->worldForReadout(pose);
            if (AngularDistance(display.azimuth(), pose.pan()) > epsilon
                || std::abs(display.elevation() - tilt) > epsilon) return 14;
        }
        const PanTiltModel overTop(180.01, 180);
        const auto canonical = device->worldOf(overTop);
        if (AngularDistance(canonical.azimuth(), 0.01) > epsilon
            || std::abs(canonical.elevation()) > epsilon) return 15;
        const auto target = device->paramsFor(device->gimbalOf(WorldModel(180, 20)), {}, 40, 2);
        if (!target) return 19;
        McztCollector arrival(*target);
        const PanTiltModel reported(179.92, McztVocab::Code2TiltAngle(-7000));
        const auto hold = GMBCollector::kMinHold;
        if (arrival.feed(PanTiltModel(179.87, reported.tilt()), {})) return 19;
        std::this_thread::sleep_for(hold);
        if (arrival.feed(reported, {})) return 19; // 水平越界样本不计入驻留。
        std::this_thread::sleep_for(hold);
        if (arrival.feed(PanTiltModel(reported.pan(), 20.06), {})) return 19; // 俯仰容差仍为 0.05°。
        if (arrival.feed(reported, {})) return 19; // 出圈后重新计算驻留。
        std::this_thread::sleep_for(hold);
        if (!arrival.feed(reported, {})) return 19;
        if (!device->setMounting(20, 10, 5)) return 16;
        const auto calibrated = device->worldForReadout(PanTiltModel(45, 145));
        const auto actual = calibrated.toOrientation();
        const auto expected = device->worldOf(PanTiltModel(45, 145)).toOrientation();
        if (std::abs(calibrated.roll()) > 90
            || SO3::logRotation(expected.inverseRotation() * actual).norm() > epsilon) return 17;
        const QPointer<QObject> lifetime(backend);
        app.processEvents();
        page.reset();
        if (lifetime) return 6;
    }
    return warned ? 4 : 0;
}
