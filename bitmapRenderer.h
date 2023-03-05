#ifndef BITMAPRENDERER_H
#define BITMAPRENDERER_H

#include <QWidget>
#include <QPen>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QPoint>
#include <QScrollBar>

#include "ibmf_defs.hpp"

class BitmapRenderer : public QWidget
{
    Q_OBJECT

public:

    static const int BITMAP_WIDTH  = 200;
    static const int BITMAP_HEIGHT = 200;

    BitmapRenderer(QWidget *parent = 0, int pixel_size = 20, bool no_scroll = false);
    bool retrieveBitmap(IBMFDefs::Bitmap **bitmap);
    void clear_and_emit(bool repaint_after = false);
    bool changed() { return bitmapChanged; }
    void setPixelSize(int pixel_size);
    void connectTo(BitmapRenderer * renderer);
    int  getPixelSize();
    void setBitmapOffsetPos(QPoint pos);

public slots:
    void clearAndLoadBitmap(IBMFDefs::Bitmap &bitmap);
    void clear_and_repaint();

signals:
    void bitmapHasChanged(IBMFDefs::Bitmap & bitmap);
    void bitmapCleared();

protected:
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void resizeEvent(QResizeEvent *event);

private:
    void setScreenPixel(QPoint pos);
    void loadBitmap(IBMFDefs::Bitmap & bitmap);
    void clearBitmap();

    typedef char DisplayBitmap[BITMAP_WIDTH * BITMAP_HEIGHT];

    bool           bitmapChanged;
    int            pixelSize;
    DisplayBitmap  displayBitmap;
    bool           wasBlack;       // used by mouse events to permit sequence of pixels drawing through mouse move
    bool           editable;
    bool           noScroll;
    QPoint         lastPos;
    QPoint         bitmapOffsetPos;
};

#endif // BITMAPRENDERER_H
