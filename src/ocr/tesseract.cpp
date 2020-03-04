#include "tesseract.h"
#include "debug.h"
#include "languagecodes.h"
#include "task.h"

#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>

#if defined(Q_OS_LINUX)
#include <fstream>
static qint64 getFreeMemory()
{
  std::string token;
  std::ifstream file("/proc/meminfo");
  qint64 freeMem = 0;
  while (file >> token) {
    if (token == "MemFree:" || token == "Buffers:" || token == "Cached:") {
      unsigned long mem = 0;
      freeMem += (file >> mem) ? mem : 0;
    }
  }
  return freeMem * 1024;
}
#elif defined(Q_OS_WIN)
#include <windows.h>
#undef min
#undef max
static qint64 getFreeMemory()
{
  MEMORYSTATUSEX statex;
  statex.dwLength = sizeof(statex);
  if (GlobalMemoryStatusEx(&statex)) {
    return statex.ullAvailPhys;
  }
  return -1;
}
#endif

static Pix *convertImage(const QImage &image)
{
  PIX *pix;

  int width = image.width();
  int height = image.height();
  int depth = image.depth();
  int bytesPerLine = image.bytesPerLine();
  int wpl = bytesPerLine / 4;

  pix = pixCreate(width, height, depth);
  pixSetWpl(pix, wpl);
  pixSetColormap(pix, nullptr);
  memmove(pix->data, image.bits(), bytesPerLine * height);

  const qreal toDPM = 1.0 / 0.0254;
  int resolutionX = image.dotsPerMeterX() / toDPM;
  int resolutionY = image.dotsPerMeterY() / toDPM;
  pixSetResolution(pix, resolutionX, resolutionY);
  return pix;
}

static QImage convertImage(Pix &image)
{
  int width = pixGetWidth(&image);
  int height = pixGetHeight(&image);
  int depth = pixGetDepth(&image);
  int bytesPerLine = pixGetWpl(&image) * 4;
  l_uint32 *datas = pixGetData(&image);

  QImage::Format format;
  if (depth == 1) {
    format = QImage::Format_Mono;
  } else if (depth == 8) {
    format = QImage::Format_Indexed8;
  } else {
    format = QImage::Format_RGB32;
  }

  QImage result((uchar *)datas, width, height, bytesPerLine, format);

  // Set resolution
  l_int32 xres, yres;
  pixGetResolution(&image, &xres, &yres);
  const qreal toDPM = 1.0 / 0.0254;
  result.setDotsPerMeterX(xres * toDPM);
  result.setDotsPerMeterY(yres * toDPM);

  // Handle palette
  QVector<QRgb> _bwCT;
  _bwCT.append(qRgb(255, 255, 255));
  _bwCT.append(qRgb(0, 0, 0));

  QVector<QRgb> _grayscaleCT(256);
  for (int i = 0; i < 256; i++) {
    _grayscaleCT.append(qRgb(i, i, i));
  }
  switch (depth) {
    case 1: result.setColorTable(_bwCT); break;
    case 8: result.setColorTable(_grayscaleCT); break;
    default: result.setColorTable(_grayscaleCT);
  }

  if (result.isNull()) {
    static QImage none(0, 0, QImage::Format_Invalid);
    qDebug("Invalid format!!!\n");
    return none;
  }

  return result;
}

static Pix *prepareImage(const QImage &image)
{
  Pix *pix = convertImage(image);
  SOFT_ASSERT(pix != NULL, return nullptr);

  Pix *gray = pixConvertRGBToGray(pix, 0.0, 0.0, 0.0);
  SOFT_ASSERT(gray != NULL, return nullptr);
  pixDestroy(&pix);

  Pix *scaled = gray;
  const auto xRes = pixGetXRes(gray);
  const auto yRes = pixGetYRes(gray);
  if (xRes * yRes != 0) {
    const auto preferredScale = std::max(300.0 / std::min(xRes, yRes), 1.0);
    if (preferredScale > 1.0) {
      const auto MAX_INT16 = 0x7fff;
      float maxScaleX = MAX_INT16 / double(gray->w);
      float scaleX = std::min(float(preferredScale), maxScaleX);
      float maxScaleY = MAX_INT16 / double(gray->h);
      float scaleY = std::min(float(preferredScale), maxScaleY);
      float scale = std::min(scaleX, scaleY);

      qint64 availableMemory = getFreeMemory() * 0.95;
      if (availableMemory > 0) {
        qint32 actualSize = gray->w * gray->h * gray->d / 8;
        float maxScaleMemory = float(availableMemory) / actualSize;
        scale = std::min(scale, maxScaleMemory);
      }
      scaled = pixScale(gray, scale, scale);
      if (scaled == NULL) {
        scaled = gray;
      }
    }
  }
  if (scaled != gray) {
    pixDestroy(&gray);
  }
  return scaled;
}

static void cleanupImage(Pix **image)
{
  pixDestroy(image);
}

Tesseract::Tesseract(const LanguageId &language, const QString &tessdataPath)
{
  SOFT_ASSERT(!tessdataPath.isEmpty(), return );
  SOFT_ASSERT(!language.isEmpty(), return );

  init(language, tessdataPath);
}

Tesseract::~Tesseract() = default;

void Tesseract::init(const LanguageId &language, const QString &tessdataPath)
{
  SOFT_ASSERT(!engine_, return );

  LanguageCodes languages;
  auto langCodes = languages.findById(language);
  if (!langCodes) {
    error_ = QObject::tr("unknown recognition language: %1").arg(language);
    return;
  }

  engine_ = std::make_unique<tesseract::TessBaseAPI>();

  auto result =
      engine_->Init(qPrintable(tessdataPath), qPrintable(langCodes->tesseract),
                    tesseract::OEM_DEFAULT);
  if (result == 0)
    return;

  error_ = QObject::tr("troubles with tessdata");
  engine_.reset();
  return;
}

const QString &Tesseract::error() const
{
  return error_;
}

QString Tesseract::recognize(const QPixmap &source)
{
  SOFT_ASSERT(engine_, return {});
  SOFT_ASSERT(!source.isNull(), return {});

  error_.clear();

  Pix *image = prepareImage(source.toImage());
  SOFT_ASSERT(image != NULL, return {});
  engine_->SetImage(image);
  char *outText = engine_->GetUTF8Text();
  engine_->Clear();
  cleanupImage(&image);

  QString result = QString(outText).trimmed();
  delete[] outText;

  if (result.isEmpty())
    error_ = QObject::tr("Failed to recognize text");
  return result;
}

bool Tesseract::isValid() const
{
  return engine_.get();
}