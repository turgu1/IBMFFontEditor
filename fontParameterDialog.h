#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>

#include "Unicode/UBlocks.hpp"

namespace Ui {
class FontParameterDialog;
}

class FontParameterDialog : public QDialog {
  Q_OBJECT

public:
  explicit FontParameterDialog(QString title, QWidget *parent = nullptr);
  ~FontParameterDialog();

  struct TTFSelection {
    QString          filename;
    CodePointBlocks *codePointBlocks;
  };
  typedef std::vector<TTFSelection> TTFSelections;

  struct FontParameters {
    int            dpi;
    bool           pt12;
    bool           pt14;
    bool           pt17;
    QString        filename;
    TTFSelections *ttfSelections;
  };

  FontParameters getParameters() { return fontParameters_; }

private slots:
  void browseIBMFFontFilename();
  void browseTTFFontFilename();
  void on_nextButton_clicked();
  void on_cancelButton_clicked();
  void onCheckBoxClicked();

private:
  Ui::FontParameterDialog *ui;
  FontParameters           fontParameters_;

  TTFSelections   *ttfSelections_;
  QLineEdit       *ibmfFontFilename_;
  QLineEdit       *ttfFontFilename_;
  CodePointBlocks *codePointBlocks_;
  QRadioButton    *dpi75_;
  QRadioButton    *dpi100_;
  QRadioButton    *dpi120_;
  QCheckBox       *pt12_;
  QCheckBox       *pt14_;
  QCheckBox       *pt17_;

  void checkForNext();
};
