#include "providers.h"
#include "decoder.h"
#include "viewer.h"

#include <QCache>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>

namespace {

QMutex s_cacheMutex;
QCache<QString, QImage> s_cache(256 * 1024);

QString pathFromId(const QString &id)
{
  const QByteArray encoded = id.section(QLatin1Char('/'), 0, 0).toLatin1();
  return QString::fromUtf8(QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

}

DocumentProvider::DocumentProvider(Viewer *viewer)
  : QQuickImageProvider(QQuickImageProvider::Image)
  , m_viewer(viewer)
{
}

QImage DocumentProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
  Q_UNUSED(id);
  Q_UNUSED(requestedSize);
  const QImage image = m_viewer->currentImage();
  if (size)
    *size = image.size();
  return image;
}

ThumbnailJob::ThumbnailJob(const QString &key, const QString &path, const QSize &size, std::shared_ptr<std::atomic_bool> cancelled)
  : m_key(key)
  , m_path(path)
  , m_size(size)
  , m_cancelled(std::move(cancelled))
{
}

void ThumbnailJob::run()
{
  if (m_cancelled->load()) {
    emit done(QImage());
    return;
  }
  QImage image;
  {
    QMutexLocker lock(&s_cacheMutex);
    if (const QImage *cached = s_cache.object(m_key))
      image = *cached;
  }
  if (image.isNull()) {
    image = decodeThumbnail(m_path, m_size);
    if (!image.isNull()) {
      QMutexLocker lock(&s_cacheMutex);
      s_cache.insert(m_key, new QImage(image), qMax<qsizetype>(1, image.sizeInBytes() / 1024));
    }
  }
  emit done(image);
}

ThumbnailResponse::ThumbnailResponse(const QString &id, const QSize &requestedSize, QThreadPool *pool)
  : m_cancelled(std::make_shared<std::atomic_bool>(false))
{
  const QSize size = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(256, 256);
  const QString key = id + QLatin1Char('@') + QString::number(size.width()) + QLatin1Char('x') + QString::number(size.height());
  auto *job = new ThumbnailJob(key, pathFromId(id), size, m_cancelled);
  connect(job, &ThumbnailJob::done, this, [this](const QImage &image) {
    m_image = image;
    emit finished();
  });
  pool->start(job);
}

QQuickTextureFactory *ThumbnailResponse::textureFactory() const
{
  return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void ThumbnailResponse::cancel()
{
  m_cancelled->store(true);
}

ThumbnailProvider::ThumbnailProvider()
{
  m_pool.setMaxThreadCount(qMax(2, QThread::idealThreadCount() - 2));
}

ThumbnailProvider::~ThumbnailProvider()
{
  m_pool.clear();
  m_pool.waitForDone();
}

QQuickImageResponse *ThumbnailProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
  return new ThumbnailResponse(id, requestedSize, &m_pool);
}
