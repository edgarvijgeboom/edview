import QtQuick
import QtQuick.Controls

Button {
  id: control

  property bool accent: false

  hoverEnabled: true
  leftPadding: 18
  rightPadding: 18
  topPadding: 9
  bottomPadding: 9
  font.pixelSize: 14

  contentItem: Text {
    text: control.text
    font: control.font
    color: control.enabled ? "#ffffff" : "#66ffffff"
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
  }

  background: Rectangle {
    implicitHeight: 38
    radius: height / 2
    color: control.accent
      ? (control.down ? "#2f6fd6" : control.hovered ? "#4a8af0" : "#3d7ee8")
      : (control.down ? "#55ffffff" : control.hovered ? "#40ffffff" : "#2affffff")
  }
}
