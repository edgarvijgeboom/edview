import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EdView

Popup {
  id: dialog

  property var info: ({})
  readonly property var rows: {
    const list = []
    const add = (label, value) => {
      if (value !== undefined && value !== null && String(value).length > 0)
        list.push({ label: label, value: String(value) })
    }
    if (info.dateTaken)
      add("Date taken", info.dateTaken)
    else
      add("File date", info.modified)
    add("Image type", info.type)
    if (info.width > 0)
      add("Image size", info.width + " × " + info.height + " px")
    if (info.editedWidth > 0)
      add("Edited size", info.editedWidth + " × " + info.editedHeight + " px")
    add("File size", info.fileSize)
    add("Location", info.location ? info.location : "Not available")
    add("Camera", info.camera ? info.camera : "Not available")
    add("Lens", info.lens)
    add("Exposure", info.exposure)
    add("Folder", info.folder)
    return list
  }

  function show() {
    info = Viewer.imageInfo()
    open()
  }

  modal: true
  focus: true
  anchors.centerIn: Overlay.overlay
  width: Math.min(620, parent ? parent.width - 80 : 620)
  padding: 26
  closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

  background: Rectangle {
    radius: 14
    color: "#f01c1c21"
    border.color: "#33ffffff"
  }

  contentItem: ColumnLayout {
    spacing: 18

    Label {
      Layout.fillWidth: true
      text: dialog.info.fileName || ""
      font.pixelSize: 18
      font.bold: true
      elide: Text.ElideMiddle
    }

    ColumnLayout {
      Layout.fillWidth: true
      spacing: 10

      Repeater {
        model: dialog.rows

        RowLayout {
          required property var modelData

          Layout.fillWidth: true
          spacing: 16

          Label {
            Layout.preferredWidth: 110
            Layout.alignment: Qt.AlignTop
            text: modelData.label
            opacity: 0.6
          }

          Label {
            Layout.fillWidth: true
            text: modelData.value
            wrapMode: Text.WrapAnywhere
            textFormat: Text.PlainText
          }
        }
      }
    }

    RowLayout {
      Layout.alignment: Qt.AlignRight
      spacing: 10

      PillButton {
        visible: dialog.info.hasLocation === true
        text: "Show on map"
        onClicked: Qt.openUrlExternally("https://www.openstreetmap.org/?mlat=" + dialog.info.latitude + "&mlon=" + dialog.info.longitude + "#map=16/" + dialog.info.latitude + "/" + dialog.info.longitude)
      }

      PillButton {
        text: "Close"
        accent: true
        onClicked: dialog.close()
      }
    }
  }
}
