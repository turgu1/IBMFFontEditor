#pragma once

#include <QWidget>

#include "../IBMFDriver/IBMFFontMod.hpp"
#include "kernEntry.h"

const constexpr int PIXEL_SIZE = 4;

class KerningRenderer : public QWidget {
  Q_OBJECT
public:
  explicit KerningRenderer(QWidget *parent, IBMFFontModPtr font, int faceIdx, KernEntry *kernEntry);
  void  setScreenPixel(QPoint pos, QPainter &painter);
  void  paintEvent(QPaintEvent *event);
  int   putGlyph(IBMFDefs::GlyphCode code, IBMFDefs::Pos atPos);
  QSize sizeHint() const;

signals:

private:
  IBMFFontModPtr          font_;
  IBMFDefs::FaceHeaderPtr faceHeader_;
  IBMFDefs::Bitmap        glyphsBitmap_;
  KernEntry              *kernEntry_;
  QPoint                  bitmapOffsetPos_;
  int                     glyphsWidth_;
  int                     glyphsHeight_;
};
