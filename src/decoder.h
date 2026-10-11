#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>

struct DecodeResult
{
  QImage image;
  QString info;
  QString error;
};

DecodeResult decodeImage(const QString &path);
QImage decodeThumbnail(const QString &path, const QSize &requestedSize);
QImage toDisplayFormat(QImage image);
int runBenchmark(const QStringList &files);
