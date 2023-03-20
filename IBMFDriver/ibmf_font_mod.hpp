#pragma once

#include <cstring>

#include "ibmf_defs.hpp"
using namespace IBMFDefs;

#include <QDataStream>
#include <QIODevice>

#include "rle_extractor.hpp"
#include "rle_generator.hpp"

#define DEBUG 0

#if DEBUG
#include <iomanip>
#include <iostream>
#endif

/**
 * @brief Access to a IBMF font.
 *
 * This is a class to allow for the modification of a IBMF font generated from METAFONT
 *
 */
class IBMFFontMod {
public:
  struct GlyphKernStep {
    unsigned int nextGlyphCode;
    FIX16        kern;
  };

  struct GlyphLigStep {
    unsigned int nextGlyphCode;
    unsigned int glyphCode;
  };

  struct GlyphLigKern {
    std::vector<GlyphLigStep *>  lig_steps;
    std::vector<GlyphKernStep *> kern_steps;
  };

  struct Face {
    FaceHeaderPtr               header;
    std::vector<GlyphInfoPtr>   glyphs;
    std::vector<Bitmap *>       bitmaps;
    std::vector<RLEBitmap *>    compressedBitmaps;
    std::vector<LigKernStep *>  ligKernSteps;
    std::vector<GlyphLigKern *> glyphsLigKern;
  };

  typedef std::unique_ptr<Face> FacePtr;

private:
  static constexpr uint8_t MAX_GLYPH_COUNT = 254; // Index Value 0xFE and 0xFF are reserved
  static constexpr uint8_t IBMF_VERSION    = 4;

  bool initialized_;

  Preamble preamble_;

  std::vector<uint32_t> faceOffsets_;
  std::vector<FacePtr>  faces_;

  uint8_t *memory_;
  uint32_t memoryLength_;

  std::vector<Plane>           planes_;
  std::vector<CodePointBundle> codePointBundles_;

  int lastError_;

  bool load() {
    // Preamble retrieval
    memcpy(&preamble_, memory_, sizeof(Preamble));
    if (strncmp("IBMF", preamble_.marker, 4) != 0) return false;
    if (preamble_.bits.version != IBMF_VERSION) return false;

    int idx = ((sizeof(Preamble) + preamble_.faceCount + 3) & 0xFFFFFFFC);

    // Faces offset retrieval
    for (int i = 0; i < preamble_.faceCount; i++) {
      uint32_t offset = *((uint32_t *) &memory_[idx]);
      faceOffsets_.push_back(offset);
      idx += 4;
    }

    // Unicode CodePoint Table retrieval
    if (preamble_.bits.fontFormat == FontFormat::UTF32) {
      PlanesPtr planes      = reinterpret_cast<PlanesPtr>(&memory_[idx]);
      int       bundleCount = 0;
      for (int i = 0; i < 4; i++) {
        planes_.push_back((*planes)[i]);
        bundleCount += (*planes)[i].entriesCount;
      }
      idx += sizeof(Planes);

      CodePointBundlesPtr codePointBundles = reinterpret_cast<CodePointBundlesPtr>(&memory_[idx]);
      for (int i = 0; i < bundleCount; i++) { codePointBundles_.push_back((*codePointBundles)[i]); }
      idx += (((*planes)[3].codePointBundlesIdx + (*planes)[3].entriesCount) *
              sizeof(CodePointBundle));
    } else {
      planes_.clear();
      codePointBundles_.clear();
    }

    // Faces retrieval
    for (int i = 0; i < preamble_.faceCount; i++) {
      if (idx != faceOffsets_[i]) return false;

      // Face Header
      FacePtr                face   = FacePtr(new Face);
      FaceHeaderPtr          header = FaceHeaderPtr(new FaceHeader);
      GlyphsPixelPoolIndexes glyphsPixelPoolIndexes;
      PixelsPoolPtr          pixelsPool;

      memcpy(header.get(), &memory_[idx], sizeof(FaceHeader));
      idx += sizeof(FaceHeader);

      // Glyphs RLE bitmaps indexes in the bitmaps pool
      glyphsPixelPoolIndexes = reinterpret_cast<GlyphsPixelPoolIndexes>(&memory_[idx]);
      idx += (sizeof(PixelPoolIndex) * header->glyphCount);

      pixelsPool =
          reinterpret_cast<PixelsPoolPtr>(&memory_[idx + (sizeof(GlyphInfo) * header->glyphCount)]);

      // Glyphs info and bitmaps
      face->glyphs.reserve(header->glyphCount);

      for (int glyphCode = 0; glyphCode < header->glyphCount; glyphCode++) {
        GlyphInfoPtr glyph_info = GlyphInfoPtr(new GlyphInfo);
        memcpy(glyph_info.get(), &memory_[idx], sizeof(GlyphInfo));
        idx += sizeof(GlyphInfo);

        int     bitmap_size = glyph_info->bitmapHeight * glyph_info->bitmapWidth;
        Bitmap *bitmap      = new Bitmap;
        bitmap->pixels      = Pixels(bitmap_size, 0);
        bitmap->dim         = Dim(glyph_info->bitmapWidth, glyph_info->bitmapHeight);

        RLEBitmap *compressedBitmap = new RLEBitmap;
        compressedBitmap->dim       = bitmap->dim;
        compressedBitmap->pixels.reserve(glyph_info->packetLength);
        compressedBitmap->length = glyph_info->packetLength;
        for (int pos = 0; pos < glyph_info->packetLength; pos++) {
          compressedBitmap->pixels.push_back(
              (*pixelsPool)[pos + (*glyphsPixelPoolIndexes)[glyphCode]]);
        }

        RLEExtractor rle;
        rle.retrieveBitmap(*compressedBitmap, *bitmap, Pos(0, 0), glyph_info->rleMetrics);
        // retrieveBitmap(idx, glyph_info.get(), *bitmap, Pos(0,0));

        face->glyphs.push_back(glyph_info);
        face->bitmaps.push_back(bitmap);
        face->compressedBitmaps.push_back(compressedBitmap);

        // idx += glyph_info->packetLength;
      }

      if (&memory_[idx] != (uint8_t *) pixelsPool) { return false; }

      idx += header->pixelsPoolSize;

      if (header->ligKernStepCount > 0) {
        face->ligKernSteps.reserve(header->ligKernStepCount);
        for (int j = 0; j < header->ligKernStepCount; j++) {
          LigKernStep *step = new LigKernStep;
          memcpy(step, &memory_[idx], sizeof(LigKernStep));

          face->ligKernSteps.push_back(step);

          idx += sizeof(LigKernStep);
        }
      }

      for (int ch = 0; ch < header->glyphCount; ch++) {
        GlyphLigKern *glk = new GlyphLigKern;

        if (face->glyphs[ch]->ligKernPgmIndex != 255) {
          int lk_idx = face->glyphs[ch]->ligKernPgmIndex;
          if (lk_idx < header->ligKernStepCount) {
            if ((face->ligKernSteps[lk_idx]->b.goTo.isAGoTo) &&
                (face->ligKernSteps[lk_idx]->b.kern.isAKern)) {
              lk_idx = face->ligKernSteps[lk_idx]->b.goTo.displacement;
            }
            do {
              if (face->ligKernSteps[lk_idx]->b.kern.isAKern) { // true = kern, false = ligature
                GlyphKernStep *step = new GlyphKernStep;
                step->nextGlyphCode = face->ligKernSteps[lk_idx]->a.nextGlyphCode;
                step->kern          = face->ligKernSteps[lk_idx]->b.kern.kerningValue;
                glk->kern_steps.push_back(step);
              } else {
                GlyphLigStep *step  = new GlyphLigStep;
                step->nextGlyphCode = face->ligKernSteps[lk_idx]->a.nextGlyphCode;
                step->glyphCode     = face->ligKernSteps[lk_idx]->b.repl.replGlyphCode;
                glk->lig_steps.push_back(step);
              }
            } while (!face->ligKernSteps[lk_idx++]->a.stop);
          }
        }
        face->glyphsLigKern.push_back(glk);
      }

      face->header = header;
      faces_.push_back(std::move(face));
    }

    return true;
  }

public:
  IBMFFontMod(uint8_t *memoryFont, uint32_t size) : memory_(memoryFont), memoryLength_(size) {

    initialized_ = load();
    lastError_   = 0;
  }

  ~IBMFFontMod() { clear(); }

  void clear() {
    initialized_ = false;
    for (auto &face : faces_) {
      for (auto bitmap : face->bitmaps) {
        bitmap->clear();
        delete bitmap;
      }
      for (auto bitmap : face->compressedBitmaps) {
        bitmap->clear();
        delete bitmap;
      }
      for (auto lig_kern : face->ligKernSteps) { delete lig_kern; }
      for (auto lig_kern : face->glyphsLigKern) {
        for (auto lig : lig_kern->lig_steps) delete lig;
        for (auto kern : lig_kern->kern_steps) delete kern;
        delete lig_kern;
      }
      face->glyphs.clear();
      face->bitmaps.clear();
      face->compressedBitmaps.clear();
      face->ligKernSteps.clear();
    }
    faces_.clear();
    faceOffsets_.clear();
    planes_.clear();
    codePointBundles_.clear();
  }

  inline Preamble getPreample() { return preamble_; }
  inline bool     isInitialized() { return initialized_; }
  inline int      getLastError() { return lastError_; }

  inline const FaceHeaderPtr getFaceHeader(int faceIdx) { return faces_[faceIdx]->header; }

  bool getGlyphLigKern(int faceIndex, int glyphCode, GlyphLigKern **glyphLigKern) {
    if (faceIndex >= preamble_.faceCount) { return false; }
    if (glyphCode >= faces_[faceIndex]->header->glyphCount) { return false; }

    *glyphLigKern = faces_[faceIndex]->glyphsLigKern[glyphCode];

    return true;
  }

  bool getGlyph(int faceIndex, int glyphCode, GlyphInfoPtr &glyph_info, Bitmap **bitmap) {
    if (faceIndex >= preamble_.faceCount) return false;
    if (glyphCode > faces_[faceIndex]->header->glyphCount) { return false; }

    int glyphIndex = glyphCode;

    glyph_info = faces_[faceIndex]->glyphs[glyphIndex];
    *bitmap    = faces_[faceIndex]->bitmaps[glyphIndex];

    return true;
  }

  bool saveFaceHeader(int faceIndex, FaceHeader &face_header) {
    if (faceIndex < preamble_.faceCount) {
      memcpy(faces_[faceIndex]->header.get(), &face_header, sizeof(FaceHeader));
      return true;
    }
    return false;
  }

  bool saveGlyph(int faceIndex, int glyphCode, GlyphInfo *newGlyphInfo, Bitmap *new_bitmap) {
    if ((faceIndex < preamble_.faceCount) && (glyphCode < faces_[faceIndex]->header->glyphCount)) {

      int glyphIndex = glyphCode;

      *faces_[faceIndex]->glyphs[glyphIndex] = *newGlyphInfo;
      delete faces_[faceIndex]->bitmaps[glyphIndex];
      faces_[faceIndex]->bitmaps[glyphIndex] = new_bitmap;
      return true;
    }
    return false;
  }

  bool convertToOneBit(const Bitmap &bitmapHeightBits, Bitmap **bitmapOneBit) {
    *bitmapOneBit        = new Bitmap;
    (*bitmapOneBit)->dim = bitmapHeightBits.dim;
    auto pix             = &(*bitmapOneBit)->pixels;
    for (int row = 0, idx = 0; row < bitmapHeightBits.dim.height; row++) {
      uint8_t data = 0;
      uint8_t mask = 0x80;
      for (int col = 0; col < bitmapHeightBits.dim.width; col++, idx++) {
        data |= bitmapHeightBits.pixels[idx] == 0 ? 0 : mask;
        mask >>= 1;
        if (mask == 0) {
          pix->push_back(data);
          data = 0;
          mask = 0x80;
        }
      }
      if (mask != 0) pix->push_back(data);
    }
    return pix->size() == (bitmapHeightBits.dim.height * ((bitmapHeightBits.dim.width + 7) >> 3));
  }

#define WRITE(v, size)                                                                             \
  if (out.writeRawData((char *) v, size) == -1) {                                                  \
    lastError_ = 1;                                                                                \
    return false;                                                                                  \
  }

  bool save(QDataStream &out) {
    lastError_ = 0;
    WRITE(&preamble_, sizeof(Preamble));

    uint32_t offset    = 0;
    auto     offsetPos = out.device()->pos();
    for (int i = 0; i < preamble_.faceCount; i++) { WRITE(&offset, 4); }

    int  fill   = 4 - ((sizeof(Preamble) + preamble_.faceCount) & 3);
    char filler = 0;
    for (auto &face : faces_) { WRITE(&face->header->pointSize, 1); }
    while (fill--) { WRITE(&filler, 1); }

    for (auto &face : faces_) {
      // Save current offset position as the location of the font face
      auto pos = out.device()->pos();
      out.device()->seek(offsetPos);
      WRITE(&pos, 4);
      offsetPos += 4;
      out.device()->seek(pos);
      if (out.device()->pos() != pos) {
        lastError_ = 2;
        return false;
      }

      int                   idx        = 0;
      int                   glyphCount = 0;
      std::vector<uint8_t> *poolData   = new std::vector<uint8_t>();

      for (auto &glyph : face->glyphs) {
        RLEGenerator *gen = new RLEGenerator;
        if (!gen->encodeBitmap(*face->bitmaps[idx++])) {
          poolData->clear();
          delete poolData;
          lastError_ = 3;
          return false;
        }
        glyph->rleMetrics.dynF         = gen->getDynF();
        glyph->rleMetrics.firstIsBlack = gen->getFirstIsBlack();
        auto data                      = gen->getData();
        glyph->packetLength            = data->size();

        copy(data->begin(), data->end(), std::back_inserter(*poolData));
        delete gen;
      }

      face->header->pixelsPoolSize = poolData->size();

      WRITE(face->header.get(), sizeof(FaceHeader));

      for (auto &glyph : face->glyphs) {
        WRITE(glyph.get(), sizeof(GlyphInfo));
        glyphCount++;
      }

      if (glyphCount != face->header->glyphCount) {
        lastError_ = 5;
        return false;
      }

      fill = 4 - (poolData->size() & 3);
      WRITE(poolData->data(), poolData->size());
      while (fill--) { WRITE(&filler, 1); }

      poolData->clear();
      delete poolData;

      int lig_kernCount = 0;
      for (auto lig_kern : face->ligKernSteps) {
        WRITE(lig_kern, sizeof(LigKernStep));
        lig_kernCount += 1;
      }

      if (lig_kernCount != face->header->ligKernStepCount) {
        lastError_ = 6;
        return false;
      }
    }
    return true;
  }
};
