#ifndef BITMAPRENDERER_H
#define BITMAPRENDERER_H

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPoint>
#include <QScrollBar>
#include <QWidget>

#include "IBMFDriver/ibmf_defs.hpp"

class BitmapRenderer : public QWidget {
  Q_OBJECT

public:
  static const int bitmapWidth  = 200;
  static const int bitmapHeight = 200;

  BitmapRenderer(QWidget *parent = 0, int pixel_size = 20, bool no_scroll = false);
  bool retrieveBitmap(IBMFDefs::Bitmap **bitmap);
  void clearAndEmit(bool repaint_after = false);
  bool changed() { return bitmapChanged; }
  void setPixelSize(int pixel_size);
  void connectTo(BitmapRenderer *renderer);
  int  getPixelSize();
  void setBitmapOffsetPos(QPoint pos);

public slots:
  void clearAndLoadBitmap(const IBMFDefs::Bitmap &bitmap, const IBMFDefs::Preamble &preamble,
                          const IBMFDefs::FaceHeader &faceHeader,
                          const IBMFDefs::GlyphInfo  &glyphInfo);
  void clearAndLoadBitmap2(const IBMFDefs::Bitmap &bitmap);
  void clearAndRepaint();

signals:
  void bitmapHasChanged(IBMFDefs::Bitmap &bitmap);
  void bitmapCleared();

protected:
  void paintEvent(QPaintEvent *event);
  void mousePressEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void resizeEvent(QResizeEvent *event);

private:
  void setScreenPixel(QPoint pos);
  void loadBitmap(const IBMFDefs::Bitmap &bitmap);
  void clearBitmap();

  typedef char DisplayBitmap[bitmapWidth * bitmapHeight];

  bool          bitmapChanged;
  bool          glyphPresent;
  int           pixelSize;
  DisplayBitmap displayBitmap;
  bool   wasBlack; // used by mouse events to permit sequence of pixels drawing through mouse move
  bool   editable;
  bool   noScroll;
  QPoint lastPos;
  QPoint bitmapOffsetPos;
  QPoint glyphBitmapPos;

  IBMFDefs::Preamble   preamble;
  IBMFDefs::FaceHeader faceHeader;
  IBMFDefs::GlyphInfo  glyphInfo;
};

#endif // BITMAPRENDERER_H
