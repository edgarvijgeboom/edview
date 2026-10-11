#include "decoder.h"
#include "gpujpeg.h"
#include "providers.h"
#include "viewer.h"

#include <QGuiApplication>
#include <QIcon>
#include <QImageReader>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QThreadPool>

#ifdef EDVIEW_HAVE_KWINDOWSYSTEM
#include <KWindowEffects>
#endif

namespace {

QString rendererDescription(QQuickWindow *window)
{
  const QSGRendererInterface::GraphicsApi api = window->rendererInterface()->graphicsApi();
  QString description;
  switch (api) {
  case QSGRendererInterface::OpenGL:
    description = QStringLiteral("OpenGL");
    break;
  case QSGRendererInterface::Vulkan:
    description = QStringLiteral("Vulkan");
    break;
  case QSGRendererInterface::Software:
    description = QStringLiteral("Software");
    break;
  default:
    description = QStringLiteral("RHI");
    break;
  }
  if (api == QSGRendererInterface::OpenGL) {
    if (QOpenGLContext *context = QOpenGLContext::currentContext()) {
      const auto *renderer = reinterpret_cast<const char *>(context->functions()->glGetString(GL_RENDERER));
      if (renderer)
        description += QStringLiteral(" on ") + QString::fromLatin1(renderer);
    }
  }
  return description;
}

}

int main(int argc, char *argv[])
{
  QImageReader::setAllocationLimit(4096);

  if (argc > 2 && qstrcmp(argv[1], "--benchmark") == 0) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    return runBenchmark(app.arguments().mid(2));
  }

  QQuickWindow::setDefaultAlphaBuffer(true);
  QGuiApplication app(argc, argv);
  QGuiApplication::setApplicationName(QStringLiteral("EdView"));
  QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));
  QGuiApplication::setDesktopFileName(QStringLiteral("edview"));
  QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/edview_icon.png")));
  QQuickStyle::setStyle(QStringLiteral("Basic"));

  QThreadPool::globalInstance()->start([] {
    GpuJpeg::instance();
  });

  Viewer viewer(nullptr);
  QQmlApplicationEngine engine;
  engine.addImageProvider(QStringLiteral("doc"), new DocumentProvider(&viewer));
  engine.addImageProvider(QStringLiteral("thumb"), new ThumbnailProvider);
  QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
    QCoreApplication::exit(1);
  }, Qt::QueuedConnection);

  const QStringList arguments = app.arguments().mid(1);
  if (!arguments.isEmpty())
    viewer.open(arguments.constFirst());

  engine.loadFromModule("EdView", "Main");
  if (engine.rootObjects().isEmpty())
    return 1;

  if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())) {
#ifdef EDVIEW_HAVE_KWINDOWSYSTEM
    KWindowEffects::enableBlurBehind(window, true);
#endif
    QObject::connect(window, &QQuickWindow::sceneGraphInitialized, window, [window, &viewer] {
      const QString info = rendererDescription(window);
      QMetaObject::invokeMethod(&viewer, [&viewer, info] {
        viewer.setRendererInfo(info);
      }, Qt::QueuedConnection);
    }, Qt::DirectConnection);
  }

  return app.exec();
}
