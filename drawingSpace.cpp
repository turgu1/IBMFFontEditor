#include "drawingSpace.h"
#include <QPainter>

DrawingSpace::DrawingSpace(IBMFFontModPtr font, int faceIdx, QWidget *parent)
    : QWidget{parent}, font_(font), faceIdx_(faceIdx), autoKerning_(false), normalKerning_(true),
      pixelSize_(1) {}

void DrawingSpace::setText(QString text) {
  textToDraw_ = text;
  repaint();
}

int DrawingSpace::computeAutoKerning(IBMFDefs::Bitmap &b1, IBMFDefs::Bitmap &b2,
                                     IBMFDefs::GlyphInfo &i1, IBMFDefs::GlyphInfo &i2) {

  int kerning = 0;

  int buffWidth   = (i1.bitmapWidth + i2.bitmapWidth + i1.advance) * 2;
  int buffHeight  = ((i1.bitmapHeight > i2.bitmapHeight) ? i1.bitmapHeight : i2.bitmapHeight) * 3;
  uint8_t *buffer = new uint8_t[buffWidth * buffHeight];

  memset(buffer, 0, buffWidth * buffHeight);

  QPoint pos = QPoint(10, buffHeight / 2);

  int voff = i1.verticalOffset;
  int hoff = i1.horizontalOffset;

  int idx = 0;
  for (int row = 0; row < b1.dim.height; row++) {
    for (int col = 0; col < b1.dim.width; col++, idx++) {
      if (b1.pixels[idx] != 0) {
        int buffIdx     = ((pos.y() - voff + row) * buffWidth) + (pos.x() - hoff + col);
        buffer[buffIdx] = 1;
      }
    }
  }
  pos.setX(pos.x() + (i1.advance >> 6));

  while (true) {
    int voff = i2.verticalOffset;
    int hoff = i2.horizontalOffset;

    int idx = 0;
    for (int row = 0; row < b2.dim.height; row++) {
      for (int col = 0; col < b2.dim.width; col++, idx++) {
        if (b2.pixels[idx] != 0) {
          int buffIdx = ((pos.y() - voff + row) * buffWidth) + (pos.x() - hoff + col);
          if (buffer[buffIdx] == 1) goto end;
        }
      }
    }
    pos.setX(pos.x() - 1);
    kerning -= 1;
  }
end:

  kerning += KERNING_SIZE + 1;

  delete[] buffer;
  return kerning;
}

void DrawingSpace::setAutoKerning(bool value) {
  autoKerning_ = value;
  if (autoKerning_) { normalKerning_ = false; }
  repaint();
}

void DrawingSpace::setNormalKerning(bool value) {
  normalKerning_ = value;
  if (normalKerning_) { autoKerning_ = false; }
  repaint();
}

void DrawingSpace::setPixelSize(int value) {
  pixelSize_ = value;
  repaint();
}

void DrawingSpace::paintEvent(QPaintEvent *event) {
  QPainter painter(this);

  painter.setPen(QPen(QBrush(QColorConstants::Black), 1));
  painter.setBrush(QBrush(QColorConstants::Black));

  int    lineHeight = font_->getLineHeight(faceIdx_);
  QPoint pos        = QPoint(0, lineHeight);

  bool first = true;

  IBMFDefs::BitmapPtr    b1, b2;
  IBMFDefs::GlyphInfoPtr i1, i2;

  QRect rect;

  for (auto &ch : textToDraw_) {
    IBMFDefs::BitmapPtr    bitmap;
    IBMFDefs::GlyphInfoPtr glyphInfo;

    font_->getGlyph(faceIdx_, font_->translate(ch.unicode()), glyphInfo, &bitmap);
    if (((pos.x() + (glyphInfo->advance >> 6)) * pixelSize_) + 20 > this->width()) {
      pos.setY(pos.y() + lineHeight);
      pos.setX(0);
    }

    if (autoKerning_) {
      int kerning;
      if (!first) {
        b1      = b2;
        i1      = i2;
        b2      = bitmap;
        i2      = glyphInfo;
        kerning = computeAutoKerning(*b1, *b2, *i1, *i2);
        // std::cout << kerning << " " << std::endl;
      } else {
        first   = false;
        kerning = 0;
        b2      = bitmap;
        i2      = glyphInfo;
      }
      pos.setX(pos.x() + kerning);
    }

    if ((pos.y() * pixelSize_) > (height() - 5)) break;
    int voff = glyphInfo->verticalOffset;
    int hoff = glyphInfo->horizontalOffset;

    int idx = 0;
    for (int row = 0; row < bitmap->dim.height; row++) {
      for (int col = 0; col < bitmap->dim.width; col++, idx++) {
        if (bitmap->pixels[idx] != 0) {
          rect = QRect(10 + (pos.x() - hoff + col) * pixelSize_,
                       (pos.y() - voff + row) * pixelSize_, pixelSize_, pixelSize_);

          painter.drawRect(rect);
        }
      }
    }
    pos.setX(pos.x() + (glyphInfo->advance >> 6));
  }
}

void DrawingSpace::resizeEvent(QResizeEvent *event) {
  repaint();
}
