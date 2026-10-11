#include "decoder.h"
#include "gpujpeg.h"

#include <QBuffer>
#include <QColorSpace>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImageIOHandler>
#include <QImageReader>
#include <QTextStream>
#include <QTransform>

#include <algorithm>

namespace {

bool isJpeg(const QByteArray &data)
{
  return data.size() >= 3 && uchar(data[0]) == 0xFF && uchar(data[1]) == 0xD8 && uchar(data[2]) == 0xFF;
}

QImageIOHandler::Transformations jpegTransformation(const QByteArray &data)
{
  QBuffer buffer;
  buffer.setData(data);
  buffer.open(QIODevice::ReadOnly);
  QImageReader reader(&buffer, "jpeg");
  return reader.transformation();
}

QImage applyTransformation(QImage image, QImageIOHandler::Transformations transformation)
{
  if (transformation == QImageIOHandler::TransformationNone)
    return image;
  if (transformation == QImageIOHandler::TransformationRotate270)
    return image.transformed(QTransform().rotate(270));
  const bool horizontal = transformation.testFlag(QImageIOHandler::TransformationMirror);
  const bool vertical = transformation.testFlag(QImageIOHandler::TransformationFlip);
  if (horizontal || vertical)
    image = image.transformed(QTransform().scale(horizontal ? -1 : 1, vertical ? -1 : 1));
  if (transformation.testFlag(QImageIOHandler::TransformationRotate90))
    image = image.transformed(QTransform().rotate(90));
  return image;
}

QImage decodeJpegOnGpu(GpuJpeg &gpu, const QByteArray &data)
{
  const QImage image = gpu.decode(data);
  if (image.isNull())
    return image;
  return applyTransformation(image, jpegTransformation(data));
}

QImage decodeOnCpu(QImageReader &reader)
{
  reader.setAutoTransform(true);
  return toDisplayFormat(reader.read());
}

QString milliseconds(QList<qint64> samples)
{
  if (samples.isEmpty())
    return QStringLiteral("-");
  std::sort(samples.begin(), samples.end());
  return QString::number(double(samples.at(samples.size() / 2)) / 1e6, 'f', 1);
}

}

QImage toDisplayFormat(QImage image)
{
  if (image.isNull())
    return image;
  if (image.colorSpace().isValid() && image.colorSpace() != QColorSpace(QColorSpace::SRgb))
    image.convertToColorSpace(QColorSpace(QColorSpace::SRgb));
  const QImage::Format format = image.hasAlphaChannel() ? QImage::Format_RGBA8888_Premultiplied : QImage::Format_RGBX8888;
  if (image.format() == format)
    return image;
  return image.convertToFormat(format);
}

DecodeResult decodeImage(const QString &path)
{
  QElapsedTimer timer;
  timer.start();
  DecodeResult result;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    result.error = file.errorString();
    return result;
  }
  GpuJpeg *gpu = GpuJpeg::ready();
  if (gpu && gpu->available() && isJpeg(file.peek(3))) {
    const QByteArray data = file.readAll();
    const QImage image = decodeJpegOnGpu(*gpu, data);
    if (!image.isNull()) {
      result.image = image;
      result.info = QStringLiteral("GPU nvJPEG · %1 ms").arg(timer.elapsed());
      return result;
    }
  }
  file.close();
  QImageReader reader(path);
  const QImage image = decodeOnCpu(reader);
  if (image.isNull()) {
    result.error = reader.errorString();
    return result;
  }
  result.image = image;
  result.info = QStringLiteral("CPU · %1 ms").arg(timer.elapsed());
  return result;
}

QImage decodeThumbnail(const QString &path, const QSize &requestedSize)
{
  QImageReader reader(path);
  reader.setAutoTransform(true);
  const QSize full = reader.size();
  if (full.isValid() && !full.isEmpty() && requestedSize.isValid() && reader.supportsOption(QImageIOHandler::ScaledSize)) {
    const qreal factor = std::max(qreal(requestedSize.width()) / full.width(), qreal(requestedSize.height()) / full.height());
    if (factor < 1.0)
      reader.setScaledSize((QSizeF(full) * factor).toSize().expandedTo(QSize(1, 1)));
  }
  QImage image = reader.read();
  if (image.isNull())
    return image;
  if (requestedSize.isValid() && (image.width() > requestedSize.width() * 2 || image.height() > requestedSize.height() * 2))
    image = image.scaled(requestedSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
  return toDisplayFormat(image);
}

int runBenchmark(const QStringList &files)
{
  QTextStream out(stdout);
  GpuJpeg &gpu = GpuJpeg::instance();
  out << "GPU decoder: " << (gpu.available() ? QStringLiteral("nvJPEG on ") + gpu.deviceName() : QStringLiteral("not available")) << Qt::endl;
  for (const QString &path : files) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      out << path << ": " << file.errorString() << Qt::endl;
      continue;
    }
    const QByteArray data = file.readAll();
    QList<qint64> cpuSamples;
    QList<qint64> gpuSamples;
    QSize size;
    for (int run = 0; run < 6; ++run) {
      QElapsedTimer timer;
      timer.start();
      QBuffer buffer;
      buffer.setData(data);
      buffer.open(QIODevice::ReadOnly);
      QImageReader reader(&buffer);
      const QImage image = decodeOnCpu(reader);
      if (run > 0)
        cpuSamples << timer.nsecsElapsed();
      size = image.size();
    }
    if (gpu.available() && isJpeg(data)) {
      for (int run = 0; run < 6; ++run) {
        QElapsedTimer timer;
        timer.start();
        const QImage image = decodeJpegOnGpu(gpu, data);
        if (image.isNull())
          break;
        if (run > 0)
          gpuSamples << timer.nsecsElapsed();
      }
    }
    out << QFileInfo(path).fileName() << "  " << size.width() << "x" << size.height()
        << "  CPU " << milliseconds(cpuSamples) << " ms"
        << "  GPU " << milliseconds(gpuSamples) << " ms" << Qt::endl;
  }
  return 0;
}
