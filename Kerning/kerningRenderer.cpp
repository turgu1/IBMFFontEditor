#include "kerningRenderer.h"

#include <iostream>

#include <QPainter>

KerningRenderer::KerningRenderer(QWidget *parent, IBMFFontMod *font, int faceIdx,
                                 KernEntry *kernEntry)
    : QWidget(parent), font_(font), _kernEntry(kernEntry) {

  this->setStyleSheet("color: black;"
                      "background-color: lightgray;"
                      "selection-color: yellow;"
                      "selection-background-color: red;");

  faceHeader_          = font->getFaceHeader(faceIdx);
  _glyphsWidth         = ((faceHeader_->emSize + 32) >> 6) * 3 + 10;
  _glyphsHeight        = faceHeader_->lineHeight + 10;
  _glyphsBitmap.pixels = Pixels(_glyphsWidth * _glyphsHeight, 0);
  _glyphsBitmap.dim    = IBMFDefs::Dim(_glyphsWidth, _glyphsHeight);

  setMinimumSize(QSize(_glyphsWidth * PIXEL_SIZE, _glyphsHeight * PIXEL_SIZE));
}

void KerningRenderer::setScreenPixel(QPoint pos, QPainter &painter) {
  QRect rect;

  rect = QRect((pos.x() - _bitmapOffsetPos.x()) * PIXEL_SIZE,
               (pos.y() - _bitmapOffsetPos.y()) * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE);

  // std::cout << "Put pixel at pos[" << rect.x() << ", " << rect.y() << "]" << std::endl;

  painter.drawRect(rect);
}

void KerningRenderer::paintEvent(QPaintEvent *event) {
  QPainter painter(this);

  painter.setPen(QPen(QBrush(QColorConstants::DarkGray), 1));
  painter.setBrush(QBrush(QColorConstants::DarkGray));

  _bitmapOffsetPos.setX(((_glyphsWidth * PIXEL_SIZE) - width()) / 2);
  _bitmapOffsetPos.setY(((_glyphsHeight * PIXEL_SIZE) - height()) / 2 - 2);

  //  std::cout << "Glyphs width: " << _glyphsWidth << ", height: " << _glyphsHeight
  //            << " Widget width: " << width() << ", height: " << height() << ", OffsetPos["
  //            << _bitmapOffsetPos.x() << ", " << _bitmapOffsetPos.y() << "]" << std::endl;

  IBMFDefs::Pos atPos(5, _glyphsHeight - 5 - faceHeader_->descenderHeight);
  memset(_glyphsBitmap.pixels.data(), 0, _glyphsBitmap.pixels.size());

  int advance = putGlyph(_kernEntry->glyphCode, atPos);
  atPos.x += advance + _kernEntry->kern;
  putGlyph(_kernEntry->nextGlyphCode, atPos);

  int idx = 0;
  for (int y = 0; y < _glyphsHeight; y++) {
    for (int x = 0; x < _glyphsWidth; x++, idx++) {
      if (_glyphsBitmap.pixels[idx] != 0) { setScreenPixel(QPoint(x, y), painter); }
    }
  }
}

int KerningRenderer::putGlyph(IBMFDefs::GlyphCode code, IBMFDefs::Pos atPos) {
  IBMFDefs::BitmapPtr    glyphBitmap;
  IBMFDefs::GlyphInfoPtr glyphInfo;
  font_->getGlyph(1, code, glyphInfo, &glyphBitmap);

  if (glyphInfo != nullptr) {
    // std::cout << "Got something!!" << std::endl;
    int outRow = atPos.y - glyphInfo->verticalOffset;
    for (int inRow = 0; inRow < glyphBitmap->dim.height; inRow++, outRow++) {
      int outCol = atPos.x - glyphInfo->horizontalOffset;
      for (int inCol = 0; inCol < glyphBitmap->dim.width; inCol++, outCol++) {
        uint8_t pixel = glyphBitmap->pixels[inRow * glyphBitmap->dim.width + inCol];
        if (pixel) _glyphsBitmap.pixels[outRow * _glyphsBitmap.dim.width + outCol] = pixel;
      }
    }
    return (glyphInfo->advance + 32) >> 6;
  } else {
    // std::cout << "Nothing received from font" << std::endl;
    return 0;
  }
}

QSize KerningRenderer::sizeHint() const {
  return minimumSizeHint();
}
