#include "IBMFFontMod.hpp"

#include "freeType.h"

class IBMFTTFImport : public IBMFFontMod {

private:
#pragma pack(push, 1)
  struct KernTableHeader {
    uint16_t version;
    uint16_t nTables; // number of sub tables
  } kernTableHeader;

  // big-endien...
  union KernCoverage {
    struct { // already in little-endien pos
      uint8_t format                     : 8;
      bool    isHorizontal               : 1;
      bool    isOverride                 : 1;
      bool    isCrossStreamPerpendicular : 1;
      bool    hasMinimumValues           : 1;
      uint8_t reserved                   : 4;
    } data;
    struct {
      uint16_t value;
    } whole;
  };

  struct KernSubTableHeader {
    uint16_t     version;
    uint16_t     length; // in bytes, including this header
    KernCoverage coverage;
    uint16_t     nPairs; // Number of pairs (Normally part of the KernFormat0Header...
  } kernSubTableHeader;

  struct KernFormat0Header {
    uint16_t searchRange;
    uint16_t entrySelector;
    uint16_t rangeShift;
  } kernFormat0Header;

  struct KernPair {
    uint16_t first; // First char Index
    uint16_t next;  // Next char Index
    int16_t  value; // In Font Units
  };
  typedef KernPair *KernPairsPtr;
  KernPairsPtr      kernPairs;
  int               kernPairsCount;

#pragma pack(pop)

  auto charSelected(char32_t ch, SelectedBlockIndexesPtr &selectedBlockIndexes) const -> bool;
  auto prepareCodePlanes(FT_Face &face, CharSelections &charSelections) -> int;
  auto retrieveKernPairsTable(FT_Face ftFace) -> void;
  auto findGlyphCodeFromIndex(int index, FT_Face ftFace, int glyphCount) -> GlyphCode;

public:
  IBMFTTFImport() : IBMFFontMod() {}

  auto loadTTF(FreeType &ft, FontParametersPtr fontParameters) -> bool;
};

typedef std::shared_ptr<IBMFTTFImport> IBMFTTFImportPtr;
