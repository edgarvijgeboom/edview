#pragma once

#include <QImage>
#include <QQuickAsyncImageProvider>
#include <QQuickImageProvider>
#include <QRunnable>
#include <QThreadPool>

#include <atomic>
#include <memory>

class Viewer;

class DocumentProvider : public QQuickImageProvider
{
public:
  explicit DocumentProvider(Viewer *viewer);
  QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
  Viewer *m_viewer;
};

class ThumbnailJob : public QObject, public QRunnable
{
  Q_OBJECT

public:
  ThumbnailJob(const QString &key, const QString &path, const QSize &size, std::shared_ptr<std::atomic_bool> cancelled);
  void run() override;

signals:
  void done(const QImage &image);

private:
  QString m_key;
  QString m_path;
  QSize m_size;
  std::shared_ptr<std::atomic_bool> m_cancelled;
};

class ThumbnailResponse : public QQuickImageResponse
{
  Q_OBJECT

public:
  ThumbnailResponse(const QString &id, const QSize &requestedSize, QThreadPool *pool);
  QQuickTextureFactory *textureFactory() const override;
  void cancel() override;

private:
  QImage m_image;
  std::shared_ptr<std::atomic_bool> m_cancelled;
};

class ThumbnailProvider : public QQuickAsyncImageProvider
{
public:
  ThumbnailProvider();
  ~ThumbnailProvider() override;
  QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
  QThreadPool m_pool;
};
