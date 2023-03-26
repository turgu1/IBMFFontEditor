#pragma once

#include <QDialog>

#include "IBMFDriver/ibmf_font_mod.hpp"
#include "drawingSpace.h"

namespace Ui {
class KerningTestDialog;
}

class KerningTestDialog : public QDialog {
  Q_OBJECT

public:
  explicit KerningTestDialog(IBMFFontModPtr font, int faceIdx, QWidget *parent = nullptr);
  ~KerningTestDialog();

  void setText(QString text);

private slots:
  void on_autoKernRadio_clicked(bool checked);
  void on_normalKernRadio_clicked(bool checked);
  void on_noKernRadio_clicked(bool checked);

  void on_pixelSizeCombo_currentIndexChanged(int index);

private:
  Ui::KerningTestDialog *ui;

  DrawingSpace *drawingSpace_;
};
