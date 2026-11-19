#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlListReference>
#include <QtQml/QQmlExtensionPlugin>
#include <QDebug>
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
        if (form.value("tiltMin").toDouble() != -90.0 || form.value("tiltMax").toDouble() != 270.0)
            return 8;
        for (int index : {1, 2}) {
            if (sections.at(index)->property("tiltMin").toDouble() != -90.0
                || sections.at(index)->property("tiltMax").toDouble() != 270.0) return 9;
        }
        for (const double angle : {-90.0, -45.0, 150.0, 270.0}) {
            const auto params = device->paramsFor(PanTiltModel(0, angle), {}, 40, 2);
            if (!device->withinTravel(0, angle) || !params || params->target.tilt() != angle) return 10;
            const auto packets = McztCollector(*params).buildStart();
            if (packets.size() != 1 || packets.front().size() != McztVocab::DownSize
                || qFromBigEndian<qint32>(packets.front().data() + McztVocab::DownTiltAngle1Off)
                    != static_cast<qint32>((90 - angle) * 100)) return 11;
        }
        const auto scan = McztVocab::buildFrame(McztPanMeta::hold(), McztTiltMeta::scan(-90, 270, 2));
        if (qFromBigEndian<qint32>(scan.constData() + McztVocab::DownTiltAngle1Off) != 18000
            || qFromBigEndian<qint32>(scan.constData() + McztVocab::DownTiltAngle2Off) != -18000
            || device->withinTravel(0, -90.01) || device->withinTravel(0, 270.01)
            || McztVocab::withinTravel(McztPanMeta::hold(), McztTiltMeta::scan(-90, 270.01, 2))) return 12;
        const QPointer<QObject> lifetime(backend);
        app.processEvents();
        page.reset();
        if (lifetime) return 6;
    }
    return warned ? 4 : 0;
}
