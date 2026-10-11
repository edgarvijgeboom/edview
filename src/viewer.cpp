#include "viewer.h"
#include "metadata.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QFutureWatcher>
#include <QImageWriter>
#include <QJSEngine>
#include <QMutexLocker>
#include <QPainter>
#include <QSaveFile>
#include <QTransform>
#include <QtConcurrent>

namespace {

Viewer *s_instance = nullptr;

struct SaveFormat
{
  QString label;
  QByteArray format;
  QString suffix;
  int quality;
  bool alpha;
};

const QList<SaveFormat> &saveFormats()
{
  static const QList<SaveFormat> formats = [] {
    const QList<QByteArray> supported = QImageWriter::supportedImageFormats();
    const QList<SaveFormat> all = {
      {QStringLiteral("JPEG image"), "jpeg", QStringLiteral("jpg"), 92, false},
      {QStringLiteral("PNG image"), "png", QStringLiteral("png"), -1, true},
      {QStringLiteral("WebP image"), "webp", QStringLiteral("webp"), 90, true},
      {QStringLiteral("AVIF image"), "avif", QStringLiteral("avif"), 85, true},
      {QStringLiteral("JPEG XL image"), "jxl", QStringLiteral("jxl"), 90, true},
      {QStringLiteral("HEIF image"), "heif", QStringLiteral("heic"), 90, true},
      {QStringLiteral("TIFF image"), "tiff", QStringLiteral("tif"), -1, true},
      {QStringLiteral("BMP image"), "bmp", QStringLiteral("bmp"), -1, false},
      {QStringLiteral("QOI image"), "qoi", QStringLiteral("qoi"), -1, true}
    };
    QList<SaveFormat> result;
    for (const SaveFormat &format : all) {
      if (supported.contains(format.format))
        result << format;
    }
    return result;
  }();
  return formats;
}

QByteArray canonicalFormat(const QString &suffix)
{
  const QString lower = suffix.toLower();
  if (lower == QLatin1String("jpg") || lower == QLatin1String("jpeg") || lower == QLatin1String("jpe") || lower == QLatin1String("jfif"))
    return "jpeg";
  if (lower == QLatin1String("tif") || lower == QLatin1String("tiff"))
    return "tiff";
  if (lower == QLatin1String("heic") || lower == QLatin1String("heif"))
    return "heif";
  return lower.toLatin1();
}

const SaveFormat *formatForSuffix(const QString &suffix)
{
  if (suffix.isEmpty())
    return nullptr;
  const QByteArray format = canonicalFormat(suffix);
  for (const SaveFormat &candidate : saveFormats()) {
    if (candidate.format == format)
      return &candidate;
  }
  return nullptr;
}

bool writeImageFile(const QImage &source, const QString &path, const SaveFormat &format, QString *error)
{
  QImage image = source;
  if (!format.alpha && image.hasAlphaChannel()) {
    QImage flat(image.size(), QImage::Format_RGB32);
    flat.fill(Qt::white);
    QPainter painter(&flat);
    painter.drawImage(0, 0, image);
    painter.end();
    image = flat;
  }
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    *error = QObject::tr("Cannot write %1: %2").arg(QFileInfo(path).fileName(), file.errorString());
    return false;
  }
  QImageWriter writer(&file, format.format);
  if (format.quality >= 0)
    writer.setQuality(format.quality);
  if (!writer.write(image)) {
    file.cancelWriting();
    *error = QObject::tr("Saving failed: %1").arg(writer.errorString());
    return false;
  }
  if (!file.commit()) {
    *error = QObject::tr("Saving failed: %1").arg(file.errorString());
    return false;
  }
  return true;
}

}

Viewer::Viewer(QObject *parent)
  : QObject(parent)
  , m_images(new ImageList(this))
  , m_cache(1024 * 1024)
{
  s_instance = this;
  m_pool.setMaxThreadCount(3);
}

Viewer::~Viewer()
{
  m_pool.clear();
  m_pool.waitForDone();
  if (s_instance == this)
    s_instance = nullptr;
}

Viewer *Viewer::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
  Q_UNUSED(qmlEngine);
  Q_UNUSED(jsEngine);
  QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
  return s_instance;
}

ImageList *Viewer::images() const
{
  return m_images;
}

QString Viewer::path() const
{
  return m_path;
}

QString Viewer::fileName() const
{
  return QFileInfo(m_path).fileName();
}

QUrl Viewer::folderUrl() const
{
  if (m_path.isEmpty())
    return {};
  return QUrl::fromLocalFile(QFileInfo(m_path).absolutePath());
}

QUrl Viewer::suggestedSaveUrl() const
{
  if (m_path.isEmpty())
    return {};
  const QFileInfo info(m_path);
  return QUrl::fromLocalFile(info.absolutePath() + QLatin1Char('/') + info.completeBaseName());
}

QString Viewer::imageUrl() const
{
  if (m_image.isNull())
    return {};
  return QStringLiteral("image://doc/") + QString::number(m_revision);
}

int Viewer::imageWidth() const
{
  return m_image.width();
}

int Viewer::imageHeight() const
{
  return m_image.height();
}

bool Viewer::modified() const
{
  return m_modified;
}

QString Viewer::decodeInfo() const
{
  return m_decodeInfo;
}

bool Viewer::loading() const
{
  return m_loading;
}

QString Viewer::errorString() const
{
  return m_error;
}

QString Viewer::rendererInfo() const
{
  return m_rendererInfo;
}

QStringList Viewer::openFilters() const
{
  QStringList patterns;
  for (const QString &suffix : ImageList::suffixes())
    patterns << QStringLiteral("*.") + suffix;
  return {tr("Images (%1)").arg(patterns.join(QLatin1Char(' '))), tr("All files (*)")};
}

QStringList Viewer::saveFilters() const
{
  QStringList filters;
  for (const SaveFormat &format : saveFormats()) {
    QString patterns = QStringLiteral("*.") + format.suffix;
    if (format.format == "jpeg")
      patterns += QStringLiteral(" *.jpeg");
    filters << QStringLiteral("%1 (%2)").arg(format.label, patterns);
  }
  return filters;
}

QImage Viewer::currentImage() const
{
  QMutexLocker lock(&m_imageMutex);
  return m_image;
}

void Viewer::setRendererInfo(const QString &info)
{
  if (info == m_rendererInfo)
    return;
  m_rendererInfo = info;
  qInfo("edview: rendering with %s", qPrintable(info));
  emit rendererInfoChanged();
}

void Viewer::open(const QString &location)
{
  QString path = location;
  if (path.startsWith(QLatin1String("file:")))
    path = QUrl(path).toLocalFile();
  if (path.isEmpty())
    return;
  const QFileInfo info(path);
  path = info.absoluteFilePath();
  if (!info.isFile()) {
    setError(tr("File not found: %1").arg(path));
    return;
  }
  if (info.absolutePath() != m_images->folder() || m_images->indexOf(path) < 0)
    m_images->load(path);
  m_images->setCurrentIndex(m_images->indexOf(path));
  m_path = path;
  const quint64 token = ++m_token;
  setError({});
  if (const QImage *cached = m_cache.object(path)) {
    setLoading(false);
    show(*cached, tr("cached"));
    emit documentOpened();
    prefetch();
    return;
  }
  setLoading(true);
  auto *watcher = new QFutureWatcher<DecodeResult>(this);
  connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, token] {
    const DecodeResult result = watcher->result();
    watcher->deleteLater();
    if (token != m_token)
      return;
    setLoading(false);
    if (result.image.isNull()) {
      {
        QMutexLocker lock(&m_imageMutex);
        m_original = QImage();
        m_image = QImage();
      }
      m_modified = false;
      m_decodeInfo.clear();
      ++m_revision;
      setError(tr("Cannot open %1: %2").arg(fileName(), result.error));
      emit imageChanged();
      emit documentOpened();
      prefetch();
      return;
    }
    show(result.image, result.info);
    emit documentOpened();
    prefetch();
  });
  watcher->setFuture(request(path, 10));
}

void Viewer::openIndex(int index)
{
  const QString path = m_images->pathAt(index);
  if (!path.isEmpty())
    open(path);
}

void Viewer::next()
{
  step(1);
}

void Viewer::previous()
{
  step(-1);
}

void Viewer::rotate(int degrees)
{
  if (m_image.isNull() || degrees % 360 == 0)
    return;
  applyEdit(m_image.transformed(QTransform().rotate(degrees)));
}

void Viewer::crop(int x, int y, int width, int height)
{
  const QRect area = QRect(x, y, width, height).intersected(m_image.rect());
  if (area.isEmpty() || area == m_image.rect())
    return;
  applyEdit(m_image.copy(area));
}

void Viewer::resize(int width, int height)
{
  if (m_image.isNull() || width < 1 || height < 1 || QSize(width, height) == m_image.size())
    return;
  applyEdit(m_image.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

void Viewer::revert()
{
  if (!m_modified)
    return;
  {
    QMutexLocker lock(&m_imageMutex);
    m_image = m_original;
  }
  m_modified = false;
  ++m_revision;
  emit imageChanged();
}

bool Viewer::save()
{
  if (m_image.isNull() || m_path.isEmpty())
    return false;
  const SaveFormat *format = formatForSuffix(QFileInfo(m_path).suffix());
  if (!format) {
    emit message(tr("This format cannot be written, use Save As"));
    return false;
  }
  QString error;
  if (!writeImageFile(m_image, m_path, *format, &error)) {
    emit message(error);
    return false;
  }
  finishSave(m_path);
  return true;
}

bool Viewer::saveAs(const QUrl &url, int filterIndex)
{
  if (m_image.isNull() || saveFormats().isEmpty())
    return false;
  QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
  if (path.isEmpty())
    return false;
  const SaveFormat *format = formatForSuffix(QFileInfo(path).suffix());
  if (!format) {
    format = &saveFormats().at(qBound(0, filterIndex, int(saveFormats().size()) - 1));
    path += QLatin1Char('.') + format->suffix;
  }
  QString error;
  if (!writeImageFile(m_image, path, *format, &error)) {
    emit message(error);
    return false;
  }
  finishSave(QFileInfo(path).absoluteFilePath());
  return true;
}

bool Viewer::deleteCurrent()
{
  if (m_path.isEmpty())
    return false;
  const QString path = m_path;
  const QString name = QFileInfo(path).fileName();
  const int index = qMax(0, m_images->indexOf(path));
  if (!QFile::moveToTrash(path)) {
    emit message(tr("Cannot move %1 to the Trash").arg(name));
    return false;
  }
  m_cache.remove(path);
  m_images->remove(path);
  if (m_images->count() > 0)
    openIndex(qMin(index, m_images->count() - 1));
  else
    clear();
  emit message(tr("Moved %1 to the Trash").arg(name));
  return true;
}

bool Viewer::renameCurrent(const QString &name)
{
  QString target = name.trimmed();
  if (m_path.isEmpty() || target.isEmpty())
    return false;
  if (target.contains(QLatin1Char('/')) || target == QLatin1String(".") || target == QLatin1String("..")) {
    emit message(tr("Invalid file name"));
    return false;
  }
  const QFileInfo current(m_path);
  if (!ImageList::suffixes().contains(QFileInfo(target).suffix().toLower()) && !current.suffix().isEmpty())
    target += QLatin1Char('.') + current.suffix();
  const QString newPath = current.absoluteDir().filePath(target);
  if (newPath == m_path)
    return true;
  if (QFileInfo::exists(newPath)) {
    emit message(tr("%1 already exists").arg(target));
    return false;
  }
  if (!QFile::rename(m_path, newPath)) {
    emit message(tr("Cannot rename %1").arg(current.fileName()));
    return false;
  }
  if (QImage *cached = m_cache.take(m_path)) {
    cacheImage(newPath, *cached);
    delete cached;
  }
  m_path = newPath;
  m_images->load(newPath);
  m_images->setCurrentIndex(m_images->indexOf(newPath));
  emit imageChanged();
  emit message(tr("Renamed to %1").arg(target));
  return true;
}

QVariantMap Viewer::imageInfo() const
{
  QVariantMap info = readImageInfo(m_path);
  QSize size = m_original.size();
  if (size.isEmpty())
    size = QImageReader(m_path).size();
  info.insert(QStringLiteral("width"), size.width());
  info.insert(QStringLiteral("height"), size.height());
  if (m_modified) {
    info.insert(QStringLiteral("editedWidth"), m_image.width());
    info.insert(QStringLiteral("editedHeight"), m_image.height());
  }
  return info;
}

void Viewer::clear()
{
  {
    QMutexLocker lock(&m_imageMutex);
    m_original = QImage();
    m_image = QImage();
  }
  ++m_token;
  m_path.clear();
  m_modified = false;
  m_decodeInfo.clear();
  ++m_revision;
  setLoading(false);
  emit imageChanged();
  emit documentOpened();
}

QFuture<DecodeResult> Viewer::request(const QString &path, int priority)
{
  const auto pending = m_pending.constFind(path);
  if (pending != m_pending.constEnd())
    return pending.value();
  QFuture<DecodeResult> future = QtConcurrent::task(&decodeImage)
    .withArguments(path)
    .onThreadPool(m_pool)
    .withPriority(priority)
    .spawn();
  m_pending.insert(path, future);
  auto *watcher = new QFutureWatcher<DecodeResult>(this);
  connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, path] {
    const DecodeResult result = watcher->result();
    watcher->deleteLater();
    m_pending.remove(path);
    if (!result.image.isNull())
      cacheImage(path, result.image);
  });
  watcher->setFuture(future);
  return future;
}

void Viewer::prefetch()
{
  const int index = m_images->currentIndex();
  const int count = m_images->count();
  if (index < 0 || count < 2)
    return;
  for (const int offset : {1, -1, 2}) {
    const QString path = m_images->pathAt((index + offset + count) % count);
    if (path != m_path && !m_cache.contains(path) && !m_pending.contains(path))
      request(path, 0);
  }
}

void Viewer::step(int offset)
{
  const int count = m_images->count();
  if (count == 0)
    return;
  const int index = qMax(0, m_images->currentIndex());
  openIndex((index + offset + count) % count);
}

void Viewer::show(const QImage &image, const QString &info)
{
  {
    QMutexLocker lock(&m_imageMutex);
    m_original = image;
    m_image = image;
  }
  m_modified = false;
  m_decodeInfo = info;
  ++m_revision;
  emit imageChanged();
}

void Viewer::applyEdit(const QImage &image)
{
  if (image.isNull())
    return;
  {
    QMutexLocker lock(&m_imageMutex);
    m_image = image;
  }
  m_modified = true;
  ++m_revision;
  emit imageChanged();
}

void Viewer::finishSave(const QString &path)
{
  {
    QMutexLocker lock(&m_imageMutex);
    m_original = m_image;
  }
  m_path = path;
  m_modified = false;
  cacheImage(path, m_image);
  if (QFileInfo(path).absolutePath() != m_images->folder() || m_images->indexOf(path) < 0)
    m_images->load(path);
  else
    m_images->refresh(path);
  m_images->setCurrentIndex(m_images->indexOf(path));
  ++m_revision;
  emit imageChanged();
  emit message(tr("Saved %1").arg(QFileInfo(path).fileName()));
}

void Viewer::cacheImage(const QString &path, const QImage &image)
{
  m_cache.insert(path, new QImage(image), qMax<qsizetype>(1, image.sizeInBytes() / 1024));
}

void Viewer::setLoading(bool loading)
{
  if (loading == m_loading)
    return;
  m_loading = loading;
  emit loadingChanged();
}

void Viewer::setError(const QString &error)
{
  if (error == m_error)
    return;
  m_error = error;
  emit errorChanged();
}
