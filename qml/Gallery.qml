import QtQuick
import QtQuick.Controls
import EdView

Rectangle {
  id: gallery

  property bool open: false
  readonly property alias hovered: hover.hovered

  signal activated(int index)

  height: 176
  color: "#b30c0c10"
  y: open ? parent.height - height : parent.height + 8
  visible: y < parent.height

  Behavior on y {
    NumberAnimation {
      duration: 220
      easing.type: Easing.OutCubic
    }
  }

  Rectangle {
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.top: parent.top
    height: 1
    color: "#22ffffff"
  }

  HoverHandler {
    id: hover
  }

  ListView {
    id: list

    anchors.fill: parent
    anchors.topMargin: 10
    anchors.bottomMargin: 10
    orientation: ListView.Horizontal
    spacing: 16
    leftMargin: Math.max(28, (width - contentWidth) / 2)
    rightMargin: 28
    model: Viewer.images
    currentIndex: Viewer.images.currentIndex
    highlightRangeMode: ListView.ApplyRange
    preferredHighlightBegin: width / 2 - 70
    preferredHighlightEnd: width / 2 + 70
    highlightMoveDuration: 250
    cacheBuffer: 1600
    boundsBehavior: Flickable.StopAtBounds

    WheelHandler {
      onWheel: (event) => list.flick(event.angleDelta.y * 12, 0)
    }

    delegate: Item {
      id: cell

      required property int index
      required property string thumbUrl
      required property string fileName
      readonly property bool current: index === Viewer.images.currentIndex

      width: 120
      height: list.height
      z: cellMouse.containsMouse ? 2 : (current ? 1 : 0)

      Rectangle {
        anchors.centerIn: parent
        width: 112
        height: 112
        radius: 10
        color: "#26ffffff"
        border.width: cell.current ? 3 : 0
        border.color: "#4a8af0"
        scale: cellMouse.containsMouse ? 1.3 : 1.0

        Behavior on scale {
          NumberAnimation {
            duration: 140
            easing.type: Easing.OutCubic
          }
        }

        Image {
          anchors.fill: parent
          anchors.margins: 4
          source: cell.thumbUrl
          sourceSize: Qt.size(224, 224)
          asynchronous: true
          fillMode: Image.PreserveAspectCrop
          clip: true
          smooth: true
        }
      }

      MouseArea {
        id: cellMouse

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: gallery.activated(cell.index)
      }

      ToolTip.visible: cellMouse.containsMouse
      ToolTip.delay: 700
      ToolTip.text: cell.fileName
    }
  }
}
