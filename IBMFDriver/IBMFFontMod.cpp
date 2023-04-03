#include "IBMFFontMod.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>

void IBMFFontMod::clear() {
  initialized_ = false;
  for (auto &face : faces_) {
    for (auto bitmap : face->bitmaps) {
      bitmap->clear();
    }
    for (auto bitmap : face->compressedBitmaps) {
      bitmap->clear();
    }
    face->glyphs.clear();
    face->bitmaps.clear();
    face->compressedBitmaps.clear();
    face->glyphsLigKern.clear();
    face->ligKernSteps.clear();
  }
  faces_.clear();
  faceOffsets_.clear();
  planes_.clear();
  codePointBundles_.clear();
}

bool IBMFFontMod::load() {
  // Preamble retrieval
  memcpy(&preamble_, memory_, sizeof(Preamble));
  if (strncmp("IBMF", preamble_.marker, 4) != 0) return false;
  if (preamble_.bits.version != IBMF_VERSION) return false;

  int idx = ((sizeof(Preamble) + preamble_.faceCount + 3) & 0xFFFFFFFC);

  // Faces offset retrieval
  for (int i = 0; i < preamble_.faceCount; i++) {
    uint32_t offset = *((uint32_t *)&memory_[idx]);
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
    for (int i = 0; i < bundleCount; i++) {
      codePointBundles_.push_back((*codePointBundles)[i]);
    }
    idx +=
        (((*planes)[3].codePointBundlesIdx + (*planes)[3].entriesCount) * sizeof(CodePointBundle));
  } else {
    planes_.clear();
    codePointBundles_.clear();
  }

  // Faces retrieval
  for (int i = 0; i < preamble_.faceCount; i++) {
    if (idx != faceOffsets_[i]) return false;

    // Face Header
    FacePtr                       face   = FacePtr(new Face);
    FaceHeaderPtr                 header = FaceHeaderPtr(new FaceHeader);
    GlyphsPixelPoolIndexesTempPtr glyphsPixelPoolIndexes;
    PixelsPoolTempPtr             pixelsPool;

    memcpy(header.get(), &memory_[idx], sizeof(FaceHeader));
    idx += sizeof(FaceHeader);

    // Glyphs RLE bitmaps indexes in the bitmaps pool
    glyphsPixelPoolIndexes = reinterpret_cast<GlyphsPixelPoolIndexesTempPtr>(&memory_[idx]);
    idx += (sizeof(PixelPoolIndex) * header->glyphCount);

    pixelsPool = reinterpret_cast<PixelsPoolTempPtr>(
        &memory_[idx + (sizeof(GlyphInfo) * header->glyphCount)]);

    // Glyphs info and bitmaps
    face->glyphs.reserve(header->glyphCount);

    for (int glyphCode = 0; glyphCode < header->glyphCount; glyphCode++) {
      GlyphInfoPtr glyph_info = GlyphInfoPtr(new GlyphInfo);
      memcpy(glyph_info.get(), &memory_[idx], sizeof(GlyphInfo));
      idx += sizeof(GlyphInfo);

      int       bitmap_size         = glyph_info->bitmapHeight * glyph_info->bitmapWidth;
      BitmapPtr bitmap              = BitmapPtr(new Bitmap);
      bitmap->pixels                = Pixels(bitmap_size, 0);
      bitmap->dim                   = Dim(glyph_info->bitmapWidth, glyph_info->bitmapHeight);

      RLEBitmapPtr compressedBitmap = RLEBitmapPtr(new RLEBitmap);
      compressedBitmap->dim         = bitmap->dim;
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

    if (&memory_[idx] != (uint8_t *)pixelsPool) {
      return false;
    }

    idx += header->pixelsPoolSize;

    if (header->ligKernStepCount > 0) {
      face->ligKernSteps.reserve(header->ligKernStepCount);
      for (int j = 0; j < header->ligKernStepCount; j++) {
        LigKernStepPtr step = LigKernStepPtr(new LigKernStep);
        memcpy(step.get(), &memory_[idx], sizeof(LigKernStep));

        face->ligKernSteps.push_back(step);

        idx += sizeof(LigKernStep);
      }
    }

    for (GlyphCode glyphCode = 0; glyphCode < header->glyphCount; glyphCode++) {
      GlyphLigKernPtr glk = GlyphLigKernPtr(new GlyphLigKern);

      if (face->glyphs[glyphCode]->ligKernPgmIndex != 255) {
        int lk_idx = face->glyphs[glyphCode]->ligKernPgmIndex;
        if (lk_idx < header->ligKernStepCount) {
          if ((face->ligKernSteps[lk_idx]->b.goTo.isAGoTo) &&
              (face->ligKernSteps[lk_idx]->b.kern.isAKern)) {
            lk_idx = face->ligKernSteps[lk_idx]->b.goTo.displacement;
          }
          do {
            if (face->ligKernSteps[lk_idx]->b.kern.isAKern) { // true = kern, false = ligature
              GlyphKernStepPtr step = GlyphKernStepPtr(new GlyphKernStep);
              step->nextGlyphCode   = face->ligKernSteps[lk_idx]->a.data.nextGlyphCode;
              step->kern            = face->ligKernSteps[lk_idx]->b.kern.kerningValue;
              glk->kernSteps.push_back(step);
            } else {
              GlyphLigStepPtr step       = GlyphLigStepPtr(new GlyphLigStep);
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

#define WRITE(v, size)                                                                             \
  if (out.writeRawData((char *)v, size) == -1) {                                                   \
    lastError_ = 1;                                                                                \
    return false;                                                                                  \
  }

#define WRITE2(v, size)                                                                            \
  if (out.writeRawData((char *)v, size) == -1) {                                                   \
    lastError_ = 1;                                                                                \
    poolIndexes->clear();                                                                          \
    poolData->clear();                                                                             \
    delete poolData;                                                                               \
    delete poolIndexes;                                                                            \
    return false;                                                                                  \
  }

auto IBMFFontMod::save(QDataStream &out) -> bool {

  lastError_ = 0;

  if (!prepareLigKernVectors()) return false;

  WRITE(&preamble_, sizeof(Preamble));

  int  fill   = 4 - ((sizeof(Preamble) + preamble_.faceCount) & 3);
  char filler = 0;
  for (auto &face : faces_) {
    WRITE(&face->header->pointSize, 1);
  }
  while (fill--) {
    WRITE(&filler, 1);
  }

  uint32_t offset    = 0;
  auto     offsetPos = out.device()->pos();
  for (int i = 0; i < preamble_.faceCount; i++) {
    WRITE(&offset, 4);
  }

  if (preamble_.bits.fontFormat == FontFormat::UTF32) {
    for (auto &plane : planes_) {
      WRITE(&plane, sizeof(Plane));
    }
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

    WRITE2(face->header.get(), sizeof(FaceHeader));

    for (auto idx : *poolIndexes) {
      WRITE2(&idx, sizeof(uint32_t));
    }

    for (auto &glyph : face->glyphs) {
      WRITE2(glyph.get(), sizeof(GlyphInfo));
      glyphCount++;
    }

    if (glyphCount != face->header->glyphCount) {
      lastError_ = 5;
      poolIndexes->clear();
      poolData->clear();
      delete poolData;
      delete poolIndexes;
      return false;
    }

    WRITE2(poolData->data(), poolData->size());
    while (fill--) {
      WRITE2(&filler, 1);
    }

    poolIndexes->clear();
    poolData->clear();
    delete poolData;
    delete poolIndexes;

    int ligKernCount = 0;
    for (auto ligKern : face->ligKernSteps) {
      WRITE(ligKern.get(), sizeof(LigKernStep));
      ligKernCount += 1;
    }

    if (ligKernCount != face->header->ligKernStepCount) {
      lastError_ = 6;
      return false;
    }
  }
  return true;
}

auto IBMFFontMod::saveFaceHeader(int faceIndex, FaceHeader &face_header) -> bool {
  if (faceIndex < preamble_.faceCount) {
    memcpy(faces_[faceIndex]->header.get(), &face_header, sizeof(FaceHeader));
    return true;
  }
  return false;
}

auto IBMFFontMod::saveGlyph(int faceIndex, int glyphCode, GlyphInfo *newGlyphInfo,
                            BitmapPtr new_bitmap) -> bool {
  if ((faceIndex < preamble_.faceCount) && (glyphCode < faces_[faceIndex]->header->glyphCount)) {

    int glyphIndex                         = glyphCode;

    *faces_[faceIndex]->glyphs[glyphIndex] = *newGlyphInfo;
    faces_[faceIndex]->bitmaps[glyphIndex] = new_bitmap;
    return true;
  }
  return false;
}

/// @brief Search Ligature and Kerning table
///
/// Using the LigKern program of **glyphCode1**, find the first entry in the
/// program for which **glyphCode2** is the next character. If a ligature is
/// found, sets **glyphCode2** with the new code and returns *true*. If a
/// kerning entry is found, it sets the kern parameter with the value
/// in the table and return *false*. If the LigKern pgm is empty or there
/// is no entry for **glyphCode2**, it returns *false*.
///
/// Note: character codes have to be translated to internal GlyphCode before
/// calling this method.
///
/// @param glyphCode1 In. The GlyhCode for which to find a LigKern entry in its program.
/// @param glyphCode2 InOut. The GlyphCode that must appear in the program as the next
///                   character in sequence. Will be replaced with the target
///                   ligature GlyphCode if found.
/// @param kern Out. When a kerning entry is found in the program, kern will receive the value.
/// @param kernFound Out. True if a kerning pair was found.
/// @return True if a ligature was found, false otherwise.
///
auto IBMFFontMod::ligKern(int faceIndex, const GlyphCode glyphCode1, GlyphCode *glyphCode2,
                          FIX16 *kern, bool *kernPairPresent) const -> bool {

  *kern            = 0;
  *kernPairPresent = false;
  //
  const GlyphLigSteps  &ligSteps  = faces_[faceIndex]->glyphsLigKern[glyphCode1]->ligSteps;
  const GlyphKernSteps &kernSteps = faces_[faceIndex]->glyphsLigKern[glyphCode1]->kernSteps;

  if ((ligSteps.size() == 0) && (kernSteps.size() == 0)) {
    return false;
  }

  GlyphCode code = faces_[faceIndex]->glyphs[*glyphCode2]->mainCode;
  if (preamble_.bits.fontFormat == FontFormat::LATIN) {
    code &= LATIN_GLYPH_CODE_MASK;
  }
  bool first = true;

  for (auto &ligStep : ligSteps) {
    if (ligStep->nextGlyphCode == code) {
      *glyphCode2 = ligStep->replacementGlyphCode;
      return true;
    }
  }

  for (auto &kernStep : kernSteps) {
    if (kernStep->nextGlyphCode == code) {
      FIX16 k = kernStep->kern;
      if (k & 0x2000) k |= 0xC000;
      *kern            = k;
      *kernPairPresent = true;
      return false;
    }
  }
  return false;
}

auto IBMFFontMod::getGlyphLigKern(int faceIndex, int glyphCode, GlyphLigKernPtr *glyphLigKern) const
    -> bool {
  if (faceIndex >= preamble_.faceCount) {
    return false;
  }
  if (glyphCode >= faces_[faceIndex]->header->glyphCount) {
    return false;
  }

  *glyphLigKern = faces_[faceIndex]->glyphsLigKern[glyphCode];

  return true;
}

auto IBMFFontMod::getGlyph(int faceIndex, int glyphCode, GlyphInfoPtr &glyph_info,
                           BitmapPtr *bitmap) const -> bool {
  if (faceIndex >= preamble_.faceCount) return false;
  if (glyphCode > faces_[faceIndex]->header->glyphCount) {
    return false;
  }

  int glyphIndex = glyphCode;

  glyph_info     = faces_[faceIndex]->glyphs[glyphIndex];
  *bitmap        = faces_[faceIndex]->bitmaps[glyphIndex];

  return true;
}

auto IBMFFontMod::convertToOneBit(const Bitmap &bitmapHeightBits, BitmapPtr *bitmapOneBit) -> bool {
  *bitmapOneBit        = BitmapPtr(new Bitmap);
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

// In the process of optimizing the size of the ligKern table, this method
// search to find if a part of the already prepared list contains the same
// steps as per the pgm received as a parameter. If so, the index of the
// similar list of steps is returned, else -1
auto IBMFFontMod::findList(std::vector<LigKernStepPtr> &pgm,
                           std::vector<LigKernStepPtr> &list) const -> int {

  auto pred = [](const LigKernStepPtr &e1, const LigKernStepPtr &e2) -> bool {
    return (e1->a.whole.val == e2->a.whole.val) && (e1->b.whole.val == e2->b.whole.val);
  };

  auto it = std::search(list.begin(), list.end(), pgm.begin(), pgm.end(), pred);
  return (it == list.end()) ? -1 : std::distance(list.begin(), it);
}

// For all faces:
//
// - Retrieves all ligature and kerning for each face glyphs, setting the
// index in the integrated vector, optimizing the glyphs' list to reuse the
// ones that are similar
//
// - If there is some series with index beyond 254, create goto entries. All
// starting indexes must be before 255
auto IBMFFontMod::prepareLigKernVectors() -> bool {
  for (auto &face : faces_) {

    auto &lkSteps = face->ligKernSteps;

    lkSteps.clear();

    std::set<int> overflowList;     // List of starting pgm index that are larger than 254
    std::set<int> uniquePgmIndexes; // List of all unique start indexes

    // Working list for glyphs pgm vector reconstruction
    // = -1 if a glyph's Lig/Kern pgm is empty
    // < -1 if it has been relocated
    std::vector<int>            glyphsPgmIndexes(face->header->glyphCount, -1);
    std::vector<LigKernStepPtr> glyphPgm;

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

      LigKernStepPtr lks = nullptr;

      auto lSteps        = face->glyphsLigKern[glyphIdx]->ligSteps;
      auto kSteps        = face->glyphsLigKern[glyphIdx]->kernSteps;

      glyphPgm.clear();
      glyphPgm.reserve(lSteps.size() + kSteps.size());

      // clang-format off
        for (auto &lStep : lSteps) {
          lks = LigKernStepPtr(new LigKernStep({
            .a = {.data = {.nextGlyphCode = lStep->nextGlyphCode, .stop = false}},
            .b = {.repl = {.replGlyphCode = lStep->replacementGlyphCode, .isAKern = false}}
          }));
          glyphPgm.push_back(lks);
        }

        for (auto &kStep : kSteps) {
          lks = LigKernStepPtr(new LigKernStep({
            .a = { .data = {.nextGlyphCode = kStep->nextGlyphCode, .stop = false}},
            .b = {.kern = {.kerningValue = (FIX14)kStep->kern, .isAGoTo = false, .isAKern = true}}
          }));
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
          glyphPgm.clear();
          glyphsPgmIndexes[glyphIdx] = -sameIdx; // negative to signify a duplicate list
          uniquePgmIndexes.insert(sameIdx);
        } else {
          int index = lkSteps.size();
          uniquePgmIndexes.insert(index);
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

    int spaceRequired = 0;
    int newLigKernIdx = 0;
    for (auto idx = uniquePgmIndexes.rbegin(); idx != uniquePgmIndexes.rend(); idx++) {
      if ((*idx + spaceRequired) >= 255) {
        overflowList.insert(*idx);
        spaceRequired += 1;
      } else {
        if (spaceRequired > 0) {
          overflowList.insert(*idx);
          spaceRequired += 1;
          newLigKernIdx = *idx;
        }
        break;
      }
    }

    for (auto idx = overflowList.rbegin(); idx != overflowList.rend(); idx++) {
      // std::cout << *idx << " treatment: " << std::endl;
      LigKernStepPtr ligKernStep = LigKernStepPtr(new LigKernStep);
      memset(ligKernStep.get(), 0, sizeof(LigKernStep));
      ligKernStep->b.goTo.isAKern      = true;
      ligKernStep->b.goTo.isAGoTo      = true;
      ligKernStep->b.goTo.displacement = (*idx + spaceRequired);

      // std::cout << "Added goto at location " << *idx << " to point at location "
      //           << (*idx + spaceRequired) << std::endl;

      lkSteps.insert(lkSteps.begin() + newLigKernIdx, ligKernStep);
      int gCode = 0;
      for (auto pgmIdx = glyphsPgmIndexes.begin(); pgmIdx != glyphsPgmIndexes.end(); pgmIdx++) {
        if (abs(*pgmIdx) == *idx) { // Must look at both duplicated and
                                    // non-duplicated indexes
          // std::cout << "Entry " << gCode << " pointing at " << *pgmIdx << " redirected to "
          //           << (-5000 - newLigKernIdx) << std::endl;
          *pgmIdx = -5000 - newLigKernIdx;
        }
        gCode++;
      }
      newLigKernIdx++;
    } // for

    glyphIdx = 0;
    for (auto &glyph : face->glyphs) {
      if (glyphsPgmIndexes[glyphIdx] == -1) {
        glyph->ligKernPgmIndex = 255;
      } else {
        if ((abs(glyphsPgmIndexes[glyphIdx]) >= 255) && (abs(glyphsPgmIndexes[glyphIdx]) < 5000)) {
          QMessageBox::warning(nullptr, "Logic Error",
                               "A logic error was encoutered in method "
                               "prepareLigKernVectors() "
                               "-> computed LigKern PGM index >= 255!!");
          return false;
        }
        if (abs(glyphsPgmIndexes[glyphIdx]) >= 5000) {
          glyph->ligKernPgmIndex = abs(glyphsPgmIndexes[glyphIdx]) - 5000;
        } else {
          glyph->ligKernPgmIndex = abs(glyphsPgmIndexes[glyphIdx]);
        }
      }
      glyphIdx += 1;
    }
  } // for each face

  return true;
}

auto IBMFFontMod::toGlyphCode(char32_t codePoint) const -> GlyphCode {

  GlyphCode glyphCode = NO_GLYPH_CODE;

  uint16_t planeIdx   = static_cast<uint16_t>(codePoint >> 16);

  if (planeIdx <= 3) {
    char16_t u16                = static_cast<char16_t>(codePoint);

    uint16_t codePointBundleIdx = planes_[planeIdx].codePointBundlesIdx;
    uint16_t entriesCount       = planes_[planeIdx].entriesCount;
    int      gCode              = planes_[planeIdx].firstGlyphCode;
    int      i                  = 0;
    while (i < entriesCount) {
      if (u16 <= codePointBundles_[codePointBundleIdx].lastCodePoint) {
        break;
      }
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

/**
 * @brief Translate UTF32 codePoint to it's internal representation
 *
 * All supported character sets must use this method to get the internal
 * glyph code correponding to the CodePoint. For the LATIN font format,
 * the glyph code contains both the glyph index and accented information. For
 * UTF32 font format, it contains the index of the glyph in all faces that are
 * part of the font.
 *
 * FontFormat 0 - Latin Char Set Translation
 * -----------------------------------------
 *
 * The class allow for latin1+ CodePoints to be plotted into a bitmap. As the
 * font doesn't contains all accented glyphs, a combination of glyphs must be
 * used to draw a single bitmap. This method translate some of the supported
 * CodePoint values to that combination. The least significant byte will contains
 * the main glyph code index and the next byte will contain the accent code.
 *
 * The supported UTF16 CodePoints are the following:
 *
 *      U+0020 to U+007F
 *      U+00A1 to U+017f
 *
 *   and the following:
 *
 * |  UTF16  | Description          |
 * |:-------:|----------------------|
 * | U+02BB  | reverse apostrophe
 * | U+02BC  | apostrophe
 * | U+02C6  | circumflex
 * | U+02DA  | ring
 * | U+02DC  | tilde ~
 * | U+2013  | endash (Not available with CM Typewriter)
 * | U+2014  | emdash (Not available with CM Typewriter)
 * | U+2018  | quote left
 * | U+2019  | quote right
 * | U+201A  | comma like ,
 * | U+201C  | quoted left "
 * | U+201D  | quoted right
 * | U+2032  | minute '
 * | U+2033  | second "
 * | U+2044  | fraction /
 * | U+20AC  | euro
 *
 * Font Format 1 - UTF32 translation
 * ---------------------------------
 *
 * Using the Planes and CodePointBundles structure, retrieves the
 * glyph code corresponding to the CodePoint.
 *
 * @param codePoint The UTF32 character code
 * @return The internal representation of CodePoint
 */
auto IBMFFontMod::translate(char32_t codePoint) const -> GlyphCode {
  GlyphCode glyphCode = SPACE_CODE;

  if (preamble_.bits.fontFormat == FontFormat::LATIN) {
    if ((codePoint > 0x20) && (codePoint < 0x7F)) {
      glyphCode = codePoint; // ASCII codes No accent
    } else if ((codePoint >= 0xA1) && (codePoint <= 0x1FF)) {
      glyphCode = latinTranslationSet[codePoint - 0xA1];
    } else {
      switch (codePoint) {
        case 0x2013: // endash
          glyphCode = 0x0015;
          break;
        case 0x2014: // emdash
          glyphCode = 0x0016;
          break;
        case 0x2018: // quote left
        case 0x02BB: // reverse apostrophe
          glyphCode = 0x0060;
          break;
        case 0x2019: // quote right
        case 0x02BC: // apostrophe
          glyphCode = 0x0027;
          break;
        case 0x201C: // quoted left "
          glyphCode = 0x0010;
          break;
        case 0x201D: // quoted right
          glyphCode = 0x0011;
          break;
        case 0x02C6: // circumflex
          glyphCode = 0x005E;
          break;
        case 0x02DA: // ring
          glyphCode = 0x0006;
          break;
        case 0x02DC: // tilde ~
          glyphCode = 0x007E;
          break;
        case 0x201A: // comma like ,
          glyphCode = 0x000D;
          break;
        case 0x2032: // minute '
          glyphCode = 0x0027;
          break;
        case 0x2033: // second "
          glyphCode = 0x0022;
          break;
        case 0x2044: // fraction /
          glyphCode = 0x002F;
          break;
        case 0x20AC: // euro
          glyphCode = 0x00AD;
          break;
      }
    }
  } else if (preamble_.bits.fontFormat == FontFormat::UTF32) {
    uint16_t planeIdx = static_cast<uint16_t>(codePoint >> 16);

    if (planeIdx <= 3) {
      char16_t u16                = static_cast<char16_t>(codePoint);

      uint16_t codePointBundleIdx = planes_[planeIdx].codePointBundlesIdx;
      uint16_t entriesCount       = planes_[planeIdx].entriesCount;
      int      gCode              = planes_[planeIdx].firstGlyphCode;
      int      i                  = 0;
      while (i < entriesCount) {
        if (u16 <= codePointBundles_[codePointBundleIdx].lastCodePoint) {
          break;
        }
        gCode += (codePointBundles_[codePointBundleIdx].lastCodePoint -
                  codePointBundles_[codePointBundleIdx].firstCodePoint + 1);
        i++;
        codePointBundleIdx++;
      }
      if ((i < entriesCount) && (u16 >= codePointBundles_[codePointBundleIdx].firstCodePoint)) {
        glyphCode = gCode + u16 - codePointBundles_[codePointBundleIdx].firstCodePoint;
      }
    }
  }

  return glyphCode;
}

// Returns the corresponding UTF32 character for the glyphCode.
auto IBMFFontMod::getUTF32(GlyphCode glyphCode) const -> char32_t {
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

// Returns true if the received character is not part of the control characters nor one of the
// space characters as defined in Unicode.
auto IBMFFontMod::charSelected(char32_t ch, SelectedBlockIndexesPtr &selectedBlockIndexes) const
    -> bool {
  // Don't populate with space and non-break-space characters
  if ((ch >= 0x0021) && (ch != 0x00A0) && ((ch < 0x02000) || (ch > 0x200F)) &&
      ((ch < 0x02028) || (ch > 0x202F)) && ((ch < 0x0205F) || (ch > 0x206F))) {
    for (auto selectedBlock : *selectedBlockIndexes) {
      if ((ch >= uBlocks[selectedBlock].first_) && (ch <= uBlocks[selectedBlock].last_)) {
        return true;
      }
    }
  }
  return false;
}

auto IBMFFontMod::prepareCodePlanes(FT_Face &face, CharSelections &charSelections) -> int {

  uint16_t glyphCode = 0;

  if (charSelections.size() == 1) {

    for (int i = 0; i < 4; i++) {
      planes_.push_back(Plane({0, 0, 0}));
    }

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
            CodePointBundle bundle = CodePointBundle({.firstCodePoint = u16, .lastCodePoint = u16});
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

#define FT_TYPEOF(type)       (__typeof__(type))
#define FT_PIX_FLOOR(x)       ((x) & ~FT_TYPEOF(x) 63)
#define FT_PIX_ROUND(x)       FT_PIX_FLOOR((x) + 32)

#define LITTLE_ENDIEN_16(val) val = (val << 8) | (val >> 8);

auto IBMFFontMod::retrieveKernPairsTable(FT_Face ftFace) -> void {
  kernPairs       = nullptr;
  kernPairsCount  = 0;

  FT_ULong length = sizeof(KernTableHeader);
  FT_Error error = FT_Load_Sfnt_Table(ftFace, TTAG_kern, 0, (FT_Byte *)(&kernTableHeader), &length);
  if (error == 0) {
    LITTLE_ENDIEN_16(kernTableHeader.nTables);
    int offset = sizeof(KernTableHeader);
    for (uint16_t i = 0; i < kernTableHeader.nTables; i++) {
      length = sizeof(KernSubTableHeader);
      error =
          FT_Load_Sfnt_Table(ftFace, TTAG_kern, offset, (FT_Byte *)(&kernSubTableHeader), &length);
      if (error == 0) {
        if (kernSubTableHeader.coverage.data.format == 0) {
          LITTLE_ENDIEN_16(kernSubTableHeader.nPairs);
          LITTLE_ENDIEN_16(kernSubTableHeader.length);
          length =
              kernSubTableHeader.length - (sizeof(KernSubTableHeader) + sizeof(KernFormat0Header));
          offset += sizeof(KernSubTableHeader) + sizeof(KernFormat0Header);
          kernPairs = (KernPairsPtr) new uint8_t[length];
          error = FT_Load_Sfnt_Table(ftFace, TTAG_kern, offset, (FT_Byte *)(kernPairs), &length);
          if (error == 0) {
            kernPairsCount = length / sizeof(KernPair);
            for (int i = 0; i < kernPairsCount; i++) {
              LITTLE_ENDIEN_16(kernPairs[i].first);
              LITTLE_ENDIEN_16(kernPairs[i].next);
              LITTLE_ENDIEN_16(kernPairs[i].value);
            }
            break;
          } else {
            kernPairs = nullptr;
            break;
          }
        } else {
          offset += kernSubTableHeader.length;
        }
      } else {
        break;
      }
    }
  }
}

auto IBMFFontMod::findGlyphCodeFromIndex(int index, FT_Face ftFace, int glyphCount) -> GlyphCode {
  for (GlyphCode glyphCode = 0; glyphCode < glyphCount; glyphCode++) {
    char32_t ch2 = getUTF32(glyphCode);
    FT_UInt  idx = FT_Get_Char_Index(ftFace, ch2);
    if ((idx != 0) && (idx == index)) return glyphCode;
  }
  return NO_GLYPH_CODE;
}
auto IBMFFontMod::loadTTF(FreeType &ft, FontParametersPtr fontParameters) -> bool {

  clear();

  std::vector<uint8_t> pointSizes;

  if (fontParameters->pt8) pointSizes.push_back(8);
  if (fontParameters->pt9) pointSizes.push_back(9);
  if (fontParameters->pt10) pointSizes.push_back(10);
  if (fontParameters->pt12) pointSizes.push_back(12);
  if (fontParameters->pt14) pointSizes.push_back(14);
  if (fontParameters->pt17) pointSizes.push_back(17);
  if (fontParameters->pt24) pointSizes.push_back(24);
  if (fontParameters->pt48) pointSizes.push_back(48);

  // ----- Preamble -----

  uint8_t faceCount = pointSizes.size();

  // clang-format off
    preamble_ = {
      .marker    = {'I', 'B', 'M', 'F'},
      .faceCount = faceCount,
      .bits      = {.version = IBMF_VERSION, .fontFormat = FontFormat::UTF32}
    };
  // clang-format on

  CharSelectionsPtr sel = fontParameters->charSelections;
  if (sel->size() == 1) {
    QString filename = (*sel)[0].filename;

    FT_Face ftFace;
    if ((ftFace = ft.openFace(filename)) != nullptr) {

      // ----- Prepare for kerning information retrieval -----

      if (fontParameters->withKerning) {
        retrieveKernPairsTable(ftFace);
      }

      // ----- Build Code Planes structures -----

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
        if (error != 0) {
          QMessageBox::critical(nullptr, "FreeType issue", "Unable to set face sizes");
          return false;
        }

        FacePtr face = FacePtr(new Face);

        // ----- For each glyphCode part of the new font -----

        for (GlyphCode glyphCode = 0; glyphCode < glyphCount; glyphCode++) {

          char32_t ch    = getUTF32(glyphCode);
          FT_UInt  index = FT_Get_Char_Index(ftFace, ch);
          if (index != 0) {
            error = FT_Load_Char(ftFace, ch, FT_LOAD_DEFAULT);
            if (error != 0) {
              QMessageBox::critical(
                  nullptr, "FreeType issue",
                  QString("Unable to load codePoint U+%1").arg(ch, 5, 16, QChar('0')));
              return false;
            }

            if (ftFace->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
              error = FT_Render_Glyph(ftFace->glyph, FT_RENDER_MODE_MONO);
              if (error != 0) {
                QMessageBox::critical(
                    nullptr, "FreeType issue",
                    QString("Unable to render codePoint U+%1").arg(ch, 5, 16, QChar('0')));
                return false;
              }
            }

            // ----- Bitmap -----

            BitmapPtr bitmap = BitmapPtr(new Bitmap());
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

            // ----- Ligature / Kerning -----

            GlyphLigKernPtr glyphLigKern = GlyphLigKernPtr(new GlyphLigKern);

            // Create ligatures for the glyph if available
            // Ensure that both next and replacement glyph codes are present in the
            // resulting IBMF font
            char32_t firstChar = getUTF32(glyphCode);
            for (auto &ligature : ligatures) {
              if (ligature.firstChar == firstChar) {
                GlyphCode nextGlyphCode        = toGlyphCode(ligature.nextChar);
                GlyphCode replacementGlyphCode = toGlyphCode(ligature.replacement);
                if ((nextGlyphCode != NO_GLYPH_CODE) && (replacementGlyphCode != NO_GLYPH_CODE)) {
                  GlyphLigStepPtr glyphLigStep = GlyphLigStepPtr(
                      new GlyphLigStep(GlyphLigStep{.nextGlyphCode        = nextGlyphCode,
                                                    .replacementGlyphCode = replacementGlyphCode}));
                  glyphLigKern->ligSteps.push_back(glyphLigStep);
                }
              }
            }

            if (fontParameters->withKerning) {

              // Retrieve kerning information for each pair defined in the font, need
              // first to find if the second char is present in this IBMF Font. if so,
              // create an entry for it

              for (int i = 0; i < kernPairsCount; i++) {
                if (kernPairs[i].first == index) {
                  GlyphCode glyphCode2 =
                      findGlyphCodeFromIndex(kernPairs[i].next, ftFace, glyphCount);
                  if (glyphCode2 != NO_GLYPH_CODE) {

                    FT_Vector akerning;
                    FT_Get_Kerning(ftFace, FT_Get_Char_Index(ftFace, getUTF32(glyphCode)),
                                   FT_Get_Char_Index(ftFace, getUTF32(glyphCode2)),
                                   FT_KERNING_DEFAULT, &akerning);

                    auto kern = static_cast<FIX16>(akerning.x);

                    if (kern != 0) {
                      GlyphKernStepPtr glyphKernStep = GlyphKernStepPtr(new GlyphKernStep(
                          GlyphKernStep{.nextGlyphCode = glyphCode2, .kern = kern}));
                      glyphLigKern->kernSteps.push_back(glyphKernStep);
                    }
                  }
                }
              }
            }

            face->glyphsLigKern.push_back(glyphLigKern);

            // ----- Glyph Info -----

            GlyphInfoPtr glyphInfo = GlyphInfoPtr(new GlyphInfo(GlyphInfo{
                .bitmapWidth      = static_cast<uint8_t>(ftFace->glyph->bitmap.width),
                .bitmapHeight     = static_cast<uint8_t>(ftFace->glyph->bitmap.rows),
                .horizontalOffset = static_cast<int8_t>(-ftFace->glyph->bitmap_left),
                .verticalOffset   = static_cast<int8_t>(ftFace->glyph->bitmap_top),
                .packetLength =
                    static_cast<uint16_t>(ftFace->glyph->bitmap.width * ftFace->glyph->bitmap.rows),
                .advance         = static_cast<FIX16>(ftFace->glyph->advance.x),
                .rleMetrics      = RLEMetrics{.dynF = 0, .firstIsBlack = false, .filler = 0},
                .ligKernPgmIndex = 0, // completed at save time
                .mainCode        = glyphCode  // maybe changed below when searching for composites
            }));

            face->glyphs.push_back(glyphInfo);
          } else {
            QMessageBox::critical(
                nullptr, "Internal error!",
                QString("Can't find utf32 codePoint for glyphCode %1)").arg(glyphCode));
            return false;
          }
        }

        // ----- Check for composite information -----

        // If the main composite element is having a kerning table associated with it, it will
        // be duplicated to the composed codePoint.

        for (GlyphCode glyphCode = 0; glyphCode < glyphCount; glyphCode++) {

          char32_t ch = getUTF32(glyphCode);
          // FT_UInt  index = FT_Get_Char_Index(ftFace, ch);
          (void)FT_Load_Char(ftFace, ch, FT_LOAD_NO_RECURSE);

          if (ftFace->glyph->format == FT_GLYPH_FORMAT_COMPOSITE) {
            //            std::cout << "Glyph composite of index: " << index << ", glyphCode " <<
            //            glyphCode
            //                      << std::endl;
            for (int i = 0; i < ftFace->glyph->num_subglyphs; i++) {
              FT_Int    p_index;
              FT_UInt   p_flags;
              FT_Int    p_arg1;
              FT_Int    p_arg2;
              FT_Matrix p_transform;
              FT_Get_SubGlyph_Info(ftFace->glyph, i, &p_index, &p_flags, &p_arg1, &p_arg2,
                                   &p_transform);
              if ((p_flags & 2) && (p_arg1 == 0) && (p_arg2 == 0)) {
                // We have a main component
                GlyphCode code = findGlyphCodeFromIndex(p_index, ftFace, glyphCount);

                if (code != NO_GLYPH_CODE) {
                  face->glyphs[glyphCode]->mainCode = code;
                  // std::cout << "Composite main code: " << code << " for glyphCode " << glyphCode
                  //           << "(U+" << std::hex << std::setfill('0') << std::setw(5) << ch
                  //           << std::dec << ")" << std::endl;
                  if (face->glyphsLigKern[glyphCode]->kernSteps.size() == 0) {
                    std::copy(face->glyphsLigKern[code]->kernSteps.begin(),
                              face->glyphsLigKern[code]->kernSteps.end(),
                              std::back_inserter(face->glyphsLigKern[glyphCode]->kernSteps));
                    // if (face->glyphsLigKern[glyphCode]->kernSteps.size() != 0) {
                    //  std::cout << "Got a main composite with augmented kern vector: "
                    //            << glyphCode << std::endl;
                    // }
                  }
                }
              }
            }
          }
        }

        // ----- Retrieve the xHeight -----

        TT_PCLT_ pclt;
        FIX16    xHeight = 0;
        error            = FT_Load_Sfnt_Table(ftFace, TTAG_PCLT, 0, (FT_Byte *)(&pclt), nullptr);
        if (error == 0) {
          xHeight = static_cast<FIX16>(pclt.xHeight * (ftFace->size->metrics.x_scale / 1024.0));
        } else {
          FT_UInt index = FT_Get_Char_Index(ftFace, 'x');

          if (index != 0) {
            FT_Load_Char(ftFace, 'x', FT_LOAD_MONOCHROME);
            xHeight = static_cast<FIX16>(ftFace->glyph->metrics.height);
          } else {
            QMessageBox::warning(nullptr, "No 'x' character",
                                 "There is no 'x' character in this font");
          }
        }

        // ----- Retrieve the EM size -----

        FIX16 emSize = static_cast<FIX16>(ftFace->size->metrics.x_ppem << 6);

        // ----- Retrieve the space size -----

        FT_UInt spaceIndex = FT_Get_Char_Index(ftFace, ' ');
        uint8_t spaceSize  = emSize / 2;
        if (spaceIndex != 0) {
          FT_Load_Char(ftFace, ' ', FT_LOAD_NO_BITMAP);
          spaceSize = static_cast<uint8_t>(ftFace->glyph->metrics.horiAdvance >> 6);
        } else {
          QMessageBox::warning(nullptr, "No space character",
                               "There is no space character in this font");
        }

        // ----- Face Header -----

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
