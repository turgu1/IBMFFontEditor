#include "kerningTestDialog.h"

#include <iostream>

#include <QVBoxLayout>

#include "IBMFDriver/ibmf_font_mod.hpp"
#include "drawingSpace.h"
#include "ui_kerningTestDialog.h"

KerningTestDialog::KerningTestDialog(IBMFFontModPtr font, int faceIdx, QWidget *parent)
    : QDialog(parent), ui(new Ui::KerningTestDialog), font_(font), faceIdx_(faceIdx),
      resizing_(false) {

  ui->setupUi(this);

  drawingSpace_ = new DrawingSpace(font, faceIdx);

  // QVBoxLayout *frameLayout = new QVBoxLayout();
  // frameLayout->addWidget(drawingSpace_);

  // ui->scrollArea->setLayout(frameLayout);
  ui->scrollArea->setWidget(drawingSpace_);

  // ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  // ui->scrollArea->setWidgetResizable(true);

  ui->comboBox->setCurrentIndex(2);
  drawingSpace_->setText(proofingTexts[0]);

  for (int i = 0; i < font_->getPreamble().faceCount; i++) {
    IBMFDefs::FaceHeaderPtr face_header = font_->getFaceHeader(i);
    ui->faceIdxCombo->addItem(QString::number(face_header->pointSize).append(" pts"));
  }
}

void KerningTestDialog::setText(QString text) {
  drawingSpace_->setText(text);
}

KerningTestDialog::~KerningTestDialog() {
  delete ui;
}

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

QString KerningTestDialog::allCodePoints() {
  QString test = "";

  for (IBMFDefs::GlyphCode glyphCode = 0; glyphCode < font_->getFaceHeader(faceIdx_)->glyphCount;
       glyphCode++) {
    test += QChar(font_->getUTF32(glyphCode));
  }
  return test;
}

void KerningTestDialog::on_comboBox_currentIndexChanged(int index) {
  if (index == 0) {
    drawingSpace_->setText(combinedLetters());
  } else if (index == 1) {
    drawingSpace_->setText(allCodePoints());
  } else {
    drawingSpace_->setText(proofingTexts[index - 2]);
  }
}

void KerningTestDialog::on_faceIdxCombo_currentIndexChanged(int index) {
  drawingSpace_->setFaceIdx(index);
}
