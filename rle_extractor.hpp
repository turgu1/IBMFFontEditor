#pragma once

#include <cinttypes>
#include <cstring>
#include <iostream>

#include "ibmf_defs.hpp"

using namespace IBMFDefs;

class RLEExtractor {
private:
  uint32_t repeatCount;

  MemoryPtr memoryPtr, memoryEnd;

  const uint8_t PK_REPEAT_COUNT = 14;
  const uint8_t PK_REPEAT_ONCE  = 15;

  bool getnext8(uint8_t &val) {
    if (memoryPtr >= memoryEnd) return false;
    val = *memoryPtr++;
    return true;
  }

  uint8_t nybbleFlipper = 0xf0U;
  uint8_t nybbleByte;

  bool getNybble(uint8_t &nyb) {
    if (nybbleFlipper == 0xf0U) {
      if (!getnext8(nybbleByte)) return false;
      nyb = nybbleByte >> 4;
    } else {
      nyb = (nybbleByte & 0x0f);
    }
    nybbleFlipper ^= 0xffU;
    return true;
  }

  // Pseudo-code:
  //
  // function pk_packed_num: integer;
  // var i,j,k: integer;
  // begin
  //   i := get_nyb;
  //   if i = 0 then begin
  //     repeat
  //       j := getnyb; incr(i);
  //     until j != 0;
  //     while i > 0 do begin
  //       j := j * 16 + get_nyb;
  //       decr(i);
  //     end;
  //     pk_packed_num := j - 15 + (13 - dynF) * 16 + dynF;
  //   end
  //   else if i <= dynF then
  //     pk_packed_num := i
  //   else if i < 14 then
  //     pk_packed_num := (i - dynF - 1) * 16 + get_nyb + dynF + 1
  //   else begin
  //     if repeatCount != 0 then abort('Extra repeat count!');
  //     if i = 14 then
  //        repeatCount := pk_packed_num
  //     else
  //        repeatCount := 1;
  //     send_out(true, repeatCount);
  //     pk_packed_num := pk_packed_num;
  //   end;
  // end;

  bool getPackedNumber(uint32_t &val, const RLEMetrics &rleMetrics) {
    uint8_t  nyb;
    uint32_t i, j;

    uint8_t dynF = rleMetrics.dynF;

    while (true) {
      if (!getNybble(nyb)) return false;
      i = nyb;
      if (i == 0) {
        do {
          if (!getNybble(nyb)) return false;
          i++;
        } while (nyb == 0);
        j = nyb;
        while (i-- > 0) {
          if (!getNybble(nyb)) return false;
          j = (j << 4) + nyb;
        }
        val = j - 15 + ((13 - dynF) << 4) + dynF;
        break;
      } else if (i <= dynF) {
        val = i;
        break;
      } else if (i < PK_REPEAT_COUNT) {
        if (!getNybble(nyb)) return false;
        val = ((i - dynF - 1) << 4) + nyb + dynF + 1;
        break;
      } else {
        // if (repeatCount != 0) {
        //   std::cerr << "Spurious repeatCount iteration!" << std::endl;
        //   return false;
        // }
        if (i == PK_REPEAT_COUNT) {
          if (!getPackedNumber(repeatCount, rleMetrics)) return false;
        } else { // i == PK_REPEAT_ONCE
          repeatCount = 1;
        }
      }
    }
    return true;
  }

  inline void copyOneRowEightBits(MemoryPtr fromLine, MemoryPtr toLine, int16_t fromCol,
                                  int size) const {
    memcpy(toLine + fromCol, fromLine + fromCol, size);
  }

  void copyOneRowOneBit(MemoryPtr fromLine, MemoryPtr toLine, int16_t fromCol, int size) const {
    uint8_t mask = 0x80 >> (fromCol & 7);
    for (int col = fromCol; col < fromCol + size; col++) {
      if constexpr (BLACK_ONE_BIT) {
        toLine[col >> 3] |= (fromLine[col >> 3] & mask);
      } else {
        toLine[col >> 3] &= fromLine[col >> 3] | ~mask;
      }
      mask >>= 1;
      if (mask == 0) mask = 0x80;
    }
  }

public:
  bool retrieveBitmap(const RLEBitmap &fromBitmap, Bitmap &toBitmap, const Pos atOffset,
                      const RLEMetrics rleMetrics) {
    // point on the glyphs' bitmap definition
    memoryPtr = (MemoryPtr)fromBitmap.pixels.data();
    memoryEnd = memoryPtr + fromBitmap.length;
    MemoryPtr toRowPtr;

    if ((atOffset.x < 0) || (atOffset.y < 0) ||
        ((atOffset.y + fromBitmap.dim.height) > toBitmap.dim.height) ||
        ((atOffset.x + fromBitmap.dim.width) > toBitmap.dim.width))
      return false;

    if (resolution == PixelResolution::ONE_BIT) {
      uint32_t toRowSize = (toBitmap.dim.width + 7) >> 3;
      toRowPtr           = toBitmap.pixels.data() + (atOffset.y * toRowSize);

      if (rleMetrics.dynF == 14) { // is a non-compressed RLE?
        uint32_t count = 8;
        uint8_t  data;

        for (uint32_t fromRow = 0; fromRow < fromBitmap.dim.height;
             fromRow++, toRowPtr += toRowSize) {
          for (uint32_t toCol = atOffset.x; toCol < fromBitmap.dim.width + atOffset.x; toCol++) {
            if (count >= 8) {
              if (!getnext8(data)) {
                std::cerr << "Not enough bitmap data!" << std::endl;
                return false;
              }
              // std::cout << std::hex << +data << ' ';
              count = 0;
            }
            if (data & (0x80U >> count)) toRowPtr[toCol >> 3] |= (0x80U >> (toCol & 7));
            count++;
          }
        }
        // std::cout << std::endl;
      } else {
        uint32_t count = 0;

        repeatCount    = 0;
        nybbleFlipper  = 0xf0U;

        bool black     = !(rleMetrics.firstIsBlack == 1);

        for (uint32_t fromRow = 0; fromRow < fromBitmap.dim.height;
             fromRow++, toRowPtr += toRowSize) {
          for (uint32_t toCol = atOffset.x; toCol < fromBitmap.dim.width + atOffset.x; toCol++) {
            if (count == 0) {
              if (!getPackedNumber(count, rleMetrics)) {
                return false;
              }
              black = !black;
              // if (black) {
              //   std::cout << count << ' ';
              // }
              // else {
              //   std::cout << '(' << count << ')' << ' ';
              // }
            }
            if (black) toRowPtr[toCol >> 3] |= (0x80U >> (toCol & 0x07));
            count--;
          }

          // if (repeatCount != 0) std::cout << "Repeat count: " << repeatCount <<
          // std::endl;
          while ((fromRow < fromBitmap.dim.height) && (repeatCount-- > 0)) {
            copyOneRowOneBit(toRowPtr, toRowPtr + toRowSize, atOffset.x, fromBitmap.dim.width);
            fromRow++;
            toRowPtr += toRowSize;
          }

          repeatCount = 0;
        }
        // std::cout << std::endl;
      }
    } else {
      uint32_t toRowSize = toBitmap.dim.width;
      toRowPtr           = toBitmap.pixels.data() + (atOffset.y * toRowSize);

      repeatCount        = 0;
      nybbleFlipper      = 0xf0U;

      if (rleMetrics.dynF == 14) { // is a bitmap?
        uint32_t count = 8;
        uint8_t  data;

        for (uint32_t fromRow = 0; fromRow < (fromBitmap.dim.height);
             fromRow++, toRowPtr += toRowSize) {
          for (uint32_t toCol = atOffset.x; toCol < (fromBitmap.dim.width + atOffset.x); toCol++) {
            if (count >= 8) {
              if (!getnext8(data)) {
                std::cerr << "Not enough bitmap data!" << std::endl;
                return false;
              }
              // std::cout << std::hex << +data << ' ';
              count = 0;
            }
            toRowPtr[toCol] = (data & (0x80U >> count)) ? 0xFF : 0;
            count++;
          }
        }
        // std::cout << std::endl;
      } else {
        uint32_t count = 0;

        repeatCount    = 0;
        nybbleFlipper  = 0xf0U;

        bool black     = !(rleMetrics.firstIsBlack == 1);

        for (uint32_t fromRow = 0; fromRow < (fromBitmap.dim.height);
             fromRow++, toRowPtr += toRowSize) {
          for (uint32_t toCol = atOffset.x; toCol < (fromBitmap.dim.width + atOffset.x); toCol++) {
            if (count == 0) {
              if (!getPackedNumber(count, rleMetrics)) {
                return false;
              }
              black = !black;
              // if (black) {
              //   std::cout << count << ' ';
              // }
              // else {
              //   std::cout << '(' << count << ')' << ' ';
              // }
            }
            if (black) toRowPtr[toCol] = 0xFF;
            count--;
          }

          // if (repeatCount != 0) std::cout << "Repeat count: " << repeatCount <<
          // std::endl;
          while ((fromRow < toBitmap.dim.height) && (repeatCount-- > 0)) {
            copyOneRowEightBits(toRowPtr, toRowPtr + toRowSize, atOffset.x, fromBitmap.dim.width);
            fromRow++;
            toRowPtr += toRowSize;
          }

          repeatCount = 0;
        }
        // std::cout << std::endl;
      }
    }
    return true;
  }

public:
  RLEExtractor() {}
};
