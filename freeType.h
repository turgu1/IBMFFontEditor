#pragma once

#include <freetype/freetype.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_TYPES_H
#include FT_OUTLINE_H
#include FT_RENDER_H

#include <QMessageBox>

class FreeType {
private:
  bool       initialized_;
  FT_Library ftLib_;
  FT_Face    ftFace_;

public:
  FreeType();
  ~FreeType();

  inline bool       isInitialized() { return initialized_; }
  inline FT_Library getLib() { return ftLib_; }
  inline FT_Face    getFace() { return ftFace_; }

  bool openFace(QString filename);
  void closeFace();
};
