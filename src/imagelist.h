#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class ImageList : public QAbstractListModel
{
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("ImageList is provided by Viewer")
  Q_PROPERTY(int count READ count NOTIFY countChanged)
  Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)

public:
  enum Roles {
    PathRole = Qt::UserRole + 1,
    FileNameRole,
    ThumbUrlRole
  };

  explicit ImageList(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  int count() const;
  int currentIndex() const;
  QString folder() const;
  QString pathAt(int index) const;
  int indexOf(const QString &path) const;

  void load(const QString &filePath);
  void refresh(const QString &path);
  void remove(const QString &path);
  void setCurrentIndex(int index);

  static const QStringList &suffixes();

signals:
  void countChanged();
  void currentIndexChanged();

private:
  struct Entry
  {
    QString path;
    QString fileName;
    qint64 modified = 0;
  };

  QList<Entry> m_entries;
  QHash<QString, int> m_index;
  QString m_folder;
  int m_currentIndex = -1;
};
