import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import EdView

ApplicationWindow {
  id: root

  property real zoom: 1
  property bool fitMode: true
  property real panX: 0
  property real panY: 0
  property bool cropMode: false
  property bool uiVisible: true
  property bool galleryOpen: false
  property bool showBusy: false
  property var pendingAction: null
  readonly property bool hasImage: Viewer.imageWidth > 0 && Viewer.imageHeight > 0
  readonly property bool canPan: photo.width > stage.width + 0.5 || photo.height > stage.height + 0.5
  readonly property bool modalOpen: scaleDialog.visible || unsavedDialog.visible || deleteDialog.visible
    || renameDialog.visible || infoDialog.visible || contextMenu.visible
  readonly property bool fullScreen: visibility === Window.FullScreen

  function fitZoomValue() {
    if (!hasImage || stage.width <= 0 || stage.height <= 0)
      return 1
    return Math.min(stage.width / Viewer.imageWidth, stage.height / Viewer.imageHeight, 1)
  }

  function clampPan() {
    panX = Math.min(0, Math.max(stage.width - photo.width, panX))
    panY = Math.min(0, Math.max(stage.height - photo.height, panY))
  }

  function relayout() {
    if (fitMode)
      zoom = fitZoomValue()
    else
      clampPan()
  }

  function fit() {
    fitMode = true
    zoom = fitZoomValue()
  }

  function setZoom(value, px, py) {
    if (!hasImage)
      return
    const minimum = Math.min(fitZoomValue(), 0.05)
    const next = Math.max(minimum, Math.min(40, value))
    const ix = (px - photo.x) / zoom
    const iy = (py - photo.y) / zoom
    fitMode = false
    zoom = next
    panX = px - ix * next
    panY = py - iy * next
    clampPan()
  }

  function actualSize() {
    setZoom(1, stage.width / 2, stage.height / 2)
  }

  function guard(action) {
    if (Viewer.modified) {
      pendingAction = action
      unsavedDialog.open()
    } else {
      action()
    }
  }

  function runPending() {
    const action = pendingAction
    pendingAction = null
    if (action)
      action()
  }

  function navigate(step) {
    if (Viewer.images.count < 2 || cropMode)
      return
    guard(() => step > 0 ? Viewer.next() : Viewer.previous())
  }

  function pokeUi() {
    uiVisible = true
    idleTimer.restart()
  }

  function startCrop() {
    if (!hasImage)
      return
    fit()
    cropOverlay.reset()
    cropMode = true
    pokeUi()
  }

  function applyCrop() {
    Viewer.crop(Math.round(cropOverlay.cropX), Math.round(cropOverlay.cropY), Math.round(cropOverlay.cropWidth), Math.round(cropOverlay.cropHeight))
    cropMode = false
    fit()
  }

  function askDelete() {
    if (Viewer.path.length > 0 && !cropMode)
      deleteDialog.open()
  }

  function askRename() {
    if (Viewer.path.length > 0 && !cropMode)
      renameDialog.open()
  }

  function showInfo() {
    if (Viewer.path.length > 0)
      infoDialog.show()
  }

  function toggleFullScreen() {
    visibility = fullScreen ? Window.Windowed : Window.FullScreen
  }

  width: 1400
  height: 900
  minimumWidth: 720
  minimumHeight: 480
  visible: true
  visibility: Window.FullScreen
  title: Viewer.fileName.length > 0 ? Viewer.fileName + " — EdView" : "EdView"
  color: "transparent"
  font.pixelSize: 14

  background: Item {
    Rectangle {
      anchors.fill: parent
      color: Qt.rgba(0.128, 0.128, 0.141, 0.45)
    }

    Rectangle {
      anchors.fill: parent
      color: "#000000"
      opacity: 0.6
    }
  }

  palette {
    window: "#1c1c21"
    windowText: "#f2f2f2"
    base: "#26262c"
    alternateBase: "#2e2e35"
    text: "#f2f2f2"
    button: "#33333b"
    buttonText: "#f2f2f2"
    highlight: "#3d7ee8"
    highlightedText: "#ffffff"
    light: "#4a4a54"
    midlight: "#3c3c45"
    mid: "#2c2c33"
    dark: "#16161a"
    shadow: "#000000"
    toolTipBase: "#26262c"
    toolTipText: "#f2f2f2"
    placeholderText: "#80f2f2f2"
  }

  onClosing: (close) => {
    if (Viewer.modified) {
      close.accepted = false
      guard(() => Qt.quit())
    }
  }

  Component.onCompleted: {
    pokeUi()
    if (Viewer.path.length === 0)
      openDialog.open()
  }

  Connections {
    target: Viewer

    function onDocumentOpened() {
      root.cropMode = false
      root.fit()
    }

    function onImageChanged() {
      root.relayout()
    }

    function onLoadingChanged() {
      if (!Viewer.loading)
        root.showBusy = false
    }

    function onMessage(text) {
      toast.show(text)
    }
  }

  Timer {
    id: busyTimer

    interval: 180
    running: Viewer.loading
    onTriggered: root.showBusy = true
  }

  Timer {
    id: idleTimer

    interval: 2500
    onTriggered: {
      if (root.fullScreen && root.hasImage && !topHover.hovered && !gallery.hovered && !root.cropMode && !root.modalOpen)
        root.uiVisible = false
    }
  }

  Item {
    id: stage

    anchors.fill: parent
    clip: true
    onWidthChanged: root.relayout()
    onHeightChanged: root.relayout()

    Image {
      id: photo

      source: Viewer.imageUrl
      asynchronous: false
      cache: false
      smooth: root.zoom < 3
      mipmap: true
      width: Viewer.imageWidth * root.zoom
      height: Viewer.imageHeight * root.zoom
      x: width <= stage.width ? (stage.width - width) / 2 : root.panX
      y: height <= stage.height ? (stage.height - height) / 2 : root.panY
    }

    MouseArea {
      id: stageMouse

      property point pressPoint
      property real startX
      property real startY
      property bool dragged: false

      anchors.fill: parent
      hoverEnabled: true
      enabled: !root.cropMode
      acceptedButtons: Qt.LeftButton | Qt.RightButton
      cursorShape: !root.uiVisible && root.fullScreen ? Qt.BlankCursor
        : pressed && dragged ? Qt.ClosedHandCursor
        : root.canPan ? Qt.OpenHandCursor : Qt.ArrowCursor

      onPressed: (mouse) => {
        if (mouse.button === Qt.RightButton) {
          root.pokeUi()
          if (Viewer.path.length > 0)
            contextMenu.popup()
          return
        }
        pressPoint = Qt.point(mouse.x, mouse.y)
        startX = photo.x
        startY = photo.y
        dragged = false
      }

      onPositionChanged: (mouse) => {
        root.pokeUi()
        if (mouse.y > height - 48)
          root.galleryOpen = true
        else if (mouse.y < height - gallery.height - 24)
          root.galleryOpen = false
        if (!(pressedButtons & Qt.LeftButton))
          return
        const dx = mouse.x - pressPoint.x
        const dy = mouse.y - pressPoint.y
        if (!dragged && Math.abs(dx) + Math.abs(dy) > 6)
          dragged = true
        if (dragged && root.canPan) {
          root.panX = startX + dx
          root.panY = startY + dy
          root.clampPan()
        }
      }

      onReleased: (mouse) => {
        if (mouse.button !== Qt.LeftButton || dragged)
          return
        if (mouse.x < width * 0.3)
          root.navigate(-1)
        else if (mouse.x > width * 0.7)
          root.navigate(1)
      }

      onDoubleClicked: (mouse) => {
        if (mouse.button !== Qt.LeftButton || mouse.x < width * 0.3 || mouse.x > width * 0.7)
          return
        if (root.fitMode)
          root.setZoom(1, mouse.x, mouse.y)
        else
          root.fit()
      }

      onWheel: (wheel) => {
        root.setZoom(root.zoom * Math.pow(1.0015, wheel.angleDelta.y), wheel.x, wheel.y)
        root.pokeUi()
      }
    }

    CropOverlay {
      id: cropOverlay

      anchors.fill: parent
      visible: root.cropMode
      target: photo
      sourceWidth: Viewer.imageWidth
      sourceHeight: Viewer.imageHeight
      onAccepted: root.applyCrop()
    }
  }

  BusyIndicator {
    anchors.centerIn: parent
    running: root.showBusy
    visible: running
  }

  Column {
    anchors.centerIn: parent
    spacing: 16
    visible: !root.hasImage && !Viewer.loading

    Text {
      anchors.horizontalCenter: parent.horizontalCenter
      text: Viewer.errorString.length > 0 ? Viewer.errorString : "No image loaded"
      color: Viewer.errorString.length > 0 ? "#ff9a9a" : "#ccffffff"
      font.pixelSize: 16
    }

    PillButton {
      anchors.horizontalCenter: parent.horizontalCenter
      text: "Open image…"
      accent: true
      onClicked: openDialog.open()
    }
  }

  IconButton {
    anchors.left: parent.left
    anchors.leftMargin: 20
    anchors.verticalCenter: parent.verticalCenter
    iconName: "go-previous"
    tip: "Previous (←)"
    iconSize: 34
    implicitWidth: 64
    implicitHeight: 64
    round: true
    fill: "#59000000"
    visible: Viewer.images.count > 1 && !root.cropMode
    opacity: root.uiVisible ? 1 : 0
    onClicked: root.navigate(-1)

    Behavior on opacity {
      NumberAnimation {
        duration: 200
      }
    }
  }

  IconButton {
    anchors.right: parent.right
    anchors.rightMargin: 20
    anchors.verticalCenter: parent.verticalCenter
    iconName: "go-next"
    tip: "Next (→)"
    iconSize: 34
    implicitWidth: 64
    implicitHeight: 64
    round: true
    fill: "#59000000"
    visible: Viewer.images.count > 1 && !root.cropMode
    opacity: root.uiVisible ? 1 : 0
    onClicked: root.navigate(1)

    Behavior on opacity {
      NumberAnimation {
        duration: 200
      }
    }
  }

  Rectangle {
    id: topBar

    anchors.left: parent.left
    anchors.right: parent.right
    anchors.top: parent.top
    height: 56
    color: "#99000000"
    opacity: root.uiVisible ? 1 : 0
    visible: opacity > 0

    Behavior on opacity {
      NumberAnimation {
        duration: 200
      }
    }

    HoverHandler {
      id: topHover
    }

    Column {
      anchors.left: parent.left
      anchors.leftMargin: 18
      anchors.verticalCenter: parent.verticalCenter
      width: Math.max(120, parent.width / 2 - centerLabel.implicitWidth / 2 - 40)
      spacing: 2

      Text {
        width: parent.width
        text: Viewer.fileName
        color: "#ffffff"
        font.pixelSize: 14
        elide: Text.ElideMiddle
      }

      Text {
        width: parent.width
        color: "#99ffffff"
        font.pixelSize: 11
        elide: Text.ElideRight
        text: (Viewer.images.count > 0 ? (Viewer.images.currentIndex + 1) + " / " + Viewer.images.count : "")
          + (Viewer.decodeInfo.length > 0 ? "   ·   " + Viewer.decodeInfo : "")
      }
    }

    Text {
      id: centerLabel

      anchors.centerIn: parent
      color: "#ffffff"
      font.pixelSize: 15
      font.weight: Font.DemiBold
      text: !root.hasImage ? ""
        : root.cropMode ? "Crop   " + Math.round(cropOverlay.cropWidth) + " × " + Math.round(cropOverlay.cropHeight) + " px"
        : Viewer.imageWidth + " × " + Viewer.imageHeight + " px   ·   " + Math.round(root.zoom * 100) + "%" + (Viewer.modified ? "   ·   edited" : "")
    }

    Row {
      anchors.right: parent.right
      anchors.rightMargin: 12
      anchors.verticalCenter: parent.verticalCenter
      spacing: 2

      IconButton {
        iconName: "document-open"
        tip: "Open (Ctrl+O)"
        onClicked: root.guard(() => openDialog.open())
      }

      Rectangle {
        width: 1
        height: 24
        anchors.verticalCenter: parent.verticalCenter
        color: "#33ffffff"
      }

      IconButton {
        iconName: "object-rotate-left"
        tip: "Rotate left (Shift+R)"
        enabled: root.hasImage && !root.cropMode
        onClicked: Viewer.rotate(-90)
      }

      IconButton {
        iconName: "object-rotate-right"
        tip: "Rotate right (R)"
        enabled: root.hasImage && !root.cropMode
        onClicked: Viewer.rotate(90)
      }

      IconButton {
        iconName: "transform-crop"
        tip: "Crop (C)"
        enabled: root.hasImage
        fill: root.cropMode ? "#403d7ee8" : "transparent"
        onClicked: {
          if (root.cropMode)
            root.cropMode = false
          else
            root.startCrop()
        }
      }

      IconButton {
        iconName: "transform-scale"
        tip: "Scale (S)"
        enabled: root.hasImage && !root.cropMode
        onClicked: scaleDialog.open()
      }

      IconButton {
        iconName: "edit-undo"
        tip: "Revert changes (Ctrl+Z)"
        enabled: Viewer.modified && !root.cropMode
        onClicked: Viewer.revert()
      }

      Rectangle {
        width: 1
        height: 24
        anchors.verticalCenter: parent.verticalCenter
        color: "#33ffffff"
      }

      IconButton {
        iconName: "document-save"
        tip: "Save (Ctrl+S)"
        enabled: Viewer.modified && !root.cropMode
        onClicked: Viewer.save()
      }

      IconButton {
        iconName: "document-save-as"
        tip: "Save as other format (Ctrl+Shift+S)"
        enabled: root.hasImage && !root.cropMode
        onClicked: saveDialog.open()
      }

      Rectangle {
        width: 1
        height: 24
        anchors.verticalCenter: parent.verticalCenter
        color: "#33ffffff"
      }

      IconButton {
        iconName: "zoom-fit-best"
        tip: "Fit to window (0)"
        enabled: root.hasImage
        onClicked: root.fit()
      }

      IconButton {
        iconName: "zoom-original"
        tip: "Actual size (1)"
        enabled: root.hasImage
        onClicked: root.actualSize()
      }

      IconButton {
        iconName: root.fullScreen ? "view-restore" : "view-fullscreen"
        tip: "Fullscreen (F)"
        onClicked: root.toggleFullScreen()
      }

      IconButton {
        iconName: "window-close"
        tip: "Close (Esc)"
        onClicked: root.close()
      }
    }
  }

  Row {
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 36
    spacing: 12
    visible: root.cropMode

    PillButton {
      text: "Select all"
      onClicked: cropOverlay.selectAll()
    }

    PillButton {
      text: "Cancel"
      onClicked: root.cropMode = false
    }

    PillButton {
      text: "Apply crop"
      accent: true
      onClicked: root.applyCrop()
    }
  }

  Gallery {
    id: gallery

    anchors.left: parent.left
    anchors.right: parent.right
    open: root.galleryOpen && !root.cropMode && Viewer.images.count > 0
    onActivated: (index) => root.guard(() => Viewer.openIndex(index))
  }

  Rectangle {
    id: toast

    function show(text) {
      toastText.text = text
      opacity = 1
      toastTimer.restart()
    }

    anchors.horizontalCenter: parent.horizontalCenter
    y: (gallery.open ? parent.height - gallery.height : parent.height) - height - 24
    width: toastText.implicitWidth + 36
    height: 40
    radius: 10
    color: "#e6202024"
    opacity: 0
    visible: opacity > 0

    Behavior on opacity {
      NumberAnimation {
        duration: 180
      }
    }

    Text {
      id: toastText

      anchors.centerIn: parent
      color: "#ffffff"
      font.pixelSize: 14
    }

    Timer {
      id: toastTimer

      interval: 2400
      onTriggered: toast.opacity = 0
    }
  }

  ScaleDialog {
    id: scaleDialog
  }

  InfoDialog {
    id: infoDialog
  }

  Menu {
    id: contextMenu

    MenuItem {
      text: "Info"
      icon.name: "documentinfo"
      onTriggered: root.showInfo()
    }

    MenuItem {
      text: "Delete"
      icon.name: "edit-delete"
      onTriggered: root.askDelete()
    }

    MenuItem {
      text: "Rename"
      icon.name: "edit-rename"
      onTriggered: root.askRename()
    }
  }

  Dialog {
    id: deleteDialog

    anchors.centerIn: Overlay.overlay
    width: 460
    modal: true
    title: "Are you sure?"
    standardButtons: Dialog.Yes | Dialog.No
    onAccepted: Viewer.deleteCurrent()

    Label {
      width: parent.width
      wrapMode: Text.Wrap
      text: "Delete " + Viewer.fileName + "?\nThe file will be moved to the Trash."
    }
  }

  Dialog {
    id: renameDialog

    anchors.centerIn: Overlay.overlay
    width: 460
    modal: true
    title: "Rename"
    standardButtons: Dialog.Ok | Dialog.Cancel
    onOpened: {
      nameField.text = Viewer.fileName
      const dot = Viewer.fileName.lastIndexOf(".")
      nameField.select(0, dot > 0 ? dot : Viewer.fileName.length)
      nameField.forceActiveFocus()
    }
    onAccepted: Viewer.renameCurrent(nameField.text)

    TextField {
      id: nameField

      width: parent.width
      selectByMouse: true
      onAccepted: renameDialog.accept()
    }
  }

  Dialog {
    id: unsavedDialog

    anchors.centerIn: Overlay.overlay
    width: 440
    modal: true
    title: "Unsaved changes"
    standardButtons: Dialog.Save | Dialog.Discard | Dialog.Cancel

    onAccepted: {
      if (Viewer.save())
        root.runPending()
      else
        root.pendingAction = null
    }

    onDiscarded: {
      Viewer.revert()
      unsavedDialog.close()
      root.runPending()
    }

    onRejected: root.pendingAction = null

    Label {
      text: "Save the changes to " + Viewer.fileName + "?"
    }
  }

  FileDialog {
    id: openDialog

    title: "Open image"
    fileMode: FileDialog.OpenFile
    nameFilters: Viewer.openFilters
    currentFolder: Viewer.folderUrl
    onAccepted: Viewer.open(selectedFile)
  }

  FileDialog {
    id: saveDialog

    title: "Save image as"
    fileMode: FileDialog.SaveFile
    nameFilters: Viewer.saveFilters
    currentFolder: Viewer.folderUrl
    selectedFile: Viewer.suggestedSaveUrl
    onAccepted: Viewer.saveAs(selectedFile, selectedNameFilter.index)
  }

  Shortcut {
    sequences: ["Right", "Space", "PgDown"]
    enabled: !root.cropMode && !root.modalOpen
    onActivated: root.navigate(1)
  }

  Shortcut {
    sequences: ["Left", "Backspace", "PgUp"]
    enabled: !root.cropMode && !root.modalOpen
    onActivated: root.navigate(-1)
  }

  Shortcut {
    sequence: "Home"
    enabled: !root.cropMode && !root.modalOpen
    onActivated: root.guard(() => Viewer.openIndex(0))
  }

  Shortcut {
    sequence: "End"
    enabled: !root.cropMode && !root.modalOpen
    onActivated: root.guard(() => Viewer.openIndex(Viewer.images.count - 1))
  }

  Shortcut {
    sequences: ["+", "=", StandardKey.ZoomIn]
    enabled: !root.modalOpen
    onActivated: root.setZoom(root.zoom * 1.25, stage.width / 2, stage.height / 2)
  }

  Shortcut {
    sequences: ["-", StandardKey.ZoomOut]
    enabled: !root.modalOpen
    onActivated: root.setZoom(root.zoom / 1.25, stage.width / 2, stage.height / 2)
  }

  Shortcut {
    sequence: "0"
    enabled: !root.modalOpen
    onActivated: root.fit()
  }

  Shortcut {
    sequence: "1"
    enabled: !root.modalOpen
    onActivated: root.actualSize()
  }

  Shortcut {
    sequence: "R"
    enabled: root.hasImage && !root.cropMode && !root.modalOpen
    onActivated: Viewer.rotate(90)
  }

  Shortcut {
    sequences: ["Shift+R", "L"]
    enabled: root.hasImage && !root.cropMode && !root.modalOpen
    onActivated: Viewer.rotate(-90)
  }

  Shortcut {
    sequence: "C"
    enabled: root.hasImage && !root.cropMode && !root.modalOpen
    onActivated: root.startCrop()
  }

  Shortcut {
    sequences: ["Return", "Enter"]
    enabled: root.cropMode
    onActivated: root.applyCrop()
  }

  Shortcut {
    sequence: "S"
    enabled: root.hasImage && !root.cropMode && !root.modalOpen
    onActivated: scaleDialog.open()
  }

  Shortcut {
    sequence: StandardKey.Save
    enabled: Viewer.modified && !root.cropMode && !root.modalOpen
    onActivated: Viewer.save()
  }

  Shortcut {
    sequence: "Ctrl+Shift+S"
    enabled: root.hasImage && !root.cropMode && !root.modalOpen
    onActivated: saveDialog.open()
  }

  Shortcut {
    sequence: StandardKey.Open
    enabled: !root.modalOpen
    onActivated: root.guard(() => openDialog.open())
  }

  Shortcut {
    sequence: "Ctrl+Z"
    enabled: Viewer.modified && !root.cropMode && !root.modalOpen
    onActivated: Viewer.revert()
  }

  Shortcut {
    sequence: StandardKey.Delete
    enabled: !root.cropMode && !root.modalOpen
    onActivated: root.askDelete()
  }

  Shortcut {
    sequence: "F2"
    enabled: !root.cropMode && !root.modalOpen
    onActivated: root.askRename()
  }

  Shortcut {
    sequence: "I"
    enabled: !root.modalOpen
    onActivated: root.showInfo()
  }

  Shortcut {
    sequences: ["F", "F11"]
    enabled: !root.modalOpen
    onActivated: root.toggleFullScreen()
  }

  Shortcut {
    sequences: ["Escape", "Q"]
    enabled: !root.modalOpen
    onActivated: {
      if (root.cropMode)
        root.cropMode = false
      else
        root.close()
    }
  }
}
