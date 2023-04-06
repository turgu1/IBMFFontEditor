#pragma once

#include "IBMFFontMod.hpp"

class IBMFHexImport : public IBMFFontMod {
public:
  IBMFHexImport() : IBMFFontMod() {}

  auto readOneGlyph(QDataStream &inStream, char32_t &codePoint, Bitmap &bitmap) -> bool;
  auto loadHex(QDataStream &inStream, FontParametersPtr fontParameters) -> bool;
};
