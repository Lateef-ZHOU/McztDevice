pragma ComponentBehavior: Bound
import QtQuick
import Lidar.UI
import Lidar.TaskUI
import QtQuick.Controls
import QtQuick.Layouts

// 末端页面独持具体型号后端，在这里连接业务、图窗与控制分区。
ModulePage {
    id: root
    readonly property string kind: "device"
    readonly property GMBDeviceBase family: internals.device
    sections: [connectionSection, positioningSection, modelControlSection].filter(item => item !== null)
    function worldHistory(begin, end) { return angles ? angles.worldHistory(begin, end) : undefined }
    // 界面只选择本机网卡；其他连接参数固定在型号配置中。
    property int networkInterfaceIndex: internals.connectionParameters.networkInterfaceDefault
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
    readonly property Section connectionSection: Section {
        title: qsTr("设备连接")
        FieldRow {
            label: qsTr("网卡")
            ChoiceField {
                Layout.fillWidth: true
                model: internals.connectionParameters.networkInterfaceOptions
                tips: internals.connectionParameters.networkInterfaceTips
                value: root.networkInterfaceIndex
                onCommitted: i => root.networkInterfaceIndex = i
            }
        }
        RowLayout {
            Button {
                Layout.fillWidth: true
                text: qsTr("连接")
                onClicked: {
                    if (internals.device.open(root.networkInterfaceIndex))
                        root.logMessage(qsTr("连接请求已提交"))
                }
            }
            Button { Layout.fillWidth: true; text: qsTr("断开"); onClicked: internals.device.close() }
        }
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
    // 借用公共只读资源接口，不暴露具体型号后端或缓存状态。
    readonly property var deviceState: internals.device.device

    QtObject {
        id: internals
        readonly property var positioningParameters: device.positioningParameters()
        readonly property var connectionParameters: device.connectionParameters
        readonly property McztDevice device: root.McztDevice
        readonly property var controlParameters: device.controlParameters()
    }
    Connections {
        target: internals.device.device
        function onLogMessage(text): void { root.logMessage(text) }
    }
}
