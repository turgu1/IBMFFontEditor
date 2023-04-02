#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>

#include "IBMFDriver/IBMFDefs.hpp"
#include "Unicode/UBlocks.hpp"
#include "freeType.h"

namespace Ui {
class FontParameterDialog;
}

class FontParameterDialog : public QDialog {
  Q_OBJECT

public:
  explicit FontParameterDialog(FreeType &ft, QString title, QWidget *parent = nullptr);
  ~FontParameterDialog();

  IBMFDefs::FontParametersPtr getParameters() { return fontParameters_; }

private slots:
  void browseIBMFFontFilename();
  void browseTTFFontFilename();
  void on_nextButton_clicked();
  void on_cancelButton_clicked();
  void onCheckBoxClicked();
  void checkDpiNbr(const QString &value);

private:
  Ui::FontParameterDialog    *ui;
  IBMFDefs::FontParametersPtr fontParameters_;
  FreeType                   &ft_;

  IBMFDefs::CharSelectionsPtr charSelections_;
  QLineEdit                  *ibmfFontFilename_;
  QLineEdit                  *ttfFontFilename_;
  CodePointBlocks            *codePointBlocks_;
  QRadioButton               *dpi75_;
  QRadioButton               *dpi96_;
  QRadioButton               *dpi150_;
  QRadioButton               *dpi166_;
  QRadioButton               *dpi212_;
  QRadioButton               *dpi300_;
  QLineEdit                  *dpiNbr_;
  QLabel                     *errorMsg_;
  QCheckBox                  *pt8_;
  QCheckBox                  *pt9_;
  QCheckBox                  *pt10_;
  QCheckBox                  *pt12_;
  QCheckBox                  *pt14_;
  QCheckBox                  *pt17_;
  QCheckBox                  *pt24_;
  QCheckBox                  *pt48_;
  QCheckBox                  *withKerning_;

  void checkForNext();
};
