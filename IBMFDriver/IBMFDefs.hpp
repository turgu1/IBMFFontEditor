#pragma once

#include <cinttypes>
#include <memory>
#include <vector>

#include "../Unicode/UBlocks.hpp"

// clang-format off
//
// The following definitions are used by all parts of the driver.
//
// The IBMF font files have the current format:
//
// 1. LATIN and UTF32 formats:
//
//  At Offset 0:
//  +--------------------+
//  |                    |  Preamble (6 bytes)
//  |                    |
//  +--------------------+
//  |                    |  Pixel sizes (one byte per face pt size present padded to 32 bits
//  |                    |  from the start) (not used by this driver)
//  +--------------------+
//  |                    |  FaceHeader offset vector 
//  |                    |  (32 bit offset for each face)
//  +--------------------+
//  |                    |  For FontFormat 1 (FontFormat::UTF32 only): the table that contains corresponding
//  |                    |  values between Unicode CodePoints and their internal GlyphCode.
//  |                    |  (content already well aligned to 32 bits frontiers)
//  +--------------------+
//
//  +--------------------+               <------------+
//  |                    |  FaceHeader                |
//  |                    |  (32 bits aligned)         |
//  |                    |                            |
//  +--------------------+                            |
//  |                    |  Glyphs' pixels indexes    |
//  |                    |  in the Pixels Pool        |
//  |                    |  (32bits each)             |
//  +--------------------+                            |
//  |                    |  GlyphsInfo                |
//  |                    |  Array (16 bits aligned)   |
//  |                    |                            |  Repeat for
//  +--------------------+                            |> each face
//  |                    |                            |  part of the
//  |                    |  Pixels Pool               |  font
//  |                    |  (No alignement, all bytes)|
//  |                    |                            |
//  +--------------------+                            |
//  |                    |  Filler (32bits padding)   | <- Takes into account both
//  +--------------------+                            |    GlyphsInfo and PixelsPool
//  |                    |                            |
//  |                    |  LigKerSteps               |
//  |                    |  (2 x 16 bits each step)   |
//  |                    |                            |
//  +--------------------+               <------------+
//             .
//             .
//             .
//
// 2. BACKUP Format:
//
// The BACKUP format is used to keep a copy of glyphs that have been modified by hand.
// This is to allow for the retrieval of changes made when there is a need to
// re-import a font.
//
// Only one backup per glyph is kept in the backup *font*.
//
// It is used by the application as another instance of an IBMF font.
//
// The application is responsible of opening and saving the backup file and to call the
// saveGlyph() method when it is required to save a glyph. The importBackup
// method can be used to retrieve glyphs present in the backup file to incorporate
// them in the current opened ibmf font. Care must be taken by the application to insure
// synchronisation of both opened file to be of the same imported font.
//
// When importing the backup glyphs to the current ibmf font, only the glyphs present in the
// ibmf font will be updated.
//
// Basically:
//
// - The number of faces may be different from the ibmf font as it contains only the
//   faces for which a glyph has been modified
// - In the FaceHeader, the glyphCount can be different for each face present as it
//   reflects the number of modified glyphs present in this backup
// - The GlyphInfo is replaced with BackupGlyphInfo that contains the codePoint
//   associated with the glyph.
// - The information kept for a glyph that was modified are: The pixels bitmap, the
//   glyphInfo metrics and the lig/kern table.
//
// - The createBackup() class method must be called to generate a new backup file if none
//   is available. A new instance of the backup *font* will then be generated without any
//   face in it.
// - The saveGlyph() method is called to save a glyph information to the backup instance.
// - The save() method is called to save the backup *font* to disk.
// - The load() method is called to load a backup  *font* from disk.
//
//  At Offset 0:
//  +--------------------+
//  |                    |  Preamble (6 bytes)
//  |                    |
//  +--------------------+
//  |                    |  Pixel sizes (one byte per face pt size present padded to 32 bits
//  |                    |  from the start) (not used by this driver)
//  +--------------------+
//  |                    |  FaceHeader offset vector
//  |                    |  (32 bit offset for each face)
//  +--------------------+
//
//  +--------------------+               <------------+
//  |                    |  FaceHeader                |
//  |                    |  (32 bits aligned)         |
//  |                    |                            |
//  +--------------------+                            |
//  |                    |  Glyphs' pixels indexes    |
//  |                    |  in the Pixels Pool        |
//  |                    |  (32bits each)             |
//  +--------------------+                            |
//  |                    |  GlyphsBackupInfo          |
//  |                    |  Array (16 bits aligned)   |
//  |                    |                            |  Repeat for
//  +--------------------+                            |> each face
//  |                    |                            |  part of the
//  |                    |  Pixels Pool               |  font
//  |                    |  (No alignement, all bytes)|
//  |                    |                            |
//  +--------------------+                            |
//  |                    |  Filler (32bits padding)   | <- Takes into account both
//  +--------------------+                            |    GlyphsInfo and PixelsPool
//  |                    |                            |
//  |                    |  LigKerSteps               |
//  |                    |  (2 x 16 bits each step)   |
//  |                    |                            |
//  +--------------------+               <------------+
//             .
//             .
//             .
//
//
// clang-format on

namespace IBMFDefs {

#ifdef DEBUG_IBMF
const constexpr int DEBUG = DEBUG_IBMF;
#else
const constexpr int DEBUG = 0;
#endif

const constexpr uint8_t IBMF_VERSION = 4;

// The followings have to be adjusted depending on the screen
// software/hardware/firmware' pixels polarity/color/shading/gray-scale
// At least, one of BLACK... or WHITE... must be 0. If not, some changes are
// required in the code.

// const constexpr uint8_t BLACK_ONE_BIT = 0;
// const constexpr uint8_t WHITE_ONE_BIT = 1;

// const constexpr uint8_t BLACK_EIGHT_BITS = 0;
// const constexpr uint8_t WHITE_EIGHT_BITS = 0xFF;

const constexpr uint8_t BLACK_ONE_BIT    = 1;
const constexpr uint8_t WHITE_ONE_BIT    = 0;

const constexpr uint8_t BLACK_EIGHT_BITS = 0xFF;
const constexpr uint8_t WHITE_EIGHT_BITS = 0x00;

enum FontFormat : uint8_t { LATIN = 0, UTF32 = 1, BACKUP = 7 };
enum class PixelResolution : uint8_t { ONE_BIT, EIGHT_BITS };

const constexpr PixelResolution resolution = PixelResolution::EIGHT_BITS;

struct Dim {
  uint8_t width;
  uint8_t height;
  Dim(uint8_t w, uint8_t h) : width(w), height(h) {}
  Dim() : width(0), height(0) {}
  bool operator==(const Dim &other) const {
    return (width == other.width) && (height == other.height);
  }
};

struct Pos {
  int8_t x;
  int8_t y;
  Pos(int8_t xpos, int8_t ypos) : x(xpos), y(ypos) {}
  Pos() : x(0), y(0) {}
  bool operator==(const Pos &other) const { return (x == other.x) && (y == other.y); }
};

typedef uint8_t              *MemoryPtr;
typedef std::vector<uint8_t>  Pixels;
typedef Pixels               *PixelsPtr;
typedef uint16_t              GlyphCode;
typedef std::vector<char32_t> CharCodes;

const constexpr GlyphCode NO_GLYPH_CODE = 0x7FFF;
const constexpr GlyphCode SPACE_CODE    = 0x7FFE;

// RLE (Run Length Encoded) Bitmap. To get something to show, they have
// to be processed through the RLEExtractor class.
// Dim contains the expected width and height once the bitmap has been
// decompressed. The length is the pixels array size in bytes.

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
typedef std::shared_ptr<RLEBitmap> RLEBitmapPtr;

// Uncompressed Bitmap.

struct Bitmap {
  Pixels pixels;
  Dim    dim;
  Bitmap() { clear(); }
  void clear() {
    pixels.clear();
    dim = Dim(0, 0);
  }
  bool operator==(const Bitmap &other) const {
    if ((pixels.size() != other.pixels.size()) || !(dim == other.dim)) return false;
    for (int idx = 0; idx < pixels.size(); idx++) {
      if (pixels.at(idx) != other.pixels.at(idx)) return false;
    }
    return true;
  }
};
typedef std::shared_ptr<Bitmap> BitmapPtr;

#pragma pack(push, 1)

typedef int16_t FIX16;
typedef int16_t FIX14;

struct Preamble {
  char    marker[4];
  uint8_t faceCount;
  struct {
    uint8_t    version    : 5;
    FontFormat fontFormat : 3;
  } bits;
};
typedef std::shared_ptr<Preamble> PreamblePtr;

struct FaceHeader {
  uint8_t  pointSize;        // In points (pt) a point is 1 / 72.27 of an inch
  uint8_t  lineHeight;       // In pixels
  uint16_t dpi;              // Pixels per inch
  FIX16    xHeight;          // Hight of character 'x' in pixels
  FIX16    emSize;           // Hight of character 'M' in pixels
  FIX16    slantCorrection;  // When an italic face
  uint8_t  descenderHeight;  // The height of the descending below the origin
  uint8_t  spaceSize;        // Size of a space character in pixels
  uint16_t glyphCount;       // Must be the same for all face (Except for BACKUP format)
  uint16_t ligKernStepCount; // Length of the Ligature/Kerning table
  uint32_t pixelsPoolSize;   // Size of the Pixels Pool
};

// typedef FaceHeader *FaceHeaderPtr;
typedef std::shared_ptr<FaceHeader> FaceHeaderPtr;
typedef uint8_t (*PixelsPoolTempPtr)[]; // Temporary pointer
typedef uint32_t PixelPoolIndex;
typedef PixelPoolIndex (*GlyphsPixelPoolIndexesTempPtr)[]; // One for each glyph

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
// [aOp bOp cOp] where 0 ≤ aOp ≤ bOp + cOp and 0 ≤ bOp, cOp ≤ 1.
//
// The character whose code is [replacementChar] is inserted between the current
// character and [nextChar]; then the current character is deleted if bOp = 0, and
// [nextChar] is deleted if cOp = 0; then we pass over aOp characters to reach the next
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
// |Kern|GoTo|      Displacement in FIX14            |  <- isAKern is true and GoTo is false => Kerning value
// +------------------------+------------------------+
// |Kern|GoTo|          Displacement                 |  <- isAkern and GoTo are true
// +------------------------+------------------------+
//
// Up to 32765 different glyph codes can be managed through this format.
// Kerning displacements reduced to 14 bits is not a big issue: kernings are
// usually small numbers. FIX14 and FIX16 are using 6 bits for the fraction. Their
// remains 8 bits for FIX14 and 10 bits for FIX16, that is more than enough...
//
// This is NOW the format in use with this driver and other support apps.
//
// clang-format on

union Nxt {
  struct {
    GlyphCode nextGlyphCode : 15;
    bool      stop          : 1;
  } data;
  struct {
    uint16_t val;
  } whole;
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
  struct {
    uint16_t val;
  } whole;
};

struct LigKernStep {
  Nxt      a;
  ReplDisp b;
};

struct RLEMetrics {
  uint8_t dynF         : 4;
  uint8_t firstIsBlack : 1;
  uint8_t filler       : 3;
  //
  bool operator==(const RLEMetrics &other) const {
    return (dynF == other.dynF) && (firstIsBlack == other.firstIsBlack);
  }
};

struct GlyphInfo {
  uint8_t    bitmapWidth;      // Width of bitmap once decompressed
  uint8_t    bitmapHeight;     // Height of bitmap once decompressed
  int8_t     horizontalOffset; // Horizontal offset from the orign
  int8_t     verticalOffset;   // Vertical offset from the origin
  uint16_t   packetLength;     // Length of the compressed bitmap
  FIX16      advance;          // Normal advance to the next glyph position in line
  RLEMetrics rleMetrics;       // RLE Compression information
  uint8_t    ligKernPgmIndex;  // = 255 if none, Index in the ligature/kern array
  GlyphCode  mainCode;         // Main composite (or not) glyphCode for kerning matching algo
  //
  bool operator==(const GlyphInfo &other) const {
    return (bitmapWidth == other.bitmapWidth) && (bitmapHeight == other.bitmapHeight) &&
           (horizontalOffset == other.horizontalOffset) &&
           (verticalOffset == other.verticalOffset) && (packetLength == other.packetLength) &&
           (advance == other.advance) && (rleMetrics == other.rleMetrics) &&
           (mainCode == other.mainCode);
  }
};

typedef std::shared_ptr<GlyphInfo> GlyphInfoPtr;

struct BackupGlyphInfo {
  uint8_t    bitmapWidth;      // Width of bitmap once decompressed
  uint8_t    bitmapHeight;     // Height of bitmap once decompressed
  int8_t     horizontalOffset; // Horizontal offset from the orign
  int8_t     verticalOffset;   // Vertical offset from the origin
  uint16_t   packetLength;     // Length of the compressed bitmap
  FIX16      advance;          // Normal advance to the next glyph position in line
  RLEMetrics rleMetrics;       // RLE Compression information
  int16_t    ligCount;
  int16_t    kernCount;
  char32_t   mainCodePoint; // Main composite (or not) codePoint for kerning matching algo
  char32_t   codePoint;     // CodePoint associated with the glyph (for BACKUP Format only)
};

typedef std::shared_ptr<BackupGlyphInfo> BackupGlyphInfoPtr;

// clang-format off
// 
// For FontFormat 1 (FontFormat::UTF32), there is a table that contains
// corresponding values between Unicode CodePoints and their internal GlyphCode.
// The GlyphCode is the index in the glyph table for the character.
//
// This table is in two parts:
//
// - Unicode plane information for the 4 planes supported
//   by the driver,
// - The list of bundle of code points that are part of each plane.
//   A bundle identifies the first codePoint and the number of consecutive codepoints
//   that are part of the bundle.
//
// For more information about the planes, please consult the followint Wikipedia page:
//
//     https://en.wikipedia.org/wiki/Plane_(Unicode)
//
// clang-format on

struct Plane {
  uint16_t  codePointBundlesIdx; // Index of the plane in the CodePointBundles table
  uint16_t  entriesCount;        // The number of entries in the CodePointBungles table
  GlyphCode firstGlyphCode;      // glyphCode corresponding to the first codePoint in
                                 // the bundles
};

struct CodePointBundle {
  char16_t firstCodePoint; // The first UTF16 codePoint of the bundle
  char16_t lastCodePoint;  // The last UTF16 codePoint of the bundle
};

typedef Plane Planes[4];
typedef CodePointBundle (*CodePointBundlesPtr)[];
typedef Plane (*PlanesPtr)[];

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

struct GlyphKernStep {
  uint16_t nextGlyphCode;
  FIX16    kern;
  bool     operator==(const GlyphKernStep &other) const {
        return (nextGlyphCode == other.nextGlyphCode) && (kern == other.kern);
  }
};
typedef std::vector<GlyphKernStep> GlyphKernSteps;

struct GlyphLigStep {
  uint16_t nextGlyphCode;
  uint16_t replacementGlyphCode;
  //
  bool operator==(const GlyphLigStep &other) const {
    return (nextGlyphCode == other.nextGlyphCode) &&
           (replacementGlyphCode == other.replacementGlyphCode);
  }
};
typedef std::vector<GlyphLigStep> GlyphLigSteps;

struct GlyphLigKern {
  GlyphLigSteps  ligSteps;
  GlyphKernSteps kernSteps;
  //
  bool operator==(const GlyphLigKern &other) const {
    if ((ligSteps.size() != other.ligSteps.size()) ||
        (kernSteps.size() != other.kernSteps.size())) {
      return false;
    }

    int idx = 0;
    for (auto &l : ligSteps) {
      if (!(l == other.ligSteps[idx])) {
        return false;
      }
      idx += 1;
    }

    idx = 0;
    for (auto &k : kernSteps) {
      if (!(k == other.kernSteps[idx])) {
        return false;
      }
      idx += 1;
    }
    return true;
  }
};
typedef std::shared_ptr<GlyphLigKern> GlyphLigKernPtr;

#pragma pack(push, 1)
struct BackupGlyphKernStep {
  char32_t nextCodePoint;
  FIX16    kern;
};
typedef std::vector<BackupGlyphKernStep> BackupGlyphKernSteps;

struct BackupGlyphLigStep {
  char32_t nextCodePoint;
  char32_t replacementCodePoint;
};
typedef std::vector<BackupGlyphLigStep> BackupGlyphLigSteps;

struct BackupGlyphLigKern {
  BackupGlyphLigSteps  ligSteps;
  BackupGlyphKernSteps kernSteps;
};
typedef std::shared_ptr<BackupGlyphLigKern> BackupGlyphLigKernPtr;
#pragma pack(pop)

// These are the structure required to create a new font
// from some parameters. For now, it is used to create UTF32
// font format files.

struct CharSelection {
  QString                 filename; // Filename to import from
  SelectedBlockIndexesPtr selectedBlockIndexes;
};
typedef std::vector<CharSelection>      CharSelections;
typedef std::shared_ptr<CharSelections> CharSelectionsPtr;

struct FontParameters {
  int               dpi;
  bool              pt8;
  bool              pt9;
  bool              pt10;
  bool              pt12;
  bool              pt14;
  bool              pt17;
  bool              pt24;
  bool              pt48;
  QString           filename;
  CharSelectionsPtr charSelections;
  bool              withKerning;
};
typedef std::shared_ptr<FontParameters> FontParametersPtr;

// Ligature table. Used to create entries in a new font defintition.
// Of course, the three letters must be present in the resulting font to have
// that ligature added to the font.

const struct Ligature {
  char32_t firstChar;
  char32_t nextChar;
  char32_t replacement;
} ligatures[] = {
    {0x0066, 0x0066, 0xFB00}, // f, f, ﬀ
    {0x0066, 0x0069, 0xFB01}, // f, i, ﬁ
    {0x0066, 0x006C, 0xFB02}, // f, l, ﬂ
    {0xFB00, 0x0069, 0xFB03}, // ﬀ ,i, ﬃ
    {0xFB00, 0x006C, 0xFB04}, // ﬀ ,l, ﬄ
    {0x0069, 0x006A, 0x0133}, // i, j, ĳ
    {0x0049, 0x004A, 0x0132}, // I, J, Ĳ
    {0x003C, 0x003C, 0x00AB}, // <, <, «
    {0x003E, 0x003E, 0x00BB}, // >, >, »
    {0x003F, 0x2018, 0x00BF}, // ?, ‘, ¿
    {0x0021, 0x2018, 0x00A1}, // !, ‘, ¡
    {0x2018, 0x2018, 0x201C}, // ‘, ‘, “
    {0x2019, 0x2019, 0x201D}, // ’, ’, ”
    {0x002C, 0x002C, 0x201E}, // , , „
    {0x2013, 0x002D, 0x2014}, // –, -, —
    {0x002D, 0x002D, 0x2013}, // -, -, –
};

// These are the corresponding Unicode value for each of the 174 characters that
// are part of an IBMF FontFormat 0 (LATIN).

const CharCodes fontFormat0CodePoints = {
    char16_t(0x0060), // `
    char16_t(0x00B4), // ´
    char16_t(0x02C6), // ˆ
    char16_t(0x02DC), // ˜
    char16_t(0x00A8), // ¨
    char16_t(0x02DD), // ˝
    char16_t(0x02DA), // ˚
    char16_t(0x02C7), // ˇ
    char16_t(0x02D8), // ˘
    char16_t(0x00AF), // ¯
    char16_t(0x02D9), // ˙
    char16_t(0x00B8), // ¸
    char16_t(0x02DB), // ˛
    char16_t(0x201A), // ‚
    char16_t(0x2039), // ‹
    char16_t(0x203A), // ›

    char16_t(0x201C), // “
    char16_t(0x201D), // ”
    char16_t(0x201E), // „
    char16_t(0x00AB), // «
    char16_t(0x00BB), // »
    char16_t(0x2013), // –
    char16_t(0x2014), // —
    char16_t(0x00BF), // ¿
    char16_t(0x2080), // ₀
    char16_t(0x0131), // ı
    char16_t(0x0237), // ȷ
    char16_t(0xFB00), // ﬀ
    char16_t(0xFB01), // ﬁ
    char16_t(0xFB02), // ﬂ
    char16_t(0xFB03), // ﬃ
    char16_t(0xFB04), // ﬄ

    char16_t(0x00A1), // ¡
    char16_t(0x0021), // !
    char16_t(0x0022), // "
    char16_t(0x0023), // #
    char16_t(0x0024), // $
    char16_t(0x0025), // %
    char16_t(0x0026), // &
    char16_t(0x2019), // ’
    char16_t(0x0028), // (
    char16_t(0x0029), // )
    char16_t(0x002A), // *
    char16_t(0x002B), // +
    char16_t(0x002C), // ,
    char16_t(0x002D), // .
    char16_t(0x002E), // -
    char16_t(0x002F), // /

    char16_t(0x0030), // 0
    char16_t(0x0031), // 1
    char16_t(0x0032), // 2
    char16_t(0x0033), // 3
    char16_t(0x0034), // 4
    char16_t(0x0035), // 5
    char16_t(0x0036), // 6
    char16_t(0x0037), // 7
    char16_t(0x0038), // 8
    char16_t(0x0039), // 9
    char16_t(0x003A), // :
    char16_t(0x003B), // ;
    char16_t(0x003C), // <
    char16_t(0x003D), // =
    char16_t(0x003E), // >
    char16_t(0x003F), // ?

    char16_t(0x0040), // @
    char16_t(0x0041), // A
    char16_t(0x0042), // B
    char16_t(0x0043), // C
    char16_t(0x0044), // D
    char16_t(0x0045), // E
    char16_t(0x0046), // F
    char16_t(0x0047), // G
    char16_t(0x0048), // H
    char16_t(0x0049), // I
    char16_t(0x004A), // J
    char16_t(0x004B), // K
    char16_t(0x004C), // L
    char16_t(0x004D), // M
    char16_t(0x004E), // N
    char16_t(0x004F), // O

    char16_t(0x0050), // P
    char16_t(0x0051), // Q
    char16_t(0x0052), // R
    char16_t(0x0053), // S
    char16_t(0x0054), // T
    char16_t(0x0055), // U
    char16_t(0x0056), // V
    char16_t(0x0057), // W
    char16_t(0x0058), // X
    char16_t(0x0059), // Y
    char16_t(0x005A), // Z
    char16_t(0x005B), // [
    char16_t(0x005C), // \ .
    char16_t(0x005D), // ]
    char16_t(0x005E), // ^
    char16_t(0x005F), // _

    char16_t(0x2018), // ‘
    char16_t(0x0061), // a
    char16_t(0x0062), // b
    char16_t(0x0063), // c
    char16_t(0x0064), // d
    char16_t(0x0065), // e
    char16_t(0x0066), // f
    char16_t(0x0067), // g
    char16_t(0x0068), // h
    char16_t(0x0069), // i
    char16_t(0x006A), // j
    char16_t(0x006B), // k
    char16_t(0x006C), // l
    char16_t(0x006D), // m
    char16_t(0x006E), // n
    char16_t(0x006F), // o

    char16_t(0x0070), // p
    char16_t(0x0071), // q
    char16_t(0x0072), // r
    char16_t(0x0073), // s
    char16_t(0x0074), // t
    char16_t(0x0075), // u
    char16_t(0x0076), // v
    char16_t(0x0077), // w
    char16_t(0x0078), // x
    char16_t(0x0079), // y
    char16_t(0x007A), // z
    char16_t(0x007B), // {
    char16_t(0x007C), // |
    char16_t(0x007D), // }
    char16_t(0x007E), // ~
    char16_t(0x013D), // Ľ

    char16_t(0x0141), // Ł
    char16_t(0x014A), // Ŋ
    char16_t(0x0132), // Ĳ
    char16_t(0x0111), // đ
    char16_t(0x00A7), // §
    char16_t(0x010F), // ď
    char16_t(0x013E), // ľ
    char16_t(0x0142), // ł
    char16_t(0x014B), // ŋ
    char16_t(0x0165), // ť
    char16_t(0x0133), // ĳ
    char16_t(0x00A3), // £
    char16_t(0x00C6), // Æ
    char16_t(0x00D0), // Ð
    char16_t(0x0152), // Œ
    char16_t(0x00D8), // Ø

    char16_t(0x00DE), // Þ
    char16_t(0x1E9E), // ẞ
    char16_t(0x00E6), // æ
    char16_t(0x00F0), // ð
    char16_t(0x0153), // œ
    char16_t(0x00F8), // ø
    char16_t(0x00FE), // þ
    char16_t(0x00DF), // ß
    char16_t(0x00A2), // ¢
    char16_t(0x00A4), // ¤
    char16_t(0x00A5), // ¥
    char16_t(0x00A6), // ¦
    char16_t(0x00A9), // ©
    char16_t(0x00AA), // ª
    char16_t(0x00AC), // ¬
    char16_t(0x00AE), // ®

    char16_t(0x00B1), // ±
    char16_t(0x00B2), // ²
    char16_t(0x00B3), // ³
    char16_t(0x00B5), // µ
    char16_t(0x00B6), // ¶
    char16_t(0x00B7), // ·
    char16_t(0x00B9), // ¹
    char16_t(0x00BA), // º
    char16_t(0x00D7), // ×
    char16_t(0x00BC), // ¼
    char16_t(0x00BD), // ½
    char16_t(0x00BE), // ¾
    char16_t(0x00F7), // ÷
    char16_t(0x20AC)  // €
};

// This table is used in support of the latin character set to identify which CodePoint
// correspond to which glyph code. A glyph code is an index into the IBMF list of glyphs,
// with diacritical information when required. The table allows glyphCodes between 0 and 2045
// (0x7FD).
//
// At bit positions 12..15, the table contains the diacritical symbol to be added to the glyph if
// it's value is not 0. In this table, grave accent is identified as 0xF but in the IBMF format, it
// has glyphCode index 0x000. The apostrophe is identified as 0x0E, with glyphCode index of 0x0027;
//
// Note: At this moment, there is no glyphCode index that have a value greater than 173.
//
// The index in the table corresponds to UTF16 U+00A1 to U+017F CodePoints.

const constexpr uint16_t LATIN_GLYPH_CODE_MASK  = 0x7FF;

const constexpr GlyphCode latinTranslationSet[] = {
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

} // namespace IBMFDefs
