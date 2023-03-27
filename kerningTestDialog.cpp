#include "kerningTestDialog.h"

#include <QVBoxLayout>
#include <iostream>

#include "IBMFDriver/ibmf_font_mod.hpp"
#include "drawingSpace.h"
#include "ui_kerningTestDialog.h"

KerningTestDialog::KerningTestDialog(IBMFFontModPtr font, int faceIdx, QWidget *parent)
    : QDialog(parent), ui(new Ui::KerningTestDialog), font_(font), faceIdx_(faceIdx),
      resizing_(false) {

  ui->setupUi(this);

  drawingSpace_ = new DrawingSpace(font, faceIdx, this);

  QVBoxLayout *frameLayout = new QVBoxLayout();
  frameLayout->addWidget(drawingSpace_);

  ui->frame->setLayout(frameLayout);

  ui->comboBox->setCurrentIndex(1);
  drawingSpace_->setText(proofingTexts[0]);

  for (int i = 0; i < font_->getPreamble().faceCount; i++) {
    IBMFDefs::FaceHeaderPtr face_header = font_->getFaceHeader(i);
    ui->faceIdxCombo->addItem(QString::number(face_header->pointSize).append(" pts"));
  }
}

void KerningTestDialog::resizeEvent(QResizeEvent *event) {}

void KerningTestDialog::paintEvent(QPaintEvent *event) {
  // std::cout << "Line Count: " << drawingSpace_->getLineCount() << std::endl;
  int linesPerPage = drawingSpace_->getLinesPerPage();
  int max          = drawingSpace_->getLineCount() - linesPerPage;
  if (max < 0) { max = drawingSpace_->getLineCount(); }
  ui->scrollBar->setMaximum(max);
  ui->scrollBar->setPageStep(linesPerPage);
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

void KerningTestDialog::on_comboBox_currentIndexChanged(int index) {
  if (index == 0) {
    drawingSpace_->setText(combinedLetters());
  } else {
    drawingSpace_->setText(proofingTexts[index - 1]);
  }
}

void KerningTestDialog::on_scrollBar_valueChanged(int value) {
  drawingSpace_->setFirstLineToDraw(value);
}

void KerningTestDialog::on_faceIdxCombo_currentIndexChanged(int index) {
  drawingSpace_->setFaceIdx(index);
}
