#pragma once

#include <vector>
#include <cinttypes>

struct Dim {
    int16_t width;
    int16_t height;
    Dim(int16_t w, int16_t h) {
      width  = w;
      height = h;
    }
    Dim() {}
};

struct Pos {
    int16_t x;
    int16_t y;
    Pos(int16_t xpos, int16_t ypos) {
      x = xpos;
      y = ypos;
    }
    Pos() {}
};

// Used by the glyphs'bitmap encoding algorithm

typedef std::vector<uint8_t> Pixels;

struct Bitmap {
  Pixels pixels;
  Dim    dim;
};

