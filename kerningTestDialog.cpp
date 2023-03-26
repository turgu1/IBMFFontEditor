#include "kerningTestDialog.h"
#include "IBMFDriver/ibmf_font_mod.hpp"
#include "drawingSpace.h"
#include "ui_kerningTestDialog.h"

#include <QVBoxLayout>

KerningTestDialog::KerningTestDialog(IBMFFontModPtr font, int faceIdx, QWidget *parent)
    : QDialog(parent), ui(new Ui::KerningTestDialog) {
  ui->setupUi(this);

  drawingSpace_ = new DrawingSpace(font, faceIdx, this);

  QVBoxLayout *frameLayout = new QVBoxLayout();
  frameLayout->addWidget(drawingSpace_);

  ui->frame->setLayout(frameLayout);
  ui->noKernRadio->setChecked(true);
}

void KerningTestDialog::setText(QString text) {
  drawingSpace_->setText(text);
}

KerningTestDialog::~KerningTestDialog() {
  delete ui;
}

void KerningTestDialog::on_autoKernRadio_clicked(bool checked) {
  drawingSpace_->setAutoKerning(checked);
  if (checked) {
    ui->normalKernRadio->setChecked(false);
    drawingSpace_->setNormalKerning(false);
    ui->noKernRadio->setChecked(false);
  } else {
    ui->noKernRadio->setChecked(true);
  }
}

void KerningTestDialog::on_normalKernRadio_clicked(bool checked) {
  drawingSpace_->setNormalKerning(checked);
  if (checked) {
    ui->autoKernRadio->setChecked(false);
    drawingSpace_->setAutoKerning(false);
    ui->noKernRadio->setChecked(false);
  } else {
    ui->noKernRadio->setChecked(true);
  }
}

void KerningTestDialog::on_noKernRadio_clicked(bool checked) {
  if (checked) {
    ui->autoKernRadio->setChecked(false);
    ui->normalKernRadio->setChecked(false);
    drawingSpace_->setNormalKerning(false);
    drawingSpace_->setAutoKerning(false);
  } else {
    ui->noKernRadio->setChecked(true);
  }
}

void KerningTestDialog::on_pixelSizeCombo_currentIndexChanged(int index) {
  drawingSpace_->setPixelSize(index + 1);
}
