#pragma once

#include <QPainter>
#include <QSize>

#include "IBMFDriver/ibmf_defs.hpp"
using namespace IBMFDefs;

class KerningItem {
public:
  enum class EditMode { Editable, ReadOnly };

  explicit KerningItem(GlyphCode glyphCode, GlyphCode nextCode, FIX16 kern);

  void  paint(QPainter *painter, const QRect &rect, const QPalette &palette, EditMode mode) const;
  QSize sizeHint() const;

  GlyphCode glyphCode() const { return myGlyphCode; }
  GlyphCode nextCode() const { return myNextCode; }
  FIX16     kern() const { return myKern; }

  void setGlyphCode(GlyphCode glyphCode) { myGlyphCode = glyphCode; }
  void setNextCode(GlyphCode nextCode) { myNextCode = nextCode; }
  void setKern(FIX16 kern) { myKern = kern; }

private:
  GlyphCode myGlyphCode;
  GlyphCode myNextCode;
  FIX16     myKern;
};

Q_DECLARE_METATYPE(KerningItem)
