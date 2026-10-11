import QtQuick

Item {
  id: overlay

  required property Item target
  required property int sourceWidth
  required property int sourceHeight
  property real cropX: 0
  property real cropY: 0
  property real cropWidth: sourceWidth
  property real cropHeight: sourceHeight
  readonly property real factor: sourceWidth > 0 ? target.width / sourceWidth : 1
  readonly property real boxX: target.x + cropX * factor
  readonly property real boxY: target.y + cropY * factor
  readonly property real boxWidth: cropWidth * factor
  readonly property real boxHeight: cropHeight * factor
  readonly property int minimumSize: 8

  signal accepted()

  function reset() {
    cropX = Math.round(sourceWidth * 0.05)
    cropY = Math.round(sourceHeight * 0.05)
    cropWidth = Math.round(sourceWidth * 0.9)
    cropHeight = Math.round(sourceHeight * 0.9)
  }

  function selectAll() {
    cropX = 0
    cropY = 0
    cropWidth = sourceWidth
    cropHeight = sourceHeight
  }

  function moveFrom(start, dx, dy) {
    cropX = Math.min(Math.max(0, start.x + dx), sourceWidth - start.width)
    cropY = Math.min(Math.max(0, start.y + dy), sourceHeight - start.height)
  }

  function resizeFrom(start, horizontal, vertical, dx, dy) {
    let x1 = start.x
    let y1 = start.y
    let x2 = start.x + start.width
    let y2 = start.y + start.height
    if (horizontal < 0)
      x1 = Math.min(Math.max(0, x1 + dx), x2 - minimumSize)
    if (horizontal > 0)
      x2 = Math.max(Math.min(sourceWidth, x2 + dx), x1 + minimumSize)
    if (vertical < 0)
      y1 = Math.min(Math.max(0, y1 + dy), y2 - minimumSize)
    if (vertical > 0)
      y2 = Math.max(Math.min(sourceHeight, y2 + dy), y1 + minimumSize)
    cropX = x1
    cropY = y1
    cropWidth = x2 - x1
    cropHeight = y2 - y1
  }

  MouseArea {
    anchors.fill: parent
    acceptedButtons: Qt.AllButtons
    onWheel: (wheel) => wheel.accepted = true
  }

  Rectangle {
    x: overlay.target.x
    y: overlay.target.y
    width: overlay.target.width
    height: overlay.boxY - overlay.target.y
    color: "#a6000000"
  }

  Rectangle {
    x: overlay.target.x
    y: overlay.boxY + overlay.boxHeight
    width: overlay.target.width
    height: overlay.target.y + overlay.target.height - y
    color: "#a6000000"
  }

  Rectangle {
    x: overlay.target.x
    y: overlay.boxY
    width: overlay.boxX - overlay.target.x
    height: overlay.boxHeight
    color: "#a6000000"
  }

  Rectangle {
    x: overlay.boxX + overlay.boxWidth
    y: overlay.boxY
    width: overlay.target.x + overlay.target.width - x
    height: overlay.boxHeight
    color: "#a6000000"
  }

  Rectangle {
    id: frame

    x: overlay.boxX
    y: overlay.boxY
    width: overlay.boxWidth
    height: overlay.boxHeight
    color: "transparent"
    border.color: "#ffffff"
    border.width: 1.5

    Repeater {
      model: 2

      Rectangle {
        required property int index

        x: frame.width * (index + 1) / 3
        width: 1
        height: frame.height
        color: "#59ffffff"
      }
    }

    Repeater {
      model: 2

      Rectangle {
        required property int index

        y: frame.height * (index + 1) / 3
        width: frame.width
        height: 1
        color: "#59ffffff"
      }
    }

    MouseArea {
      property point origin
      property rect start

      anchors.fill: parent
      cursorShape: Qt.SizeAllCursor
      onDoubleClicked: overlay.accepted()
      onPressed: (mouse) => {
        origin = mapToItem(overlay, mouse.x, mouse.y)
        start = Qt.rect(overlay.cropX, overlay.cropY, overlay.cropWidth, overlay.cropHeight)
      }
      onPositionChanged: (mouse) => {
        const point = mapToItem(overlay, mouse.x, mouse.y)
        overlay.moveFrom(start, (point.x - origin.x) / overlay.factor, (point.y - origin.y) / overlay.factor)
      }
    }
  }

  Repeater {
    model: [
      { h: -1, v: -1, cursor: Qt.SizeFDiagCursor },
      { h: 0, v: -1, cursor: Qt.SizeVerCursor },
      { h: 1, v: -1, cursor: Qt.SizeBDiagCursor },
      { h: 1, v: 0, cursor: Qt.SizeHorCursor },
      { h: 1, v: 1, cursor: Qt.SizeFDiagCursor },
      { h: 0, v: 1, cursor: Qt.SizeVerCursor },
      { h: -1, v: 1, cursor: Qt.SizeBDiagCursor },
      { h: -1, v: 0, cursor: Qt.SizeHorCursor }
    ]

    Rectangle {
      id: handle

      required property var modelData

      width: 14
      height: 14
      radius: 3
      color: "#ffffff"
      border.color: "#202024"
      x: overlay.boxX + (modelData.h + 1) / 2 * overlay.boxWidth - width / 2
      y: overlay.boxY + (modelData.v + 1) / 2 * overlay.boxHeight - height / 2

      MouseArea {
        property point origin
        property rect start

        anchors.fill: parent
        anchors.margins: -8
        cursorShape: handle.modelData.cursor
        onPressed: (mouse) => {
          origin = mapToItem(overlay, mouse.x, mouse.y)
          start = Qt.rect(overlay.cropX, overlay.cropY, overlay.cropWidth, overlay.cropHeight)
        }
        onPositionChanged: (mouse) => {
          const point = mapToItem(overlay, mouse.x, mouse.y)
          overlay.resizeFrom(start, handle.modelData.h, handle.modelData.v, (point.x - origin.x) / overlay.factor, (point.y - origin.y) / overlay.factor)
        }
      }
    }
  }
}
