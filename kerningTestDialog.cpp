#include "kerningTestDialog.h"

#include <QVBoxLayout>

#include "IBMFDriver/ibmf_font_mod.hpp"
#include "drawingSpace.h"
#include "ui_kerningTestDialog.h"

KerningTestDialog::KerningTestDialog(IBMFFontModPtr font, int faceIdx, QWidget *parent)
    : QDialog(parent), ui(new Ui::KerningTestDialog) {
  ui->setupUi(this);

  drawingSpace_            = new DrawingSpace(font, faceIdx, this);

  QVBoxLayout *frameLayout = new QVBoxLayout();
  frameLayout->addWidget(drawingSpace_);

  ui->frame->setLayout(frameLayout);

  ui->comboBox->setCurrentIndex(1);
  drawingSpace_->setText(proofingTexts[0]);
}

void KerningTestDialog::setText(QString text) { drawingSpace_->setText(text); }

KerningTestDialog::~KerningTestDialog() { delete ui; }

void KerningTestDialog::on_pixelSizeCombo_currentIndexChanged(int index) {
  drawingSpace_->setPixelSize(index + 1);
}

void KerningTestDialog::on_autoKernCheckBox_toggled(bool checked) {
  drawingSpace_->setAutoKerning(checked);
}

void KerningTestDialog::on_normalKernCheckBox_toggled(bool checked) {
  drawingSpace_->setNormalKerning(checked);
}

QString KerningTestDialog::combinedLetters() {
  QString test = "";
  for (int i = 0; i < 26; i++) {
    for (int j = 0; j < 26; j++) {
      test += QChar('A' + i);
      test += QChar('a' + j);
    }
  }
  for (int i = 0; i < 26; i++) {
    for (int j = 0; j < 26; j++) {
      test += QChar('A' + i);
      test += QChar('A' + j);
    }
  }
  for (int i = 0; i < 26; i++) {
    for (int j = 0; j < 26; j++) {
      test += QChar('a' + i);
      test += QChar('a' + j);
    }
  }
  return test;
}

void KerningTestDialog::on_comboBox_currentIndexChanged(int index) {
  if (index == 0) {
    drawingSpace_->setText(combinedLetters());
  } else {
    drawingSpace_->setText(proofingTexts[index - 1]);
  }
}
