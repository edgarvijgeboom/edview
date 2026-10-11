#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

#include <memory>
#include <mutex>

class GpuJpeg
{
public:
  static GpuJpeg &instance();
  static GpuJpeg *ready();

  bool available() const;
  QString deviceName() const;
  QImage decode(const QByteArray &data);

private:
  GpuJpeg();
  ~GpuJpeg();

  struct State;
  std::unique_ptr<State> d;
  std::mutex m_mutex;
  bool m_available = false;
  QString m_deviceName;
};
