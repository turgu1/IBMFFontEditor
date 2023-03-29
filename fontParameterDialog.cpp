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

  charSelections_              = IBMFDefs::CharSelectionsPtr(new IBMFDefs::CharSelections);
  codePointBlocks_             = nullptr;

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

  QHBoxLayout *dpi1 = new QHBoxLayout();
  dpi75_            = new QRadioButton("75 dpi");
  dpi96_            = new QRadioButton("96 dpi");
  dpi150_           = new QRadioButton("150 dpi");
  dpi1->addWidget(dpi75_);
  dpi1->addWidget(dpi96_);
  dpi1->addWidget(dpi150_);
  dpi75_->setChecked(true);

  QHBoxLayout *dpi2 = new QHBoxLayout();
  dpi166_           = new QRadioButton("166 dpi");
  dpi212_           = new QRadioButton("212 dpi");
  dpi300_           = new QRadioButton("300 dpi");
  dpiNbr_           = new QLineEdit("");
  dpiNbr_->setMaximumWidth(60);
  dpi2->addWidget(dpi166_);
  dpi2->addWidget(dpi212_);
  dpi2->addWidget(dpi300_);

  QHBoxLayout *dpi3 = new QHBoxLayout();
  errorMsg_         = new QLabel("Must be a valid integer between 50 and 500!");
  errorMsg_->setVisible(false);
  errorMsg_->setStyleSheet("QLabel {color: red;}");
  dpi3->addWidget(new QLabel("SPecific DPI:"));
  dpi3->addWidget(dpiNbr_);
  dpi3->addWidget(errorMsg_);
  dpi3->addStretch();

  QHBoxLayout *pointSizes1 = new QHBoxLayout();
  pt8_                     = new QCheckBox("8pt");
  pt9_                     = new QCheckBox("9pt");
  pt10_                    = new QCheckBox("10pt");
  pt12_                    = new QCheckBox("12pt");
  pointSizes1->addWidget(pt8_);
  pointSizes1->addWidget(pt9_);
  pointSizes1->addWidget(pt10_);
  pointSizes1->addWidget(pt12_);

  QHBoxLayout *pointSizes2 = new QHBoxLayout();
  pt14_                    = new QCheckBox("14pt");
  pt17_                    = new QCheckBox("17pt");
  pt24_                    = new QCheckBox("24pt");
  pt48_                    = new QCheckBox("48pt");
  pt14_->setChecked(true);
  pointSizes2->addWidget(pt14_);
  pointSizes2->addWidget(pt17_);
  pointSizes2->addWidget(pt24_);
  pointSizes2->addWidget(pt48_);

  QFormLayout *formLayout = new QFormLayout;
  formLayout->addRow(tr("TTF Font File Name:"), ttfFontFilename);
  formLayout->addRow(tr("IBMF Font File Name:"), fontFilename);
  formLayout->addRow(tr("Resolution:"), dpi1);
  formLayout->addRow(tr(""), dpi2);
  formLayout->addRow(tr(""), dpi3);
  formLayout->addRow(tr("Face Sizes:"), pointSizes1);
  formLayout->addRow(tr(""), pointSizes2);

  ui->mainFrame->setLayout(formLayout);

  QObject::connect(browse, &QPushButton::clicked, this,
                   &FontParameterDialog::browseIBMFFontFilename);
  QObject::connect(ttfBrowse, &QPushButton::clicked, this,
                   &FontParameterDialog::browseTTFFontFilename);
  QObject::connect(pt8_, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(pt9_, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(pt10_, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(pt12_, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(pt14_, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(pt17_, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(pt24_, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(pt48_, &QCheckBox::clicked, this, &FontParameterDialog::onCheckBoxClicked);
  QObject::connect(dpiNbr_, &QLineEdit::textChanged, this, &FontParameterDialog::checkDpiNbr);
}

FontParameterDialog::~FontParameterDialog() { delete ui; }

void FontParameterDialog::checkDpiNbr(const QString &value) {
  if (value.isEmpty()) {
    errorMsg_->setVisible(false);
    dpi75_->setEnabled(true);
    dpi96_->setEnabled(true);
    dpi150_->setEnabled(true);
    dpi166_->setEnabled(true);
    dpi212_->setEnabled(true);
    dpi300_->setEnabled(true);
  } else if ((value.toFloat() < 50) || (value.toFloat() > 500)) {
    errorMsg_->setVisible(true);
  } else {
    errorMsg_->setVisible(false);
    dpi75_->setEnabled(false);
    dpi96_->setEnabled(false);
    dpi150_->setEnabled(false);
    dpi166_->setEnabled(false);
    dpi212_->setEnabled(false);
    dpi300_->setEnabled(false);
  }
}

void FontParameterDialog::checkForNext() {
  ui->nextButton->setEnabled(
      !(ttfFontFilename_->text().isEmpty() || ibmfFontFilename_->text().isEmpty()) &&
      (pt8_->isChecked() || pt9_->isChecked() || pt10_->isChecked() || pt12_->isChecked() ||
       pt14_->isChecked() || pt17_->isChecked() || pt24_->isChecked() || pt48_->isChecked()));
  if (ui->nextButton->isEnabled()) {
    ui->nextButton->setFocus(Qt::OtherFocusReason);
  }
}

void FontParameterDialog::browseIBMFFontFilename() {
  QSettings settings("ibmf", "IBMFEditor");
  QString   filename = ibmfFontFilename_->text();
  if (filename.isEmpty()) filename = settings.value("ibmfFolder").toString();
  QString newFilePath =
      QFileDialog::getSaveFileName(this, "New IBMF Font File", filename, "*.ibmf");
  if (!newFilePath.isEmpty()) {
    ibmfFontFilename_->setText(newFilePath);
  }
  checkForNext();
}

void FontParameterDialog::browseTTFFontFilename() {
  QSettings settings("ibmf", "IBMFEditor");
  QString   filename = ttfFontFilename_->text();
  if (filename.isEmpty()) filename = settings.value("ttfFolder").toString();
  QString newFilePath =
      QFileDialog::getOpenFileName(this, "Open TTF/OTF Font File", filename, "Font (*.ttf *.otf)");
  if (!newFilePath.isEmpty()) {
    ttfFontFilename_->setText(newFilePath);
  }
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

    fontParameters_ = IBMFDefs::FontParametersPtr(new IBMFDefs::FontParameters(
        IBMFDefs::FontParameters{.dpi = dpiNbr_->text().toInt() != 0 ? dpiNbr_->text().toInt()
                                        : dpi75_->isChecked()        ? 75
                                        : dpi96_->isChecked()        ? 96
                                        : dpi150_->isChecked()       ? 150
                                        : dpi166_->isChecked()       ? 166
                                        : dpi212_->isChecked()       ? 212
                                                                     : 300,
                                 //
                                 .pt8            = pt8_->isChecked(),
                                 .pt9            = pt9_->isChecked(),
                                 .pt10           = pt10_->isChecked(),
                                 .pt12           = pt12_->isChecked(),
                                 .pt14           = pt14_->isChecked(),
                                 .pt17           = pt17_->isChecked(),
                                 .pt24           = pt24_->isChecked(),
                                 .pt48           = pt48_->isChecked(),
                                 .filename       = ibmfFontFilename_->text(),
                                 .charSelections = charSelections_}));
    accept();
  }
}

void FontParameterDialog::on_cancelButton_clicked() { reject(); }

void FontParameterDialog::onCheckBoxClicked() { checkForNext(); }
