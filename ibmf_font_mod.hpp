#pragma once

#include <cstring>

#include "defs.hpp"

#include <QChar>
#include <QDataStream>
#include <QIODevice>

#include "rle_generator.hpp"

#define DEBUG 0

#if DEBUG
  #include <iostream>
  #include <iomanip>
#endif

// These are the corresponding Unicode value for each of the 174 characters that are part of
// an IBMF Font;
const QChar characterCodes[] = {
    QChar(0x0060), // `
    QChar(0x00B4), // ´
    QChar(0x02C6), // ˆ
    QChar(0x02DC), // ˜
    QChar(0x00A8), // ¨
    QChar(0x02DD), // ˝
    QChar(0x02DA), // ˚
    QChar(0x02C7), // ˇ
    QChar(0x02D8), // ˘
    QChar(0x00AF), // ¯
    QChar(0x02D9), // ˙
    QChar(0x00B8), // ¸
    QChar(0x02DB), // ˛
    QChar(0x201A), // ‚
    QChar(0x2039), // ‹
    QChar(0x203A), // ›
    
    QChar(0x201C), // “
    QChar(0x201D), // ”
    QChar(0x201E), // „
    QChar(0x00AB), // «
    QChar(0x00BB), // »
    QChar(0x2013), // –
    QChar(0x2014), // —
    QChar(0x00BF), // ¿
    QChar(0x2080), // ₀
    QChar(0x0131), // ı
    QChar(0x0237), // ȷ
    QChar(0xFB00), // ﬀ
    QChar(0xFB01), // ﬁ
    QChar(0xFB02), // ﬂ
    QChar(0xFB03), // ﬃ
    QChar(0xFB04), // ﬄ
    
    QChar(0x00A1), // ¡
    QChar(0x0021), // !
    QChar(0x0022), // "
    QChar(0x0023), // #
    QChar(0x0024), // $
    QChar(0x0025), // %
    QChar(0x0026), // &
    QChar(0x2019), // ’
    QChar(0x0028), // (
    QChar(0x0029), // )
    QChar(0x002A), // *
    QChar(0x002B), // +
    QChar(0x002C), // ,
    QChar(0x002D), // .
    QChar(0x002E), // -
    QChar(0x002F), // /
    
    QChar(0x0030), // 0
    QChar(0x0031), // 1
    QChar(0x0032), // 2
    QChar(0x0033), // 3
    QChar(0x0034), // 4
    QChar(0x0035), // 5
    QChar(0x0036), // 6
    QChar(0x0037), // 7
    QChar(0x0038), // 8
    QChar(0x0039), // 9
    QChar(0x003A), // :
    QChar(0x003B), // ;
    QChar(0x003C), // <
    QChar(0x003D), // =
    QChar(0x003E), // >
    QChar(0x003F), // ?
    
    QChar(0x0040), // @
    QChar(0x0041), // A
    QChar(0x0042), // B
    QChar(0x0043), // C
    QChar(0x0044), // D
    QChar(0x0045), // E
    QChar(0x0046), // F
    QChar(0x0047), // G
    QChar(0x0048), // H
    QChar(0x0049), // I
    QChar(0x004A), // J
    QChar(0x004B), // K
    QChar(0x004C), // L
    QChar(0x004D), // M
    QChar(0x004E), // N
    QChar(0x004F), // O

    QChar(0x0050), // P
    QChar(0x0051), // Q
    QChar(0x0052), // R
    QChar(0x0053), // S
    QChar(0x0054), // T
    QChar(0x0055), // U
    QChar(0x0056), // V
    QChar(0x0057), // W
    QChar(0x0058), // X
    QChar(0x0059), // Y
    QChar(0x005A), // Z
    QChar(0x005B), // [
    QChar(0x005C), // \ .
    QChar(0x005D), // ]
    QChar(0x005E), // ^
    QChar(0x005F), // _

    QChar(0x2018), // ‘
    QChar(0x0061), // a
    QChar(0x0062), // b
    QChar(0x0063), // c
    QChar(0x0064), // d
    QChar(0x0065), // e
    QChar(0x0066), // f
    QChar(0x0067), // g
    QChar(0x0068), // h
    QChar(0x0069), // i
    QChar(0x006A), // j
    QChar(0x006B), // k
    QChar(0x006C), // l
    QChar(0x006D), // m
    QChar(0x006E), // n
    QChar(0x006F), // o

    QChar(0x0070), // p
    QChar(0x0071), // q
    QChar(0x0072), // r
    QChar(0x0073), // s
    QChar(0x0074), // t
    QChar(0x0075), // u
    QChar(0x0076), // v
    QChar(0x0077), // w
    QChar(0x0078), // x
    QChar(0x0079), // y
    QChar(0x007A), // z
    QChar(0x007B), // {
    QChar(0x007C), // |
    QChar(0x007D), // }
    QChar(0x007E), // ~
    QChar(0x013D), // Ľ

    QChar(0x0141), // Ł
    QChar(0x014A), // Ŋ
    QChar(0x0132), // Ĳ
    QChar(0x0111), // đ
    QChar(0x00A7), // §
    QChar(0x010F), // ď
    QChar(0x013E), // ľ
    QChar(0x0142), // ł
    QChar(0x014B), // ŋ
    QChar(0x0165), // ť
    QChar(0x0133), // ĳ
    QChar(0x00A3), // £
    QChar(0x00C6), // Æ
    QChar(0x00D0), // Ð
    QChar(0x0152), // Œ
    QChar(0x00D8), // Ø

    QChar(0x00DE), // Þ
    QChar(0x1E9E), // ẞ
    QChar(0x00E6), // æ
    QChar(0x00F0), // ð
    QChar(0x0153), // œ
    QChar(0x00F8), // ø
    QChar(0x00FE), // þ
    QChar(0x00DF), // ß
    QChar(0x00A2), // ¢
    QChar(0x00A4), // ¤
    QChar(0x00A5), // ¥
    QChar(0x00A6), // ¦
    QChar(0x00A9), // ©
    QChar(0x00AA), // ª
    QChar(0x00AC), // ¬
    QChar(0x00AE), // ®

    QChar(0x00B1), // ±
    QChar(0x00B2), // ²
    QChar(0x00B3), // ³
    QChar(0x00B5), // µ
    QChar(0x00B6), // ¶
    QChar(0x00B7), // ·
    QChar(0x00B9), // ¹
    QChar(0x00BA), // º
    QChar(0x00D7), // ×
    QChar(0x00BC), // ¼
    QChar(0x00BD), // ½
    QChar(0x00BE), // ¾
    QChar(0x00F7), // ÷
    QChar(0x20AC)  // €
};
 
/**
 * @brief Access to a IBMF font.
 * 
 * This is a class to allow for the modification of a IBMF font generated from METAFONT
 * 
 */
class IBMFFontMod
{
  public:
    typedef int16_t FIX16;

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

    #pragma pack(push, 1)

      
      // The lig kern array contains instructions (struct LibKernStep) in a simple programming 
      // language that explains what to do for special letter pairs. The information in squared
      // brackets relate to fields that are part of the LibKernStep struct. Each entry in this 
      // array is a lig kern command of four bytes:
      //
      // - first byte: skip byte, indicates that this is the final program step if the byte 
      //                          is 128 or more, otherwise the next step is obtained by 
      //                          skipping this number of intervening steps [next_step_relative].
      // - second byte: next char, if next character follows the current character, then 
      //                           perform the operation and stop, otherwise continue.
      // - third byte: op byte, indicates a ligature step if less than 128, a kern step otherwise.
      // - fourth byte: remainder.
      //
      // In a kern step [is_a_kern == true], an additional space equal to kern located at
      // [(displ_high << 8) + displ_low] in the kern array is inserted between the current 
      // character and [next_char]. This amount is often negative, so that the characters 
      // are brought closer together by kerning; but it might be positive.
      //
      // There are eight kinds of ligature steps [is_a_kern == false], having op byte codes 
      // [a_op b_op c_op] where 0 ≤ a_op ≤ b_op + c_op and 0 ≤ b_op, c_op ≤ 1.
      //
      // The character whose code is [replacement_char] is inserted between the current 
      // character and [next_char]; then the current character is deleted if b_op = 0, and 
      // [next_char] is deleted if c_op = 0; then we pass over a_op characters to reach the next 
      // current character (which may have a ligature/kerning program of its own).
      //
      // If the very first instruction of a character’s lig kern program has [whole > 128], 
      // the program actually begins in location [(displ_high << 8) + displ_low]. This feature 
      // allows access to large lig kern arrays, because the first instruction must otherwise 
      // appear in a location ≤ 255.
      //
      // Any instruction with [whole > 128] in the lig kern array must have 
      // [(displ_high << 8) + displ_low] < the size of the array. If such an instruction is 
      // encountered during normal program execution, it denotes an unconditional halt; no
      // ligature or kerning command is performed.
      //
      // (The following usage has been extracted from the lig/kern array as not being used outside
      //  of a TeX generated document)
      //
      // If the very first instruction of the lig kern array has [whole == 0xFF], the 
      // [next_char] byte is the so-called right boundary character of this font; the value 
      // of [next_char] need not lie between char codes boundaries. 
      //
      // If the very last instruction of the lig kern array has [whole == 0xFF], there is 
      // a special ligature/kerning program for a left boundary character, beginning at location 
      // [(displ_high << 8) + displ_low] . The interpretation is that TEX puts implicit boundary 
      // characters before and after each consecutive string of characters from the same font.
      // These implicit characters do not appear in the output, but they can affect ligatures 
      // and kerning.
      //

      union SkipByte {
        unsigned int   whole:8;
        struct {
          unsigned int next_step_relative:7;
          bool         stop:1;
        } s;
      };

      union OpCodeByte {
        struct {
          bool         c_op:1;
          bool         b_op:1;
          unsigned int a_op:5;
          bool         is_a_kern:1;
        } op;
        struct {
          unsigned int displ_high:7;
          bool         is_a_kern:1;
        } d;
      };

      union RemainderByte {
        unsigned int replacement_char:8;
        unsigned int displ_low:8;  // Ligature: replacement char code, kern: displacement
      };

      struct LigKernStep {
        SkipByte      skip;
        uint8_t       next_char;
        OpCodeByte    op_code;
        RemainderByte remainder;
      };    

      // ----

      struct GlyphMetric {
        unsigned int          dyn_f:4;
        unsigned int first_is_black:1;
        unsigned int         filler:3;
      };

      struct GlyphInfo {
        uint8_t             char_code;
        uint8_t          bitmap_width;
        uint8_t         bitmap_height;
        int8_t      horizontal_offset;
        int8_t        vertical_offset;
        uint8_t    lig_kern_pgm_index; // = 65535 if none
        uint16_t        packet_length;
        FIX16                 advance;
        GlyphMetric      glyph_metric;
      };
      typedef std::shared_ptr<GlyphInfo>  GlyphInfoPtr;

      // ----

      struct Preamble {
        char         marker[4];
        uint8_t     face_count;
        struct {
          uint8_t    version:5;
          uint8_t   char_set:3;
        } bits;
      };

      struct FaceHeader {
        uint8_t    point_size;
        uint8_t    line_height;
        uint16_t   dpi;
        FIX16      x_height;
        FIX16      em_size;
        FIX16      slant_correction;
        uint8_t    descender_height;
        uint8_t    space_size;
        uint16_t   glyph_count;
        uint16_t   lig_kern_pgm_count;
        uint8_t    kern_count;
      };
      typedef std::shared_ptr<FaceHeader> FaceHeaderPtr;

    #pragma pack(pop)

    struct Face {
      FaceHeaderPtr               header;
      std::vector<GlyphInfoPtr>   glyphs;
      std::vector<Bitmap *>       bitmaps;
      std::vector<Bitmap *>       compressed_bitmaps;
      std::vector<LigKernStep *>  lig_kern_steps;
      std::vector<FIX16>          kerns;
      std::vector<GlyphLigKern *> glyphs_lig_kern;
    };

    typedef std::unique_ptr<Face> FacePtr;

  private:
    static constexpr uint8_t MAX_GLYPH_COUNT = 254; // Index Value 0xFE and 0xFF are reserved
    static constexpr uint8_t IBMF_VERSION    =   3;

    bool initialized;

    std::vector<uint32_t> face_offsets;
    std::vector<FacePtr>  faces;

    uint8_t     * memory;
    uint32_t      memory_length;

    uint8_t     * memory_ptr;
    uint8_t     * memory_end;

    uint32_t      repeat_count;

    Preamble      preamble;

    int           last_error;

    static constexpr uint8_t PK_REPEAT_COUNT =   14;
    static constexpr uint8_t PK_REPEAT_ONCE  =   15;

    bool
    getnext8(uint8_t & val)
    {
      if (memory_ptr >= memory_end) return false;  
      val = *memory_ptr++;
      return true;
    }

    uint8_t nybble_flipper = 0xf0U;
    uint8_t nybble_byte;

    bool
    get_nybble(uint8_t & nyb)
    {
      if (nybble_flipper == 0xf0U) {
        if (!getnext8(nybble_byte)) return false;
        nyb = nybble_byte >> 4;
      }
      else {
        nyb = (nybble_byte & 0x0f);
      }
      nybble_flipper ^= 0xffU;
      return true;
    }

    /// @brief Retrieve a number from a packed RLE bitmap
    /// @param value - The returned value found at the next location in the bitmap
    /// @param glyph - The glyph where the bitmap is located
    /// @return true: the value contains the next number. False: format error or at the end of the bitmap.
    ///
    /// Pseudo-code:
    ///
    /// Translated to C++ from: https://tug.ctan.org/info/knuth-pdf/mfware/gftopk.pdf
    ///
    /// function pk_packed_num: integer;
    /// var i,j,k: integer;
    /// begin 
    ///   i := get_nyb;
    ///   if i = 0 then begin 
    ///     repeat 
    ///       j := getnyb; incr(i);
    ///     until j != 0;
    ///     while i > 0 do begin 
    ///       j := j * 16 + get_nyb; 
    ///       decr(i);
    ///     end;
    ///     pk_packed_num := j - 15 + (13 - dyn_f) * 16 + dyn_f;
    ///   end
    ///   else if i <= dyn_f then 
    ///     pk_packed_num := i
    ///   else if i < 14 then 
    ///     pk_packed_num := (i - dyn_f - 1) * 16 + get_nyb + dyn_f + 1
    ///   else begin 
    ///     if repeat_count != 0 then abort('Extra repeat count!');
    ///     if i = 14 then
    ///        repeat_count := pk_packed_num
    ///     else
    ///        repeat_count := 1;
    ///     send_out(true, repeat_count);
    ///     pk_packed_num := pk_packed_num;
    ///   end;
    /// end;

    bool
    get_packed_number(uint32_t & val, const GlyphInfo & glyph)
    {
      uint8_t  nyb;
      uint32_t i, j;

      uint8_t dyn_f = glyph.glyph_metric.dyn_f;

      while (true) {
        if (!get_nybble(nyb)) return false; 
        i = nyb;
        if (i == 0) {
          do {
            if (!get_nybble(nyb)) return false;
            i++;
          } while (nyb == 0);
          j = nyb;
          while (i-- > 0) {
            if (!get_nybble(nyb)) return false;
            j = (j << 4) + nyb;
          }
          val = j - 15 + ((13 - dyn_f) << 4) + dyn_f;
          break;
        }
        else if (i <= dyn_f) {
          val = i;
          break;
        }
        else if (i < PK_REPEAT_COUNT) {
          if (!get_nybble(nyb)) return false;
          val = ((i - dyn_f - 1) << 4) + nyb + dyn_f + 1;
          break;
        }
        else { 
          if (i == PK_REPEAT_COUNT) {
            if (!get_packed_number(repeat_count, glyph)) return false;
          }
          else { // i == PK_REPEAT_ONCE
            repeat_count = 1;
          }
        }
      }
      return true;
    }

    bool
    retrieve_bitmap(uint32_t idx, GlyphInfo * glyph_info, Bitmap & bitmap, Pos offsets)
    {
      // point on the glyphs' bitmap definition
      memory_ptr = &memory[idx];
      int rowp;

      uint32_t  row_size = bitmap.dim.width;
      rowp = offsets.y * row_size;

      repeat_count   = 0;
      nybble_flipper = 0xf0U;

      if (glyph_info->glyph_metric.dyn_f == 14) {  // is a bitmap?
        uint32_t  count = 8;
        uint8_t   data;

        for (int row = 0;
             row < (glyph_info->bitmap_height);
             row++, rowp += row_size) {
          for (int col = offsets.x;
               col < (glyph_info->bitmap_width + offsets.x);
               col++) {
            if (count >= 8) {
              if (!getnext8(data)) {
                return false;
              }
              count = 0;
            }
            bitmap.pixels[rowp + col] = (data & (0x80U >> count)) ? 0xFF : 0;
            count++;
          }
        }
      }
      else {
        uint32_t count = 0;

        repeat_count   = 0;
        nybble_flipper = 0xf0U;

        bool black = !(glyph_info->glyph_metric.first_is_black == 1);

        for (uint32_t row = 0;
             row < (glyph_info->bitmap_height);
             row++, rowp += row_size) {
          for (uint32_t col = offsets.x;
               col < (glyph_info->bitmap_width + offsets.x);
               col++) {
            if (count == 0) {
              if (!get_packed_number(count, *glyph_info)) {
                return false;
              }
              black = !black;
            }
            if (black) bitmap.pixels[rowp + col] = 0xFF;
            count--;
          }

          while ((row < bitmap.dim.height) && (repeat_count-- > 0)) {
            memcpy(&bitmap.pixels[rowp + row_size], &bitmap.pixels[rowp], row_size);
            row++;
            rowp += row_size;
          }

          repeat_count = 0;
        }
      }

      return true;
    }

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

          Bitmap * compressed_bitmap = new Bitmap;
          compressed_bitmap->dim = bitmap->dim;
          compressed_bitmap->pixels.reserve(glyph_info->packet_length);
          for (int pos = 0; pos < glyph_info->packet_length; pos++) {
            compressed_bitmap->pixels.push_back(memory[idx + pos]);
          }
          retrieve_bitmap(idx, glyph_info.get(), *bitmap, Pos(0,0));

          face->glyphs.push_back(glyph_info);
          face->bitmaps.push_back(bitmap);
          face->compressed_bitmaps.push_back(compressed_bitmap);

          idx += glyph_info->packet_length;
        }

        if (header->lig_kern_pgm_count > 0) {
          face->lig_kern_steps.reserve(header->lig_kern_pgm_count);
          for (int j = 0; j < header->lig_kern_pgm_count; j++) {
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
            if (lk_idx < header->lig_kern_pgm_count) {
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
          delete bitmap;
        }
        for (auto bitmap : face->compressed_bitmaps) {
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
      if (glyph_code >= faces[face_index]->header->glyph_count) return false;

      *glyph_lig_kern = faces[face_index]->glyphs_lig_kern[glyph_code];

      return true;
    }

    bool
    get_glyph(int face_index, int glyph_code, GlyphInfoPtr & glyph_info, Bitmap ** bitmap)
    {
      if (face_index >= preamble.face_count) return false;
      if (glyph_code >= faces[face_index]->header->glyph_count) return false;

      glyph_info = faces[face_index]->glyphs[glyph_code];
      *bitmap = faces[face_index]->bitmaps[glyph_code];
      
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
      if ((face_index < preamble.face_count) && (glyph_code < faces[face_index]->header->glyph_count)) {
        *faces[face_index]->glyphs[glyph_code] = *new_glyph_info;
        delete faces[face_index]->bitmaps[glyph_code];
        faces[face_index]->bitmaps[glyph_code] = new_bitmap;
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
          glyph->glyph_metric.dyn_f = gen->get_dyn_f();
          glyph->glyph_metric.first_is_black = gen->get_first_is_black();
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

        if (lig_kern_count != face->header->lig_kern_pgm_count) {
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
