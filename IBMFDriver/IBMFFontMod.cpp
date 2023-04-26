#include "IBMFFontMod.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>

#include <QIODevice>
#include <QMessageBox>

void IBMFFontMod::clear() {
  initialized_ = false;
  for (auto &face : faces_) {
    for (auto bitmap : face->bitmaps) { bitmap->clear(); }
    for (auto bitmap : face->compressedBitmaps) { bitmap->clear(); }
    face->glyphs.clear();
    face->glyphsBackup.clear();
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

    if (preamble_.bits.fontFormat == FontFormat::BACKUP) {
      for (int glyphCode = 0; glyphCode < header->glyphCount; glyphCode++) {
        GlyphBackupInfoPtr glyphBackupInfo = GlyphBackupInfoPtr(new GlyphBackupInfo);
        memcpy(glyphBackupInfo.get(), &memory_[idx], sizeof(GlyphBackupInfo));
        idx += sizeof(GlyphBackupInfo);

        int       bitmap_size = glyphBackupInfo->bitmapHeight * glyphBackupInfo->bitmapWidth;
        BitmapPtr bitmap      = BitmapPtr(new Bitmap);
        bitmap->pixels        = Pixels(bitmap_size, 0);
        bitmap->dim           = Dim(glyphBackupInfo->bitmapWidth, glyphBackupInfo->bitmapHeight);

        RLEBitmapPtr compressedBitmap = RLEBitmapPtr(new RLEBitmap);
        compressedBitmap->dim         = bitmap->dim;
        compressedBitmap->pixels.reserve(glyphBackupInfo->packetLength);
        compressedBitmap->length = glyphBackupInfo->packetLength;
        for (int pos = 0; pos < glyphBackupInfo->packetLength; pos++) {
          compressedBitmap->pixels.push_back(
              (*pixelsPool)[pos + (*glyphsPixelPoolIndexes)[glyphCode]]);
        }

        RLEExtractor rle;
        rle.retrieveBitmap(*compressedBitmap, *bitmap, Pos(0, 0), glyphBackupInfo->rleMetrics);
        // retrieveBitmap(idx, glyphInfo.get(), *bitmap, Pos(0,0));

        face->glyphsBackup.push_back(glyphBackupInfo);
        face->bitmaps.push_back(bitmap);
        face->compressedBitmaps.push_back(compressedBitmap);

        // idx += glyphInfo->packetLength;
      }
    } else {
      for (int glyphCode = 0; glyphCode < header->glyphCount; glyphCode++) {
        GlyphInfoPtr glyphInfo = GlyphInfoPtr(new GlyphInfo);
        memcpy(glyphInfo.get(), &memory_[idx], sizeof(GlyphInfo));
        idx += sizeof(GlyphInfo);

        int       bitmap_size = glyphInfo->bitmapHeight * glyphInfo->bitmapWidth;
        BitmapPtr bitmap      = BitmapPtr(new Bitmap);
        bitmap->pixels        = Pixels(bitmap_size, 0);
        bitmap->dim           = Dim(glyphInfo->bitmapWidth, glyphInfo->bitmapHeight);

        RLEBitmapPtr compressedBitmap = RLEBitmapPtr(new RLEBitmap);
        compressedBitmap->dim         = bitmap->dim;
        compressedBitmap->pixels.reserve(glyphInfo->packetLength);
        compressedBitmap->length = glyphInfo->packetLength;
        for (int pos = 0; pos < glyphInfo->packetLength; pos++) {
          compressedBitmap->pixels.push_back(
              (*pixelsPool)[pos + (*glyphsPixelPoolIndexes)[glyphCode]]);
        }

        RLEExtractor rle;
        rle.retrieveBitmap(*compressedBitmap, *bitmap, Pos(0, 0), glyphInfo->rleMetrics);
        // retrieveBitmap(idx, glyphInfo.get(), *bitmap, Pos(0,0));

        face->glyphs.push_back(glyphInfo);
        face->bitmaps.push_back(bitmap);
        face->compressedBitmaps.push_back(compressedBitmap);

        // idx += glyphInfo->packetLength;
      }
    }

    if (&memory_[idx] != (uint8_t *) pixelsPool) { return false; }

    idx += header->pixelsPoolSize;

    if (header->ligKernStepCount > 0) {
      face->ligKernSteps.reserve(header->ligKernStepCount);
      for (int j = 0; j < header->ligKernStepCount; j++) {
        LigKernStep step;
        memcpy(&step, &memory_[idx], sizeof(LigKernStep));

        face->ligKernSteps.push_back(step);

        idx += sizeof(LigKernStep);
      }
    }

    for (GlyphCode glyphCode = 0; glyphCode < header->glyphCount; glyphCode++) {
      GlyphLigKernPtr glk = GlyphLigKernPtr(new GlyphLigKern);

      if (face->glyphs[glyphCode]->ligKernPgmIndex != 255) {
        int lk_idx = face->glyphs[glyphCode]->ligKernPgmIndex;
        if (lk_idx < header->ligKernStepCount) {
          if ((face->ligKernSteps[lk_idx].b.goTo.isAGoTo) &&
              (face->ligKernSteps[lk_idx].b.kern.isAKern)) {
            lk_idx = face->ligKernSteps[lk_idx].b.goTo.displacement;
          }
          do {
            if (face->ligKernSteps[lk_idx].b.kern.isAKern) { // true = kern, false = ligature
              glk->kernSteps.push_back(
                  GlyphKernStep{.nextGlyphCode = face->ligKernSteps[lk_idx].a.data.nextGlyphCode,
                                .kern          = face->ligKernSteps[lk_idx].b.kern.kerningValue});
            } else {
              glk->ligSteps.push_back(GlyphLigStep{
                  .nextGlyphCode        = face->ligKernSteps[lk_idx].a.data.nextGlyphCode,
                  .replacementGlyphCode = face->ligKernSteps[lk_idx].b.repl.replGlyphCode});
            }
          } while (!face->ligKernSteps[lk_idx++].a.data.stop);
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
  if (out.writeRawData((char *) v, size) == -1) {                                                  \
    lastError_ = 1;                                                                                \
    return false;                                                                                  \
  }

#define WRITE2(v, size)                                                                            \
  if (out.writeRawData((char *) v, size) == -1) {                                                  \
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

  int fill = 4 - ((sizeof(Preamble) + preamble_.faceCount) & 3);
  if (fill == 4) { fill = 0; }
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

    if (preamble_.bits.fontFormat == FontFormat::BACKUP) {
      for (auto &glyph : face->glyphsBackup) {
        if (face->bitmaps[idx]->dim.width == 0) {
          glyph->rleMetrics.dynF         = 14;
          glyph->rleMetrics.firstIsBlack = false;
          glyph->packetLength            = 0;
          poolIndexes->push_back(0);
          idx += 1;
        } else {
          RLEGenerator *gen = new RLEGenerator;
          if (!gen->encodeBitmap(face->bitmaps[idx++])) {
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
      }
    } else {
      for (auto &glyph : face->glyphs) {
        if (face->bitmaps[idx]->dim.width == 0) {
          glyph->rleMetrics.dynF         = 14;
          glyph->rleMetrics.firstIsBlack = false;
          glyph->packetLength            = 0;
          poolIndexes->push_back(0);
          idx += 1;
        } else {
          RLEGenerator *gen = new RLEGenerator;
          if (!gen->encodeBitmap(face->bitmaps[idx++])) {
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
      }
    }

    fill = 4 - (poolData->size() + (sizeof(GlyphInfo) * face->header->glyphCount) &
                3); // to keep alignment to 32bits offsets
    if (fill == 4) fill = 0;

    face->header->pixelsPoolSize   = poolData->size() + fill;
    face->header->ligKernStepCount = face->ligKernSteps.size();

    WRITE2(face->header.get(), sizeof(FaceHeader));

    for (auto idx : *poolIndexes) { WRITE2(&idx, sizeof(uint32_t)); }

    if (preamble_.bits.fontFormat == FontFormat::BACKUP) {
      for (auto &glyph : face->glyphsBackup) {
        WRITE2(glyph.get(), sizeof(GlyphBackupInfo));
        glyphCount++;
      }
    } else {
      for (auto &glyph : face->glyphs) {
        WRITE2(glyph.get(), sizeof(GlyphInfo));
        glyphCount++;
      }
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
    while (fill--) { WRITE2(&filler, 1); }

    poolIndexes->clear();
    poolData->clear();
    delete poolData;
    delete poolIndexes;

    int ligKernCount = 0;
    for (auto ligKern : face->ligKernSteps) {
      WRITE(&ligKern, sizeof(LigKernStep));
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

auto IBMFFontMod::findFace(uint8_t pointSize) -> FacePtr {

  for (auto &face : faces_) {
    if (face->header->pointSize == pointSize) return face;
  }
  return nullptr;
}

auto IBMFFontMod::findGlyphIndex(FacePtr face, char32_t codePoint) -> int {

  int idx = 0;
  for (auto &glyph : face->glyphsBackup) {
    if (glyph->codePoint == codePoint) return idx;
    idx++;
  }
  return -1;
}

auto IBMFFontMod::saveGlyph(int faceIndex, int glyphCode, GlyphInfoPtr newGlyphInfo,
                            BitmapPtr newBitmap, GlyphLigKernPtr glyphLigKern, IBMFFontMod *font)
    -> bool {

  if (preamble_.bits.fontFormat == FontFormat::BACKUP) {
    FaceHeaderPtr fontFaceHeader = font->getFaceHeader(faceIndex);
    FacePtr       face           = findFace(fontFaceHeader->pointSize);
    if (face == nullptr) {
      FaceHeaderPtr backupFaceHeader = std::make_shared<FaceHeader>(*fontFaceHeader);
      backupFaceHeader->glyphCount   = 0;
      face                           = std::make_shared<Face>();
      face->header                   = backupFaceHeader;
      faces_.push_back(face);
      preamble_.faceCount += 1;
    }
    GlyphBackupInfoPtr glyphBackupInfo = GlyphBackupInfoPtr(
        new GlyphBackupInfo{.bitmapWidth      = newGlyphInfo->bitmapWidth,
                            .bitmapHeight     = newGlyphInfo->bitmapHeight,
                            .horizontalOffset = newGlyphInfo->horizontalOffset,
                            .verticalOffset   = newGlyphInfo->verticalOffset,
                            .packetLength     = newGlyphInfo->packetLength,
                            .advance          = newGlyphInfo->advance,
                            .rleMetrics       = newGlyphInfo->rleMetrics,
                            .ligKernPgmIndex  = newGlyphInfo->ligKernPgmIndex,
                            .mainCodePoint    = font->getUTF32(newGlyphInfo->mainCode),
                            .codePoint        = font->getUTF32(glyphCode)});

    int idx = findGlyphIndex(face, glyphBackupInfo->codePoint);
    if (idx != -1) {
      face->glyphsBackup[idx]  = glyphBackupInfo;
      face->bitmaps[idx]       = newBitmap;
      face->glyphsLigKern[idx] = glyphLigKern;
    } else {
      face->glyphsBackup.push_back(glyphBackupInfo);
      face->bitmaps.push_back(newBitmap);
      face->glyphsLigKern.push_back(glyphLigKern);
      face->header->glyphCount += 1;
    }
  } else {
    if ((faceIndex >= preamble_.faceCount) || (glyphCode < 0) ||
        (glyphCode >= faces_[faceIndex]->header->glyphCount)) {
      return false;
    }

    faces_[faceIndex]->glyphs[glyphCode]        = newGlyphInfo;
    faces_[faceIndex]->bitmaps[glyphCode]       = newBitmap;
    faces_[faceIndex]->glyphsLigKern[glyphCode] = glyphLigKern;
  }

  return true;
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
                          FIX16 *kern, bool *kernPairPresent, GlyphLigKernPtr bypassLigKern) const
    -> bool {

  *kern            = 0;
  *kernPairPresent = false;

  if ((faceIndex >= preamble_.faceCount) || (glyphCode1 < 0) ||
      (glyphCode1 >= faces_[faceIndex]->header->glyphCount) || (*glyphCode2 < 0) ||
      (*glyphCode2 >= faces_[faceIndex]->header->glyphCount)) {
    return false;
  }

  //
  GlyphLigSteps  *ligSteps;
  GlyphKernSteps *kernSteps;

  if (bypassLigKern == nullptr) {
    ligSteps  = &faces_[faceIndex]->glyphsLigKern[glyphCode1]->ligSteps;
    kernSteps = &faces_[faceIndex]->glyphsLigKern[glyphCode1]->kernSteps;
  } else {
    ligSteps  = &bypassLigKern->ligSteps;
    kernSteps = &bypassLigKern->kernSteps;
  }

  if ((ligSteps->size() == 0) && (kernSteps->size() == 0)) { return false; }

  GlyphCode code = faces_[faceIndex]->glyphs[*glyphCode2]->mainCode;
  if (preamble_.bits.fontFormat == FontFormat::LATIN) { code &= LATIN_GLYPH_CODE_MASK; }
  bool first = true;

  for (auto &ligStep : *ligSteps) {
    if (ligStep.nextGlyphCode == *glyphCode2) {
      *glyphCode2 = ligStep.replacementGlyphCode;
      return true;
    }
  }

  for (auto &kernStep : *kernSteps) {
    if (kernStep.nextGlyphCode == code) {
      FIX16 k = kernStep.kern;
      if (k & 0x2000) k |= 0xC000;
      *kern            = k;
      *kernPairPresent = true;
      return false;
    }
  }
  return false;
}

auto IBMFFontMod::getGlyph(int faceIndex, int glyphCode, GlyphInfoPtr &glyphInfo, BitmapPtr &bitmap,
                           GlyphLigKernPtr &glyphLigKern) const -> bool {

  if ((faceIndex >= preamble_.faceCount) || (glyphCode < 0) ||
      (glyphCode >= faces_[faceIndex]->header->glyphCount)) {
    return false;
  }

  int glyphIndex = glyphCode;

  glyphInfo    = std::make_shared<GlyphInfo>(*faces_[faceIndex]->glyphs[glyphIndex]);
  bitmap       = std::make_shared<Bitmap>(*faces_[faceIndex]->bitmaps[glyphIndex]);
  glyphLigKern = std::make_shared<GlyphLigKern>(*faces_[faceIndex]->glyphsLigKern[glyphCode]);

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
auto IBMFFontMod::findList(std::vector<LigKernStep> &pgm, std::vector<LigKernStep> &list) const
    -> int {

  auto pred = [](const LigKernStep &e1, const LigKernStep &e2) -> bool {
    return (e1.a.whole.val == e2.a.whole.val) && (e1.b.whole.val == e2.b.whole.val);
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
    std::vector<int>         glyphsPgmIndexes(face->header->glyphCount, -1);
    std::vector<LigKernStep> glyphPgm;

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

      auto &lSteps = face->glyphsLigKern[glyphIdx]->ligSteps;
      auto &kSteps = face->glyphsLigKern[glyphIdx]->kernSteps;

      glyphPgm.clear();
      glyphPgm.reserve(lSteps.size() + kSteps.size());

      for (auto &lStep : lSteps) {
        glyphPgm.push_back(LigKernStep{
            .a = {.data = {.nextGlyphCode = lStep.nextGlyphCode, .stop = false}},
            .b = {.repl = {.replGlyphCode = lStep.replacementGlyphCode, .isAKern = false}}});
      }

      for (auto &kStep : kSteps) {
        glyphPgm.push_back(LigKernStep{
            .a = {.data = {.nextGlyphCode = kStep.nextGlyphCode, .stop = false}},
            .b = {
                .kern = {.kerningValue = (FIX14) kStep.kern, .isAGoTo = false, .isAKern = true}}});
      }

      if (glyphPgm.size() == 0) {
        glyphsPgmIndexes[glyphIdx] = -1; // empty list
      } else {
        glyphPgm[glyphPgm.size() - 1].a.data.stop = true;
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
      LigKernStep ligKernStep;
      memset(&ligKernStep, 0, sizeof(LigKernStep));
      ligKernStep.b.goTo.isAKern      = true;
      ligKernStep.b.goTo.isAGoTo      = true;
      ligKernStep.b.goTo.displacement = (*idx + spaceRequired);

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
    if (glyphCode < fontFormat0CodePoints.size()) { codePoint = fontFormat0CodePoints[glyphCode]; }
  }
  return codePoint;
}
