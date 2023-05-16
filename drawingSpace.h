#pragma once

#include <QPainter>
#include <QSize>
#include <QWidget>

#include "IBMFDriver/IBMFFontMod.hpp"

#define KERNING_SIZE 1

class DrawingSpace : public QWidget {
  Q_OBJECT
public:
  explicit DrawingSpace(IBMFFontModPtr font = nullptr, int faceIdx = 0, QWidget *parent = nullptr);
  void drawScreen(QPainter *painter);

  void newDrawScreen(QPainter *painter);

  void setText(QString text);
  void setOpticalKerning(bool value);
  void setNormalKerning(bool value);
  void setPixelSize(int value);
  void setFont(IBMFFontModPtr font);
  void setFaceIdx(int faceIdx);
  void setBypassGlyph(IBMFDefs::GlyphCode glyphCode, IBMFDefs::BitmapPtr bitmap,
                      IBMFDefs::GlyphInfoPtr glyphInfo, GlyphLigKernPtr glyphLigKern);

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
  typedef std::shared_ptr<OneGlyph> OneGlyphPtr;

  typedef std::vector<OneGlyphPtr> Word;
  typedef std::shared_ptr<Word>    WordPtr;

  typedef std::vector<WordPtr> Line;

  Line line_;
  int  linePixelWidth_;
  int  spaceSize_;

  QString        textToDraw_;
  IBMFFontModPtr font_;
  int            faceIdx_;
  bool           opticalKerning_{false};
  bool           normalKerning_{false};
  int            pixelSize_{1};
  int            wordLength_{0};
  QSize          requiredSize_{QSize(0, 0)};
  QPoint         pos_{QPoint(0, 0)};

  IBMFDefs::GlyphCode       bypassGlyphCode_{IBMFDefs::NO_GLYPH_CODE};
  IBMFDefs::BitmapPtr       bypassBitmap_{nullptr};
  IBMFDefs::GlyphInfoPtr    bypassGlyphInfo_{nullptr};
  IBMFDefs::GlyphLigKernPtr bypassGlyphLigKern_{nullptr};

  auto computeOpticalKerning(const BitmapPtr b1, const BitmapPtr b2, const GlyphInfoPtr i1,
                             const GlyphInfoPtr i2) const -> FIX16;
  auto paintWord(QPainter *painter, int lineHeight) -> void;
  auto computeSize() -> void;

  auto printLine() -> void;
  auto addWordToLine(QString &w) -> void;
};
