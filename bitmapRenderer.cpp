#include "bitmapRenderer.h"

#include <QMessageBox>

#include "qwidget.h"
#include "setPixelCommand.h"

BitmapRenderer::BitmapRenderer(QWidget *parent, int pixelSize, bool noScroll,
                               QUndoStack *undoStack_)
    : QWidget(parent), undoStack_(undoStack_), _bitmapChanged(false), _glyphPresent(false),
      _pixelSize(pixelSize), _wasBlack(true), _editable(true), _noScroll(noScroll),
      _bitmapOffsetPos(QPoint(0, 0)) {
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);
  clearBitmap();
}

void BitmapRenderer::resizeEvent(QResizeEvent *event) {
  if (_noScroll) {
    _bitmapOffsetPos = QPoint((bitmapWidth - (width() / _pixelSize)) / 2,
                              (bitmapHeight - (height() / _pixelSize)) / 2);
    if (_bitmapOffsetPos.x() < 0) _bitmapOffsetPos.setX(0);
    if (_bitmapOffsetPos.y() < 0) _bitmapOffsetPos.setY(0);
  }
}

int BitmapRenderer::getPixelSize() { return _pixelSize; }

void BitmapRenderer::connectTo(BitmapRenderer *main_renderer) {
  QObject::connect(main_renderer, &BitmapRenderer::bitmapHasChanged, this,
                   &BitmapRenderer::clearAndReloadBitmap);
  QObject::connect(main_renderer, &BitmapRenderer::bitmapCleared, this,
                   &BitmapRenderer::clearAndRepaint);
  _editable = false;
}

void BitmapRenderer::clearBitmap() {
  memset(_displayBitmap, PixelType::WHITE, sizeof(_displayBitmap));
}

void BitmapRenderer::clearAndRepaint() {
  clearBitmap();
  repaint();
}

void BitmapRenderer::clearAndEmit(bool repaint_after) {
  clearBitmap();
  if (repaint_after) repaint();
  emit bitmapCleared();
}

void BitmapRenderer::setPixelSize(int pixel_size) {
  IBMFDefs::Bitmap *bitmap;

  _pixelSize = pixel_size;
  if (retrieveBitmap(&bitmap)) {
    clearAndReloadBitmap(*bitmap);
    delete bitmap;
  } else {
    repaint();
  }
}

void BitmapRenderer::setBitmapOffsetPos(QPoint pos) {
  if ((pos.x() < 0) || (pos.x() >= bitmapWidth) || (pos.y() < 0) || (pos.y() >= bitmapHeight)) {
    QMessageBox::warning(this, "Internal error", "setBitmapOffsetPos() received a bad position!");
  }
  if ((pos.x() != _bitmapOffsetPos.x()) || (pos.y() != _bitmapOffsetPos.y())) {
    _bitmapOffsetPos = pos;
  }
}

void BitmapRenderer::setScreenPixel(QPoint pos) {
  QPainter painter(this);
  QRect    rect;

  painter.setPen(QPen(QBrush(QColorConstants::DarkGray), 1));
  painter.setBrush(QBrush(QColorConstants::DarkGray));

  if (_editable) {
    // Leave some space for grid lines
    rect = QRect((pos.x() - _bitmapOffsetPos.x()) * _pixelSize + 2,
                 (pos.y() - _bitmapOffsetPos.y()) * _pixelSize + 2, _pixelSize - 4, _pixelSize - 4);
  } else {
    rect = QRect((pos.x() - _bitmapOffsetPos.x()) * _pixelSize,
                 (pos.y() - _bitmapOffsetPos.y()) * _pixelSize, _pixelSize, _pixelSize);
  }
  painter.drawRect(rect);
}

void BitmapRenderer::paintEvent(QPaintEvent * /* event */) {
  QPainter painter(this);

  painter.setPen(QPen(QBrush(QColorConstants::LightGray), 1));
  painter.setBrush(QBrush(QColorConstants::Red));

  if (_editable) {

    for (int row = _pixelSize; row < height(); row += _pixelSize) {
      for (int col = _pixelSize; col < width(); col += _pixelSize) {
        painter.drawLine(QPoint(col, 0), QPoint(col, height()));
        painter.drawLine(QPoint(0, row), QPoint(width(), row));
      }
    }

    if (_glyphPresent) {
      int originRow    = (_glyphOriginPos.y() - _bitmapOffsetPos.y()) * _pixelSize;
      int originCol    = (_glyphOriginPos.x() - _bitmapOffsetPos.x()) * _pixelSize;
      int descenderRow = originRow + (_faceHeader.descenderHeight * _pixelSize);
      int topRow       = descenderRow - (_faceHeader.lineHeight * _pixelSize);
      int xRow         = originRow - (((float)_faceHeader.xHeight / 64.0) * _pixelSize);
      int advCol       = originCol + (((float)_glyphInfo.advance / 64.0) * _pixelSize);
      // int emCol        = originCol + (floor(((float) _faceHeader.emSize / 64.0)) * _pixelSize);

      painter.setPen(QPen(QBrush(QColorConstants::Red), 1));
      painter.drawLine(QPoint(originCol, originRow), QPoint(advCol, originRow));
      painter.drawLine(QPoint(originCol, descenderRow), QPoint(originCol, topRow));
      painter.drawLine(QPoint(originCol, descenderRow), QPoint(advCol, descenderRow));
      painter.drawLine(QPoint(originCol, topRow), QPoint(advCol, topRow));

      painter.setPen(QPen(QBrush(QColorConstants::Blue), 1));
      painter.drawLine(QPoint(originCol, xRow), QPoint(advCol, xRow));

      painter.setPen(QPen(QBrush(QColorConstants::Green), 1));
      painter.drawLine(QPoint(advCol, descenderRow), QPoint(advCol, topRow));
    }

    painter.setPen(QPen(QBrush(QColorConstants::LightGray), 1));
  }

  int rowp;
  for (int row = _bitmapOffsetPos.y(), rowp = row * bitmapWidth; row < bitmapHeight;
       row++, rowp += bitmapWidth) {
    for (int col = _bitmapOffsetPos.x(); col < bitmapWidth; col++) {
      if (_displayBitmap[rowp + col] == PixelType::BLACK) {
        setScreenPixel(QPoint(col, row));
      }
    }
  }
}

void BitmapRenderer::paintPixel(PixelType pixelType, QPoint atPos) {
  int idx             = atPos.y() * bitmapWidth + atPos.x();
  _displayBitmap[idx] = pixelType;
  repaint();

  IBMFDefs::Bitmap *theBitmap;
  if (retrieveBitmap(&theBitmap)) {
    emit bitmapHasChanged(*theBitmap);
  } else {
    emit bitmapCleared();
  }
}

void BitmapRenderer::mousePressEvent(QMouseEvent *event) {
  if (_editable) {
    _lastPos = QPoint(_bitmapOffsetPos.x() + event->pos().x() / _pixelSize,
                      _bitmapOffsetPos.y() + event->pos().y() / _pixelSize);
    if ((_lastPos.x() < bitmapWidth) && (_lastPos.y() < bitmapHeight)) {
      int       idx = _lastPos.y() * bitmapWidth + _lastPos.x();
      PixelType newPixelType;
      if (_displayBitmap[idx] == PixelType::BLACK) {
        newPixelType = PixelType::WHITE;
        _wasBlack    = false;
      } else {
        newPixelType = PixelType::BLACK;
        _wasBlack    = true;
      }
      undoStack_->push(new SetPixelCommand(this, newPixelType, _lastPos));
    }
  }
}

void BitmapRenderer::mouseMoveEvent(QMouseEvent *event) {
  if (_editable) {
    QPoint pos = QPoint(_bitmapOffsetPos.x() + event->pos().x() / _pixelSize,
                        _bitmapOffsetPos.y() + event->pos().y() / _pixelSize);
    if ((pos.x() < bitmapWidth) && (pos.y() < bitmapHeight) &&
        ((pos.x() != _lastPos.x()) || (pos.y() != _lastPos.y()))) {
      PixelType newPixelType = _wasBlack ? PixelType::BLACK : PixelType::WHITE;
      _lastPos               = pos;
      undoStack_->push(new SetPixelCommand(this, newPixelType, _lastPos));
    }
  }
}

void BitmapRenderer::wheelEvent(QWheelEvent *event) {}

void BitmapRenderer::clearAndLoadBitmap(const IBMFDefs::Bitmap     &bitmap,
                                        const IBMFDefs::Preamble   &preamble,
                                        const IBMFDefs::FaceHeader &faceHeader,
                                        const IBMFDefs::GlyphInfo  &glyphInfo) {
  _preamble     = preamble;
  _faceHeader   = faceHeader;
  _glyphInfo    = glyphInfo;
  _glyphPresent = true;

  _glyphBitmapPos =
      QPoint((bitmapWidth - bitmap.dim.width) / 2, (bitmapHeight - bitmap.dim.height) / 2);
  _glyphOriginPos = QPoint(_glyphBitmapPos.x() + _glyphInfo.horizontalOffset,
                           1 + _glyphBitmapPos.y() + _glyphInfo.verticalOffset);
  clearAndEmit();
  loadBitmap(bitmap);
}

void BitmapRenderer::clearAndReloadBitmap(const IBMFDefs::Bitmap &bitmap) {
  clearAndEmit();
  loadBitmap(bitmap);
}

void BitmapRenderer::loadBitmap(const IBMFDefs::Bitmap &bitmap) {

  if ((_glyphBitmapPos.x() < 0) || (_glyphBitmapPos.y() < 0)) {
    QMessageBox::warning(this, "Internal error",
                         "The Glyph's bitmap is too large for the application capability. Largest "
                         "bitmap width/height is set to 200 pixels.");
    return;
  }

  int idx;
  int rowp;
  for (int row = _glyphBitmapPos.y(), rowp = row * bitmapWidth, idx = 0;
       row < _glyphBitmapPos.y() + bitmap.dim.height; row++, rowp += bitmapWidth) {
    for (int col = _glyphBitmapPos.x(); col < _glyphBitmapPos.x() + bitmap.dim.width;
         col++, idx++) {
      _displayBitmap[rowp + col] = (bitmap.pixels[idx] == 0) ? PixelType::WHITE : PixelType::BLACK;
    }
  }

  _bitmapChanged = false;

  this->repaint();
}

bool BitmapRenderer::retrieveBitmap(IBMFDefs::Bitmap **bitmap) {
  QPoint topLeft;
  QPoint bottomRight;

  int row;
  int col;

  bool stop = false;
  int  idx;
  for (row = 0, idx = 0; row < bitmapHeight; row++) {
    for (col = 0; col < bitmapWidth; col++, idx++) {
      stop = _displayBitmap[idx] != PixelType::WHITE;
      if (stop) break;
    }
    if (stop) break;
  }

  if (row >= bitmapHeight) return false; // The bitmap is empty of black pixels

  topLeft.setY(row);

  stop = false;
  int rowp;
  for (row = bitmapHeight - 1, rowp = (bitmapHeight - 1) * bitmapWidth; row >= 0;
       row--, rowp -= bitmapWidth) {
    for (col = 0; col < bitmapWidth; col++) {
      stop = _displayBitmap[rowp + col] != PixelType::WHITE;
      if (stop) break;
    }
    if (stop) break;
  }
  bottomRight.setY(row);

  stop = false;
  for (col = 0; col < bitmapWidth; col++) {
    for (row = 0, rowp = 0; row < bitmapHeight; row++, rowp += bitmapWidth) {
      stop = _displayBitmap[rowp + col] != PixelType::WHITE;
      if (stop) break;
    }
    if (stop) break;
  }
  topLeft.setX(col);

  stop = false;
  for (col = bitmapWidth - 1; col >= 0; col--) {
    for (row = 0, rowp = 0; row < bitmapHeight; row++, rowp += bitmapWidth) {
      stop = _displayBitmap[rowp + col] != PixelType::WHITE;
      if (stop) break;
    }
    if (stop) break;
  }
  bottomRight.setX(col);

  IBMFDefs::Bitmap *theBitmap = new IBMFDefs::Bitmap;
  theBitmap->dim =
      IBMFDefs::Dim(bottomRight.x() - topLeft.x() + 1, bottomRight.y() - topLeft.y() + 1);
  int size          = theBitmap->dim.width * theBitmap->dim.height;
  theBitmap->pixels = IBMFDefs::Pixels(size, 0);

  idx               = 0;
  for (row = topLeft.y(), rowp = row * bitmapWidth; row <= bottomRight.y();
       row++, rowp += bitmapWidth) {
    for (col = topLeft.x(); col <= bottomRight.x(); col++) {
      theBitmap->pixels[idx++] = _displayBitmap[rowp + col] == PixelType::WHITE ? 0 : 0xff;
    }
  }

  *bitmap = theBitmap;

  return true;
}
