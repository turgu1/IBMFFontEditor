#pragma once

#include <QWidget>

#include "IBMFDriver/ibmf_font_mod.hpp"

#define AUTO_KERNING 0
#define KERNING_SIZE 2

class DrawingSpace : public QWidget {
  Q_OBJECT
public:
  explicit DrawingSpace(IBMFFontModPtr font, int faceIdx, QWidget *parent = nullptr);

  void setText(QString text);
  void setAutoKerning(bool value);
  void setNormalKerning(bool value);
  void setPixelSize(int value);

signals:

protected:
  void paintEvent(QPaintEvent *event);
  void resizeEvent(QResizeEvent *event);

private:
  QString        textToDraw_;
  IBMFFontModPtr font_;
  int            faceIdx_;
  bool           autoKerning_;
  bool           normalKerning_;
  int            pixelSize_;

  int computeAutoKerning(IBMFDefs::Bitmap &b1, IBMFDefs::Bitmap &b2, IBMFDefs::GlyphInfo &i1,
                         IBMFDefs::GlyphInfo &i2);
};
