import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EdView

Popup {
  id: dialog

  function applyPercent(percent) {
    widthBox.value = Math.max(1, Math.round(Viewer.imageWidth * percent / 100))
    heightBox.value = Math.max(1, Math.round(Viewer.imageHeight * percent / 100))
  }

  modal: true
  focus: true
  anchors.centerIn: Overlay.overlay
  padding: 24
  closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

  onOpened: {
    widthBox.value = Viewer.imageWidth
    heightBox.value = Viewer.imageHeight
    widthBox.forceActiveFocus()
  }

  background: Rectangle {
    radius: 14
    color: "#f01c1c21"
    border.color: "#33ffffff"
  }

  contentItem: ColumnLayout {
    spacing: 16

    Label {
      text: "Scale image"
      font.pixelSize: 18
      font.bold: true
    }

    Label {
      text: "Current size: " + Viewer.imageWidth + " × " + Viewer.imageHeight + " px"
      opacity: 0.7
    }

    GridLayout {
      columns: 3
      columnSpacing: 12
      rowSpacing: 10

      Label {
        text: "Width"
      }

      SpinBox {
        id: widthBox

        from: 1
        to: 100000
        editable: true
        textFromValue: (value) => String(value)
        valueFromText: (text) => Math.max(1, parseInt(text.replace(/\D/g, "")) || 1)
        validator: RegularExpressionValidator {
          regularExpression: /\d{1,6}/
        }
        onValueModified: {
          if (lockBox.checked && Viewer.imageWidth > 0)
            heightBox.value = Math.max(1, Math.round(value * Viewer.imageHeight / Viewer.imageWidth))
        }
      }

      Label {
        text: "px"
      }

      Label {
        text: "Height"
      }

      SpinBox {
        id: heightBox

        from: 1
        to: 100000
        editable: true
        textFromValue: (value) => String(value)
        valueFromText: (text) => Math.max(1, parseInt(text.replace(/\D/g, "")) || 1)
        validator: RegularExpressionValidator {
          regularExpression: /\d{1,6}/
        }
        onValueModified: {
          if (lockBox.checked && Viewer.imageHeight > 0)
            widthBox.value = Math.max(1, Math.round(value * Viewer.imageWidth / Viewer.imageHeight))
        }
      }

      Label {
        text: "px"
      }
    }

    CheckBox {
      id: lockBox

      text: "Keep aspect ratio"
      checked: true
    }

    RowLayout {
      spacing: 8

      Repeater {
        model: [25, 50, 75, 150, 200]

        PillButton {
          required property int modelData

          text: modelData + "%"
          onClicked: dialog.applyPercent(modelData)
        }
      }
    }

    RowLayout {
      Layout.alignment: Qt.AlignRight
      spacing: 10

      PillButton {
        text: "Cancel"
        onClicked: dialog.close()
      }

      PillButton {
        text: "Apply"
        accent: true
        onClicked: {
          Viewer.resize(widthBox.value, heightBox.value)
          dialog.close()
        }
      }
    }
  }
}
