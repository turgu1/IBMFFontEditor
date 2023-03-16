#pragma once

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPoint>
#include <QScrollBar>
#include <QUndoStack>
#include <QWidget>

#include "IBMFDriver/ibmf_defs.hpp"

class BitmapRenderer : public QWidget {
  Q_OBJECT

public:
  static const int bitmapWidth  = 200;
  static const int bitmapHeight = 200;

  enum PixelType : uint8_t { WHITE, BLACK };

  BitmapRenderer(QWidget *parent = 0, int pixel_size = 20, bool no_scroll = false,
                 QUndoStack *_undoStack = nullptr);
  bool retrieveBitmap(IBMFDefs::Bitmap **bitmap);
  void clearAndEmit(bool repaint_after = false);
  bool changed() { return _bitmapChanged; }
  void setPixelSize(int pixel_size);
  void connectTo(BitmapRenderer *renderer);
  int  getPixelSize();
  void setBitmapOffsetPos(QPoint pos);

  void paintPixel(PixelType pixelType, QPoint atPos); // pixel rendering suport for undo/redo

public slots:
  void clearAndLoadBitmap(const IBMFDefs::Bitmap &bitmap, const IBMFDefs::Preamble &preamble,
                          const IBMFDefs::FaceHeader &faceHeader,
                          const IBMFDefs::GlyphInfo  &glyphInfo);
  void clearAndReloadBitmap(const IBMFDefs::Bitmap &bitmap);
  void clearAndRepaint();

signals:
  void bitmapHasChanged(IBMFDefs::Bitmap &bitmap);
  void bitmapCleared();

protected:
  void paintEvent(QPaintEvent *event);
  void mousePressEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void resizeEvent(QResizeEvent *event);
  void wheelEvent(QWheelEvent *event);

private:
  void setScreenPixel(QPoint pos);
  void loadBitmap(const IBMFDefs::Bitmap &bitmap);
  void clearBitmap();

  typedef PixelType DisplayBitmap[bitmapWidth * bitmapHeight];

  QUndoStack   *_undoStack;     // The master undo stack as received from the main window
  bool          _bitmapChanged; // True if some pixel modified on screen
  bool          _glyphPresent;  // True if there is a glyph shown on screen
  int           _pixelSize;     // How large a glyph pixel will appear on screen
  DisplayBitmap _displayBitmap; // Each entry correspond to one pixel of a glyph
  bool   _wasBlack; // used by mouse events to permit sequence of pixels drawing through mouse move
  bool   _editable; // Only the main renderer is editable with lines delimiting pixels on screen
  bool   _noScroll; // True for all secondary BitmapRenderer. No scroll bar will appear on screen
  QPoint _lastPos;  // The last position on the displayBitmap received by a Mouse Event
  QPoint _bitmapOffsetPos; // Offset of the displayBitmap upper left corner as whown on screen
  QPoint _glyphBitmapPos;  // Upper left position of the glyph bitmap on the displayBitmap
  QPoint _glyphOriginPos;  // Origin position of the glyph bitmap on the displayBitmap

  IBMFDefs::Preamble   _preamble;   // Copies of the font structure related to the current glyph
  IBMFDefs::FaceHeader _faceHeader; // idem
  IBMFDefs::GlyphInfo  _glyphInfo;  // idem
};
