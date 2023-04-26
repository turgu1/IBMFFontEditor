#include "drawingSpace.h"

#include <array>
#include <iostream>

#include <QPainter>

DrawingSpace::DrawingSpace(IBMFFontModPtr font, int faceIdx, QWidget *parent)
    : QWidget{parent}, font_(font), faceIdx_(faceIdx) {
  this->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

void DrawingSpace::setBypassGlyph(IBMFDefs::GlyphCode glyphCode, IBMFDefs::BitmapPtr bitmap,
                                  IBMFDefs::GlyphInfoPtr    glyphInfo,
                                  IBMFDefs::GlyphLigKernPtr glyphLigKern) {
  if (bitmap.get() != nullptr) {
    bypassGlyphCode_    = glyphCode;
    bypassBitmap_       = bitmap;
    bypassGlyphInfo_    = glyphInfo;
    bypassGlyphLigKern_ = glyphLigKern;
  } else {
    bypassGlyphCode_ = NO_GLYPH_CODE;
  }
  update();
}

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

auto DrawingSpace::computeOpticalKerning(const IBMFDefs::BitmapPtr b1, const IBMFDefs::BitmapPtr b2,
                                         const IBMFDefs::GlyphInfoPtr i1,
                                         const IBMFDefs::GlyphInfoPtr i2) const -> FIX16 {
  int kerning = 0;

#if CONVEX_HULL
  FIX16 result1 = 0;

  int normal_distance =
      i1->horizontalOffset + ((i1->advance + 32) >> 6) - i1->bitmapWidth - i2->horizontalOffset;

  // std::cout << "Normal distance:" << normal_distance << std::endl;

  int8_t origin = MAX(i1->verticalOffset, i2->verticalOffset);

  // start positions in each dist arrays
  int8_t distIdxLeft  = origin - i1->verticalOffset;
  int8_t distIdxRight = origin - i2->verticalOffset;

  // idx and length in each bitmaps to compare
  int8_t firstIdxLeft  = origin - i2->verticalOffset;
  int8_t firstIdxRight = origin - i1->verticalOffset;
  int8_t length        = MIN((i1->bitmapHeight - firstIdxLeft), (i2->bitmapHeight - firstIdxRight));
  int8_t firstIdx      = MAX(firstIdxLeft, firstIdxRight);

  if (length > 0) { // Length <= 0 means that there is no alignment between the characters
    // hight of significant parts of dist arrays
    int8_t hight = origin + MAX((i1->bitmapHeight - i1->verticalOffset),
                                (i2->bitmapHeight - i2->verticalOffset));

    // distance computation for left and right characters
    auto distLeft  = std::shared_ptr<int8_t[]>(new int8_t[hight]);
    auto distRight = std::shared_ptr<int8_t[]>(new int8_t[hight]);

    memset(distLeft.get(), -1, hight);
    memset(distRight.get(), -1, hight);

    // distLeft is receiving the right distance in pixels of the first black pixel on each
    // line of the character
    int idx = 0;
    for (uint8_t i = distIdxLeft; i < i1->bitmapHeight + distIdxLeft; i++, idx += i1->bitmapWidth) {
      distLeft[i] = 0;
      for (int col = i1->bitmapWidth - 1; col >= 0; col--) {
        if (b1->pixels[idx + col]) break;
        distLeft[i] += 1;
      }
    }

    // distRight is receiving the left distance in pixels of the first black pixel on each
    // line of the character
    idx = 0;
    for (uint8_t i = distIdxRight; i < i2->bitmapHeight + distIdxRight;
         i++, idx += i2->bitmapWidth) {
      distRight[i] = 0;
      for (int col = 0; col < i2->bitmapWidth; col++) {
        if (b2->pixels[idx + col]) break;
        distRight[i] += 1;
      }
    }

    // find convex corner locations and adjust distances

    // Right Side Convex Hull for the character at left
    if (i1->bitmapHeight >= 3) { // 1 and 2 line characters don't need adjustment

      // Compute the cross product of 3 points. If negative, the angle is convex
      auto crossLeft = [distLeft](int i, int j, int k) -> int {
        return (distLeft[j] - distLeft[i]) * (k - i) - (j - i) * (distLeft[k] - distLeft[i]);
      };

      // Adjusts distances to get a line between two vertices of the Convex Hull
      auto adjustLeft = [distLeft](int i, int j) {
        if ((j - i) > 1) {
          if ((distLeft[j] - distLeft[i]) == 0) {
            for (int k = i + 1; k < j; k++) { distLeft[k] = distLeft[i]; }
          } else {
            float slope = float(distLeft[j] - distLeft[i]) / float(j - i);
            float v     = distLeft[i];
            for (int k = i + 1; k < j; k++) {
              v += slope;
              distLeft[k] = v + 0.5;
            }
          }
        }
      };

      // Find vertices using the cross product and adjust the distances
      // to get the right portion of the Convex Hull polygon.
      int i = distIdxLeft;
      int j = i + 1;
      while (j < i1->bitmapHeight + distIdxLeft) {
        bool found = true;
        for (int k = j + 1; k < i1->bitmapHeight + distIdxLeft; k++) {
          int val = crossLeft(i, j, k);
          if (val >= 0) {
            found = false;
            break;
          }
        }
        if (found) {
          adjustLeft(i, j);
          i = j;
          j = i + 1;
        } else {
          j += 1;
        }
      }
    }

    // Left side Convex Hull for the character at right

    if (i2->bitmapHeight >= 3) { // 1 and 2 line characters don't need adjustment

      // Compute the cross product of 3 points. If negative, the angle is convex.
      auto crossRight = [distRight](int i, int j, int k) -> int {
        return (distRight[j] - distRight[i]) * (k - i) - (j - i) * (distRight[k] - distRight[i]);
      };

      // Adjusts distances to get a line between two vertices of the Convex Hull
      auto adjustRight = [distRight](int i, int j) {
        if ((j - i) > 1) {
          if ((distRight[j] - distRight[i]) == 0) {
            for (int k = i + 1; k < j; k++) { distRight[k] = distRight[i]; }
          } else {
            float slope = float(distRight[j] - distRight[i]) / float(j - i);
            float v     = distRight[i];
            for (int k = i + 1; k < j; k++) {
              v += slope;
              distRight[k] = v + 0.5;
            }
          }
        }
      };

      // Find vertices using the cross product and adjust the distances
      // to get the left portion of the Convex Hull polygon.
      int i = distIdxRight;
      int j = i + 1;
      while (j < i2->bitmapHeight + distIdxRight) {
        bool found = true;
        for (int k = j + 1; k < i2->bitmapHeight + distIdxRight; k++) {
          int val = crossRight(i, j, k);
          if (val >= 0) {
            found = false;
            break;
          }
        }
        if (found) {
          adjustRight(i, j);
          i = j;
          j = i + 1;
        } else {
          j += 1;
        }
      }
    }

#if 0
    std::cout << "Computed left-right convex hulls (origin:" << +origin << "firstIdx:" << +firstIdx
              << ", length:" << +length << ", hight:" << +hight << "):" << std::endl;
    for (int i = 0; i < hight; i++) {
      std::cout << "  ";
      if (distLeft[i] != -1) {
        std::cout << +distLeft[i];
      } else {
        std::cout << " ";
      }

      std::cout << ", ";

      if (distRight[i] != -1) {
        std::cout << +distRight[i];
      } else {
        std::cout << " ";
      }
      std::cout << std::endl;
    }
#endif

    // Now, compute the smallest distance that exists between
    // the two characters. Pixels on each line are checked as well
    // as angled pixels (on the lines above and below)
    kerning = 999;
    for (int i = firstIdx; i < firstIdx + length; i++) {
      int dist = distLeft[i] + distRight[i];
      if (dist < kerning) kerning = dist;
      if ((i > 0) && (distLeft[i - 1] != -1)) {
        int dist = distLeft[i - 1] + distRight[i];
        if (dist < kerning) kerning = dist;
      }
      if ((i < (hight - 1)) && (distLeft[i + 1] != -1)) {
        int dist = distLeft[i + 1] + distRight[i];
        if (dist < kerning) kerning = dist;
      }
    }
    if ((firstIdx > 0) && (distRight[firstIdx - 1] != -1)) {
      int dist = distLeft[firstIdx] + distRight[firstIdx - 1];
      if (dist < kerning) kerning = dist;
    }
    int lastIdx = firstIdx + length - 1;
    if ((lastIdx < (hight - 1)) && (distRight[lastIdx + 1] != -1)) {
      int dist = distLeft[lastIdx] + distRight[lastIdx + 1];
      if (dist < kerning) kerning = dist;
    }

    // Adjust the resulting kerning value, considering the targetted KERNING_SIZE (the space to have
    // between characters), the size of the character and the normal distance that will be used by
    // the writing algorithm
    kerning = (-MIN(kerning - KERNING_SIZE, i2->bitmapWidth)) - normal_distance;

    //} else {
    //  std::cout << "No common lines between glyphs." << std::endl;
  }

  // std::cout << "Kerning value: " << kerning << std::endl;
  result1 = kerning << 6;

#else
  kerning        = 0;
  int buffWidth  = (font_->getFaceHeader(faceIdx_)->emSize >> 6) * 2;
  int buffHeight = font_->getFaceHeader(faceIdx_)->lineHeight * 2;

  uint8_t *buffer = new uint8_t[buffWidth * buffHeight];

  memset(buffer, 0, buffWidth * buffHeight);

  QPoint pos = QPoint(5, (buffHeight / 3) * 2); //  This is the origin

  int voff = i1->verticalOffset;
  int hoff = i1->horizontalOffset;

  int idx = 0;
  for (int row = 0; row < b1->dim.height; row++) {
    for (int col = 0; col < b1->dim.width; col++, idx++) {
      if (b1->pixels[idx] != 0) {
        int buffIdx     = ((pos.y() - voff + row) * buffWidth) + (pos.x() - hoff + col);
        buffer[buffIdx] = 1;
      }
    }
  }

  int advance = ((i1->advance + 32) >> 6);
  if (advance == 0) advance = i1->bitmapWidth + 1;

  pos.setX(pos.x() + advance);

  int max = advance; // + hoff;
  while (max > 0) {
    int voff = i2->verticalOffset;
    int hoff = i2->horizontalOffset;

    for (int col = 0; col < b2->dim.width; col++) {
      for (int row = 0; row < b2->dim.height; row++) {
        if (b2->pixels[row * b2->dim.width + col] != 0) {
          int buffIdx = ((pos.y() - voff + row) * buffWidth) + (pos.x() - hoff + col);
          if (buffer[buffIdx] == 1) {
            buffer[buffIdx] = 2;
            goto end;
          } else if (buffer[buffIdx - buffWidth] == 1) {
            buffer[buffIdx - buffWidth] = 2;
            goto end;
          } else if (buffer[buffIdx + buffWidth] == 1) {
            buffer[buffIdx + buffWidth] = 2;
            goto end;
          } else {
            buffer[buffIdx] = 3;
          }
        }
      }
    }

    pos.setX(pos.x() - 1);
    kerning -= 1;
    max -= 1;
  }

end:
  //

  delete[] buffer;
  FIX16 result2 = (max >= 0) ? ((kerning + KERNING_SIZE + 1) << 6) : 0;
#endif

  // std::cout << "Convex Hull: " << (result1 / 64) << " Other: " << (result2 / 64) << std::endl;

  return result1;
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

void DrawingSpace::setOpticalKerning(bool value) {
  opticalKerning_ = value;
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

auto DrawingSpace::computeSize() -> void {
  if ((font_ != nullptr) && (faceIdx_ < font_->getPreamble().faceCount) && (faceIdx_ >= 0)) {
    drawScreen(nullptr);
    requiredSize_ = QSize(width(), (pos_.y() + font_->getLineHeight(faceIdx_)) * pixelSize_);
    adjustSize();
    update();
  }
}

QSize DrawingSpace::sizeHint() const {
  //  std::cout << "drawingSpace size: " << requiredSize_.width() << ", " <<
  //  requiredSize_.height()
  //            << " -- Widget size: " << this->width() << ", " << this->height() << std::endl;
  return requiredSize_;
}

void DrawingSpace::paintWord(QPainter *painter, int lineHeight) {

  QRect rect;

  if (((pos_.x() + wordLength_) * pixelSize_ + 20) > this->width()) {
    if ((wordLength_ * pixelSize_ + 20) < this->width()) {
      pos_.setY(pos_.y() + lineHeight);
      pos_.setX(0);
    }
  }

  // std::cout << "paintWord()" << std::endl;

  bool firstChar = true;
  for (auto &ch : word_) {

    // std::cout << "Position:" << pos_.x();
    // std::cout << " CH kern:" << ch.kern / 64 << " adv:" << ch.glyphInfo->advance / 64
    //           << " hoff:" << +ch.glyphInfo->horizontalOffset << std::endl;

    int diff = ((ch.kern + (ch.kern < 0 ? -32 : 32)) / 64);
    pos_.setX(pos_.x() + diff);

    int voff = ch.glyphInfo->verticalOffset;

    // The first character of a word must not use the horizontalOffset.
    // Also, there is a need to remove the effect on the advance param (hoff2),
    // see at the end of the loop.
    int hoff  = firstChar ? 0 : ch.glyphInfo->horizontalOffset;
    int hoff2 = firstChar ? -ch.glyphInfo->horizontalOffset : 0;

    int advance = (ch.glyphInfo->advance + 32) >> 6;
    if (advance == 0) advance = ch.glyphInfo->bitmapWidth + 1;

    if (((pos_.x() + advance) * pixelSize_ + 20) > this->width()) {
      pos_.setY(pos_.y() + lineHeight);
      pos_.setX(0);
    }

    // std::cout << "Drawing at pos:" << pos_.x() - hoff << std::endl;

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

    pos_.setX(pos_.x() + advance - hoff2);
    firstChar = false;
  }

  // Adjustment for the last character of a word. This is to get equal spaces between characters.

  auto &ch = word_.at(word_.size() - 1);
  pos_.setX(pos_.x() - (((ch.glyphInfo->advance + 32) >> 6) + ch.glyphInfo->horizontalOffset -
                        ch.bitmap->dim.width));

  // Prepare for next word.
  word_.clear();
  wordLength_ = 0;
}

void DrawingSpace::drawScreen(QPainter *painter) {

  // std::cout << "drawScreen()..." << std::endl;

  if ((font_ == nullptr) || textToDraw_.isEmpty()) return;

  word_.clear();
  wordLength_ = 0;

  if (painter != nullptr) {
    painter->setPen(QPen(QBrush(QColorConstants::DarkGray), 1));
    painter->setBrush(QBrush(QColorConstants::DarkGray));
  }

  int  lineHeight = font_->getLineHeight(faceIdx_);
  bool first      = true;
  pos_            = QPoint(0, lineHeight);

  bool startOfLine = true; // True if we are at the beginning of a line,
                           // to not print the spaces there

#if 0

  IBMFDefs::GlyphCode    g1, g2;
  IBMFDefs::BitmapPtr    b1, b2;
  IBMFDefs::GlyphInfoPtr i1, i2;

  bool  kernPairPresent = false;
  FIX16 kern            = 0;

  g1 = NO_GLYPH_CODE;

  auto iter = textToDraw_.begin();
  while (true) {
    if (g1 != NO_GLYPH_CODE) {
      if ((!kernPairPresent) && opticalKerning_ && (g2 != NO_GLYPH_CODE)) {
        kern = computeOpticalKerning(b1, b2, i1, i2);
      }

      FIX16 advance = i1->advance;
      if (advance == 0) advance = (i1->bitmapWidth + 1) << 6;

      wordLength_ += ((advance + kern + 32) >> 6); // - glyphInfo->horizontalOffset;
      word_.push_back(OneGlyph({.bitmap = b1, .glyphInfo = i1, .kern = kern}));
      startOfLine = false;
      g1          = g2;
      i1          = i2;
      b1          = b2;
    }
    if (iter != textToDraw_.end()) {
      QChar ch = *iter++;
      if (ch == '\n') {
        if (word_.size() > 0) { paintWord(painter, lineHeight); }
        pos_.setY(pos_.y() + lineHeight);
        pos_.setX(0);
        startOfLine = true;
      } else if (ch == ' ') {
        if (word_.size() > 0) { paintWord(painter, lineHeight); }
        if (!startOfLine) { pos_.setX(pos_.x() + font_->getFaceHeader(faceIdx_)->spaceSize); }
      } else {
        g2 = font_->translate(ch.unicode());
        if (normalKerning_) {
          while (font_->ligKern(faceIdx_, g1, &g2, &kern, &kernPairPresent)) {
            // Got a ligature
            g1 = g2;
            if (iter == textToDraw_.end()) break;
            ch = *iter++;
            if ((ch == '\n') || (ch == ' ')) {
              iter--;
              break;
            }
          }
        }

        if (!font_->getGlyph(faceIdx_, g2, i2, b2)) { g2 = NO_GLYPH_CODE; }
      }
    } else {
      if (g1 == NO_GLYPH_CODE) break;
    }
  }

#else
  IBMFDefs::BitmapPtr b1, b2;
  IBMFDefs::GlyphInfoPtr i1, i2;
  IBMFDefs::GlyphCode g1, g2;
  IBMFDefs::GlyphLigKernPtr k1, k2;

  for (auto &ch : textToDraw_) {
    IBMFDefs::BitmapPtr bitmap;
    IBMFDefs::GlyphInfoPtr glyphInfo;
    IBMFDefs::GlyphCode glyphCode;
    IBMFDefs::GlyphLigKernPtr ligKerns;

    if (ch == '\n') {
      if (word_.size() > 0) { paintWord(painter, lineHeight); }
      pos_.setY(pos_.y() + lineHeight);
      pos_.setX(0);
      startOfLine = true;
      first = true;
      continue;
    } else if (ch == ' ') {
      if (word_.size() > 0) {
        paintWord(painter, lineHeight);
        first = true;
      }
      if (!startOfLine) { pos_.setX(pos_.x() + font_->getFaceHeader(faceIdx_)->spaceSize); }
      continue;
    } else {
      glyphCode = font_->translate(ch.unicode());
      // bypassXXX is a glyph currently being edited on screen. If present, it will be
      // taken instead of the same glyphCode present in the font that was still not
      // updated
      if ((bypassGlyphCode_ != IBMFDefs::NO_GLYPH_CODE) && (glyphCode == bypassGlyphCode_)) {
        bitmap = bypassBitmap_;
        glyphInfo = bypassGlyphInfo_;
        ligKerns = bypassGlyphLigKern_;
      } else {
        if (!font_->getGlyph(faceIdx_, glyphCode, glyphInfo, bitmap, ligKerns)) { continue; }
      }
    }

    //    if ((pos.y() * pixelSize_) > (height() - 5)) {
    //      this->resize(this->width(), this->height() + 100);
    //    }

    FIX16 kerning = 0;
    bool kernPairPresent;

    if (opticalKerning_ || normalKerning_) {
      if (!first) {
        b1 = b2;
        i1 = i2;
        g1 = g2;
        k1 = k2;
        b2 = bitmap;
        i2 = glyphInfo;
        g2 = glyphCode;
        k2 = ligKerns;

        if (normalKerning_) {
          IBMFDefs::GlyphCode code = g2;
          FIX16 kern;
          while (font_->ligKern(faceIdx_, g1, &code, &kern, &kernPairPresent, k1))
            ;
          if (code != g2) {
            word_.pop_back();
            glyphCode = g2 = code;
            if ((bypassGlyphCode_ != IBMFDefs::NO_GLYPH_CODE) && (glyphCode == bypassGlyphCode_)) {
              bitmap = bypassBitmap_;
              glyphInfo = bypassGlyphInfo_;
              ligKerns = bypassGlyphLigKern_;
            } else {
              if (!font_->getGlyph(faceIdx_, glyphCode, glyphInfo, bitmap, ligKerns)) { continue; }
            }
            b2 = bitmap;
            i2 = glyphInfo;
            k2 = ligKerns;
          }
          kerning = float(kern);
        }

        if ((!kernPairPresent) && opticalKerning_) {
          kerning = computeOpticalKerning(b1, b2, i1, i2);
        }

        // std::cout << kerning << " " << std::endl;
      } else {
        first = false;
        b2 = bitmap;
        i2 = glyphInfo;
        g2 = glyphCode;
        k2 = ligKerns;
      }
    }

    FIX16 advance = glyphInfo->advance;
    if (advance == 0) advance = (glyphInfo->bitmapWidth + 1) << 6;

    wordLength_ += ((advance + kerning + 32) >> 6); // - glyphInfo->horizontalOffset;
    word_.push_back(OneGlyph({.bitmap = bitmap, .glyphInfo = glyphInfo, .kern = kerning}));
    startOfLine = false;
  }

  if (word_.size() != 0) paintWord(painter, lineHeight);
#endif
}

void DrawingSpace::paintEvent(QPaintEvent *event) {
  if (font_ == nullptr) return;
  QPainter painter(this);
  drawScreen(&painter);
}
