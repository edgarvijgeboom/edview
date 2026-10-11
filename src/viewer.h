#pragma once

#include "decoder.h"
#include "imagelist.h"

#include <QCache>
#include <QFuture>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QThreadPool>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

class Viewer : public QObject
{
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(ImageList *images READ images CONSTANT)
  Q_PROPERTY(QString path READ path NOTIFY imageChanged)
  Q_PROPERTY(QString fileName READ fileName NOTIFY imageChanged)
  Q_PROPERTY(QUrl folderUrl READ folderUrl NOTIFY imageChanged)
  Q_PROPERTY(QUrl suggestedSaveUrl READ suggestedSaveUrl NOTIFY imageChanged)
  Q_PROPERTY(QString imageUrl READ imageUrl NOTIFY imageChanged)
  Q_PROPERTY(int imageWidth READ imageWidth NOTIFY imageChanged)
  Q_PROPERTY(int imageHeight READ imageHeight NOTIFY imageChanged)
  Q_PROPERTY(bool modified READ modified NOTIFY imageChanged)
  Q_PROPERTY(QString decodeInfo READ decodeInfo NOTIFY imageChanged)
  Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
  Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
  Q_PROPERTY(QString rendererInfo READ rendererInfo NOTIFY rendererInfoChanged)
  Q_PROPERTY(QStringList openFilters READ openFilters CONSTANT)
  Q_PROPERTY(QStringList saveFilters READ saveFilters CONSTANT)

public:
  explicit Viewer(QObject *parent);
  ~Viewer() override;

  static Viewer *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

  ImageList *images() const;
  QString path() const;
  QString fileName() const;
  QUrl folderUrl() const;
  QUrl suggestedSaveUrl() const;
  QString imageUrl() const;
  int imageWidth() const;
  int imageHeight() const;
  bool modified() const;
  QString decodeInfo() const;
  bool loading() const;
  QString errorString() const;
  QString rendererInfo() const;
  QStringList openFilters() const;
  QStringList saveFilters() const;
  QImage currentImage() const;

  void setRendererInfo(const QString &info);

  Q_INVOKABLE void open(const QString &location);
  Q_INVOKABLE void openIndex(int index);
  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();
  Q_INVOKABLE void rotate(int degrees);
  Q_INVOKABLE void crop(int x, int y, int width, int height);
  Q_INVOKABLE void resize(int width, int height);
  Q_INVOKABLE void revert();
  Q_INVOKABLE bool save();
  Q_INVOKABLE bool saveAs(const QUrl &url, int filterIndex);
  Q_INVOKABLE bool deleteCurrent();
  Q_INVOKABLE bool renameCurrent(const QString &name);
  Q_INVOKABLE QVariantMap imageInfo() const;

signals:
  void imageChanged();
  void documentOpened();
  void loadingChanged();
  void errorChanged();
  void rendererInfoChanged();
  void message(const QString &text);

private:
  QFuture<DecodeResult> request(const QString &path, int priority);
  void prefetch();
  void step(int offset);
  void show(const QImage &image, const QString &info);
  void clear();
  void applyEdit(const QImage &image);
  void finishSave(const QString &path);
  void cacheImage(const QString &path, const QImage &image);
  void setLoading(bool loading);
  void setError(const QString &error);

  ImageList *m_images;
  QThreadPool m_pool;
  QCache<QString, QImage> m_cache;
  QHash<QString, QFuture<DecodeResult>> m_pending;
  mutable QMutex m_imageMutex;
  QString m_path;
  QImage m_original;
  QImage m_image;
  QString m_decodeInfo;
  QString m_error;
  QString m_rendererInfo;
  quint64 m_revision = 0;
  quint64 m_token = 0;
  bool m_loading = false;
  bool m_modified = false;
};
