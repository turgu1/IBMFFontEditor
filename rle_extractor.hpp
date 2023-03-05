#pragma once

#include <cinttypes>
#include <cstring>
#include <iostream>

#include "ibmf_defs.hpp"

using namespace IBMFDefs;

class RLEExtractor {
private:
    uint32_t repeat_count;

    MemoryPtr memory_ptr, memory_end;

    const uint8_t PK_REPEAT_COUNT = 14;
    const uint8_t PK_REPEAT_ONCE = 15;

    bool getnext8(uint8_t &val) {
        if (memory_ptr >= memory_end) return false;
        val = *memory_ptr++;
        return true;
    }

    uint8_t nybble_flipper = 0xf0U;
    uint8_t nybble_byte;

    bool get_nybble(uint8_t &nyb) {
        if (nybble_flipper == 0xf0U) {
            if (!getnext8(nybble_byte)) return false;
            nyb = nybble_byte >> 4;
        } else {
            nyb = (nybble_byte & 0x0f);
        }
        nybble_flipper ^= 0xffU;
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
    //     pk_packed_num := j - 15 + (13 - dyn_f) * 16 + dyn_f;
    //   end
    //   else if i <= dyn_f then
    //     pk_packed_num := i
    //   else if i < 14 then
    //     pk_packed_num := (i - dyn_f - 1) * 16 + get_nyb + dyn_f + 1
    //   else begin
    //     if repeat_count != 0 then abort('Extra repeat count!');
    //     if i = 14 then
    //        repeat_count := pk_packed_num
    //     else
    //        repeat_count := 1;
    //     send_out(true, repeat_count);
    //     pk_packed_num := pk_packed_num;
    //   end;
    // end;

    bool get_packed_number(uint32_t &val, const RLEMetrics &rle_metrics) {
        uint8_t nyb;
        uint32_t i, j;

        uint8_t dyn_f = rle_metrics.dyn_f;

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
            } else if (i <= dyn_f) {
                val = i;
                break;
            } else if (i < PK_REPEAT_COUNT) {
                if (!get_nybble(nyb)) return false;
                val = ((i - dyn_f - 1) << 4) + nyb + dyn_f + 1;
                break;
            } else {
                // if (repeat_count != 0) {
                //   std::cerr << "Spurious repeat_count iteration!" << std::endl;
                //   return false;
                // }
                if (i == PK_REPEAT_COUNT) {
                    if (!get_packed_number(repeat_count, rle_metrics)) return false;
                } else { // i == PK_REPEAT_ONCE
                    repeat_count = 1;
                }
            }
        }
        return true;
    }

public:
    bool retrieve_bitmap(const RLEBitmap &fromBitmap, Bitmap &toBitmap, const Pos atOffset,
                         const RLEMetrics rle_metrics) {
        // point on the glyphs' bitmap definition
        memory_ptr = (MemoryPtr)fromBitmap.pixels.data();
        memory_end = memory_ptr + fromBitmap.length;
        uint8_t *rowp;

        if (resolution == PixelResolution::ONE_BIT) {
            uint32_t row_size = (toBitmap.dim.width + 7) >> 3;
            rowp = toBitmap.pixels.data() + (atOffset.y * row_size);

            if (rle_metrics.dyn_f == 14) { // is a non-compressed RLE?
                uint32_t count = 8;
                uint8_t data;

                for (uint32_t row = 0; row < fromBitmap.dim.height; row++, rowp += row_size) {
                    for (uint32_t col = atOffset.x; col < fromBitmap.dim.width + atOffset.x;
                         col++) {
                        if (count >= 8) {
                            if (!getnext8(data)) {
                                std::cerr << "Not enough bitmap data!" << std::endl;
                                return false;
                            }
                            // std::cout << std::hex << +data << ' ';
                            count = 0;
                        }
                        if (data & (0x80U >> count)) rowp[col >> 3] |= (0x80U >> (col & 7));
                        count++;
                    }
                }
                // std::cout << std::endl;
            } else {
                uint32_t count = 0;

                repeat_count = 0;
                nybble_flipper = 0xf0U;

                bool black = !(rle_metrics.first_is_black == 1);

                for (uint32_t row = 0; row < fromBitmap.dim.height; row++, rowp += row_size) {
                    for (uint32_t col = atOffset.x; col < fromBitmap.dim.width + atOffset.x;
                         col++) {
                        if (count == 0) {
                            if (!get_packed_number(count, rle_metrics)) {
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
                        if (black) rowp[col >> 3] |= (0x80U >> (col & 0x07));
                        count--;
                    }

                    // if (repeat_count != 0) std::cout << "Repeat count: " << repeat_count <<
                    // std::endl;
                    while ((row < fromBitmap.dim.height) && (repeat_count-- > 0)) {
                        memcpy(rowp + row_size, rowp, row_size);
                        row++;
                        rowp += row_size;
                    }

                    repeat_count = 0;
                }
                // std::cout << std::endl;
            }
        } else {
            uint32_t row_size = toBitmap.dim.width;
            rowp = toBitmap.pixels.data() + (atOffset.y * row_size);

            repeat_count = 0;
            nybble_flipper = 0xf0U;

            if (rle_metrics.dyn_f == 14) { // is a bitmap?
                uint32_t count = 8;
                uint8_t data;

                for (uint32_t row = 0; row < (fromBitmap.dim.height); row++, rowp += row_size) {
                    for (uint32_t col = atOffset.x; col < (fromBitmap.dim.width + atOffset.x);
                         col++) {
                        if (count >= 8) {
                            if (!getnext8(data)) {
                                std::cerr << "Not enough bitmap data!" << std::endl;
                                return false;
                            }
                            // std::cout << std::hex << +data << ' ';
                            count = 0;
                        }
                        rowp[col] = (data & (0x80U >> count)) ? 0xFF : 0;
                        count++;
                    }
                }
                // std::cout << std::endl;
            } else {
                uint32_t count = 0;

                repeat_count = 0;
                nybble_flipper = 0xf0U;

                bool black = !(rle_metrics.first_is_black == 1);

                for (uint32_t row = 0; row < (fromBitmap.dim.height); row++, rowp += row_size) {
                    for (uint32_t col = atOffset.x; col < (fromBitmap.dim.width + atOffset.x);
                         col++) {
                        if (count == 0) {
                            if (!get_packed_number(count, rle_metrics)) {
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
                        if (black) rowp[col] = 0xFF;
                        count--;
                    }

                    // if (repeat_count != 0) std::cout << "Repeat count: " << repeat_count <<
                    // std::endl;
                    while ((row < toBitmap.dim.height) && (repeat_count-- > 0)) {
                        memcpy(rowp + row_size, rowp, row_size);
                        row++;
                        rowp += row_size;
                    }

                    repeat_count = 0;
                }
                // std::cout << std::endl;
            }
        }
        return true;
    }

public:
    RLEExtractor() {}
};
