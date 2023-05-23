#include "drawingSpace.h"

#include <array>
#include <cmath>
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

typedef int32_t FIX32;

#define FRACT_BITS          10
#define FIXED_POINT_ONE     (1 << FRACT_BITS)
#define MAKE_INT_FIXED(x)   (static_cast<FIX32>((x) << FRACT_BITS))
#define MAKE_FLOAT_FIXED(x) (static_cast<FIX32>((x) *FIXED_POINT_ONE))
#define MAKE_FIXED_INT(x)   ((x) >> FRACT_BITS)
#define MAKE_FIXED_FLOAT(x) ((static_cast<float>(x)) / FIXED_POINT_ONE)

#define FIXED_MULT(x, y) ((x) * (y) >> FRACT_BITS)
#define FIXED_DIV(x, y)  (((x) << FRACT_BITS) / (y))

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

auto DrawingSpace::computeOpticalKerning(const IBMFDefs::BitmapPtr b1, const IBMFDefs::BitmapPtr b2,
                                         const IBMFDefs::GlyphInfoPtr i1,
                                         const IBMFDefs::GlyphInfoPtr i2) const -> FIX16 {
  FIX16 result1 = 0;

  int normal_distance =
      i1->horizontalOffset + ((i1->advance + 32) >> 6) - i1->bitmapWidth - i2->horizontalOffset;

  // std::cout << "Normal distance:" << normal_distance << std::endl;

  int8_t origin = MAX(i1->verticalOffset, i2->verticalOffset);

  // start positions in each dist arrays
  uint8_t distIdxLeft  = origin - i1->verticalOffset;
  uint8_t distIdxRight = origin - i2->verticalOffset;

  // idx and length in each bitmaps to compare
  uint8_t firstIdxLeft  = origin - i2->verticalOffset;
  uint8_t firstIdxRight = origin - i1->verticalOffset;
  int8_t  length   = MIN((i1->bitmapHeight - firstIdxLeft), (i2->bitmapHeight - firstIdxRight));
  uint8_t firstIdx = MAX(firstIdxLeft, firstIdxRight);

  FIX32 kerning    = 0;
  // if (length > 0) { // Length <= 0 means that there is no alignment between the characters
  //  hight of significant parts of dist arrays
  int8_t hight = origin + MAX((i1->bitmapHeight - i1->verticalOffset),
                              (i2->bitmapHeight - i2->verticalOffset));

  // distance computation for left and right characters
  auto distLeft  = std::shared_ptr<FIX32[]>(new FIX32[hight]);
  auto distRight = std::shared_ptr<FIX32[]>(new FIX32[hight]);

  for (int i = 0; i < hight; i++) {
    distLeft[i]  = MAKE_FLOAT_FIXED(-1.0);
    distRight[i] = MAKE_FLOAT_FIXED(-1.0);
  }

  // distLeft is receiving the right distance in pixels of the first black pixel on each
  // line of the character
  int idx = 0;
  for (uint8_t i = distIdxLeft; i < i1->bitmapHeight + distIdxLeft; i++, idx += i1->bitmapWidth) {
    distLeft[i] = 0;
    for (int col = i1->bitmapWidth - 1; col >= 0; col--) {
      if (b1->pixels[idx + col]) break;
      distLeft[i] += FIXED_POINT_ONE;
    }
  }

  // distRight is receiving the left distance in pixels of the first black pixel on each
  // line of the character
  idx = 0;
  for (uint8_t i = distIdxRight; i < i2->bitmapHeight + distIdxRight; i++, idx += i2->bitmapWidth) {
    distRight[i] = 0;
    for (int col = 0; col < i2->bitmapWidth; col++) {
      if (b2->pixels[idx + col]) break;
      distRight[i] += FIXED_POINT_ONE;
    }
  }

  // find convex corner locations and adjust distances

  // Right Side Convex Hull for the character at left
  if (i1->bitmapHeight >= 3) { // 1 and 2 line characters don't need adjustment

    // Compute the cross product of 3 points. If negative, the angle is convex
    auto crossLeft = [distLeft](int i, int j, int k) -> FIX32 {
      return FIXED_MULT((distLeft[j] - distLeft[i]), MAKE_INT_FIXED(k - i)) -
             FIXED_MULT(MAKE_INT_FIXED(j - i), (distLeft[k] - distLeft[i]));
    };

    // Adjusts distances to get a line between two vertices of the Convex Hull
    auto adjustLeft = [distLeft](int i, int j) {
      if ((j - i) > 1) {
        if (abs(distLeft[j] - distLeft[i]) <= MAKE_FLOAT_FIXED(0.01)) {
          for (int k = i + 1; k < j; k++) { distLeft[k] = distLeft[i]; }
        } else {
          FIX32 slope = FIXED_DIV((distLeft[j] - distLeft[i]), MAKE_INT_FIXED(j - i));
          FIX32 v     = distLeft[i];
          for (int k = i + 1; k < j; k++) {
            v += slope;
            distLeft[k] = v;
          }
        }
      }
    };

    // Find vertices using the cross product and adjust the distances
    // to get the right portion of the Convex Hull polygon.
    int i = distIdxLeft;
    int j = i + 1;
    while (j < (i1->bitmapHeight + distIdxLeft)) {
      bool found = true;
      for (int k = j + 1; k < i1->bitmapHeight + distIdxLeft; k++) {
        FIX32 val = crossLeft(i, j, k);
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
    auto crossRight = [distRight](int i, int j, int k) -> FIX32 {
      return FIXED_MULT((distRight[j] - distRight[i]), MAKE_INT_FIXED(k - i)) -
             FIXED_MULT(MAKE_INT_FIXED(j - i), (distRight[k] - distRight[i]));
    };

    // Adjusts distances to get a line between two vertices of the Convex Hull
    auto adjustRight = [distRight](int i, int j) {
      if ((j - i) > 1) {
        if (abs(distRight[j] - distRight[i]) <= MAKE_FLOAT_FIXED(0.01)) {
          for (int k = i + 1; k < j; k++) { distRight[k] = distRight[i]; }
        } else {
          FIX32 slope = FIXED_DIV((distRight[j] - distRight[i]), MAKE_INT_FIXED(j - i));
          FIX32 v     = distRight[i];
          for (int k = i + 1; k < j; k++) {
            v += slope;
            distRight[k] = v;
          }
        }
      }
    };

    // Find vertices using the cross product and adjust the distances
    // to get the left portion of the Convex Hull polygon.
    int i = distIdxRight;
    int j = i + 1;
    while (j < (i2->bitmapHeight + distIdxRight)) {
      bool found = true;
      for (int k = j + 1; k < i2->bitmapHeight + distIdxRight; k++) {
        FIX32 val = crossRight(i, j, k);
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

  if (length <= 0) {
    if (distIdxRight > distIdxLeft) {
      FIX32   val = distRight[distIdxRight];
      uint8_t i   = distIdxRight - 1;
      while (length <= 0) {
        distRight[i--] = val;
        length += 1;
        firstIdx -= 1;
        distIdxRight -= 1;
      }
    } else {
      FIX32   val = distLeft[distIdxLeft];
      uint8_t i   = distIdxLeft - 1;
      while (length <= 0) {
        distLeft[i--] = val;
        length += 1;
        firstIdx -= 1;
        distIdxLeft -= 1;
      }
    }
  }

#if 0
    std::cout << "Computed left-right convex hulls (origin:" << +origin << "firstIdx:" << +firstIdx
              << ", length:" << +length << ", hight:" << +hight << "):" << std::endl;
    for (int i = 0; i < hight; i++) {
      std::cout << "  ";
      if (distLeft[i] != MAKE_FLOAT_FIXED(-1)) {
        std::cout << MAKE_FIXED_FLOAT(distLeft[i]);
      } else {
        std::cout << " ";
      }

      std::cout << ", ";

      if (distRight[i] != MAKE_FLOAT_FIXED(-1)) {
        std::cout << MAKE_FIXED_FLOAT(distRight[i]);
      } else {
        std::cout << " ";
      }
      std::cout << std::endl;
    }
#endif

  // Now, compute the smallest distance that exists between
  // the two characters. Pixels on each line are checked as well
  // as angled pixels (on the lines above and below)
  kerning = MAKE_INT_FIXED(999);
  FIX32 dist;
  for (int i = firstIdx; i < firstIdx + length; i++) {
    dist = distLeft[i] + distRight[i];
    if (dist < kerning) kerning = dist;
    if ((i > 0) && (distLeft[i - 1] >= 0)) {
      dist = distLeft[i - 1] + distRight[i];
      if (dist < kerning) kerning = dist;
    }
    if ((i < (hight - 1)) && (distLeft[i + 1] >= 0)) {
      dist = distLeft[i + 1] + distRight[i];
      if (dist < kerning) kerning = dist;
    }
  }
  if ((firstIdx > 0) && (distRight[firstIdx - 1] >= 0)) {
    dist = distLeft[firstIdx] + distRight[firstIdx - 1];
    if (dist < kerning) kerning = dist;
  }
  int lastIdx = firstIdx + length - 1;
  if ((lastIdx < (hight - 1)) && (distRight[lastIdx + 1] >= 0)) {
    dist = distLeft[lastIdx] + distRight[lastIdx + 1];
    if (dist < kerning) kerning = dist;
  }

  int addedWildcard;

  if (i2->rleMetrics.beforeAddedOptKern == 3) {
    addedWildcard = -1;
  } else {
    addedWildcard = i2->rleMetrics.beforeAddedOptKern;
  }
  addedWildcard += i1->rleMetrics.afterAddedOptKern;

  // std::cout << "Minimal distance: " << MAKE_FIXED_FLOAT(kerning) << std::endl;

  // Adjust the resulting kerning value, considering the targetted KERNING_SIZE (the space to have
  // between characters), the size of the character and the normal distance that will be used by
  // the writing algorithm
  kerning = (-MIN(kerning - MAKE_INT_FIXED(KERNING_SIZE + addedWildcard),
                  MAKE_INT_FIXED(i2->bitmapWidth))) -
            MAKE_INT_FIXED(normal_distance);
  // }

  result1 = kerning >> 4; // Convert to FIX16

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
    //    adjustSize();
    update();
  }
}

QSize DrawingSpace::sizeHint() const {
  //   std::cout << "drawingSpace size: " << requiredSize_.width() << ", " <<
  //   requiredSize_.height()
  //             << " -- Widget size: " << this->width() << ", " << this->height() << std::endl;
  return requiredSize_;
}

void DrawingSpace::printWord(WordPtr &word, QPainter *painter) {

  QRect rect;

  bool firstChar = true;
  for (auto &ch : *word) {

    //    std::cout << "Position:" << pos_.x();
    //    std::cout << " CH kern:" << ch->kern / 64 << " adv:" << ch->glyphInfo->advance / 64
    //              << " hoff:" << +ch->glyphInfo->horizontalOffset << std::endl;

    //    int diff = ((ch->kern + (ch->kern < 0 ? -32 : 32)) / 64);
    //    pos_.setX(pos_.x() + diff);

    int voff = ch->glyphInfo->verticalOffset;

    // The first character of a word must not use the horizontalOffset.
    // Also, there is a need to remove the effect on the advance param (hoff2),
    // see at the end of the loop.
    int hoff    = firstChar ? 0 : ch->glyphInfo->horizontalOffset;
    int hoff2   = firstChar ? -ch->glyphInfo->horizontalOffset : 0;

    int advance = (ch->glyphInfo->advance + 32) >> 6;
    if (advance == 0) advance = ch->glyphInfo->bitmapWidth + 1;

    if (painter != nullptr) {
      int idx = 0;
      if (pixelSize_ == 1) {
        for (int row = 0; row < ch->bitmap->dim.height; row++) {
          for (int col = 0; col < ch->bitmap->dim.width; col++, idx++) {
            if (ch->bitmap->pixels[idx] != 0) {
              painter->drawPoint(QPoint(10 + (pos_.x() - hoff + col), pos_.y() - voff + row));
            }
          }
        }
      } else {
        for (int row = 0; row < ch->bitmap->dim.height; row++) {
          for (int col = 0; col < ch->bitmap->dim.width; col++, idx++) {
            if (ch->bitmap->pixels[idx] != 0) {
              rect = QRect(10 + (pos_.x() - hoff + col) * pixelSize_,
                           (pos_.y() - voff + row) * pixelSize_, pixelSize_, pixelSize_);

              painter->drawRect(rect);
            }
          }
        }
      }
    }

    pos_.setX(pos_.x() + advance - hoff2 + ((ch->kern + (ch->kern < 0 ? -32 : 32)) / 64));
    firstChar = false;
  }

  // Adjustment for the last character of a word. This is to get equal spaces between characters.

  // auto &ch = word->at(word->size() - 1);
  // pos_.setX(pos_.x() - (((ch->glyphInfo->advance + 32) >> 6) + ch->glyphInfo->horizontalOffset -
  //                       ch->bitmap->dim.width));
}

auto DrawingSpace::printLine(QPainter *painter) -> void {

  for (auto &w : line_) {
    printWord(w, painter);
    pos_.setX(pos_.x() + font_->getFaceHeader(faceIdx_)->spaceSize);
  }

  pos_.setY(pos_.y() + font_->getLineHeight(faceIdx_));
  pos_.setX(0);

  line_.clear();
  linePixelWidth_ = 0;
}

auto DrawingSpace::addWordToLine(QString &w, QPainter *painter) -> void {

  auto word = WordPtr(new Word);

  auto iter = w.begin();

  IBMFDefs::BitmapPtr       b1, b2;
  IBMFDefs::GlyphInfoPtr    i1, i2;
  IBMFDefs::GlyphCode       g1, g2;
  IBMFDefs::GlyphLigKernPtr k1, k2;

  g1                   = font_->translate((*iter++).unicode());
  g2                   = (iter == w.end()) ? NO_GLYPH_CODE : font_->translate((*iter++).unicode());

  FIX16 kern           = 0;

  bool firstWordChar   = true;
  bool kernPairPresent = false;
  int  wordPixelWidth  = 0;

  if (linePixelWidth_ > 0) { linePixelWidth_ += spaceSize_; }
  if ((linePixelWidth_ * pixelSize_ + 20) > this->width()) { printLine(painter); }

  while (g1 != NO_GLYPH_CODE) {
    bool g2_loaded = false;

    if (firstWordChar) {
      firstWordChar = false;
      if ((bypassGlyphCode_ != IBMFDefs::NO_GLYPH_CODE) && (g1 == bypassGlyphCode_)) {
        b1 = bypassBitmap_;
        i1 = bypassGlyphInfo_;
        k1 = bypassGlyphLigKern_;
      } else {
        if (!font_->getGlyph(faceIdx_, g1, i1, b1, k1)) { break; }
      }
    }

    kern = 0;

    if (normalKerning_) {

      // Ligature loop for glyphCode1
      while (font_->ligKern(faceIdx_, g1, &g2, &kern, &kernPairPresent, k1)) {
        g1 = g2;
        g2 = (iter == w.end()) ? NO_GLYPH_CODE : font_->translate((*iter++).unicode());

        if ((bypassGlyphCode_ != IBMFDefs::NO_GLYPH_CODE) && (g1 == bypassGlyphCode_)) {
          b1 = bypassBitmap_;
          i1 = bypassGlyphInfo_;
          k1 = bypassGlyphLigKern_;
        } else {
          if (!font_->getGlyph(faceIdx_, g1, i1, b1, k1)) { break; }
        }
      }

      if (g2 != NO_GLYPH_CODE) {
        if ((bypassGlyphCode_ != IBMFDefs::NO_GLYPH_CODE) && (g2 == bypassGlyphCode_)) {
          b2 = bypassBitmap_;
          i2 = bypassGlyphInfo_;
          k2 = bypassGlyphLigKern_;
        } else {
          if (!font_->getGlyph(faceIdx_, g2, i2, b2, k2)) { break; }
        }
        g2_loaded = true;

        // Ligature loop for glyphCode2
        GlyphCode g3 = (iter == w.end()) ? NO_GLYPH_CODE : font_->translate((*iter).unicode());
        if (g3 != NO_GLYPH_CODE) {
          bool  some_lig = false;
          bool  dummy;
          FIX16 k;
          while (font_->ligKern(faceIdx_, g2, &g3, &k, &dummy, k2)) {
            g2       = g3;
            g3       = (iter == w.end()) ? NO_GLYPH_CODE : font_->translate((*++iter).unicode());
            some_lig = true;

            if ((bypassGlyphCode_ != IBMFDefs::NO_GLYPH_CODE) && (g2 == bypassGlyphCode_)) {
              b2 = bypassBitmap_;
              i2 = bypassGlyphInfo_;
              k2 = bypassGlyphLigKern_;
            } else {
              if (!font_->getGlyph(faceIdx_, g2, i2, b2, k2)) { break; }
            }
          }
          if (some_lig) { font_->ligKern(faceIdx_, g1, &g2, &kern, &kernPairPresent); }
        }
      }
    }

    if ((!g2_loaded) && (g2 != NO_GLYPH_CODE)) {
      if ((bypassGlyphCode_ != IBMFDefs::NO_GLYPH_CODE) && (g2 == bypassGlyphCode_)) {
        b2 = bypassBitmap_;
        i2 = bypassGlyphInfo_;
        k2 = bypassGlyphLigKern_;
      } else {
        if (!font_->getGlyph(faceIdx_, g2, i2, b2, k2)) { break; }
      }
    }

    if (opticalKerning_ && !kernPairPresent && (g2 != NO_GLYPH_CODE)) {
      kern = computeOpticalKerning(b1, b2, i1, i2);
    }

    if (((linePixelWidth_ + wordPixelWidth + ((i1->advance + 32) >> 6)) * pixelSize_ + 20) >
        this->width()) {
      if ((line_.size() == 0) && (word->size() > 0)) {
        line_.push_back(word);
        word           = WordPtr(new Word);
        wordPixelWidth = 0;
      }
    }

    word->push_back(OneGlyphPtr(new OneGlyph{.bitmap = b1, .glyphInfo = i1, .kern = kern}));
    wordPixelWidth += ((i1->advance + 32) >> 6);

    g1 = g2;
    i1 = i2;
    b1 = b2;
    k1 = k2;

    g2 = (iter == w.end()) ? NO_GLYPH_CODE : font_->translate((*iter++).unicode());
  }

  if (((linePixelWidth_ + wordPixelWidth) * pixelSize_ + 20) > this->width()) {
    printLine(painter);
  }
  line_.push_back(word);
  linePixelWidth_ += wordPixelWidth;

  w.clear();
}

void DrawingSpace::drawScreen(QPainter *painter) {

  std::cout << "Window width: " << this->width() << std::endl;

  if ((font_ == nullptr) || textToDraw_.isEmpty()) return;

  if (painter != nullptr) {
    painter->setPen(QPen(QBrush(QColorConstants::DarkGray), 1));
    painter->setBrush(QBrush(QColorConstants::DarkGray));
  }

  line_.clear();
  linePixelWidth_ = 0;
  spaceSize_      = font_->getFaceHeader(faceIdx_)->spaceSize;
  pos_            = QPoint(0, font_->getLineHeight(faceIdx_));

  QString word;

  for (auto &ch : textToDraw_) {
    if (ch == ' ') {
      if (word.length() > 0) { addWordToLine(word, painter); }
    } else if (ch == '\n') {
      if (word.length() > 0) { addWordToLine(word, painter); }
      if (line_.size() > 0) { printLine(painter); }
    } else {
      word.append(ch);
    }
  }

  if (word.length() > 0) { addWordToLine(word, painter); }
  if (line_.size() > 0) { printLine(painter); }
}

void DrawingSpace::paintEvent(QPaintEvent *event) {
  if (font_ == nullptr) return;
  QPainter painter(this);
  drawScreen(&painter);
}
