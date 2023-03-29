#include "drawingSpace.h"

#include <iostream>

#include <QPainter>

DrawingSpace::DrawingSpace(IBMFFontModPtr font, int faceIdx, QWidget *parent)
    : QWidget{parent}, font_(font), faceIdx_(faceIdx), autoKerning_(false), normalKerning_(false),
      pixelSize_(1), kernFactor_(1.0), wordLength_(0), bypassGlyphCode_(IBMFDefs::NO_GLYPH_CODE),
      bypassGlyphInfo_(nullptr) {
  this->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

void DrawingSpace::setBypassGlyph(IBMFDefs::GlyphCode glyphCode, IBMFDefs::BitmapPtr bitmap,
                                  IBMFDefs::GlyphInfoPtr glyphInfo) {
  if (bitmap.get() != nullptr) {
    bypassGlyphCode_ = glyphCode;
    bypassBitmap_    = bitmap;
    bypassGlyphInfo_ = glyphInfo;
  } else {
    bypassGlyphCode_ = NO_GLYPH_CODE;
  }
  update();
}

auto DrawingSpace::computeAutoKerning(const IBMFDefs::BitmapPtr b1, const IBMFDefs::BitmapPtr b2,
                                      const IBMFDefs::GlyphInfo &i1,
                                      const IBMFDefs::GlyphInfo &i2) const -> int {
  int kerning     = 0;

  int buffWidth   = (font_->getFaceHeader(faceIdx_)->emSize >> 6) * 4;
  int buffHeight  = font_->getFaceHeader(faceIdx_)->lineHeight * 2;

  uint8_t *buffer = new uint8_t[buffWidth * buffHeight];

  memset(buffer, 0, buffWidth * buffHeight);

  QPoint pos = QPoint(10, buffHeight / 2); //  This is the origin

  int voff   = i1.verticalOffset;
  int hoff   = i1.horizontalOffset;

  int idx    = 0;
  for (int row = 0; row < b1->dim.height; row++) {
    for (int col = 0; col < b1->dim.width; col++, idx++) {
      if (b1->pixels[idx] != 0) {
        int buffIdx     = ((pos.y() - voff + row) * buffWidth) + (pos.x() - hoff + col);
        buffer[buffIdx] = 1;
      }
    }
  }
  int advance = (i1.advance >> 6);
  if (advance == 0) advance = i1.bitmapWidth + 1;

  pos.setX(pos.x() + advance);

  int max = advance - 2;
  while (max > 0) {
    int voff = i2.verticalOffset;
    int hoff = i2.horizontalOffset;

    for (int col = 0; col < b2->dim.width; col++) {
      for (int row = 0; row < b2->dim.height; row++) {
        if (b2->pixels[row * b2->dim.width + col] != 0) {
          int buffIdx = ((pos.y() - voff + row) * buffWidth) + (pos.x() - hoff + col);
          if (buffer[buffIdx] == 1) goto end;
        }
      }
    }
    pos.setX(pos.x() - 1);
    kerning -= 1;
    max -= 1;
  }
end:

  delete[] buffer;
  return (max > 0) ? kerning + KERNING_SIZE + 1 : 0;
}

void DrawingSpace::setFont(IBMFFontModPtr font) {
  font_    = font;
  faceIdx_ = 0;
}

void DrawingSpace::setFaceIdx(int faceIdx) {
  if ((font_ != nullptr) && (faceIdx < font_->getPreamble().faceCount) && (faceIdx >= 0)) {
    faceIdx_ = faceIdx;
    computeSize();
  }
}

void DrawingSpace::setText(QString text) {
  textToDraw_ = text;
  computeSize();
}

void DrawingSpace::setAutoKerning(bool value) {
  autoKerning_ = value;
  computeSize();
}

void DrawingSpace::setNormalKerning(bool value) {
  normalKerning_ = value;
  computeSize();
}

void DrawingSpace::setPixelSize(int value) {
  pixelSize_ = value;
  computeSize();
}

void DrawingSpace::setKernFactor(float value) {
  if (value > 0.5) {
    kernFactor_ = value;
    computeSize();
  }
}

auto DrawingSpace::computeSize() -> void {
  if ((font_ != nullptr) && (faceIdx_ < font_->getPreamble().faceCount) && (faceIdx_ >= 0)) {
    drawScreen(nullptr);
    requiredSize_ = QSize(width(), (pos_.y() + font_->getLineHeight(faceIdx_)) * pixelSize_);
    adjustSize();
    update();
  }
}

QSize DrawingSpace::sizeHint() const {
  //  std::cout << "drawingSpace size: " << requiredSize_.width() << ", " << requiredSize_.height()
  //            << " -- Widget size: " << this->width() << ", " << this->height() << std::endl;
  return requiredSize_;
}

void DrawingSpace::paintWord(QPainter *painter, int lineHeight) {

  QRect rect;

  if (((pos_.x() + wordLength_) * pixelSize_ + 20) > this->width()) {
    if ((wordLength_ * pixelSize_ + 20) < this->width()) {
      pos_.setY(pos_.y() + lineHeight);
      pos_.setX(0);
      //      if ((pos.y() * pixelSize_) > (this->height() - 5)) {
      //        this->resize(this->width(), this->height() + 100);
      //      }
    }
  }

  for (auto &ch : word_) {

    pos_.setX(pos_.x() + ch.kern);

    int voff    = ch.glyphInfo->verticalOffset;
    int hoff    = ch.glyphInfo->horizontalOffset;

    int advance = (ch.glyphInfo->advance >> 6);
    if (advance == 0) advance = ch.glyphInfo->bitmapWidth + 1;

    if (((pos_.x() - hoff + advance) * pixelSize_ + 20) > this->width()) {
      pos_.setY(pos_.y() + lineHeight);
      pos_.setX(0);
      //      if ((pos.y() * pixelSize_) > (this->height() - 5)) {
      //        this->resize(this->width(), this->height() + 100);
      //      }
    }

    if (painter != nullptr) {
      int idx = 0;
      if (pixelSize_ == 1) {
        for (int row = 0; row < ch.bitmap->dim.height; row++) {
          for (int col = 0; col < ch.bitmap->dim.width; col++, idx++) {
            if (ch.bitmap->pixels[idx] != 0) {
              painter->drawPoint(QPoint(10 + (pos_.x() - hoff + col), pos_.y() - voff + row));
            }
          }
        }
      } else {
        for (int row = 0; row < ch.bitmap->dim.height; row++) {
          for (int col = 0; col < ch.bitmap->dim.width; col++, idx++) {
            if (ch.bitmap->pixels[idx] != 0) {
              rect = QRect(10 + (pos_.x() - hoff + col) * pixelSize_,
                           (pos_.y() - voff + row) * pixelSize_, pixelSize_, pixelSize_);

              painter->drawRect(rect);
            }
          }
        }
      }
    }

    advance = (ch.glyphInfo->advance >> 6);
    if (advance == 0) advance = ch.glyphInfo->bitmapWidth + 1;
    pos_.setX(pos_.x() + advance);
  }
  word_.clear();
  wordLength_ = 0;
}

void DrawingSpace::drawScreen(QPainter *painter) {

  // std::cout << "drawScreen()..." << std::endl;

  if (font_ == nullptr) return;

  word_.clear();
  wordLength_ = 0;

  if (painter != nullptr) {
    painter->setPen(QPen(QBrush(QColorConstants::DarkGray), 1));
    painter->setBrush(QBrush(QColorConstants::DarkGray));
  }

  int lineHeight   = font_->getLineHeight(faceIdx_);

  pos_             = QPoint(0, lineHeight);

  bool first       = true;
  bool startOfLine = true;

  IBMFDefs::BitmapPtr    b1, b2;
  IBMFDefs::GlyphInfoPtr i1, i2;
  IBMFDefs::GlyphCode    g1, g2;

  for (auto &ch : textToDraw_) {
    IBMFDefs::BitmapPtr    bitmap;
    IBMFDefs::GlyphInfoPtr glyphInfo;
    IBMFDefs::GlyphCode    glyphCode;

    if (ch == '\n') {
      if (wordLength_ > 0) {
        paintWord(painter, lineHeight);
      }
      pos_.setY(pos_.y() + lineHeight);
      pos_.setX(0);
      startOfLine = true;
      first       = true;
      continue;
    } else if (ch == ' ') {
      if (word_.size() > 0) {
        paintWord(painter, lineHeight);
        first = true;
      }
      if (!startOfLine) {
        pos_.setX(pos_.x() + font_->getFaceHeader(faceIdx_)->spaceSize);
      }
      continue;
    } else {
      glyphCode = font_->translate(ch.unicode());
      if ((bypassGlyphCode_ != IBMFDefs::NO_GLYPH_CODE) && (glyphCode == bypassGlyphCode_)) {
        bitmap    = bypassBitmap_;
        glyphInfo = bypassGlyphInfo_;
      } else {
        if (!font_->getGlyph(faceIdx_, glyphCode, glyphInfo, &bitmap)) {
          continue;
        }
      }
    }

    //    if ((pos.y() * pixelSize_) > (height() - 5)) {
    //      this->resize(this->width(), this->height() + 100);
    //    }

    int kerning = 0;

    if (autoKerning_ || normalKerning_) {
      if (!first) {
        b1 = b2;
        i1 = i2;
        g1 = g2;
        b2 = bitmap;
        i2 = glyphInfo;
        g2 = glyphCode;

        if (normalKerning_) {
          IBMFDefs::GlyphCode code = g2;
          FIX16               kern;
          while (font_->ligKern(faceIdx_, g1, &code, &kern))
            ;
          if (code != g2) {
            word_.pop_back();
            glyphCode = g2 = code;
            if ((bypassGlyphCode_ != IBMFDefs::NO_GLYPH_CODE) && (glyphCode == bypassGlyphCode_)) {
              bitmap    = bypassBitmap_;
              glyphInfo = bypassGlyphInfo_;
            } else {
              if (!font_->getGlyph(faceIdx_, glyphCode, glyphInfo, &bitmap)) {
                continue;
              }
            }
            b2 = bitmap;
            i2 = glyphInfo;
          }
          float fkerning = (float(kern + ((kern < 0) ? -32 : 32)) / 64.0) * kernFactor_;
          kerning        = fkerning;
        }

        if ((kerning == 0) && autoKerning_) {
          kerning = computeAutoKerning(b1, b2, *i1, *i2);
        }

        // std::cout << kerning << " " << std::endl;
      } else {
        first   = false;
        kerning = 0;
        b2      = bitmap;
        i2      = glyphInfo;
        g2      = glyphCode;
      }
    }

    int advance = (glyphInfo->advance >> 6);
    if (advance == 0) {
      advance = glyphInfo->bitmapWidth + 1; // Some codePoints have a zero advance...
    }
    word_.push_back(OneGlyph({.bitmap = bitmap, .glyphInfo = glyphInfo, .kern = kerning}));
    wordLength_ += advance + kerning - glyphInfo->horizontalOffset;
    startOfLine = false;
  }

  if (word_.size() != 0) paintWord(painter, lineHeight);
}

void DrawingSpace::paintEvent(QPaintEvent *event) {
  if (font_ == nullptr) return;
  QPainter painter(this);
  drawScreen(&painter);
}
