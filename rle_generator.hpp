#pragma once

#include <cstring>

#include "defs.hpp"

#define DEBUG 0

#define FIRST_IS_BLACK(c)      (c == 0)
#define SET_AS_FIRST_BLACK     (0)
#define SET_AS_REPEAT_COUNT(c) (-c)
#define REPEAT_COUNT(c)        (-c)
#define IS_A_REPEAT_COUNT(c)   (c < 0)
#define REPEAT_COUNT_IS_ONE(c) (c == -1)

class RLEGenerator {
  private:
    uint8_t value;
    bool first_nyb;
    std::vector<uint8_t> data;

    typedef std::vector<int16_t> RepeatCounts;
    typedef int Chunk;
    typedef std::vector<Chunk> Chunks;

  public:
    RLEGenerator() {
      value = 0;
      first_nyb = true;
      data.clear();
    }

    uint8_t dyn_f;       // = 14 if not compressed
    bool first_is_black; // if compressed, true if first nibble contains black pixels

    void clean() {
      value = 0;
      first_nyb = true;
      data.clear();
    }

    #if DEBUG
      void show() {
        std::cout << "RLEGenerator content:"
                  << std::endl
                  << "  First nybble is "
                  << (first_is_black ? "" : "NOT ")
                  << "for black pixels."
                  << std::endl
                  << "  dyn_f value is "
                  << +dyn_f
                  << std::endl;

        for (auto byte : data) {
          std::cout << std::hex
                    << std::setfill('0')
                    << std::setw(2)
                    << std::uppercase
                    << +byte << ' ';
        }
        std::cout << std::endl;
      }
    #endif

    void put_nyb(uint8_t val) {
      if (first_nyb) value = val;
      else {
        value = (value << 4) | val;
        data.push_back(value);
      }
      first_nyb = !first_nyb;
    }

    void put_byte(uint8_t val) {
      value = val;
      data.push_back(value);
      first_nyb = true;
    }

    void put_remainder() {
      if (first_nyb) value <<= 4;
      data.push_back(value);
    }

    #if DEBUG
      void show_chunks(const Chunks & chunks)
      {
        bool first = true;
        bool black = false;
        for (auto chunk : chunks) {
          if (first) {
            first = false;
            if (FIRST_IS_BLACK(chunk)) {
              black = true;
              continue;
            }
          }

          if (IS_A_REPEAT_COUNT(chunk)) std::cout << '[' << REPEAT_COUNT(chunk) << "] ";
          else if (black) std::cout << chunk << " ";
          else std::cout << '(' << chunk << ") ";

          black = !black;
        }
        std::cout << std::endl;
      }

      void show_repeat_counts(const RepeatCounts repeat_counts)
      {
        for (auto count : repeat_counts) std::cout << count << " ";
        std::cout << std::endl;
      }
    #endif

    void
    compute_chunks(Chunks & chunks, const Bitmap & bitmap, const RepeatCounts & repeat_counts)
    {
      chunks.clear(); chunks.reserve(50);
      Chunk chunk;
      uint8_t val;
      if ((val = bitmap.pixels[0]) != 0) chunks.push_back(SET_AS_FIRST_BLACK);
      chunk = 1;
      int row = 0;
      int col = 1;
      int idx = 1;
      bool show_repeat = repeat_counts[row] > 0;
      while (row < bitmap.dim.height) {
        while (col < bitmap.dim.width) {
          if (val == bitmap.pixels[idx++]) {
            chunk++;
          }
          else {
            chunks.push_back(chunk);
            if (show_repeat) {
              show_repeat = false;
              chunks.push_back(SET_AS_REPEAT_COUNT(repeat_counts[row]));
            }
            val ^= 0xFF;
            chunk = 1;
          }
          col++;
        }
        row++;
        col = 0;
        while ((row < bitmap.dim.height) && (repeat_counts[row] == -1)) {
          row++;
          idx += bitmap.dim.width;
        }
        show_repeat = repeat_counts[row] > 0;
      }
      chunks.push_back(chunk);
    }

    void
    compute_repeat_counts(const Bitmap & bitmap, RepeatCounts & repeat_counts)
    {
      int row, col, current;
      uint8_t val;
      bool same;

      repeat_counts.clear();
      repeat_counts.reserve(bitmap.dim.height);

      for (row = 0; row < bitmap.dim.height; row++) repeat_counts.push_back(0);

      row = 0;
      current = 1;
      while (current < bitmap.dim.height) {
        // Check if all pixels are the same, if so, we pass the row
        val = bitmap.pixels[row * bitmap.dim.width];
        same = true;
        for (col = 1; col < bitmap.dim.width; col++) {
          same = val == bitmap.pixels[row * bitmap.dim.width + col];
          if (!same) break;
        }
        if (same) {
          row++;
          current++;
        }
        else {
          same = true;
          for (col = 0; col < bitmap.dim.width; col++) {
            same = bitmap.pixels[row * bitmap.dim.width + col] == bitmap.pixels[current * bitmap.dim.width + col];
            if (!same) break;
          }
          if (same) {
            repeat_counts[row]++;
            repeat_counts[current++] = -1;
          }
          else {
            row = current;
            current++;
          }
        }
      }
    }

    bool encode_bitmap(const Bitmap & bitmap) {

      if ((bitmap.dim.height * bitmap.dim.width) == 0) return false;

      int comp_size = 0;
      int deriv[14];    // index 1..13 are used

      memset(deriv, 0, sizeof(deriv));

      // compute compression size and dyn_f

      RepeatCounts repeat_counts;
      Chunks chunks;

      compute_repeat_counts(bitmap, repeat_counts);
      compute_chunks(chunks, bitmap, repeat_counts);

      #if DEBUG
        show_repeat_counts(repeat_counts);
        show_chunks(chunks);
      #endif

      first_is_black = false;

      bool first = true;
      for (auto chunk : chunks) {
        if (first) {
          first = false;
          if (FIRST_IS_BLACK(chunk)) {
            first_is_black = true;
            continue;
          }
        }
        int count = chunk;
        if (REPEAT_COUNT_IS_ONE(count)) comp_size += 1;
        else {
          if (IS_A_REPEAT_COUNT(count)) {
            comp_size += 1;
            count = REPEAT_COUNT(count);
          }
          if (count < 209) comp_size += 2;
          else {
            int k = count - 193;
            while (k >= 16) {
              k >>= 4;
              comp_size += 2;
            }
            comp_size += 1;
          }
          if (count < 14) deriv[count] -= 1;
          else if (count < 209) deriv[(223 - count) / 15] += 1;
          else {
            int k = 16;
            while ((k << 4) < (count + 3)) k <<= 4;
            if ((count - k) <= 192) deriv[(207 - count + k) / 15] += 2;
          }
        }
      }

      int b_comp_size = comp_size;
      dyn_f = 0;

      for (int i = 1; i <= 13; i++) {
        comp_size += deriv[i];
        if (comp_size <= b_comp_size) {
          b_comp_size = comp_size;
          dyn_f = i;
        }
      }

      comp_size = (b_comp_size + 1) >> 1;
      if ((comp_size > ((bitmap.dim.height * bitmap.dim.width + 7) >> 3))) {
        comp_size = (bitmap.dim.height * bitmap.dim.width + 7) >> 3;
        dyn_f = 14;
      }

      data.reserve(comp_size);

      #if DEBUG
        std::cout << "Best packing is dyn_f of "
                  << dyn_f
                  << " with length "
                  << comp_size
                  << "." << std::endl;
      #endif

      if (dyn_f != 14) {

        // ---- Send compress format ----

        int bit_weight = 16;
        const int max_2 = 208 - 15 * dyn_f; // the highest count that fits in two bytes

        bool first = true;
        for (auto chunk : chunks) {
          if (first) {
            first = false;
            if (FIRST_IS_BLACK(chunk)) continue;
          }
          int count = chunk;
          if (REPEAT_COUNT_IS_ONE(count)) put_nyb(15);
          else {
            if (IS_A_REPEAT_COUNT(count)) {
              put_nyb(14);
              count = REPEAT_COUNT(count);
            }
            if (count <= dyn_f) put_nyb(count);
            else if (count <= max_2) {
              count = count - dyn_f - 1;
              put_nyb((count >> 4) + dyn_f + 1);
              put_nyb(count & 0x0F);
            }
            else {
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
        if (bit_weight != 16) put_remainder();
      }
      else {

        // ---- Send bit map (uncompress format) ----

        int idx = 0;
        for (int row = 0; row < bitmap.dim.width; row++) {
          int byte = 0;
          int bit_count = 0;
          for (int col = 0; col < bitmap.dim.height; col++) {
            byte = (byte << 1) + ((bitmap.pixels[idx++] == 0) ? 0 : 1);
            if (++bit_count == 8) {
              put_byte(byte);
              bit_count = 0;
            }
            if (bit_count != 0) put_byte(byte << (8 - bit_count));
          }
        }
      }

      return true;
    }
};
