#pragma once

#include <QPainter>
#include <QWidget>

#include "IBMFDriver/ibmf_font_mod.hpp"

#define AUTO_KERNING 0
#define KERNING_SIZE 2

class DrawingSpace : public QWidget {
  Q_OBJECT
public:
  explicit DrawingSpace(IBMFFontModPtr font = nullptr, int faceIdx = 0, QWidget *parent = nullptr);
  void drawScreen(QPainter *painter);

  void setText(QString text);
  void setAutoKerning(bool value);
  void setNormalKerning(bool value);
  void setPixelSize(int value);
  void setFont(IBMFFontModPtr font);
  void setFaceIdx(int faceIdx);

signals:

protected:
  void paintEvent(QPaintEvent *event);
  void resizeEvent(QResizeEvent *event);

private:
  struct OneGlyph {
    IBMFDefs::BitmapPtr    bitmap;
    IBMFDefs::GlyphInfoPtr glyphInfo;
    int                    kern;
  };

  std::vector<OneGlyph> word_;

  QString        textToDraw_;
  IBMFFontModPtr font_;
  int            faceIdx_;
  bool           autoKerning_;
  bool           normalKerning_;
  int            pixelSize_;
  int            wordLength_;
  bool           resizing_;

  int  computeAutoKerning(IBMFDefs::Bitmap &b1, IBMFDefs::Bitmap &b2, IBMFDefs::GlyphInfo &i1,
                          IBMFDefs::GlyphInfo &i2);
  void paintWord(QPainter *painter, QPoint &pos, int lineHeight);
  void incrementLineNbr();
};
