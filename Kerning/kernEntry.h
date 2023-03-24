#pragma once

#include <QMetaType>

#include "../IBMFDriver/ibmf_defs.hpp"

struct KernEntry {
  IBMFDefs::GlyphCode glyphCode;
  IBMFDefs::GlyphCode nextGlyphCode;
  float               kern;
  KernEntry(IBMFDefs::GlyphCode code, IBMFDefs::GlyphCode nextCode, float kern)
      : glyphCode(code), nextGlyphCode(nextCode), kern(kern) {}
  KernEntry() : glyphCode(0), nextGlyphCode(0), kern(0.0) {}
  ~KernEntry() {}
};
Q_DECLARE_METATYPE(KernEntry)
