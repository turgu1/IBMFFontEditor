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

  bool initialized;

  Preamble preamble;

  std::vector<uint32_t> faceOffsets;
  std::vector<FacePtr>  faces;

  uint8_t *memory;
  uint32_t memoryLength;

  uint8_t *memoryPtr;
  uint8_t *memoryEnd;

  // uint32_t      repeatCount;

  int lastError;

  bool load() {
    memcpy(&preamble, memory, sizeof(Preamble));
    if (strncmp("IBMF", preamble.marker, 4) != 0) return false;
    if (preamble.bits.version != IBMF_VERSION) return false;

    int idx = sizeof(Preamble);

    for (int i = 0; i < preamble.faceCount; i++) {
      uint32_t offset = *((uint32_t *) &memory[idx]);
      faceOffsets.push_back(offset);
      idx += 4;
    }

    for (int i = 0; i < preamble.faceCount; i++) {
      uint32_t      idx    = faceOffsets[i];
      FacePtr       face   = FacePtr(new Face);
      FaceHeaderPtr header = FaceHeaderPtr(new FaceHeader);
      memcpy(header.get(), &memory[idx], sizeof(FaceHeader));
      idx += sizeof(FaceHeader);

      face->glyphs.reserve(header->glyphCount);

      for (int j = 0; j < header->glyphCount; j++) {
        GlyphInfoPtr glyph_info = GlyphInfoPtr(new GlyphInfo);
        memcpy(glyph_info.get(), &memory[idx], sizeof(GlyphInfo));
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
          compressedBitmap->pixels.push_back(memory[idx + pos]);
        }

        RLEExtractor rle;
        rle.retrieveBitmap(*compressedBitmap, *bitmap, Pos(0, 0), glyph_info->rleMetrics);
        // retrieveBitmap(idx, glyph_info.get(), *bitmap, Pos(0,0));

        face->glyphs.push_back(glyph_info);
        face->bitmaps.push_back(bitmap);
        face->compressedBitmaps.push_back(compressedBitmap);

        idx += glyph_info->packetLength;
      }

      if (header->ligKernStepCount > 0) {
        face->ligKernSteps.reserve(header->ligKernStepCount);
        for (int j = 0; j < header->ligKernStepCount; j++) {
          LigKernStep *step = new LigKernStep;
          memcpy(step, &memory[idx], sizeof(LigKernStep));

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
      faces.push_back(std::move(face));
    }

    return true;
  }

public:
  IBMFFontMod(uint8_t *memoryFont, uint32_t size) : memory(memoryFont), memoryLength(size) {

    memoryEnd   = memory + memoryLength;
    initialized = load();
    lastError   = 0;
  }

  ~IBMFFontMod() { clear(); }

  void clear() {
    initialized = false;
    for (auto &face : faces) {
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
    faces.clear();
    faceOffsets.clear();
  }

  inline Preamble getPreample() { return preamble; }
  inline bool     isInitialized() { return initialized; }
  inline int      getLastError() { return lastError; }

  inline const FaceHeaderPtr getFaceHeader(int faceIdx) { return faces[faceIdx]->header; }

  bool getGlyphLigKern(int faceIndex, int glyphCode, GlyphLigKern **glyphLigKern) {
    if (faceIndex >= preamble.faceCount) { return false; }
    if (glyphCode >= faces[faceIndex]->header->glyphCount) { return false; }

    *glyphLigKern = faces[faceIndex]->glyphsLigKern[glyphCode];

    return true;
  }

  bool getGlyph(int faceIndex, int glyphCode, GlyphInfoPtr &glyph_info, Bitmap **bitmap) {
    if (faceIndex >= preamble.faceCount) return false;
    if (glyphCode > faces[faceIndex]->header->glyphCount) { return false; }

    int glyphIndex = glyphCode;

    glyph_info = faces[faceIndex]->glyphs[glyphIndex];
    *bitmap    = faces[faceIndex]->bitmaps[glyphIndex];

    return true;
  }

  bool saveFaceHeader(int faceIndex, FaceHeader &face_header) {
    if (faceIndex < preamble.faceCount) {
      memcpy(faces[faceIndex]->header.get(), &face_header, sizeof(FaceHeader));
      return true;
    }
    return false;
  }

  bool saveGlyph(int faceIndex, int glyphCode, GlyphInfo *newGlyphInfo, Bitmap *new_bitmap) {
    if ((faceIndex < preamble.faceCount) && (glyphCode < faces[faceIndex]->header->glyphCount)) {

      int glyphIndex = glyphCode;

      *faces[faceIndex]->glyphs[glyphIndex] = *newGlyphInfo;
      delete faces[faceIndex]->bitmaps[glyphIndex];
      faces[faceIndex]->bitmaps[glyphIndex] = new_bitmap;
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
    lastError = 1;                                                                                 \
    return false;                                                                                  \
  }

  bool save(QDataStream &out) {
    lastError = 0;
    WRITE(&preamble, sizeof(Preamble));

    uint32_t offset    = 0;
    auto     offsetPos = out.device()->pos();
    for (int i = 0; i < preamble.faceCount; i++) { WRITE(&offset, 4); }

    for (auto &face : faces) { WRITE(&face->header->pointSize, 1); }
    if (preamble.faceCount & 1) {
      char filler = 0;
      WRITE(&filler, 1);
    }

    for (auto &face : faces) {
      // Save current offset position as the location of the font face
      auto pos = out.device()->pos();
      out.device()->seek(offsetPos);
      WRITE(&pos, 4);
      offsetPos += 4;
      out.device()->seek(pos);
      if (out.device()->pos() != pos) {
        lastError = 2;
        return false;
      }

      WRITE(face->header.get(), sizeof(FaceHeader));

      int idx        = 0;
      int glyphCount = 0;

      for (auto &glyph : face->glyphs) {
        RLEGenerator *gen = new RLEGenerator;
        if (!gen->encodeBitmap(*face->bitmaps[idx++])) {
          lastError = 3;
          return false;
        }
        glyph->rleMetrics.dynF         = gen->getDynF();
        glyph->rleMetrics.firstIsBlack = gen->getFirstIsBlack();
        auto data                      = gen->getData();
        if (data->size() != glyph->packetLength) {
          lastError = 4;
          return false;
        }
        WRITE(glyph.get(), sizeof(GlyphInfo));
        WRITE(data->data(), data->size());
        delete gen;
        glyphCount++;
      }

      if (glyphCount != face->header->glyphCount) {
        lastError = 5;
        return false;
      }

      int lig_kernCount = 0;
      for (auto lig_kern : face->ligKernSteps) {
        WRITE(lig_kern, sizeof(LigKernStep));
        lig_kernCount += 1;
      }

      if (lig_kernCount != face->header->ligKernStepCount) {
        lastError = 6;
        return false;
      }
    }
    return true;
  }
};
