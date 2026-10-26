pragma ComponentBehavior: Bound
import QtQuick
import Lidar.UI
import Lidar.TaskUI
import QtQuick.Controls

// 末端页面独持具体型号后端，在这里连接业务、图窗与控制分区。
ModulePage {
    id: root
    readonly property string kind: "device"
    readonly property GMBDeviceBase family: internals.device
    sections: [connectionSection, positioningSection, modelControlSection].filter(item => item !== null)
    function worldHistory(begin, end) { return angles ? angles.worldHistory(begin, end) : undefined }
    // 待连接表单；编辑不修改 Handle 的运行快照。
    property int networkInterfaceIndex: internals.connectionParameters.networkInterfaceDefault
    property string hostIPv4: internals.connectionParameters.hostIPv4Default
    property int hostPort: internals.connectionParameters.hostPortDefault
    property string deviceIPv4: internals.connectionParameters.deviceIPv4Default
    property int devicePort: internals.connectionParameters.devicePortDefault
    property string deviceMAC: internals.connectionParameters.deviceMACDefault
    property real panSpeed: internals.controlParameters.panSpeedDefault
    property real tiltSpeed: internals.controlParameters.tiltSpeedDefault
    function moveParameters() { return ({ panSpeed: root.panSpeed, tiltSpeed: root.tiltSpeed }) }
    mainPages: [Page {
        title: qsTr("角度")
        contentItem: GimbalCharts {
            id: angles
            device: internals.device
            configuration: internals.device.chartParameters().configuration
        }
    }]
    readonly property Section connectionSection: PcapCard {
        device: internals.device
        settings: root
        parameters: internals.connectionParameters
    }
    readonly property Section positioningSection: GimbalSection {
        device: internals.device
        settings: root
        panMin: internals.positioningParameters.panMin
        panMax: internals.positioningParameters.panMax
        tiltMin: internals.positioningParameters.tiltMin
        tiltMax: internals.positioningParameters.tiltMax
    }
    readonly property Section modelControlSection: McztControl {
        device: internals.device
        settings: root
        parameters: internals.controlParameters
        panMin: internals.positioningParameters.panMin
        panMax: internals.positioningParameters.panMax
        tiltMin: internals.positioningParameters.tiltMin
        tiltMax: internals.positioningParameters.tiltMax
    }
    objectName: "McztModule"
    readonly property string name: internals.device.device.displayName
    readonly property string subtitle: internals.device.device.subtitle
    readonly property bool isOpen: internals.device.device.isOpen
    readonly property bool isLocked: internals.device.device.isLocked
    readonly property string holder: internals.device.device.holder

    QtObject {
        id: internals
        readonly property var positioningParameters: device.positioningParameters()
        readonly property var connectionParameters: device.connectionParameters
        readonly property McztDevice device: root.McztDevice
        readonly property var controlParameters: device.controlParameters()
    }
    Connections {
        target: internals.device.device.logSource
        function onLogMessage(text): void { root.logMessage(text) }
    }
}
