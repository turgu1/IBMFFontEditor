#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>

#include "IBMFDriver/IBMFDefs.hpp"
#include "Unicode/UBlocks.hpp"

namespace Ui {
class HexFontParameterDialog;
}

class HexFontParameterDialog : public QDialog {
  Q_OBJECT

public:
  explicit HexFontParameterDialog(QString title, QWidget *parent = nullptr);
  ~HexFontParameterDialog();

  IBMFDefs::FontParametersPtr getParameters() { return fontParameters_; }

private slots:
  void browseIBMFFontFilename();
  void browseHexFontFilename();
  void on_nextButton_clicked();
  void on_cancelButton_clicked();
  void onCheckBoxClicked();

private:
  Ui::HexFontParameterDialog *ui;
  IBMFDefs::FontParametersPtr fontParameters_;

  IBMFDefs::CharSelectionsPtr charSelections_;
  CodePointBlocks            *codePointBlocks_;

  void checkForNext();
};
