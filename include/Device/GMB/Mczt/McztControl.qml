// McztControl —— 铭辰云台的「设备控制」分区：**只有本台特有的**——速度 / 扇扫与循环。
//
// 两组目标角、前往云台系 / 世界系、立即停止、安装标定都在族层那块「状态与定位」
// （GimbalSection.qml）。没有单轴前往（用户定：用不上，两轴一起前往就够）。
//
// **动作一律直发**（不起 run、不拿锁、不等到位）：这里是测试用的控制，随时能被下一条命令打断。
// 验锁在 Device::send 里统一做——被任务占着就拒并记一条，界面不做门控。
import QtQuick
import Lidar.UI
import QtQuick.Controls
import QtQuick.Layouts

Section {
    id: root
    required property McztDevice device
    // 表单由官邸持有；控件不反向依赖包含自己的具体页面类型。
    required property var settings
    required property var parameters
    required property real panMin
    required property real panMax
    required property real tiltMin
    required property real tiltMax
    title: qsTr("设备控制")

    Label { text: qsTr("速度"); font.bold: true }
    // 控件按协议步距提交；占用时保留原值，SliderField 随后回读。C++ 发令时仍校验、量化。
    FieldRow {
        label: qsTr("水平速度（°/s）")
        SliderField { Layout.fillWidth: true; decimals: 1
                      from: root.parameters.speedStep; to: root.parameters.panSpeedMax
                      stepSize: root.parameters.speedStep
                      value: root.settings.panSpeed
                      onCommitted: function (v) {
                          if (v === root.settings.panSpeed) return
                          if (root.settings.isLocked) {
                              root.settings.logMessage(qsTr("设备正被占用，水平速度未改"))
                              return
                          }
                          root.settings.panSpeed = v
                          root.settings.logMessage(qsTr("水平速度：%1 °/s").arg(v.toFixed(1)))
                      } }
    }
    FieldRow {
        label: qsTr("俯仰速度（°/s）")
        SliderField { Layout.fillWidth: true; decimals: 1
                      from: root.parameters.speedStep; to: root.parameters.tiltSpeedMax
                      stepSize: root.parameters.speedStep
                      value: root.settings.tiltSpeed
                      onCommitted: function (v) {
                          if (v === root.settings.tiltSpeed) return
                          if (root.settings.isLocked) {
                              root.settings.logMessage(qsTr("设备正被占用，俯仰速度未改"))
                              return
                          }
                          root.settings.tiltSpeed = v
                          root.settings.logMessage(qsTr("俯仰速度：%1 °/s").arg(v.toFixed(1)))
                      } }
    }

    Label { text: qsTr("扇扫与循环"); font.bold: true; Layout.topMargin: 6 }
    // **C++ 侧零存储**：四个角只在点「扇扫」那一刻被读，故住界面，点按钮时当实参递下去。
    // **两组角的约束正好相反**，这正是不把"范围输入框"抽成一个控件的理由：
    //   · 水平：起点 / 终点互不约束——方位是环面，150° → 30° 是合法的反向扫；
    //   · 俯仰：下限不得高于上限——俯仰不绕圈，两个框的 validator 互相咬住对方的当前值。
    // 行程上下限由本型号随创建参数交入，设备实际发令仍检查同一型号的范围。
    FieldStrip {
        FieldStrip.Col {
            label: qsTr("水平扇扫（°，起点 → 终点）")
            Layout.preferredWidth: 160 // 两个框，别把最小宽度报小了
            RealField { id: panFrom; Layout.fillWidth: true
                        minimum: root.panMin; maximum: root.panMax
                        value: 30
                        onCommitted: function (v) { panFrom.value = v } }
            RealField { id: panTo; Layout.fillWidth: true
                        minimum: root.panMin; maximum: root.panMax
                        value: 150
                        onCommitted: function (v) { panTo.value = v } }
        }
        FieldStrip.Col {
            label: qsTr("俯仰扇扫（°，下限 ~ 上限）")
            Layout.preferredWidth: 160
            RealField { id: tiltLo; Layout.fillWidth: true
                        minimum: root.tiltMin; maximum: tiltHi.value
                        value: 10
                        onCommitted: function (v) { tiltLo.value = v } }
            RealField { id: tiltHi; Layout.fillWidth: true
                        minimum: tiltLo.value; maximum: root.tiltMax
                        value: 60
                        onCommitted: function (v) { tiltHi.value = v } }
        }
    }
    // 按轴分两列、按动作分两行：上排扇扫、下排循环，右下那格是本台**没有**的档。
    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 6
        rowSpacing: 6
        Button { Layout.fillWidth: true; text: qsTr("水平扇扫")
                 onClicked: if (root.device.scanPan(panFrom.value, panTo.value, root.settings.panSpeed))
                     root.settings.logMessage(qsTr("已下发：水平扇扫 %1° → %2°")
                         .arg(panFrom.value.toFixed(2)).arg(panTo.value.toFixed(2))) }
        Button { Layout.fillWidth: true; text: qsTr("俯仰扇扫")
                 onClicked: if (root.device.scanTilt(tiltLo.value, tiltHi.value, root.settings.tiltSpeed))
                     root.settings.logMessage(qsTr("已下发：俯仰扇扫 %1° ~ %2°")
                         .arg(tiltLo.value.toFixed(2)).arg(tiltHi.value.toFixed(2))) }
        Button { Layout.fillWidth: true; text: qsTr("水平循环")
                 onClicked: if (root.device.rotatePan(root.settings.panSpeed))
                     root.settings.logMessage(qsTr("已下发：水平循环 %1°/s").arg(root.settings.panSpeed.toFixed(1))) }
        // **这个 disable 不是交互门控**（那条纪律禁的是"按运行状态动态启停"）：俯仰轴**根本没有
        // 循环档**——协议如此，McztTiltMeta 连 rotate() 都没有、
        // 写了当场编不过。摆一个灰的而不是干脆不摆，是为了让人看得见"这一格是空的"，
        // 免得对着三个按钮猜第四个去哪了。
        Button { Layout.fillWidth: true; text: qsTr("俯仰循环"); enabled: false }
    }
    Label {
        text: qsTr("俯仰轴没有循环档（协议不支持）。")
        color: palette.placeholderText
        font.pointSize: 9
        wrapMode: Text.Wrap
        Layout.preferredWidth: 1 // 换行文字的 implicitWidth 是「整段不折行」的宽度，会顶爆列宽
        Layout.fillWidth: true
    }

    // 与「状态与定位」那颗是同一个动作（issueStop），故名字也一样——
    // 两处叫法不同会让人以为是两种停法（用户定）。摆两处是因为两处都用得着。
    Button {
        Layout.fillWidth: true
        Layout.topMargin: 4
        text: qsTr("立即停止")
        onClicked: {
            if (root.device.issueStop())
                root.settings.logMessage(qsTr("已下发：停止"))
        }
    }
}
