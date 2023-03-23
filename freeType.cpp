#include "freeType.h"

FreeType::FreeType() : initialized_(false), ftLib_(nullptr), ftFace_(nullptr) {
  FT_Error error = FT_Err_Ok;
  error          = FT_Init_FreeType(&ftLib_);
  if (error) {
    QMessageBox::warning(nullptr, "Freetype Not Initialized", FT_Error_String(error));
  } else {
    initialized_ = true;
  }
}

FreeType::~FreeType() {
  if (ftFace_ != nullptr) {
    FT_Done_Face(ftFace_);
    ftFace_ = nullptr;
  }
  if (ftLib_ != nullptr) {
    FT_Done_FreeType(ftLib_);
    ftLib_ = nullptr;
  }
}

bool FreeType::openFace(QString filename) {
  if (isInitialized()) {
    FT_Error error = FT_New_Face(ftLib_, filename.toStdString().c_str(), 0, &ftFace_);
    if (error) {
      QMessageBox::warning(nullptr, "Unable to open font",
                           QString("Not able to open font %1").arg(filename));
      return false;
    }
    return true;
  } else {
    return false;
  }
}

void FreeType::closeFace() {
  if (ftFace_ != nullptr) {
    FT_Done_Face(ftFace_);
    ftFace_ = nullptr;
  }
}
