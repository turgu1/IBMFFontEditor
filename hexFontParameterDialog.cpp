#include "hexFontParameterDialog.h"

#include <fstream>
#include <iostream>

#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QSettings>

#include "Unicode/UBlocks.hpp"
#include "blocksDialog.h"
#include "freeType.h"
#include "ui_hexFontParameterDialog.h"

HexFontParameterDialog::HexFontParameterDialog(QString title, QWidget *parent)
    : QDialog(parent), ui(new Ui::HexFontParameterDialog), fontParameters_(nullptr) {

  ui->setupUi(this);

  QSettings settings("ibmf", "IBMFEditor");

  setWindowTitle(title);

  ui->nextButton->setEnabled(false);

  charSelections_  = IBMFDefs::CharSelectionsPtr(new IBMFDefs::CharSelections);
  codePointBlocks_ = nullptr;

  QObject::connect(ui->browse, &QPushButton::clicked, this,
                   &HexFontParameterDialog::browseIBMFFontFilename);
  QObject::connect(ui->hexBrowse, &QPushButton::clicked, this,
                   &HexFontParameterDialog::browseHexFontFilename);
}

HexFontParameterDialog::~HexFontParameterDialog() { delete ui; }

void HexFontParameterDialog::checkForNext() {
  ui->nextButton->setEnabled(
      !(ui->hexFontFilename->text().isEmpty() || ui->ibmfFontFilename->text().isEmpty()));
  if (ui->nextButton->isEnabled()) {
    ui->nextButton->setFocus(Qt::OtherFocusReason);
  }
}

void HexFontParameterDialog::browseIBMFFontFilename() {
  QSettings settings("ibmf", "IBMFEditor");
  QString   filename = ui->ibmfFontFilename->text();
  if (filename.isEmpty()) filename = settings.value("ibmfFolder").toString();
  releaseKeyboard();
  QString newFilePath =
      QFileDialog::getSaveFileName(this, "New IBMF Font File", filename, "*.ibmf");
  grabKeyboard();
  if (!newFilePath.isEmpty()) {
    ui->ibmfFontFilename->setText(newFilePath);
  }
  checkForNext();
}

void HexFontParameterDialog::browseHexFontFilename() {
  QSettings settings("ibmf", "IBMFEditor");
  QString   filename = ui->hexFontFilename->text();
  if (filename.isEmpty()) filename = settings.value("hexFolder").toString();
  releaseKeyboard();
  QString newFilePath =
      QFileDialog::getOpenFileName(this, "Open HEX Font File", filename, "Font *.hex");
  grabKeyboard();
  if (!newFilePath.isEmpty()) {
    ui->hexFontFilename->setText(newFilePath);
  }
  checkForNext();
}

void HexFontParameterDialog::on_nextButton_clicked() {

  QSettings settings("ibmf", "IBMFEditor");
  QFileInfo fileInfo(ui->ibmfFontFilename->text());
  settings.setValue("ibmfFolder", fileInfo.absolutePath());
  fileInfo.setFile(ui->hexFontFilename->text());
  settings.setValue("hexFolder", fileInfo.absolutePath());

  std::fstream in;
  in.open(ui->hexFontFilename->text().toStdString(), std::ios::in);

  if (!in.is_open()) {
    QMessageBox::critical(nullptr, "Open File Error",
                          QString("Not able to open HEX file %1").arg(ui->hexFontFilename->text()));
    reject();
    return;
  }

  BlocksDialog *blocksDialog = new BlocksDialog(
      [&in](char32_t *charCode, bool first) -> bool {
        int value;
        if (in.eof()) return false;
        if (first) {
          in.seekg(0, in.beg);
        }
        in >> std::hex >> value;
        *charCode = static_cast<char32_t>(value);
        while (!in.eof() && (in.get() != '\n'))
          ;
        return true;
      },
      [](char32_t ch, bool first) -> bool { return true; }, fileInfo.fileName());

  if (blocksDialog->exec() == QDialog::Accepted) {
    SelectedBlockIndexesPtr blockIndexes = blocksDialog->getSelectedBlockIndexes();
    // codePointBlocks_  = blocksDialog->getCodePointBlocks();

    charSelections_->push_back(IBMFDefs::CharSelection(
        {.filename = ui->hexFontFilename->text(), .selectedBlockIndexes = blockIndexes}));

    fontParameters_ = IBMFDefs::FontParametersPtr(new IBMFDefs::FontParameters(
        IBMFDefs::FontParameters{.dpi = 75,
                                 //
                                 .pt8            = false,
                                 .pt9            = false,
                                 .pt10           = true,
                                 .pt12           = false,
                                 .pt14           = false,
                                 .pt17           = false,
                                 .pt24           = false,
                                 .pt48           = false,
                                 .filename       = ui->ibmfFontFilename->text(),
                                 .charSelections = charSelections_,
                                 .withKerning    = false}));
    accept();
  }
}

void HexFontParameterDialog::on_cancelButton_clicked() { reject(); }

void HexFontParameterDialog::onCheckBoxClicked() { checkForNext(); }
