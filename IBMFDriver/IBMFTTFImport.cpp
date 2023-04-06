#include "IBMFTTFImport.hpp"

#include <QMessageBox>

auto IBMFTTFImport::prepareCodePlanes(FT_Face &face, CharSelections &charSelections) -> int {

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

// Returns true if the received character is not part of the control characters nor one of the
// space characters as defined in Unicode.
auto IBMFTTFImport::charSelected(char32_t ch, SelectedBlockIndexesPtr &selectedBlockIndexes) const
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

#define FT_TYPEOF(type) (__typeof__(type))
#define FT_PIX_FLOOR(x) ((x) & ~FT_TYPEOF(x) 63)
#define FT_PIX_ROUND(x) FT_PIX_FLOOR((x) + 32)

#define LITTLE_ENDIEN_16(val) val = (val << 8) | (val >> 8);

auto IBMFTTFImport::retrieveKernPairsTable(FT_Face ftFace) -> void {
  kernPairs      = nullptr;
  kernPairsCount = 0;

  FT_ULong length = sizeof(KernTableHeader);
  FT_Error error =
      FT_Load_Sfnt_Table(ftFace, TTAG_kern, 0, (FT_Byte *) (&kernTableHeader), &length);
  if (error == 0) {
    LITTLE_ENDIEN_16(kernTableHeader.nTables);
    int offset = sizeof(KernTableHeader);
    for (uint16_t i = 0; i < kernTableHeader.nTables; i++) {
      length = sizeof(KernSubTableHeader);
      error =
          FT_Load_Sfnt_Table(ftFace, TTAG_kern, offset, (FT_Byte *) (&kernSubTableHeader), &length);
      if (error == 0) {
        if (kernSubTableHeader.coverage.data.format == 0) {
          LITTLE_ENDIEN_16(kernSubTableHeader.nPairs);
          LITTLE_ENDIEN_16(kernSubTableHeader.length);
          length =
              kernSubTableHeader.length - (sizeof(KernSubTableHeader) + sizeof(KernFormat0Header));
          offset += sizeof(KernSubTableHeader) + sizeof(KernFormat0Header);
          kernPairs = (KernPairsPtr) new uint8_t[length];
          error = FT_Load_Sfnt_Table(ftFace, TTAG_kern, offset, (FT_Byte *) (kernPairs), &length);
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

auto IBMFTTFImport::findGlyphCodeFromIndex(int index, FT_Face ftFace, int glyphCount) -> GlyphCode {
  for (GlyphCode glyphCode = 0; glyphCode < glyphCount; glyphCode++) {
    char32_t ch2 = getUTF32(glyphCode);
    FT_UInt  idx = FT_Get_Char_Index(ftFace, ch2);
    if ((idx != 0) && (idx == index)) return glyphCode;
  }
  return NO_GLYPH_CODE;
}

auto IBMFTTFImport::loadTTF(FreeType &ft, FontParametersPtr fontParameters) -> bool {

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

      if (fontParameters->withKerning) { retrieveKernPairsTable(ftFace); }

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
          (void) FT_Load_Char(ftFace, ch, FT_LOAD_NO_RECURSE);

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
        error            = FT_Load_Sfnt_Table(ftFace, TTAG_PCLT, 0, (FT_Byte *) (&pclt), nullptr);
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
