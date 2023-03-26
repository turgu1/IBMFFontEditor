#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_TAGS_H
#include FT_SYSTEM_H

#include <QMessageBox>

class FreeType {
private:
  bool       initialized_;
  FT_Library ftLib_;

public:
  FreeType();
  ~FreeType();

  inline bool       isInitialized() { return initialized_; }
  inline FT_Library getLib() { return ftLib_; }

  FT_Face openFace(QString filename);
};
