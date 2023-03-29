#pragma once

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPoint>
#include <QScrollBar>
#include <QUndoStack>
#include <QWidget>

#include "IBMFDriver/IBMFDefs.hpp"

class BitmapRenderer : public QWidget {
  Q_OBJECT

public:
  static const int bitmapWidth  = 200;
  static const int bitmapHeight = 200;

  enum PixelType : uint8_t { WHITE, BLACK };

  BitmapRenderer(QWidget *parent = 0, int pixel_size = 20, bool no_scroll = false,
                 QUndoStack *undoStack_ = nullptr);
  bool retrieveBitmap(IBMFDefs::BitmapPtr *bitmap, QPoint *offsets = nullptr);
  void clearAndEmit(bool repaint_after = false);
  bool changed() { return bitmapChanged_; }
  void setPixelSize(int pixel_size);
  void connectTo(BitmapRenderer *renderer);
  int  getPixelSize();
  void setBitmapOffsetPos(QPoint pos);
  void setAdvance(IBMFDefs::FIX16 newAdvance);

  void paintPixel(PixelType pixelType,
                  QPoint    atPos); // pixel rendering suport for undo/redo

public slots:
  void clearAndLoadBitmap(const IBMFDefs::Bitmap &bitmap, const IBMFDefs::Preamble &preamble,
                          const IBMFDefs::FaceHeader &faceHeader,
                          const IBMFDefs::GlyphInfo  &glyphInfo);
  void clearAndReloadBitmap(const IBMFDefs::Bitmap &bitmap, const QPoint &originOffsets);
  void clearAndRepaint();

signals:
  void bitmapHasChanged(const IBMFDefs::Bitmap &bitmap, const QPoint &originOffsets);
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

  QUndoStack   *undoStack_;     // The master undo stack as received from the main window
  bool          bitmapChanged_; // True if some pixel modified on screen
  bool          glyphPresent_;  // True if there is a glyph shown on screen
  int           pixelSize_;     // How large a glyph pixel will appear on screen
  DisplayBitmap displayBitmap_; // Each entry correspond to one pixel of a glyph
  bool          wasBlack_;      // used by mouse events to permit sequence of pixels drawing
                                // through mouse move
  bool editable_;               // Only the main renderer is editable with lines delimiting
                                // pixels on screen
  bool noScroll_;               // True for all secondary BitmapRenderer. No scroll bar will
                                // appear on screen
  QPoint lastPos_;              // The last position on the displayBitmap received by a Mouse
                                // Event
  QPoint bitmapOffsetPos_;      // Offset of the displayBitmap upper left corner as
                                // whown on screen
  QPoint glyphBitmapPos_;       // Upper left position of the glyph bitmap on the
                                // displayBitmap
  QPoint glyphOriginPos_;       // Origin position of the glyph bitmap on the
                                // displayBitmap

  IBMFDefs::Preamble   preamble_;   // Copies of the font structure related to the current glyph
  IBMFDefs::FaceHeader faceHeader_; // idem
  IBMFDefs::GlyphInfo  glyphInfo_;  // idem
};
