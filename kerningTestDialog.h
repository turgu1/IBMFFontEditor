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

private:
  Ui::KerningTestDialog *ui;

  DrawingSpace *drawingSpace_;
};
