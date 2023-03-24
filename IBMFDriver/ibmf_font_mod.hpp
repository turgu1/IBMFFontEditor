#pragma once

#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>

#include "freeType.h"
#include "ibmf_defs.hpp"

using namespace IBMFDefs;

#include <QDataStream>
#include <QIODevice>
#include <QMessageBox>

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
 * This is a class to allow for the modification of a IBMF font generated from
 * METAFONT
 *
 */
class IBMFFontMod {
public:
  struct GlyphKernStep {
    uint16_t nextGlyphCode;
    FIX16    kern;
  };
  typedef GlyphKernStep *GlyphKernStepPtr;

  struct GlyphLigStep {
    uint16_t nextGlyphCode;
    uint16_t replacementGlyphCode;
  };
  typedef GlyphLigStep *GlyphLigStepPtr;

  struct GlyphLigKern {
    std::vector<GlyphLigStepPtr>  ligSteps;
    std::vector<GlyphKernStepPtr> kernSteps;
  };
  typedef GlyphLigKern *GlyphLigKernPtr;

  struct Face {
    FaceHeaderPtr                header;
    std::vector<GlyphInfoPtr>    glyphs;
    std::vector<BitmapPtr>       bitmaps;
    std::vector<GlyphLigKernPtr> glyphsLigKern; // Specific to each glyph

    // used ontly at save and load time
    std::vector<RLEBitmap *> compressedBitmaps; // Todo: maybe unused at the end

    // used only at save time
    std::vector<LigKernStep *> ligKernSteps; // The complete list of lig/kerns
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

  int findList(std::vector<LigKernStep *> &pgm, std::vector<LigKernStep *> &list) {
    int idx = 0;
    for (auto entry = list.begin(); entry != list.end(); entry++) {
      bool found = true;
      for (auto pgmEntry = pgm.begin(); pgmEntry != pgm.end(); pgmEntry++) {
        if (((*pgmEntry)->a.whole.val != (*entry)->a.whole.val) ||
            ((*pgmEntry)->b.whole.val != (*entry)->b.whole.val)) {
          found = false;
          break;
        } else {
          int startIdx = idx++;
          pgmEntry++;
          entry++;
          while ((pgmEntry != pgm.end()) && (entry != list.end())) {
            if (((*pgmEntry)->a.whole.val != (*entry)->a.whole.val) ||
                ((*pgmEntry)->b.whole.val != (*entry)->b.whole.val)) {
              found = false;
              break;
            }
            pgmEntry++;
            entry++;
            idx++;
          }
          if (found) return startIdx;
        }
      }
      idx += 1;
    }
    return -1;
  }

  // For all faces:
  //
  // - Retrieves all ligature and kerning for each face glyphs, setting the
  // index in the integrated vector, optimizing the glyphs' list to reuse the
  // ones that are similar
  //
  // - If there is some series with index beyond 254, create goto entries. All
  // starting indexes must be before 255

  bool prepareLigKernVectors() {
    for (auto &face : faces_) {

      auto &lkSteps = face->ligKernSteps;

      lkSteps.clear();

      std::set<int>    overflowList;     // List of starting pgm index that are larger than 254
      std::vector<int> uniquePgmIndexes; // List of all unique start indexes

      // Working list for glyphs pgm vector reconstruction
      // = -1 if a glyph's Lig/Kern pgm is empty
      // < -1 if it has been relocated
      std::vector<int>           glyphsPgmIndexes(face->header->glyphCount, -1);
      std::vector<LigKernStep *> glyphPgm;

      // ----- Retrieves all ligature and kerning in a single list -----
      //
      // glyphsPgmIndexes receives the starting index of each glyph's pgm.
      // uniquePgmIndexes receive the non-duplicate indexes
      // face's ligKernSteps receives the integrated list.
      //
      // Optimization is done to reuse part of pgms that are the same for
      // a glyph vs the other ones

      int glyphIdx = 0;
      for (int glyphIdx = 0; glyphIdx < face->header->glyphCount; glyphIdx++) {
        //      for (int glyphIdx = face->header->glyphCount - 1; glyphIdx >= 0;
        //      glyphIdx--) {

        LigKernStep *lks = nullptr;

        auto lSteps = face->glyphsLigKern[glyphIdx]->ligSteps;
        auto kSteps = face->glyphsLigKern[glyphIdx]->kernSteps;

        glyphPgm.clear();
        glyphPgm.reserve(lSteps.size() + kSteps.size());

        // clang-format off
        for (auto lStep : lSteps) {
          lks = new LigKernStep({
            .a = {.data = {.nextGlyphCode = lStep->nextGlyphCode, .stop = false}},
            .b = {.repl = {.replGlyphCode = lStep->replacementGlyphCode, .isAKern = false}}
          });
          glyphPgm.push_back(lks);
        }

        for (auto kStep : kSteps) {
          lks = new LigKernStep({
            .a = { .data = {.nextGlyphCode = kStep->nextGlyphCode, .stop = false}},
            .b = {.kern = {.kerningValue = (FIX14)kStep->kern, .isAGoTo = false, .isAKern = true}}
          });
          glyphPgm.push_back(lks);
        }
        // clang-format on

        if (glyphPgm.size() == 0) {
          glyphsPgmIndexes[glyphIdx] = -1; // empty list
        } else if (lks != nullptr) {
          lks->a.data.stop = true;
          int sameIdx; // Idx of the equivalent pgm if found (-1 otherwise)
          //           Must start at 2 as cannot have a sameIdx equal to 0 or 1:
          //           Cannot negate 0, and -1 is reserved for a null pgm in
          //           glyphsPgmIndexes
          if ((sameIdx = findList(glyphPgm, lkSteps)) > 1) {
            // We found a duplicated list. Remove the duplicate one and make it
            // point to the first found to be similar.
            for (auto entry : glyphPgm) { delete entry; }
            glyphPgm.clear();
            glyphsPgmIndexes[glyphIdx] = -sameIdx; // negative to signify a duplicate list
            if (sameIdx >= 255) { overflowList.insert(sameIdx); }
          } else {
            int index = lkSteps.size();
            if (index >= 255) { overflowList.insert(index); }
            uniquePgmIndexes.push_back(index);
            glyphsPgmIndexes[glyphIdx] = index;
            std::move(glyphPgm.begin(), glyphPgm.end(), std::back_inserter(lkSteps));
          }
        } else {
          QMessageBox::warning(nullptr, "Logic error",
                               "There is a logic error in the prepareLigKernVectors() method -> "
                               "lks not expected to be null!!");
          return false;
        }
      }

      // ----- Relocate entries that overflowed beyond 254 -----

      // Put them in a vector such that we can access them through indices.

      // Compute how many entries we need to add to the lig/kern vector to
      // redirect over the limiting 255 indexes, and where to add them.

      int spaceRequired = overflowList.size();

      if (spaceRequired > 0) {
        int i = uniquePgmIndexes.size() - (spaceRequired + 1);

        while (true) {
          if ((uniquePgmIndexes[i] + spaceRequired) >= 255) {
            spaceRequired += 1;
            overflowList.insert(uniquePgmIndexes[i]);
            i -= 1;
          } else break;
        }

        overflowList.insert(uniquePgmIndexes[i]);

        // Starting at index uniquePgmIndexes[i], all items must go down for an
        // amount of spaceRequired The corresponding indices in the glyphs table
        // must be adjusted accordingly.

        int newLigKernIdx = uniquePgmIndexes[i];

        // Doing it in reverse order will avoid potential collision between old
        // vs new indexes.
        for (auto idx = overflowList.rbegin(); idx != overflowList.rend(); idx++) {
          // std::cout << *idx << " treatment: " << std::endl;
          LigKernStep *lks = new LigKernStep;
          memset(lks, 0, sizeof(LigKernStep));
          lks->b.goTo.isAKern      = true;
          lks->b.goTo.isAGoTo      = true;
          lks->b.goTo.displacement = (*idx + spaceRequired + 1);

          lkSteps.insert(lkSteps.begin() + newLigKernIdx, lks);
          for (auto pgmIdx = glyphsPgmIndexes.begin(); pgmIdx != glyphsPgmIndexes.end(); pgmIdx++) {
            if (abs(*pgmIdx) == *idx) { // Must look at both duplicated and
                                        // non-duplicated indexes
              *pgmIdx = -newLigKernIdx;
            }
          }
          newLigKernIdx++;
        } // for
      }   // spaceRequired > 0

      glyphIdx = 0;
      for (auto &glyph : face->glyphs) {
        if (glyphsPgmIndexes[glyphIdx] == -1) {
          glyph->ligKernPgmIndex = 255;
        } else {
          if (abs(glyphsPgmIndexes[glyphIdx]) >= 255) {
            QMessageBox::warning(nullptr, "Logic Error",
                                 "A logic error was encoutered in method "
                                 "prepareLigKernVectors() "
                                 "-> computed LigKern PGM index >= 255!!");
            return false;
          }
          glyph->ligKernPgmIndex = abs(glyphsPgmIndexes[glyphIdx]);
        }
        glyphIdx += 1;
      }
    } // for each face

    return true;
  }

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
                step->nextGlyphCode = face->ligKernSteps[lk_idx]->a.data.nextGlyphCode;
                step->kern          = face->ligKernSteps[lk_idx]->b.kern.kerningValue;
                glk->kernSteps.push_back(step);
              } else {
                GlyphLigStep *step         = new GlyphLigStep;
                step->nextGlyphCode        = face->ligKernSteps[lk_idx]->a.data.nextGlyphCode;
                step->replacementGlyphCode = face->ligKernSteps[lk_idx]->b.repl.replGlyphCode;
                glk->ligSteps.push_back(step);
              }
            } while (!face->ligKernSteps[lk_idx++]->a.data.stop);
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

  // The following constructor is used ONLY for importing other font formats.
  // A specific load method must then be used to retrieve the font information
  // and populate the structure from that foreign format.
  IBMFFontMod() : memory_(nullptr), memoryLength_(0) {}

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
      for (auto ligKern : face->ligKernSteps) { delete ligKern; }
      for (auto ligKern : face->glyphsLigKern) {
        for (auto lig : ligKern->ligSteps) delete lig;
        for (auto kern : ligKern->kernSteps) delete kern;
        delete ligKern;
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

    if (!prepareLigKernVectors()) return false;

    WRITE(&preamble_, sizeof(Preamble));

    int  fill   = 4 - ((sizeof(Preamble) + preamble_.faceCount) & 3);
    char filler = 0;
    for (auto &face : faces_) { WRITE(&face->header->pointSize, 1); }
    while (fill--) { WRITE(&filler, 1); }

    uint32_t offset    = 0;
    auto     offsetPos = out.device()->pos();
    for (int i = 0; i < preamble_.faceCount; i++) { WRITE(&offset, 4); }

    if (preamble_.bits.fontFormat == FontFormat::UTF32) {
      for (auto &plane : planes_) { WRITE(&plane, sizeof(Plane)); }
      for (auto &codePointBundle : codePointBundles_) {
        WRITE(&codePointBundle, sizeof(CodePointBundle));
      }
    }

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

      int                    idx         = 0;
      int                    glyphCount  = 0;
      std::vector<uint8_t>  *poolData    = new std::vector<uint8_t>();
      std::vector<uint32_t> *poolIndexes = new std::vector<uint32_t>();

      for (auto &glyph : face->glyphs) {
        RLEGenerator *gen = new RLEGenerator;
        if (!gen->encodeBitmap(*face->bitmaps[idx++])) {
          poolData->clear();
          delete poolData;
          poolIndexes->clear();
          delete poolIndexes;

          lastError_ = 3;
          return false;
        }
        glyph->rleMetrics.dynF         = gen->getDynF();
        glyph->rleMetrics.firstIsBlack = gen->getFirstIsBlack();
        auto data                      = gen->getData();
        glyph->packetLength            = data->size();
        poolIndexes->push_back(poolData->size());
        copy(data->begin(), data->end(), std::back_inserter(*poolData));
        delete gen;
      }

      fill = 4 - (poolData->size() + (sizeof(GlyphInfo) * face->header->glyphCount) &
                  3); // to keep alignment to 32bits offsets
      face->header->pixelsPoolSize   = poolData->size() + fill;
      face->header->ligKernStepCount = face->ligKernSteps.size();

      WRITE(face->header.get(), sizeof(FaceHeader));

      for (auto idx : *poolIndexes) { WRITE(&idx, sizeof(uint32_t)); }

      for (auto &glyph : face->glyphs) {
        WRITE(glyph.get(), sizeof(GlyphInfo));
        glyphCount++;
      }

      if (glyphCount != face->header->glyphCount) {
        lastError_ = 5;
        return false;
      }

      WRITE(poolData->data(), poolData->size());
      while (fill--) { WRITE(&filler, 1); }

      poolIndexes->clear();
      poolData->clear();
      delete poolData;
      delete poolIndexes;

      int ligKernCount = 0;
      for (auto ligKern : face->ligKernSteps) {
        WRITE(ligKern, sizeof(LigKernStep));
        ligKernCount += 1;
      }

      if (ligKernCount != face->header->ligKernStepCount) {
        lastError_ = 6;
        return false;
      }
    }
    return true;
  }

  // Returns the corresponding UTF32 character for the glyphCode.
  char32_t getUTF32(GlyphCode glyphCode) {
    char32_t codePoint = 0;
    if (preamble_.bits.fontFormat == FontFormat::UTF32) {
      int i = 0;
      while (i < 3) {
        if (planes_[i + 1].firstGlyphCode > glyphCode) break;
        i += 1;
      }
      if (i < 4) {
        int planeMask    = i << 16;
        int bundleIdx    = planes_[i].codePointBundlesIdx;
        int idx          = planes_[i].firstGlyphCode;
        int entriesCount = planes_[i].entriesCount;
        int j            = 0;
        int bundleSize   = codePointBundles_[bundleIdx].lastCodePoint -
                         codePointBundles_[bundleIdx].firstCodePoint + 1;

        while (j < entriesCount) {
          if ((idx + bundleSize) > glyphCode) break;
          idx += bundleSize;
          bundleIdx += 1;
          bundleSize = codePointBundles_[bundleIdx].lastCodePoint -
                       codePointBundles_[bundleIdx].firstCodePoint + 1;
          j += 1;
        }
        if (j < entriesCount) {
          codePoint = (codePointBundles_[bundleIdx].firstCodePoint + (glyphCode - idx)) | planeMask;
        }
      }
    } else {
      if (glyphCode < fontFormat0CodePoints.size()) {
        codePoint = fontFormat0CodePoints[glyphCode];
      }
    }
    return codePoint;
  }

  bool charSelected(char32_t ch, SelectedBlockIndexesPtr &selectedBlockIndexes) {
    // Don't populate with space and non-break-space characters
    if ((ch != ' ') && (ch != char32_t(160))) {
      for (auto selectedBlock : *selectedBlockIndexes) {
        if ((ch >= uBlocks[selectedBlock].first_) && (ch <= uBlocks[selectedBlock].last_)) {
          return true;
        }
      }
    }
    return false;
  }

  int prepareCodePlanes(FT_Face &face, CharSelections &charSelections) {

    uint16_t glyphCode = 0;

    if (charSelections.size() == 1) {

      for (int i = 0; i < 4; i++) { planes_.push_back(Plane({0, 0, 0})); }

      SelectedBlockIndexesPtr selectedBlockIndexes = charSelections[0].selectedBlockIndexes;

      FT_UInt  index;
      char32_t ch                     = FT_Get_First_Char(face, &index);
      bool     firstSelected          = false;
      char16_t currCodePoint          = 0;
      int      currPlaneIdx           = 0;
      int      currCodePointBundleIdx = 0;

      while (index != 0) {
        if (charSelected(ch, selectedBlockIndexes)) {
          int planeIdx = ch >> 16;
          if (planeIdx < 4) { // Only the first 4 planes are managed
            char16_t u16 = static_cast<char16_t>(ch & 0x0000FFFF);
            if (!firstSelected) {
              currCodePoint     = u16;
              currPlaneIdx      = planeIdx;
              planes_[planeIdx] = Plane{
                  .codePointBundlesIdx = static_cast<uint16_t>(codePointBundles_.size()),
                  .entriesCount        = 1,
                  .firstGlyphCode      = 0,
              };
              currCodePointBundleIdx = codePointBundles_.size();
              CodePointBundle bundle =
                  CodePointBundle({.firstCodePoint = u16, .lastCodePoint = u16});
              codePointBundles_.push_back(bundle);
              firstSelected = true;
            } else {
              if (planeIdx != currPlaneIdx) {
                for (int idx = planeIdx + 1; idx <= currPlaneIdx; idx++) {
                  planes_[idx].codePointBundlesIdx = codePointBundles_.size();
                }
                planes_[planeIdx] =
                    Plane{.codePointBundlesIdx = static_cast<uint16_t>(codePointBundles_.size()),
                          .entriesCount        = 1,
                          .firstGlyphCode      = glyphCode};
                currCodePointBundleIdx = codePointBundles_.size();
                CodePointBundle bundle =
                    CodePointBundle({.firstCodePoint = u16, .lastCodePoint = u16});
                codePointBundles_.push_back(bundle);
                currCodePoint = u16;
                currPlaneIdx  = planeIdx;
              } else {
                if (u16 == (currCodePoint + 1)) {
                  codePointBundles_[currCodePointBundleIdx].lastCodePoint = u16;
                } else {
                  currCodePointBundleIdx = codePointBundles_.size();
                  CodePointBundle bundle =
                      CodePointBundle({.firstCodePoint = u16, .lastCodePoint = u16});
                  codePointBundles_.push_back(bundle);
                  planes_[currPlaneIdx].entriesCount += 1;
                }
                currCodePoint = u16;
              }
            }
          }
          glyphCode += 1;
        }
        ch = FT_Get_Next_Char(face, ch, &index);
      }
      // Completes the info of planes not used
      for (int idx = currPlaneIdx + 1; idx < 4; idx++) {
        planes_[idx].codePointBundlesIdx = codePointBundles_.size();
        planes_[idx].firstGlyphCode      = glyphCode;
      }
    }

    return glyphCode;
  }

  GlyphCode toGlyphCode(char32_t codePoint) {

    GlyphCode glyphCode = NO_GLYPH_CODE;

    uint16_t planeIdx = static_cast<uint16_t>(codePoint >> 16);

    if (planeIdx <= 3) {
      char16_t u16 = static_cast<char16_t>(codePoint);

      uint16_t codePointBundleIdx = planes_[planeIdx].codePointBundlesIdx;
      uint16_t entriesCount       = planes_[planeIdx].entriesCount;
      int      gCode              = planes_[planeIdx].firstGlyphCode;
      int      i                  = 0;
      while (i < entriesCount) {
        if (u16 <= codePointBundles_[codePointBundleIdx].lastCodePoint) { break; }
        gCode += (codePointBundles_[codePointBundleIdx].lastCodePoint -
                  codePointBundles_[codePointBundleIdx].firstCodePoint + 1);
        i++;
        codePointBundleIdx++;
      }
      if ((i < entriesCount) && (u16 >= codePointBundles_[codePointBundleIdx].firstCodePoint)) {
        glyphCode = gCode + u16 - codePointBundles_[codePointBundleIdx].firstCodePoint;
      }
    }

    return glyphCode;
  }

  bool loadTTF(FreeType &ft, FontParametersPtr fontParameters) {

    clear();

    std::vector<uint8_t> pointSizes;

    if (fontParameters->pt12) pointSizes.push_back(12);
    if (fontParameters->pt14) pointSizes.push_back(14);
    if (fontParameters->pt17) pointSizes.push_back(17);

    // ----- Preamble -----

    uint8_t faceCount = pointSizes.size();

    // clang-format off
    preamble_ = {
      .marker    = {'I', 'B', 'M', 'F'},
      .faceCount = faceCount,
      .bits      = {.version = IBMF_VERSION, .fontFormat = FontFormat::UTF32}
    };
    // clang-format on

    CharSelections *sel = fontParameters->charSelections;
    if (sel->size() == 1) {
      QString filename = (*sel)[0].filename;

      FT_Face ftFace;
      if ((ftFace = ft.openFace(filename)) != nullptr) {

        // Testing

        //        FT_Set_Char_Size(ftFace,              // handle to face object
        //                         0,                   // char_width in 1/64th of points
        //                         14 * 64,             // char_height in 1/64th of points
        //                         fontParameters->dpi, // horizontal device resolution
        //                         fontParameters->dpi);
        //        FT_UInt index  = FT_Get_Char_Index(ftFace, '!');
        //        index          = FT_Get_Char_Index(ftFace, 'a');
        //        index          = FT_Get_Char_Index(ftFace, 'B');
        //        FT_Error error = FT_Load_Char(ftFace, index, FT_LOAD_DEFAULT);
        //        error          = FT_Render_Glyph(ftFace->glyph, FT_RENDER_MODE_MONO);

        // End of testing

        uint16_t glyphCount = prepareCodePlanes(ftFace, *sel);
        // This is a test that could be removed in the future
        for (GlyphCode i = 0; i < glyphCount; i++) {
          if ((i != toGlyphCode(getUTF32(i)))) {
            QMessageBox::critical(nullptr, "Internal Error!!",
                                  QString("Problem with getUTF32() and toGlyphCode() that are not "
                                          "orthogonal for glyphCode %1")
                                      .arg(i));
          }
        }

        for (int faceIdx = 0; faceIdx < faceCount; faceIdx++) {
          FT_Error error =
              FT_Set_Char_Size(ftFace,                   // handle to face object
                               0,                        // char_width in 1/64th of points
                               pointSizes[faceIdx] * 64, // char_height in 1/64th of points
                               fontParameters->dpi,      // horizontal device resolution
                               fontParameters->dpi);

          FacePtr face = FacePtr(new Face);

          for (int glyphCode = 0; glyphCode < glyphCount; glyphCode++) {

            FT_ULong ch    = getUTF32(glyphCode);
            FT_UInt  index = FT_Get_Char_Index(ftFace, ch);
            if (index != 0) {
              error = FT_Load_Char(ftFace, ch, FT_LOAD_DEFAULT);

              if (ftFace->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
                error = FT_Render_Glyph(ftFace->glyph, FT_RENDER_MODE_MONO);
              }

              BitmapPtr bitmap = new Bitmap();
              uint8_t  *buffer = ftFace->glyph->bitmap.buffer;
              for (int row = 0; row < ftFace->glyph->bitmap.rows; row++) {
                uint8_t mask = 0x80;
                for (int col = 0; col < ftFace->glyph->bitmap.width; col++) {
                  uint8_t pixel = ((buffer[col >> 3] & mask) == 0) ? 0 : 0xFF;
                  bitmap->pixels.push_back(pixel);
                  mask >>= 1;
                  if (mask == 0) mask = 0x80;
                }
                buffer += ftFace->glyph->bitmap.pitch;
              }
              bitmap->dim = Dim(ftFace->glyph->bitmap.width, ftFace->glyph->bitmap.rows);

              face->bitmaps.push_back(bitmap);
              GlyphLigKernPtr glyphLigKern = new GlyphLigKern;

              // Create ligatures for the glyph if available
              char32_t firstChar = getUTF32(glyphCode);
              for (auto &ligature : ligatures) {
                if (ligature.firstChar == firstChar) {
                  GlyphCode nextGlyphCode        = toGlyphCode(ligature.nextChar);
                  GlyphCode replacementGlyphCode = toGlyphCode(ligature.replacement);
                  if ((nextGlyphCode != NO_GLYPH_CODE) && (replacementGlyphCode != NO_GLYPH_CODE)) {
                    GlyphLigStepPtr glyphLigStep =
                        new GlyphLigStep({.nextGlyphCode        = nextGlyphCode,
                                          .replacementGlyphCode = replacementGlyphCode});
                    glyphLigKern->ligSteps.push_back(glyphLigStep);
                  }
                }
              }

              face->glyphsLigKern.push_back(glyphLigKern);

              GlyphInfoPtr glyphInfo = GlyphInfoPtr(new GlyphInfo({
                  .bitmapWidth      = static_cast<uint8_t>(ftFace->glyph->bitmap.width),
                  .bitmapHeight     = static_cast<uint8_t>(ftFace->glyph->bitmap.rows),
                  .horizontalOffset = static_cast<int8_t>(ftFace->glyph->bitmap_left),
                  .verticalOffset   = static_cast<int8_t>(-ftFace->glyph->bitmap_top),
                  .packetLength     = static_cast<uint16_t>(ftFace->glyph->bitmap.width *
                                                        ftFace->glyph->bitmap.rows),
                  .advance          = static_cast<FIX16>(ftFace->glyph->advance.x),
                  .rleMetrics       = RLEMetrics{.dynF = 0, .firstIsBlack = false, .filler = 0},
                  .ligKernPgmIndex  = 0, // completed at save time
              }));

              face->glyphs.push_back(glyphInfo);

            } else {
              QMessageBox::critical(
                  nullptr, "Internal error!",
                  QString("Can't find utf32 codePoint for glyphCode %1)").arg(glyphCode));
              return false;
            }
          }

          FT_UInt index   = FT_Get_Char_Index(ftFace, 'x');
          FIX16   xHeight = 0;
          if (index != 0) {
            FT_Load_Char(ftFace, index, FT_LOAD_NO_BITMAP);
            xHeight = static_cast<FIX16>(ftFace->glyph->metrics.height);
          } else {
            QMessageBox::warning(nullptr, "No 'x' character",
                                 "There is no 'x' character in this font");
          }

          FIX16 emSize = static_cast<FIX16>(ftFace->size->metrics.x_ppem << 6);

          index             = FT_Get_Char_Index(ftFace, ' ');
          uint8_t spaceSize = emSize / 2;
          if (index != 0) {
            FT_Load_Char(ftFace, index, FT_LOAD_NO_BITMAP);
            spaceSize = static_cast<uint8_t>(ftFace->glyph->metrics.horiAdvance >> 6);
          } else {
            QMessageBox::warning(nullptr, "No space character",
                                 "There is no space character in this font");
          }

          face->header = FaceHeaderPtr(new FaceHeader({
              .pointSize        = pointSizes[faceIdx],
              .lineHeight       = static_cast<uint8_t>(ftFace->size->metrics.height >> 6),
              .dpi              = static_cast<uint16_t>(fontParameters->dpi),
              .xHeight          = xHeight,
              .emSize           = emSize,
              .slantCorrection  = 0, // not available for FreeType
              .descenderHeight  = static_cast<uint8_t>(-(ftFace->size->metrics.descender >> 6)),
              .spaceSize        = spaceSize,
              .glyphCount       = glyphCount,
              .ligKernStepCount = 0, // will be set at save time
              .pixelsPoolSize   = 0, // will be set at save time
              .maxHeight        = 0,
              .filler           = {0, 0, 0}
          }));
          faces_.push_back(std::move(face));
        }
      } else {
        return false;
      }
    } else {
      return false;
    }
    return true;
  }
};
