#include "gpujpeg.h"

#include <QtGlobal>

#include <atomic>

#ifdef EDVIEW_HAVE_NVJPEG
#include <cuda_runtime_api.h>
#include <nvjpeg.h>
#endif

namespace {

std::atomic<GpuJpeg *> s_ready{nullptr};

}

struct GpuJpeg::State
{
#ifdef EDVIEW_HAVE_NVJPEG
  nvjpegHandle_t handle = nullptr;
  nvjpegJpegState_t state = nullptr;
  cudaStream_t stream = nullptr;
  unsigned char *device = nullptr;
  size_t deviceSize = 0;
  unsigned char *host = nullptr;
  size_t hostSize = 0;

  bool reserve(size_t size)
  {
    if (size > deviceSize) {
      if (device)
        cudaFree(device);
      device = nullptr;
      deviceSize = 0;
      if (cudaMalloc(reinterpret_cast<void **>(&device), size) != cudaSuccess)
        return false;
      deviceSize = size;
    }
    if (size > hostSize) {
      if (host)
        cudaFreeHost(host);
      host = nullptr;
      hostSize = 0;
      if (cudaMallocHost(reinterpret_cast<void **>(&host), size) != cudaSuccess)
        return false;
      hostSize = size;
    }
    return true;
  }
#endif
};

GpuJpeg &GpuJpeg::instance()
{
  static GpuJpeg *gpu = [] {
    auto *created = new GpuJpeg;
    s_ready.store(created);
    return created;
  }();
  return *gpu;
}

GpuJpeg *GpuJpeg::ready()
{
  return s_ready.load();
}

GpuJpeg::GpuJpeg()
  : d(std::make_unique<State>())
{
  if (qEnvironmentVariableIsSet("EDVIEW_NO_GPU")) {
    qInfo("edview: GPU decoding disabled by EDVIEW_NO_GPU");
    return;
  }
#ifdef EDVIEW_HAVE_NVJPEG
  int count = 0;
  if (cudaGetDeviceCount(&count) != cudaSuccess || count == 0) {
    qInfo("edview: no CUDA device, JPEG decoding uses the CPU");
    return;
  }
  cudaDeviceProp properties{};
  if (cudaGetDeviceProperties(&properties, 0) == cudaSuccess)
    m_deviceName = QString::fromLatin1(properties.name);
  if (cudaStreamCreateWithFlags(&d->stream, cudaStreamNonBlocking) != cudaSuccess)
    return;
  if (nvjpegCreateEx(NVJPEG_BACKEND_DEFAULT, nullptr, nullptr, NVJPEG_FLAGS_DEFAULT, &d->handle) != NVJPEG_STATUS_SUCCESS)
    return;
  if (nvjpegJpegStateCreate(d->handle, &d->state) != NVJPEG_STATUS_SUCCESS)
    return;
  m_available = true;
  qInfo("edview: GPU JPEG decoding via nvJPEG on %s", qPrintable(m_deviceName));
#else
  qInfo("edview: built without nvJPEG, JPEG decoding uses the CPU");
#endif
}

GpuJpeg::~GpuJpeg() = default;

bool GpuJpeg::available() const
{
  return m_available;
}

QString GpuJpeg::deviceName() const
{
  return m_deviceName;
}

QImage GpuJpeg::decode(const QByteArray &data)
{
#ifdef EDVIEW_HAVE_NVJPEG
  if (!m_available || data.isEmpty())
    return {};
  std::lock_guard lock(m_mutex);
  const auto *bytes = reinterpret_cast<const unsigned char *>(data.constData());
  const size_t length = size_t(data.size());
  int components = 0;
  nvjpegChromaSubsampling_t subsampling = NVJPEG_CSS_UNKNOWN;
  int widths[NVJPEG_MAX_COMPONENT] = {};
  int heights[NVJPEG_MAX_COMPONENT] = {};
  if (nvjpegGetImageInfo(d->handle, bytes, length, &components, &subsampling, widths, heights) != NVJPEG_STATUS_SUCCESS)
    return {};
  if (subsampling == NVJPEG_CSS_UNKNOWN || widths[0] <= 0 || heights[0] <= 0)
    return {};
  const int width = widths[0];
  const int height = heights[0];
  const size_t pitch = (size_t(width) * 3 + 3) & ~size_t(3);
  const size_t total = pitch * size_t(height);
  if (!d->reserve(total))
    return {};
  nvjpegImage_t output{};
  output.channel[0] = d->device;
  output.pitch[0] = pitch;
  if (nvjpegDecode(d->handle, d->state, bytes, length, NVJPEG_OUTPUT_RGBI, &output, d->stream) != NVJPEG_STATUS_SUCCESS)
    return {};
  if (cudaMemcpyAsync(d->host, d->device, total, cudaMemcpyDeviceToHost, d->stream) != cudaSuccess)
    return {};
  if (cudaStreamSynchronize(d->stream) != cudaSuccess)
    return {};
  const QImage rgb(d->host, width, height, qsizetype(pitch), QImage::Format_RGB888);
  return rgb.convertToFormat(QImage::Format_RGBX8888);
#else
  Q_UNUSED(data);
  return {};
#endif
}
