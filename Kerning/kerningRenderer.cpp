#include "kerningRenderer.h"

#include <iostream>

#include <QPainter>

KerningRenderer::KerningRenderer(QWidget *parent, IBMFFontModPtr font, int faceIdx,
                                 KernEntry *kernEntry)
    : QWidget(parent), font_(font), faceIdx_(faceIdx), kernEntry_(kernEntry) {

  this->setStyleSheet("color: black;"
                      "background-color: lightgray;"
                      "selection-color: yellow;"
                      "selection-background-color: red;");

  faceHeader_          = font->getFaceHeader(faceIdx);
  glyphsWidth_         = ((faceHeader_->emSize + 32) >> 6) * 3 + 10;
  glyphsHeight_        = faceHeader_->lineHeight + 10;
  glyphsBitmap_.pixels = Pixels(glyphsWidth_ * glyphsHeight_, 0);
  glyphsBitmap_.dim    = IBMFDefs::Dim(glyphsWidth_, glyphsHeight_);

  setMinimumSize(QSize(glyphsWidth_ * PIXEL_SIZE, glyphsHeight_ * PIXEL_SIZE));
}

void KerningRenderer::setScreenPixel(QPoint pos, QPainter &painter) {
  QRect rect;

  rect = QRect((pos.x() - bitmapOffsetPos_.x()) * PIXEL_SIZE,
               (pos.y() - bitmapOffsetPos_.y()) * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE);

  // std::cout << "Put pixel at pos[" << rect.x() << ", " << rect.y() << "]" << std::endl;

  painter.drawRect(rect);
}

void KerningRenderer::paintEvent(QPaintEvent *event) {
  QPainter painter(this);

  painter.setPen(QPen(QBrush(QColorConstants::DarkGray), 1));
  painter.setBrush(QBrush(QColorConstants::DarkGray));

  bitmapOffsetPos_.setX(((glyphsWidth_ * PIXEL_SIZE) - width()) / 2);
  bitmapOffsetPos_.setY(((glyphsHeight_ * PIXEL_SIZE) - height()) / 2 - 2);

  //  std::cout << "Glyphs width: " << glyphsWidth_ << ", height: " << glyphsHeight_
  //            << " Widget width: " << width() << ", height: " << height() << ", OffsetPos["
  //            << bitmapOffsetPos_.x() << ", " << bitmapOffsetPos_.y() << "]" << std::endl;

  IBMFDefs::Pos atPos(5, glyphsHeight_ - 5 - faceHeader_->descenderHeight);
  memset(glyphsBitmap_.pixels.data(), 0, glyphsBitmap_.pixels.size());

  int advance = putGlyph(kernEntry_->glyphCode, atPos);
  atPos.x += advance + kernEntry_->kern;
  putGlyph(kernEntry_->nextGlyphCode, atPos);

  int idx = 0;
  for (int y = 0; y < glyphsHeight_; y++) {
    for (int x = 0; x < glyphsWidth_; x++, idx++) {
      if (glyphsBitmap_.pixels[idx] != 0) {
        setScreenPixel(QPoint(x, y), painter);
      }
    }
  }
}

int KerningRenderer::putGlyph(IBMFDefs::GlyphCode code, IBMFDefs::Pos atPos) {
  IBMFDefs::BitmapPtr    glyphBitmap;
  IBMFDefs::GlyphInfoPtr glyphInfo;
  font_->getGlyph(faceIdx_, code, glyphInfo, &glyphBitmap);

  if (glyphInfo != nullptr) {
    int outRow = atPos.y - glyphInfo->verticalOffset;
    for (int inRow = 0; inRow < glyphBitmap->dim.height; inRow++, outRow++) {
      int outCol = atPos.x - glyphInfo->horizontalOffset;
      for (int inCol = 0; inCol < glyphBitmap->dim.width; inCol++, outCol++) {
        uint8_t pixel = glyphBitmap->pixels[inRow * glyphBitmap->dim.width + inCol];
        if (pixel) glyphsBitmap_.pixels[outRow * glyphsBitmap_.dim.width + outCol] = pixel;
      }
    }
    return (glyphInfo->advance + 32) >> 6;
  } else {
    // std::cout << "Nothing received from font" << std::endl;
    return 0;
  }
}

QSize KerningRenderer::sizeHint() const { return minimumSizeHint(); }
