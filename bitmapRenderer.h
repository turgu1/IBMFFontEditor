#pragma once

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPoint>
#include <QScrollBar>
#include <QSize>
#include <QUndoStack>
#include <QVector>
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

  auto getSelection(QRect *atLocation = nullptr) const -> IBMFDefs::BitmapPtr;
  auto pasteSelection(IBMFDefs::BitmapPtr selection, QPoint *atPos) -> void;
  auto getSelectionLocation() -> QPoint *;

public slots:
  void clearAndLoadBitmap(const IBMFDefs::Bitmap &bitmap, const IBMFDefs::FaceHeader &faceHeader,
                          const IBMFDefs::GlyphInfo &glyphInfo);
  void clearAndReloadBitmap(const IBMFDefs::Bitmap &bitmap, const QPoint &originOffsets);
  void clearAndRepaint();

signals:
  void bitmapHasChanged(const IBMFDefs::Bitmap &bitmap, const QPoint &originOffsets);
  void bitmapCleared();
  void someSelection(bool some);
  void keyPressed(QKeyEvent *event);

protected:
  void paintEvent(QPaintEvent *event);
  void mousePressEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void resizeEvent(QResizeEvent *event);
  void wheelEvent(QWheelEvent *event);
  void keyPressEvent(QKeyEvent *event);

private:
  void setScreenPixel(QPoint pos);
  void loadBitmap(const IBMFDefs::Bitmap &bitmap);
  void clearBitmap();

  typedef PixelType DisplayBitmap[bitmapWidth * bitmapHeight];

  int  pixelSize_;        // How large a glyph pixel will appear on screen
  bool noScroll_;         // True for all secondary BitmapRenderer. No scroll bar will
                          // appear on screen
  QUndoStack *undoStack_; // The master undo stack as received from the main window

  bool bitmapChanged_{false};    // True if some pixel modified on screen
  bool glyphPresent_{false};     // True if there is a glyph shown on screen
  bool wasBlack_{true};          // used by mouse events to permit sequence of pixels drawing
                                 // through mouse move
  bool editable_{true};          // Only the main renderer is editable with lines delimiting
                                 // pixels on screen
  QPoint lastPos_{QPoint(0, 0)}; // The last position on the displayBitmap received by a Mouse
                                 // Event
  QPoint bitmapOffsetPos_{QPoint(0, 0)}; // Offset of the displayBitmap upper left corner as
                                         // shown on screen
  QPoint glyphBitmapPos_{QPoint(0, 0)};  // Upper left position of the glyph bitmap on the
                                         // displayBitmap
  QPoint glyphOriginPos_{QPoint(0, 0)};  // Origin position of the glyph bitmap on the
                                         // displayBitmap

  QPoint selectionStartPos_{QPoint(0, 0)};
  QPoint selectionEndPos_{QPoint(0, 0)};
  bool   selectionStarted_{false};
  bool   selectionCompleted_{false};

  DisplayBitmap        displayBitmap_; // Each entry correspond to one pixel of a glyph
  IBMFDefs::FaceHeader faceHeader_;    // idem
  IBMFDefs::GlyphInfo  glyphInfo_;     // idem
};
