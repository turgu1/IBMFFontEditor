#pragma once

#include <QWidget>

#include "../IBMFDriver/IBMFFontMod.hpp"
#include "kernEntry.h"

const constexpr int PIXEL_SIZE = 4;

class KerningRenderer : public QWidget {
  Q_OBJECT
public:
  explicit KerningRenderer(QWidget *parent, IBMFFontMod *font, int faceIdx, KernEntry *kernEntry);
  void  setScreenPixel(QPoint pos, QPainter &painter);
  void  paintEvent(QPaintEvent *event);
  int   putGlyph(IBMFDefs::GlyphCode code, IBMFDefs::Pos atPos);
  QSize sizeHint() const;

signals:

private:
  IBMFFontMod            *font_;
  IBMFDefs::FaceHeaderPtr faceHeader_;
  IBMFDefs::Bitmap        _glyphsBitmap;
  KernEntry              *_kernEntry;
  QPoint                  _bitmapOffsetPos;
  int                     _glyphsWidth;
  int                     _glyphsHeight;
};
