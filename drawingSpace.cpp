#include "drawingSpace.h"

#include <QPainter>

DrawingSpace::DrawingSpace(IBMFFontModPtr font, int faceIdx, QWidget *parent)
    : QWidget{parent}, font_(font), faceIdx_(faceIdx), autoKerning_(false), normalKerning_(false),
      pixelSize_(1), wordLength_(0), firstLineToDraw_(0), currentLineNbr_(0), lineCount_(0),
      linesPerPage_(0), drawingStarted_(false), resizing_(true) {}

void DrawingSpace::setFont(IBMFFontModPtr font) {
  font_ = font;
}

void DrawingSpace::setFaceIdx(int faceIdx) {
  faceIdx_ = faceIdx;
  if (font_ != nullptr) repaint();
}

void DrawingSpace::setText(QString text) {
  textToDraw_ = text;
  repaint();
}

void DrawingSpace::resizeEvent(QResizeEvent *event) {
  resizing_ = true;
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

    for (int col = 0; col < b2.dim.width; col++) {
      for (int row = 0; row < b2.dim.height; row++) {
        if (b2.pixels[row * b2.dim.width + col] != 0) {
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
  resizing_    = true;
  repaint();
}

void DrawingSpace::setNormalKerning(bool value) {
  normalKerning_ = value;
  resizing_      = true;
  repaint();
}

void DrawingSpace::setPixelSize(int value) {
  pixelSize_ = value;
  resizing_  = true;
  repaint();
}

void DrawingSpace::paintWord(QPainter *painter, QPoint &pos, int lineHeight) {

  QRect rect;

  if (((pos.x() + wordLength_) * pixelSize_ + 20) > this->width()) {
    if ((wordLength_ * pixelSize_ + 20) < this->width()) {
      if (drawingStarted_) pos.setY(pos.y() + lineHeight);
      pos.setX(0);
      incrementLineNbr();
      if ((painter != nullptr) && ((pos.y() * pixelSize_) > (this->height() - 5))) return;
    }
  }

  for (auto &ch : word_) {

    pos.setX(pos.x() + ch.kern);

    int voff = ch.glyphInfo->verticalOffset;
    int hoff = ch.glyphInfo->horizontalOffset;

    if (((pos.x() - hoff + (ch.glyphInfo->advance >> 6)) * pixelSize_ + 20) > this->width()) {
      if (drawingStarted_) pos.setY(pos.y() + lineHeight);
      pos.setX(0);
      incrementLineNbr();
      if ((painter != nullptr) && ((pos.y() * pixelSize_) > (this->height() - 5))) return;
    }

    int idx = 0;

    if (pixelSize_ == 1) {
      if ((painter != nullptr) && drawingStarted_) {
        for (int row = 0; row < ch.bitmap->dim.height; row++) {
          for (int col = 0; col < ch.bitmap->dim.width; col++, idx++) {
            if (ch.bitmap->pixels[idx] != 0) {
              painter->drawPoint(QPoint(10 + (pos.x() - hoff + col), pos.y() - voff + row));
            }
          }
        }
      }
    } else {
      if ((painter != nullptr) && drawingStarted_) {
        for (int row = 0; row < ch.bitmap->dim.height; row++) {
          for (int col = 0; col < ch.bitmap->dim.width; col++, idx++) {
            if (ch.bitmap->pixels[idx] != 0) {
              rect = QRect(10 + (pos.x() - hoff + col) * pixelSize_,
                           (pos.y() - voff + row) * pixelSize_, pixelSize_, pixelSize_);

              painter->drawRect(rect);
            }
          }
        }
      }
    }
    pos.setX(pos.x() + (ch.glyphInfo->advance >> 6));
  }
  word_.clear();
  wordLength_ = 0;
}

void DrawingSpace::drawScreen(QPainter *painter) {

  // std::cout << "drawScreen()..." << std::endl;

  if (font_ == nullptr) return;

  word_.clear();
  wordLength_     = 0;
  currentLineNbr_ = 0;
  drawingStarted_ = ((painter == nullptr) || (currentLineNbr_ >= firstLineToDraw_));

  if (painter != nullptr) {
    painter->setPen(QPen(QBrush(QColorConstants::DarkGray), 1));
    painter->setBrush(QBrush(QColorConstants::DarkGray));
  }

  int    lineHeight = font_->getLineHeight(faceIdx_);
  QPoint pos        = QPoint(0, lineHeight);

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
      if (wordLength_ > 0) { paintWord(painter, pos, lineHeight); }
      if (drawingStarted_) pos.setY(pos.y() + lineHeight);
      pos.setX(0);
      incrementLineNbr();
      startOfLine = true;
      first       = true;
      continue;
    } else if (ch == ' ') {
      if (word_.size() > 0) {
        paintWord(painter, pos, lineHeight);
        first = true;
      }
      if (!startOfLine) { pos.setX(pos.x() + font_->getFaceHeader(faceIdx_)->spaceSize); }
      continue;
    } else {
      if (!font_->getGlyph(faceIdx_, glyphCode = font_->translate(ch.unicode()), glyphInfo,
                           &bitmap)) {
        continue;
      }
    }

    if ((painter != nullptr) && ((pos.y() * pixelSize_) > (height() - 5))) break;

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
            if (!font_->getGlyph(faceIdx_, glyphCode, glyphInfo, &bitmap)) { continue; }
            b2 = bitmap;
            i2 = glyphInfo;
          }
          kerning = (kern + 32) >> 6;
        }

        if ((kerning == 0) && autoKerning_) { kerning = computeAutoKerning(*b1, *b2, *i1, *i2); }

        // std::cout << kerning << " " << std::endl;
      } else {
        first   = false;
        kerning = 0;
        b2      = bitmap;
        i2      = glyphInfo;
        g2      = glyphCode;
      }
    }

    word_.push_back(OneGlyph({.bitmap = bitmap, .glyphInfo = glyphInfo, .kern = kerning}));
    wordLength_ += (glyphInfo->advance >> 6) + kerning;
    startOfLine = false;
  }

  if (word_.size() != 0) paintWord(painter, pos, lineHeight);
}

void DrawingSpace::incrementLineNbr() {
  currentLineNbr_++;
  drawingStarted_ = drawingStarted_ || (currentLineNbr_ >= firstLineToDraw_);
}

int DrawingSpace::getLineCount() {
  return lineCount_;
}

int DrawingSpace::getLinesPerPage() {
  return linesPerPage_;
}

void DrawingSpace::setFirstLineToDraw(int value) {
  firstLineToDraw_ = value;
  repaint();
}

void DrawingSpace::paintEvent(QPaintEvent *event) {
  if (font_ == nullptr) return;
  QPainter painter(this);
  if (resizing_) {
    drawScreen(nullptr);
    resizing_     = false;
    lineCount_    = currentLineNbr_ + 1;
    linesPerPage_ = this->height() / (font_->getFaceHeader(faceIdx_)->lineHeight * pixelSize_);
    if (lineCount_ <= firstLineToDraw_) {
      firstLineToDraw_ = lineCount_ - linesPerPage_;
      if (firstLineToDraw_ < 0) firstLineToDraw_ = 0;
    }
  }
  drawScreen(&painter);
}
