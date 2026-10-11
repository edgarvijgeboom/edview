import QtQuick
import QtQuick.Controls

ToolButton {
  id: control

  property string iconName
  property string tip
  property int iconSize: 22
  property bool round: false
  property color fill: "transparent"

  icon.name: iconName
  icon.width: iconSize
  icon.height: iconSize
  icon.color: enabled ? "#f4f4f4" : "#55ffffff"
  display: AbstractButton.IconOnly
  text: tip
  hoverEnabled: true
  focusPolicy: Qt.NoFocus
  implicitWidth: iconSize + 18
  implicitHeight: iconSize + 18
  ToolTip.visible: hovered && tip.length > 0
  ToolTip.delay: 600
  ToolTip.text: tip

  background: Rectangle {
    radius: control.round ? width / 2 : 8
    color: control.down ? "#4dffffff" : control.hovered ? "#2effffff" : control.fill
  }
}
