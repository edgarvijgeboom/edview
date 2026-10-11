cmake_minimum_required(VERSION 3.21)
project(edview VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(EDVIEW_WITH_NVJPEG "Decode JPEG images on NVIDIA GPUs with nvJPEG" ON)
option(EDVIEW_WITH_KWINDOWSYSTEM "Blur the window background on KDE Plasma" ON)
option(EDVIEW_WITH_EXIV2 "Read camera, date and GPS details with Exiv2" ON)
option(EDVIEW_CUDA_STATIC "Link nvJPEG and the CUDA runtime statically so only the NVIDIA driver is needed" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Concurrent)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(edview
  src/main.cpp
  src/decoder.cpp
  src/decoder.h
  src/gpujpeg.cpp
  src/gpujpeg.h
  src/imagelist.cpp
  src/imagelist.h
  src/metadata.cpp
  src/metadata.h
  src/providers.cpp
  src/providers.h
  src/viewer.cpp
  src/viewer.h
)

qt_add_qml_module(edview
  URI EdView
  VERSION 1.0
  QML_FILES
    qml/Main.qml
    qml/IconButton.qml
    qml/PillButton.qml
    qml/Gallery.qml
    qml/CropOverlay.qml
    qml/ScaleDialog.qml
    qml/InfoDialog.qml
)

qt_add_resources(edview "icons"
  PREFIX "/"
  FILES icons/edview_icon.png
)

target_include_directories(edview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(edview PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Quick
  Qt6::QuickControls2
  Qt6::Concurrent
)

if(EDVIEW_WITH_NVJPEG)
  if(NOT DEFINED CUDAToolkit_ROOT AND EXISTS /usr/local/cuda)
    set(CUDAToolkit_ROOT /usr/local/cuda)
  endif()
  find_package(CUDAToolkit QUIET)
  if(EDVIEW_CUDA_STATIC AND TARGET CUDA::nvjpeg_static AND TARGET CUDA::cudart_static)
    target_link_libraries(edview PRIVATE CUDA::nvjpeg_static CUDA::cudart_static)
    target_compile_definitions(edview PRIVATE EDVIEW_HAVE_NVJPEG)
    message(STATUS "edview: nvJPEG GPU decoding enabled, linked statically (CUDA ${CUDAToolkit_VERSION})")
  elseif(TARGET CUDA::nvjpeg AND TARGET CUDA::cudart)
    target_link_libraries(edview PRIVATE CUDA::nvjpeg CUDA::cudart)
    target_compile_definitions(edview PRIVATE EDVIEW_HAVE_NVJPEG)
    message(STATUS "edview: nvJPEG GPU decoding enabled (CUDA ${CUDAToolkit_VERSION})")
  else()
    message(STATUS "edview: nvJPEG not found, JPEG decoding falls back to the CPU")
  endif()
endif()

if(EDVIEW_WITH_KWINDOWSYSTEM)
  find_package(KF6WindowSystem QUIET)
  if(TARGET KF6::WindowSystem)
    target_link_libraries(edview PRIVATE KF6::WindowSystem)
    target_compile_definitions(edview PRIVATE EDVIEW_HAVE_KWINDOWSYSTEM)
    message(STATUS "edview: KDE background blur enabled")
  else()
    message(STATUS "edview: KF6WindowSystem not found, background blur disabled")
  endif()
endif()

if(EDVIEW_WITH_EXIV2)
  find_package(exiv2 QUIET)
  if(TARGET Exiv2::exiv2lib)
    target_link_libraries(edview PRIVATE Exiv2::exiv2lib)
    target_compile_definitions(edview PRIVATE EDVIEW_HAVE_EXIV2)
    message(STATUS "edview: Exiv2 image details enabled")
  else()
    message(STATUS "edview: Exiv2 not found, image info shows file details only")
  endif()
endif()

set_target_properties(edview PROPERTIES INSTALL_RPATH_USE_LINK_PATH ON)

include(GNUInstallDirs)
install(TARGETS edview RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES edview.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES icons/edview_icon.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps RENAME edview.png)
install(CODE "file(REMOVE \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps/edview.svg\")")

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "edview")
set(CPACK_PACKAGE_VENDOR "Edgar Vijgeboom")
set(CPACK_PACKAGE_CONTACT "Edgar Vijgeboom <edgar@studioparkers.nl>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fast GPU accelerated image viewer")
set(CPACK_PACKAGE_DESCRIPTION "EdView opens images fullscreen on a translucent, blurred glass background, with a thumbnail gallery, crop, rotate, scale and format conversion. JPEGs are decoded on NVIDIA GPUs with nvJPEG when available.")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_STRIP_FILES ON)
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
  OWNER_READ OWNER_WRITE OWNER_EXECUTE
  GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qml6-module-qtqml, qml6-module-qtqml-models, qml6-module-qtqml-workerscript, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-templates, qml6-module-qtquick-dialogs, qml6-module-qtquick-layouts, qml6-module-qtquick-window, qml6-module-qtquick-shapes, qml6-module-qt-labs-folderlistmodel, qml6-module-qt-labs-qmlmodels")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "kimageformat6-plugins, qt6-image-formats-plugins, qt6-svg-plugins")
include(CPack)
