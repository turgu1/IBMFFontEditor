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
  bool first_nyb;
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
    first_nyb = true;
    data.clear();
  }

  uint8_t get_dynF() { return dynF; }
  bool get_firstIsBlack() { return firstIsBlack; }
  DataPtr get_data() { return &data; }

  void clean() {
    value = 0;
    first_nyb = true;
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

  void put_nyb(uint8_t val) {
    if (first_nyb)
      value = val;
    else {
      value = (value << 4) | val;
      data.push_back(value);
    }
    first_nyb = !first_nyb;
  }

  void put_byte(uint8_t val) {
    value = val;
    data.push_back(val);
    first_nyb = true;
  }

  void put_remainder() {
    if (!first_nyb)
      value <<= 4;
    data.push_back(value);
    first_nyb = true;
  }

#if DEBUG
  void show_chunks(const Chunks &chunks) {
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

  void show_repeatCounts(const RepeatCounts repeatCounts) {
    for (auto count : repeatCounts)
      std::cout << count << " ";
    std::cout << std::endl;
  }
#endif

  void compute_chunks(Chunks &chunks, const IBMFDefs::Bitmap &bitmap,
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

  void compute_repeatCounts(const Bitmap &bitmap, RepeatCounts &repeatCounts) {
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

  bool encode_bitmap(const Bitmap &bitmap) {

    if ((bitmap.dim.height * bitmap.dim.width) == 0)
      return false;

    int comp_size = 0;
    int deriv[14]; // index 1..13 are used

    memset(deriv, 0, sizeof(deriv));

    // compute compression size and dynF

    RepeatCounts repeatCounts;
    Chunks chunks;

    compute_repeatCounts(bitmap, repeatCounts);
    compute_chunks(chunks, bitmap, repeatCounts);

#if DEBUG
    show_repeatCounts(repeatCounts);
    show_chunks(chunks);
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
        comp_size += 1;
      else {
        if (IS_A_REPEAT_COUNT(count)) {
          comp_size += 1;
          count = REPEAT_COUNT(count);
        }
        if (count < 209)
          comp_size += 2;
        else {
          int k = count - 193;
          while (k >= 16) {
            k >>= 4;
            comp_size += 2;
          }
          comp_size += 1;
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

    int b_comp_size = comp_size;
    dynF = 0;

    for (int i = 1; i <= 13; i++) {
      comp_size += deriv[i];
      if (comp_size <= b_comp_size) {
        b_comp_size = comp_size;
        dynF = i;
      }
    }

    comp_size = (b_comp_size + 1) >> 1;
    if ((comp_size > ((bitmap.dim.height * bitmap.dim.width + 7) >> 3))) {
      comp_size = (bitmap.dim.height * bitmap.dim.width + 7) >> 3;
      dynF = 14;
    }

    data.reserve(comp_size);

#if DEBUG
    std::cout << "Best packing is dynF of " << dynF << " with length "
              << comp_size << "." << std::endl;
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
          put_nyb(15);
        } else {
          if (IS_A_REPEAT_COUNT(count)) {
            put_nyb(14);
            count = REPEAT_COUNT(count);
          }
          if (count <= dynF) {
            put_nyb(count);
          } else if (count <= max_2) {
            count = count - dynF - 1;
            put_nyb((count >> 4) + dynF + 1);
            put_nyb(count & 0x0F);
          } else {
            count = count - max_2 + 15;
            int k = 16;
            while (k <= count) {
              k <<= 4;
              put_nyb(0);
            }
            while (k > 1) {
              k >>= 4;
              put_nyb(count / k);
              count %= k;
            }
          }
        }
      }
      if (!first_nyb)
        put_remainder();
    } else {

      // ---- Send bit map (rle uncompressed format) ----

      uint8_t buff = 0;
      int p_bit = 8;
      int i = 1; // index in the chunks list
      int r_i = 0;
      int s_i = 0;
      int h_bit = bitmap.dim.width;
      bool on = false; // true if it's for black pixels
      bool r_on = false;
      bool s_on = false;
      bool repeating = false; // true if we are repeating previous line
      int count = chunks[0];  // number of bits in the current chunk
      int r_count = 0;
      int s_count = 0;
      int repeat_flag = 0;

      while ((i < chunks.size()) || repeating || (count > 0)) {
        if (repeating) {
          count = r_count;
          i = r_i;
          on = r_on;
          repeat_flag -= 1;
        } else {
          r_count = count;
          r_i = i;
          r_on = on;
        }

        // Send one row of bits
        do {
          if (count == 0) {
            if (chunks[i] < 0) {
              if (!repeating)
                repeat_flag = -chunks[i];
              i += 1;
            }
            count = chunks[i++];
            on = !on;
          }
          if ((count >= p_bit) && (p_bit < h_bit)) {
            // we end a byte, we don’t end the row
            if (on)
              buff += (1 << p_bit) - 1;
            put_byte(buff);
            buff = 0;
            h_bit -= p_bit;
            count -= p_bit;
            p_bit = 8;
          } else if ((count < p_bit) && (count < h_bit)) {
            // we end neither the row nor the byte
            if (on)
              buff += (1 << p_bit) - (1 << (p_bit - count));
            p_bit -= count;
            h_bit -= count;
            count = 0;
          } else {
            // we end a row and maybe a byte
            if (on)
              buff += (1 << p_bit) - (1 << (p_bit - h_bit));
            count -= h_bit;
            p_bit -= h_bit;
            h_bit = bitmap.dim.width;
            if (p_bit == 0) {
              put_byte(buff);
              buff = 0;
              p_bit = 8;
            }
          }
        } while (h_bit != bitmap.dim.width);

        if (repeating && (repeat_flag == 0)) {
          count = s_count;
          i = s_i;
          on = s_on;
          repeating = false;
        } else if (!repeating && (repeat_flag > 0)) {
          s_count = count;
          s_i = i;
          s_on = on;
          repeating = true;
        }
      }
      if (p_bit != 8)
        put_byte(buff);
    }

    return true;
  }
};
