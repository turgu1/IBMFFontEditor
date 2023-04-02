#pragma once

#include <QPainter>
#include <QSize>
#include <QWidget>

#include "IBMFDriver/IBMFFontMod.hpp"

#define AUTO_KERNING 0
#define KERNING_SIZE 1

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
  void setKernFactor(float value);
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
    FIX16                  kern;
  };

  std::vector<OneGlyph> word_;

  QString        textToDraw_;
  IBMFFontModPtr font_;
  int            faceIdx_;
  bool           autoKerning_{false};
  bool           normalKerning_{false};
  int            pixelSize_{1};
  float          kernFactor_{1.0};
  int            wordLength_{0};
  QSize          requiredSize_{QSize(0, 0)};
  QPoint         pos_{QPoint(0, 0)};

  IBMFDefs::GlyphCode    bypassGlyphCode_{IBMFDefs::NO_GLYPH_CODE};
  IBMFDefs::BitmapPtr    bypassBitmap_{nullptr};
  IBMFDefs::GlyphInfoPtr bypassGlyphInfo_{nullptr};

  auto computeAutoKerning(const BitmapPtr b1, const BitmapPtr b2, const GlyphInfo &i1,
                          const GlyphInfo &i2) const -> FIX16;
  auto paintWord(QPainter *painter, int lineHeight) -> void;
  auto computeSize() -> void;
};
