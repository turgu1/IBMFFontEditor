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
class TTFFontParameterDialog;
}

class TTFFontParameterDialog : public QDialog {
  Q_OBJECT

public:
  explicit TTFFontParameterDialog(FreeType &ft, QString title, QWidget *parent = nullptr);
  ~TTFFontParameterDialog();

  IBMFDefs::FontParametersPtr getParameters() { return fontParameters_; }

private slots:
  void browseIBMFFontFilename();
  void browseTTFFontFilename();
  void on_nextButton_clicked();
  void on_cancelButton_clicked();
  void onCheckBoxClicked();
  void checkDpiNbr(const QString &value);

private:
  Ui::TTFFontParameterDialog *ui;
  IBMFDefs::FontParametersPtr fontParameters_;
  FreeType                   &ft_;

  IBMFDefs::CharSelectionsPtr charSelections_;
  CodePointBlocks            *codePointBlocks_;

  void checkForNext();
};
