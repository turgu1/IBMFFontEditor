#pragma once

#include <cstring>

#include "ibmf_defs.hpp"
using namespace IBMFDefs;

#define DEBUG 0

#if DEBUG
#include <iomanip>
#include <iostream>
#endif

#define firstIsBlack(c) (c == 0)
#define SET_AS_FIRST_BLACK (0)
#define SET_AS_REPEAT_COUNT(c) (-c)
#define REPEAT_COUNT(c) (-c)
#define IS_A_REPEAT_COUNT(c) (c < 0)
#define REPEAT_COUNT_IS_ONE(c) (c == -1)

class RLEGenerator {

public:
  typedef std::vector<uint8_t> Data;

  typedef Data *DataPtr;

private:
  uint8_t value;
  bool firstNyb;
  Data data;

  typedef std::vector<int16_t> RepeatCounts;
  typedef int Chunk;
  typedef std::vector<Chunk> Chunks;

  uint8_t dynF; // = 14 if not compressed
  bool
      firstIsBlack; // if compressed, true if first nibble contains black pixels

public:
  RLEGenerator() {
    value = 0;
    firstNyb = true;
    data.clear();
  }

  uint8_t getDynF() { return dynF; }
  bool getFirstIsBlack() { return firstIsBlack; }
  DataPtr getData() { return &data; }

  void clean() {
    value = 0;
    firstNyb = true;
    data.clear();
  }

#if DEBUG
  void show() {
    std::cout << "RLEGenerator content:" << std::endl
              << "  First nybble is " << (firstIsBlack ? "" : "NOT ")
              << "for black pixels." << std::endl
              << "  dynF value is " << +dynF << std::endl;

    for (auto byte : data) {
      std::cout << std::hex << std::setfill('0') << std::setw(2)
                << std::uppercase << +byte << ' ';
    }
    std::cout << std::endl;
  }
#endif

  void putNyb(uint8_t val) {
    if (firstNyb) {
      value = val;
    } else {
      value = (value << 4) | val;
      data.push_back(value);
    }
    firstNyb = !firstNyb;
  }

  void putByte(uint8_t val) {
    value = val;
    data.push_back(val);
    firstNyb = true;
  }

  void putRemainder() {
    if (!firstNyb) {
      value <<= 4;
    }
    data.push_back(value);
    firstNyb = true;
  }

#if DEBUG
  void showChunks(const Chunks &chunks) {
    bool first = true;
    bool black = false;
    for (auto chunk : chunks) {
      if (first) {
        first = false;
        if (firstIsBlack(chunk)) {
          black = true;
          continue;
        }
      }

      if (IS_A_REPEAT_COUNT(chunk))
        std::cout << '[' << REPEAT_COUNT(chunk) << "] ";
      else if (black)
        std::cout << chunk << " ";
      else
        std::cout << '(' << chunk << ") ";

      black = !black;
    }
    std::cout << std::endl;
  }

  void showRepeatCounts(const RepeatCounts repeatCounts) {
    for (auto count : repeatCounts)
      std::cout << count << " ";
    std::cout << std::endl;
  }
#endif

  void computeChunks(Chunks &chunks, const IBMFDefs::Bitmap &bitmap,
                      const RepeatCounts &repeatCounts) {
    chunks.clear();
    chunks.reserve(50);
    Chunk chunk;
    uint8_t val;
    if ((val = bitmap.pixels[0]) != 0)
      chunks.push_back(SET_AS_FIRST_BLACK);
    chunk = 1;
    int row = 0;
    int col = 1;
    int idx = 1;
    bool show_repeat = repeatCounts[row] > 0;
    while (row < bitmap.dim.height) {
      while (col < bitmap.dim.width) {
        if (val == bitmap.pixels[idx++]) {
          chunk++;
        } else {
          chunks.push_back(chunk);
          if (show_repeat) {
            show_repeat = false;
            chunks.push_back(SET_AS_REPEAT_COUNT(repeatCounts[row]));
          }
          val ^= 0xFF;
          chunk = 1;
        }
        col++;
      }
      row++;
      col = 0;
      while ((row < bitmap.dim.height) && (repeatCounts[row] == -1)) {
        row++;
        idx += bitmap.dim.width;
      }
      show_repeat = repeatCounts[row] > 0;
    }
    chunks.push_back(chunk);
  }

  void computeRepeatCounts(const Bitmap &bitmap, RepeatCounts &repeatCounts) {
    int row, col, current;
    uint8_t val;
    bool same;

    repeatCounts.clear();
    repeatCounts.reserve(bitmap.dim.height);

    for (row = 0; row < bitmap.dim.height; row++)
      repeatCounts.push_back(0);

    row = 0;
    current = 1;
    while (current < bitmap.dim.height) {
      // Check if all pixels are the same, if so, we pass the row
      val = bitmap.pixels[row * bitmap.dim.width];
      same = true;
      for (col = 1; col < bitmap.dim.width; col++) {
        same = val == bitmap.pixels[row * bitmap.dim.width + col];
        if (!same)
          break;
      }
      if (same) {
        row++;
        current++;
      } else {
        same = true;
        for (col = 0; col < bitmap.dim.width; col++) {
          same = bitmap.pixels[row * bitmap.dim.width + col] ==
                 bitmap.pixels[current * bitmap.dim.width + col];
          if (!same)
            break;
        }
        if (same) {
          repeatCounts[row]++;
          repeatCounts[current++] = -1;
        } else {
          row = current;
          current++;
        }
      }
    }
  }

  bool encodeBitmap(const Bitmap &bitmap) {

    if ((bitmap.dim.height * bitmap.dim.width) == 0)
      return false;

    int compSize = 0;
    int deriv[14]; // index 1..13 are used

    memset(deriv, 0, sizeof(deriv));

    // compute compression size and dynF

    RepeatCounts repeatCounts;
    Chunks chunks;

    computeRepeatCounts(bitmap, repeatCounts);
    computeChunks(chunks, bitmap, repeatCounts);

#if DEBUG
    showRepeatCounts(repeatCounts);
    showChunks(chunks);
#endif

    firstIsBlack = false;

    bool first = true;
    for (auto chunk : chunks) {
      if (first) {
        first = false;
        if (firstIsBlack(chunk)) {
          firstIsBlack = true;
          continue;
        }
      }
      int count = chunk;
      if (REPEAT_COUNT_IS_ONE(count))
        compSize += 1;
      else {
        if (IS_A_REPEAT_COUNT(count)) {
          compSize += 1;
          count = REPEAT_COUNT(count);
        }
        if (count < 209)
          compSize += 2;
        else {
          int k = count - 193;
          while (k >= 16) {
            k >>= 4;
            compSize += 2;
          }
          compSize += 1;
        }
        if (count < 14)
          deriv[count] -= 1;
        else if (count < 209)
          deriv[(223 - count) / 15] += 1;
        else {
          int k = 16;
          while ((k << 4) < (count + 3))
            k <<= 4;
          if ((count - k) <= 192)
            deriv[(207 - count + k) / 15] += 2;
        }
      }
    }

    int bCompSize = compSize;
    dynF = 0;

    for (int i = 1; i <= 13; i++) {
      compSize += deriv[i];
      if (compSize <= bCompSize) {
        bCompSize = compSize;
        dynF = i;
      }
    }

    compSize = (bCompSize + 1) >> 1;
    if ((compSize > ((bitmap.dim.height * bitmap.dim.width + 7) >> 3))) {
      compSize = (bitmap.dim.height * bitmap.dim.width + 7) >> 3;
      dynF = 14;
    }

    data.reserve(compSize);

#if DEBUG
    std::cout << "Best packing is dynF of " << dynF << " with length "
              << compSize << "." << std::endl;
#endif

    if (dynF != 14) {

      // ---- Send rle format ----

      const int max_2 =
          208 - 15 * dynF; // the highest count that fits in two bytes

      bool first = true;
      for (auto chunk : chunks) {
        if (first) {
          first = false;
          if (firstIsBlack(chunk)) {
            continue;
          }
        }
        int count = chunk;
        if (REPEAT_COUNT_IS_ONE(count)) {
          putNyb(15);
        } else {
          if (IS_A_REPEAT_COUNT(count)) {
            putNyb(14);
            count = REPEAT_COUNT(count);
          }
          if (count <= dynF) {
            putNyb(count);
          } else if (count <= max_2) {
            count = count - dynF - 1;
            putNyb((count >> 4) + dynF + 1);
            putNyb(count & 0x0F);
          } else {
            count = count - max_2 + 15;
            int k = 16;
            while (k <= count) {
              k <<= 4;
              putNyb(0);
            }
            while (k > 1) {
              k >>= 4;
              putNyb(count / k);
              count %= k;
            }
          }
        }
      }
      if (!firstNyb)
        putRemainder();
    } else {

      // ---- Send bit map (rle uncompressed format) ----

      uint8_t buff = 0;
      int pBit = 8;
      int i = 1; // index in the chunks list
      int rI = 0;
      int sI = 0;
      int hBit = bitmap.dim.width;
      bool on = false; // true if it's for black pixels
      bool rOn = false;
      bool sOn = false;
      bool repeating = false; // true if we are repeating previous line
      int count = chunks[0];  // number of bits in the current chunk
      int rCount = 0;
      int sCount = 0;
      int repeatFlag = 0;

      while ((i < chunks.size()) || repeating || (count > 0)) {
        if (repeating) {
          count = rCount;
          i = rI;
          on = rOn;
          repeatFlag -= 1;
        } else {
          rCount = count;
          rI = i;
          rOn = on;
        }

        // Send one row of bits
        do {
          if (count == 0) {
            if (chunks[i] < 0) {
              if (!repeating) {
                repeatFlag = -chunks[i];
              }
              i += 1;
            }
            count = chunks[i++];
            on = !on;
          }
          if ((count >= pBit) && (pBit < hBit)) {
            // we end a byte, we don’t end the row
            if (on) {
              buff += (1 << pBit) - 1;
            }
            putByte(buff);
            buff = 0;
            hBit -= pBit;
            count -= pBit;
            pBit = 8;
          } else if ((count < pBit) && (count < hBit)) {
            // we end neither the row nor the byte
            if (on) {
              buff += (1 << pBit) - (1 << (pBit - count));
            }
            pBit -= count;
            hBit -= count;
            count = 0;
          } else {
            // we end a row and maybe a byte
            if (on) {
              buff += (1 << pBit) - (1 << (pBit - hBit));
            }
            count -= hBit;
            pBit -= hBit;
            hBit = bitmap.dim.width;
            if (pBit == 0) {
              putByte(buff);
              buff = 0;
              pBit = 8;
            }
          }
        } while (hBit != bitmap.dim.width);

        if (repeating && (repeatFlag == 0)) {
          count = sCount;
          i = sI;
          on = sOn;
          repeating = false;
        } else if (!repeating && (repeatFlag > 0)) {
          sCount = count;
          sI = i;
          sOn = on;
          repeating = true;
        }
      }
      if (pBit != 8)
        putByte(buff);
    }

    return true;
  }
};
