#include "bitmapRenderer.h"

#include <QMessageBox>

#include "qwidget.h"
#include "setPixelCommand.h"

BitmapRenderer::BitmapRenderer(QWidget *parent, int pixelSize, bool noScroll,
                               QUndoStack *undoStack_)
    : QWidget(parent), undoStack_(undoStack_), bitmapChanged_(false), glyphPresent_(false),
      pixelSize_(pixelSize), wasBlack_(true), editable_(true), noScroll_(noScroll),
      lastPos_(QPoint(0, 0)), bitmapOffsetPos_(QPoint(0, 0)), glyphBitmapPos_(QPoint(0, 0)),
      glyphOriginPos_(QPoint(0, 0)) {
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);
  clearBitmap();
}

void BitmapRenderer::resizeEvent(QResizeEvent *event) {
  if (noScroll_) {
    bitmapOffsetPos_ = QPoint((bitmapWidth - (width() / pixelSize_)) / 2,
                              (bitmapHeight - (height() / pixelSize_)) / 2);
    if (bitmapOffsetPos_.x() < 0) bitmapOffsetPos_.setX(0);
    if (bitmapOffsetPos_.y() < 0) bitmapOffsetPos_.setY(0);
  }
}

int BitmapRenderer::getPixelSize() {
  return pixelSize_;
}

void BitmapRenderer::connectTo(BitmapRenderer *main_renderer) {
  QObject::connect(main_renderer, &BitmapRenderer::bitmapHasChanged, this,
                   &BitmapRenderer::clearAndReloadBitmap);
  QObject::connect(main_renderer, &BitmapRenderer::bitmapCleared, this,
                   &BitmapRenderer::clearAndRepaint);
  editable_ = false;
}

void BitmapRenderer::clearBitmap() {
  memset(displayBitmap_, PixelType::WHITE, sizeof(displayBitmap_));
}

void BitmapRenderer::clearAndRepaint() {
  clearBitmap();
  update();
}

void BitmapRenderer::clearAndEmit(bool repaint_after) {
  clearBitmap();
  if (repaint_after) update();
  emit bitmapCleared();
}

void BitmapRenderer::setPixelSize(int pixel_size) {
  IBMFDefs::BitmapPtr bitmap;
  QPoint              originOffsets;

  pixelSize_ = pixel_size;
  if (retrieveBitmap(&bitmap, &originOffsets)) {
    clearAndReloadBitmap(*bitmap, originOffsets);
  } else {
    update();
  }
}

void BitmapRenderer::setBitmapOffsetPos(QPoint pos) {
  if ((pos.x() < 0) || (pos.x() >= bitmapWidth) || (pos.y() < 0) || (pos.y() >= bitmapHeight)) {
    QMessageBox::warning(this, "Internal error", "setBitmapOffsetPos() received a bad position!");
  }
  if ((pos.x() != bitmapOffsetPos_.x()) || (pos.y() != bitmapOffsetPos_.y())) {
    bitmapOffsetPos_ = pos;
  }
}

// Put a pixel on screen. The pixel will be sized according to *pixelSize_* value
// and if the renderer is the main editable or not
void BitmapRenderer::setScreenPixel(QPoint pos) {
  QPainter painter(this);
  QRect    rect;

  painter.setPen(QPen(QBrush(QColorConstants::DarkGray), 1));
  painter.setBrush(QBrush(QColorConstants::DarkGray));

  if (editable_) {
    // Leave some space for grid lines
    rect = QRect((pos.x() - bitmapOffsetPos_.x()) * pixelSize_ + 2,
                 (pos.y() - bitmapOffsetPos_.y()) * pixelSize_ + 2, pixelSize_ - 4, pixelSize_ - 4);
  } else {
    rect = QRect((pos.x() - bitmapOffsetPos_.x()) * pixelSize_,
                 (pos.y() - bitmapOffsetPos_.y()) * pixelSize_, pixelSize_, pixelSize_);
  }
  painter.drawRect(rect);
}

void BitmapRenderer::setAdvance(IBMFDefs::FIX16 newAdvance) {
  glyphInfo_.advance = newAdvance;
  update();
}

// The event will paint the grid lines, the limiting lines and the pixels that are part
// of the glyph
void BitmapRenderer::paintEvent(QPaintEvent * /* event */) {
  QPainter painter(this);

  painter.setPen(QPen(QBrush(QColorConstants::LightGray), 1));
  painter.setBrush(QBrush(QColorConstants::Red));

  if (editable_) {

    for (int row = pixelSize_; row < height(); row += pixelSize_) {
      for (int col = pixelSize_; col < width(); col += pixelSize_) {
        painter.drawLine(QPoint(col, 0), QPoint(col, height()));
        painter.drawLine(QPoint(0, row), QPoint(width(), row));
      }
    }

    if (glyphPresent_) {
      int originRow    = (glyphOriginPos_.y() - bitmapOffsetPos_.y() - 1) * pixelSize_;
      int originCol    = (glyphOriginPos_.x() - bitmapOffsetPos_.x()) * pixelSize_;
      int descenderRow = originRow + (faceHeader_.descenderHeight * pixelSize_);
      int topRow       = descenderRow - (faceHeader_.lineHeight * pixelSize_);
      int xRow         = originRow - (((float) faceHeader_.xHeight / 64.0) * pixelSize_);
      int advCol       = originCol + (((float) glyphInfo_.advance / 64.0) * pixelSize_);

      painter.setPen(QPen(QBrush(QColorConstants::Red), 1));
      painter.drawLine(QPoint(originCol, originRow), QPoint(advCol, originRow));
      painter.drawLine(QPoint(originCol, descenderRow), QPoint(originCol, topRow));
      painter.drawLine(QPoint(originCol, descenderRow), QPoint(advCol, descenderRow));
      painter.drawLine(QPoint(originCol, topRow), QPoint(advCol, topRow));

      painter.drawLine(QPoint(originCol, xRow), QPoint(advCol, xRow));

      painter.setPen(QPen(QBrush(QColorConstants::Blue), 1));
      painter.drawLine(QPoint(advCol, descenderRow), QPoint(advCol, topRow));
    }

    painter.setPen(QPen(QBrush(QColorConstants::LightGray), 1));
  }

  int rowp;
  for (int row = bitmapOffsetPos_.y(), rowp = row * bitmapWidth; row < bitmapHeight;
       row++, rowp += bitmapWidth) {
    for (int col = bitmapOffsetPos_.x(); col < bitmapWidth; col++) {
      if (displayBitmap_[rowp + col] == PixelType::BLACK) { setScreenPixel(QPoint(col, row)); }
    }
  }
}

void BitmapRenderer::paintPixel(PixelType pixelType, QPoint atPos) {
  int idx             = atPos.y() * bitmapWidth + atPos.x();
  displayBitmap_[idx] = pixelType;

  IBMFDefs::BitmapPtr theBitmap;
  QPoint              originOffsets;
  if (retrieveBitmap(&theBitmap, &originOffsets)) {
    emit bitmapHasChanged(*theBitmap, originOffsets);
  } else {
    emit bitmapCleared();
  }

  update();
}

void BitmapRenderer::mousePressEvent(QMouseEvent *event) {
  if (editable_) {
    lastPos_ = QPoint(bitmapOffsetPos_.x() + event->pos().x() / pixelSize_,
                      bitmapOffsetPos_.y() + event->pos().y() / pixelSize_);
    if ((lastPos_.x() < bitmapWidth) && (lastPos_.y() < bitmapHeight)) {
      int       idx = lastPos_.y() * bitmapWidth + lastPos_.x();
      PixelType newPixelType;
      if (displayBitmap_[idx] == PixelType::BLACK) {
        newPixelType = PixelType::WHITE;
        wasBlack_    = false;
      } else {
        newPixelType = PixelType::BLACK;
        wasBlack_    = true;
      }
      undoStack_->push(new SetPixelCommand(this, newPixelType, lastPos_));
    }
  }
}

void BitmapRenderer::mouseMoveEvent(QMouseEvent *event) {
  if (editable_) {
    QPoint pos = QPoint(bitmapOffsetPos_.x() + event->pos().x() / pixelSize_,
                        bitmapOffsetPos_.y() + event->pos().y() / pixelSize_);
    if ((pos.x() < bitmapWidth) && (pos.y() < bitmapHeight) &&
        ((pos.x() != lastPos_.x()) || (pos.y() != lastPos_.y()))) {
      PixelType newPixelType = wasBlack_ ? PixelType::BLACK : PixelType::WHITE;
      lastPos_               = pos;
      undoStack_->push(new SetPixelCommand(this, newPixelType, lastPos_));
    }
  }
}

void BitmapRenderer::wheelEvent(QWheelEvent *event) {}

void BitmapRenderer::clearAndLoadBitmap(const IBMFDefs::Bitmap     &bitmap,
                                        const IBMFDefs::Preamble   &preamble,
                                        const IBMFDefs::FaceHeader &faceHeader,
                                        const IBMFDefs::GlyphInfo  &glyphInfo) {
  preamble_     = preamble;
  faceHeader_   = faceHeader;
  glyphInfo_    = glyphInfo;
  glyphPresent_ = true;

  glyphBitmapPos_ =
      QPoint((bitmapWidth - bitmap.dim.width) / 2, (bitmapHeight - bitmap.dim.height) / 2);
  glyphOriginPos_ = QPoint(glyphBitmapPos_.x() + glyphInfo_.horizontalOffset,
                           1 + glyphBitmapPos_.y() + glyphInfo_.verticalOffset);
  clearAndEmit();
  loadBitmap(bitmap);
}

void BitmapRenderer::clearAndReloadBitmap(const IBMFDefs::Bitmap &bitmap,
                                          const QPoint           &originOffsets) {
  clearAndEmit();
  loadBitmap(bitmap);
}

void BitmapRenderer::loadBitmap(const IBMFDefs::Bitmap &bitmap) {

  if ((glyphBitmapPos_.x() < 0) || (glyphBitmapPos_.y() < 0)) {
    QMessageBox::warning(this, "Internal error",
                         "The Glyph's bitmap is too large for the application "
                         "capability. Largest "
                         "bitmap width/height is set to 200 pixels.");
    return;
  }

  int idx;
  int rowp;
  for (int row = glyphBitmapPos_.y(), rowp = row * bitmapWidth, idx = 0;
       row < glyphBitmapPos_.y() + bitmap.dim.height; row++, rowp += bitmapWidth) {
    for (int col = glyphBitmapPos_.x(); col < glyphBitmapPos_.x() + bitmap.dim.width;
         col++, idx++) {
      displayBitmap_[rowp + col] = (bitmap.pixels[idx] == 0) ? PixelType::WHITE : PixelType::BLACK;
    }
  }

  bitmapChanged_ = false;

  update();
}

bool BitmapRenderer::retrieveBitmap(IBMFDefs::BitmapPtr *bitmap, QPoint *originOffsets) {
  QPoint topLeft;
  QPoint bottomRight;

  int row;
  int col;

  int idx;
  for (row = 0, idx = 0; row < bitmapHeight; row++) {
    for (col = 0; col < bitmapWidth; col++, idx++) {
      if (displayBitmap_[idx] != PixelType::WHITE) goto cont1;
    }
  }
cont1:
  if (row >= bitmapHeight) return false; // The bitmap is empty of black pixels

  topLeft.setY(row);

  int rowp;
  for (row = bitmapHeight - 1, rowp = (bitmapHeight - 1) * bitmapWidth; row >= 0;
       row--, rowp -= bitmapWidth) {
    for (col = 0; col < bitmapWidth; col++) {
      if (displayBitmap_[rowp + col] != PixelType::WHITE) goto cont2;
    }
  }
cont2:
  bottomRight.setY(row);

  for (col = 0; col < bitmapWidth; col++) {
    for (row = 0, rowp = 0; row < bitmapHeight; row++, rowp += bitmapWidth) {
      if (displayBitmap_[rowp + col] != PixelType::WHITE) goto cont3;
    }
  }
cont3:
  topLeft.setX(col);

  for (col = bitmapWidth - 1; col >= 0; col--) {
    for (row = 0, rowp = 0; row < bitmapHeight; row++, rowp += bitmapWidth) {
      if (displayBitmap_[rowp + col] != PixelType::WHITE) goto cont4;
    }
  }
cont4:
  bottomRight.setX(col);

  IBMFDefs::BitmapPtr theBitmap = IBMFDefs::BitmapPtr(new IBMFDefs::Bitmap);
  theBitmap->dim =
      IBMFDefs::Dim(bottomRight.x() - topLeft.x() + 1, bottomRight.y() - topLeft.y() + 1);
  int size          = theBitmap->dim.width * theBitmap->dim.height;
  theBitmap->pixels = IBMFDefs::Pixels(size, 0);

  idx = 0;
  for (row = topLeft.y(), rowp = row * bitmapWidth; row <= bottomRight.y();
       row++, rowp += bitmapWidth) {
    for (col = topLeft.x(); col <= bottomRight.x(); col++) {
      theBitmap->pixels[idx++] = displayBitmap_[rowp + col] == PixelType::WHITE ? 0 : 0xff;
    }
  }

  if (originOffsets != nullptr) {
    *originOffsets =
        QPoint(glyphOriginPos_.x() - topLeft.x(), glyphOriginPos_.y() - topLeft.y() - 1);

    float oldAdvance = (float) glyphInfo_.advance / 64.0;
    int   oldWidth   = glyphInfo_.bitmapWidth;
    int   oldHOffset = glyphInfo_.horizontalOffset;

    int   oldNormalizedWidth = oldWidth - oldHOffset;
    int   newNormalizedWidth = theBitmap->dim.width - originOffsets->x();
    float newAdvance         = oldAdvance + (newNormalizedWidth - oldNormalizedWidth);

    glyphInfo_.advance          = newAdvance * 64.0;
    glyphInfo_.bitmapWidth      = theBitmap->dim.width;
    glyphInfo_.bitmapHeight     = theBitmap->dim.height;
    glyphInfo_.horizontalOffset = originOffsets->x();
    glyphInfo_.verticalOffset   = originOffsets->y();
  }
  *bitmap = theBitmap;

  return true;
}
