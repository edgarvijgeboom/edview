#include "imagelist.h"

#include <QCollator>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>

#include <algorithm>

namespace {

QString thumbnailUrl(const QString &path, qint64 modified)
{
  const QByteArray encoded = path.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
  return QStringLiteral("image://thumb/") + QString::fromLatin1(encoded) + QLatin1Char('/') + QString::number(modified);
}

}

ImageList::ImageList(QObject *parent)
  : QAbstractListModel(parent)
{
}

int ImageList::rowCount(const QModelIndex &parent) const
{
  return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant ImageList::data(const QModelIndex &index, int role) const
{
  if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
    return {};
  const Entry &entry = m_entries.at(index.row());
  switch (role) {
  case PathRole:
    return entry.path;
  case Qt::DisplayRole:
  case FileNameRole:
    return entry.fileName;
  case ThumbUrlRole:
    return thumbnailUrl(entry.path, entry.modified);
  default:
    return {};
  }
}

QHash<int, QByteArray> ImageList::roleNames() const
{
  return {
    {PathRole, "path"},
    {FileNameRole, "fileName"},
    {ThumbUrlRole, "thumbUrl"}
  };
}

int ImageList::count() const
{
  return int(m_entries.size());
}

int ImageList::currentIndex() const
{
  return m_currentIndex;
}

QString ImageList::folder() const
{
  return m_folder;
}

QString ImageList::pathAt(int index) const
{
  if (index < 0 || index >= m_entries.size())
    return {};
  return m_entries.at(index).path;
}

int ImageList::indexOf(const QString &path) const
{
  return m_index.value(path, -1);
}

void ImageList::load(const QString &filePath)
{
  const QFileInfo target(filePath);
  const QDir dir = target.absoluteDir();
  QStringList filters;
  for (const QString &suffix : suffixes())
    filters << QStringLiteral("*.") + suffix;
  const QFileInfoList infos = dir.entryInfoList(filters, QDir::Files | QDir::Readable);
  QList<Entry> entries;
  entries.reserve(infos.size() + 1);
  bool hasTarget = false;
  for (const QFileInfo &info : infos) {
    const QString path = info.absoluteFilePath();
    hasTarget = hasTarget || path == target.absoluteFilePath();
    entries.append({path, info.fileName(), info.lastModified().toMSecsSinceEpoch()});
  }
  if (!hasTarget && target.isFile())
    entries.append({target.absoluteFilePath(), target.fileName(), target.lastModified().toMSecsSinceEpoch()});
  QCollator collator;
  collator.setNumericMode(true);
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  std::sort(entries.begin(), entries.end(), [&collator](const Entry &a, const Entry &b) {
    return collator.compare(a.fileName, b.fileName) < 0;
  });
  beginResetModel();
  m_entries = std::move(entries);
  m_index.clear();
  m_index.reserve(m_entries.size());
  for (int i = 0; i < m_entries.size(); ++i)
    m_index.insert(m_entries.at(i).path, i);
  m_folder = dir.absolutePath();
  m_currentIndex = -1;
  endResetModel();
  emit countChanged();
  emit currentIndexChanged();
}

void ImageList::refresh(const QString &path)
{
  const int row = indexOf(path);
  if (row < 0)
    return;
  m_entries[row].modified = QFileInfo(path).lastModified().toMSecsSinceEpoch();
  const QModelIndex changed = index(row);
  emit dataChanged(changed, changed, {ThumbUrlRole});
}

void ImageList::remove(const QString &path)
{
  const int row = indexOf(path);
  if (row < 0)
    return;
  beginRemoveRows({}, row, row);
  m_entries.removeAt(row);
  m_index.clear();
  for (int i = 0; i < m_entries.size(); ++i)
    m_index.insert(m_entries.at(i).path, i);
  endRemoveRows();
  m_currentIndex = -1;
  emit countChanged();
  emit currentIndexChanged();
}

void ImageList::setCurrentIndex(int index)
{
  if (index == m_currentIndex)
    return;
  m_currentIndex = index;
  emit currentIndexChanged();
}

const QStringList &ImageList::suffixes()
{
  static const QStringList list = [] {
    const QStringList excluded = {QStringLiteral("eps"), QStringLiteral("epsf"), QStringLiteral("epsi")};
    QStringList result;
    for (const QByteArray &format : QImageReader::supportedImageFormats()) {
      const QString suffix = QString::fromLatin1(format).toLower();
      if (!excluded.contains(suffix))
        result << suffix;
    }
    result << QStringLiteral("jpe") << QStringLiteral("jfif");
    result.removeDuplicates();
    result.sort();
    return result;
  }();
  return list;
}
