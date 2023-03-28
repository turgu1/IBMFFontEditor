#pragma once

#include <QPainter>
#include <QSize>
#include <QWidget>

#include "IBMFDriver/IBMFFontMod.hpp"

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
  void setBypassGlyph(IBMFDefs::GlyphCode glyphCode, IBMFDefs::BitmapPtr bitmap,
                      IBMFDefs::GlyphInfoPtr glyphInfo);

signals:

protected:
  void  paintEvent(QPaintEvent *event) override;
  QSize sizeHint() const override;

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
  QSize          requiredSize_;
  QPoint         pos_;

  IBMFDefs::GlyphCode    bypassGlyphCode_;
  IBMFDefs::BitmapPtr    bypassBitmap_;
  IBMFDefs::GlyphInfoPtr bypassGlyphInfo_;

  auto computeAutoKerning(const BitmapPtr b1, const BitmapPtr b2, const GlyphInfo &i1,
                          const GlyphInfo &i2) const -> int;
  auto paintWord(QPainter *painter, int lineHeight) -> void;
  auto computeSize() -> void;
};
