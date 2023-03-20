#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>

namespace Ui {
class FontParameterDialog;
}

class FontParameterDialog : public QDialog {
  Q_OBJECT

public:
  explicit FontParameterDialog(QWidget *parent = nullptr);
  ~FontParameterDialog();

  struct FontParameters {
    int     dpi;
    bool    pt12;
    bool    pt14;
    bool    pt17;
    QString filename;
  };

  FontParameters getParameters() { return fontParameters_; }

private slots:
  void on_buttonBox_accepted();
  void browseFontFilename();

private:
  Ui::FontParameterDialog *ui;
  FontParameters           fontParameters_;

  QLineEdit    *ibmfFontFilename_;
  QRadioButton *dpi75_;
  QRadioButton *dpi100_;
  QRadioButton *dpi120_;
  QCheckBox    *pt12_;
  QCheckBox    *pt14_;
  QCheckBox    *pt17_;
};
