#include "fontParameterDialog.h"

#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QSettings>

#include "Unicode/UBlocks.hpp"
#include "blocksDialog.h"
#include "freeType.h"
#include "ui_fontParameterDialog.h"

FontParameterDialog::FontParameterDialog(FreeType &ft, QString title, QWidget *parent)
    : QDialog(parent), ft_(ft), ui(new Ui::FontParameterDialog), fontParameters_(nullptr) {

  ui->setupUi(this);

  QSettings settings("ibmf", "IBMFEditor");

  setWindowTitle(title);

  ui->nextButton->setEnabled(false);
  ui->errorMsg->setVisible(false);

  charSelections_  = IBMFDefs::CharSelectionsPtr(new IBMFDefs::CharSelections);
  codePointBlocks_ = nullptr;

  QObject::connect(ui->browse, &QPushButton::clicked, this,
                   &FontParameterDialog::browseIBMFFontFilename);
  QObject::connect(ui->ttfBrowse, &QPushButton::clicked, this,
                   &FontParameterDialog::browseTTFFontFilename);
  QObject::connect(ui->pt8, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(ui->pt9, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(ui->pt10, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(ui->pt12, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(ui->pt14, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(ui->pt17, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(ui->pt24, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(ui->pt48, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(ui->dpiNbr, &QLineEdit::textChanged, this, &FontParameterDialog::checkDpiNbr);
}

FontParameterDialog::~FontParameterDialog() {
  delete ui;
}

void FontParameterDialog::checkDpiNbr(const QString &value) {
  if (value.isEmpty()) {
    ui->errorMsg->setVisible(false);
    ui->dpi75->setEnabled(true);
    ui->dpi96->setEnabled(true);
    ui->dpi150->setEnabled(true);
    ui->dpi166->setEnabled(true);
    ui->dpi212->setEnabled(true);
    ui->dpi300->setEnabled(true);
  } else if ((value.toFloat() < 50) || (value.toFloat() > 500)) {
    ui->errorMsg->setVisible(true);
  } else {
    ui->errorMsg->setVisible(false);
    ui->dpi75->setEnabled(false);
    ui->dpi96->setEnabled(false);
    ui->dpi150->setEnabled(false);
    ui->dpi166->setEnabled(false);
    ui->dpi212->setEnabled(false);
    ui->dpi300->setEnabled(false);
  }
}

void FontParameterDialog::checkForNext() {
  ui->nextButton->setEnabled(
      !(ui->ttfFontFilename->text().isEmpty() || ui->ibmfFontFilename->text().isEmpty()) &&
      (ui->pt8->isChecked() || ui->pt9->isChecked() || ui->pt10->isChecked() ||
       ui->pt12->isChecked() || ui->pt14->isChecked() || ui->pt17->isChecked() ||
       ui->pt24->isChecked() || ui->pt48->isChecked()));
  if (ui->nextButton->isEnabled()) { ui->nextButton->setFocus(Qt::OtherFocusReason); }
}

void FontParameterDialog::browseIBMFFontFilename() {
  QSettings settings("ibmf", "IBMFEditor");
  QString   filename = ui->ibmfFontFilename->text();
  if (filename.isEmpty()) filename = settings.value("ibmfFolder").toString();
  QString newFilePath =
      QFileDialog::getSaveFileName(this, "New IBMF Font File", filename, "*.ibmf");
  if (!newFilePath.isEmpty()) { ui->ibmfFontFilename->setText(newFilePath); }
  checkForNext();
}

void FontParameterDialog::browseTTFFontFilename() {
  QSettings settings("ibmf", "IBMFEditor");
  QString   filename = ui->ttfFontFilename->text();
  if (filename.isEmpty()) filename = settings.value("ttfFolder").toString();
  QString newFilePath =
      QFileDialog::getOpenFileName(this, "Open TTF/OTF Font File", filename, "Font (*.ttf *.otf)");
  if (!newFilePath.isEmpty()) { ui->ttfFontFilename->setText(newFilePath); }
  checkForNext();
}

void FontParameterDialog::on_nextButton_clicked() {

  QSettings settings("ibmf", "IBMFEditor");
  QFileInfo fileInfo(ui->ibmfFontFilename->text());
  settings.setValue("ibmfFolder", fileInfo.absolutePath());
  fileInfo.setFile(ui->ttfFontFilename->text());
  settings.setValue("ttfFolder", fileInfo.absolutePath());

  BlocksDialog *blocksDialog =
      new BlocksDialog(ft_, ui->ttfFontFilename->text(), fileInfo.fileName());
  if (blocksDialog->exec() == QDialog::Accepted) {
    SelectedBlockIndexesPtr blockIndexes = blocksDialog->getSelectedBlockIndexes();
    // codePointBlocks_  = blocksDialog->getCodePointBlocks();

    charSelections_->push_back(IBMFDefs::CharSelection(
        {.filename = ui->ttfFontFilename->text(), .selectedBlockIndexes = blockIndexes}));

    fontParameters_ = IBMFDefs::FontParametersPtr(new IBMFDefs::FontParameters(
        IBMFDefs::FontParameters{.dpi = ui->dpiNbr->text().toInt() != 0 ? ui->dpiNbr->text().toInt()
                                        : ui->dpi75->isChecked()        ? 75
                                        : ui->dpi96->isChecked()        ? 96
                                        : ui->dpi150->isChecked()       ? 150
                                        : ui->dpi166->isChecked()       ? 166
                                        : ui->dpi212->isChecked()       ? 212
                                                                        : 300,
                                 //
                                 .pt8            = ui->pt8->isChecked(),
                                 .pt9            = ui->pt9->isChecked(),
                                 .pt10           = ui->pt10->isChecked(),
                                 .pt12           = ui->pt12->isChecked(),
                                 .pt14           = ui->pt14->isChecked(),
                                 .pt17           = ui->pt17->isChecked(),
                                 .pt24           = ui->pt24->isChecked(),
                                 .pt48           = ui->pt48->isChecked(),
                                 .filename       = ui->ibmfFontFilename->text(),
                                 .charSelections = charSelections_,
                                 .withKerning    = ui->withKerning->isChecked()}));
    accept();
  }
}

void FontParameterDialog::on_cancelButton_clicked() {
  reject();
}

void FontParameterDialog::onCheckBoxClicked() {
  checkForNext();
}
