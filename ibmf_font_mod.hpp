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
      unsigned int next_char_code;
      FIX16 kern;
    };

    struct GlyphLigStep {
      unsigned int next_char_code;
      unsigned int char_code;
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

    uint8_t     * memory_ptr;
    uint8_t     * memory_end;

    //uint32_t      repeat_count;


    int           last_error;

    bool
    load()
    {
      memcpy(&preamble, memory, sizeof(Preamble));
      if (strncmp("IBMF", preamble.marker, 4) != 0) return false;
      if (preamble.bits.version != IBMF_VERSION) return false;

      int idx = sizeof(Preamble);

      for (int i = 0; i < preamble.face_count; i++) {
        uint32_t offset = *((uint32_t *) &memory[idx]);
        face_offsets.push_back(offset);
        idx += 4;
      }

      for (int i = 0; i < preamble.face_count; i++) {
        uint32_t      idx    = face_offsets[i];
        FacePtr       face   = FacePtr(new Face);
        FaceHeaderPtr header = FaceHeaderPtr(new FaceHeader);
        memcpy(header.get(), &memory[idx], sizeof(FaceHeader));
        idx += sizeof(FaceHeader);

        face->glyphs.reserve(header->glyph_count);

        for (int j = 0; j < header->glyph_count; j++) {
          GlyphInfoPtr glyph_info = GlyphInfoPtr(new GlyphInfo);
          memcpy(glyph_info.get(), &memory[idx], sizeof(GlyphInfo));
          idx += sizeof(GlyphInfo);

          int bitmap_size = glyph_info->bitmap_height * glyph_info->bitmap_width;
          Bitmap * bitmap = new Bitmap;
          bitmap->pixels = Pixels(bitmap_size, 0);
          bitmap->dim = Dim(glyph_info->bitmap_width, glyph_info->bitmap_height);

          RLEBitmap * compressed_bitmap = new RLEBitmap;
          compressed_bitmap->dim = bitmap->dim;
          compressed_bitmap->pixels.reserve(glyph_info->packet_length);
          compressed_bitmap->length = glyph_info->packet_length;
          for (int pos = 0; pos < glyph_info->packet_length; pos++) {
            compressed_bitmap->pixels.push_back(memory[idx + pos]);
          }

          RLEExtractor rle;
          rle.retrieve_bitmap(*compressed_bitmap, *bitmap, Pos(0,0), glyph_info->rle_metrics);
          //retrieve_bitmap(idx, glyph_info.get(), *bitmap, Pos(0,0));

          face->glyphs.push_back(glyph_info);
          face->bitmaps.push_back(bitmap);
          face->compressed_bitmaps.push_back(compressed_bitmap);

          idx += glyph_info->packet_length;
        }

        if (header->lig_kern_step_count > 0) {
          face->lig_kern_steps.reserve(header->lig_kern_step_count);
          for (int j = 0; j < header->lig_kern_step_count; j++) {
            LigKernStep * step = new LigKernStep;
            memcpy(step, &memory[idx], sizeof(LigKernStep));

            face->lig_kern_steps.push_back(step);

            idx += sizeof(LigKernStep);
          }
        }

        if (header->kern_count > 0) {
          face->kerns.reserve(header->kern_count);
          for (int j = 0; j < header->kern_count; j++) {
            FIX16 val;
            memcpy(&val, &memory[idx], sizeof(FIX16));

            face->kerns.push_back(val);

            idx += sizeof(FIX16);
          }
        }

        for (int ch = 0; ch < header->glyph_count; ch++) {
          GlyphLigKern * glk = new GlyphLigKern;

          if (face->glyphs[ch]->lig_kern_pgm_index != 255) {
            int lk_idx = face->glyphs[ch]->lig_kern_pgm_index;
            if (lk_idx < header->lig_kern_step_count) {
              if (face->lig_kern_steps[lk_idx]->skip.whole == 255) {
                lk_idx = (face->lig_kern_steps[lk_idx]->op_code.d.displ_high << 8) +
                         face->lig_kern_steps[lk_idx]->remainder.displ_low;
              }
              do {
                if (face->lig_kern_steps[lk_idx]->op_code.d.is_a_kern) { // true = kern, false = ligature
                  GlyphKernStep * step = new GlyphKernStep;
                  step->next_char_code = face->lig_kern_steps[lk_idx]->next_char;
                  step->kern = face->kerns[face->lig_kern_steps[lk_idx]->remainder.displ_low];
                  glk->kern_steps.push_back(step);
                }
                else {
                  GlyphLigStep * step = new GlyphLigStep;
                  step->next_char_code = face->lig_kern_steps[lk_idx]->next_char;
                  step->char_code = face->lig_kern_steps[lk_idx]->remainder.replacement_char;
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
            
      memory_end  = memory + memory_length;
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
      if (face_index >= preamble.face_count) return false;
      if ((glyph_code > faces[face_index]->header->last_code) || 
          (glyph_code < faces[face_index]->header->first_code)) return false;

      *glyph_lig_kern = faces[face_index]->glyphs_lig_kern[glyph_code - faces[face_index]->header->first_code];

      return true;
    }

    bool
    get_glyph(int face_index, int glyph_code, GlyphInfoPtr & glyph_info, Bitmap ** bitmap)
    {
      if (face_index >= preamble.face_count) return false;
      if ((glyph_code > faces[face_index]->header->last_code) || 
          (glyph_code < faces[face_index]->header->first_code)) return false;

      int glyph_index = glyph_code - faces[face_index]->header->first_code;

      glyph_info = faces[face_index]->glyphs[glyph_index];
      *bitmap = faces[face_index]->bitmaps[glyph_index];
      
      return true;
    }

    bool
    save_face_header(int face_index, FaceHeader & face_header)
    {
      if (face_index < preamble.face_count) {
        memcpy(faces[face_index]->header.get(), &face_header, sizeof(FaceHeader));
        return true;
      }
      return false;
    }

    bool
    save_glyph(int face_index, int glyph_code, GlyphInfo * new_glyph_info, Bitmap * new_bitmap)
    {
      if ((face_index < preamble.face_count) &&  
          ((glyph_code <= faces[face_index]->header->last_code ) && 
           (glyph_code >= faces[face_index]->header->first_code))) {

        int glyph_index = glyph_code - faces[face_index]->header->first_code;

        *faces[face_index]->glyphs[glyph_index] = *new_glyph_info;
        delete faces[face_index]->bitmaps[glyph_index];
        faces[face_index]->bitmaps[glyph_index] = new_bitmap;
        return true;
      }
      return false;
    }

    bool convert_to_one_bit(const Bitmap & bitmap_height_bits, Bitmap **bitmap_one_bit)
    {
      *bitmap_one_bit = new Bitmap;
      (*bitmap_one_bit)->dim = bitmap_height_bits.dim;
      auto pix = &(*bitmap_one_bit)->pixels;
      for (int row = 0, idx = 0; row < bitmap_height_bits.dim.height; row++) {
        uint8_t data = 0;
        uint8_t mask = 0x80;
        for (int col = 0; col < bitmap_height_bits.dim.width; col++, idx++) {
          data |= bitmap_height_bits.pixels[idx] == 0 ? 0 : mask;
          mask >>= 1;
          if (mask == 0) {
            pix->push_back(data);
            data = 0;
            mask = 0x80;
          }
        }
        if (mask != 0) pix->push_back(data);
      }
      return pix->size() == (bitmap_height_bits.dim.height * ((bitmap_height_bits.dim.width + 7) >> 3));
    }

    #define WRITE(v, size) if (out.writeRawData((char *) v, size) == -1) { last_error = 1; return false; }

    bool
    save(QDataStream & out)
    {
      last_error = 0;
      WRITE(&preamble, sizeof(Preamble));

      uint32_t offset = 0;
      auto offset_pos = out.device()->pos();
      for (int i = 0; i < preamble.face_count; i++) {
        WRITE(&offset, 4);
      }

      for (auto & face : faces) {
        WRITE(&face->header->point_size, 1);
      }
      if (preamble.face_count & 1) {
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
        int glyph_count = 0;

        for (auto & glyph : face->glyphs) {
          RLEGenerator * gen = new RLEGenerator;
          if (!gen->encode_bitmap(*face->bitmaps[idx++])) {
            last_error = 3;
            return false;
          }
          glyph->rle_metrics.dyn_f = gen->get_dyn_f();
          glyph->rle_metrics.first_is_black = gen->get_first_is_black();
          auto data = gen->get_data();
          if (data->size() != glyph->packet_length) {
            last_error = 4;
            return false;
          }
          WRITE(glyph.get(), sizeof(GlyphInfo));
          WRITE(data->data(), data->size());
          delete gen;
          glyph_count++;
        }

        if (glyph_count != face->header->glyph_count) {
          last_error = 5;
          return false;
        }

        int lig_kern_count = 0;
        for (auto lig_kern : face->lig_kern_steps) {
          WRITE(lig_kern, sizeof(LigKernStep));
          lig_kern_count += 1;
        }

        if (lig_kern_count != face->header->lig_kern_step_count) {
          last_error = 6;
          return false;
        }
        int kern_count = 0;
        for (auto k : face->kerns) {
          WRITE(&k, 2);
          kern_count += 1;
        }

        if (kern_count != face->header->kern_count) {
          last_error = 7;
          return false;
        }
      }
      return true;
    }
};
