#pragma once

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>
#include <vector>

#include "IBMFDefs.hpp"

using namespace IBMFDefs;

#include <QDataStream>

#include "../Kerning/kerningModel.h"
#include "RLEExtractor.hpp"
#include "RLEGenerator.hpp"

#define DEBUG 0

#if DEBUG
  #include <iomanip>
  #include <iostream>
#endif

/**
 * @brief Access to a IBMF font.
 *
 * This is a class to allow for the modification of a IBMF font generated from
 * METAFONT
 *
 */
class IBMFFontMod {
public:
  struct Face {
    FaceHeaderPtr                   header;
    std::vector<GlyphInfoPtr>       glyphs;       // Not used with BAKCUP format
    std::vector<GlyphBackupInfoPtr> glyphsBackup; // Only used with BACKUP format
    std::vector<BitmapPtr>          bitmaps;
    std::vector<GlyphLigKernPtr>    glyphsLigKern; // Specific to each glyph

    // used ontly at save and load time
    std::vector<RLEBitmapPtr> compressedBitmaps; // Todo: maybe unused at the end

    // used only at save time
    std::vector<LigKernStep> ligKernSteps; // The complete list of lig/kerns
  };

  typedef std::shared_ptr<Face> FacePtr;

  IBMFFontMod(uint8_t *memoryFont, uint32_t size) : memory_(memoryFont), memoryLength_(size) {
    initialized_ = load();
    lastError_   = 0;
  }

  // The following constructor is used ONLY for importing other font formats
  // or to create a BACKUP font format.
  // A specific load method must then be used to retrieve the font information
  // and populate the structure from that foreign format.
  IBMFFontMod() : memory_(nullptr), memoryLength_(0) {}

  ~IBMFFontMod() { clear(); }

  static auto createBackup() -> IBMFFontMod * {
    IBMFFontMod *font = new IBMFFontMod();
    // clang-format off
    font->preamble_ = Preamble{
        .marker    = {'I', 'B', 'M', 'F'},
        .faceCount = 0,
        .bits      = {.version = IBMF_VERSION, .fontFormat = FontFormat::BACKUP}
    };
    // clang-format on
    font->initialized_ = true;
    font->lastError_   = 0;

    return font;
  }

  auto clear() -> void;

  inline auto getPreamble() const -> Preamble { return preamble_; }
  inline auto isInitialized() const -> bool { return initialized_; }
  inline auto getLastError() const -> int { return lastError_; }
  inline auto getLineHeight(int faceIdx) const -> int {
    return faceIdx < preamble_.faceCount ? faces_[faceIdx]->header->lineHeight : 0;
  }

  inline auto getFaceHeader(int faceIdx) const -> const FaceHeaderPtr {
    return faces_[faceIdx]->header;
  }

  inline auto characterCodes() const -> const CharCodes * {
    CharCodes *chCodes = new CharCodes;
    for (GlyphCode i = 0; i < faces_[0]->header->glyphCount; i++) {
      char32_t ch = getUTF32(i);
      chCodes->push_back(ch);
    }
    return chCodes;
  }

  auto findFace(uint8_t pointSize) -> FacePtr;
  auto findGlyphIndex(FacePtr face, char32_t codePoint) -> int;

  auto ligKern(int faceIndex, const GlyphCode glyphCode1, GlyphCode *glyphCode2, FIX16 *kern,
               bool *kernPairPresent, GlyphLigKernPtr bypassLigKern = nullptr) const -> bool;
  auto getGlyph(int faceIndex, int glyphCode, GlyphInfoPtr &glyph_info, BitmapPtr &bitmap,
                GlyphLigKernPtr &glyphLigKern) const -> bool;
  auto saveFaceHeader(int faceIndex, FaceHeader &face_header) -> bool;
  // The font parameter is only used with the BACKUP format
  auto saveGlyph(int faceIndex, int glyphCode, GlyphInfoPtr newGlyphInfo, BitmapPtr newBitmap,
                 GlyphLigKernPtr glyphLigKern, IBMFFontMod *font = nullptr) -> bool;
  auto convertToOneBit(const Bitmap &bitmapHeightBits, BitmapPtr *bitmapOneBit) -> bool;
  auto save(QDataStream &out) -> bool;
  auto translate(char32_t codePoint) const -> GlyphCode;
  auto getUTF32(GlyphCode glyphCode) const -> char32_t;
  auto toGlyphCode(char32_t codePoint) const -> GlyphCode;

protected:
  static constexpr uint8_t IBMF_VERSION = 4;

  Preamble preamble_;

  std::vector<Plane>           planes_;
  std::vector<CodePointBundle> codePointBundles_;
  std::vector<FacePtr>         faces_;

private:
  bool initialized_;

  std::vector<uint32_t> faceOffsets_;

  uint8_t *memory_;
  uint32_t memoryLength_;

  int lastError_;

  auto findList(std::vector<LigKernStep> &pgm, std::vector<LigKernStep> &list) const -> int;
  auto prepareLigKernVectors() -> bool;
  auto load() -> bool;
};

typedef std::shared_ptr<IBMFFontMod> IBMFFontModPtr;
