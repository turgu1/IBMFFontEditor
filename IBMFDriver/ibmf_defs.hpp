#pragma once

#include <QChar>
#include <cinttypes>
#include <vector>

// clang-format off
//
// The following definitions are used by all parts of the driver.
//
// The IBMF font files have the current format:
//
//  At Offset 0:
//  +--------------------+
//  |                    |  Preamble
//  |                    |
//  +--------------------+
//  |                    |  FaceHeader offset vector One 32 bit offset for each face
//  |                    |
//  +--------------------+
//  |                    |  Pixel sizes (one byte per face pt size present padded to be even)
//  |                    |  (not used by this driver)
//  +--------------------+
//
//  +--------------------+                       <------------+
//  |                    |  FaceHeader                        |
//  |                    |                                    |
//  |                    |                                    |
//  +--------------------+                <--+                |
//  |                    |  GlyphInfo        |                |
//  |                    |                   |                |
//  |                    |                   |   Repeat for   |
//  +--------------------+                   |>  each glyph   |
//  |                    |  Glyph Pixels     |   part of the  |
//  |                    |                   |   face         |  Repeat for
//  |                    |                   |                |> each face
//  +--------------------+                <--+                |  part of the
//             .                                              |  font
//             .                                              |
//             .                                              |
//  +--------------------+                                    |
//  |                    | LigKerSteps                        |
//  |                    |                                    |
//  |                    |                                    |
//  +--------------------+                                    |
//  |                    | Kerns                              |
//  |                    |                                    |
//  +--------------------+                       <------------+
//             .
//             .
//             .
//
// The new upcoming format (Version 5) is the following:
//
// 
//  +--------------------+
//  |                    |  Preamble
//  |                    |
//  +--------------------+
//  |   Unicode Table    |  The table of all  unicode glyphs part of this font
//  |                    |
//  |                    |
//  |                    |
//  +--------------------+
//  |     FaceHeaders    |  All face headers are combined in a single table
//  |                    |
//  |                    |
//  |                    |
//  +--------------------+
//  |   Glyphs metrics   | All glyphs metrics of all fonts
//  |                    |
//  |                    |
//  |                    |
//  |                    |
//  |                    |
//  +--------------------+
//
// clang-format on

namespace IBMFDefs {

#ifdef DEBUG_IBMF
const constexpr int DEBUG = DEBUG_IBMF;
#else
const constexpr int DEBUG = 0;
#endif

const constexpr uint8_t IBMF_VERSION    = 4;
const constexpr uint8_t MAX_GLYPH_COUNT = 254; // Index Value 0xFE and 0xFF are reserved

// The followings have to be adjusted depending on the screen
// software/hardware/firmware' pixels polarity/color/shading/gray-scale
// At least, one of BLACK... or WHITE... must be 0. If not, some changes are
// required in the code.

// const constexpr uint8_t BLACK_ONE_BIT = 0;
// const constexpr uint8_t WHITE_ONE_BIT = 1;

// const constexpr uint8_t BLACK_EIGHT_BITS = 0;
// const constexpr uint8_t WHITE_EIGHT_BITS = 0xFF;

const constexpr uint8_t BLACK_ONE_BIT = 1;
const constexpr uint8_t WHITE_ONE_BIT = 0;

const constexpr uint8_t BLACK_EIGHT_BITS = 0xFF;
const constexpr uint8_t WHITE_EIGHT_BITS = 0x00;

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

typedef uint8_t             *MemoryPtr;
typedef std::vector<uint8_t> Pixels;
typedef Pixels              *PixelsPtr;

struct RLEBitmap {
  Pixels   pixels;
  Dim      dim;
  uint16_t length;
  void     clear() {
        pixels.clear();
        dim    = Dim(0, 0);
        length = 0;
  }
};
typedef RLEBitmap *RLEBitmapPtr;

struct Bitmap {
  Pixels pixels;
  Dim    dim;
  void   clear() {
      pixels.clear();
      dim = Dim(0, 0);
  }
};
typedef Bitmap *BitmapPtr;

#pragma pack(push, 1)

typedef int16_t FIX16;
typedef int16_t FIX14;
typedef uint8_t FontFormat;

struct Preamble {
  char    marker[4];
  uint8_t faceCount;
  struct {
    uint8_t    version    : 5;
    FontFormat fontFormat : 3;
  } bits;
};
typedef Preamble *PreamblePtr;

struct FaceHeader {
  uint8_t  pointSize;
  uint8_t  lineHeight;
  uint16_t dpi;
  FIX16    xHeight;
  FIX16    emSize;
  FIX16    slantCorrection;
  uint8_t  descenderHeight;
  uint8_t  spaceSize;
  uint16_t glyphCount;
  uint16_t ligKernStepCount;
  uint16_t firstCode;
  uint16_t lastCode;
  uint8_t  kernCount;
  uint8_t  maxHeight;
};
// typedef FaceHeader *FaceHeaderPtr;
typedef std::shared_ptr<FaceHeader> FaceHeaderPtr;

// clang-format off
//
// The lig kern array contains instructions (struct LibKernStep) in a simple programming
// language that explains what to do for special letter pairs. The information in squared
// brackets relate to fields that are part of the LibKernStep struct. Each entry in this
// array is a lig kern command of four bytes:
//
// - first byte: skip byte, indicates that this is the final program step if the byte
//                          is 128 or more, otherwise the next step is obtained by
//                          skipping this number of intervening steps [nextStepRelative].
// - second byte: next char, if next character follows the current character, then
//                           perform the operation and stop, otherwise continue.
// - third byte: op byte, indicates a ligature step if less than 128, a kern step otherwise.
// - fourth byte: remainder.
//
// In a kern step [isAKern == true], an additional space equal to kern located at
// [(displHigh << 8) + displLow] in the kern array is inserted between the current
// character and [nextChar]. This amount is often negative, so that the characters
// are brought closer together by kerning; but it might be positive.
//
// There are eight kinds of ligature steps [isAKern == false], having op byte codes
// [a_op b_op c_op] where 0 ≤ a_op ≤ b_op + c_op and 0 ≤ b_op, c_op ≤ 1.
//
// The character whose code is [replacementChar] is inserted between the current
// character and [nextChar]; then the current character is deleted if b_op = 0, and
// [nextChar] is deleted if c_op = 0; then we pass over a_op characters to reach the next
// current character (which may have a ligature/kerning program of its own).
//
// If the very first instruction of a character’s lig kern program has [whole > 128],
// the program actually begins in location [(displHigh << 8) + displLow]. This feature
// allows access to large lig kern arrays, because the first instruction must otherwise
// appear in a location ≤ 255.
//
// Any instruction with [whole > 128] in the lig kern array must have
// [(displHigh << 8) + displLow] < the size of the array. If such an instruction is
// encountered during normal program execution, it denotes an unconditional halt; no
// ligature or kerning command is performed.
//
// (The following usage has been extracted from the lig/kern array as not being used outside
//  of a TeX generated document)
//
// If the very first instruction of the lig kern array has [whole == 0xFF], the
// [nextChar] byte is the so-called right boundary character of this font; the value
// of [nextChar] need not lie between char codes boundaries.
//
// If the very last instruction of the lig kern array has [whole == 0xFF], there is
// a special ligature/kerning program for a left boundary character, beginning at location
// [(displHigh << 8) + displLow] . The interpretation is that TEX puts implicit boundary
// characters before and after each consecutive string of characters from the same font.
// These implicit characters do not appear in the output, but they can affect ligatures
// and kerning.
//
// -----
//
// Here is the original LigKern table entry format (4 bytes). Byte 1, 3 and 4 have
// two different meanings as show below (a big-endian format...):
//
//           Byte 1                   Byte 2
// +------------------------+------------------------+
// |        whole           |                        |
// +------------------------+       Next Char        +
// |Stop| nextStepRelative  |                        |
// +------------------------+------------------------+
//
//
//           Byte 3                   Byte 4
// +------------------------+------------------------+
// |Kern|       | a | b | c |    Replacement Char    |  <- isAKern (Kern in the diagram) is false
// +------------------------+------------------------+
// |isKern|Displacement High|    Displacement Low    |  <- isAKern is true
// +------------------------+------------------------+
//
// The following fields are not used/replaced in this application:
//
//    - nextStepRelative
//    - Ops a, b, and c
//    - whole can be reduced to one GoTo bit
//
// ----
//
// Here is the optimized version considering larger characters table
// and that some fields are not being used (BEWARE: a little-endian format):
//
//           Byte 2                   Byte 1
// +------------------------+------------------------+
// |Stop|               Next Char                    |
// +------------------------+------------------------+
//
//
//           Byte 4                   Byte 3
// +------------------------+------------------------+
// |Kern|             Replacement Char               |  <- isAKern (Kern in the diagram) is false
// +------------------------+------------------------+
// |Kern|GoTo|      Displacement in FIX14            |  <- isAKern is true and GoTo is false =>
// Kerning value
// +------------------------+------------------------+
// |Kern|GoTo|          Displacement                 |  <- isAkern and GoTo are true
// +------------------------+------------------------+
//
// Up to 32765 different glyph codes can be managed through this format.
// Kerning displacements reduced to 14 bits is not a big issue: kernings are
// usually small numbers. FIX14 and FIX16 are using 6 bits for the fraction. Their
// remains 8 bits for FIX14 and 10 bits for FIX16, that is more than enough...
//
// clang-format on

#define ORIGINAL_FORMAT 1
#if ORIGINAL_FORMAT
union SkipByte {
  uint8_t whole : 8;
  struct {
    uint8_t nextStepRelative : 7;
    bool    stop             : 1;
  } s;
};

union OpCodeByte {
  struct {
    bool    c_op    : 1;
    bool    b_op    : 1;
    uint8_t a_op    : 5;
    bool    isAKern : 1;
  } op;
  struct {
    uint8_t displHigh : 7;
    bool    isAKern   : 1;
  } d;
};

union RemainderByte {
  uint8_t replacementChar : 8;
  uint8_t displLow        : 8; // Ligature: replacement char code, kern: displacement
};

struct LigKernStep {
  SkipByte      skip;
  uint8_t       nextChar;
  OpCodeByte    opCode;
  RemainderByte remainder;
};
#else
struct Nxt {
  GlyphCode nextGlyphCode : 15;
  bool      stop          : 1;
};
union ReplDisp {
  struct {
    GlyphCode replGlyphCode : 15;
    bool      isAKern       : 1;
  } repl;
  struct {
    FIX14 kerningValue : 14;
    bool  isAGoTo      : 1;
    bool  isAKern      : 1;
  } kern;
  struct {
    uint16_t displacement : 14;
    bool     isAGoTo      : 1;
    bool     isAKern      : 1;
  } goTo;
};

struct LigKernStep {
  Nxt      a;
  ReplDisp b;
};
#endif

typedef LigKernStep *LigKernStepPtr;

struct RLEMetrics {
  uint8_t dynF         : 4;
  uint8_t firstIsBlack : 1;
  uint8_t filler       : 3;
};

struct GlyphInfo {
  uint8_t    charCode;
  uint8_t    bitmapWidth;
  uint8_t    bitmapHeight;
  int8_t     horizontalOffset;
  int8_t     verticalOffset;
  uint8_t    ligKernPgmIndex; // = 255 if none
  uint16_t   packetLength;
  FIX16      advance;
  RLEMetrics rleMetrics;
};

typedef std::shared_ptr<GlyphInfo> GlyphInfoPtr;

#pragma pack(pop)

struct GlyphMetrics {
  int16_t xoff, yoff;
  int16_t advance;
  int16_t lineHeight;
  int16_t ligatureAndKernPgmIndex;
  void    clear() {
       xoff = yoff = 0;
       advance = lineHeight    = 0;
       ligatureAndKernPgmIndex = 255;
  }
};

struct Glyph {
  GlyphMetrics metrics;
  Bitmap       bitmap;
  uint8_t      pointSize;
  void         clear() {
            metrics.clear();
            bitmap.clear();
            pointSize = 0;
  }
};

// These are the corresponding Unicode value for each of the 174 characters that are part of
// an IBMF Font;
const QChar fontFormat0CharacterCodes[] = {
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
const constexpr uint16_t latinTranslationSet[] = {
    /* 0x0A1 */ 0x0020, // ¡
    /* 0x0A2 */ 0x0098, // ¢
    /* 0x0A3 */ 0x008B, // £
    /* 0x0A4 */ 0x0099, // ¤
    /* 0x0A5 */ 0x009A, // ¥
    /* 0x0A6 */ 0x009B, // ¦
    /* 0x0A7 */ 0x0084, // §
    /* 0x0A8 */ 0x0004, // ¨
    /* 0x0A9 */ 0x009C, // ©
    /* 0x0AA */ 0x009D, // ª
    /* 0x0AB */ 0x0013, // «
    /* 0x0AC */ 0x009E, // ¬
    /* 0x0AD */ 0x002D, // Soft hyphen
    /* 0x0AE */ 0x009F, // ®
    /* 0x0AF */ 0x0009, // Macron
    /* 0x0B0 */ 0x0006, // ° Degree
    /* 0x0B1 */ 0x00A0, // ±
    /* 0x0B2 */ 0x00A1, // ²
    /* 0x0B3 */ 0x00A2, // ³
    /* 0x0B4 */ 0x0001, // accute accent
    /* 0x0B5 */ 0x00A3, // µ
    /* 0x0B6 */ 0x00A4, // ¶
    /* 0x0B7 */ 0x00A5, // middle dot
    /* 0x0B8 */ 0x000B, // cedilla
    /* 0x0B9 */ 0x00A6, // ¹
    /* 0x0BA */ 0x00A7, // º
    /* 0x0BB */ 0x0014, // »
    /* 0x0BC */ 0x00A9, // ¼
    /* 0x0BD */ 0x00AA, // ½
    /* 0x0BE */ 0x00AB, // ¾
    /* 0x0BF */ 0x0017, // ¿
    /* 0x0C0 */ 0xF041, // À
    /* 0x0C1 */ 0x1041, // Á
    /* 0x0C2 */ 0x2041, // Â
    /* 0x0C3 */ 0x3041, // Ã
    /* 0x0C4 */ 0x4041, // Ä
    /* 0x0C5 */ 0x6041, // Å
    /* 0x0C6 */ 0x008C, // Æ
    /* 0x0C7 */ 0xB043, // Ç
    /* 0x0C8 */ 0xF045, // È
    /* 0x0C9 */ 0x1045, // É
    /* 0x0CA */ 0x2045, // Ê
    /* 0x0CB */ 0x4045, // Ë
    /* 0x0CC */ 0xF049, // Ì
    /* 0x0CD */ 0x1049, // Í
    /* 0x0CE */ 0x2049, // Î
    /* 0x0CF */ 0x4049, // Ï
    /* 0x0D0 */ 0x008D, // Ð
    /* 0x0D1 */ 0x304E, // Ñ
    /* 0x0D2 */ 0xF04F, // Ò
    /* 0x0D3 */ 0x104F, // Ó
    /* 0x0D4 */ 0x204F, // Ô
    /* 0x0D5 */ 0x304F, // Õ
    /* 0x0D6 */ 0x404F, // Ö
    /* 0x0D7 */ 0x00A8, // ×
    /* 0x0D8 */ 0x008F, // Ø
    /* 0x0D9 */ 0xF055, // Ù
    /* 0x0DA */ 0x1055, // Ú
    /* 0x0DB */ 0x2055, // Û
    /* 0x0DC */ 0x4055, // Ü
    /* 0x0DD */ 0x1059, // Ý
    /* 0x0DE */ 0x0090, // Þ
    /* 0x0DF */ 0x0097, // ß
    /* 0x0E0 */ 0xF061, // à
    /* 0x0E1 */ 0x1061, // á
    /* 0x0E2 */ 0x2061, // â
    /* 0x0E3 */ 0x3061, // ã
    /* 0x0E4 */ 0x4061, // ä
    /* 0x0E5 */ 0x6061, // å
    /* 0x0E6 */ 0x0092, // æ
    /* 0x0E7 */ 0xB063, // ç
    /* 0x0E8 */ 0xF065, // è
    /* 0x0E9 */ 0x1065, // é
    /* 0x0EA */ 0x2065, // ê
    /* 0x0EB */ 0x4065, // ë
    /* 0x0EC */ 0xF019, // ì
    /* 0x0ED */ 0x1019, // í
    /* 0x0EE */ 0x2019, // î
    /* 0x0EF */ 0x4019, // ï
    /* 0x0F0 */ 0x0093, // ð
    /* 0x0F1 */ 0x306E, // ñ
    /* 0x0F2 */ 0xF06F, // ò
    /* 0x0F3 */ 0x106F, // ó
    /* 0x0F4 */ 0x206F, // ô
    /* 0x0F5 */ 0x306F, // õ
    /* 0x0F6 */ 0x406F, // ö
    /* 0x0F7 */ 0x00AC, // ÷
    /* 0x0F8 */ 0x0095, // ø
    /* 0x0F9 */ 0xF075, // ù
    /* 0x0FA */ 0x1075, // ú
    /* 0x0FB */ 0x2075, // û
    /* 0x0FC */ 0x4075, // ü
    /* 0x0FD */ 0x1079, // ý
    /* 0x0FE */ 0x0096, // þ
    /* 0x0FF */ 0x4079, // ÿ

    /* 0x100 */ 0x9041, // Ā
    /* 0x101 */ 0x9061, // ā
    /* 0x102 */ 0x8041, // Ă
    /* 0x103 */ 0x8061, // ă
    /* 0x104 */ 0xC041, // Ą
    /* 0x105 */ 0xC061, // ą
    /* 0x106 */ 0x1043, // Ć
    /* 0x107 */ 0x1063, // ć
    /* 0x108 */ 0x2043, // Ĉ
    /* 0x109 */ 0x2063, // ĉ
    /* 0x10A */ 0xA043, // Ċ
    /* 0x10B */ 0xA063, // ċ
    /* 0x10C */ 0x7043, // Č
    /* 0x10D */ 0x7063, // č
    /* 0x10E */ 0x7044, // Ď
    /* 0x10F */ 0x0085, // ď

    /* 0x110 */ 0x008D, // Đ
    /* 0x111 */ 0x0083, // đ
    /* 0x112 */ 0x9045, // Ē
    /* 0x113 */ 0x9065, // ē
    /* 0x114 */ 0x8045, // Ĕ
    /* 0x115 */ 0x8065, // ĕ
    /* 0x116 */ 0xA045, // Ė
    /* 0x117 */ 0xA065, // ė
    /* 0x118 */ 0xC045, // Ę
    /* 0x119 */ 0xC065, // ę
    /* 0x11A */ 0x7045, // Ě
    /* 0x11B */ 0x7065, // ě
    /* 0x11C */ 0x2047, // Ĝ
    /* 0x11D */ 0x2067, // ĝ
    /* 0x11E */ 0x8047, // Ğ
    /* 0x11F */ 0x8067, // ğ

    /* 0x120 */ 0xA047, // Ġ
    /* 0x121 */ 0xA067, // ġ
    /* 0x122 */ 0xB047, // Ģ
    /* 0x123 */ 0x0067, // ģ   ??
    /* 0x124 */ 0x2048, // Ĥ
    /* 0x125 */ 0x2068, // ĥ
    /* 0x126 */ 0x0048, // Ħ
    /* 0x127 */ 0x0068, // ħ
    /* 0x128 */ 0x3049, // Ĩ
    /* 0x129 */ 0x3019, // ĩ
    /* 0x12A */ 0x9049, // Ī
    /* 0x12B */ 0x9019, // ī
    /* 0x12C */ 0x8049, // Ĭ
    /* 0x12D */ 0x8019, // ĭ
    /* 0x12E */ 0xC049, // Į
    /* 0x12F */ 0xC069, // į

    /* 0x130 */ 0xA049, // İ
    /* 0x131 */ 0x0019, // ı
    /* 0x132 */ 0x0082, // Ĳ
    /* 0x133 */ 0x008A, // ĳ
    /* 0x134 */ 0x204A, // Ĵ
    /* 0x135 */ 0x201A, // ĵ
    /* 0x136 */ 0xB04B, // Ķ
    /* 0x137 */ 0xB06B, // ķ
    /* 0x138 */ 0x006B, // ĸ   ??
    /* 0x139 */ 0x104C, // Ĺ
    /* 0x13A */ 0x106C, // ĺ
    /* 0x13B */ 0xB04C, // Ļ
    /* 0x13C */ 0xB06C, // ļ
    /* 0x13D */ 0x007F, // Ľ
    /* 0x13E */ 0x0086, // ľ
    /* 0x13F */ 0x004C, // Ŀ   ??

    /* 0x140 */ 0x006C, // ŀ   ??
    /* 0x141 */ 0x0080, // Ł
    /* 0x142 */ 0x0087, // ł
    /* 0x143 */ 0x104E, // Ń
    /* 0x144 */ 0x106E, // ń
    /* 0x145 */ 0xB04E, // Ņ
    /* 0x146 */ 0xB06E, // ņ
    /* 0x147 */ 0x704E, // Ň
    /* 0x148 */ 0x706E, // ň
    /* 0x149 */ 0xE06E, // ŉ
    /* 0x14A */ 0x0081, // Ŋ
    /* 0x14B */ 0x0088, // ŋ
    /* 0x14C */ 0x904F, // Ō
    /* 0x14D */ 0x906F, // ō
    /* 0x14E */ 0x804F, // Ŏ
    /* 0x14F */ 0x806F, // ŏ

    /* 0x150 */ 0x504F, // Ő
    /* 0x151 */ 0x506F, // ő
    /* 0x152 */ 0x008E, // Œ
    /* 0x153 */ 0x0094, // œ
    /* 0x154 */ 0x1052, // Ŕ
    /* 0x155 */ 0x1072, // ŕ
    /* 0x156 */ 0xB052, // Ŗ
    /* 0x157 */ 0xB072, // ŗ
    /* 0x158 */ 0x7052, // Ř
    /* 0x159 */ 0x7072, // ř
    /* 0x15A */ 0x1053, // Ś
    /* 0x15B */ 0x1073, // ś
    /* 0x15C */ 0x2053, // Ŝ
    /* 0x15D */ 0x2073, // ŝ
    /* 0x15E */ 0xB053, // Ş
    /* 0x15F */ 0xB073, // ş

    /* 0x160 */ 0x7053, // Š
    /* 0x161 */ 0x7073, // š
    /* 0x162 */ 0xB054, // Ţ
    /* 0x163 */ 0xB074, // ţ
    /* 0x164 */ 0x7054, // Ť
    /* 0x165 */ 0x0089, // ť
    /* 0x166 */ 0x0054, // Ŧ  ??
    /* 0x167 */ 0x0074, // ŧ  ??
    /* 0x168 */ 0x3055, // Ũ
    /* 0x169 */ 0x3075, // ũ
    /* 0x16A */ 0x9055, // Ū
    /* 0x16B */ 0x9075, // ū
    /* 0x16C */ 0x8055, // Ŭ
    /* 0x16D */ 0x8075, // ŭ
    /* 0x16E */ 0x6055, // Ů
    /* 0x16F */ 0x6075, // ů

    /* 0x170 */ 0x5055, // Ű
    /* 0x171 */ 0x5075, // ű
    /* 0x172 */ 0xC055, // Ų
    /* 0x173 */ 0xC075, // ų
    /* 0x174 */ 0x2057, // Ŵ
    /* 0x175 */ 0x2077, // ŵ
    /* 0x176 */ 0x2059, // Ŷ
    /* 0x177 */ 0x2079, // ŷ
    /* 0x178 */ 0x4059, // Ÿ
    /* 0x179 */ 0x105A, // Ź
    /* 0x17A */ 0x107A, // ź
    /* 0x17B */ 0xA05A, // Ż
    /* 0x17C */ 0xA07A, // ż
    /* 0x17D */ 0x705A, // Ž
    /* 0x17E */ 0x707A, // ž
    /* 0x17F */ 0x00FF  // ſ   ???
};
#endif

} // namespace IBMFDefs
