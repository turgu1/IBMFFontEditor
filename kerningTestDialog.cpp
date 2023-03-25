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
}

void KerningTestDialog::setText(QString text) {
  drawingSpace_->setText(text);
}

KerningTestDialog::~KerningTestDialog() {
  delete ui;
}
