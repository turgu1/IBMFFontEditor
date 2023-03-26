#include "freeType.h"

#include <iostream>

FreeType::FreeType() : initialized_(false), ftLib_(nullptr) {
  FT_Error error = FT_Init_FreeType(&ftLib_);
  if (error) {
    QMessageBox::warning(nullptr, "Freetype Not Initialized", FT_Error_String(error));
  } else {
    initialized_ = true;
  }
  FT_Int amajor, aminor, apatch;

  FT_Library_Version(ftLib_, &amajor, &aminor, &apatch);

  //  std::cout << "FreeType Version " << amajor << "." << aminor << "." << apatch << std::endl;
}

FreeType::~FreeType() {
  if (ftLib_ != nullptr) {
    FT_Done_FreeType(ftLib_);
    ftLib_ = nullptr;
  }
}

FT_Face FreeType::openFace(QString filename) {
  if (isInitialized()) {
    FT_Face  ftFace;
    FT_Error error = FT_New_Face(ftLib_, filename.toStdString().c_str(), 0, &ftFace);
    if (error) {
      QMessageBox::warning(nullptr, "Unable to open font",
                           QString("Not able to open font %1").arg(filename));
      return nullptr;
    }
    return ftFace;
  } else {
    return nullptr;
  }
}
