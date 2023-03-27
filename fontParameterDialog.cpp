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

  charSelections_  = new IBMFDefs::CharSelections;
  codePointBlocks_ = nullptr;

  QHBoxLayout *ttfFontFilename = new QHBoxLayout();
  ttfFontFilename_             = new QLineEdit();
  QPushButton *ttfBrowse       = new QPushButton("...");
  ttfBrowse->setMaximumWidth(40);
  ttfFontFilename->addWidget(ttfFontFilename_);
  ttfFontFilename->addWidget(ttfBrowse);

  QHBoxLayout *fontFilename = new QHBoxLayout();
  ibmfFontFilename_         = new QLineEdit();
  QPushButton *browse       = new QPushButton("...");
  browse->setMaximumWidth(40);
  fontFilename->addWidget(ibmfFontFilename_);
  fontFilename->addWidget(browse);

  QHBoxLayout *dpi = new QHBoxLayout();
  dpi75_           = new QRadioButton("75 dpi");
  dpi100_          = new QRadioButton("100 dpi");
  dpi120_          = new QRadioButton("120 dpi");
  dpi200_          = new QRadioButton("200 dpi");
  dpi300_          = new QRadioButton("300 dpi");
  dpi75_->setChecked(true);
  dpi->addWidget(dpi75_);
  dpi->addWidget(dpi100_);
  dpi->addWidget(dpi120_);
  dpi->addWidget(dpi200_);
  dpi->addWidget(dpi300_);

  QHBoxLayout *pointSizes = new QHBoxLayout();
  pt10_                   = new QCheckBox("10pt");
  pt12_                   = new QCheckBox("12pt");
  pt14_                   = new QCheckBox("14pt");
  pt17_                   = new QCheckBox("17pt");
  pt24_                   = new QCheckBox("24pt");
  pt14_->setChecked(true);
  pointSizes->addWidget(pt10_);
  pointSizes->addWidget(pt12_);
  pointSizes->addWidget(pt14_);
  pointSizes->addWidget(pt17_);
  pointSizes->addWidget(pt24_);

  QFormLayout *formLayout = new QFormLayout;
  formLayout->addRow(tr("TTF Font File Name:"), ttfFontFilename);
  formLayout->addRow(tr("IBMF Font File Name:"), fontFilename);
  formLayout->addRow(tr("Resolution:"), dpi);
  formLayout->addRow(tr("Face Sizes:"), pointSizes);

  ui->mainFrame->setLayout(formLayout);

  QObject::connect(browse, &QPushButton::clicked, this,
                   &FontParameterDialog::browseIBMFFontFilename);
  QObject::connect(ttfBrowse, &QPushButton::clicked, this,
                   &FontParameterDialog::browseTTFFontFilename);
  QObject::connect(pt10_, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(pt12_, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(pt14_, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(pt17_, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(pt24_, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
}

FontParameterDialog::~FontParameterDialog() {
  delete ui;
  delete charSelections_;
}

void FontParameterDialog::checkForNext() {
  ui->nextButton->setEnabled(
      !(ttfFontFilename_->text().isEmpty() || ibmfFontFilename_->text().isEmpty()) &&
      (pt10_->isChecked() || pt12_->isChecked() || pt14_->isChecked() || pt17_->isChecked() ||
       pt24_->isChecked()));
  if (ui->nextButton->isEnabled()) { ui->nextButton->setFocus(Qt::OtherFocusReason); }
}

void FontParameterDialog::browseIBMFFontFilename() {
  QSettings settings("ibmf", "IBMFEditor");
  QString   filename = ibmfFontFilename_->text();
  if (filename.isEmpty()) filename = settings.value("ibmfFolder").toString();
  QString newFilePath =
      QFileDialog::getSaveFileName(this, "New IBMF Font File", filename, "*.ibmf");
  if (!newFilePath.isEmpty()) { ibmfFontFilename_->setText(newFilePath); }
  checkForNext();
}

void FontParameterDialog::browseTTFFontFilename() {
  QSettings settings("ibmf", "IBMFEditor");
  QString   filename = ttfFontFilename_->text();
  if (filename.isEmpty()) filename = settings.value("ttfFolder").toString();
  QString newFilePath =
      QFileDialog::getOpenFileName(this, "Open TTF/OTF Font File", filename, "Font (*.ttf *.otf)");
  if (!newFilePath.isEmpty()) { ttfFontFilename_->setText(newFilePath); }
  checkForNext();
}

void FontParameterDialog::on_nextButton_clicked() {

  QSettings settings("ibmf", "IBMFEditor");
  QFileInfo fileInfo(ibmfFontFilename_->text());
  settings.setValue("ibmfFolder", fileInfo.absolutePath());
  fileInfo.setFile(ttfFontFilename_->text());
  settings.setValue("ttfFolder", fileInfo.absolutePath());

  BlocksDialog *blocksDialog = new BlocksDialog(ft_, ttfFontFilename_->text(), fileInfo.fileName());
  if (blocksDialog->exec() == QDialog::Accepted) {
    SelectedBlockIndexesPtr blockIndexes = blocksDialog->getSelectedBlockIndexes();
    // codePointBlocks_  = blocksDialog->getCodePointBlocks();

    charSelections_->push_back(IBMFDefs::CharSelection(
        {.filename = ttfFontFilename_->text(), .selectedBlockIndexes = blockIndexes}));

    fontParameters_ =
        new IBMFDefs::FontParameters(IBMFDefs::FontParameters{.dpi      = dpi75_->isChecked() ? 75
                                                                          : dpi100_->isChecked() ? 100
                                                                          : dpi120_->isChecked() ? 120
                                                                          : dpi200_->isChecked() ? 200
                                                                                                 : 300,
                                                              .pt10     = pt10_->isChecked(),
                                                              .pt12     = pt12_->isChecked(),
                                                              .pt14     = pt14_->isChecked(),
                                                              .pt17     = pt17_->isChecked(),
                                                              .pt24     = pt24_->isChecked(),
                                                              .filename = ibmfFontFilename_->text(),
                                                              .charSelections = charSelections_});
    accept();
  }
}

void FontParameterDialog::on_cancelButton_clicked() {
  reject();
}

void FontParameterDialog::onCheckBoxClicked() {
  checkForNext();
}
