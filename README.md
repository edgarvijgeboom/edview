# EdView

**Version 0.1**

EdView is a fast, minimal image viewer for Linux. It opens an image fullscreen on a translucent, blurred glass background and keeps the interface out of the way: a slim top bar with a few editing tools, navigation arrows on the sides, and a thumbnail gallery that slides up from the bottom of the screen.

It is written in C++ with Qt 6 Quick and uses the GPU for both drawing and JPEG decoding.

![EdView icon](icons/edview.svg)

## Features

- **Fullscreen viewing** on a translucent background with KDE Plasma's blur behind the window.
- **Top bar** showing the file name, the position in the folder (for example `3 / 42`), how the image was decoded and how long it took, the image size in pixels, and the current zoom level. It hides automatically after a few seconds of inactivity in fullscreen.
- **Navigation** with the left and right arrow buttons, by clicking the left or right third of the image, with the arrow keys, or with the mouse wheel on the gallery.
- **Gallery** of thumbnails for all images in the same folder. Move the mouse to the bottom edge of the screen to show it. Thumbnails grow 30% on hover; click one to open it.
- **Zoom and pan**: the mouse wheel zooms around the cursor, dragging pans, and double-clicking toggles between fit-to-screen and 100%.
- **Editing**: rotate left and right, crop (with handles and a rule-of-thirds grid), scale (with a locked aspect ratio and presets), and revert.
- **Save and Save As**: Save overwrites the original safely (it writes to a temporary file first). Save As converts to JPEG, PNG, WebP, AVIF, JPEG XL, HEIF, TIFF, BMP or QOI.
- **Right-click menu** with Info, Delete and Rename.
  - **Info** shows the date and time taken, image type, image size, file size, location (GPS, with a "Show on map" button that opens OpenStreetMap), camera, lens and exposure settings.
  - **Delete** asks for confirmation and moves the file to the Trash.
  - **Rename** keeps the file extension if you leave it out.
- **Unsaved changes** are never lost silently: switching images or closing the app with edits asks whether to save, discard or cancel.

## Supported formats

EdView opens every format that Qt and the KDE image plugins can read, including JPEG, PNG, WebP, GIF, BMP, TIFF, AVIF, HEIF/HEIC, JPEG XL, SVG, ICO, TGA, QOI, PSD, XCF, EXR, JPEG 2000, PNM and many camera RAW formats.

## Performance

- **GPU rendering**: the image, zooming and the interface are drawn on the GPU through Qt's scene graph (OpenGL on NVIDIA).
- **GPU JPEG decoding**: JPEGs are decoded on NVIDIA GPUs with nvJPEG (CUDA). Other formats, and the first image right after startup while CUDA initializes, use Qt's optimized CPU decoders (libjpeg-turbo for JPEG).
- **Prefetching**: the next, previous and second-next images are decoded in the background, so moving through a folder is instant.
- **Caching**: up to 1 GB of decoded images and 256 MB of thumbnails are kept in memory.
- **Fast thumbnails**: thumbnails are decoded at reduced size on a separate thread pool, so the gallery never blocks the viewer.

## Keyboard and mouse

| Action | Keys / mouse |
| --- | --- |
| Next image | Right, Space, Page Down, click the right third |
| Previous image | Left, Backspace, Page Up, click the left third |
| First / last image | Home / End |
| Zoom in / out | `+` / `-`, mouse wheel |
| Fit to screen / 100% | `0` / `1`, double-click |
| Rotate right / left | `R` / `Shift+R` |
| Crop | `C`, then Enter or double-click inside the frame to apply |
| Scale | `S` |
| Revert changes | `Ctrl+Z` |
| Save / Save As | `Ctrl+S` / `Ctrl+Shift+S` |
| Open | `Ctrl+O` |
| Info | `I`, or right-click → Info |
| Rename | `F2`, or right-click → Rename |
| Delete (to Trash) | `Delete`, or right-click → Delete |
| Toggle fullscreen | `F`, `F11` |
| Cancel crop / close | `Esc` |

## Requirements

- Linux with Qt 6.5 or newer (developed on Ubuntu 25.10 with Qt 6.9 and KDE Plasma 6 on Wayland)
- CMake 3.21 or newer, a C++20 compiler and Ninja

Optional features are enabled automatically when their libraries are found:

| Feature | Needs |
| --- | --- |
| GPU JPEG decoding | NVIDIA GPU with the CUDA toolkit (nvJPEG), looked up in `/usr/local/cuda` |
| Background blur | KDE Frameworks 6 WindowSystem (`libkf6windowsystem-dev`) |
| Camera, date and GPS details | Exiv2 (`libexiv2-dev`) |
| Extra image formats | `kimageformat6-plugins`, `qt6-image-formats-plugins` |

Install the build dependencies on Ubuntu:

```bash
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-declarative-dev qt6-base-private-dev libkf6windowsystem-dev libexiv2-dev kimageformat6-plugins qt6-image-formats-plugins
```

## Building

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

The configure step reports which optional features are enabled:

```
-- edview: nvJPEG GPU decoding enabled (CUDA 13.0.88)
-- edview: KDE background blur enabled
-- edview: Exiv2 image details enabled
```

Each feature can be switched off with `-DEDVIEW_WITH_NVJPEG=OFF`, `-DEDVIEW_WITH_KWINDOWSYSTEM=OFF` or `-DEDVIEW_WITH_EXIV2=OFF`. nvJPEG is linked statically by default; use `-DEDVIEW_CUDA_STATIC=OFF` to link the shared CUDA libraries instead.

## Running

```bash
./build/edview /path/to/image.jpg
```

Without a file, EdView shows an open dialog. On startup it prints which GPU it renders on and whether GPU JPEG decoding is active.

### Benchmark

Compare CPU and GPU JPEG decoding times (median of 5 runs):

```bash
./build/edview --benchmark photo1.jpg photo2.jpg
```

### Disable GPU decoding

```bash
EDVIEW_NO_GPU=1 ./build/edview photo.jpg
```

## Installing

### Debian package

Build a `.deb` package (Ubuntu, Debian and derivatives) from an existing build:

```bash
cmake --build build
cd build && cpack
```

This creates `build/edview_0.1.0_amd64.deb`. Install it with:

```bash
sudo apt install ./build/edview_0.1.0_amd64.deb
```

`apt` pulls in the required Qt, QML, KDE and Exiv2 packages automatically and recommends the extra image format plugins. nvJPEG and the CUDA runtime are linked statically into the binary, so the target machine only needs the NVIDIA driver for GPU JPEG decoding; without it, EdView decodes on the CPU. Uninstall with `sudo apt remove edview`.

The package depends on the library versions of the system it was built on, so build it on the oldest Ubuntu release you want to support.

### From the build directory

System-wide:

```bash
sudo cmake --install build
```

This installs the binary, `edview.desktop` and the icon. For a single user without sudo, copy `icons/edview.svg` to `~/.local/share/icons/hicolor/scalable/apps/` and `edview.desktop` to `~/.local/share/applications/`, with `Exec=` pointing to the built binary, then run `kbuildsycoca6`.

## Customizing the glass background

The background is set in the `background` section of `qml/Main.qml`: a grey tint (`Qt.rgba(0.128, 0.128, 0.141, 0.45)`, where the last number is its opacity) with a black darkening layer on top (`opacity: 0.7`). Higher values make the glass darker.

The blur strength is a KDE system setting (1 to 15):

```bash
kwriteconfig6 --file kwinrc --group Effect-blur --key BlurStrength 6
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.reconfigureEffect blur
```

## Project layout

```
CMakeLists.txt      build configuration
edview.desktop      desktop entry (app menu, file associations)
icons/edview.svg    application icon
src/main.cpp        startup, window setup, blur, benchmark mode
src/viewer.*        current image, editing, saving, delete and rename, prefetching
src/decoder.*       CPU and GPU decoding, color conversion, EXIF orientation
src/gpujpeg.*       nvJPEG (CUDA) JPEG decoder
src/imagelist.*     list of images in the folder, natural sort order
src/providers.*     image providers for the current image and thumbnails
src/metadata.*      EXIF details for the Info window (Exiv2)
qml/Main.qml        main window, top bar, navigation, dialogs, shortcuts
qml/Gallery.qml     thumbnail gallery
qml/CropOverlay.qml crop frame and handles
qml/ScaleDialog.qml scale dialog
qml/InfoDialog.qml  image info window
qml/IconButton.qml  toolbar button
qml/PillButton.qml  rounded text button
```

## Roadmap

- **Phase 2**: video playback.
