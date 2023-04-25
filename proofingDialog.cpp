#include "proofingDialog.h"

#include <iostream>

#include <QSettings>
#include <QVBoxLayout>

#include "IBMFDriver/IBMFFontMod.hpp"
#include "drawingSpace.h"
#include "ui_proofingDialog.h"

ProofingDialog::ProofingDialog(IBMFFontModPtr font, int faceIdx, QWidget *parent)
    : QDialog(parent), ui(new Ui::ProofingDialog), font_(font), faceIdx_(faceIdx),
      resizing_(false) {

  ui->setupUi(this);

  drawingSpace_ = new DrawingSpace(font, faceIdx);

  // QVBoxLayout *frameLayout = new QVBoxLayout();
  // frameLayout->addWidget(drawingSpace_);

  // ui->scrollArea->setLayout(frameLayout);
  ui->scrollArea->setWidget(drawingSpace_);

  // ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  // ui->scrollArea->setWidgetResizable(true);

  readSettings();

  int index = ui->patternCombo->currentIndex();
  if (index == 0) {
    drawingSpace_->setText(combinedLetters());
  } else if (index == 1) {
    drawingSpace_->setText(allCodePoints());
  } else {
    drawingSpace_->setText(proofingTexts[index - 2]);
  }

  for (int i = 0; i < font_->getPreamble().faceCount; i++) {
    IBMFDefs::FaceHeaderPtr face_header = font_->getFaceHeader(i);
    ui->faceIdxCombo->addItem(QString::number(face_header->pointSize).append(" pts"));
  }
}

void ProofingDialog::setText(QString text) {
  drawingSpace_->setText(text);
}

ProofingDialog::~ProofingDialog() {
  delete ui;
}

void ProofingDialog::on_pixelSizeCombo_currentIndexChanged(int index) {
  drawingSpace_->setPixelSize(index + 1);
}

void ProofingDialog::on_autoKernCheckBox_toggled(bool checked) {
  drawingSpace_->setOpticalKerning(checked);
}

void ProofingDialog::on_normalKernCheckBox_toggled(bool checked) {
  drawingSpace_->setNormalKerning(checked);
}

QString ProofingDialog::combinedLetters() {
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

QString ProofingDialog::allCodePoints() {
  QString test = "";

  for (IBMFDefs::GlyphCode glyphCode = 0; glyphCode < font_->getFaceHeader(faceIdx_)->glyphCount;
       glyphCode++) {
    test += QChar(font_->getUTF32(glyphCode));
  }
  return test;
}

void ProofingDialog::on_faceIdxCombo_currentIndexChanged(int index) {
  drawingSpace_->setFaceIdx(index);
}

void ProofingDialog::on_patternCombo_currentIndexChanged(int index) {
  if (index == 0) {
    drawingSpace_->setText(combinedLetters());
  } else if (index == 1) {
    drawingSpace_->setText(allCodePoints());
  } else {
    drawingSpace_->setText(proofingTexts[index - 2]);
  }
}

void ProofingDialog::writeSettings() {
  QSettings settings("ibmf", "IBMFEditor");

  settings.beginGroup("ProofingDialog");
  settings.setValue("ProofingOpticalKern", ui->autoKernCheckBox->isChecked());
  settings.setValue("ProofingNormalKern", ui->normalKernCheckBox->isChecked());
  settings.setValue("ProofingPixelSize", ui->pixelSizeCombo->currentIndex());
  settings.setValue("ProofingPattern", ui->patternCombo->currentIndex());
  settings.endGroup();
}

void ProofingDialog::readSettings() {
  QSettings settings("ibmf", "IBMFEditor");

  settings.beginGroup("ProofingDialog");
  ui->autoKernCheckBox->setChecked(settings.value("ProofingOpticalKern").toBool());
  ui->normalKernCheckBox->setChecked(settings.value("ProofingNormalKern").toBool());
  ui->pixelSizeCombo->setCurrentIndex(settings.value("ProofingPixelSize").toInt());
  ui->patternCombo->setCurrentIndex(settings.value("ProofingPattern").toInt());
  settings.endGroup();
}

void ProofingDialog::closeEvent(QCloseEvent *event) {
  writeSettings();
}

void ProofingDialog::on_buttonBox_clicked(QAbstractButton *button) {
  writeSettings();
}
