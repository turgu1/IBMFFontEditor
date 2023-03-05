#pragma once

#include <cinttypes>
#include <vector>
#include <QChar>

namespace IBMFDefs {

#ifdef DEBUG_IBMF
  const constexpr int DEBUG = DEBUG_IBMF;
#else
  const constexpr int DEBUG = 0;
#endif

const constexpr uint8_t IBMF_VERSION = 4;
const constexpr uint8_t MAX_GLYPH_COUNT = 254; // Index Value 0xFE and 0xFF are reserved

enum class PixelResolution : uint8_t { ONE_BIT, EIGHT_BITS };

const constexpr PixelResolution resolution = PixelResolution::EIGHT_BITS;

struct Dim {
    uint8_t width;
    uint8_t height;
    Dim(uint8_t w, uint8_t h) : width(w), height(h) {}
    Dim() {}
};

struct Pos {
    int8_t x;
    int8_t y;
    Pos(int8_t xpos, int8_t ypos) : x(xpos), y(ypos) {}
    Pos() {}
};

typedef uint8_t * MemoryPtr;
typedef std::vector<uint8_t> Pixels;
typedef Pixels * PixelsPtr;

struct RLEBitmap {
    Pixels pixels;
    Dim dim;
    uint16_t length;
    void clear() {
        pixels.clear();
        dim = Dim(0, 0);
        length = 0;
    }
};
typedef RLEBitmap *RLEBitmapPtr;

struct Bitmap {
    Pixels pixels;
    Dim dim;
    void clear() {
        pixels.clear();
        dim = Dim(0, 0);
    }
};
typedef Bitmap *BitmapPtr;

#pragma pack(push, 1)

typedef int16_t FIX16;

struct Preamble {
    char marker[4];
    uint8_t face_count;
    struct {
        uint8_t version : 5;
        uint8_t char_set : 3;
    } bits;
};
typedef Preamble *PreamblePtr;

struct FaceHeader {
    uint8_t point_size;
    uint8_t line_height;
    uint16_t dpi;
    FIX16 x_height;
    FIX16 em_size;
    FIX16 slant_correction;
    uint8_t descender_height;
    uint8_t space_size;
    uint16_t glyph_count;
    uint16_t lig_kern_step_count;
    uint16_t first_code;
    uint16_t last_code;
    uint8_t kern_count;
    uint8_t max_height;
};
//typedef FaceHeader *FaceHeaderPtr;
typedef std::shared_ptr<FaceHeader> FaceHeaderPtr;

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
    uint8_t whole : 8;
    struct {
        uint8_t next_step_relative : 7;
        bool stop : 1;
    } s;
};

union OpCodeByte {
    struct {
        bool c_op : 1;
        bool b_op : 1;
        uint8_t a_op : 5;
        bool is_a_kern : 1;
    } op;
    struct {
        uint8_t displ_high : 7;
        bool is_a_kern : 1;
    } d;
};

union RemainderByte {
    uint8_t replacement_char : 8;
    uint8_t displ_low : 8; // Ligature: replacement char code, kern: displacement
};

struct LigKernStep {
    SkipByte skip;
    uint8_t next_char;
    OpCodeByte op_code;
    RemainderByte remainder;
};
typedef LigKernStep *LigKernStepPtr;

struct RLEMetrics {
    uint8_t dyn_f : 4;
    uint8_t first_is_black : 1;
    uint8_t filler : 3;
};

struct GlyphInfo {
    uint8_t char_code;
    uint8_t bitmap_width;
    uint8_t bitmap_height;
    int8_t horizontal_offset;
    int8_t vertical_offset;
    uint8_t lig_kern_pgm_index; // = 255 if none
    uint16_t packet_length;
    FIX16 advance;
    RLEMetrics rle_metrics;
};
//typedef GlyphInfo *GlyphInfoPtr;
typedef std::shared_ptr<GlyphInfo>  GlyphInfoPtr;

#pragma pack(pop)

struct GlyphMetrics {
    int16_t xoff, yoff;
    int16_t advance;
    int16_t line_height;
    int16_t ligature_and_kern_pgm_index;
    void clear() {
        xoff = yoff = 0;
        advance = line_height = 0;
        ligature_and_kern_pgm_index = 255;
    }
};

struct Glyph {
    GlyphMetrics metrics;
    Bitmap bitmap;
    uint8_t point_size;
    void clear() {
        metrics.clear();
        bitmap.clear();
        point_size = 0;
    }
};

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

#if 0
const constexpr uint16_t set2_translation_latin_1[] = {
    /* 0xA1 */ 0xFF20, // ¡
    /* 0xA2 */ 0xFF98, // ¢
    /* 0xA3 */ 0xFF8B, // £
    /* 0xA4 */ 0xFF99, // ¤
    /* 0xA5 */ 0xFF9A, // ¥
    /* 0xA6 */ 0xFF9B, // ¦
    /* 0xA7 */ 0xFF84, // §
    /* 0xA8 */ 0xFF04, // ¨
    /* 0xA9 */ 0xFF9C, // ©
    /* 0xAA */ 0xFF9D, // ª
    /* 0xAB */ 0xFF13, // «
    /* 0xAC */ 0xFF9E, // ¬
    /* 0xAD */ 0xFF2D, // Soft hyphen
    /* 0xAE */ 0xFF9F, // ®
    /* 0xAF */ 0xFF09, // Macron
    /* 0xB0 */ 0xFF06, // ° Degree
    /* 0xB1 */ 0xFFA0, // ±
    /* 0xB2 */ 0xFFA1, // ²
    /* 0xB3 */ 0xFFA2, // ³
    /* 0xB4 */ 0xFF01, // accute accent
    /* 0xB5 */ 0xFFA3, // µ
    /* 0xB6 */ 0xFFA4, // ¶
    /* 0xB7 */ 0xFFA5, // middle dot
    /* 0xB8 */ 0xFF0B, // cedilla
    /* 0xB9 */ 0xFFA6, // ¹
    /* 0xBA */ 0xFFA7, // º
    /* 0xBB */ 0xFF14, // »
    /* 0xBC */ 0xFFA9, // ¼
    /* 0xBD */ 0xFFAA, // ½
    /* 0xBE */ 0xFFAB, // ¾
    /* 0xBF */ 0xFF17, // ¿
    /* 0xC0 */ 0x0041, // À
    /* 0xC1 */ 0x0141, // Á
    /* 0xC2 */ 0x0241, // Â
    /* 0xC3 */ 0x0341, // Ã
    /* 0xC4 */ 0x0441, // Ä
    /* 0xC5 */ 0x0641, // Å
    /* 0xC6 */ 0xFF8C, // Æ
    /* 0xC7 */ 0x0B43, // Ç
    /* 0xC8 */ 0x0045, // È
    /* 0xC9 */ 0x0145, // É
    /* 0xCA */ 0x0245, // Ê
    /* 0xCB */ 0x0445, // Ë
    /* 0xCC */ 0x0049, // Ì
    /* 0xCD */ 0x0149, // Í
    /* 0xCE */ 0x0249, // Î
    /* 0xCF */ 0x0449, // Ï
    /* 0xD0 */ 0xFF8D, // Ð
    /* 0xD1 */ 0x034E, // Ñ
    /* 0xD2 */ 0x004F, // Ò
    /* 0xD3 */ 0x014F, // Ó
    /* 0xD4 */ 0x024F, // Ô
    /* 0xD5 */ 0x034F, // Õ
    /* 0xD6 */ 0x044F, // Ö
    /* 0xD7 */ 0xFFA8, // ×
    /* 0xD8 */ 0xFF8F, // Ø
    /* 0xD9 */ 0x0055, // Ù
    /* 0xDA */ 0x0155, // Ú
    /* 0xDB */ 0x0255, // Û
    /* 0xDC */ 0x0455, // Ü
    /* 0xDD */ 0x0159, // Ý
    /* 0xDE */ 0xFF90, // Þ
    /* 0xDF */ 0xFF97, // ß
    /* 0xE0 */ 0x0061, // à
    /* 0xE1 */ 0x0161, // á
    /* 0xE2 */ 0x0261, // â
    /* 0xE3 */ 0x0361, // ã
    /* 0xE4 */ 0x0461, // ä
    /* 0xE5 */ 0x0661, // å
    /* 0xE6 */ 0xFF92, // æ
    /* 0xE7 */ 0x0B63, // ç
    /* 0xE8 */ 0x0065, // è
    /* 0xE9 */ 0x0165, // é
    /* 0xEA */ 0x0265, // ê
    /* 0xEB */ 0x0465, // ë
    /* 0xEC */ 0x0019, // ì
    /* 0xED */ 0x0119, // í
    /* 0xEE */ 0x0219, // î
    /* 0xEF */ 0x0419, // ï
    /* 0xF0 */ 0xFF93, // ð
    /* 0xF1 */ 0x036E, // ñ
    /* 0xF2 */ 0x006F, // ò
    /* 0xF3 */ 0x016F, // ó
    /* 0xF4 */ 0x026F, // ô
    /* 0xF5 */ 0x036F, // õ
    /* 0xF6 */ 0x046F, // ö
    /* 0xF7 */ 0xFFAC, // ÷
    /* 0xF8 */ 0xFF95, // ø
    /* 0xF9 */ 0x0075, // ù
    /* 0xFA */ 0x0175, // ú
    /* 0xFB */ 0x0275, // û
    /* 0xFC */ 0x0475, // ü
    /* 0xFD */ 0x0179, // ý
    /* 0xFE */ 0xFF96, // þ
    /* 0xFF */ 0x0479  // ÿ
};

const constexpr uint16_t set2_translation_latin_A[] = {
    /* 0x100 */ 0x0941, // Ā
    /* 0x101 */ 0x0961, // ā
    /* 0x102 */ 0x0841, // Ă
    /* 0x103 */ 0x0861, // ă
    /* 0x104 */ 0x0C41, // Ą
    /* 0x105 */ 0x0C61, // ą
    /* 0x106 */ 0x0143, // Ć
    /* 0x107 */ 0x0163, // ć
    /* 0x108 */ 0x0243, // Ĉ
    /* 0x109 */ 0x0263, // ĉ
    /* 0x10A */ 0x0A43, // Ċ
    /* 0x10B */ 0x0A63, // ċ
    /* 0x10C */ 0x0743, // Č
    /* 0x10D */ 0x0763, // č
    /* 0x10E */ 0x0744, // Ď
    /* 0x10F */ 0xFF85, // ď

    /* 0x110 */ 0xFF8D, // Đ
    /* 0x111 */ 0xFF83, // đ
    /* 0x112 */ 0x0945, // Ē
    /* 0x113 */ 0x0965, // ē
    /* 0x114 */ 0x0845, // Ĕ
    /* 0x115 */ 0x0865, // ĕ
    /* 0x116 */ 0x0A45, // Ė
    /* 0x117 */ 0x0A65, // ė
    /* 0x118 */ 0x0C45, // Ę
    /* 0x119 */ 0x0C65, // ę
    /* 0x11A */ 0x0745, // Ě
    /* 0x11B */ 0x0765, // ě
    /* 0x11C */ 0x0247, // Ĝ
    /* 0x11D */ 0x0267, // ĝ
    /* 0x11E */ 0x0847, // Ğ
    /* 0x11F */ 0x0867, // ğ

    /* 0x120 */ 0x0A47, // Ġ
    /* 0x121 */ 0x0A67, // ġ
    /* 0x122 */ 0x0B47, // Ģ
    /* 0x123 */ 0xFF67, // ģ   ??
    /* 0x124 */ 0x0248, // Ĥ
    /* 0x125 */ 0x0268, // ĥ
    /* 0x126 */ 0xFF48, // Ħ
    /* 0x127 */ 0xFF68, // ħ
    /* 0x128 */ 0x0349, // Ĩ
    /* 0x129 */ 0x0319, // ĩ
    /* 0x12A */ 0x0949, // Ī
    /* 0x12B */ 0x0919, // ī
    /* 0x12C */ 0x0849, // Ĭ
    /* 0x12D */ 0x0819, // ĭ
    /* 0x12E */ 0x0C49, // Į
    /* 0x12F */ 0x0C69, // į

    /* 0x130 */ 0x0A49, // İ
    /* 0x131 */ 0xFF19, // ı
    /* 0x132 */ 0xFF82, // Ĳ
    /* 0x133 */ 0xFF8A, // ĳ
    /* 0x134 */ 0x024A, // Ĵ
    /* 0x135 */ 0x021A, // ĵ
    /* 0x136 */ 0x0B4B, // Ķ
    /* 0x137 */ 0x0B6B, // ķ
    /* 0x138 */ 0xFF6B, // ĸ   ??
    /* 0x139 */ 0x014C, // Ĺ
    /* 0x13A */ 0x016C, // ĺ
    /* 0x13B */ 0x0B4C, // Ļ
    /* 0x13C */ 0x0B6C, // ļ
    /* 0x13D */ 0xFF7F, // Ľ
    /* 0x13E */ 0xFF86, // ľ
    /* 0x13F */ 0xFF4C, // Ŀ   ??

    /* 0x140 */ 0xFF6C, // ŀ   ??
    /* 0x141 */ 0xFF80, // Ł
    /* 0x142 */ 0xFF87, // ł
    /* 0x143 */ 0x014E, // Ń
    /* 0x144 */ 0x016E, // ń
    /* 0x145 */ 0x0B4E, // Ņ
    /* 0x146 */ 0x0B6E, // ņ
    /* 0x147 */ 0x074E, // Ň
    /* 0x148 */ 0x076E, // ň
    /* 0x149 */ 0x276E, // ŉ
    /* 0x14A */ 0xFF81, // Ŋ
    /* 0x14B */ 0xFF88, // ŋ
    /* 0x14C */ 0x094F, // Ō
    /* 0x14D */ 0x096F, // ō
    /* 0x14E */ 0x084F, // Ŏ
    /* 0x14F */ 0x086F, // ŏ

    /* 0x150 */ 0x054F, // Ő
    /* 0x151 */ 0x056F, // ő
    /* 0x152 */ 0xFF8E, // Œ
    /* 0x153 */ 0xFF94, // œ
    /* 0x154 */ 0x0152, // Ŕ
    /* 0x155 */ 0x0172, // ŕ
    /* 0x156 */ 0x0B52, // Ŗ
    /* 0x157 */ 0x0B72, // ŗ
    /* 0x158 */ 0x0752, // Ř
    /* 0x159 */ 0x0772, // ř
    /* 0x15A */ 0x0153, // Ś
    /* 0x15B */ 0x0173, // ś
    /* 0x15C */ 0x0253, // Ŝ
    /* 0x15D */ 0x0273, // ŝ
    /* 0x15E */ 0x0B53, // Ş
    /* 0x15F */ 0x0B73, // ş

    /* 0x160 */ 0x0753, // Š
    /* 0x161 */ 0x0773, // š
    /* 0x162 */ 0x0B54, // Ţ
    /* 0x163 */ 0x0B74, // ţ
    /* 0x164 */ 0x0754, // Ť
    /* 0x165 */ 0xFF89, // ť
    /* 0x166 */ 0xFF54, // Ŧ  ??
    /* 0x167 */ 0xFF74, // ŧ  ??
    /* 0x168 */ 0x0355, // Ũ
    /* 0x169 */ 0x0375, // ũ
    /* 0x16A */ 0x0955, // Ū
    /* 0x16B */ 0x0975, // ū
    /* 0x16C */ 0x0855, // Ŭ
    /* 0x16D */ 0x0875, // ŭ
    /* 0x16E */ 0x0655, // Ů
    /* 0x16F */ 0x0675, // ů

    /* 0x170 */ 0x0555, // Ű
    /* 0x171 */ 0x0575, // ű
    /* 0x172 */ 0x0C55, // Ų
    /* 0x173 */ 0x0C75, // ų
    /* 0x174 */ 0x0257, // Ŵ
    /* 0x175 */ 0x0277, // ŵ
    /* 0x176 */ 0x0259, // Ŷ
    /* 0x177 */ 0x0279, // ŷ
    /* 0x178 */ 0x0459, // Ÿ
    /* 0x179 */ 0x015A, // Ź
    /* 0x17A */ 0x017A, // ź
    /* 0x17B */ 0x0A5A, // Ż
    /* 0x17C */ 0x0A7A, // ż
    /* 0x17D */ 0x075A, // Ž
    /* 0x17E */ 0x077A, // ž
    /* 0x17F */ 0xFFFF  // ſ   ???
};
#endif

} // namespace IBMFDefs
