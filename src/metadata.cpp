#include "metadata.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>

#include <cmath>

#ifdef EDVIEW_HAVE_EXIV2
#include <exiv2/exiv2.hpp>
#endif

namespace {

QString formatDate(const QDateTime &date)
{
  return date.toString(QStringLiteral("d MMMM yyyy   HH:mm:ss"));
}

#ifdef EDVIEW_HAVE_EXIV2
QString exifValue(const Exiv2::ExifData &exif, const char *key)
{
  const auto it = exif.findKey(Exiv2::ExifKey(key));
  if (it == exif.end() || it->count() == 0)
    return {};
  return QString::fromStdString(it->toString()).remove(QChar(0)).trimmed();
}

double exifNumber(const Exiv2::ExifData &exif, const char *key)
{
  const auto it = exif.findKey(Exiv2::ExifKey(key));
  if (it == exif.end() || it->count() == 0)
    return 0;
  const double value = it->toFloat(0);
  return std::isfinite(value) ? value : 0;
}

bool gpsValue(const Exiv2::ExifData &exif, const char *key, const char *refKey, double *out)
{
  const auto it = exif.findKey(Exiv2::ExifKey(key));
  if (it == exif.end() || it->count() < 3)
    return false;
  double value = it->toFloat(0) + it->toFloat(1) / 60.0 + it->toFloat(2) / 3600.0;
  if (!std::isfinite(value))
    return false;
  const QString ref = exifValue(exif, refKey);
  if (ref.startsWith(QLatin1Char('S')) || ref.startsWith(QLatin1Char('W')))
    value = -value;
  *out = value;
  return true;
}

void readExif(const QString &path, QVariantMap &info)
{
  try {
    Exiv2::LogMsg::setLevel(Exiv2::LogMsg::mute);
    auto image = Exiv2::ImageFactory::open(QFile::encodeName(path).toStdString());
    image->readMetadata();
    const Exiv2::ExifData &exif = image->exifData();
    if (exif.empty())
      return;

    for (const char *key : {"Exif.Photo.DateTimeOriginal", "Exif.Photo.DateTimeDigitized", "Exif.Image.DateTime"}) {
      const QString value = exifValue(exif, key);
      const QDateTime date = QDateTime::fromString(value.left(19), QStringLiteral("yyyy:MM:dd HH:mm:ss"));
      if (date.isValid()) {
        info.insert(QStringLiteral("dateTaken"), formatDate(date));
        break;
      }
    }

    const QString make = exifValue(exif, "Exif.Image.Make");
    const QString model = exifValue(exif, "Exif.Image.Model");
    QString camera = model;
    if (!make.isEmpty() && !model.startsWith(make.section(QLatin1Char(' '), 0, 0), Qt::CaseInsensitive))
      camera = (make + QLatin1Char(' ') + model).trimmed();
    if (!camera.isEmpty())
      info.insert(QStringLiteral("camera"), camera);

    const QString lens = exifValue(exif, "Exif.Photo.LensModel");
    if (!lens.isEmpty())
      info.insert(QStringLiteral("lens"), lens);

    QStringList exposure;
    const double aperture = exifNumber(exif, "Exif.Photo.FNumber");
    const double shutter = exifNumber(exif, "Exif.Photo.ExposureTime");
    const QString iso = exifValue(exif, "Exif.Photo.ISOSpeedRatings");
    const double focal = exifNumber(exif, "Exif.Photo.FocalLength");
    if (aperture > 0)
      exposure << QStringLiteral("f/") + QString::number(aperture, 'g', 3);
    if (shutter > 0 && shutter < 1)
      exposure << QStringLiteral("1/%1 s").arg(qRound(1.0 / shutter));
    else if (shutter >= 1)
      exposure << QString::number(shutter, 'g', 3) + QStringLiteral(" s");
    if (!iso.isEmpty())
      exposure << QStringLiteral("ISO ") + iso.section(QLatin1Char(' '), 0, 0);
    if (focal > 0)
      exposure << QString::number(focal, 'g', 4) + QStringLiteral(" mm");
    if (!exposure.isEmpty())
      info.insert(QStringLiteral("exposure"), exposure.join(QStringLiteral("   ")));

    double latitude = 0;
    double longitude = 0;
    if (gpsValue(exif, "Exif.GPSInfo.GPSLatitude", "Exif.GPSInfo.GPSLatitudeRef", &latitude)
        && gpsValue(exif, "Exif.GPSInfo.GPSLongitude", "Exif.GPSInfo.GPSLongitudeRef", &longitude)
        && (latitude != 0 || longitude != 0)) {
      info.insert(QStringLiteral("hasLocation"), true);
      info.insert(QStringLiteral("latitude"), latitude);
      info.insert(QStringLiteral("longitude"), longitude);
      QString location = QStringLiteral("%1° %2   %3° %4")
        .arg(QString::number(std::abs(latitude), 'f', 6), latitude < 0 ? QStringLiteral("S") : QStringLiteral("N"))
        .arg(QString::number(std::abs(longitude), 'f', 6), longitude < 0 ? QStringLiteral("W") : QStringLiteral("E"));
      const auto altitude = exif.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSAltitude"));
      if (altitude != exif.end() && altitude->count() > 0 && std::isfinite(altitude->toFloat(0))) {
        double meters = altitude->toFloat(0);
        if (exifValue(exif, "Exif.GPSInfo.GPSAltitudeRef") == QLatin1String("1"))
          meters = -meters;
        location += QStringLiteral("   %1 m").arg(qRound(meters));
      }
      info.insert(QStringLiteral("location"), location);
    }
  } catch (const std::exception &) {
  }
}
#endif

}

QVariantMap readImageInfo(const QString &path)
{
  QVariantMap info;
  const QFileInfo file(path);
  if (!file.isFile())
    return info;
  info.insert(QStringLiteral("fileName"), file.fileName());
  info.insert(QStringLiteral("folder"), file.absolutePath());
  info.insert(QStringLiteral("fileSize"), QLocale::c().formattedDataSize(file.size(), 1, QLocale::DataSizeTraditionalFormat));
  const QString type = QMimeDatabase().mimeTypeForFile(path).comment();
  info.insert(QStringLiteral("type"), type.isEmpty() ? file.suffix().toUpper() : type + QStringLiteral(" (") + file.suffix().toUpper() + QLatin1Char(')'));
  info.insert(QStringLiteral("modified"), formatDate(file.lastModified()));
#ifdef EDVIEW_HAVE_EXIV2
  readExif(path, info);
#endif
  return info;
}
