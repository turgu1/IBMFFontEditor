#pragma once

#include <cstring>

#include "ibmf_defs.hpp"
using namespace IBMFDefs;

#include <QChar>
#include <QDataStream>
#include <QIODevice>

#include "rle_generator.hpp"
#include "rle_extractor.hpp"

#define DEBUG 0

#if DEBUG
  #include <iostream>
  #include <iomanip>
#endif


/**
 * @brief Access to a IBMF font.
 * 
 * This is a class to allow for the modification of a IBMF font generated from METAFONT
 * 
 */
class IBMFFontMod
{
  public:
    struct GlyphKernStep {
      unsigned int nextcharCode;
      FIX16 kern;
    };

    struct GlyphLigStep {
      unsigned int nextcharCode;
      unsigned int charCode;
    };

    struct GlyphLigKern {
      std::vector<GlyphLigStep *>  lig_steps;
      std::vector<GlyphKernStep *> kern_steps;
    };

    struct Face {
      FaceHeaderPtr               header;
      std::vector<GlyphInfoPtr>   glyphs;
      std::vector<Bitmap *>       bitmaps;
      std::vector<RLEBitmap *>    compressed_bitmaps;
      std::vector<LigKernStep *>  lig_kern_steps;
      std::vector<FIX16>          kerns;
      std::vector<GlyphLigKern *> glyphs_lig_kern;
    };

    typedef std::unique_ptr<Face> FacePtr;

  private:
    static constexpr uint8_t MAX_GLYPH_COUNT = 254; // Index Value 0xFE and 0xFF are reserved
    static constexpr uint8_t IBMF_VERSION    =   4;

    bool initialized;

    Preamble      preamble;

    std::vector<uint32_t> face_offsets;
    std::vector<FacePtr>  faces;

    uint8_t     * memory;
    uint32_t      memory_length;

    uint8_t     * memoryPtr;
    uint8_t     * memoryEnd;

    //uint32_t      repeatCount;


    int           last_error;

    bool
    load()
    {
      memcpy(&preamble, memory, sizeof(Preamble));
      if (strncmp("IBMF", preamble.marker, 4) != 0) return false;
      if (preamble.bits.version != IBMF_VERSION) return false;

      int idx = sizeof(Preamble);

      for (int i = 0; i < preamble.faceCount; i++) {
        uint32_t offset = *((uint32_t *) &memory[idx]);
        face_offsets.push_back(offset);
        idx += 4;
      }

      for (int i = 0; i < preamble.faceCount; i++) {
        uint32_t      idx    = face_offsets[i];
        FacePtr       face   = FacePtr(new Face);
        FaceHeaderPtr header = FaceHeaderPtr(new FaceHeader);
        memcpy(header.get(), &memory[idx], sizeof(FaceHeader));
        idx += sizeof(FaceHeader);

        face->glyphs.reserve(header->glyphCount);

        for (int j = 0; j < header->glyphCount; j++) {
          GlyphInfoPtr glyph_info = GlyphInfoPtr(new GlyphInfo);
          memcpy(glyph_info.get(), &memory[idx], sizeof(GlyphInfo));
          idx += sizeof(GlyphInfo);

          int bitmap_size = glyph_info->bitmapHeight * glyph_info->bitmapWidth;
          Bitmap * bitmap = new Bitmap;
          bitmap->pixels = Pixels(bitmap_size, 0);
          bitmap->dim = Dim(glyph_info->bitmapWidth, glyph_info->bitmapHeight);

          RLEBitmap * compressed_bitmap = new RLEBitmap;
          compressed_bitmap->dim = bitmap->dim;
          compressed_bitmap->pixels.reserve(glyph_info->packetLength);
          compressed_bitmap->length = glyph_info->packetLength;
          for (int pos = 0; pos < glyph_info->packetLength; pos++) {
            compressed_bitmap->pixels.push_back(memory[idx + pos]);
          }

          RLEExtractor rle;
          rle.retrieveBitmap(*compressed_bitmap, *bitmap, Pos(0,0), glyph_info->rleMetrics);
          //retrieveBitmap(idx, glyph_info.get(), *bitmap, Pos(0,0));

          face->glyphs.push_back(glyph_info);
          face->bitmaps.push_back(bitmap);
          face->compressed_bitmaps.push_back(compressed_bitmap);

          idx += glyph_info->packetLength;
        }

        if (header->ligKernStepCount > 0) {
          face->lig_kern_steps.reserve(header->ligKernStepCount);
          for (int j = 0; j < header->ligKernStepCount; j++) {
            LigKernStep * step = new LigKernStep;
            memcpy(step, &memory[idx], sizeof(LigKernStep));

            face->lig_kern_steps.push_back(step);

            idx += sizeof(LigKernStep);
          }
        }

        if (header->kernCount > 0) {
          face->kerns.reserve(header->kernCount);
          for (int j = 0; j < header->kernCount; j++) {
            FIX16 val;
            memcpy(&val, &memory[idx], sizeof(FIX16));

            face->kerns.push_back(val);

            idx += sizeof(FIX16);
          }
        }

        for (int ch = 0; ch < header->glyphCount; ch++) {
          GlyphLigKern * glk = new GlyphLigKern;

          if (face->glyphs[ch]->ligKernPgmIndex != 255) {
            int lk_idx = face->glyphs[ch]->ligKernPgmIndex;
            if (lk_idx < header->ligKernStepCount) {
              if (face->lig_kern_steps[lk_idx]->skip.whole == 255) {
                lk_idx = (face->lig_kern_steps[lk_idx]->opCode.d.displHigh << 8) +
                         face->lig_kern_steps[lk_idx]->remainder.displLow;
              }
              do {
                if (face->lig_kern_steps[lk_idx]->opCode.d.isAKern) { // true = kern, false = ligature
                  GlyphKernStep * step = new GlyphKernStep;
                  step->nextcharCode = face->lig_kern_steps[lk_idx]->nextChar;
                  step->kern = face->kerns[face->lig_kern_steps[lk_idx]->remainder.displLow];
                  glk->kern_steps.push_back(step);
                }
                else {
                  GlyphLigStep * step = new GlyphLigStep;
                  step->nextcharCode = face->lig_kern_steps[lk_idx]->nextChar;
                  step->charCode = face->lig_kern_steps[lk_idx]->remainder.replacementChar;
                  glk->lig_steps.push_back(step);
                }
              } while (!face->lig_kern_steps[lk_idx++]->skip.s.stop);
            }
          }
          face->glyphs_lig_kern.push_back(glk);
        }

        face->header = header;
        faces.push_back(std::move(face));
      }

      return true;
    }

  public:

    IBMFFontMod(uint8_t * memory_font, uint32_t size)
        : memory(memory_font), 
          memory_length(size) {
            
      memoryEnd  = memory + memory_length;
      initialized = load();
      last_error  = 0;
    }

   ~IBMFFontMod() {
      clear();
    }

    void clear() {
      initialized = false;
      for (auto & face : faces) {
        for (auto bitmap : face->bitmaps) {
          bitmap->clear();
          delete bitmap;
        }
        for (auto bitmap : face->compressed_bitmaps) {
          bitmap->clear();
          delete bitmap;
        }
        for (auto lig_kern : face->lig_kern_steps) {
          delete lig_kern;
        }
        for (auto lig_kern : face->glyphs_lig_kern) {
            for (auto lig : lig_kern->lig_steps) delete lig;
            for (auto kern : lig_kern->kern_steps) delete kern;
            delete lig_kern;
        }
        face->glyphs.clear();
        face->bitmaps.clear();
        face->compressed_bitmaps.clear();
        face->lig_kern_steps.clear();
        face->kerns.clear();
      }
      faces.clear();
      face_offsets.clear();
    }

    inline Preamble   get_preample() { return preamble;    }
    inline bool     is_initialized() { return initialized; }
    inline int      get_last_error() { return last_error;  }

    inline const FaceHeaderPtr get_face_header(int face_idx) {
      return faces[face_idx]->header;
    }

    bool get_glyph_lig_kern(int face_index, int glyph_code, GlyphLigKern ** glyph_lig_kern)
    {
      if (face_index >= preamble.faceCount) return false;
      if ((glyph_code > faces[face_index]->header->lastCode) || 
          (glyph_code < faces[face_index]->header->firstCode)) return false;

      *glyph_lig_kern = faces[face_index]->glyphs_lig_kern[glyph_code - faces[face_index]->header->firstCode];

      return true;
    }

    bool
    get_glyph(int face_index, int glyph_code, GlyphInfoPtr & glyph_info, Bitmap ** bitmap)
    {
      if (face_index >= preamble.faceCount) return false;
      if ((glyph_code > faces[face_index]->header->lastCode) || 
          (glyph_code < faces[face_index]->header->firstCode)) return false;

      int glyph_index = glyph_code - faces[face_index]->header->firstCode;

      glyph_info = faces[face_index]->glyphs[glyph_index];
      *bitmap = faces[face_index]->bitmaps[glyph_index];
      
      return true;
    }

    bool
    save_face_header(int face_index, FaceHeader & face_header)
    {
      if (face_index < preamble.faceCount) {
        memcpy(faces[face_index]->header.get(), &face_header, sizeof(FaceHeader));
        return true;
      }
      return false;
    }

    bool
    save_glyph(int face_index, int glyph_code, GlyphInfo * new_glyph_info, Bitmap * new_bitmap)
    {
      if ((face_index < preamble.faceCount) &&  
          ((glyph_code <= faces[face_index]->header->lastCode ) && 
           (glyph_code >= faces[face_index]->header->firstCode))) {

        int glyph_index = glyph_code - faces[face_index]->header->firstCode;

        *faces[face_index]->glyphs[glyph_index] = *new_glyph_info;
        delete faces[face_index]->bitmaps[glyph_index];
        faces[face_index]->bitmaps[glyph_index] = new_bitmap;
        return true;
      }
      return false;
    }

    bool convert_to_one_bit(const Bitmap & bitmapHeight_bits, Bitmap **bitmap_one_bit)
    {
      *bitmap_one_bit = new Bitmap;
      (*bitmap_one_bit)->dim = bitmapHeight_bits.dim;
      auto pix = &(*bitmap_one_bit)->pixels;
      for (int row = 0, idx = 0; row < bitmapHeight_bits.dim.height; row++) {
        uint8_t data = 0;
        uint8_t mask = 0x80;
        for (int col = 0; col < bitmapHeight_bits.dim.width; col++, idx++) {
          data |= bitmapHeight_bits.pixels[idx] == 0 ? 0 : mask;
          mask >>= 1;
          if (mask == 0) {
            pix->push_back(data);
            data = 0;
            mask = 0x80;
          }
        }
        if (mask != 0) pix->push_back(data);
      }
      return pix->size() == (bitmapHeight_bits.dim.height * ((bitmapHeight_bits.dim.width + 7) >> 3));
    }

    #define WRITE(v, size) if (out.writeRawData((char *) v, size) == -1) { last_error = 1; return false; }

    bool
    save(QDataStream & out)
    {
      last_error = 0;
      WRITE(&preamble, sizeof(Preamble));

      uint32_t offset = 0;
      auto offset_pos = out.device()->pos();
      for (int i = 0; i < preamble.faceCount; i++) {
        WRITE(&offset, 4);
      }

      for (auto & face : faces) {
        WRITE(&face->header->pointSize, 1);
      }
      if (preamble.faceCount & 1) {
        char filler = 0;
        WRITE(&filler, 1);
      }

      for (auto & face : faces) {
        // Save current offset position as the location of the font face
        auto pos = out.device()->pos();
        out.device()->seek(offset_pos);
        WRITE(&pos, 4);
        offset_pos += 4;
        out.device()->seek(pos);
        if (out.device()->pos() != pos) {
          last_error = 2;
          return false;
        }

        WRITE(face->header.get(), sizeof(FaceHeader));

        int idx = 0;
        int glyphCount = 0;

        for (auto & glyph : face->glyphs) {
          RLEGenerator * gen = new RLEGenerator;
          if (!gen->encode_bitmap(*face->bitmaps[idx++])) {
            last_error = 3;
            return false;
          }
          glyph->rleMetrics.dynF = gen->get_dynF();
          glyph->rleMetrics.firstIsBlack = gen->get_firstIsBlack();
          auto data = gen->get_data();
          if (data->size() != glyph->packetLength) {
            last_error = 4;
            return false;
          }
          WRITE(glyph.get(), sizeof(GlyphInfo));
          WRITE(data->data(), data->size());
          delete gen;
          glyphCount++;
        }

        if (glyphCount != face->header->glyphCount) {
          last_error = 5;
          return false;
        }

        int lig_kernCount = 0;
        for (auto lig_kern : face->lig_kern_steps) {
          WRITE(lig_kern, sizeof(LigKernStep));
          lig_kernCount += 1;
        }

        if (lig_kernCount != face->header->ligKernStepCount) {
          last_error = 6;
          return false;
        }
        int kernCount = 0;
        for (auto k : face->kerns) {
          WRITE(&k, 2);
          kernCount += 1;
        }

        if (kernCount != face->header->kernCount) {
          last_error = 7;
          return false;
        }
      }
      return true;
    }
};
