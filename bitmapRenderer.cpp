#include "bitmapRenderer.h"
#include "qwidget.h"

#include <QMessageBox>

BitmapRenderer::BitmapRenderer(QWidget *parent, int pixel_size, bool no_scroll)
    : QWidget(parent)
{
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);
  bitmapChanged = false;
  pixelSize = pixel_size;
  wasBlack = true;
  editable = true;
  noScroll = no_scroll;
  bitmapOffsetPos = QPoint(0, 0);
  clearBitmap();
}

void BitmapRenderer::resizeEvent(QResizeEvent *event) {
  if (noScroll) {
    bitmapOffsetPos = QPoint(
      (BITMAP_WIDTH - (width() / pixelSize)) / 2,
      (BITMAP_HEIGHT - (height() / pixelSize)) / 2
    );
    if (bitmapOffsetPos.x() < 0) bitmapOffsetPos.setX(0);
    if (bitmapOffsetPos.y() < 0) bitmapOffsetPos.setY(0);
  }
}

int BitmapRenderer::getPixelSize() { return pixelSize; }

void BitmapRenderer::connectTo(BitmapRenderer * main_renderer)
{
  QObject::connect(main_renderer, &BitmapRenderer::bitmapHasChanged, this, &BitmapRenderer::clearAndLoadBitmap);
  QObject::connect(main_renderer, &BitmapRenderer::bitmapCleared,    this, &BitmapRenderer::clear_and_repaint);
  editable = false;
}

void BitmapRenderer::clearBitmap()
{
  memset(displayBitmap, 0, sizeof(displayBitmap));
}

void BitmapRenderer::clear_and_repaint()
{
  clearBitmap();
  repaint();
}

void BitmapRenderer::clear_and_emit(bool repaint_after)
{
  clearBitmap();
  if (repaint_after) repaint();
  emit bitmapCleared();
}

void BitmapRenderer::setPixelSize(int pixel_size)
{
  Bitmap * bitmap;

  pixelSize = pixel_size;
  if (retrieveBitmap(&bitmap)) {
    clearAndLoadBitmap(*bitmap);
    delete bitmap;
  }
  else {
    repaint();
  }
}

void BitmapRenderer::setBitmapOffsetPos(QPoint pos)
{
    if ((pos.x() < 0) || (pos.x() >= BITMAP_WIDTH) ||
        (pos.y() < 0) || (pos.y() >= BITMAP_HEIGHT)) {
        QMessageBox::warning(this, "Internal error", "setBitmapOffsetPos() received a bad position!");
    }
    if ((pos.x() != bitmapOffsetPos.x()) || (pos.y() != bitmapOffsetPos.y())) {
      bitmapOffsetPos = pos;
      repaint();
    }
}

void BitmapRenderer::setScreenPixel(QPoint pos)
{
  QPainter painter(this);
  QRect rect;

  painter.setPen(QPen(QBrush(QColorConstants::DarkGray), 1));
  painter.setBrush(QBrush(QColorConstants::DarkGray));

  if (pixelSize >= 10) {
    rect = QRect(
      (pos.x() - bitmapOffsetPos.x()) * pixelSize + 2,
      (pos.y() - bitmapOffsetPos.y()) * pixelSize + 2,
      pixelSize - 4,
      pixelSize - 4);
  }
  else {
    rect = QRect(
      (pos.x() - bitmapOffsetPos.x()) * pixelSize,
      (pos.y() - bitmapOffsetPos.x()) * pixelSize,
      pixelSize,
      pixelSize);
  }
  painter.drawRect(rect);
}

void BitmapRenderer::paintEvent(QPaintEvent * /* event */)
{
  QPainter painter(this);

  painter.setPen(QPen(QBrush(QColorConstants::LightGray), 1));
  painter.setBrush(QBrush(QColorConstants::Red));

  if (pixelSize >= 10) {
    for (int row = pixelSize; row < height(); row += pixelSize) {
      for (int col = pixelSize; col < width(); col += pixelSize) {
        painter.drawLine(QPoint(col, 0), QPoint(col, height()));
        painter.drawLine(QPoint(0, row), QPoint(width(), row));
      }
    }
  }

  int rowp;
  for (int row = bitmapOffsetPos.y(), rowp = row * BITMAP_WIDTH;
       row < BITMAP_HEIGHT;
       row ++, rowp += BITMAP_WIDTH) {
    for (int col = bitmapOffsetPos.x(); col < BITMAP_WIDTH; col++) {
      if (displayBitmap[rowp + col] == 1) {
        setScreenPixel(QPoint(col, row));
      }
    }
  }

}

void BitmapRenderer::mousePressEvent(QMouseEvent *event)
{
  if (editable) {
    lastPos = QPoint(bitmapOffsetPos.x() + event->pos().x() / pixelSize, bitmapOffsetPos.y() + event->pos().y() / pixelSize);
    if ((lastPos.x() < BITMAP_WIDTH) && (lastPos.y() < BITMAP_HEIGHT)) {
      int idx = lastPos.y() * BITMAP_WIDTH + lastPos.x();
      if (displayBitmap[idx] == 1) {
        displayBitmap[idx] = 0;
        wasBlack = false;
      }
      else {
        displayBitmap[idx] = 1;
        wasBlack = true;
      }
      repaint();

      Bitmap * theBitmap;
      if (retrieveBitmap(&theBitmap)) {
        emit bitmapHasChanged(*theBitmap);
      }
      else {
        emit bitmapCleared();
      }
    }
  }
}

void BitmapRenderer::mouseMoveEvent(QMouseEvent *event)
{
  if (editable) {
    QPoint pos = QPoint(bitmapOffsetPos.x() + event->pos().x() / pixelSize, bitmapOffsetPos.y() + event->pos().y() / pixelSize);
    int idx = pos.y() * BITMAP_WIDTH + pos.x();
    if ((pos.x() < BITMAP_WIDTH) &&
        (pos.y() < BITMAP_HEIGHT) &&
        ((pos.x() != lastPos.x()) ||
         (pos.y() != lastPos.y()))) {
      displayBitmap[idx] = wasBlack ? 1 : 0;

      repaint();

      lastPos.setX(pos.x());
      lastPos.setY(pos.y());

      Bitmap * theBitmap;
      if (retrieveBitmap(&theBitmap)) {
        emit bitmapHasChanged(*theBitmap);
      }
      else {
        emit bitmapCleared();
      }
    }
  }
}

void BitmapRenderer::clearAndLoadBitmap(Bitmap & bitmap)
{
  clear_and_emit();
  loadBitmap(bitmap);
}

void BitmapRenderer::loadBitmap(Bitmap & bitmap)
{
  QPoint pos((BITMAP_WIDTH - bitmap.dim.width) / 2, (BITMAP_HEIGHT - bitmap.dim.height) / 2);

  if ((pos.x() < 0) || (pos.y() < 0)) {
    QMessageBox::warning(this, "Internal error", "The Glyph's bitmap is too large for the application capability. Largest bitmap width/height is set to 200 pixels.");
    return;
  }

  int idx;
  int rowp;
  for (int row = pos.y(), rowp = row * BITMAP_WIDTH, idx = 0;
       row < pos.y() + bitmap.dim.height;
       row++, rowp += BITMAP_WIDTH) {
    for (int col = pos.x();
         col < pos.x() + bitmap.dim.width;
         col++, idx++) {
      displayBitmap[rowp + col] = (bitmap.pixels[idx] == 0) ? 0 : 1;
    }
  }

  bitmapChanged = false;

  this->repaint();
}

bool BitmapRenderer::retrieveBitmap(Bitmap ** bitmap)
{
  QPoint topLeft;
  QPoint bottomRight;

  int row;
  int col;

  bool stop = false;
  int idx;
  for (row = 0, idx = 0; row < BITMAP_HEIGHT; row++) {
    for (col = 0; col < BITMAP_WIDTH; col++, idx++) {
      stop = displayBitmap[idx] != 0;
      if (stop) break;
    }
    if (stop) break;
  }

  if (row >= BITMAP_HEIGHT) return false; // The bitmap is empty of black pixels

  topLeft.setY(row);

  stop = false;
  int rowp;
  for (row = BITMAP_HEIGHT - 1, rowp = (BITMAP_HEIGHT - 1) * BITMAP_WIDTH;
       row >= 0;
       row--, rowp -= BITMAP_WIDTH) {
    for (col = 0; col < BITMAP_WIDTH; col++) {
      stop = displayBitmap[rowp + col] != 0;
      if (stop) break;
    }
    if (stop) break;
  }
  bottomRight.setY(row);

  stop = false;
  for (col = 0; col < BITMAP_WIDTH; col++) {
    for (row = 0, rowp = 0; row < BITMAP_HEIGHT; row++, rowp += BITMAP_WIDTH) {
      stop = displayBitmap[rowp + col] != 0;
      if (stop) break;
    }
    if (stop) break;
  }
  topLeft.setX(col);

  stop = false;
  for (col = BITMAP_WIDTH - 1; col >= 0; col--) {
    for (row = 0, rowp = 0; row < BITMAP_HEIGHT; row++, rowp += BITMAP_WIDTH) {
        stop = displayBitmap[rowp + col] != 0;
        if (stop) break;
    }
    if (stop) break;
  }
  bottomRight.setX(col);

  Bitmap * theBitmap = new Bitmap;
  theBitmap->dim = Dim(bottomRight.x() - topLeft.x() + 1, bottomRight.y() - topLeft.y() + 1);
  int size   = theBitmap->dim.width * theBitmap->dim.height;
  theBitmap->pixels = Pixels(size, 0);

  idx = 0;
  for (row = topLeft.y(), rowp = row * BITMAP_WIDTH; row <= bottomRight.y(); row++, rowp += BITMAP_WIDTH) {
    for (col = topLeft.x(); col <= bottomRight.x(); col++) {
      theBitmap->pixels[idx++] = displayBitmap[rowp + col] == 0 ? 0 : 0xff;
    }
  }

  *bitmap = theBitmap;

  return true;
}
