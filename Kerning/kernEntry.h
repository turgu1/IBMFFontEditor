#pragma once

#include <QMetaType>

#include "../IBMFDriver/IBMFDefs.hpp"

struct KernEntry {
  IBMFDefs::GlyphCode glyphCode{0};
  IBMFDefs::GlyphCode nextGlyphCode{0};
  float               kern{0.0};
  KernEntry(IBMFDefs::GlyphCode code, IBMFDefs::GlyphCode nextCode, float kern)
      : glyphCode(code), nextGlyphCode(nextCode), kern(kern) {}
  KernEntry()  = default;
  ~KernEntry() = default;
};

Q_DECLARE_METATYPE(KernEntry)
